/*
 * Décodeur I2C passif, pur (aucune dépendance matérielle).
 *
 * Il reçoit la suite des niveaux (SCL, SDA) observés à chaque changement,
 * horodatés en microsecondes, et reconstitue les transactions :
 * START, adresse 7 bits + R/W, octets, ACK/NACK, REPEATED START, STOP.
 *
 * Il ne produit AUCUNE sortie vers le bus : il n'a ni état « esclave »,
 * ni génération d'ACK. C'est un observateur.
 */
#ifndef I2C_DECODER_H
#define I2C_DECODER_H

#include <stdint.h>

#define I2C_DEC_MAX_BYTES 24   /* trame Essensys la plus longue : 19 octets */
#define I2C_DEC_MAX_SEGS  4

/* Indicateurs d'anomalie d'une transaction. */
#define I2C_TX_TRUNCATED     0x01u  /* START/STOP au milieu d'un octet */
#define I2C_TX_OVERFLOW      0x02u  /* plus de I2C_DEC_MAX_BYTES octets */
#define I2C_TX_TIMEOUT       0x04u  /* plus aucun front pendant timeout_us */
#define I2C_TX_TOO_MANY_SEGS 0x08u  /* plus de I2C_DEC_MAX_SEGS (RE)START */

#define I2C_DEC_DEFAULT_TIMEOUT_US 25000u

typedef struct {
    uint8_t valid;      /* l'octet d'adresse a été reçu en entier */
    uint8_t addr;       /* adresse 7 bits */
    uint8_t rw;         /* 0 = écriture, 1 = lecture */
    uint8_t addr_ack;   /* 1 = adresse acquittée (SDA basse au 9e bit) */
    uint8_t len;
    uint8_t data[I2C_DEC_MAX_BYTES];
    uint32_t ack_mask;  /* bit i = 1 : octet i acquitté */
} i2c_segment_t;

typedef struct {
    uint64_t t_start_us;
    uint64_t t_end_us;
    uint8_t nseg;
    uint8_t flags;
    i2c_segment_t seg[I2C_DEC_MAX_SEGS];
} i2c_transaction_t;

typedef void (*i2c_tx_cb_t)(const i2c_transaction_t *tx, void *user);

typedef struct {
    uint32_t transactions;
    uint32_t starts;
    uint32_t stops;
    uint32_t truncated;
    uint32_t timeouts;
    uint32_t overflows;
    uint32_t empty;      /* START puis STOP/timeout sans octet d'adresse */
    uint32_t resyncs;    /* transactions abandonnées après perte de fronts */
} i2c_decoder_stats_t;

typedef struct {
    i2c_tx_cb_t cb;
    void *user;
    uint8_t scl;
    uint8_t sda;
    uint8_t active;
    uint8_t in_seg;
    uint8_t bitcnt;
    uint8_t shift;
    uint64_t last_edge_us;
    uint32_t timeout_us;
    i2c_transaction_t tx;
    i2c_decoder_stats_t st;
} i2c_decoder_t;

void i2c_decoder_init(i2c_decoder_t *d, i2c_tx_cb_t cb, void *user, uint8_t scl, uint8_t sda);
/* Nouveaux niveaux observés à l'instant t_us (au moins une ligne a changé). */
void i2c_decoder_feed(i2c_decoder_t *d, uint64_t t_us, uint8_t scl, uint8_t sda);
/* À appeler périodiquement : clôt une transaction sans front depuis timeout_us. */
void i2c_decoder_tick(i2c_decoder_t *d, uint64_t now_us);
/* Perte de fronts (file pleine) : abandon de la transaction en cours et reprise
 * sur les niveaux courants. */
void i2c_decoder_resync(i2c_decoder_t *d, uint8_t scl, uint8_t sda);

#endif
