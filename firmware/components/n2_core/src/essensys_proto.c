#include "essensys_proto.h"

#include <string.h>

#include "n2_crc.h"

uint8_t ess_expected_write_len(uint8_t code)
{
    switch (code) {
    case ESS_CODE_FORCAGE_SORTIES: return 11;
    case ESS_CODE_CONF_SORTIES:    return 11;
    case ESS_CODE_TPS_EXTINCTION:  return 19;
    case ESS_CODE_TPS_ACTION:      return 11;
    case ESS_CODE_ACTIONS:         return 4;
    default:                       return 0;
    }
}

const char *ess_code_name(uint8_t code)
{
    switch (code) {
    case ESS_CODE_FORCAGE_SORTIES: return "FORCAGE_SORTIES";
    case ESS_CODE_CONF_SORTIES:    return "CONF_SORTIES";
    case ESS_CODE_TPS_EXTINCTION:  return "TPS_EXTINCTION";
    case ESS_CODE_TPS_ACTION:      return "TPS_ACTION";
    case ESS_CODE_ACTIONS:         return "ACTIONS";
    default:                       return "INCONNU";
    }
}

void ess_analyze(const i2c_transaction_t *tx, ess_frame_t *f)
{
    memset(f, 0, sizeof(*f));
    f->t_us = tx->t_start_us;
    if (tx->flags != 0) {
        f->flags |= ESS_F_BUS_ERROR;
    }
    if (tx->nseg == 0 || !tx->seg[0].valid) {
        f->flags |= ESS_F_SEQUENCE;
        return;
    }

    const i2c_segment_t *w = &tx->seg[0];
    f->addr = w->addr;
    if (w->addr < ESS_ADDR_BA_FIRST || w->addr > ESS_ADDR_BA_LAST) {
        f->flags |= ESS_F_UNKNOWN_ADDR;
    }
    if (w->rw != 0) {
        f->flags |= ESS_F_SEQUENCE;
    }
    if (!w->addr_ack) {
        f->flags |= ESS_F_ADDR_NACK;
        return;
    }

    f->wlen = w->len;
    memcpy(f->w, w->data, w->len);
    if (w->len == 0) {
        f->flags |= ESS_F_WRITE_LEN;
    } else {
        f->code = w->data[0];
        uint8_t exp = ess_expected_write_len(f->code);
        if (exp == 0) {
            f->flags |= ESS_F_UNKNOWN_CODE;
        } else if (w->len != exp) {
            f->flags |= ESS_F_WRITE_LEN;
        }
    }
    if (w->len >= 3) {
        f->crc_sent = (uint16_t)(w->data[w->len - 2] | (w->data[w->len - 1] << 8));
        if (n2_crc16_modbus(w->data, (size_t)w->len - 2u) != f->crc_sent) {
            f->flags |= ESS_F_WRITE_CRC;
        }
    }
    for (uint8_t i = 0; i < w->len; i++) {
        if (!(w->ack_mask & (1u << i))) {
            f->flags |= ESS_F_DATA_NACK;
            break;
        }
    }

    if (tx->nseg < 2) {
        f->flags |= ESS_F_NO_READ;
        return;
    }
    if (tx->nseg > 2) {
        f->flags |= ESS_F_SEQUENCE;
    }
    const i2c_segment_t *r = &tx->seg[1];
    if (!r->valid || r->rw != 1 || r->addr != w->addr) {
        f->flags |= ESS_F_SEQUENCE;
        return;
    }
    if (!r->addr_ack) {
        f->flags |= ESS_F_READ_ADDR_NACK;
        return;
    }
    f->rlen = r->len > ESS_MAX_READ ? ESS_MAX_READ : r->len;
    memcpy(f->r, r->data, f->rlen);
    if (r->len != ESS_READ_LEN && r->len != ESS_READ_LEN_EXT) {
        f->flags |= ESS_F_READ_LEN;
    }
    if (r->len < ESS_READ_LEN) {
        return;
    }
    uint16_t rcrc = (uint16_t)(r->data[3] | (r->data[4] << 8));
    if (n2_crc16_modbus(r->data, 3) != rcrc) {
        f->flags |= ESS_F_READ_CRC;
    }
    if (r->data[0] != f->code) {
        f->flags |= ESS_F_ECHO_CODE;
    }
    uint16_t echo = (uint16_t)(r->data[1] | (r->data[2] << 8));
    if (echo != f->crc_sent) {
        if (echo == 0) {
            f->flags |= ESS_F_BA_REJECTED_CRC;
        } else {
            f->flags |= ESS_F_ECHO_CRC;
        }
    }
    if (r->len == ESS_READ_LEN_EXT) {
        f->has_comm_stat = 1;
        f->comm_stat = r->data[5];
    }
}
