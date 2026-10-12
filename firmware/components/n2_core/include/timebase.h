/*
 * Base de temps : horloge RTC sauvegardée (RV-3028) au démarrage, corrigée
 * par NTP dès que le réseau est disponible. Sans source valide, l'horodatage
 * est la durée depuis le démarrage (source N2_TS_UPTIME).
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

typedef enum { N2_TS_UPTIME = 0, N2_TS_RTC = 1, N2_TS_NTP = 2 } n2_tsrc_t;

/* 2026-01-01T00:00:00Z : toute date antérieure est une RTC non initialisée. */
#define N2_EPOCH_MIN_MS 1767225600000LL

typedef struct {
    int64_t offset_ms;   /* epoch_ms = uptime_ms + offset_ms */
    uint8_t src;
} timebase_t;

void timebase_init(timebase_t *tb);
/* 0 si accepté ; -1 si date invalide ; 1 si ignoré (RTC alors que NTP déjà acquis). */
int timebase_set(timebase_t *tb, int64_t epoch_ms, int64_t uptime_ms, n2_tsrc_t src);
int64_t timebase_now(const timebase_t *tb, int64_t uptime_ms, uint8_t *src);
const char *timebase_src_name(uint8_t src);

#endif
