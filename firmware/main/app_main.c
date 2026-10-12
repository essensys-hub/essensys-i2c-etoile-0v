/*
 * Module ESP32-C3 de la carte N2 : espion I2C passif, surveillance du bus et
 * du +15 V, alarme intrusion, journal des surtensions, publication MQTT/TLS.
 * Change OpenSpec n2-esp32-spy-alarm-2026-10-008.
 */
#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "n2_app.h"
#include "n2_board.h"
#include "n2_console.h"
#include "n2_hal.h"
#include "n2_net.h"
#include "n2_pins.h"
#include "n2_sniffer.h"
#include "n2_store.h"
#include "pin_guard.h"

static const char *TAG = "n2";

n2_app_t g_n2;
static flog_entry_t s_frames[N2_FRAME_LOG_CAP];

#define MONITOR_PERIOD_MS   50
#define BUTTON_LONG_MS      3000
#define PIN_RECHECK_MS      10000
#define APP_VALID_AFTER_MS  60000

int64_t n2_uptime_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void n2_app_notify_publisher(void)
{
    if (g_n2.publisher != NULL) {
        xTaskNotifyGive(g_n2.publisher);
    }
}

uint32_t n2_app_edges_dropped(void)
{
    return g_n2.spy_running ? n2_sniffer_edges_dropped() : 0u;
}

void n2_app_post_locked(n2_evq_t *q)
{
    uint32_t now = n2_now_ms();
    /* La file peut grandir pendant le parcours (événements sirène). */
    for (size_t i = 0; i < q->n; i++) {
        n2_evt_t e = q->buf[i];
        uint8_t src;
        int64_t ts = timebase_now(&g_n2.tb, n2_uptime_ms(), &src);
        if (g_n2.evlog_ok) {
            evlog_append(&g_n2.evlog, ts, src, &e, NULL);
        }
        ESP_LOGI(TAG, "événement %s ch=%u val=%ld", n2_ev_name(e.type), e.channel, (long)e.value);
        if (e.type == N2_EV_SIREN_TRIGGER) {
            siren_start(&g_n2.siren, now, q);
        } else if (e.type == N2_EV_DISARMED) {
            siren_stop(&g_n2.siren, N2_SIREN_OFF_STOP, q);
        }
    }
    if (q->dropped) {
        ESP_LOGW(TAG, "%lu événement(s) non enregistrés (file pleine)", (unsigned long)q->dropped);
    }
    if (q->n) {
        n2_app_notify_publisher();
    }
}

void n2_app_post(n2_evq_t *q)
{
    N2_LOCK();
    n2_app_post_locked(q);
    N2_UNLOCK();
}

void n2_app_post_one(uint8_t type, uint8_t channel, int32_t value)
{
    n2_evt_t buf[4];
    n2_evq_t q;
    n2_evq_init(&q, buf, 4);
    n2_evq_push(&q, type, channel, value);
    n2_app_post(&q);
}

n2_cmd_result_t n2_app_handle_cmd(const char *payload, size_t len, n2_cmd_t *out)
{
    n2_evt_t buf[16];
    n2_evq_t q;
    n2_evq_init(&q, buf, 16);
    N2_LOCK();
    uint32_t before = g_n2.cmd.last_seq;
    n2_cmd_result_t r = n2_cmd_handle(&g_n2.cmd, payload, len, n2_now_ms(), n2_uptime_ms(), &q, out);
    uint32_t after = g_n2.cmd.last_seq;
    n2_app_post_locked(&q);
    N2_UNLOCK();
    if (after != before) {
        n2_state_set_u32("cmd_seq", after); /* anti-rejeu persistant */
    }
    return r;
}

void n2_app_local_arm(bool arm, al_source_t src)
{
    n2_evt_t buf[8];
    n2_evq_t q;
    n2_evq_init(&q, buf, 8);
    N2_LOCK();
    if (arm) {
        alarm_arm(&g_n2.alarm, src, n2_now_ms(), &q);
    } else {
        alarm_disarm(&g_n2.alarm, src, &q);
    }
    n2_app_post_locked(&q);
    N2_UNLOCK();
}

void n2_app_local_debug(bool on, uint32_t ttl_s)
{
    n2_evt_t buf[4];
    n2_evq_t q;
    n2_evq_init(&q, buf, 4);
    N2_LOCK();
    if (on) {
        n2_debug_enable(&g_n2.dbg, N2_DBG_SRC_LOCAL, ttl_s ? ttl_s : g_n2.debug_ttl_s, n2_uptime_ms(), &q);
    } else {
        n2_debug_disable(&g_n2.dbg, &q);
    }
    n2_app_post_locked(&q);
    N2_UNLOCK();
}

int n2_app_local_clear_events(void)
{
    int r = -1;
    N2_LOCK();
    if (g_n2.evlog_ok && evlog_clear_local(&g_n2.evlog) == 0) {
        r = 0;
    }
    N2_UNLOCK();
    if (r == 0) {
        n2_app_post_one(N2_EV_LOG_CLEARED, 0, (int32_t)g_n2.evlog.first_seq);
    }
    return r;
}

void n2_app_time_synced(int64_t epoch_ms)
{
    N2_LOCK();
    int r = timebase_set(&g_n2.tb, epoch_ms, n2_uptime_ms(), N2_TS_NTP);
    N2_UNLOCK();
    if (r == 0) {
        n2_hal_rtc_write(epoch_ms);
        ESP_LOGI(TAG, "heure NTP acquise, RTC mise à jour");
    }
}

/* ---------- configuration ---------- */

static void configure_modules(void)
{
    const n2_prov_t *p = &g_n2.prov;

    bh_config_t bc;
    bh_default_config(&bc);
    bc.ba_mute_ms = p->ba_mute_ms ? p->ba_mute_ms : CONFIG_N2_BA_MUTE_TIMEOUT_MS;
    bc.stuck_ms = CONFIG_N2_LINE_STUCK_MS;
    bh_init(&g_n2.bh, &bc, 1, 1, 0);

    pm_config_t pc;
    pm_default_config(&pc);
    pc.v15_low_mv = (int32_t)(p->v15_low_mv ? p->v15_low_mv : CONFIG_N2_V15_LOW_MV);
    pm_init(&g_n2.pm, &pc);

    surge_config_t sc;
    surge_default_config(&sc);
    sc.gap_threshold_mv = (int32_t)(p->gap_mv ? p->gap_mv : CONFIG_N2_ZERO_V_GAP_MV);
    surge_init(&g_n2.surge, &sc, n2_now_ms());

    al_config_t ac;
    alarm_default_config(&ac);
    ac.exit_delay_ms = 1000u * (p->exit_delay_s ? p->exit_delay_s : CONFIG_N2_EXIT_DELAY_S);
    ac.entry_delay_ms = 1000u * (p->entry_delay_s ? p->entry_delay_s : CONFIG_N2_ENTRY_DELAY_S);
    if (p->zone_type_set) {
        memcpy(ac.zone_type, p->zone_type, sizeof(ac.zone_type));
    }
    alarm_init(&g_n2.alarm, &ac);

    siren_config_t sic;
    siren_default_config(&sic);
    sic.max_ms = 1000u * (p->siren_max_s ? p->siren_max_s : CONFIG_N2_SIREN_MAX_S);
    siren_init(&g_n2.siren, &sic);

    n2_debug_init(&g_n2.dbg); /* mode debug toujours désactivé au démarrage */
    g_n2.debug_ttl_s = CONFIG_N2_DEBUG_DEFAULT_TTL_S;

    flog_init(&g_n2.flog, s_frames, N2_FRAME_LOG_CAP);

    g_n2.cmd.alarm = &g_n2.alarm;
    g_n2.cmd.dbg = &g_n2.dbg;
    g_n2.cmd.key = p->cmd_key;
    g_n2.cmd.keylen = p->cmd_key_len;
    g_n2.cmd.device_id = p->dev_id;
    uint32_t seq = 0;
    n2_state_get_u32("cmd_seq", &seq);
    g_n2.cmd.last_seq = seq;
}

/* ---------- surveillance périodique ---------- */

typedef struct {
    uint8_t stable;
    uint8_t cnt;
} din_t;

static bool din_update(din_t *d, uint8_t raw, uint8_t n)
{
    if (raw == d->stable) {
        d->cnt = 0;
        return false;
    }
    if (++d->cnt >= n) {
        d->stable = raw;
        d->cnt = 0;
        return true;
    }
    return false;
}

static void monitor_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    din_t key = { 0, 0 }, btn = { 0, 0 };
    uint32_t btn_since = 0;
    bool btn_done = false;
    uint32_t next_pin_check = 0;
    bool pin_alarm = false;
    bool app_valid = false;
    bool io_warned = false;
    uint8_t key_arm = 0;
    uint8_t siren_fault = 0;
    bool alarm_led = false;
    n2_evt_t evbuf[32];

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(MONITOR_PERIOD_MS));
        uint32_t now = n2_now_ms();
        n2_evq_t q;
        n2_evq_init(&q, evbuf, 32);
        siren_fault = 0;

        int16_t raw[N2_ADC_COUNT];
        bool adc_ok = n2_hal_adc_read_all(raw) == ESP_OK;
        uint8_t exp = 0;
        bool exp_ok = n2_hal_exp_read(&exp) == ESP_OK;
        if ((!adc_ok || !exp_ok) && !io_warned) {
            ESP_LOGE(TAG, "ADC ou extension d'E/S illisible : surveillance partielle");
            io_warned = true;
        }
        uint8_t latch = 0;
        if (exp_ok) {
            latch |= ((exp >> N2_EXP_LATCH_0V) & 1u) ? SURGE_LATCH_0V : 0u;
            latch |= ((exp >> N2_EXP_LATCH_OV) & 1u) ? SURGE_LATCH_OV : 0u;
            key_arm = (uint8_t)!((exp >> N2_EXP_ARM_IN) & 1u); /* actif bas */
        }
        /* TODO(schéma) : pas d'entrée d'autoprotection du coffret en révision B ;
         * câbler le contact sur une boucle configurée en zone 24 h. */
        const uint8_t box_tamper = 0;
        uint8_t btn_down = (uint8_t)!n2_hal_level(N2_PIN_USER_BTN);

        N2_LOCK();
        surge_sample_t ss = { .latch = latch };
        if (adc_ok) {
            pm_sample_t ps = {
                .v15_bp = n2_adc_to_mv(N2_ADC_V15_BP, raw[N2_ADC_V15_BP]),
                .vbat = n2_adc_to_mv(N2_ADC_VBAT, raw[N2_ADC_VBAT]),
            };
            ss.v15_mv = ps.v15_bp;
            for (unsigned i = 0; i < PM_NUM_PORTS; i++) {
                ps.v15_sw[i] = n2_adc_to_mv(N2_ADC_V15_SW1 + (int)i, raw[N2_ADC_V15_SW1 + i]);
                ps.v15_ba[i] = n2_adc_to_mv(N2_ADC_V15_BA1 + (int)i, raw[N2_ADC_V15_BA1 + i]);
                ss.dv0_mv[i] = n2_adc_to_mv(N2_ADC_DV0_BA1 + (int)i, raw[N2_ADC_DV0_BA1 + i]);
            }
            pm_update(&g_n2.pm, &ps, &q);
            uint16_t zpm[AL_ZONES];
            for (unsigned i = 0; i < AL_ZONES; i++) {
                zpm[i] = n2_zone_permille(raw[N2_ADC_ZONE1 + i]);
            }
            alarm_step(&g_n2.alarm, zpm, box_tamper, now, &q);
            siren_fault = siren_feedback_fault(&g_n2.siren, now, n2_adc_to_mv(N2_ADC_SIREN, raw[N2_ADC_SIREN]));
        }
        /* Sans ADC, une impulsion capturée est quand même enregistrée (valeurs à 0). */
        if (surge_update(&g_n2.surge, &ss, now, &q)) {
            n2_hal_latch_reset_pulse();
        }

        /* Contact à clé : armement / désarmement local. */
        if (din_update(&key, key_arm, 3)) {
            if (key.stable) {
                alarm_arm(&g_n2.alarm, AL_SRC_LOCAL_KEY, now, &q);
            } else {
                alarm_disarm(&g_n2.alarm, AL_SRC_LOCAL_KEY, &q);
            }
        }
        /* Bouton : appui long = bascule du mode debug local. */
        if (din_update(&btn, btn_down, 2)) {
            btn_since = now;
            btn_done = false;
        }
        if (btn.stable && !btn_done && now - btn_since >= BUTTON_LONG_MS) {
            btn_done = true;
            if (g_n2.dbg.enabled) {
                n2_debug_disable(&g_n2.dbg, &q);
            } else {
                n2_debug_enable(&g_n2.dbg, N2_DBG_SRC_LOCAL, g_n2.debug_ttl_s, n2_uptime_ms(), &q);
            }
        }
        n2_debug_active(&g_n2.dbg, n2_uptime_ms(), &q);

        n2_app_post_locked(&q); /* déclenche/arrête la sirène selon les événements */
        n2_evq_init(&q, evbuf, 32);
        uint8_t out = siren_step(&g_n2.siren, now, siren_fault, &q);
        al_state_t st = g_n2.alarm.state;
        if (q.n) {
            n2_app_post_locked(&q);
        }
        N2_UNLOCK();

        n2_hal_siren(out != 0);
        n2_hal_led(st == AL_ALARM ? ((now / 250u) & 1u) : (st != AL_DISARMED));
        bool want_alarm_led = (st == AL_ALARM);
        if (want_alarm_led != alarm_led && n2_hal_alarm_led(want_alarm_led) == ESP_OK) {
            alarm_led = want_alarm_led;
        }

        /* Garde périodique des broches d'écoute. */
        if ((int32_t)(now - next_pin_check) >= 0) {
            next_pin_check = now + PIN_RECHECK_MS;
            int bad = n2_pin_guard_check(N2_SNIFF_PINS_MASK, n2_hal_pin_is_output, NULL);
            if (bad >= 0 && !pin_alarm) {
                pin_alarm = true;
                ESP_LOGE(TAG, "broche d'écoute GPIO%d en sortie : reconfiguration en entrée", bad);
                n2_sniffer_config_pins();
                n2_app_post_one(N2_EV_PIN_GUARD_FAIL, (uint8_t)bad, 0);
            }
        }
        if (!app_valid && now >= APP_VALID_AFTER_MS) {
            app_valid = true;
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}

void app_main(void)
{
    g_n2.lock = xSemaphoreCreateMutex();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS principale illisible : effacement (le provisionnement doit être refait)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    n2_prov_load(&g_n2.prov);
    timebase_init(&g_n2.tb);
    configure_modules();
    g_n2.evlog_ok = n2_store_open(&g_n2.evlog, CONFIG_N2_EVLOG_CAPACITY) == ESP_OK;
    if (!g_n2.evlog_ok) {
        ESP_LOGE(TAG, "journal non volatil indisponible");
    }

    /* Écoute en entrée AVANT tout autre pilote, puis garde de démarrage. */
    ESP_ERROR_CHECK(n2_sniffer_config_pins());
    n2_hal_init();

    int64_t rtc_ms;
    if (n2_hal_rtc_read(&rtc_ms) == ESP_OK &&
        timebase_set(&g_n2.tb, rtc_ms, n2_uptime_ms(), N2_TS_RTC) == 0) {
        ESP_LOGI(TAG, "heure RTC valide");
    } else {
        ESP_LOGW(TAG, "RTC invalide : horodatage relatif jusqu'à la synchronisation NTP");
    }
    n2_app_post_one(N2_EV_BOOT, 0, (int32_t)esp_reset_reason());

    int bad = n2_pin_guard_check(N2_SNIFF_PINS_MASK, n2_hal_pin_is_output, NULL);
    if (bad >= 0) {
        /* Assertion de démarrage : l'écoute n'est pas lancée, l'alarme reste active. */
        ESP_LOGE(TAG, "GARDE : broche d'écoute GPIO%d en sortie, écoute désactivée", bad);
        n2_app_post_one(N2_EV_PIN_GUARD_FAIL, (uint8_t)bad, 0);
    } else {
        g_n2.spy_running = n2_sniffer_start() == ESP_OK;
    }

    xTaskCreate(monitor_task, "n2_monitor", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
    n2_console_start();
    if (g_n2.prov.ok) {
        n2_net_start();
    } else {
        ESP_LOGW(TAG, "réseau non démarré : provisionnement incomplet (voir README)");
    }
}
