/*
 * Commandes MQTT authentifiées.
 *
 * Format (texte, 256 octets max) :
 *   v=1;seq=<n>;cmd=<nom>[;arg=<valeur>][;ttl=<s>];mac=<64 hex>
 * mac = HMAC-SHA256(clé de l'appareil, "<device_id>|" + tout ce qui précède ";mac=").
 * seq doit être strictement supérieur au dernier seq accepté (anti-rejeu).
 *
 * GARANTIE (spec i2c-passive-sniffer, tâche 7.2) : la liste blanche ci-dessous
 * ne contient AUCUNE commande agissant sur le bus I2C Essensys. Le firmware ne
 * possède d'ailleurs aucune fonction d'écriture sur ce bus.
 */
#ifndef MQTT_CMD_H
#define MQTT_CMD_H

#include <stddef.h>
#include <stdint.h>

#define N2_CMD_MAX_LEN 256u
#define N2_CMD_ARG_MAX 200u
#define N2_CMD_TTL_MAX 86400u

typedef enum {
    N2_CMD_NONE = 0,
    N2_CMD_ARM,       /* armement (temporisation de sortie) */
    N2_CMD_DISARM,
    N2_CMD_DEBUG,     /* arg = on | off ; ttl optionnel */
    N2_CMD_STATUS,
    N2_CMD_EVENTS,    /* relecture du journal d'événements */
    N2_CMD_FRAMES,    /* relecture du journal des trames */
    N2_CMD_OTA,       /* arg = URL https de l'image signée */
    N2_CMD__COUNT
} n2_cmd_id_t;

typedef enum {
    N2_CMDR_OK = 0,
    N2_CMDR_TOO_LONG,
    N2_CMDR_NO_MAC,
    N2_CMDR_MALFORMED,
    N2_CMDR_BAD_MAC,
    N2_CMDR_REPLAY,
    N2_CMDR_UNKNOWN,
    N2_CMDR_BAD_ARG,
    N2_CMDR_NO_KEY,
} n2_cmd_result_t;

typedef struct {
    n2_cmd_id_t id;
    uint32_t seq;
    uint32_t ttl_s;
    char arg[N2_CMD_ARG_MAX + 1];
} n2_cmd_t;

n2_cmd_result_t n2_cmd_parse_verify(const char *payload, size_t len, const uint8_t *key, size_t keylen,
                                    const char *device_id, uint32_t last_seq, n2_cmd_t *out);
const char *n2_cmd_result_name(n2_cmd_result_t r);
const char *n2_cmd_name(n2_cmd_id_t id);

/* Calcule le MAC hexadécimal (65 octets avec le zéro final) d'une partie signée. */
void n2_cmd_mac_hex(const uint8_t *key, size_t keylen, const char *device_id, const char *signed_part,
                    size_t signed_len, char out_hex[65]);

#endif
