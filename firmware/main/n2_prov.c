#include "n2_prov.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#include "n2_topics.h"

static const char *TAG = "n2_prov";

#define NS_PROV  "n2prov"
#define NS_STATE "n2state"

static bool get_str(nvs_handle_t h, const char *key, char *buf, size_t len)
{
    size_t n = len;
    if (nvs_get_str(h, key, buf, &n) != ESP_OK) {
        buf[0] = '\0';
        return false;
    }
    return buf[0] != '\0';
}

static char *get_str_alloc(nvs_handle_t h, const char *key)
{
    size_t n = 0;
    if (nvs_get_str(h, key, NULL, &n) != ESP_OK || n < 2 || n > 8192) {
        return NULL;
    }
    char *s = malloc(n);
    if (s != NULL && nvs_get_str(h, key, s, &n) != ESP_OK) {
        free(s);
        s = NULL;
    }
    return s;
}

static void get_u32(nvs_handle_t h, const char *key, uint32_t *v)
{
    uint32_t x;
    if (nvs_get_u32(h, key, &x) == ESP_OK) {
        *v = x;
    }
}

esp_err_t n2_prov_load(n2_prov_t *p)
{
    memset(p, 0, sizeof(*p));
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_PROV, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "aucun provisionnement (espace %s absent) : mode autonome, sans réseau", NS_PROV);
        return err;
    }
    bool ok = true;
    ok &= get_str(h, "dev_id", p->dev_id, sizeof(p->dev_id));
    ok &= get_str(h, "wifi_ssid", p->wifi_ssid, sizeof(p->wifi_ssid));
    get_str(h, "wifi_pass", p->wifi_pass, sizeof(p->wifi_pass));
    ok &= get_str(h, "mqtt_uri", p->mqtt_uri, sizeof(p->mqtt_uri));
    ok &= get_str(h, "mqtt_user", p->mqtt_user, sizeof(p->mqtt_user));
    ok &= get_str(h, "mqtt_pass", p->mqtt_pass, sizeof(p->mqtt_pass));
    get_str(h, "ota_prefix", p->ota_prefix, sizeof(p->ota_prefix));
    p->mqtt_ca = get_str_alloc(h, "mqtt_ca");
    p->ota_ca = get_str_alloc(h, "ota_ca");
    p->cmd_key_len = sizeof(p->cmd_key);
    if (nvs_get_blob(h, "cmd_key", p->cmd_key, &p->cmd_key_len) != ESP_OK) {
        p->cmd_key_len = 0;
    }
    get_u32(h, "ba_mute_ms", &p->ba_mute_ms);
    get_u32(h, "siren_max_s", &p->siren_max_s);
    get_u32(h, "exit_s", &p->exit_delay_s);
    get_u32(h, "entry_s", &p->entry_delay_s);
    get_u32(h, "v15_low_mv", &p->v15_low_mv);
    get_u32(h, "gap_mv", &p->gap_mv);
    size_t zl = sizeof(p->zone_type);
    p->zone_type_set = nvs_get_blob(h, "zone_types", p->zone_type, &zl) == ESP_OK && zl == sizeof(p->zone_type);
    nvs_close(h);

    if (!n2_device_id_valid(p->dev_id)) {
        ESP_LOGE(TAG, "dev_id invalide (attendu [a-z0-9_-]{1,32})");
        ok = false;
    }
    if (!n2_uri_is_tls(p->mqtt_uri)) {
        ESP_LOGE(TAG, "mqtt_uri doit commencer par mqtts:// (TLS obligatoire)");
        ok = false;
    }
    if (p->mqtt_ca == NULL) {
        ESP_LOGE(TAG, "mqtt_ca absent : le serveur MQTT ne peut pas être authentifié");
        ok = false;
    }
    if (p->cmd_key_len < 16) {
        ESP_LOGW(TAG, "cmd_key absente : toutes les commandes MQTT seront rejetées");
    }
    p->ok = ok;
    /* Aucune valeur secrète n'est journalisée. */
    ESP_LOGI(TAG, "provisionnement %s pour %s", ok ? "complet" : "incomplet", p->dev_id[0] ? p->dev_id : "?");
    return ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t n2_state_get_u32(const char *key, uint32_t *v)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_u32(h, key, v);
    nvs_close(h);
    return err;
}

esp_err_t n2_state_set_u32(const char *key, uint32_t v)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u32(h, key, v);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
