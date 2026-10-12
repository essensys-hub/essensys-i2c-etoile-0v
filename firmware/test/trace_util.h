/* Chargement et rejeu des traces SDA/SCL (dossier test/traces). */
#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "bus_health.h"
#include "i2c_decoder.h"

typedef struct {
    uint64_t t_us;
    uint8_t scl;
    uint8_t sda;
} trace_row_t;

typedef struct {
    trace_row_t *rows;
    size_t n;
} trace_t;

int trace_load(const char *name, trace_t *t);   /* nom relatif à TRACES_DIR */
void trace_free(trace_t *t);

/* Simule la latence d'interruption : un changement de SDA pendant SCL basse
 * n'est vu qu'au front montant suivant de SCL (un seul échantillon). */
void trace_coalesce(trace_t *t);

/* Rejoue la trace : tick décodeur/santé toutes les 10 ms et à chaque ligne. */
void trace_replay(const trace_t *t, i2c_decoder_t *d, bus_health_t *h, n2_evq_t *q);

#endif
