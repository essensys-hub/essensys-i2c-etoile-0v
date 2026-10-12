#include "bus_health.h"

#include <string.h>

#include "essensys_proto.h"

void bh_default_config(bh_config_t *c)
{
    c->ba_mute_ms = 10000;
    c->stuck_ms = 100;
}

void bh_init(bus_health_t *h, const bh_config_t *c, uint8_t scl, uint8_t sda, uint32_t now_ms)
{
    memset(h, 0, sizeof(*h));
    h->cfg = *c;
    h->line[N2_LINE_SDA].level = sda ? 1 : 0;
    h->line[N2_LINE_SCL].level = scl ? 1 : 0;
    h->line[N2_LINE_SDA].low_since_ms = now_ms;
    h->line[N2_LINE_SCL].low_since_ms = now_ms;
}

void bh_on_addr(bus_health_t *h, uint8_t addr, uint8_t ack, uint32_t now_ms)
{
    if (addr < ESS_ADDR_BA_FIRST || addr > ESS_ADDR_BA_LAST) {
        return;
    }
    bh_ba_t *b = &h->ba[addr - ESS_ADDR_BA_FIRST];
    if (ack) {
        if (b->muted) {
            b->muted = 0;
            b->back_pending = 1;
        }
        b->nack_pending = 0;
        b->last_ack_ms = now_ms;
    } else if (!b->nack_pending) {
        b->nack_pending = 1;
        b->first_nack_ms = now_ms;
    }
}

static void line_level(bus_health_t *h, int idx, uint8_t level, uint32_t now_ms)
{
    bh_line_t *l = &h->line[idx];
    level = level ? 1 : 0;
    if (level == l->level) {
        return;
    }
    l->level = level;
    if (level == 0) {
        l->low_since_ms = now_ms;
        l->alerted = 0;
        return;
    }
    uint32_t dur = now_ms - l->low_since_ms;
    if (l->alerted || dur >= h->cfg.stuck_ms) {
        l->late_alert = l->alerted ? 0 : 1;
        l->release_pending = 1;
        l->release_dur_ms = dur;
    }
}

void bh_on_levels(bus_health_t *h, uint8_t scl, uint8_t sda, uint32_t now_ms)
{
    line_level(h, N2_LINE_SDA, sda, now_ms);
    line_level(h, N2_LINE_SCL, scl, now_ms);
}

void bh_tick(bus_health_t *h, uint32_t now_ms, n2_evq_t *q)
{
    for (int i = 0; i < 2; i++) {
        bh_line_t *l = &h->line[i];
        if (l->release_pending) {
            if (l->late_alert) {
                n2_evq_push(q, N2_EV_LINE_STUCK, (uint8_t)i, (int32_t)l->release_dur_ms);
            }
            n2_evq_push(q, N2_EV_LINE_RELEASED, (uint8_t)i, (int32_t)l->release_dur_ms);
            l->release_pending = 0;
            l->late_alert = 0;
            l->alerted = 0;
        }
        if (l->level == 0 && !l->alerted && now_ms - l->low_since_ms >= h->cfg.stuck_ms) {
            l->alerted = 1;
            n2_evq_push(q, N2_EV_LINE_STUCK, (uint8_t)i, (int32_t)(now_ms - l->low_since_ms));
        }
    }
    for (unsigned i = 0; i < BH_NUM_BA; i++) {
        bh_ba_t *b = &h->ba[i];
        uint8_t addr = (uint8_t)(ESS_ADDR_BA_FIRST + i);
        if (b->back_pending) {
            n2_evq_push(q, N2_EV_BA_BACK, addr, 0);
            b->back_pending = 0;
        }
        if (b->nack_pending && !b->muted && now_ms - b->first_nack_ms >= h->cfg.ba_mute_ms) {
            b->muted = 1;
            n2_evq_push(q, N2_EV_BA_MUTE, addr, (int32_t)(now_ms - b->first_nack_ms));
        }
    }
}

int bh_ba_is_muted(const bus_health_t *h, uint8_t addr)
{
    if (addr < ESS_ADDR_BA_FIRST || addr > ESS_ADDR_BA_LAST) {
        return 0;
    }
    return h->ba[addr - ESS_ADDR_BA_FIRST].muted;
}
