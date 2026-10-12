#include "surge.h"

#include <string.h>

void surge_default_config(surge_config_t *c)
{
    c->gap_threshold_mv = 500;
    c->rearm_ms = 1000;
    c->gap_holdoff_ms = 60000;
}

void surge_init(surge_t *s, const surge_config_t *c, uint32_t now_ms)
{
    memset(s, 0, sizeof(*s));
    s->cfg = *c;
    s->next_rearm_ms = now_ms;
}

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static int reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

int surge_update(surge_t *s, const surge_sample_t *in, uint32_t now_ms, n2_evq_t *q)
{
    int reset = 0;
    const int32_t thr = s->cfg.gap_threshold_mv;

    uint8_t src = in->latch & (SURGE_LATCH_0V | SURGE_LATCH_OV);
    if (src) {
        if (reached(now_ms, s->next_rearm_ms)) {
            uint8_t port = 0;
            int32_t best = 0;
            for (unsigned i = 0; i < SURGE_NUM_PORTS; i++) {
                if (iabs32(in->dv0_mv[i]) > iabs32(best)) {
                    best = in->dv0_mv[i];
                    port = (uint8_t)(i + 1u);
                }
            }
            if (iabs32(best) <= thr) {
                port = 0; /* écart résiduel non significatif : port indéterminé */
            }
            int32_t value = (src & SURGE_LATCH_0V) ? best : in->v15_mv;
            n2_evq_push(q, N2_EV_SURGE, SURGE_EV_CHANNEL(src, port), value);
            s->count++;
            s->next_rearm_ms = now_ms + s->cfg.rearm_ms;
            reset = 1;
        } else {
            s->deferred++;
        }
    }

    for (unsigned i = 0; i < SURGE_NUM_PORTS; i++) {
        int32_t a = iabs32(in->dv0_mv[i]);
        if (a > thr) {
            if (!s->gap_active[i] || reached(now_ms, s->gap_next_ms[i])) {
                n2_evq_push(q, N2_EV_ZERO_V_GAP, (uint8_t)(i + 1u), in->dv0_mv[i]);
                s->gap_active[i] = 1;
                s->gap_next_ms[i] = now_ms + s->cfg.gap_holdoff_ms;
            }
        } else if (a <= (thr * 4) / 5) {
            s->gap_active[i] = 0;
        }
    }
    return reset;
}
