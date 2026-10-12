#include "debug_mode.h"

void n2_debug_init(n2_debug_t *d)
{
    d->enabled = 0;
    d->source = 0;
    d->until_ms = 0;
}

void n2_debug_enable(n2_debug_t *d, n2_dbg_src_t src, uint32_t ttl_s, int64_t now_ms, n2_evq_t *q)
{
    if (src != N2_DBG_SRC_LOCAL && src != N2_DBG_SRC_MQTT_AUTH) {
        return;
    }
    if (ttl_s == 0) {
        ttl_s = N2_DEBUG_DEFAULT_TTL_S;
    }
    if (ttl_s > N2_DEBUG_MAX_TTL_S) {
        ttl_s = N2_DEBUG_MAX_TTL_S;
    }
    d->enabled = 1;
    d->source = (uint8_t)src;
    d->until_ms = now_ms + (int64_t)ttl_s * 1000;
    n2_evq_push(q, N2_EV_DEBUG_ON, (uint8_t)src, (int32_t)ttl_s);
}

void n2_debug_disable(n2_debug_t *d, n2_evq_t *q)
{
    if (!d->enabled) {
        return;
    }
    d->enabled = 0;
    n2_evq_push(q, N2_EV_DEBUG_OFF, d->source, 0);
}

int n2_debug_active(n2_debug_t *d, int64_t now_ms, n2_evq_t *q)
{
    if (d->enabled && now_ms >= d->until_ms) {
        n2_debug_disable(d, q);
    }
    return d->enabled;
}
