/*
 * Réseau : Wi-Fi WPA2/WPA3, NTP, MQTT sur TLS authentifié (tâche 7.1).
 *
 * Publication : essensys/n2/<id>/{status,events,frames,counters,alarm/state,
 * events/dump,frames/dump,cmd/result}. Seule souscription : essensys/n2/<id>/cmd.
 * Les commandes passent par n2_app_handle_cmd (HMAC + anti-rejeu) ; aucune
 * n'agit sur le bus Essensys (tâche 7.2, vérifié par check_no_bus_write.sh).
 * Le protocole HTTP legacy IoT BP <-> serveur n'est pas concerné.
 */
#include "n2_net.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#include "n2_app.h"
#include "n2_ota.h"
#include "n2_topics.h"

static const char *TAG = "n2_net";

#define TOPIC_LEN 96
#define COUNTERS_PERIOD_MS 60000
#define DUMP_MAX 50

static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_connected;
static bool s_started;
static char s_t_status[TOPIC_LEN], s_t_events[TOPIC_LEN], s_t_frames[TOPIC_LEN], s_t_counters[TOPIC_LEN];
static char s_t_alarm[TOPIC_LEN], s_t_dump_ev[TOPIC_LEN], s_t_dump_fr[TOPIC_LEN];
static char s_t_cmd[TOPIC_LEN], s_t_cmd_res[TOPIC_LEN];

static int publish(const char *topic, const char *data, int qos, int retain)
{
    if (!s_connected || s_mqtt == NULL) {
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt, topic, data, 0, qos, retain);
}

static void publish_status(void)
{
    char buf[256];
    N2_LOCK();
    const char *alarm = alarm_state_name(g_n2.alarm.state);
    bool dbg = g_n2.dbg.enabled;
    uint8_t tsrc = g_n2.tb.src;
    int m1 = bh_ba_is_muted(&g_n2.bh, 0x11), m2 = bh_ba_is_muted(&g_n2.bh, 0x12), m3 = bh_ba_is_muted(&g_n2.bh, 0x13);
    bool running = g_n2.spy_running;
    N2_UNLOCK();
    snprintf(buf, sizeof(buf),
             "{\"state\":\"online\",\"fw\":\"%s\",\"alarm\":\"%s\",\"debug\":%s,\"spy\":%s,\"tsrc\":\"%s\","
             "\"uptime_s\":%" PRId64 ",\"ba_mute\":[%d,%d,%d]}",
             esp_app_get_description()->version, alarm, dbg ? "true" : "false", running ? "true" : "false",
             timebase_src_name(tsrc), n2_uptime_ms() / 1000, m1, m2, m3);
    publish(s_t_status, buf, 1, 1);
}

static void dump_events(void)
{
    char buf[200];
    N2_LOCK();
    uint32_t next = g_n2.evlog.next_seq;
    uint32_t from = evlog_oldest(&g_n2.evlog);
    N2_UNLOCK();
    if (next > from + DUMP_MAX) {
        from = next - DUMP_MAX;
    }
    for (uint32_t s = from; s < next; s++) {
        n2_record_t r;
        N2_LOCK();
        int ok = g_n2.evlog_ok && evlog_read(&g_n2.evlog, s, &r) == 0;
        N2_UNLOCK();
        if (ok && n2_fmt_event(buf, sizeof(buf), &r) > 0) {
            publish(s_t_dump_ev, buf, 0, 0);
        }
    }
}

static void dump_frames(void)
{
    char buf[384];
    N2_LOCK();
    uint32_t next = g_n2.flog.next_seq;
    N2_UNLOCK();
    uint32_t after = next > DUMP_MAX + 1u ? next - DUMP_MAX - 1u : 0u;
    for (int i = 0; i < DUMP_MAX; i++) {
        flog_entry_t e;
        N2_LOCK();
        int got = flog_get_after(&g_n2.flog, after, &e);
        N2_UNLOCK();
        if (!got) {
            break;
        }
        after = e.seq;
        if (n2_fmt_frame(buf, sizeof(buf), &e, 1) > 0) {
            publish(s_t_dump_fr, buf, 0, 0);
        }
    }
}

static void handle_cmd(const char *data, int len)
{
    n2_cmd_t cmd;
    n2_cmd_result_t r = n2_app_handle_cmd(data, (size_t)len, &cmd);
    char res[160];
    snprintf(res, sizeof(res), "{\"seq\":%" PRIu32 ",\"cmd\":\"%s\",\"result\":\"%s\"}", cmd.seq,
             n2_cmd_name(cmd.id), n2_cmd_result_name(r));
    publish(s_t_cmd_res, res, 1, 0);
    if (r != N2_CMDR_OK) {
        ESP_LOGW(TAG, "commande rejetée : %s", n2_cmd_result_name(r));
        return;
    }
    switch (cmd.id) {
    case N2_CMD_STATUS:
        publish_status();
        break;
    case N2_CMD_EVENTS:
        dump_events();
        break;
    case N2_CMD_FRAMES:
        dump_frames();
        break;
    case N2_CMD_OTA:
        n2_ota_start(cmd.arg);
        break;
    default:
        break; /* arm, disarm, debug : déjà exécutées par n2_app_handle_cmd */
    }
}

static void mqtt_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t ev = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT connecté (TLS)");
        esp_mqtt_client_subscribe(s_mqtt, s_t_cmd, 1);
        publish_status();
        n2_app_notify_publisher();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT déconnecté");
        break;
    case MQTT_EVENT_DATA:
        if (ev->topic_len == (int)strlen(s_t_cmd) && strncmp(ev->topic, s_t_cmd, (size_t)ev->topic_len) == 0) {
            if (ev->current_data_offset != 0 || ev->data_len != ev->total_data_len) {
                ESP_LOGW(TAG, "commande fragmentée ignorée");
                break;
            }
            handle_cmd(ev->data, ev->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "erreur MQTT");
        break;
    default:
        break;
    }
}

static void publisher_task(void *arg)
{
    (void)arg;
    char buf[384];
    uint32_t frames_seq = 0;
    int64_t next_counters = 0;
    int last_alarm = -1;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (!s_connected) {
            continue;
        }
        /* Événements non publiés (y compris ceux survenus hors réseau). */
        for (int i = 0; i < 32; i++) {
            n2_record_t r;
            N2_LOCK();
            int ok = g_n2.evlog_ok && evlog_next_unpublished(&g_n2.evlog, &r) == 0;
            N2_UNLOCK();
            if (!ok || n2_fmt_event(buf, sizeof(buf), &r) < 0 || publish(s_t_events, buf, 1, 0) < 0) {
                break;
            }
            N2_LOCK();
            evlog_mark_published(&g_n2.evlog, r.seq);
            N2_UNLOCK();
        }
        /* Trames : en erreur seulement, toutes et détaillées en mode debug. */
        for (int i = 0; i < 64; i++) {
            flog_entry_t e;
            N2_LOCK();
            int got = flog_get_after(&g_n2.flog, frames_seq, &e);
            bool dbg = g_n2.dbg.enabled;
            N2_UNLOCK();
            if (!got) {
                break;
            }
            frames_seq = e.seq;
            if ((e.f.flags != 0 || dbg) && n2_fmt_frame(buf, sizeof(buf), &e, dbg) > 0) {
                publish(s_t_frames, buf, 0, 0);
            }
        }
        /* État de l'alarme (retenu). */
        N2_LOCK();
        int st = (int)g_n2.alarm.state;
        flog_counters_t c = g_n2.flog.c;
        uint32_t pending = g_n2.evlog_ok ? evlog_pending(&g_n2.evlog) : 0;
        N2_UNLOCK();
        if (st != last_alarm) {
            snprintf(buf, sizeof(buf), "{\"state\":\"%s\"}", alarm_state_name((al_state_t)st));
            if (publish(s_t_alarm, buf, 1, 1) >= 0) {
                last_alarm = st;
            }
        }
        int64_t now = n2_uptime_ms();
        if (now >= next_counters) {
            next_counters = now + COUNTERS_PERIOD_MS;
            if (n2_fmt_counters(buf, sizeof(buf), &c, n2_app_edges_dropped(), pending) > 0) {
                publish(s_t_counters, buf, 0, 0);
            }
        }
    }
}

static void time_sync_cb(struct timeval *tv)
{
    n2_app_time_synced((int64_t)tv->tv_sec * 1000 + tv->tv_usec / 1000);
}

static void start_mqtt(void)
{
    const n2_prov_t *p = &g_n2.prov;
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = p->mqtt_uri,
        .broker.verification.certificate = p->mqtt_ca,
        .credentials.username = p->mqtt_user,
        .credentials.client_id = p->dev_id,
        .credentials.authentication.password = p->mqtt_pass,
        .session.last_will.topic = s_t_status,
        .session.last_will.msg = "{\"state\":\"offline\"}",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .session.keepalive = 60,
    };
    s_mqtt = esp_mqtt_client_init(&cfg);
    if (s_mqtt == NULL) {
        ESP_LOGE(TAG, "client MQTT non créé");
        return;
    }
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_handler, NULL);
    esp_mqtt_client_start(s_mqtt);
}

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi connecté");
        if (!s_started) {
            s_started = true;
            esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_N2_NTP_SERVER);
            sc.sync_cb = time_sync_cb;
            esp_netif_sntp_init(&sc);
            start_mqtt();
        }
    }
}

void n2_net_start(void)
{
    const n2_prov_t *p = &g_n2.prov;
    char *topics[] = { s_t_status, s_t_events, s_t_frames, s_t_counters, s_t_alarm,
                       s_t_dump_ev, s_t_dump_fr, s_t_cmd, s_t_cmd_res };
    const char *leaves[] = { N2_LEAF_STATUS, N2_LEAF_EVENTS, N2_LEAF_FRAMES, N2_LEAF_COUNTERS, N2_LEAF_ALARM,
                             N2_LEAF_DUMP_EV, N2_LEAF_DUMP_FR, N2_LEAF_CMD, N2_LEAF_CMD_RESULT };
    for (size_t i = 0; i < sizeof(topics) / sizeof(topics[0]); i++) {
        if (n2_topic(topics[i], TOPIC_LEN, p->dev_id, leaves[i]) < 0) {
            ESP_LOGE(TAG, "identifiant d'appareil invalide");
            return;
        }
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wic));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL, NULL));

    wifi_config_t wc;
    memset(&wc, 0, sizeof(wc));
    strlcpy((char *)wc.sta.ssid, p->wifi_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, p->wifi_pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; /* WPA2 minimum, WPA3 accepté */
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    xTaskCreate(publisher_task, "n2_pub", 6144, NULL, 5, &g_n2.publisher);
}
