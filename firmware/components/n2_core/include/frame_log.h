/*
 * Journal horodaté des trames (anneau en RAM) et compteurs.
 * Non réentrant : la plateforme le protège par un mutex.
 */
#ifndef FRAME_LOG_H
#define FRAME_LOG_H

#include <stddef.h>
#include <stdint.h>

#include "essensys_proto.h"

typedef struct {
    uint32_t seq;      /* numéro croissant, à partir de 1 */
    int64_t ts_ms;     /* horodatage absolu (ms epoch) ou durée depuis démarrage */
    uint8_t tsrc;      /* source d'horodatage (n2_tsrc_t) */
    ess_frame_t f;
} flog_entry_t;

typedef struct {
    uint32_t total;
    uint32_t ok;
    uint32_t errors;
    uint32_t addr_nack;
    uint32_t crc_errors;
    uint32_t bus_errors;
    uint32_t overwritten;   /* entrées écrasées avant publication/lecture */
    uint32_t lost;          /* trames perdues (fronts perdus, décodeur resynchronisé) */
} flog_counters_t;

typedef struct {
    flog_entry_t *buf;
    uint16_t cap;
    uint16_t head;      /* prochaine case écrite */
    uint16_t count;
    uint32_t next_seq;
    flog_counters_t c;
} flog_t;

void flog_init(flog_t *l, flog_entry_t *buf, uint16_t cap);
uint32_t flog_push(flog_t *l, const ess_frame_t *f, int64_t ts_ms, uint8_t tsrc);
void flog_add_lost(flog_t *l, uint32_t n);
/* Première entrée de numéro > after_seq encore présente ; 1 si trouvée. */
int flog_get_after(const flog_t *l, uint32_t after_seq, flog_entry_t *out);
uint32_t flog_oldest_seq(const flog_t *l);

#endif
