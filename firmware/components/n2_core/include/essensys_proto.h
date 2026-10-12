/*
 * Analyse d'une transaction I2C selon le protocole BP <-> BA Essensys
 * (essensys-board-SC944D docs/i2c.md ; client-essensys-legacy C/ba_i2c.c) :
 *   START, adresse BA + W, code, données (1/8/16), CRC-16 (poids faible, fort),
 *   REPEATED START, adresse BA + R, écho code, écho CRC (2), CRC réponse (2), STOP.
 */
#ifndef ESSENSYS_PROTO_H
#define ESSENSYS_PROTO_H

#include <stdint.h>

#include "i2c_decoder.h"

#define ESS_ADDR_BP        0x10u
#define ESS_ADDR_BA_FIRST  0x11u  /* SC940D pièces de vie */
#define ESS_ADDR_BA_LAST   0x13u  /* SC941C pièces d'eau */
#define ESS_NUM_BA         3u

#define ESS_CODE_FORCAGE_SORTIES 1u
#define ESS_CODE_CONF_SORTIES    2u
#define ESS_CODE_TPS_EXTINCTION  3u
#define ESS_CODE_TPS_ACTION      4u
#define ESS_CODE_ACTIONS         5u

#define ESS_READ_LEN       5u   /* BP 099-37 */
#define ESS_READ_LEN_EXT   6u   /* BA 1.8 : octet comm_stat en plus (format à confirmer) */

#define ESS_MAX_WRITE I2C_DEC_MAX_BYTES
#define ESS_MAX_READ  8u

/* Indicateurs d'erreur d'une trame (0 = trame correcte). */
#define ESS_F_ADDR_NACK        (1u << 0)   /* BA absent / muet */
#define ESS_F_UNKNOWN_ADDR     (1u << 1)
#define ESS_F_UNKNOWN_CODE     (1u << 2)
#define ESS_F_WRITE_LEN        (1u << 3)
#define ESS_F_WRITE_CRC        (1u << 4)   /* CRC émis par le BP faux */
#define ESS_F_DATA_NACK        (1u << 5)   /* octet de données non acquitté */
#define ESS_F_NO_READ          (1u << 6)
#define ESS_F_READ_ADDR_NACK   (1u << 7)
#define ESS_F_READ_LEN         (1u << 8)
#define ESS_F_READ_CRC         (1u << 9)   /* CRC de la réponse BA faux */
#define ESS_F_ECHO_CODE        (1u << 10)
#define ESS_F_ECHO_CRC         (1u << 11)  /* écho du CRC différent du CRC émis */
#define ESS_F_BA_REJECTED_CRC  (1u << 12)  /* écho nul : le BA signale un CRC faux */
#define ESS_F_BUS_ERROR        (1u << 13)  /* trame tronquée, débordement, timeout */
#define ESS_F_SEQUENCE         (1u << 14)  /* enchaînement inattendu */

#define ESS_F_CRC_ANY (ESS_F_WRITE_CRC | ESS_F_READ_CRC | ESS_F_ECHO_CRC | ESS_F_BA_REJECTED_CRC)

typedef struct {
    uint64_t t_us;
    uint8_t addr;
    uint8_t code;
    uint8_t wlen;
    uint8_t rlen;
    uint8_t w[ESS_MAX_WRITE];
    uint8_t r[ESS_MAX_READ];
    uint16_t crc_sent;
    uint16_t flags;
    uint8_t has_comm_stat;
    uint8_t comm_stat;
} ess_frame_t;

/* Longueur totale attendue de l'écriture (code + données + CRC), 0 si code inconnu. */
uint8_t ess_expected_write_len(uint8_t code);
const char *ess_code_name(uint8_t code);
void ess_analyze(const i2c_transaction_t *tx, ess_frame_t *f);

#endif
