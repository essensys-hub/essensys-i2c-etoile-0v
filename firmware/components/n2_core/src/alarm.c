#include "alarm.h"

#include <string.h>

void alarm_default_config(al_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->zone_type[0] = AL_ZT_DELAYED;   /* porte d'entrée */
    c->zone_type[1] = AL_ZT_INSTANT;
    c->zone_type[2] = AL_ZT_INSTANT;
    c->zone_type[3] = AL_ZT_INSTANT;
    c->short_max_pm = 227;
    c->closed_max_pm = 436;
    c->open_max_pm = 580;
    c->debounce = 3;
    c->exit_delay_ms = 30000;
    c->entry_delay_ms = 30000;
    c->tamper_siren_when_disarmed = 0;
    c->max_siren_triggers = 3;
}

al_loop_t al_classify(const al_config_t *c, uint16_t pm)
{
    if (pm < c->short_max_pm) {
        return AL_LOOP_SHORT;
    }
    if (pm < c->closed_max_pm) {
        return AL_LOOP_CLOSED;
    }
    if (pm < c->open_max_pm) {
        return AL_LOOP_OPEN;
    }
    return AL_LOOP_CUT;
}

void alarm_init(alarm_t *a, const al_config_t *c)
{
    memset(a, 0, sizeof(*a));
    a->cfg = *c;
    if (a->cfg.debounce == 0) {
        a->cfg.debounce = 1;
    }
    for (unsigned i = 0; i < AL_ZONES; i++) {
        a->z[i].stable = AL_LOOP_CLOSED;
        a->z[i].cand = AL_LOOP_CLOSED;
    }
    a->state = AL_DISARMED;
}

const char *alarm_state_name(al_state_t s)
{
    switch (s) {
    case AL_DISARMED:    return "disarmed";
    case AL_EXIT_DELAY:  return "exit_delay";
    case AL_ARMED:       return "armed";
    case AL_ENTRY_DELAY: return "entry_delay";
    case AL_ALARM:       return "alarm";
    default:             return "unknown";
    }
}

static int is_tamper(uint8_t loop)
{
    return loop == AL_LOOP_SHORT || loop == AL_LOOP_CUT;
}

static int reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void go_alarm(alarm_t *a, uint8_t zone, n2_evq_t *q)
{
    if (a->state != AL_ALARM) {
        a->state = AL_ALARM;
        a->triggers = 0;
    }
    n2_evq_push(q, N2_EV_ALARM, zone, 0);
    if (a->triggers < a->cfg.max_siren_triggers) {
        a->triggers++;
        n2_evq_push(q, N2_EV_SIREN_TRIGGER, zone, a->triggers);
    }
}

static int is_armed_state(al_state_t s)
{
    return s == AL_EXIT_DELAY || s == AL_ARMED || s == AL_ENTRY_DELAY || s == AL_ALARM;
}

static void tamper_trigger(alarm_t *a, uint8_t zone, n2_evq_t *q)
{
    if (is_armed_state(a->state)) {
        go_alarm(a, zone, q);
    } else if (a->cfg.tamper_siren_when_disarmed) {
        n2_evq_push(q, N2_EV_SIREN_TRIGGER, zone, 0);
    }
}

int alarm_arm(alarm_t *a, al_source_t src, uint32_t now_ms, n2_evq_t *q)
{
    if (a->state != AL_DISARMED) {
        return 1;
    }
    if (a->box_tamper) {
        n2_evq_push(q, N2_EV_ARM_REFUSED, N2_CH_BOX, (int32_t)src);
        return -1;
    }
    for (unsigned i = 0; i < AL_ZONES; i++) {
        uint8_t t = a->cfg.zone_type[i];
        if (t == AL_ZT_DISABLED) {
            continue;
        }
        uint8_t st = a->z[i].stable;
        if (is_tamper(st) || (st == AL_LOOP_OPEN && t != AL_ZT_DELAYED)) {
            n2_evq_push(q, N2_EV_ARM_REFUSED, (uint8_t)(i + 1u), (int32_t)src);
            return -1;
        }
    }
    a->triggers = 0;
    n2_evq_push(q, N2_EV_ARMING, (uint8_t)src, (int32_t)a->cfg.exit_delay_ms);
    if (a->cfg.exit_delay_ms == 0) {
        a->state = AL_ARMED;
        n2_evq_push(q, N2_EV_ARMED, 0, 0);
    } else {
        a->state = AL_EXIT_DELAY;
        a->deadline_ms = now_ms + a->cfg.exit_delay_ms;
    }
    return 0;
}

int alarm_disarm(alarm_t *a, al_source_t src, n2_evq_t *q)
{
    if (a->state == AL_DISARMED) {
        return 1;
    }
    a->state = AL_DISARMED;
    a->triggers = 0;
    n2_evq_push(q, N2_EV_DISARMED, (uint8_t)src, 0);
    return 0;
}

void alarm_step(alarm_t *a, const uint16_t zone_pm[AL_ZONES], uint8_t box_tamper, uint32_t now_ms,
                n2_evq_t *q)
{
    /* 1. Zones : anti-rebond, autoprotection, ouvertures. */
    for (unsigned i = 0; i < AL_ZONES; i++) {
        uint8_t type = a->cfg.zone_type[i];
        if (type == AL_ZT_DISABLED) {
            continue;
        }
        al_zone_t *z = &a->z[i];
        uint8_t raw = (uint8_t)al_classify(&a->cfg, zone_pm[i]);
        uint8_t zone = (uint8_t)(i + 1u);
        if (raw == z->stable) {
            z->cand = raw;
            z->cnt = 0;
            continue;
        }
        if (raw != z->cand) {
            z->cand = raw;
            z->cnt = 1;
        } else {
            z->cnt++;
        }
        if (z->cnt < a->cfg.debounce) {
            continue;
        }
        uint8_t old = z->stable;
        z->stable = raw;
        z->cnt = 0;
        if (is_tamper(raw) && !is_tamper(old)) {
            n2_evq_push(q, N2_EV_TAMPER, zone, raw);
            tamper_trigger(a, zone, q);
        } else if (!is_tamper(raw) && is_tamper(old)) {
            n2_evq_push(q, N2_EV_TAMPER_CLEAR, zone, raw);
        }
        if (raw == AL_LOOP_OPEN) {
            if (type == AL_ZT_24H || a->state == AL_ALARM) {
                go_alarm(a, zone, q);
            }
        }
    }

    /* 2. Autoprotection du boîtier. */
    box_tamper = box_tamper ? 1 : 0;
    if (box_tamper && !a->box_tamper) {
        a->box_tamper = 1;
        n2_evq_push(q, N2_EV_TAMPER, N2_CH_BOX, 0);
        tamper_trigger(a, N2_CH_BOX, q);
    } else if (!box_tamper && a->box_tamper) {
        a->box_tamper = 0;
        n2_evq_push(q, N2_EV_TAMPER_CLEAR, N2_CH_BOX, 0);
    }

    /* 3. Temporisations. */
    if (a->state == AL_EXIT_DELAY && reached(now_ms, a->deadline_ms)) {
        a->state = AL_ARMED;
        n2_evq_push(q, N2_EV_ARMED, 0, 0);
    }
    if (a->state == AL_ENTRY_DELAY && reached(now_ms, a->deadline_ms)) {
        go_alarm(a, a->entry_zone, q);
    }

    /* 4. Niveau des zones en mode armé. */
    if (a->state == AL_ARMED || a->state == AL_ENTRY_DELAY) {
        for (unsigned i = 0; i < AL_ZONES; i++) {
            if (a->cfg.zone_type[i] == AL_ZT_INSTANT && a->z[i].stable == AL_LOOP_OPEN) {
                go_alarm(a, (uint8_t)(i + 1u), q);
                return;
            }
        }
        if (a->state == AL_ARMED) {
            for (unsigned i = 0; i < AL_ZONES; i++) {
                if (a->cfg.zone_type[i] == AL_ZT_DELAYED && a->z[i].stable == AL_LOOP_OPEN) {
                    a->state = AL_ENTRY_DELAY;
                    a->entry_zone = (uint8_t)(i + 1u);
                    a->deadline_ms = now_ms + a->cfg.entry_delay_ms;
                    n2_evq_push(q, N2_EV_ENTRY_DELAY, a->entry_zone, (int32_t)a->cfg.entry_delay_ms);
                    if (a->cfg.entry_delay_ms == 0) {
                        go_alarm(a, a->entry_zone, q);
                    }
                    break;
                }
            }
        }
    }
}
