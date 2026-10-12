/*
 * Écoute passive du bus I2C BP <-> BA (tâche 3.2).
 *
 * Les broches d'écoute reçoivent les sorties de l'isolateur unidirectionnel
 * (bus -> module). Elles sont configurées en ENTRÉE, sans résistance de
 * tirage, et ne sont jamais reconfigurées. L'interruption sur fronts relève
 * les deux niveaux et l'horloge CPU dans une file ; la tâche de décodage les
 * rejoue dans le décodeur pur (n2_core/i2c_decoder). Le périphérique I2C
 * matériel n'est PAS utilisé pour l'écoute (un esclave I2C pourrait acquitter).
 */
#include "n2_sniffer.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

#include "edge_ring.h"
#include "essensys_proto.h"
#include "n2_app.h"
#include "n2_pins.h"

static const char *TAG = "n2_sniffer";

static edge_ring_t s_ring;
static gpio_isr_handle_t s_isr;
static int64_t s_uptime_offset_us; /* esp_timer - horloge des fronts */
static uint64_t s_cyc64;            /* horloge des fronts étendue à 64 bits */
static uint32_t s_last32;

static int lvl(int pin)
{
    return gpio_get_level((gpio_num_t)pin);
}

#define CPU_MHZ ((uint64_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
#define TICK_US 10000u

esp_err_t n2_sniffer_config_pins(void)
{
    gpio_config_t io = {
        .pin_bit_mask = N2_SNIFF_PINS_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

static void IRAM_ATTR sniff_isr(void *arg)
{
    (void)arg;
    uint32_t st = REG_READ(GPIO_STATUS_REG);
    REG_WRITE(GPIO_STATUS_W1TC_REG, st);
    uint32_t in = REG_READ(GPIO_IN_REG);
    uint32_t t = esp_cpu_get_cycle_count();
    uint32_t lv = ((in >> N2_PIN_SNIFF_SCL) & 1u) ? EDGE_LV_SCL : 0u;
    lv |= ((in >> N2_PIN_SNIFF_SDA) & 1u) ? EDGE_LV_SDA : 0u;
    edge_ring_push(&s_ring, t, lv);
}

uint32_t n2_sniffer_edges_dropped(void)
{
    return edge_ring_dropped(&s_ring);
}

/* Appelé par le décodeur (verrou pris) pour chaque transaction complète. */
static void on_transaction(const i2c_transaction_t *tx, void *user)
{
    (void)user;
    ess_frame_t f;
    ess_analyze(tx, &f);
    uint8_t src;
    int64_t up_ms = ((int64_t)tx->t_start_us + s_uptime_offset_us) / 1000;
    int64_t ts = timebase_now(&g_n2.tb, up_ms, &src);
    flog_push(&g_n2.flog, &f, ts, src);
    for (unsigned i = 0; i < tx->nseg; i++) {
        if (tx->seg[i].valid) {
            bh_on_addr(&g_n2.bh, tx->seg[i].addr, tx->seg[i].addr_ack, (uint32_t)(tx->t_start_us / 1000u));
        }
    }
    if (f.flags != 0 || g_n2.dbg.enabled) {
        n2_app_notify_publisher();
    }
}

static void decoder_task(void *arg)
{
    (void)arg;
    uint64_t cyc64 = s_cyc64;
    uint32_t last32 = s_last32;
    uint32_t seen_dropped = 0;
    uint64_t next_tick_us = 0;
    n2_evt_t evbuf[8];
    edge_rec_t r;

    for (;;) {
        int n = 0;
        N2_LOCK();
        uint32_t dropped = edge_ring_dropped(&s_ring);
        if (dropped != seen_dropped) {
            /* Fronts perdus : la transaction en cours est abandonnée et comptée. */
            seen_dropped = dropped;
            flog_add_lost(&g_n2.flog, 1);
            i2c_decoder_resync(&g_n2.dec, (uint8_t)lvl(N2_PIN_SNIFF_SCL), (uint8_t)lvl(N2_PIN_SNIFF_SDA));
        }
        while (n < 256 && edge_ring_pop(&s_ring, &r)) {
            int32_t d = (int32_t)(r.t - last32);
            if (d > 0) { /* un front relevé juste avant « now » reste daté à « now » */
                cyc64 += (uint32_t)d;
                last32 = r.t;
            }
            uint64_t t_us = cyc64 / CPU_MHZ;
            uint8_t scl = (r.lv & EDGE_LV_SCL) ? 1 : 0;
            uint8_t sda = (r.lv & EDGE_LV_SDA) ? 1 : 0;
            i2c_decoder_feed(&g_n2.dec, t_us, scl, sda);
            bh_on_levels(&g_n2.bh, scl, sda, (uint32_t)(t_us / 1000u));
            n++;
        }
        /* Horloge courante, prolongée sans perdre les tours du compteur 32 bits. */
        uint32_t now32 = esp_cpu_get_cycle_count();
        int32_t dn = (int32_t)(now32 - last32);
        if (dn > 0) {
            cyc64 += (uint32_t)dn;
            last32 = now32;
        }
        uint64_t now_us = cyc64 / CPU_MHZ;
        if (now_us >= next_tick_us) {
            next_tick_us = now_us + TICK_US;
            i2c_decoder_tick(&g_n2.dec, now_us);
            n2_evq_t q;
            n2_evq_init(&q, evbuf, 8);
            bh_tick(&g_n2.bh, (uint32_t)(now_us / 1000u), &q);
            if (q.n) {
                n2_app_post_locked(&q);
            }
        }
        N2_UNLOCK();
        if (n == 0) {
            vTaskDelay(1);
        } else {
            taskYIELD();
        }
    }
}

esp_err_t n2_sniffer_start(void)
{
    edge_ring_reset(&s_ring);
    s_last32 = esp_cpu_get_cycle_count();
    s_cyc64 = s_last32;
    s_uptime_offset_us = esp_timer_get_time() - (int64_t)(s_cyc64 / CPU_MHZ);
    int scl = lvl(N2_PIN_SNIFF_SCL);
    int sda = lvl(N2_PIN_SNIFF_SDA);
    N2_LOCK();
    i2c_decoder_init(&g_n2.dec, on_transaction, NULL, (uint8_t)scl, (uint8_t)sda);
    N2_UNLOCK();

    esp_err_t err = gpio_isr_register(sniff_isr, NULL, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, &s_isr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "interruption : %s", esp_err_to_name(err));
        return err;
    }
    gpio_set_intr_type(N2_PIN_SNIFF_SCL, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(N2_PIN_SNIFF_SDA, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(N2_PIN_SNIFF_SCL);
    gpio_intr_enable(N2_PIN_SNIFF_SDA);
    if (xTaskCreate(decoder_task, "n2_i2c_dec", 4096, NULL, configMAX_PRIORITIES - 3, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "écoute passive démarrée (SCL=GPIO%d, SDA=GPIO%d, entrées seules)", N2_PIN_SNIFF_SCL,
             N2_PIN_SNIFF_SDA);
    return ESP_OK;
}
