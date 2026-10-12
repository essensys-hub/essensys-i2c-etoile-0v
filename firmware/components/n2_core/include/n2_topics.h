/*
 * Topics MQTT essensys/n2/<id>/<feuille> et mise en forme JSON des messages.
 */
#ifndef N2_TOPICS_H
#define N2_TOPICS_H

#include <stddef.h>
#include <stdint.h>

#include "event_log.h"
#include "frame_log.h"

#define N2_TOPIC_ROOT "essensys/n2/"
#define N2_DEVICE_ID_MAX 32u

/* Feuilles publiées / souscrites. */
#define N2_LEAF_STATUS     "status"       /* online/offline (retenu, LWT) */
#define N2_LEAF_EVENTS     "events"       /* journal d'événements (QoS 1) */
#define N2_LEAF_FRAMES     "frames"       /* trames en erreur, toutes en debug */
#define N2_LEAF_COUNTERS   "counters"     /* compteurs périodiques */
#define N2_LEAF_ALARM      "alarm/state"  /* état de l'alarme (retenu) */
#define N2_LEAF_DUMP_EV    "events/dump"
#define N2_LEAF_DUMP_FR    "frames/dump"
#define N2_LEAF_CMD        "cmd"          /* seule souscription */
#define N2_LEAF_CMD_RESULT "cmd/result"

/* Identifiant : [a-z0-9_-]{1,32}, sans caractère joker MQTT. */
int n2_device_id_valid(const char *id);
/* Longueur écrite, ou -1 (identifiant invalide, tampon trop petit). */
int n2_topic(char *buf, size_t len, const char *device_id, const char *leaf);
/* 1 si l'URI impose TLS (mqtts://). */
int n2_uri_is_tls(const char *uri);

int n2_fmt_event(char *buf, size_t len, const n2_record_t *r);
int n2_fmt_frame(char *buf, size_t len, const flog_entry_t *e, int detailed);
int n2_fmt_counters(char *buf, size_t len, const flog_counters_t *c, uint32_t edges_dropped,
                    uint32_t events_pending);

#endif
