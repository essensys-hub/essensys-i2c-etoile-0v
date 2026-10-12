/*
 * Traitement d'une commande MQTT reçue : authentification, anti-rejeu,
 * journalisation des rejets et exécution des commandes sans entrée/sortie
 * (armement, désarmement, debug). Les commandes de consultation et l'OTA sont
 * rendues à la plateforme. Aucune branche n'agit sur le bus I2C.
 */
#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#include "alarm.h"
#include "debug_mode.h"
#include "mqtt_cmd.h"
#include "n2_events.h"

typedef struct {
    alarm_t *alarm;
    n2_debug_t *dbg;
    const uint8_t *key;
    size_t keylen;
    const char *device_id;
    uint32_t last_seq;     /* à persister par la plateforme quand il change */
} n2_cmd_ctx_t;

/* Retourne le résultat de la vérification ; *out décrit la commande acceptée. */
n2_cmd_result_t n2_cmd_handle(n2_cmd_ctx_t *ctx, const char *payload, size_t len, uint32_t now_ms,
                              int64_t now64_ms, n2_evq_t *q, n2_cmd_t *out);

#endif
