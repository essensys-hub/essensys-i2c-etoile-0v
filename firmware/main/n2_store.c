/* Stockage du journal d'événements (event_log) dans la partition NVS « evlog ». */
#include "n2_store.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "n2_store";
static nvs_handle_t s_h;

static int st_read(void *ctx, const char *key, void *buf, size_t len)
{
    (void)ctx;
    size_t n = len;
    if (nvs_get_blob(s_h, key, buf, &n) != ESP_OK || n != len) {
        return -1;
    }
    return 0;
}

static int st_write(void *ctx, const char *key, const void *buf, size_t len)
{
    (void)ctx;
    if (nvs_set_blob(s_h, key, buf, len) != ESP_OK || nvs_commit(s_h) != ESP_OK) {
        return -1;
    }
    return 0;
}

static int st_erase(void *ctx)
{
    (void)ctx;
    if (nvs_erase_all(s_h) != ESP_OK || nvs_commit(s_h) != ESP_OK) {
        return -1;
    }
    return 0;
}

static const evlog_backend_t s_backend = { st_read, st_write, st_erase, NULL };

esp_err_t n2_store_open(evlog_t *l, uint16_t cap)
{
    esp_err_t err = nvs_flash_init_partition(N2_EVLOG_PARTITION);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Partition illisible : on la réinitialise plutôt que de perdre les nouveaux événements. */
        ESP_LOGW(TAG, "partition %s réinitialisée (%s)", N2_EVLOG_PARTITION, esp_err_to_name(err));
        nvs_flash_erase_partition(N2_EVLOG_PARTITION);
        err = nvs_flash_init_partition(N2_EVLOG_PARTITION);
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_open_from_partition(N2_EVLOG_PARTITION, "ev", NVS_READWRITE, &s_h);
    if (err != ESP_OK) {
        return err;
    }
    if (evlog_open(l, &s_backend, cap) != 0) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "journal : %u événement(s), %u à publier", (unsigned)(l->next_seq - evlog_oldest(l)),
             (unsigned)evlog_pending(l));
    return ESP_OK;
}
