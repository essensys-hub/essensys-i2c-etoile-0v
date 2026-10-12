/* Tâches 3.2/3.3 : analyse protocolaire, journal horodaté, compteurs, file de fronts. */
#include "edge_ring.h"
#include "essensys_proto.h"
#include "frame_log.h"
#include "n2_crc.h"
#include "n2_test.h"

static void make_tx(i2c_transaction_t *tx, uint8_t addr, const uint8_t *w, uint8_t wl, const uint8_t *r,
                    uint8_t rl)
{
    memset(tx, 0, sizeof(*tx));
    tx->nseg = rl ? 2 : 1;
    tx->seg[0].valid = 1;
    tx->seg[0].addr = addr;
    tx->seg[0].addr_ack = 1;
    tx->seg[0].len = wl;
    memcpy(tx->seg[0].data, w, wl);
    tx->seg[0].ack_mask = (1u << wl) - 1u;
    if (rl) {
        tx->seg[1].valid = 1;
        tx->seg[1].addr = addr;
        tx->seg[1].rw = 1;
        tx->seg[1].addr_ack = 1;
        tx->seg[1].len = rl;
        memcpy(tx->seg[1].data, r, rl);
    }
}

static uint8_t build_ok(uint8_t code, uint8_t ndata, uint8_t *w, uint8_t *r)
{
    w[0] = code;
    for (uint8_t i = 0; i < ndata; i++) {
        w[1 + i] = (uint8_t)(i * 3);
    }
    uint16_t c = n2_crc16_modbus(w, 1u + ndata);
    w[1 + ndata] = (uint8_t)c;
    w[2 + ndata] = (uint8_t)(c >> 8);
    r[0] = code;
    r[1] = (uint8_t)c;
    r[2] = (uint8_t)(c >> 8);
    uint16_t rc = n2_crc16_modbus(r, 3);
    r[3] = (uint8_t)rc;
    r[4] = (uint8_t)(rc >> 8);
    return (uint8_t)(3 + ndata);
}

static void test_expected_lengths(void)
{
    TEST_ASSERT_EQ(ess_expected_write_len(1), 11);
    TEST_ASSERT_EQ(ess_expected_write_len(2), 11);
    TEST_ASSERT_EQ(ess_expected_write_len(3), 19);
    TEST_ASSERT_EQ(ess_expected_write_len(4), 11);
    TEST_ASSERT_EQ(ess_expected_write_len(5), 4);
    TEST_ASSERT_EQ(ess_expected_write_len(0), 0);
    TEST_ASSERT_EQ(ess_expected_write_len(6), 0);
}

static void test_unknown_code_and_len(void)
{
    uint8_t w[24], r[8];
    i2c_transaction_t tx;
    ess_frame_t f;
    uint8_t wl = build_ok(5, 2, w, r); /* ACTIONS avec 2 octets au lieu de 1 */
    make_tx(&tx, 0x11, w, wl, r, 5);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_WRITE_LEN);
    wl = build_ok(9, 1, w, r);
    make_tx(&tx, 0x11, w, wl, r, 5);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_UNKNOWN_CODE);
}

static void test_unknown_addr_echo_and_seq(void)
{
    uint8_t w[24], r[8];
    i2c_transaction_t tx;
    ess_frame_t f;
    uint8_t wl = build_ok(5, 1, w, r);
    make_tx(&tx, 0x20, w, wl, r, 5);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_UNKNOWN_ADDR);

    make_tx(&tx, 0x11, w, wl, r, 5);
    tx.seg[1].data[0] = 4; /* écho de code faux, CRC de réponse recalculé */
    uint16_t rc = n2_crc16_modbus(tx.seg[1].data, 3);
    tx.seg[1].data[3] = (uint8_t)rc;
    tx.seg[1].data[4] = (uint8_t)(rc >> 8);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_ECHO_CODE);

    make_tx(&tx, 0x11, w, wl, r, 5);
    tx.seg[1].addr = 0x12;
    ess_analyze(&tx, &f);
    TEST_ASSERT(f.flags & ESS_F_SEQUENCE);

    make_tx(&tx, 0x11, w, wl, r, 5);
    tx.seg[0].ack_mask = 0x1; /* BA 1.8 : NACK tampon occupé */
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_DATA_NACK);
}

static void test_ba18_comm_stat(void)
{
    uint8_t w[24], r[8];
    i2c_transaction_t tx;
    ess_frame_t f;
    uint8_t wl = build_ok(5, 1, w, r);
    r[5] = 0x01;
    make_tx(&tx, 0x13, w, wl, r, 6);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, 0);
    TEST_ASSERT_EQ(f.has_comm_stat, 1);
    TEST_ASSERT_EQ(f.comm_stat, 1);
    make_tx(&tx, 0x13, w, wl, r, 3);
    ess_analyze(&tx, &f);
    TEST_ASSERT_EQ(f.flags, ESS_F_READ_LEN);
}

static void test_frame_log_ring_and_counters(void)
{
    flog_entry_t buf[4];
    flog_t l;
    flog_init(&l, buf, 4);
    ess_frame_t ok, nack, crc;
    memset(&ok, 0, sizeof(ok));
    ok.addr = 0x11;
    nack = ok;
    nack.flags = ESS_F_ADDR_NACK;
    crc = ok;
    crc.flags = ESS_F_READ_CRC;
    TEST_ASSERT_EQ(flog_push(&l, &ok, 1000, 2), 1);
    TEST_ASSERT_EQ(flog_push(&l, &nack, 1100, 2), 2);
    TEST_ASSERT_EQ(flog_push(&l, &crc, 1200, 2), 3);
    flog_entry_t e;
    TEST_ASSERT(flog_get_after(&l, 0, &e));
    TEST_ASSERT_EQ(e.seq, 1);
    TEST_ASSERT_EQ(e.ts_ms, 1000);
    TEST_ASSERT(flog_get_after(&l, 2, &e));
    TEST_ASSERT_EQ(e.seq, 3);
    TEST_ASSERT_EQ(e.f.flags, ESS_F_READ_CRC);
    TEST_ASSERT(!flog_get_after(&l, 3, &e));
    for (int i = 0; i < 3; i++) {
        flog_push(&l, &ok, 2000 + i, 2);
    }
    /* 6 entrées, capacité 4 : 1 et 2 écrasées */
    TEST_ASSERT_EQ(l.c.overwritten, 2);
    TEST_ASSERT_EQ(flog_oldest_seq(&l), 3);
    TEST_ASSERT(flog_get_after(&l, 0, &e));
    TEST_ASSERT_EQ(e.seq, 3);
    TEST_ASSERT(flog_get_after(&l, 5, &e));
    TEST_ASSERT_EQ(e.seq, 6);
    TEST_ASSERT_EQ(e.ts_ms, 2002);
    TEST_ASSERT_EQ(l.c.total, 6);
    TEST_ASSERT_EQ(l.c.ok, 4);
    TEST_ASSERT_EQ(l.c.errors, 2);
    TEST_ASSERT_EQ(l.c.addr_nack, 1);
    TEST_ASSERT_EQ(l.c.crc_errors, 1);
    flog_add_lost(&l, 3);
    TEST_ASSERT_EQ(l.c.lost, 3);
}

static edge_ring_t g_ring;

static void test_edge_ring_overflow(void)
{
    edge_ring_reset(&g_ring);
    for (uint32_t i = 0; i < EDGE_RING_SIZE; i++) {
        TEST_ASSERT(edge_ring_push(&g_ring, i, i & 3u));
    }
    TEST_ASSERT(!edge_ring_push(&g_ring, 99, 0));
    TEST_ASSERT(!edge_ring_push(&g_ring, 100, 0));
    TEST_ASSERT_EQ(edge_ring_dropped(&g_ring), 2);
    edge_rec_t r;
    TEST_ASSERT(edge_ring_pop(&g_ring, &r));
    TEST_ASSERT_EQ(r.t, 0);
    TEST_ASSERT(edge_ring_pop(&g_ring, &r));
    TEST_ASSERT_EQ(r.t, 1);
    TEST_ASSERT_EQ(r.lv, 1);
    TEST_ASSERT(edge_ring_push(&g_ring, 5000, 3));
    uint32_t n = 0;
    while (edge_ring_pop(&g_ring, &r)) {
        n++;
    }
    TEST_ASSERT_EQ(n, EDGE_RING_SIZE - 1u);
    TEST_ASSERT_EQ(r.t, 5000);
    TEST_ASSERT(!edge_ring_pop(&g_ring, &r));
}

int main(void)
{
    RUN_TEST(test_expected_lengths);
    RUN_TEST(test_unknown_code_and_len);
    RUN_TEST(test_unknown_addr_echo_and_seq);
    RUN_TEST(test_ba18_comm_stat);
    RUN_TEST(test_frame_log_ring_and_counters);
    RUN_TEST(test_edge_ring_overflow);
    return TEST_END();
}
