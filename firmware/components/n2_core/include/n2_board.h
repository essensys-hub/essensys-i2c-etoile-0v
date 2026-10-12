/*
 * Chaînes de mesure de la carte N2, révision B (design D4, scripts/n2_module.py bloc D).
 *
 * Quatre ADC MCP3428 (4 voies différentielles, référence interne 2,048 V) sur le
 * bus I2C local, utilisés en 12 bits (240 éch./s, 1 mV par pas, gain 1) :
 *   U10 : +15 V BP, amont PTC 1..3        (÷16 : 150 kΩ / 10 kΩ)
 *   U11 : aval PTC 1..3, VBAT             (÷16 ; VBAT ÷2)
 *   U12 : boucles Z1..Z4                  (tirage 10 kΩ vers 3,3 V, 1 kΩ série, FDL 2 x 4,7 kΩ)
 *   U13 : écarts 0 V 1..3 (VDIFF - VREF, G = 0,249), sortie sirène (÷11)
 * TODO(recoupement schéma, tâche 1.2) : adresses (Adr0/Adr1, table 5-3 de la
 * fiche MCP3428 à confirmer) et rapports à revérifier sur le schéma figé.
 */
#ifndef N2_BOARD_H
#define N2_BOARD_H

#include <stdint.h>

#define N2_RTC_I2C_ADDR  0x52u   /* RV-3028-C7 (U7) */
#define N2_EXP_I2C_ADDR  0x20u   /* TCA9534 (U8), A0..A2 à la masse */
#define N2_LOCAL_I2C_HZ  400000u

#define N2_ADC_DEVICES   4u
#define N2_ADC_CH_PER_DEV 4u
/* U10 (Adr0=0, Adr1=0), U11 (1, 0), U12 (0, 1), U13 (1, 1). */
#define N2_ADC_ADDRS { 0x68u, 0x6Cu, 0x6Au, 0x6Eu }

#define N2_SUPPLY_MV     3300    /* tirage des boucles d'alarme */

typedef enum {
    N2_ADC_V15_BP = 0,   /* U10 CH1 : +15 V entrée J1 */
    N2_ADC_V15_SW1,      /* U10 CH2..4 : amont PTC (après cavalier JPn) */
    N2_ADC_V15_SW2,
    N2_ADC_V15_SW3,
    N2_ADC_V15_BA1,      /* U11 CH1..3 : aval PTC (vers le BA) */
    N2_ADC_V15_BA2,
    N2_ADC_V15_BA3,
    N2_ADC_VBAT,         /* U11 CH4 : batterie LiFePO4 */
    N2_ADC_ZONE1,        /* U12 CH1..4 : boucles d'alarme */
    N2_ADC_ZONE2,
    N2_ADC_ZONE3,
    N2_ADC_ZONE4,
    N2_ADC_DV0_BA1,      /* U13 CH1..3 : écart 0 V port n - 0 V entrée BP */
    N2_ADC_DV0_BA2,
    N2_ADC_DV0_BA3,
    N2_ADC_SIREN,        /* U13 CH4 : tension de sortie sirène (retour BTS4140N) */
    N2_ADC_COUNT
} n2_adc_ch_t;

/* Code brut 12 bits signé (1 mV/pas, ±2048) -> grandeur en mV (signée pour les écarts de 0 V). */
int32_t n2_adc_to_mv(int ch, int16_t raw);
/* Boucle d'alarme : tension du nœud en pour-mille de l'alimentation 3,3 V. */
uint16_t n2_zone_permille(int16_t raw);

#endif
