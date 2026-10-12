/*
 * Provisionnement par appareil, HORS DÉPÔT : partition NVS (chiffrée en
 * release) générée depuis provisioning/n2_prov.csv (non versionné, voir
 * provisioning/n2_prov.csv.example et firmware/README.md).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char dev_id[33];
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_uri[129];
    char mqtt_user[65];
    char mqtt_pass[129];
    char ota_prefix[129];
    char *mqtt_ca;        /* PEM, alloué */
    char *ota_ca;         /* PEM, alloué (absent = OTA désactivée) */
    uint8_t cmd_key[32];
    size_t cmd_key_len;
    bool ok;              /* identifiants réseau complets et valides */
    /* Surcharges facultatives des paramètres (0 = valeur Kconfig). */
    uint32_t ba_mute_ms;
    uint32_t siren_max_s;
    uint32_t exit_delay_s;
    uint32_t entry_delay_s;
    uint32_t v15_low_mv;
    uint32_t gap_mv;
    uint8_t zone_type[4];
    bool zone_type_set;
} n2_prov_t;

esp_err_t n2_prov_load(n2_prov_t *p);
esp_err_t n2_state_get_u32(const char *key, uint32_t *v);
esp_err_t n2_state_set_u32(const char *key, uint32_t v);
