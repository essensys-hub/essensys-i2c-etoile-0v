/*
 * OTA signée (tâche 7.3).
 *  1. URL https sous le préfixe provisionné, serveur authentifié par ota_ca ;
 *  2. image de longueur compatible avec une image signée et bloc de signature
 *     Secure Boot V2 présent (contrôle de forme, ota_policy) : sinon rejet ;
 *  3. vérification RSA-PSS par ESP-IDF à esp_https_ota_finish()
 *     (CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT en dev, Secure Boot V2 en release) ;
 *  4. retour arrière automatique si la nouvelle image ne se valide pas
 *     (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE).
 */
#include "n2_ota.h"

#include <stdlib.h>
#include <string.h>

#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "n2_app.h"
#include "ota_policy.h"

static const char *TAG = "n2_ota";
static volatile bool s_busy;

static n2_ota_result_t run(const char *url)
{
    esp_http_client_config_t http = {
        .url = url,
        .cert_pem = g_n2.prov.ota_ca,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t cfg = { .http_config = &http };
    esp_https_ota_handle_t h = NULL;
    if (esp_https_ota_begin(&cfg, &h) != ESP_OK) {
        return N2_OTA_IO;
    }
    esp_err_t err;
    while ((err = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    }
    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(h)) {
        esp_https_ota_abort(h);
        return N2_OTA_IO;
    }
    int len = esp_https_ota_get_image_len_read(h);
    if (len <= 0 || !n2_ota_layout_ok((size_t)len)) {
        esp_https_ota_abort(h);
        return N2_OTA_LAYOUT;
    }
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    uint8_t *blk = malloc(N2_SIG_BLOCK_LEN);
    bool sig_ok = blk != NULL && part != NULL &&
                  esp_partition_read(part, (size_t)len - N2_SIG_SECTOR_LEN, blk, N2_SIG_BLOCK_LEN) == ESP_OK &&
                  n2_ota_sig_block_ok(blk, N2_SIG_BLOCK_LEN);
    free(blk);
    if (!sig_ok) {
        esp_https_ota_abort(h);
        return N2_OTA_NO_SIG;
    }
    err = esp_https_ota_finish(h);
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        return N2_OTA_VERIFY;
    }
    return err == ESP_OK ? N2_OTA_OK : N2_OTA_IO;
}

static void ota_task(void *arg)
{
    char *url = arg;
    n2_ota_result_t r = run(url);
    free(url);
    if (r == N2_OTA_OK) {
        ESP_LOGI(TAG, "image signée acceptée, redémarrage");
        n2_app_post_one(N2_EV_OTA_APPLIED, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    ESP_LOGW(TAG, "image rejetée (raison %d)", (int)r);
    n2_app_post_one(N2_EV_OTA_REJECTED, 0, (int32_t)r);
    s_busy = false;
    vTaskDelete(NULL);
}

esp_err_t n2_ota_start(const char *url)
{
    if (s_busy) {
        return ESP_ERR_INVALID_STATE;
    }
    if (g_n2.prov.ota_ca == NULL || !n2_ota_url_ok(url, g_n2.prov.ota_prefix)) {
        n2_app_post_one(N2_EV_OTA_REJECTED, 0, N2_OTA_URL_REJECTED);
        return ESP_ERR_INVALID_ARG;
    }
    char *copy = strdup(url);
    if (copy == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_busy = true;
    if (xTaskCreate(ota_task, "n2_ota", 8192, copy, 4, NULL) != pdPASS) {
        free(copy);
        s_busy = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
