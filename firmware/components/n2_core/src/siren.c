#include "siren.h"

#include <string.h>

void siren_default_config(siren_config_t *c)
{
    c->max_ms = 180000;
    c->retry_ms = 5000;
    c->max_retries = 3;
    c->fb_min_mv = 6000;
    c->fb_settle_ms = 300;
}

void siren_init(siren_t *s, const siren_config_t *c)
{
    memset(s, 0, sizeof(*s));
    s->cfg = *c;
}

void siren_start(siren_t *s, uint32_t now_ms, n2_evq_t *q)
{
    if (s->active || s->cfg.max_ms == 0) {
        return;
    }
    s->active = 1;
    s->output = 1;
    s->retries = 0;
    s->start_ms = now_ms;
    s->on_since_ms = now_ms;
    n2_evq_push(q, N2_EV_SIREN_ON, 0, 0);
}

void siren_stop(siren_t *s, uint8_t reason, n2_evq_t *q)
{
    if (!s->active) {
        return;
    }
    s->active = 0;
    s->output = 0;
    n2_evq_push(q, N2_EV_SIREN_OFF, 0, reason);
}

uint8_t siren_step(siren_t *s, uint32_t now_ms, uint8_t fault, n2_evq_t *q)
{
    if (!s->active) {
        s->output = 0;
        return 0;
    }
    if (now_ms - s->start_ms >= s->cfg.max_ms) {
        siren_stop(s, N2_SIREN_OFF_TIMEOUT, q);
        return 0;
    }
    if (s->output && fault) {
        s->output = 0;
        n2_evq_push(q, N2_EV_SIREN_FAULT, 0, s->retries);
        if (s->retries >= s->cfg.max_retries) {
            siren_stop(s, N2_SIREN_OFF_FAULT, q);
            return 0;
        }
        s->retries++;
        s->retry_at_ms = now_ms + s->cfg.retry_ms;
        return 0;
    }
    if (!s->output && (int32_t)(now_ms - s->retry_at_ms) >= 0) {
        s->output = 1;
        s->on_since_ms = now_ms;
    }
    return s->output;
}

uint8_t siren_feedback_fault(const siren_t *s, uint32_t now_ms, int32_t v_out_mv)
{
    return (uint8_t)(s->active && s->output && now_ms - s->on_since_ms >= s->cfg.fb_settle_ms &&
                     v_out_mv < s->cfg.fb_min_mv);
}
