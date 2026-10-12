/*
 * Alarme intrusion (aide à la détection, non certifiée EN 50131).
 *
 * Boucles équilibrées à double résistance de fin de ligne (FDL 2 x 4,7 kΩ),
 * nœud de mesure tiré à 3,3 V par 10 kΩ, 1 kΩ série vers la borne (bloc F de
 * la révision B). Tension du nœud en pour-mille de 3,3 V :
 *   court-circuit ~91 ‰ | fermé (4,7 kΩ) ~363 ‰ | ouvert (9,4 kΩ) ~510 ‰ |
 *   coupé : saturation de l'ADC (2,048 V) ~620 ‰.
 * Classement : < short_max_pm court-circuit, < closed_max_pm fermé,
 * < open_max_pm ouvert, au-delà coupé.
 */
#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>

#include "n2_events.h"

#define AL_ZONES 4u

typedef enum { AL_ZT_DISABLED = 0, AL_ZT_INSTANT, AL_ZT_DELAYED, AL_ZT_24H } al_zone_type_t;
typedef enum { AL_LOOP_SHORT = 0, AL_LOOP_CLOSED, AL_LOOP_OPEN, AL_LOOP_CUT } al_loop_t;
typedef enum { AL_DISARMED = 0, AL_EXIT_DELAY, AL_ARMED, AL_ENTRY_DELAY, AL_ALARM } al_state_t;
typedef enum { AL_SRC_LOCAL_KEY = 1, AL_SRC_LOCAL_CONSOLE = 2, AL_SRC_MQTT_AUTH = 3 } al_source_t;

typedef struct {
    uint8_t zone_type[AL_ZONES];
    uint16_t short_max_pm;
    uint16_t closed_max_pm;
    uint16_t open_max_pm;
    uint8_t debounce;
    uint32_t exit_delay_ms;
    uint32_t entry_delay_ms;
    uint8_t tamper_siren_when_disarmed;
    uint8_t max_siren_triggers;      /* par cycle d'armement */
} al_config_t;

typedef struct {
    uint8_t stable;
    uint8_t cand;
    uint8_t cnt;
} al_zone_t;

typedef struct {
    al_config_t cfg;
    al_state_t state;
    uint32_t deadline_ms;
    uint8_t entry_zone;
    uint8_t triggers;
    uint8_t box_tamper;
    al_zone_t z[AL_ZONES];
} alarm_t;

void alarm_default_config(al_config_t *c);
al_loop_t al_classify(const al_config_t *c, uint16_t permille);
void alarm_init(alarm_t *a, const al_config_t *c);
/* 0 = armement lancé, 1 = déjà armé, -1 = refusé (zone en défaut ou ouverte). */
int alarm_arm(alarm_t *a, al_source_t src, uint32_t now_ms, n2_evq_t *q);
/* 0 = désarmé, 1 = déjà désarmé. */
int alarm_disarm(alarm_t *a, al_source_t src, n2_evq_t *q);
void alarm_step(alarm_t *a, const uint16_t zone_pm[AL_ZONES], uint8_t box_tamper, uint32_t now_ms,
                n2_evq_t *q);
const char *alarm_state_name(al_state_t s);

#endif
