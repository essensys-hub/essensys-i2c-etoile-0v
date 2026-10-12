/*
 * Surveillance du +15 V (amont), des départs BA (amont/aval de chaque PTC)
 * et de la batterie locale.
 */
#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

#include <stdint.h>

#include "n2_events.h"

#define PM_NUM_PORTS 3u

typedef enum { PM_MAINS_OK = 0, PM_MAINS_LOW, PM_MAINS_ABSENT } pm_mains_t;
typedef enum { PM_PORT_OK = 0, PM_PORT_LOW, PM_PORT_PTC_TRIPPED, PM_PORT_OPEN } pm_port_t;
typedef enum { PM_BATT_OK = 0, PM_BATT_LOW } pm_batt_t;

typedef struct {
    int32_t v15_present_mv;  /* en dessous : +15 V absent (sur batterie) */
    int32_t v15_low_mv;      /* seuil de chute configurable */
    int32_t ptc_trip_mv;     /* aval PTC sous ce seuil alors que l'amont est présent */
    int32_t hyst_mv;
    int32_t vbat_low_mv;
    int32_t vbat_hyst_mv;
    uint8_t debounce;        /* nombre d'échantillons concordants */
} pm_config_t;

typedef struct {
    int32_t v15_bp;                  /* entrée J1, +15 V du BP */
    int32_t v15_sw[PM_NUM_PORTS];    /* amont PTC (après cavalier) */
    int32_t v15_ba[PM_NUM_PORTS];    /* aval PTC (vers le BA) */
    int32_t vbat;
} pm_sample_t;

typedef struct {
    uint8_t state;
    uint8_t cand;
    uint8_t cnt;
} pm_deb_t;

typedef struct {
    pm_config_t cfg;
    pm_deb_t mains;
    pm_deb_t port[PM_NUM_PORTS];
    pm_deb_t batt;
} power_monitor_t;

void pm_default_config(pm_config_t *c);
void pm_init(power_monitor_t *p, const pm_config_t *c);
void pm_update(power_monitor_t *p, const pm_sample_t *s, n2_evq_t *q);

#endif
