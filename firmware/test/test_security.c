/* Tâches 3.4, 7.1 à 7.3 : debug, commandes authentifiées, topics, OTA signée, brochage. */
#include "cmd_handler.h"
#include "debug_mode.h"
#include "mqtt_cmd.h"
#include "n2_crc.h"
#include "n2_pins.h"
#include "n2_sha256.h"
#include "n2_test.h"
#include "n2_topics.h"
#include "ota_policy.h"
#include "pin_guard.h"

static void hex(const uint8_t *b, size_t n, char *out)
{
    for (size_t i = 0; i < n; i++) {
        snprintf(out + 2 * i, 3, "%02x", b[i]);
    }
}

static void test_sha256_vectors(void)
{
    uint8_t d[32];
    char h[65];
    n2_sha256("abc", 3, d);
    hex(d, 32, h);
    TEST_ASSERT_STR(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    n2_sha256("", 0, d);
    hex(d, 32, h);
    TEST_ASSERT_STR(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    n2_sha256(m, strlen(m), d);
    hex(d, 32, h);
    TEST_ASSERT_STR(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

static void test_hmac_rfc4231(void)
{
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    n2_hmac_t hm;
    uint8_t d[32];
    char h[65];
    n2_hmac_init(&hm, key, sizeof(key));
    n2_hmac_update(&hm, "Hi There", 8);
    n2_hmac_final(&hm, d);
    hex(d, 32, h);
    TEST_ASSERT_STR(h, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    n2_hmac_init(&hm, (const uint8_t *)"Jefe", 4);
    n2_hmac_update(&hm, "what do ya want for nothing?", 28);
    n2_hmac_final(&hm, d);
    hex(d, 32, h);
    TEST_ASSERT_STR(h, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

static uint8_t KEY[32];

static n2_cmd_result_t try_cmd(const char *body, uint32_t last, n2_cmd_t *c)
{
    char mac[65], p[400];
    n2_cmd_mac_hex(KEY, sizeof(KEY), "n2-banc-01", body, strlen(body), mac);
    snprintf(p, sizeof(p), "%s;mac=%s", body, mac);
    return n2_cmd_parse_verify(p, strlen(p), KEY, sizeof(KEY), "n2-banc-01", last, c);
}

static void test_cmd_parsing_and_whitelist(void)
{
    for (int i = 0; i < 32; i++) {
        KEY[i] = (uint8_t)(0xA0 + i);
    }
    n2_cmd_t c;
    TEST_ASSERT_EQ(try_cmd("v=1;seq=1;cmd=status", 0, &c), N2_CMDR_OK);
    TEST_ASSERT_EQ(c.id, N2_CMD_STATUS);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=debug;arg=on;ttl=600", 4, &c), N2_CMDR_OK);
    TEST_ASSERT_EQ(c.ttl_s, 600);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=debug;arg=maybe", 4, &c), N2_CMDR_BAD_ARG);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=debug;arg=on;ttl=999999", 4, &c), N2_CMDR_BAD_ARG);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=ota", 4, &c), N2_CMDR_BAD_ARG);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=arm;arg=x", 4, &c), N2_CMDR_BAD_ARG);
    TEST_ASSERT_EQ(try_cmd("v=2;seq=5;cmd=arm", 4, &c), N2_CMDR_MALFORMED);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=5;cmd=arm;cmd=disarm", 4, &c), N2_CMDR_MALFORMED);
    TEST_ASSERT_EQ(try_cmd("v=1;cmd=arm", 4, &c), N2_CMDR_MALFORMED);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=4;cmd=arm", 4, &c), N2_CMDR_REPLAY);
    TEST_ASSERT_EQ(try_cmd("v=1;seq=x;cmd=arm", 4, &c), N2_CMDR_MALFORMED);

    /* 7.2 : aucune commande, même correctement signée, n'agit sur le bus. */
    static const char *const bus_like[] = { "i2c_write", "write", "bus", "send", "raw", "sda", "scl",
                                            "reset_bus", "recover", "ba_power", "force", "pilot" };
    for (size_t i = 0; i < sizeof(bus_like) / sizeof(bus_like[0]); i++) {
        char body[64];
        snprintf(body, sizeof(body), "v=1;seq=9;cmd=%s", bus_like[i]);
        TEST_ASSERT_EQ(try_cmd(body, 0, &c), N2_CMDR_UNKNOWN);
    }
    /* Liste blanche figée : toute nouvelle commande impose de revoir ce test. */
    TEST_ASSERT_EQ(N2_CMD__COUNT, 8);
    static const char *const allowed[] = { "none", "arm", "disarm", "debug", "status", "events", "frames", "ota" };
    for (int id = 0; id < N2_CMD__COUNT; id++) {
        TEST_ASSERT_STR(n2_cmd_name((n2_cmd_id_t)id), allowed[id]);
    }
}

/* Interopérabilité avec tools/n2_cmd.py (même clé de test 0xA0..0xBF). */
static void test_cmd_matches_python_signer(void)
{
    const char *p = "v=1;seq=42;cmd=disarm;mac=52f114ca6b479a31dad4b697144fbe4ad926f15362a8386f3c41499e109d9eb1";
    n2_cmd_t c;
    TEST_ASSERT_EQ(n2_cmd_parse_verify(p, strlen(p), KEY, 32, "n2-banc-01", 41, &c), N2_CMDR_OK);
    TEST_ASSERT_EQ(c.id, N2_CMD_DISARM);
    TEST_ASSERT_EQ(c.seq, 42);
}

static void test_cmd_auth_failures(void)
{
    n2_cmd_t c;
    const char *nomac = "v=1;seq=1;cmd=disarm";
    TEST_ASSERT_EQ(n2_cmd_parse_verify(nomac, strlen(nomac), KEY, 32, "n2-banc-01", 0, &c), N2_CMDR_NO_MAC);
    char p[300];
    snprintf(p, sizeof(p), "v=1;seq=1;cmd=disarm;mac=%064d", 0);
    TEST_ASSERT_EQ(n2_cmd_parse_verify(p, strlen(p), KEY, 32, "n2-banc-01", 0, &c), N2_CMDR_BAD_MAC);
    uint8_t zero[32] = { 0 };
    TEST_ASSERT_EQ(n2_cmd_parse_verify(p, strlen(p), zero, 32, "n2-banc-01", 0, &c), N2_CMDR_NO_KEY);
    char big[400];
    memset(big, 'a', sizeof(big));
    TEST_ASSERT_EQ(n2_cmd_parse_verify(big, sizeof(big), KEY, 32, "n2-banc-01", 0, &c), N2_CMDR_TOO_LONG);
}

static void test_debug_mode_default_off_and_sources(void)
{
    n2_evt_t buf[16];
    n2_evq_t q;
    n2_evq_init(&q, buf, 16);
    n2_debug_t d;
    n2_debug_init(&d);
    TEST_ASSERT_EQ(n2_debug_active(&d, 0, &q), 0); /* désactivé par défaut */
    n2_debug_enable(&d, N2_DBG_SRC_LOCAL, 0, 1000, &q);
    TEST_ASSERT_EQ(n2_debug_active(&d, 1000, &q), 1);
    TEST_ASSERT_EQ(n2_debug_active(&d, 1000 + 3600 * 1000 - 1, &q), 1);
    TEST_ASSERT_EQ(n2_debug_active(&d, 1000 + 3600 * 1000, &q), 0); /* expiration */
    TEST_ASSERT_EQ(q.buf[1].type, N2_EV_DEBUG_OFF);
    n2_debug_enable(&d, (n2_dbg_src_t)7, 60, 0, &q); /* source inconnue refusée */
    TEST_ASSERT_EQ(d.enabled, 0);
    n2_debug_enable(&d, N2_DBG_SRC_MQTT_AUTH, 999999, 0, &q);
    TEST_ASSERT(d.until_ms == (int64_t)N2_DEBUG_MAX_TTL_S * 1000);

    /* par MQTT : seulement authentifié */
    n2_debug_init(&d);
    alarm_t a;
    al_config_t ac;
    alarm_default_config(&ac);
    alarm_init(&a, &ac);
    n2_cmd_ctx_t ctx = { &a, &d, KEY, 32, "n2-banc-01", 0 };
    n2_cmd_t c;
    const char *forged = "v=1;seq=1;cmd=debug;arg=on;mac="
                         "0000000000000000000000000000000000000000000000000000000000000000";
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, forged, strlen(forged), 0, 0, &q, &c), N2_CMDR_BAD_MAC);
    TEST_ASSERT_EQ(d.enabled, 0);
    char mac[65], p[200];
    const char *body = "v=1;seq=1;cmd=debug;arg=on;ttl=120";
    n2_cmd_mac_hex(KEY, 32, "n2-banc-01", body, strlen(body), mac);
    snprintf(p, sizeof(p), "%s;mac=%s", body, mac);
    TEST_ASSERT_EQ(n2_cmd_handle(&ctx, p, strlen(p), 0, 0, &q, &c), N2_CMDR_OK);
    TEST_ASSERT_EQ(d.enabled, 1);
    TEST_ASSERT_EQ(d.source, N2_DBG_SRC_MQTT_AUTH);
}

static void test_topics_and_tls(void)
{
    char t[96];
    TEST_ASSERT(n2_topic(t, sizeof(t), "n2-banc-01", N2_LEAF_EVENTS) > 0);
    TEST_ASSERT_STR(t, "essensys/n2/n2-banc-01/events");
    TEST_ASSERT(n2_topic(t, sizeof(t), "n2-banc-01", N2_LEAF_CMD_RESULT) > 0);
    TEST_ASSERT_STR(t, "essensys/n2/n2-banc-01/cmd/result");
    TEST_ASSERT_EQ(n2_topic(t, sizeof(t), "n2/#", "events"), -1);
    TEST_ASSERT_EQ(n2_topic(t, sizeof(t), "+", "events"), -1);
    TEST_ASSERT_EQ(n2_topic(t, sizeof(t), "", "events"), -1);
    TEST_ASSERT_EQ(n2_topic(t, sizeof(t), "N2", "events"), -1);
    TEST_ASSERT_EQ(n2_topic(t, 10, "n2-banc-01", "events"), -1);
    TEST_ASSERT(n2_uri_is_tls("mqtts://broker.example.invalid:8883"));
    TEST_ASSERT(!n2_uri_is_tls("mqtt://broker.example.invalid:1883"));
    TEST_ASSERT(!n2_uri_is_tls("mqtts://"));
    TEST_ASSERT(!n2_uri_is_tls(NULL));
}

static void test_json_formatting(void)
{
    char b[256];
    n2_record_t r = { .ts_ms = 1790000000000LL, .seq = 7, .value = -450, .type = N2_EV_ZERO_V_GAP,
                      .channel = 1, .tsrc = 2 };
    TEST_ASSERT(n2_fmt_event(b, sizeof(b), &r) > 0);
    TEST_ASSERT_STR(b, "{\"seq\":7,\"ts\":1790000000000,\"tsrc\":\"ntp\",\"type\":\"zero_v_gap\",\"ch\":1,"
                       "\"value\":-450}");
    flog_entry_t e;
    memset(&e, 0, sizeof(e));
    e.seq = 3;
    e.ts_ms = 1234;
    e.f.addr = 0x13;
    e.f.code = 5;
    e.f.flags = ESS_F_ADDR_NACK;
    TEST_ASSERT(n2_fmt_frame(b, sizeof(b), &e, 0) > 0);
    TEST_ASSERT_STR(b, "{\"seq\":3,\"ts\":1234,\"tsrc\":\"uptime\",\"addr\":\"0x13\",\"code\":5,"
                       "\"name\":\"ACTIONS\",\"flags\":\"0x0001\",\"ok\":false}");
    e.f.flags = 0;
    e.f.wlen = 2;
    e.f.w[0] = 5;
    e.f.w[1] = 0xab;
    TEST_ASSERT(n2_fmt_frame(b, sizeof(b), &e, 1) > 0);
    TEST_ASSERT(strstr(b, "\"w\":\"05 ab\",\"r\":\"\"") != NULL);
    TEST_ASSERT_EQ(n2_fmt_frame(b, 20, &e, 1), -1);
}

/* ---- 7.3 OTA signée ---- */

static void make_sig_block(uint8_t *blk)
{
    memset(blk, 0x5A, N2_SIG_BLOCK_LEN);
    blk[0] = N2_SIG_BLOCK_MAGIC;
    blk[1] = N2_SIG_BLOCK_VER_RSA;
    blk[2] = blk[3] = 0;
    uint32_t c = n2_crc32(0, blk, N2_SIG_BLOCK_CRC_OFF);
    blk[N2_SIG_BLOCK_CRC_OFF] = (uint8_t)c;
    blk[N2_SIG_BLOCK_CRC_OFF + 1] = (uint8_t)(c >> 8);
    blk[N2_SIG_BLOCK_CRC_OFF + 2] = (uint8_t)(c >> 16);
    blk[N2_SIG_BLOCK_CRC_OFF + 3] = (uint8_t)(c >> 24);
}

static void test_ota_unsigned_image_rejected(void)
{
    TEST_ASSERT_EQ(n2_crc32(0, (const uint8_t *)"123456789", 9), 0xCBF43926u);
    uint8_t blk[N2_SIG_SECTOR_LEN];
    /* image non signée : la fin de l'image (0xFF ou code) n'est pas un bloc de signature */
    memset(blk, 0xFF, sizeof(blk));
    TEST_ASSERT(!n2_ota_sig_block_ok(blk, sizeof(blk)));
    memset(blk, 0x13, sizeof(blk));
    TEST_ASSERT(!n2_ota_sig_block_ok(blk, sizeof(blk)));
    make_sig_block(blk);
    TEST_ASSERT(n2_ota_sig_block_ok(blk, sizeof(blk)));
    blk[100] ^= 1; /* bloc altéré */
    TEST_ASSERT(!n2_ota_sig_block_ok(blk, sizeof(blk)));
    make_sig_block(blk);
    TEST_ASSERT(!n2_ota_sig_block_ok(blk, 100));
    TEST_ASSERT(n2_ota_layout_ok(64 * 4096));
    TEST_ASSERT(!n2_ota_layout_ok(64 * 4096 + 17)); /* image non alignée : non signée */
    TEST_ASSERT(!n2_ota_layout_ok(4096));
}

static void test_ota_url_policy(void)
{
    const char *pre = "https://ota.example.invalid/n2/";
    TEST_ASSERT(n2_ota_url_ok("https://ota.example.invalid/n2/n2-1.0.1.bin", pre));
    TEST_ASSERT(!n2_ota_url_ok("http://ota.example.invalid/n2/n2-1.0.1.bin", pre));
    TEST_ASSERT(!n2_ota_url_ok("https://evil.example.invalid/n2/x.bin", pre));
    TEST_ASSERT(!n2_ota_url_ok("https://ota.example.invalid/n2/", pre));
    TEST_ASSERT(!n2_ota_url_ok("https://ota.example.invalid/n2/a b.bin", pre));
    TEST_ASSERT(n2_ota_url_ok("https://autre.example.invalid/x.bin", ""));
    TEST_ASSERT(!n2_ota_url_ok("https://x", "http://"));
}

/* ---- 3.2 garde des broches d'écoute ---- */

static uint64_t g_outputs;
static int fake_is_output(int pin, void *ctx)
{
    (void)ctx;
    return (int)((g_outputs >> pin) & 1u);
}

static void test_pin_guard(void)
{
    g_outputs = N2_DRIVEN_PINS_MASK; /* configuration normale */
    TEST_ASSERT_EQ(n2_pin_guard_check(N2_SNIFF_PINS_MASK, fake_is_output, NULL), -1);
    g_outputs |= N2_BIT(N2_PIN_SNIFF_SDA); /* firmware fautif */
    TEST_ASSERT_EQ(n2_pin_guard_check(N2_SNIFF_PINS_MASK, fake_is_output, NULL), N2_PIN_SNIFF_SDA);
    TEST_ASSERT_EQ(n2_pin_guard_check(N2_SNIFF_PINS_MASK, NULL, NULL), N2_PIN_SNIFF_SCL);
    TEST_ASSERT_EQ((N2_SNIFF_PINS_MASK & N2_DRIVEN_PINS_MASK), 0);
    TEST_ASSERT(N2_PIN_LOCAL_SDA != N2_PIN_SNIFF_SDA && N2_PIN_LOCAL_SCL != N2_PIN_SNIFF_SCL);
}

int main(void)
{
    RUN_TEST(test_sha256_vectors);
    RUN_TEST(test_hmac_rfc4231);
    RUN_TEST(test_cmd_parsing_and_whitelist);
    RUN_TEST(test_cmd_matches_python_signer);
    RUN_TEST(test_cmd_auth_failures);
    RUN_TEST(test_debug_mode_default_off_and_sources);
    RUN_TEST(test_topics_and_tls);
    RUN_TEST(test_json_formatting);
    RUN_TEST(test_ota_unsigned_image_rejected);
    RUN_TEST(test_ota_url_policy);
    RUN_TEST(test_pin_guard);
    return TEST_END();
}
