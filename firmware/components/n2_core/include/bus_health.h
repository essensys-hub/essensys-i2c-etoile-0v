/*
 * Santé du bus I2C : BA muet (plus d'ACK pendant un délai configurable) et
 * ligne SDA/SCL collée basse. Observation seule : aucune action sur le bus.
 */
#ifndef BUS_HEALTH_H
#define BUS_HEALTH_H

#include <stdint.h>

#include "n2_events.h"

#define BH_NUM_BA 3u

typedef struct {
    uint32_t ba_mute_ms;   /* défaut 10000 */
    uint32_t stuck_ms;     /* défaut 100 */
} bh_config_t;

typedef struct {
    uint8_t nack_pending;
    uint8_t muted;
    uint8_t back_pending;
    uint32_t first_nack_ms;
    uint32_t last_ack_ms;
} bh_ba_t;

typedef struct {
    uint8_t level;
    uint8_t alerted;
    uint8_t release_pending;
    uint8_t late_alert;       /* collée puis relâchée entre deux tick */
    uint32_t low_since_ms;
    uint32_t release_dur_ms;
} bh_line_t;

typedef struct {
    bh_config_t cfg;
    bh_ba_t ba[BH_NUM_BA];
    bh_line_t line[2];        /* N2_LINE_SDA, N2_LINE_SCL */
} bus_health_t;

void bh_default_config(bh_config_t *c);
void bh_init(bus_health_t *h, const bh_config_t *c, uint8_t scl, uint8_t sda, uint32_t now_ms);
/* Adresse observée dans un segment d'écriture, avec son ACK. */
void bh_on_addr(bus_health_t *h, uint8_t addr, uint8_t ack, uint32_t now_ms);
/* Niveaux courants des lignes (à chaque front, et à chaque tick). */
void bh_on_levels(bus_health_t *h, uint8_t scl, uint8_t sda, uint32_t now_ms);
void bh_tick(bus_health_t *h, uint32_t now_ms, n2_evq_t *q);
int bh_ba_is_muted(const bus_health_t *h, uint8_t addr);

#endif
