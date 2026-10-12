/*
 * Journal d'événements non volatil (preuves de surtension, alarme, sécurité).
 *
 * Anneau de `cap` enregistrements stockés sous des clés "e<slot>" d'un
 * stockage clé/valeur abstrait (NVS sur cible, RAM en test). Chaque
 * enregistrement porte un CRC-32 ; les métadonnées ("meta") aussi. Après une
 * coupure entre l'écriture d'un enregistrement et celle des métadonnées, la
 * réouverture retrouve l'enregistrement. L'effacement n'est possible que par
 * evlog_clear_local(), réservé à une action locale explicite.
 */
#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stddef.h>
#include <stdint.h>

#include "n2_events.h"

typedef struct {
    int64_t ts_ms;
    uint32_t seq;
    int32_t value;
    uint8_t type;
    uint8_t channel;
    uint8_t tsrc;
    uint8_t rsv;
    uint32_t crc;
} n2_record_t;

_Static_assert(sizeof(n2_record_t) == 24, "format d'enregistrement fige");

typedef struct {
    /* 0 = succès ; toute autre valeur = absent ou erreur. */
    int (*read)(void *ctx, const char *key, void *buf, size_t len);
    int (*write)(void *ctx, const char *key, const void *buf, size_t len);
    int (*erase_all)(void *ctx);
    void *ctx;
} evlog_backend_t;

typedef struct {
    const evlog_backend_t *be;
    uint16_t cap;
    uint32_t next_seq;     /* prochain numéro attribué (>= 1) */
    uint32_t first_seq;    /* plus petit numéro conservé après effacement */
    uint32_t pub_seq;      /* dernier numéro publié */
    uint32_t write_errors;
} evlog_t;

#define EVLOG_MAX_CAP 1000u

int evlog_open(evlog_t *l, const evlog_backend_t *be, uint16_t cap);
int evlog_append(evlog_t *l, int64_t ts_ms, uint8_t tsrc, const n2_evt_t *ev, n2_record_t *out);
int evlog_read(const evlog_t *l, uint32_t seq, n2_record_t *out);
uint32_t evlog_oldest(const evlog_t *l);
/* Premier enregistrement non publié encore disponible ; 0 si trouvé. */
int evlog_next_unpublished(const evlog_t *l, n2_record_t *out);
int evlog_mark_published(evlog_t *l, uint32_t seq);
uint32_t evlog_pending(const evlog_t *l);
/* Action locale explicite uniquement (console USB, bouton). */
int evlog_clear_local(evlog_t *l);

#endif
