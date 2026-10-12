/* Tâche 3.2 : décodeur I2C passif vérifié sur traces SDA/SCL enregistrées. */
#include "essensys_proto.h"
#include "i2c_decoder.h"
#include "n2_crc.h"
#include "n2_test.h"
#include "trace_util.h"

#define MAX_TX 16

typedef struct {
    i2c_transaction_t tx[MAX_TX];
    ess_frame_t fr[MAX_TX];
    int n;
} capture_t;

static void on_tx(const i2c_transaction_t *tx, void *user)
{
    capture_t *c = user;
    if (c->n < MAX_TX) {
        c->tx[c->n] = *tx;
        ess_analyze(tx, &c->fr[c->n]);
        c->n++;
    }
}

static int run(const char *name, capture_t *c, int coalesce, i2c_decoder_t *dout)
{
    trace_t t;
    memset(c, 0, sizeof(*c));
    if (trace_load(name, &t) != 0) {
        return -1;
    }
    if (coalesce) {
        trace_coalesce(&t);
    }
    i2c_decoder_t d;
    i2c_decoder_init(&d, on_tx, c, t.rows[0].scl, t.rows[0].sda);
    trace_replay(&t, &d, NULL, NULL);
    if (dout) {
        *dout = d;
    }
    trace_free(&t);
    return 0;
}

static void test_crc16_reference(void)
{
    /* Vecteur de référence CRC-16/MODBUS. */
    TEST_ASSERT_EQ(n2_crc16_modbus((const uint8_t *)"123456789", 9), 0x4B37);
}

static void test_t01_actions_ok(void)
{
    capture_t c;
    TEST_ASSERT(run("t01_actions_ba1_ok.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    const i2c_transaction_t *tx = &c.tx[0];
    TEST_ASSERT_EQ(tx->nseg, 2);
    TEST_ASSERT_EQ(tx->flags, 0);
    TEST_ASSERT_EQ(tx->seg[0].addr, 0x11);
    TEST_ASSERT_EQ(tx->seg[0].rw, 0);
    TEST_ASSERT_EQ(tx->seg[0].addr_ack, 1);
    TEST_ASSERT_EQ(tx->seg[0].len, 4);
    TEST_ASSERT_EQ(tx->seg[0].data[0], 5);
    TEST_ASSERT_EQ(tx->seg[0].data[1], 1);
    TEST_ASSERT_EQ(tx->seg[0].ack_mask, 0x0F);
    TEST_ASSERT_EQ(tx->seg[1].addr, 0x11);
    TEST_ASSERT_EQ(tx->seg[1].rw, 1);
    TEST_ASSERT_EQ(tx->seg[1].len, 5);
    TEST_ASSERT_EQ(tx->seg[1].ack_mask, 0x0F); /* dernier octet NACK par le maître */
    const ess_frame_t *f = &c.fr[0];
    TEST_ASSERT_EQ(f->flags, 0);
    TEST_ASSERT_EQ(f->code, ESS_CODE_ACTIONS);
    TEST_ASSERT_EQ(f->crc_sent, n2_crc16_modbus((const uint8_t[]){ 5, 1 }, 2));
    TEST_ASSERT_EQ(f->r[0], 5);
    TEST_ASSERT_EQ(f->r[1] | (f->r[2] << 8), f->crc_sent);
    TEST_ASSERT(tx->t_start_us == 55);
}

static void test_t02_forcage_stretch_ok(void)
{
    capture_t c;
    TEST_ASSERT(run("t02_forcage_ba2_stretch_ok.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    TEST_ASSERT_EQ(c.fr[0].flags, 0);
    TEST_ASSERT_EQ(c.fr[0].addr, 0x12);
    TEST_ASSERT_EQ(c.fr[0].code, ESS_CODE_FORCAGE_SORTIES);
    TEST_ASSERT_EQ(c.fr[0].wlen, 11);
}

static void test_t03_tps_extinction_ok(void)
{
    capture_t c;
    TEST_ASSERT(run("t03_tps_extinction_ba3_ok.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    TEST_ASSERT_EQ(c.fr[0].flags, 0);
    TEST_ASSERT_EQ(c.fr[0].addr, 0x13);
    TEST_ASSERT_EQ(c.fr[0].wlen, 19);
    TEST_ASSERT_EQ(c.fr[0].w[16], 16);
}

static void test_t04_ba_absent_nack(void)
{
    capture_t c;
    TEST_ASSERT(run("t04_ba3_absent_nack.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 3);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(c.tx[i].nseg, 1);
        TEST_ASSERT_EQ(c.tx[i].seg[0].addr, 0x13);
        TEST_ASSERT_EQ(c.tx[i].seg[0].addr_ack, 0);
        TEST_ASSERT_EQ(c.fr[i].flags, ESS_F_ADDR_NACK);
    }
    TEST_ASSERT(c.tx[1].t_start_us - c.tx[0].t_start_us > 100000);
}

static void test_t05_write_crc_error(void)
{
    capture_t c;
    TEST_ASSERT(run("t05_write_crc_error.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    TEST_ASSERT_EQ(c.fr[0].flags, ESS_F_WRITE_CRC | ESS_F_BA_REJECTED_CRC);
}

static void test_t06_response_crc_error(void)
{
    capture_t c;
    TEST_ASSERT(run("t06_response_crc_error.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    TEST_ASSERT_EQ(c.fr[0].flags, ESS_F_READ_CRC);
}

static void test_t07_truncated(void)
{
    capture_t c;
    i2c_decoder_t d;
    TEST_ASSERT(run("t07_truncated.csv", &c, 0, &d) == 0);
    TEST_ASSERT_EQ(c.n, 1);
    TEST_ASSERT(c.tx[0].flags & I2C_TX_TRUNCATED);
    TEST_ASSERT_EQ(c.tx[0].seg[0].len, 2);
    TEST_ASSERT(c.fr[0].flags & ESS_F_BUS_ERROR);
    TEST_ASSERT(c.fr[0].flags & ESS_F_NO_READ);
    TEST_ASSERT_EQ(d.st.truncated, 1);
}

static void test_t08_sequence(void)
{
    capture_t c;
    TEST_ASSERT(run("t08_sequence_mixed.csv", &c, 0, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 4);
    TEST_ASSERT_EQ(c.fr[0].addr, 0x11);
    TEST_ASSERT_EQ(c.fr[0].flags, 0);
    TEST_ASSERT_EQ(c.fr[1].addr, 0x12);
    TEST_ASSERT_EQ(c.fr[1].code, ESS_CODE_CONF_SORTIES);
    TEST_ASSERT_EQ(c.fr[1].flags, 0);
    TEST_ASSERT_EQ(c.fr[2].addr, 0x13);
    TEST_ASSERT_EQ(c.fr[2].code, ESS_CODE_TPS_ACTION);
    TEST_ASSERT_EQ(c.fr[2].flags, 0);
    TEST_ASSERT_EQ(c.fr[3].addr, 0x12);
    TEST_ASSERT_EQ(c.fr[3].flags, ESS_F_ADDR_NACK);
}

/* Latence d'interruption : SDA et SCL vues changer dans le même échantillon. */
static void test_t08_coalesced_edges(void)
{
    capture_t c;
    TEST_ASSERT(run("t08_sequence_mixed.csv", &c, 1, NULL) == 0);
    TEST_ASSERT_EQ(c.n, 4);
    TEST_ASSERT_EQ(c.fr[0].flags, 0);
    TEST_ASSERT_EQ(c.fr[1].flags, 0);
    TEST_ASSERT_EQ(c.fr[2].flags, 0);
    TEST_ASSERT_EQ(c.fr[3].flags, ESS_F_ADDR_NACK);
}

static void test_t09_sda_stuck_then_ok(void)
{
    capture_t c;
    i2c_decoder_t d;
    TEST_ASSERT(run("t09_sda_stuck_low.csv", &c, 0, &d) == 0);
    TEST_ASSERT_EQ(c.n, 1);           /* le défaut ne crée pas de fausse trame */
    TEST_ASSERT_EQ(c.fr[0].flags, 0);
    TEST_ASSERT_EQ(d.st.empty, 1);
}

static void test_t10_scl_stuck_timeout(void)
{
    capture_t c;
    i2c_decoder_t d;
    TEST_ASSERT(run("t10_scl_stuck_low.csv", &c, 0, &d) == 0);
    TEST_ASSERT_EQ(c.n, 2);
    TEST_ASSERT(c.tx[0].flags & I2C_TX_TIMEOUT);
    TEST_ASSERT_EQ(c.tx[0].seg[0].addr, 0x12);
    TEST_ASSERT(c.fr[0].flags & ESS_F_BUS_ERROR);
    TEST_ASSERT_EQ(c.fr[1].flags, 0);
    TEST_ASSERT_EQ(d.st.timeouts, 1);
}

/* Perte de fronts (file pleine) : la trame en cours est abandonnée, la suivante décodée. */
static void test_resync_after_lost_edges(void)
{
    trace_t t;
    capture_t c;
    memset(&c, 0, sizeof(c));
    TEST_ASSERT(trace_load("t08_sequence_mixed.csv", &t) == 0);
    i2c_decoder_t d;
    i2c_decoder_init(&d, on_tx, &c, 1, 1);
    size_t cut = 60; /* au milieu de la première trame */
    for (size_t i = 0; i < t.n; i++) {
        if (i == cut) {
            i = cut + 40; /* 40 fronts perdus */
            i2c_decoder_resync(&d, t.rows[i].scl, t.rows[i].sda);
            continue;
        }
        i2c_decoder_tick(&d, t.rows[i].t_us);
        i2c_decoder_feed(&d, t.rows[i].t_us, t.rows[i].scl, t.rows[i].sda);
    }
    i2c_decoder_tick(&d, t.rows[t.n - 1].t_us + 100000);
    trace_free(&t);
    TEST_ASSERT_EQ(d.st.resyncs, 1);
    /* Le reste de la trame interrompue (lecture après REPEATED START) est
     * remonté comme fragment en erreur, jamais comme une trame valide. */
    TEST_ASSERT_EQ(c.n, 4);
    TEST_ASSERT(c.fr[0].flags & ESS_F_SEQUENCE);
    TEST_ASSERT_EQ(c.fr[1].addr, 0x12);
    TEST_ASSERT_EQ(c.fr[1].flags, 0);
    TEST_ASSERT_EQ(c.fr[2].flags, 0);
    TEST_ASSERT_EQ(c.fr[3].flags, ESS_F_ADDR_NACK);
}

static void test_no_change_ignored(void)
{
    capture_t c;
    memset(&c, 0, sizeof(c));
    i2c_decoder_t d;
    i2c_decoder_init(&d, on_tx, &c, 1, 1);
    i2c_decoder_feed(&d, 10, 1, 1);
    i2c_decoder_feed(&d, 20, 1, 1);
    TEST_ASSERT_EQ(d.st.starts, 0);
    TEST_ASSERT_EQ(d.last_edge_us, 0);
}

int main(void)
{
    RUN_TEST(test_crc16_reference);
    RUN_TEST(test_t01_actions_ok);
    RUN_TEST(test_t02_forcage_stretch_ok);
    RUN_TEST(test_t03_tps_extinction_ok);
    RUN_TEST(test_t04_ba_absent_nack);
    RUN_TEST(test_t05_write_crc_error);
    RUN_TEST(test_t06_response_crc_error);
    RUN_TEST(test_t07_truncated);
    RUN_TEST(test_t08_sequence);
    RUN_TEST(test_t08_coalesced_edges);
    RUN_TEST(test_t09_sda_stuck_then_ok);
    RUN_TEST(test_t10_scl_stuck_timeout);
    RUN_TEST(test_resync_after_lost_edges);
    RUN_TEST(test_no_change_ignored);
    return TEST_END();
}
