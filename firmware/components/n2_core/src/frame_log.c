#include "frame_log.h"

#include <string.h>

void flog_init(flog_t *l, flog_entry_t *buf, uint16_t cap)
{
    memset(l, 0, sizeof(*l));
    l->buf = buf;
    l->cap = cap;
    l->next_seq = 1;
}

uint32_t flog_push(flog_t *l, const ess_frame_t *f, int64_t ts_ms, uint8_t tsrc)
{
    if (l->cap == 0) {
        return 0;
    }
    flog_entry_t *e = &l->buf[l->head];
    e->seq = l->next_seq++;
    e->ts_ms = ts_ms;
    e->tsrc = tsrc;
    e->f = *f;
    l->head = (uint16_t)((l->head + 1u) % l->cap);
    if (l->count < l->cap) {
        l->count++;
    } else {
        l->c.overwritten++;
    }

    l->c.total++;
    if (f->flags == 0) {
        l->c.ok++;
    } else {
        l->c.errors++;
        if (f->flags & ESS_F_ADDR_NACK) {
            l->c.addr_nack++;
        }
        if (f->flags & ESS_F_CRC_ANY) {
            l->c.crc_errors++;
        }
        if (f->flags & ESS_F_BUS_ERROR) {
            l->c.bus_errors++;
        }
    }
    return e->seq;
}

void flog_add_lost(flog_t *l, uint32_t n)
{
    l->c.lost += n;
}

uint32_t flog_oldest_seq(const flog_t *l)
{
    return l->next_seq - l->count;
}

int flog_get_after(const flog_t *l, uint32_t after_seq, flog_entry_t *out)
{
    if (l->count == 0) {
        return 0;
    }
    uint32_t oldest = flog_oldest_seq(l);
    uint32_t want = after_seq + 1u;
    if (want < oldest) {
        want = oldest;
    }
    if (want >= l->next_seq) {
        return 0;
    }
    uint32_t back = l->next_seq - want;          /* 1 = dernière écrite */
    uint32_t idx = ((uint32_t)l->head + l->cap - back) % l->cap;
    *out = l->buf[idx];
    return 1;
}
