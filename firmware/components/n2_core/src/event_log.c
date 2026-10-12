#include "event_log.h"

#include <stdio.h>
#include <string.h>

#include "n2_crc.h"

#define META_MAGIC 0x4E32454Cu /* "N2EL" */

typedef struct {
    uint32_t magic;
    uint32_t next_seq;
    uint32_t first_seq;
    uint32_t pub_seq;
    uint32_t crc;
} evlog_meta_t;

static void slot_key(const evlog_t *l, uint32_t seq, char key[8])
{
    snprintf(key, 8, "e%u", (unsigned)(seq % l->cap));
}

static uint32_t rec_crc(const n2_record_t *r)
{
    return n2_crc32(0, (const uint8_t *)r, offsetof(n2_record_t, crc));
}

static uint32_t meta_crc(const evlog_meta_t *m)
{
    return n2_crc32(0, (const uint8_t *)m, offsetof(evlog_meta_t, crc));
}

static int write_meta(evlog_t *l)
{
    evlog_meta_t m = { META_MAGIC, l->next_seq, l->first_seq, l->pub_seq, 0 };
    m.crc = meta_crc(&m);
    if (l->be->write(l->be->ctx, "meta", &m, sizeof(m)) != 0) {
        l->write_errors++;
        return -1;
    }
    return 0;
}

static int read_slot(const evlog_t *l, uint32_t seq, n2_record_t *r)
{
    char key[8];
    slot_key(l, seq, key);
    if (l->be->read(l->be->ctx, key, r, sizeof(*r)) != 0) {
        return -1;
    }
    if (r->crc != rec_crc(r) || r->seq != seq) {
        return -1;
    }
    return 0;
}

int evlog_open(evlog_t *l, const evlog_backend_t *be, uint16_t cap)
{
    if (cap == 0 || cap > EVLOG_MAX_CAP) {
        return -1;
    }
    memset(l, 0, sizeof(*l));
    l->be = be;
    l->cap = cap;

    evlog_meta_t m;
    int dirty = 0;
    if (be->read(be->ctx, "meta", &m, sizeof(m)) == 0 && m.magic == META_MAGIC &&
        m.crc == meta_crc(&m) && m.next_seq >= 1 && m.first_seq >= 1 && m.first_seq <= m.next_seq) {
        l->next_seq = m.next_seq;
        l->first_seq = m.first_seq;
        l->pub_seq = m.pub_seq;
    } else {
        /* Métadonnées absentes ou corrompues : on reconstruit à partir des
         * enregistrements valides pour ne perdre aucune preuve. */
        uint32_t max_seq = 0;
        for (uint16_t slot = 0; slot < cap; slot++) {
            char key[8];
            n2_record_t r;
            snprintf(key, sizeof(key), "e%u", (unsigned)slot);
            if (be->read(be->ctx, key, &r, sizeof(r)) == 0 && r.crc == rec_crc(&r) && r.seq >= 1 &&
                r.seq % cap == slot && r.seq > max_seq) {
                max_seq = r.seq;
            }
        }
        l->next_seq = max_seq + 1u;
        l->first_seq = 1;
        l->pub_seq = 0;  /* tout republier plutôt que perdre un événement */
        dirty = 1;
    }

    /* Reprise : enregistrements écrits mais métadonnées non mises à jour. */
    n2_record_t r;
    for (unsigned i = 0; i < cap && read_slot(l, l->next_seq, &r) == 0; i++) {
        l->next_seq++;
        dirty = 1;
    }
    if (dirty) {
        write_meta(l);
    }
    return 0;
}

uint32_t evlog_oldest(const evlog_t *l)
{
    uint32_t oldest = l->next_seq > l->cap ? l->next_seq - l->cap : 1u;
    return oldest > l->first_seq ? oldest : l->first_seq;
}

int evlog_append(evlog_t *l, int64_t ts_ms, uint8_t tsrc, const n2_evt_t *ev, n2_record_t *out)
{
    n2_record_t r;
    memset(&r, 0, sizeof(r));
    r.ts_ms = ts_ms;
    r.seq = l->next_seq;
    r.value = ev->value;
    r.type = ev->type;
    r.channel = ev->channel;
    r.tsrc = tsrc;
    r.crc = rec_crc(&r);

    char key[8];
    slot_key(l, r.seq, key);
    if (l->be->write(l->be->ctx, key, &r, sizeof(r)) != 0) {
        l->write_errors++;
        return -1;
    }
    l->next_seq++;
    write_meta(l);
    if (out) {
        *out = r;
    }
    return 0;
}

int evlog_read(const evlog_t *l, uint32_t seq, n2_record_t *out)
{
    if (seq < evlog_oldest(l) || seq >= l->next_seq) {
        return -1;
    }
    return read_slot(l, seq, out);
}

int evlog_next_unpublished(const evlog_t *l, n2_record_t *out)
{
    uint32_t s = l->pub_seq + 1u;
    uint32_t oldest = evlog_oldest(l);
    if (s < oldest) {
        s = oldest;
    }
    for (; s < l->next_seq; s++) {
        if (read_slot(l, s, out) == 0) {
            return 0;
        }
    }
    return -1;
}

int evlog_mark_published(evlog_t *l, uint32_t seq)
{
    if (seq <= l->pub_seq || seq >= l->next_seq) {
        return -1;
    }
    l->pub_seq = seq;
    return write_meta(l);
}

uint32_t evlog_pending(const evlog_t *l)
{
    uint32_t s = l->pub_seq + 1u;
    uint32_t oldest = evlog_oldest(l);
    if (s < oldest) {
        s = oldest;
    }
    return l->next_seq > s ? l->next_seq - s : 0u;
}

int evlog_clear_local(evlog_t *l)
{
    if (l->be->erase_all(l->be->ctx) != 0) {
        l->write_errors++;
        return -1;
    }
    /* Les numéros continuent : un effacement reste visible dans la séquence. */
    l->first_seq = l->next_seq;
    l->pub_seq = l->next_seq - 1u;
    return write_meta(l);
}
