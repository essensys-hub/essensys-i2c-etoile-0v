/*
 * Surtension : lecture/réarmement du comparateur verrouillé (latch) et
 * surveillance de l'écart de potentiel entre les 0 V des départs.
 */
#ifndef SURGE_H
#define SURGE_H

#include <stdint.h>

#include "n2_events.h"

#define SURGE_NUM_PORTS 3u

typedef struct {
    int32_t gap_threshold_mv;   /* écart de 0 V au-delà duquel un événement est créé */
    uint32_t rearm_ms;          /* délai minimal entre deux événements latch */
    uint32_t gap_holdoff_ms;    /* répétition d'un écart persistant */
} surge_config_t;

/* Sources du latch (74HC74 U16, lues par le TCA9534). */
#define SURGE_LATCH_0V 0x01u   /* écart de 0 V hors fenêtre ±1 V (comparateurs U14/U15) */
#define SURGE_LATCH_OV 0x02u   /* +15 V du BP > 20 V */

typedef struct {
    uint8_t latch;                      /* SURGE_LATCH_* capturés depuis le dernier réarmement */
    int32_t v15_mv;                     /* +15 V du BP au moment de la lecture */
    int32_t dv0_mv[SURGE_NUM_PORTS];    /* écart 0 V de chaque départ (signé) */
} surge_sample_t;

/* Événement N2_EV_SURGE : channel = (sources << 4) | port d'écart max (0 = aucun),
 * value = écart 0 V max (signé) si SURGE_LATCH_0V, sinon +15 V mesuré.
 * La révision B n'a pas de détecteur de crête : la valeur est celle lue au
 * traitement du latch (au plus 50 ms après) ; le latch prouve le dépassement. */
#define SURGE_EV_CHANNEL(src, port) ((uint8_t)(((src) << 4) | ((port) & 0x0Fu)))

typedef struct {
    surge_config_t cfg;
    uint32_t next_rearm_ms;
    uint32_t gap_next_ms[SURGE_NUM_PORTS];
    uint8_t gap_active[SURGE_NUM_PORTS];
    uint32_t count;
    uint32_t deferred;          /* latch vu pendant le délai de réarmement */
} surge_t;

void surge_default_config(surge_config_t *c);
void surge_init(surge_t *s, const surge_config_t *c, uint32_t now_ms);
/* Retourne 1 si la plateforme doit réarmer le latch (impulsion RESET). */
int surge_update(surge_t *s, const surge_sample_t *in, uint32_t now_ms, n2_evq_t *q);

#endif
