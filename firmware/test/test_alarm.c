/* Tâches 5.1 à 5.3 : boucles équilibrées, autoprotection, sirène, armement authentifié. */
#include "alarm.h"
#include "cmd_handler.h"
#include "n2_board.h"
#include "n2_test.h"
#include "siren.h"

static n2_evt_t evbuf[64];

/* Valeurs nominales de la révision B (n2_board.h, alarm.h). */
#define PM_SHORT  91
#define PM_CLOSED 363
#define PM_OPEN   510
#define PM_CUT    620

static int count_type(const n2_evq_t *q, uint8_t type)
{
    int n = 0;
    for (size_t i = 0; i < q->n; i++) {
        n += q->buf[i].type == type;
    }
    return n;
}

static const n2_evt_t *last_of(const n2_evq_t *q, uint8_t type)
{
    for (size_t i = q->n; i > 0; i--) {
        if (q->buf[i - 1].type == type) {
            return &q->buf[i - 1];
        }
    }
    return NULL;
}

static void steps(alarm_t *a, uint16_t z[AL_ZONES], uint8_t box, uint32_t *t, int n, n2_evq_t *q)
{
    for (int i = 0; i < n; i++) {
        *t += 50;
        alarm_step(a, z, box, *t, q);
    }
}

static void setup(alarm_t *a, n2_evq_t *q)
{
    al_config_t c;
    alarm_default_config(&c);
    alarm_init(a, &c);
    n2_evq_init(q, evbuf, 64);
}

static void test_loop_classification(void)
{
    al_config_t c;
    alarm_default_config(&c);
    TEST_ASSERT_EQ(al_classify(&c, 0), AL_LOOP_SHORT);
    TEST_ASSERT_EQ(al_classify(&c, PM_SHORT), AL_LOOP_SHORT);
    TEST_ASSERT_EQ(al_classify(&c, 226), AL_LOOP_SHORT);
    TEST_ASSERT_EQ(al_classify(&c, PM_CLOSED), AL_LOOP_CLOSED);
    TEST_ASSERT_EQ(al_classify(&c, 435), AL_LOOP_CLOSED);
    TEST_ASSERT_EQ(al_classify(&c, PM_OPEN), AL_LOOP_OPEN);
    TEST_ASSERT_EQ(al_classify(&c, 579), AL_LOOP_OPEN);
    TEST_ASSERT_EQ(al_classify(&c, PM_CUT), AL_LOOP_CUT);
    TEST_ASSERT_EQ(al_classify(&c, 1000), AL_LOOP_CUT);
    /* chaîne complète : tension du nœud (mV) -> code ADC 12 bits -> pour-mille */
    TEST_ASSERT_EQ(al_classify(&c, n2_zone_permille(300)), AL_LOOP_SHORT);   /* 0 Ω */
    TEST_ASSERT_EQ(al_classify(&c, n2_zone_permille(1198)), AL_LOOP_CLOSED); /* 4,7 kΩ */
    TEST_ASSERT_EQ(al_classify(&c, n2_zone_permille(1682)), AL_LOOP_OPEN);   /* 9,4 kΩ */
    TEST_ASSERT_EQ(al_classify(&c, n2_zone_permille(2047)), AL_LOOP_CUT);    /* saturation */
}

static void test_arm_exit_delay_then_instant_alarm(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_CLOSED, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    steps(&a, z, 0, &t, 5, &q);
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), 0);
    TEST_ASSERT_EQ(a.state, AL_EXIT_DELAY);
    steps(&a, z, 0, &t, 599, &q); /* 29,95 s */
    TEST_ASSERT_EQ(a.state, AL_EXIT_DELAY);
    steps(&a, z, 0, &t, 1, &q);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_ARMED), 1);
    z[2] = PM_OPEN; /* zone 3 instantanée */
    steps(&a, z, 0, &t, 2, &q);
    TEST_ASSERT_EQ(a.state, AL_ARMED); /* anti-rebond */
    steps(&a, z, 0, &t, 1, &q);
    TEST_ASSERT_EQ(a.state, AL_ALARM);
    const n2_evt_t *e = last_of(&q, N2_EV_ALARM);
    TEST_ASSERT(e != NULL);
    TEST_ASSERT_EQ(e->channel, 3);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_TRIGGER), 1);
}

static void test_glitch_ignored(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    a.cfg.exit_delay_ms = 0;
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_CLOSED, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
    z[1] = PM_OPEN;
    steps(&a, z, 0, &t, 2, &q);
    z[1] = PM_CLOSED;
    steps(&a, z, 0, &t, 5, &q);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
}

static void test_entry_delay_then_alarm_or_disarm(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    a.cfg.exit_delay_ms = 0;
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_CLOSED, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    alarm_arm(&a, AL_SRC_MQTT_AUTH, t, &q);
    z[0] = PM_OPEN; /* porte d'entrée temporisée */
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(a.state, AL_ENTRY_DELAY);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_ENTRY_DELAY)->channel, 1);
    steps(&a, z, 0, &t, 100, &q); /* 5 s */
    TEST_ASSERT_EQ(alarm_disarm(&a, AL_SRC_LOCAL_KEY, &q), 0);
    TEST_ASSERT_EQ(a.state, AL_DISARMED);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_ALARM), 0);

    /* sans désarmement : alarme à l'échéance */
    z[0] = PM_CLOSED;
    steps(&a, z, 0, &t, 5, &q);
    alarm_arm(&a, AL_SRC_MQTT_AUTH, t, &q);
    z[0] = PM_OPEN;
    steps(&a, z, 0, &t, 3, &q);
    uint32_t start = t;
    while (a.state == AL_ENTRY_DELAY && t - start < 40000) {
        steps(&a, z, 0, &t, 1, &q);
    }
    TEST_ASSERT_EQ(a.state, AL_ALARM);
    TEST_ASSERT(t - start >= 30000 && t - start <= 30100);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_ALARM)->channel, 1);
}

static void test_cut_loop_tamper_armed_or_not(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_CLOSED, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    z[3] = PM_CUT; /* câble coupé, désarmé */
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_TAMPER), 1);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_TAMPER)->channel, 4);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_TRIGGER), 0);
    TEST_ASSERT_EQ(a.state, AL_DISARMED);
    /* armement refusé tant que la boucle est en défaut */
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), -1);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_ARM_REFUSED)->channel, 4);
    z[3] = PM_CLOSED;
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_TAMPER_CLEAR), 1);
    a.cfg.exit_delay_ms = 0;
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), 0);
    z[1] = PM_SHORT; /* court-circuit en mode armé */
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_TAMPER), 2);
    TEST_ASSERT_EQ(a.state, AL_ALARM);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_TRIGGER), 1);
}

static void test_box_tamper_and_refusals(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_OPEN, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), -1); /* zone instantanée ouverte */
    z[1] = PM_CLOSED;
    z[0] = PM_OPEN; /* porte temporisée ouverte : armement accepté */
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), 0);
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), 1);
    alarm_disarm(&a, AL_SRC_LOCAL_KEY, &q);
    steps(&a, z, 1, &t, 1, &q); /* ouverture du boîtier */
    TEST_ASSERT_EQ(last_of(&q, N2_EV_TAMPER)->channel, N2_CH_BOX);
    TEST_ASSERT_EQ(alarm_arm(&a, AL_SRC_LOCAL_KEY, t, &q), -1);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_ARM_REFUSED)->channel, N2_CH_BOX);
}

static void test_24h_zone_and_retrigger_limit(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    a.cfg.zone_type[3] = AL_ZT_24H;
    a.cfg.max_siren_triggers = 2;
    uint16_t z[AL_ZONES] = { PM_CLOSED, PM_CLOSED, PM_CLOSED, PM_CLOSED };
    uint32_t t = 0;
    z[3] = PM_OPEN; /* 24 h/24, même désarmé */
    steps(&a, z, 0, &t, 3, &q);
    TEST_ASSERT_EQ(a.state, AL_ALARM);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_TRIGGER), 1);
    for (int k = 0; k < 3; k++) {
        z[1] = PM_OPEN;
        steps(&a, z, 0, &t, 3, &q);
        z[1] = PM_CLOSED;
        steps(&a, z, 0, &t, 3, &q);
    }
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_TRIGGER), 2); /* borné */
    TEST_ASSERT_EQ(count_type(&q, N2_EV_ALARM), 4);
}

/* ---- 5.2 sirène ---- */

static void test_siren_max_duration_default_3min(void)
{
    siren_config_t c;
    siren_default_config(&c);
    TEST_ASSERT_EQ(c.max_ms, 180000);
    siren_t s;
    siren_init(&s, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    siren_start(&s, 1000, &q);
    TEST_ASSERT_EQ(siren_step(&s, 1000, 0, &q), 1);
    siren_start(&s, 100000, &q); /* nouvelle demande : pas de prolongation */
    TEST_ASSERT_EQ(siren_step(&s, 180999, 0, &q), 1);
    TEST_ASSERT_EQ(siren_step(&s, 181000, 0, &q), 0);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_SIREN_OFF)->value, N2_SIREN_OFF_TIMEOUT);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_ON), 1);
}

static void test_siren_short_circuit_fault(void)
{
    siren_config_t c;
    siren_default_config(&c);
    siren_t s;
    siren_init(&s, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    siren_start(&s, 0, &q);
    TEST_ASSERT_EQ(siren_step(&s, 10, 1, &q), 0); /* défaut : coupure immédiate */
    TEST_ASSERT_EQ(count_type(&q, N2_EV_SIREN_FAULT), 1);
    TEST_ASSERT_EQ(siren_step(&s, 4000, 0, &q), 0);
    TEST_ASSERT_EQ(siren_step(&s, 5010, 0, &q), 1); /* reprise après 5 s */
    for (int k = 0; k < 4; k++) {
        siren_step(&s, 6000u + 6000u * (uint32_t)k, 1, &q);
        siren_step(&s, 11100u + 6000u * (uint32_t)k, 0, &q);
    }
    TEST_ASSERT_EQ(s.active, 0);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_SIREN_OFF)->value, N2_SIREN_OFF_FAULT);
    TEST_ASSERT_EQ(siren_step(&s, 30000, 0, &q), 0);
}

static void test_siren_feedback_short_circuit(void)
{
    siren_config_t c;
    siren_default_config(&c);
    siren_t s;
    siren_init(&s, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    TEST_ASSERT_EQ(siren_feedback_fault(&s, 0, 0), 0); /* au repos : pas de défaut */
    siren_start(&s, 1000, &q);
    TEST_ASSERT_EQ(siren_feedback_fault(&s, 1200, 0), 0);     /* boost en démarrage */
    TEST_ASSERT_EQ(siren_feedback_fault(&s, 1300, 12000), 0); /* 12 V : normal */
    uint8_t f = siren_feedback_fault(&s, 1300, 800);           /* sortie en court-circuit */
    TEST_ASSERT_EQ(f, 1);
    TEST_ASSERT_EQ(siren_step(&s, 1300, f, &q), 0);
    TEST_ASSERT_EQ(siren_step(&s, 6300, 0, &q), 1);           /* reprise */
    TEST_ASSERT_EQ(siren_feedback_fault(&s, 6400, 800), 0);   /* nouveau délai d'établissement */
}

static void test_siren_stop_on_disarm(void)
{
    siren_config_t c;
    siren_default_config(&c);
    siren_t s;
    siren_init(&s, &c);
    n2_evq_t q;
    n2_evq_init(&q, evbuf, 64);
    siren_start(&s, 0, &q);
    siren_stop(&s, N2_SIREN_OFF_STOP, &q);
    TEST_ASSERT_EQ(siren_step(&s, 10, 0, &q), 0);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_SIREN_OFF)->value, N2_SIREN_OFF_STOP);
}

/* ---- 5.3 désarmement MQTT non authentifié rejeté et journalisé ---- */

static const uint8_t KEY[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };

static void sign(char *out, size_t len, const char *dev, const char *body)
{
    char mac[65];
    n2_cmd_mac_hex(KEY, sizeof(KEY), dev, body, strlen(body), mac);
    snprintf(out, len, "%s;mac=%s", body, mac);
}

static void test_mqtt_disarm_requires_auth(void)
{
    alarm_t a;
    n2_evq_t q;
    setup(&a, &q);
    a.cfg.exit_delay_ms = 0;
    n2_debug_t dbg;
    n2_debug_init(&dbg);
    alarm_arm(&a, AL_SRC_LOCAL_KEY, 0, &q);
    n2_cmd_ctx_t ctx = { .alarm = &a, .dbg = &dbg, .key = KEY, .keylen = sizeof(KEY),
                         .device_id = "n2-test", .last_seq = 10 };
    n2_cmd_t cmd;
    const char *nomac = "v=1;seq=11;cmd=disarm";
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, nomac, strlen(nomac), 0, 0, &q, &cmd), N2_CMDR_NO_MAC);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_CMD_REJECTED)->value, N2_CMDR_NO_MAC);

    char p[300];
    sign(p, sizeof(p), "autre-n2", "v=1;seq=11;cmd=disarm"); /* clé ok, autre appareil */
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &cmd), N2_CMDR_BAD_MAC);
    TEST_ASSERT_EQ(a.state, AL_ARMED);

    sign(p, sizeof(p), "n2-test", "v=1;seq=11;cmd=disarm");
    p[9] = '2'; /* seq=12 après signature : altération */
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &cmd), N2_CMDR_BAD_MAC);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
    TEST_ASSERT_EQ(count_type(&q, N2_EV_CMD_REJECTED), 3);

    sign(p, sizeof(p), "n2-test", "v=1;seq=11;cmd=disarm");
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &cmd), N2_CMDR_OK);
    TEST_ASSERT_EQ(a.state, AL_DISARMED);
    TEST_ASSERT_EQ(last_of(&q, N2_EV_DISARMED)->channel, AL_SRC_MQTT_AUTH);
    TEST_ASSERT_EQ(ctx.last_seq, 11);
    /* rejeu du même message */
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &cmd), N2_CMDR_REPLAY);

    sign(p, sizeof(p), "n2-test", "v=1;seq=12;cmd=arm");
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &cmd), N2_CMDR_OK);
    TEST_ASSERT_EQ(a.state, AL_ARMED);
}

int main(void)
{
    RUN_TEST(test_loop_classification);
    RUN_TEST(test_arm_exit_delay_then_instant_alarm);
    RUN_TEST(test_glitch_ignored);
    RUN_TEST(test_entry_delay_then_alarm_or_disarm);
    RUN_TEST(test_cut_loop_tamper_armed_or_not);
    RUN_TEST(test_box_tamper_and_refusals);
    RUN_TEST(test_24h_zone_and_retrigger_limit);
    RUN_TEST(test_siren_max_duration_default_3min);
    RUN_TEST(test_siren_short_circuit_fault);
    RUN_TEST(test_siren_feedback_short_circuit);
    RUN_TEST(test_siren_stop_on_disarm);
    RUN_TEST(test_mqtt_disarm_requires_auth);
    return TEST_END();
}
