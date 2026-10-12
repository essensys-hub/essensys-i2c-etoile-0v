/*
 * Mode debug détaillé : désactivé au démarrage, activable uniquement
 * localement (console USB, bouton) ou par commande MQTT authentifiée, pour
 * une durée bornée. Il n'existe pas de source « MQTT non authentifié ».
 */
#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdint.h>

#include "n2_events.h"

typedef enum { N2_DBG_SRC_LOCAL = 1, N2_DBG_SRC_MQTT_AUTH = 2 } n2_dbg_src_t;

#define N2_DEBUG_DEFAULT_TTL_S 3600u
#define N2_DEBUG_MAX_TTL_S     86400u

typedef struct {
    uint8_t enabled;
    uint8_t source;
    int64_t until_ms;
} n2_debug_t;

void n2_debug_init(n2_debug_t *d);
void n2_debug_enable(n2_debug_t *d, n2_dbg_src_t src, uint32_t ttl_s, int64_t now_ms, n2_evq_t *q);
void n2_debug_disable(n2_debug_t *d, n2_evq_t *q);
/* Gère l'expiration ; retourne 1 si actif. */
int n2_debug_active(n2_debug_t *d, int64_t now_ms, n2_evq_t *q);

#endif
