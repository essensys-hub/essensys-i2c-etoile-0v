#include "n2_hal.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/soc.h"

#include "n2_pins.h"

static const char *TAG = "n2_hal";

static i2c_master_bus_handle_t s_local_bus;
static i2c_master_dev_handle_t s_adc[N2_ADC_DEVICES];
static i2c_master_dev_handle_t s_rtc;
static i2c_master_dev_handle_t s_exp;
static SemaphoreHandle_t s_bus_lock;
static uint8_t s_exp_out = 0xFF;

#define LOCAL_TIMEOUT_MS 20

/* ---------- RV-3028 (U7) ---------- */
#define RV_STATUS        0x0E
#define RV_CTRL1         0x0F
#define RV_UNIX0         0x1B
#define RV_EECMD         0x27
#define RV_EE_BACKUP     0x37
#define RV_STATUS_EEBUSY 0x80
#define RV_CTRL1_EERD    0x08
#define RV_BSM_MASK      0x0C
#define RV_BSM_DSM       0x04   /* basculement direct sur RTC_VB (supercondensateur C20) */

static esp_err_t dev_read(i2c_master_dev_handle_t d, uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(d, &reg, 1, buf, n, LOCAL_TIMEOUT_MS);
}

static esp_err_t dev_write(i2c_master_dev_handle_t d, uint8_t reg, const uint8_t *buf, size_t n)
{
    uint8_t tmp[8];
    if (n > sizeof(tmp) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    tmp[0] = reg;
    memcpy(tmp + 1, buf, n);
    return i2c_master_transmit(d, tmp, n + 1, LOCAL_TIMEOUT_MS);
}

/* Active le basculement sur la sauvegarde (registre EEPROM 0x37), une seule fois.
 * TODO(banc) : vérifier la tenue de l'heure, batterie et alimentation retirées. */
static void rv_ensure_backup_switchover(void)
{
    uint8_t v;
    if (dev_read(s_rtc, RV_EE_BACKUP, &v, 1) != ESP_OK || (v & RV_BSM_MASK) == RV_BSM_DSM) {
        return;
    }
    uint8_t c1;
    if (dev_read(s_rtc, RV_CTRL1, &c1, 1) != ESP_OK) {
        return;
    }
    uint8_t c1_eerd = (uint8_t)(c1 | RV_CTRL1_EERD);
    uint8_t nv = (uint8_t)((v & ~RV_BSM_MASK) | RV_BSM_DSM);
    uint8_t cmd0 = 0x00, cmd_update = 0x11;
    if (dev_write(s_rtc, RV_CTRL1, &c1_eerd, 1) == ESP_OK && dev_write(s_rtc, RV_EE_BACKUP, &nv, 1) == ESP_OK &&
        dev_write(s_rtc, RV_EECMD, &cmd0, 1) == ESP_OK && dev_write(s_rtc, RV_EECMD, &cmd_update, 1) == ESP_OK) {
        for (int i = 0; i < 20; i++) {
            uint8_t st;
            vTaskDelay(pdMS_TO_TICKS(10));
            if (dev_read(s_rtc, RV_STATUS, &st, 1) == ESP_OK && !(st & RV_STATUS_EEBUSY)) {
                break;
            }
        }
        ESP_LOGI(TAG, "RTC : basculement sur sauvegarde activé");
    }
    dev_write(s_rtc, RV_CTRL1, &c1, 1);
}

esp_err_t n2_hal_rtc_read(int64_t *epoch_ms)
{
    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t a[4], b[4];
    esp_err_t err = ESP_FAIL;
    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    /* Compteur UNIX non verrouillé : deux lectures identiques requises. */
    for (int i = 0; i < 3; i++) {
        if (dev_read(s_rtc, RV_UNIX0, a, 4) == ESP_OK && dev_read(s_rtc, RV_UNIX0, b, 4) == ESP_OK &&
            memcmp(a, b, 4) == 0) {
            err = ESP_OK;
            break;
        }
    }
    xSemaphoreGive(s_bus_lock);
    if (err == ESP_OK) {
        uint32_t s = (uint32_t)a[0] | ((uint32_t)a[1] << 8) | ((uint32_t)a[2] << 16) | ((uint32_t)a[3] << 24);
        *epoch_ms = (int64_t)s * 1000;
    }
    return err;
}

esp_err_t n2_hal_rtc_write(int64_t epoch_ms)
{
    if (s_rtc == NULL || epoch_ms < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t s = (uint32_t)(epoch_ms / 1000);
    uint8_t b[4] = { (uint8_t)s, (uint8_t)(s >> 8), (uint8_t)(s >> 16), (uint8_t)(s >> 24) };
    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    esp_err_t err = dev_write(s_rtc, RV_UNIX0, b, 4);
    xSemaphoreGive(s_bus_lock);
    return err;
}

/* ---------- TCA9534 (U8) ---------- */
#define EXP_REG_IN   0x00
#define EXP_REG_OUT  0x01
#define EXP_REG_CFG  0x03

esp_err_t n2_hal_exp_read(uint8_t *port)
{
    if (s_exp == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    esp_err_t err = dev_read(s_exp, EXP_REG_IN, port, 1);
    xSemaphoreGive(s_bus_lock);
    return err;
}

esp_err_t n2_hal_alarm_led(bool on)
{
    if (s_exp == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t v = on ? (uint8_t)(s_exp_out & ~(1u << N2_EXP_LED_ALM)) : (uint8_t)(s_exp_out | (1u << N2_EXP_LED_ALM));
    if (v == s_exp_out) {
        return ESP_OK;
    }
    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    esp_err_t err = dev_write(s_exp, EXP_REG_OUT, &v, 1);
    xSemaphoreGive(s_bus_lock);
    if (err == ESP_OK) {
        s_exp_out = v;
    }
    return err;
}

/* ---------- MCP3428 x4 (U10..U13) ---------- */
/* Octet de configuration : /RDY=1 (lancer), voie, conversion unique, 240 éch./s (12 bits), gain 1. */
#define MCP_CFG(ch) ((uint8_t)(0x80u | ((unsigned)(ch) << 5)))

esp_err_t n2_hal_adc_read_all(int16_t raw[N2_ADC_COUNT])
{
    esp_err_t err = ESP_OK;
    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    for (unsigned ch = 0; ch < N2_ADC_CH_PER_DEV && err == ESP_OK; ch++) {
        /* Les quatre convertisseurs mesurent la voie ch en parallèle. */
        for (unsigned d = 0; d < N2_ADC_DEVICES && err == ESP_OK; d++) {
            uint8_t cfg = MCP_CFG(ch);
            err = s_adc[d] ? i2c_master_transmit(s_adc[d], &cfg, 1, LOCAL_TIMEOUT_MS) : ESP_ERR_INVALID_STATE;
        }
        vTaskDelay(pdMS_TO_TICKS(5)); /* 4,2 ms à 240 éch./s */
        for (unsigned d = 0; d < N2_ADC_DEVICES && err == ESP_OK; d++) {
            uint8_t rx[3] = { 0, 0, 0x80 };
            for (int tries = 0; tries < 4; tries++) {
                err = i2c_master_receive(s_adc[d], rx, 3, LOCAL_TIMEOUT_MS);
                if (err != ESP_OK || !(rx[2] & 0x80u)) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            if (err == ESP_OK && (rx[2] & 0x80u)) {
                err = ESP_ERR_TIMEOUT;
            }
            raw[d * N2_ADC_CH_PER_DEV + ch] = (int16_t)(((uint16_t)rx[0] << 8) | rx[1]);
        }
    }
    xSemaphoreGive(s_bus_lock);
    return err;
}

/* ---------- GPIO ---------- */

int n2_hal_pin_is_output(int pin, void *ctx)
{
    (void)ctx;
    if (pin < 0 || pin > 21) {
        return 1;
    }
    uint32_t en = REG_READ(GPIO_ENABLE_REG);
    uint32_t sel = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4u * (uint32_t)pin) & 0xFFu;
    return ((en >> pin) & 1u) || sel != SIG_GPIO_OUT_IDX;
}

int n2_hal_level(int pin)
{
    return gpio_get_level((gpio_num_t)pin);
}

void n2_hal_siren(bool on)
{
    gpio_set_level(N2_PIN_SIREN_EN, on ? 1 : 0);
}

void n2_hal_led(bool on)
{
    gpio_set_level(N2_PIN_STATUS_LED, on ? 0 : 1); /* LED1 active basse */
}

void n2_hal_latch_reset_pulse(void)
{
    gpio_set_level(N2_PIN_LATCH_RST, 0); /* /CLR du 74HC74 */
    esp_rom_delay_us(20);
    gpio_set_level(N2_PIN_LATCH_RST, 1);
}

esp_err_t n2_hal_init(void)
{
    s_bus_lock = xSemaphoreCreateMutex();

    /* Sorties locales : niveaux sûrs (sirène coupée, latch non effacé, LED éteinte) avant activation. */
    _Static_assert((N2_PLAIN_OUTPUT_PINS_MASK & N2_SNIFF_PINS_MASK) == 0, "sortie sur broche d'ecoute");
    gpio_set_level(N2_PIN_SIREN_EN, 0);
    gpio_set_level(N2_PIN_LATCH_RST, 1);
    gpio_set_level(N2_PIN_STATUS_LED, 1);
    gpio_config_t out = {
        .pin_bit_mask = N2_PLAIN_OUTPUT_PINS_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    gpio_config_t in = {
        .pin_bit_mask = N2_INPUT_PINS_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, /* tirages externes R14, R15, R19 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    /* Bus I2C LOCAL : jamais les broches d'écoute du bus Essensys. */
    i2c_master_bus_config_t bc = {
        .i2c_port = 0,
        .sda_io_num = N2_PIN_LOCAL_SDA,
        .scl_io_num = N2_PIN_LOCAL_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* R17/R18 4,7 kΩ */
    };
    esp_err_t err = i2c_new_master_bus(&bc, &s_local_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus local : %s", esp_err_to_name(err));
        return err;
    }
    static const uint8_t adc_addrs[N2_ADC_DEVICES] = N2_ADC_ADDRS;
    for (unsigned i = 0; i < N2_ADC_DEVICES + 2u; i++) {
        uint16_t addr = i < N2_ADC_DEVICES ? adc_addrs[i] : (i == N2_ADC_DEVICES ? N2_RTC_I2C_ADDR : N2_EXP_I2C_ADDR);
        i2c_master_dev_handle_t *h = i < N2_ADC_DEVICES ? &s_adc[i] : (i == N2_ADC_DEVICES ? &s_rtc : &s_exp);
        i2c_device_config_t dc = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = N2_LOCAL_I2C_HZ,
        };
        if (i2c_master_bus_add_device(s_local_bus, &dc, h) != ESP_OK || i2c_master_probe(s_local_bus, addr, LOCAL_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGE(TAG, "périphérique local 0x%02x absent", addr);
        }
    }

    xSemaphoreTake(s_bus_lock, portMAX_DELAY);
    if (s_exp != NULL) {
        uint8_t cfg = (uint8_t)~N2_EXP_OUTPUTS_MASK; /* P6 en sortie, le reste en entrée */
        if (dev_write(s_exp, EXP_REG_OUT, &s_exp_out, 1) != ESP_OK || dev_write(s_exp, EXP_REG_CFG, &cfg, 1) != ESP_OK) {
            ESP_LOGE(TAG, "TCA9534 : configuration impossible");
        }
    }
    if (s_rtc != NULL) {
        rv_ensure_backup_switchover();
    }
    xSemaphoreGive(s_bus_lock);
    return ESP_OK;
}
