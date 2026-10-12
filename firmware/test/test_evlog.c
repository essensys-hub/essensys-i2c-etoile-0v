/* Tâche 6.2 : journal non volatil, relecture après coupure, horodatage RTC/NTP. */
#include "event_log.h"
#include "n2_test.h"
#include "timebase.h"

/* Stockage clé/valeur simulé, persistant entre deux « démarrages ». */
#define KV_MAX 1100
typedef struct {
    char key[8];
    uint8_t val[32];
    size_t len;
    int used;
} kv_t;

typedef struct {
    kv_t kv[KV_MAX];
    int fail_meta_writes;
} store_t;

static kv_t *kv_find(store_t *s, const char *key)
{
    for (int i = 0; i < KV_MAX; i++) {
        if (s->kv[i].used && strcmp(s->kv[i].key, key) == 0) {
            return &s->kv[i];
        }
    }
    return NULL;
}

static int st_read(void *ctx, const char *key, void *buf, size_t len)
{
    kv_t *e = kv_find(ctx, key);
    if (e == NULL || e->len != len) {
        return -1;
    }
    memcpy(buf, e->val, len);
    return 0;
}

static int st_write(void *ctx, const char *key, const void *buf, size_t len)
{
    store_t *s = ctx;
    if (s->fail_meta_writes && strcmp(key, "meta") == 0) {
        return -1;
    }
    kv_t *e = kv_find(s, key);
    for (int i = 0; e == NULL && i < KV_MAX; i++) {
        if (!s->kv[i].used) {
            e = &s->kv[i];
            e->used = 1;
            snprintf(e->key, sizeof(e->key), "%s", key);
        }
    }
    if (e == NULL || len > sizeof(e->val)) {
        return -1;
    }
    memcpy(e->val, buf, len);
    e->len = len;
    return 0;
}

static int st_erase(void *ctx)
{
    memset(((store_t *)ctx)->kv, 0, sizeof(((store_t *)ctx)->kv));
    return 0;
}

static store_t g_store;
static const evlog_backend_t BE = { st_read, st_write, st_erase, &g_store };

static void reset_store(void)
{
    memset(&g_store, 0, sizeof(g_store));
}

static n2_evt_t ev(uint8_t type, uint8_t ch, int32_t v)
{
    n2_evt_t e = { type, ch, v };
    return e;
}

static void test_survives_power_cut(void)
{
    reset_store();
    evlog_t l;
    TEST_ASSERT_EQ(evlog_open(&l, &BE, 64), 0);
    n2_evt_t e = ev(N2_EV_SURGE, 2, 27000);
    TEST_ASSERT_EQ(evlog_append(&l, 1767300000123LL, N2_TS_RTC, &e, NULL), 0);
    e = ev(N2_EV_PTC_TRIP, 2, 800);
    TEST_ASSERT_EQ(evlog_append(&l, 1767300000456LL, N2_TS_RTC, &e, NULL), 0);

    /* coupure totale : nouvelle instance sur le même stockage */
    evlog_t l2;
    TEST_ASSERT_EQ(evlog_open(&l2, &BE, 64), 0);
    TEST_ASSERT_EQ(l2.next_seq, 3);
    n2_record_t r;
    TEST_ASSERT_EQ(evlog_read(&l2, 1, &r), 0);
    TEST_ASSERT_EQ(r.type, N2_EV_SURGE);
    TEST_ASSERT_EQ(r.channel, 2);
    TEST_ASSERT_EQ(r.value, 27000);
    TEST_ASSERT(r.ts_ms == 1767300000123LL);
    TEST_ASSERT_EQ(r.tsrc, N2_TS_RTC);
    TEST_ASSERT_EQ(evlog_read(&l2, 2, &r), 0);
    TEST_ASSERT_EQ(r.type, N2_EV_PTC_TRIP);
    TEST_ASSERT(evlog_read(&l2, 3, &r) != 0);
}

static void test_recovery_when_meta_not_written(void)
{
    reset_store();
    evlog_t l;
    evlog_open(&l, &BE, 16);
    n2_evt_t e = ev(N2_EV_SURGE, 1, 1000);
    evlog_append(&l, 1, N2_TS_UPTIME, &e, NULL);
    g_store.fail_meta_writes = 1; /* coupure entre enregistrement et métadonnées */
    e.value = 2000;
    evlog_append(&l, 2, N2_TS_UPTIME, &e, NULL);
    g_store.fail_meta_writes = 0;
    evlog_t l2;
    evlog_open(&l2, &BE, 16);
    TEST_ASSERT_EQ(l2.next_seq, 3);
    n2_record_t r;
    TEST_ASSERT_EQ(evlog_read(&l2, 2, &r), 0);
    TEST_ASSERT_EQ(r.value, 2000);
}

static void test_corruption_detected(void)
{
    reset_store();
    evlog_t l;
    evlog_open(&l, &BE, 16);
    n2_evt_t e = ev(N2_EV_SURGE, 1, 1000);
    evlog_append(&l, 1, N2_TS_UPTIME, &e, NULL);
    kv_find(&g_store, "e1")->val[12] ^= 0x40; /* bit inversé dans la valeur */
    n2_record_t r;
    TEST_ASSERT(evlog_read(&l, 1, &r) != 0);
    kv_find(&g_store, "meta")->val[4] ^= 1; /* métadonnées corrompues : réinitialisées */
    evlog_t l2;
    TEST_ASSERT_EQ(evlog_open(&l2, &BE, 16), 0);
    TEST_ASSERT_EQ(l2.first_seq, 1);
}

static void test_ring_overflow_and_publish_cursor(void)
{
    reset_store();
    evlog_t l;
    evlog_open(&l, &BE, 8);
    for (int i = 1; i <= 20; i++) {
        n2_evt_t e = ev(N2_EV_ZERO_V_GAP, 1, i);
        evlog_append(&l, i, N2_TS_UPTIME, &e, NULL);
    }
    TEST_ASSERT_EQ(evlog_oldest(&l), 13);
    n2_record_t r;
    TEST_ASSERT(evlog_read(&l, 12, &r) != 0);
    TEST_ASSERT_EQ(evlog_read(&l, 13, &r), 0);
    TEST_ASSERT_EQ(r.value, 13);
    TEST_ASSERT_EQ(evlog_pending(&l), 8);
    /* réseau absent puis retour : publication dans l'ordre depuis le plus ancien conservé */
    TEST_ASSERT_EQ(evlog_next_unpublished(&l, &r), 0);
    TEST_ASSERT_EQ(r.seq, 13);
    TEST_ASSERT_EQ(evlog_mark_published(&l, r.seq), 0);
    TEST_ASSERT_EQ(evlog_next_unpublished(&l, &r), 0);
    TEST_ASSERT_EQ(r.seq, 14);
    TEST_ASSERT_EQ(evlog_pending(&l), 7);
    TEST_ASSERT(evlog_mark_published(&l, 13) != 0); /* pas de recul */
    evlog_mark_published(&l, 20);
    TEST_ASSERT(evlog_next_unpublished(&l, &r) != 0);
    /* le curseur de publication survit à la coupure */
    evlog_t l2;
    evlog_open(&l2, &BE, 8);
    TEST_ASSERT_EQ(evlog_pending(&l2), 0);
    TEST_ASSERT_EQ(l2.pub_seq, 20);
}

static void test_clear_is_local_and_keeps_numbering(void)
{
    reset_store();
    evlog_t l;
    evlog_open(&l, &BE, 8);
    for (int i = 0; i < 3; i++) {
        n2_evt_t e = ev(N2_EV_SURGE, 0, i);
        evlog_append(&l, i, N2_TS_UPTIME, &e, NULL);
    }
    TEST_ASSERT_EQ(evlog_clear_local(&l), 0);
    n2_record_t r;
    TEST_ASSERT(evlog_read(&l, 1, &r) != 0);
    n2_evt_t e = ev(N2_EV_LOG_CLEARED, 0, 4);
    evlog_append(&l, 10, N2_TS_UPTIME, &e, NULL);
    evlog_t l2;
    evlog_open(&l2, &BE, 8);
    TEST_ASSERT_EQ(evlog_oldest(&l2), 4);
    TEST_ASSERT_EQ(evlog_read(&l2, 4, &r), 0);
    TEST_ASSERT_EQ(r.type, N2_EV_LOG_CLEARED);
}

static void test_timebase_rtc_then_ntp(void)
{
    timebase_t tb;
    timebase_init(&tb);
    uint8_t src;
    TEST_ASSERT(timebase_now(&tb, 5000, &src) == 5000);
    TEST_ASSERT_EQ(src, N2_TS_UPTIME);
    /* RTC non initialisée (date 2000) : refusée */
    TEST_ASSERT_EQ(timebase_set(&tb, 946684800000LL, 5000, N2_TS_RTC), -1);
    TEST_ASSERT_EQ(timebase_set(&tb, 1790000000000LL, 5000, N2_TS_RTC), 0);
    TEST_ASSERT(timebase_now(&tb, 6000, &src) == 1790000001000LL);
    TEST_ASSERT_EQ(src, N2_TS_RTC);
    TEST_ASSERT_EQ(timebase_set(&tb, 1790000002500LL, 7000, N2_TS_NTP), 0);
    TEST_ASSERT(timebase_now(&tb, 8000, &src) == 1790000003500LL);
    TEST_ASSERT_EQ(src, N2_TS_NTP);
    /* la RTC ne remplace pas une heure NTP */
    TEST_ASSERT_EQ(timebase_set(&tb, 1790000000000LL, 9000, N2_TS_RTC), 1);
    TEST_ASSERT_EQ(tb.src, N2_TS_NTP);
}

int main(void)
{
    RUN_TEST(test_survives_power_cut);
    RUN_TEST(test_recovery_when_meta_not_written);
    RUN_TEST(test_corruption_detected);
    RUN_TEST(test_ring_overflow_and_publish_cursor);
    RUN_TEST(test_clear_is_local_and_keeps_numbering);
    RUN_TEST(test_timebase_rtc_then_ntp);
    return TEST_END();
}
