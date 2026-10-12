/*
 * Sortie sirène : durée maximale (défaut 3 min) et coupure sur défaut, avec
 * reprises bornées. Le commutateur BTS4140N limite le courant et se coupe en
 * surchauffe ; le défaut est déduit du retour de tension (siren_feedback_fault).
 */
#ifndef SIREN_H
#define SIREN_H

#include <stdint.h>

#include "n2_events.h"

#define N2_SIREN_OFF_STOP    1   /* désarmement / arrêt demandé */
#define N2_SIREN_OFF_TIMEOUT 2   /* durée maximale atteinte */
#define N2_SIREN_OFF_FAULT   3   /* défauts répétés */

typedef struct {
    uint32_t max_ms;
    uint32_t retry_ms;
    uint8_t max_retries;
    int32_t fb_min_mv;      /* tension de sortie minimale une fois établie */
    uint32_t fb_settle_ms;  /* délai d'établissement (démarrage du boost 12 V) */
} siren_config_t;

typedef struct {
    siren_config_t cfg;
    uint8_t active;
    uint8_t output;
    uint8_t retries;
    uint32_t start_ms;
    uint32_t retry_at_ms;
    uint32_t on_since_ms;
} siren_t;

void siren_default_config(siren_config_t *c);
void siren_init(siren_t *s, const siren_config_t *c);
void siren_start(siren_t *s, uint32_t now_ms, n2_evq_t *q);   /* sans effet si déjà active */
void siren_stop(siren_t *s, uint8_t reason, n2_evq_t *q);
/* fault = 1 si un défaut est détecté (siren_feedback_fault). Retourne le niveau de sortie. */
uint8_t siren_step(siren_t *s, uint32_t now_ms, uint8_t fault, n2_evq_t *q);
/* 1 si la sortie est commandée depuis fb_settle_ms et que sa tension reste sous fb_min_mv. */
uint8_t siren_feedback_fault(const siren_t *s, uint32_t now_ms, int32_t v_out_mv);

#endif
