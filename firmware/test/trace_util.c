#include "trace_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TRACES_DIR
#error "TRACES_DIR doit etre defini"
#endif

int trace_load(const char *name, trace_t *t)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", TRACES_DIR, name);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("  trace introuvable : %s\n", path);
        return -1;
    }
    size_t cap = 256;
    t->rows = malloc(cap * sizeof(trace_row_t));
    t->n = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == 't' || line[0] == '\n') {
            continue;
        }
        unsigned long long tu;
        unsigned scl, sda;
        if (sscanf(line, "%llu,%u,%u", &tu, &scl, &sda) != 3) {
            continue;
        }
        if (t->n == cap) {
            cap *= 2;
            t->rows = realloc(t->rows, cap * sizeof(trace_row_t));
        }
        t->rows[t->n].t_us = tu;
        t->rows[t->n].scl = (uint8_t)scl;
        t->rows[t->n].sda = (uint8_t)sda;
        t->n++;
    }
    fclose(f);
    return t->n > 0 ? 0 : -1;
}

void trace_free(trace_t *t)
{
    free(t->rows);
    t->rows = NULL;
    t->n = 0;
}

void trace_coalesce(trace_t *t)
{
    size_t o = 0;
    for (size_t i = 0; i < t->n; i++) {
        trace_row_t r = t->rows[i];
        int sda_only_scl_low = (i + 1 < t->n) && r.scl == 0 && o > 0 && t->rows[o - 1].scl == 0 &&
                               t->rows[i + 1].scl == 1 && t->rows[i + 1].sda == r.sda;
        if (sda_only_scl_low) {
            continue;  /* fusionné avec le front montant suivant */
        }
        t->rows[o++] = r;
    }
    t->n = o;
}

void trace_replay(const trace_t *t, i2c_decoder_t *d, bus_health_t *h, n2_evq_t *q)
{
    uint64_t next_tick = 10000;
    for (size_t i = 0; i < t->n; i++) {
        const trace_row_t *r = &t->rows[i];
        while (next_tick <= r->t_us) {
            if (d) i2c_decoder_tick(d, next_tick);
            if (h) bh_tick(h, (uint32_t)(next_tick / 1000u), q);
            next_tick += 10000;
        }
        if (d) {
            i2c_decoder_tick(d, r->t_us);
            i2c_decoder_feed(d, r->t_us, r->scl, r->sda);
        }
        if (h) {
            bh_on_levels(h, r->scl, r->sda, (uint32_t)(r->t_us / 1000u));
        }
    }
    uint64_t end = t->rows[t->n - 1].t_us + 50000;
    while (next_tick <= end) {
        if (d) i2c_decoder_tick(d, next_tick);
        if (h) bh_tick(h, (uint32_t)(next_tick / 1000u), q);
        next_tick += 10000;
    }
}
