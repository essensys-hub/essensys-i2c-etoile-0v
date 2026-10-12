#include "power_monitor.h"

#include <string.h>

void pm_default_config(pm_config_t *c)
{
    c->v15_present_mv = 5000;
    c->v15_low_mv = 13000;
    c->ptc_trip_mv = 3000;
    c->hyst_mv = 300;
    c->vbat_low_mv = 3000;     /* LiFePO4 1S */
    c->vbat_hyst_mv = 100;
    c->debounce = 3;
}

void pm_init(power_monitor_t *p, const pm_config_t *c)
{
    memset(p, 0, sizeof(*p));
    p->cfg = *c;
    if (p->cfg.debounce == 0) {
        p->cfg.debounce = 1;
    }
}

/* Retourne 1 si l'état stable change. */
static int deb(pm_deb_t *d, uint8_t raw, uint8_t n)
{
    if (raw == d->state) {
        d->cnt = 0;
        d->cand = raw;
        return 0;
    }
    if (raw != d->cand) {
        d->cand = raw;
        d->cnt = 1;
    } else {
        d->cnt++;
    }
    if (d->cnt >= n) {
        d->state = raw;
        d->cnt = 0;
        return 1;
    }
    return 0;
}

static uint8_t classify_mains(const power_monitor_t *p, int32_t v)
{
    const pm_config_t *c = &p->cfg;
    int32_t low = c->v15_low_mv + (p->mains.state != PM_MAINS_OK ? c->hyst_mv : 0);
    if (v < c->v15_present_mv) {
        return PM_MAINS_ABSENT;
    }
    if (v < low) {
        return PM_MAINS_LOW;
    }
    return PM_MAINS_OK;
}

static uint8_t classify_port(const power_monitor_t *p, unsigned i, int32_t sw, int32_t ba, int eval_low)
{
    const pm_config_t *c = &p->cfg;
    if (sw < c->v15_present_mv) {
        return PM_PORT_OPEN;
    }
    if (ba < c->ptc_trip_mv) {
        return PM_PORT_PTC_TRIPPED;
    }
    if (eval_low) {
        int32_t low = c->v15_low_mv + (p->port[i].state != PM_PORT_OK ? c->hyst_mv : 0);
        if (ba < low) {
            return PM_PORT_LOW;
        }
    }
    return PM_PORT_OK;
}

void pm_update(power_monitor_t *p, const pm_sample_t *s, n2_evq_t *q)
{
    const uint8_t n = p->cfg.debounce;

    if (deb(&p->mains, classify_mains(p, s->v15_bp), n)) {
        static const uint8_t ev[] = { N2_EV_V15_OK, N2_EV_V15_LOW, N2_EV_ON_BATTERY };
        n2_evq_push(q, ev[p->mains.state], 0, s->v15_bp);
    }

    /* +15 V absent : tous les départs sont à 0, rien à attribuer à une PTC. */
    if (p->mains.state != PM_MAINS_ABSENT) {
        int eval_low = (p->mains.state == PM_MAINS_OK);
        for (unsigned i = 0; i < PM_NUM_PORTS; i++) {
            uint8_t raw = classify_port(p, i, s->v15_sw[i], s->v15_ba[i], eval_low);
            if (!eval_low && raw == PM_PORT_OK && p->port[i].state == PM_PORT_LOW) {
                raw = PM_PORT_LOW;  /* pas de conclusion tant que l'amont est bas */
            }
            if (deb(&p->port[i], raw, n)) {
                static const uint8_t ev[] = { N2_EV_PORT_OK, N2_EV_PORT_LOW, N2_EV_PTC_TRIP, N2_EV_PORT_OPEN };
                n2_evq_push(q, ev[p->port[i].state], (uint8_t)(i + 1u), s->v15_ba[i]);
            }
        }
    }

    int32_t blow = p->cfg.vbat_low_mv + (p->batt.state == PM_BATT_LOW ? p->cfg.vbat_hyst_mv : 0);
    uint8_t braw = s->vbat < blow ? PM_BATT_LOW : PM_BATT_OK;
    if (deb(&p->batt, braw, n)) {
        n2_evq_push(q, p->batt.state == PM_BATT_LOW ? N2_EV_BATT_LOW : N2_EV_BATT_OK, 0, s->vbat);
    }
}
