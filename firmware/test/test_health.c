/* Tâches 4.1 à 4.3 et 6.1 : BA muet, ligne collée, +15 V/PTC, latch et écart 0 V. */
#include "bus_health.h"
#include "n2_board.h"
#include "n2_test.h"
#include "power_monitor.h"
#include "surge.h"
#include "trace_util.h"

static n2_evt_t evbuf[64];

static int count_type(const n2_evq_t *q, uint8_t type)
{
    int n = 0;
    for (size_t i = 0; i < q->n; i++) {
        if (q->buf[i].type == type) {
            n++;
        }
    }
    return n;
}

static const n2_evt_t *find(const n2_evq_t *q, uint8_t type)
{
    for (size_t i = 0; i < q->n; i++) {
        if (q->buf[i].type == type) {
            return &q->buf[i];
        }
    }
    return NULL;
}

/* ---- 4.1 BA muet ---- */

static void test_ba_mute_default_10s(void)
{
    bh_config_t c;
    bh_default_config(&c);
    TEST_ASSERT_EQ(c.ba_mute_ms, 10000);
    bus_health_t h;
    bh_init(&h, &c, 1, 1, 0);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    bh_on_addr(&h, 0x11, 1, 0);
    /* BP 099-37 : 21 essais espacés de 100 ms, puis abandon */
    for (uint32_t t = 1000; t <= 3000; t += 100) {
        bh_on_addr(&h, 0x13, 0, t);
        bh_tick(&h, t, &q);
    }
    bh_tick(&h, 10999, &q);
    TEST_ASSERT_EQ(q.n, 0);
    bh_tick(&h, 11000, &q);
    TEST_ASSERT_EQ(q.n, 1);
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_BA_MUTE);
    TEST_ASSERT_EQ(q.buf[0].channel, 0x13);
    TEST_ASSERT_EQ(q.buf[0].value, 10000);
    TEST_ASSERT(bh_ba_is_muted(&h, 0x13));
    TEST_ASSERT(!bh_ba_is_muted(&h, 0x11));
    bh_tick(&h, 20000, &q);
    TEST_ASSERT_EQ(q.n, 1); /* une seule alerte */
    bh_on_addr(&h, 0x13, 1, 21000);
    bh_tick(&h, 21010, &q);
    TEST_ASSERT_EQ(q.n, 2);
    TEST_ASSERT_EQ(q.buf[1].type, N2_EV_BA_BACK);
    TEST_ASSERT_EQ(q.buf[1].channel, 0x13);
}

static void test_ba_mute_configurable_and_recovered_before(void)
{
    bh_config_t c = { .ba_mute_ms = 2000, .stuck_ms = 100 };
    bus_health_t h;
    bh_init(&h, &c, 1, 1, 0);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    bh_on_addr(&h, 0x12, 0, 100);
    bh_on_addr(&h, 0x12, 1, 1500); /* répond avant le délai */
    bh_tick(&h, 5000, &q);
    TEST_ASSERT_EQ(q.n, 0);
    bh_on_addr(&h, 0x12, 0, 6000);
    bh_tick(&h, 8000, &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_BA_MUTE), 1);
    bh_on_addr(&h, 0x42, 0, 6000); /* adresse hors BA ignorée */
    TEST_ASSERT(!bh_ba_is_muted(&h, 0x42));
}

/* ---- 4.2 ligne collée ---- */

static void replay_health(const char *name, n2_evq_t *q)
{
    trace_t t;
    if (trace_load(name, &t) != 0) {
        return;
    }
    bh_config_t c;
    bh_default_config(&c);
    bus_health_t h;
    bh_init(&h, &c, t.rows[0].scl, t.rows[0].sda, 0);
    trace_replay(&t, NULL, &h, q);
    trace_free(&t);
}

static void test_sda_stuck_trace(void)
{
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    replay_health("t09_sda_stuck_low.csv", &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_LINE_STUCK), 1);
    const n2_evt_t *s = find(&q, N2_EV_LINE_STUCK);
    TEST_ASSERT_EQ(s->channel, N2_LINE_SDA);
    TEST_ASSERT(s->value >= 100 && s->value < 120);
    const n2_evt_t *r = find(&q, N2_EV_LINE_RELEASED);
    TEST_ASSERT(r != NULL);
    TEST_ASSERT_EQ(r->channel, N2_LINE_SDA);
    TEST_ASSERT(r->value >= 149 && r->value <= 151);
}

static void test_scl_stuck_trace(void)
{
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    replay_health("t10_scl_stuck_low.csv", &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_LINE_STUCK), 1);
    TEST_ASSERT_EQ(find(&q, N2_EV_LINE_STUCK)->channel, N2_LINE_SCL);
    TEST_ASSERT_EQ(find(&q, N2_EV_LINE_RELEASED)->channel, N2_LINE_SCL);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_LINE_RELEASED), 1);
}

static void test_normal_traffic_no_stuck(void)
{
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    replay_health("t08_sequence_mixed.csv", &q);
    replay_health("t02_forcage_ba2_stretch_ok.csv", &q);
    TEST_ASSERT_EQ(q.n, 0);
}

static void test_stuck_released_between_ticks(void)
{
    bh_config_t c;
    bh_default_config(&c);
    bus_health_t h;
    bh_init(&h, &c, 1, 1, 0);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    bh_on_levels(&h, 1, 0, 1001);
    bh_tick(&h, 1100, &q);           /* 99 ms : pas encore */
    TEST_ASSERT_EQ(q.n, 0);
    bh_on_levels(&h, 1, 1, 1105);    /* relâchée à 104 ms, avant le tick suivant */
    bh_tick(&h, 1110, &q);
    TEST_ASSERT_EQ(q.n, 2);
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_LINE_STUCK);
    TEST_ASSERT_EQ(q.buf[1].type, N2_EV_LINE_RELEASED);
    TEST_ASSERT_EQ(q.buf[1].value, 104);
    /* 50 ms : pas d'alerte */
    bh_on_levels(&h, 0, 1, 2000);
    bh_on_levels(&h, 1, 1, 2050);
    bh_tick(&h, 2200, &q);
    TEST_ASSERT_EQ(q.n, 2);
}

/* ---- 4.3 +15 V et PTC ---- */

static pm_sample_t nominal(void)
{
    pm_sample_t s = { .v15_bp = 15000, .v15_sw = { 15000, 15000, 15000 },
                      .v15_ba = { 14900, 14900, 14900 }, .vbat = 3300 };
    return s;
}

static void feed(power_monitor_t *p, const pm_sample_t *s, int n, n2_evq_t *q)
{
    for (int i = 0; i < n; i++) {
        pm_update(p, s, q);
    }
}

static void test_ptc_trip_identified_with_debounce(void)
{
    pm_config_t c;
    pm_default_config(&c);
    power_monitor_t p;
    pm_init(&p, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    pm_sample_t s = nominal();
    feed(&p, &s, 5, &q);
    TEST_ASSERT_EQ(q.n, 0);
    s.v15_ba[1] = 800; /* départ 2 en surcharge : PTC ouverte */
    feed(&p, &s, 2, &q);
    TEST_ASSERT_EQ(q.n, 0); /* anti-rebond : 3 échantillons */
    feed(&p, &s, 1, &q);
    TEST_ASSERT_EQ(q.n, 1);
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_PTC_TRIP);
    TEST_ASSERT_EQ(q.buf[0].channel, 2);
    TEST_ASSERT_EQ(q.buf[0].value, 800);
    s = nominal();
    feed(&p, &s, 3, &q);
    TEST_ASSERT_EQ(q.buf[1].type, N2_EV_PORT_OK);
    TEST_ASSERT_EQ(q.buf[1].channel, 2);
}

static void test_jumper_open_is_not_ptc(void)
{
    pm_config_t c;
    pm_default_config(&c);
    power_monitor_t p;
    pm_init(&p, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    pm_sample_t s = nominal();
    s.v15_sw[2] = 0;
    s.v15_ba[2] = 0;
    feed(&p, &s, 3, &q);
    TEST_ASSERT_EQ(q.n, 1);
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_PORT_OPEN);
    TEST_ASSERT_EQ(q.buf[0].channel, 3);
}

static void test_v15_drop_and_battery(void)
{
    pm_config_t c;
    pm_default_config(&c);
    c.v15_low_mv = 13500; /* seuil configurable */
    power_monitor_t p;
    pm_init(&p, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    pm_sample_t s = nominal();
    s.v15_bp = 13000;
    for (int i = 0; i < 3; i++) {
        s.v15_sw[i] = 13000;
        s.v15_ba[i] = 12900;
    }
    feed(&p, &s, 3, &q);
    TEST_ASSERT_EQ(q.n, 1); /* chute globale : pas d'alerte par départ */
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_V15_LOW);
    /* le BP coupe le +15 V : passage sur batterie, aucune PTC accusée */
    pm_sample_t z = { .v15_bp = 0, .vbat = 3250 };
    feed(&p, &z, 3, &q);
    TEST_ASSERT_EQ(q.n, 2);
    TEST_ASSERT_EQ(q.buf[1].type, N2_EV_ON_BATTERY);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_PTC_TRIP), 0);
    /* batterie faible */
    z.vbat = 2950;
    feed(&p, &z, 3, &q);
    TEST_ASSERT_EQ(q.buf[2].type, N2_EV_BATT_LOW);
    /* retour : hystérésis +300 mV */
    s = nominal();
    s.v15_bp = 13700;
    feed(&p, &s, 3, &q);
    TEST_ASSERT_EQ(q.buf[3].type, N2_EV_V15_LOW);
    TEST_ASSERT_EQ(q.buf[4].type, N2_EV_BATT_OK);
    s.v15_bp = 13900;
    feed(&p, &s, 3, &q);
    TEST_ASSERT_EQ(q.n, 6);
    TEST_ASSERT_EQ(q.buf[5].type, N2_EV_V15_OK);
}

/* ---- 6.1 latch et écart 0 V ---- */

static void test_surge_latch_event_and_rearm(void)
{
    surge_config_t c;
    surge_default_config(&c);
    surge_t s;
    surge_init(&s, &c, 0);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    surge_sample_t in = { .latch = 0, .v15_mv = 15000, .dv0_mv = { 10, -5, 0 } };
    TEST_ASSERT_EQ(surge_update(&s, &in, 100, &q), 0);
    TEST_ASSERT_EQ(q.n, 0);
    /* impulsion injectée : latch écart 0 V, écart résiduel mesuré sur le port 3 */
    in.latch = SURGE_LATCH_0V;
    in.dv0_mv[2] = -2400;
    TEST_ASSERT_EQ(surge_update(&s, &in, 200, &q), 1); /* réarmement demandé */
    const n2_evt_t *e = find(&q, N2_EV_SURGE);
    TEST_ASSERT(e != NULL);
    TEST_ASSERT_EQ(e->value, -2400);
    TEST_ASSERT_EQ(e->channel, SURGE_EV_CHANNEL(SURGE_LATCH_0V, 3));
    TEST_ASSERT_EQ(count_type(&q, N2_EV_ZERO_V_GAP), 1);
    /* nouvelle impulsion pendant le délai de réarmement : différée */
    in.dv0_mv[2] = 0;
    in.latch = SURGE_LATCH_OV;
    in.v15_mv = 21500;
    TEST_ASSERT_EQ(surge_update(&s, &in, 700, &q), 0);
    TEST_ASSERT_EQ(s.deferred, 1);
    TEST_ASSERT_EQ(surge_update(&s, &in, 1200, &q), 1);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SURGE), 2);
    TEST_ASSERT_EQ(q.buf[q.n - 1].channel, SURGE_EV_CHANNEL(SURGE_LATCH_OV, 0));
    TEST_ASSERT_EQ(q.buf[q.n - 1].value, 21500); /* surtension du +15 V */
    TEST_ASSERT_EQ(s.count, 2);
}

/* Chaîne de mesure : codes ADC 12 bits (1 mV/pas) -> grandeurs (révision B). */
static void test_board_scaling(void)
{
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_V15_BP, 938), 15008);   /* 15 V / 16 */
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_VBAT, 1650), 3300);
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_DV0_BA2, -249), -1000); /* G = 0,249 */
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_SIREN, 1091), 12001);
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_V15_BA1, 4000), 2047 * 16); /* saturation */
    TEST_ASSERT_EQ(n2_adc_to_mv(N2_ADC_COUNT, 100), 0);
    TEST_ASSERT_EQ(n2_zone_permille(-5), 0);
    TEST_ASSERT_EQ(n2_zone_permille(3300), 1000);
}

static void test_zero_v_gap_threshold_and_holdoff(void)
{
    surge_config_t c;
    surge_default_config(&c);
    c.gap_threshold_mv = 300;
    surge_t s;
    surge_init(&s, &c, 0);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    surge_sample_t in = { 0 };
    in.dv0_mv[0] = 290;
    surge_update(&s, &in, 0, &q);
    TEST_ASSERT_EQ(q.n, 0);
    in.dv0_mv[0] = -450;
    surge_update(&s, &in, 10, &q);
    TEST_ASSERT_EQ(q.n, 1);
    TEST_ASSERT_EQ(q.buf[0].type, N2_EV_ZERO_V_GAP);
    TEST_ASSERT_EQ(q.buf[0].channel, 1);
    TEST_ASSERT_EQ(q.buf[0].value, -450);
    surge_update(&s, &in, 1000, &q);
    TEST_ASSERT_EQ(q.n, 1); /* écart persistant : pas de répétition avant 60 s */
    surge_update(&s, &in, 60010, &q);
    TEST_ASSERT_EQ(q.n, 2);
    in.dv0_mv[0] = 0;
    surge_update(&s, &in, 60020, &q);
    in.dv0_mv[0] = 500;
    surge_update(&s, &in, 60030, &q); /* nouvel épisode : immédiat */
    TEST_ASSERT_EQ(q.n, 3);
}

int main(void)
{
    RUN_TEST(test_ba_mute_default_10s);
    RUN_TEST(test_ba_mute_configurable_and_recovered_before);
    RUN_TEST(test_sda_stuck_trace);
    RUN_TEST(test_scl_stuck_trace);
    RUN_TEST(test_normal_traffic_no_stuck);
    RUN_TEST(test_stuck_released_between_ticks);
    RUN_TEST(test_ptc_trip_identified_with_debounce);
    RUN_TEST(test_jumper_open_is_not_ptc);
    RUN_TEST(test_v15_drop_and_battery);
    RUN_TEST(test_surge_latch_event_and_rearm);
    RUN_TEST(test_zero_v_gap_threshold_and_holdoff);
    RUN_TEST(test_board_scaling);
    return TEST_END();
}
