#include "i2c_decoder.h"

#include <string.h>

void i2c_decoder_init(i2c_decoder_t *d, i2c_tx_cb_t cb, void *user, uint8_t scl, uint8_t sda)
{
    memset(d, 0, sizeof(*d));
    d->cb = cb;
    d->user = user;
    d->scl = scl ? 1 : 0;
    d->sda = sda ? 1 : 0;
    d->timeout_us = I2C_DEC_DEFAULT_TIMEOUT_US;
}

static void finish(i2c_decoder_t *d)
{
    if (d->tx.nseg == 0 || !d->tx.seg[0].valid) {
        d->st.empty++;
    } else {
        d->st.transactions++;
        if (d->tx.flags & I2C_TX_TRUNCATED) {
            d->st.truncated++;
        }
        if (d->tx.flags & I2C_TX_OVERFLOW) {
            d->st.overflows++;
        }
        if (d->cb) {
            d->cb(&d->tx, d->user);
        }
    }
    d->active = 0;
    d->in_seg = 0;
    d->bitcnt = 0;
    d->shift = 0;
}

static void open_segment(i2c_decoder_t *d)
{
    d->bitcnt = 0;
    d->shift = 0;
    if (d->tx.nseg >= I2C_DEC_MAX_SEGS) {
        d->tx.flags |= I2C_TX_TOO_MANY_SEGS;
        d->in_seg = 0;
        return;
    }
    memset(&d->tx.seg[d->tx.nseg], 0, sizeof(d->tx.seg[0]));
    d->tx.nseg++;
    d->in_seg = 1;
}

/*
 * Avant un (RE)START ou un STOP, le maître remonte SCL : ce front a déjà été
 * compté comme le 1er bit d'un octet. Un compteur de 0 ou 1 bit est donc
 * normal ; au-delà, l'octet en cours est tronqué.
 */
static void check_partial(i2c_decoder_t *d)
{
    if (d->in_seg && d->bitcnt > 1) {
        d->tx.flags |= I2C_TX_TRUNCATED;
    }
}

static void on_start(i2c_decoder_t *d, uint64_t t)
{
    d->st.starts++;
    if (d->active) {
        check_partial(d);
    } else {
        memset(&d->tx, 0, sizeof(d->tx));
        d->tx.t_start_us = t;
        d->active = 1;
    }
    open_segment(d);
}

static void on_stop(i2c_decoder_t *d, uint64_t t)
{
    d->st.stops++;
    if (!d->active) {
        return;
    }
    check_partial(d);
    d->tx.t_end_us = t;
    finish(d);
}

static void on_bit(i2c_decoder_t *d, uint8_t bit)
{
    if (!d->active || !d->in_seg) {
        return;
    }
    if (d->bitcnt < 8) {
        d->shift = (uint8_t)((d->shift << 1) | bit);
        d->bitcnt++;
        return;
    }
    /* 9e bit : ACK (SDA basse) ou NACK (SDA haute). */
    uint8_t ack = (bit == 0) ? 1 : 0;
    i2c_segment_t *s = &d->tx.seg[d->tx.nseg - 1];
    if (!s->valid) {
        s->valid = 1;
        s->addr = (uint8_t)(d->shift >> 1);
        s->rw = (uint8_t)(d->shift & 1u);
        s->addr_ack = ack;
    } else if (s->len < I2C_DEC_MAX_BYTES) {
        s->data[s->len] = d->shift;
        if (ack) {
            s->ack_mask |= (1u << s->len);
        }
        s->len++;
    } else {
        d->tx.flags |= I2C_TX_OVERFLOW;
    }
    d->bitcnt = 0;
    d->shift = 0;
}

void i2c_decoder_feed(i2c_decoder_t *d, uint64_t t_us, uint8_t scl, uint8_t sda)
{
    scl = scl ? 1 : 0;
    sda = sda ? 1 : 0;
    uint8_t pscl = d->scl;
    uint8_t psda = d->sda;
    if (scl == pscl && sda == psda) {
        return;
    }
    d->scl = scl;
    d->sda = sda;
    d->last_edge_us = t_us;

    if (pscl && scl) {
        /* SCL haute stable : seule SDA a changé -> condition START ou STOP. */
        if (psda && !sda) {
            on_start(d, t_us);
        } else if (!psda && sda) {
            on_stop(d, t_us);
        }
        return;
    }
    if (!pscl && scl) {
        /* Front montant de SCL : échantillonnage de SDA. Si SDA a changé dans
         * le même échantillon (latence d'interruption), la nouvelle valeur est
         * celle qui était établie avant le front. */
        on_bit(d, sda);
    }
    /* Front descendant de SCL ou changement de SDA pendant SCL basse : rien. */
}

void i2c_decoder_tick(i2c_decoder_t *d, uint64_t now_us)
{
    if (d->active && now_us - d->last_edge_us >= d->timeout_us) {
        d->tx.flags |= I2C_TX_TIMEOUT;
        d->tx.t_end_us = d->last_edge_us;
        d->st.timeouts++;
        finish(d);
    }
}

void i2c_decoder_resync(i2c_decoder_t *d, uint8_t scl, uint8_t sda)
{
    if (d->active) {
        d->st.resyncs++;
    }
    d->active = 0;
    d->in_seg = 0;
    d->bitcnt = 0;
    d->shift = 0;
    d->scl = scl ? 1 : 0;
    d->sda = sda ? 1 : 0;
}
