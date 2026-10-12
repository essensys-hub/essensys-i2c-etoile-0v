/*
 * Abstraction matérielle de la carte N2 côté module (révision B, n2_pins.h et n2_board.h).
 * Aucune fonction de ce module ne pilote le bus I2C Essensys. Les seules sorties
 * sont la sirène, les LED, le réarmement du latch et le bus I2C LOCAL (RTC, ADC,
 * extension d'E/S), sur des broches distinctes des broches d'écoute.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "n2_board.h"

esp_err_t n2_hal_init(void);
/* 1 si la broche a sa sortie activée (registre d'activation ou signal de la matrice). */
int n2_hal_pin_is_output(int pin, void *ctx);
/* 16 voies, codes 12 bits signés (1 mV/pas). Environ 22 ms. */
esp_err_t n2_hal_adc_read_all(int16_t raw[N2_ADC_COUNT]);
/* Port d'entrée du TCA9534 (bits N2_EXP_*). */
esp_err_t n2_hal_exp_read(uint8_t *port);
esp_err_t n2_hal_alarm_led(bool on);
int n2_hal_level(int pin);
void n2_hal_siren(bool on);
void n2_hal_led(bool on);
void n2_hal_latch_reset_pulse(void);
esp_err_t n2_hal_rtc_read(int64_t *epoch_ms);
esp_err_t n2_hal_rtc_write(int64_t epoch_ms);
