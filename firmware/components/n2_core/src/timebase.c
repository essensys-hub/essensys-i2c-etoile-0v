#include "timebase.h"

void timebase_init(timebase_t *tb)
{
    tb->offset_ms = 0;
    tb->src = N2_TS_UPTIME;
}

int timebase_set(timebase_t *tb, int64_t epoch_ms, int64_t uptime_ms, n2_tsrc_t src)
{
    if (epoch_ms < N2_EPOCH_MIN_MS || src == N2_TS_UPTIME) {
        return -1;
    }
    if (src == N2_TS_RTC && tb->src == N2_TS_NTP) {
        return 1;
    }
    tb->offset_ms = epoch_ms - uptime_ms;
    tb->src = (uint8_t)src;
    return 0;
}

int64_t timebase_now(const timebase_t *tb, int64_t uptime_ms, uint8_t *src)
{
    if (src) {
        *src = tb->src;
    }
    if (tb->src == N2_TS_UPTIME) {
        return uptime_ms;
    }
    return uptime_ms + tb->offset_ms;
}

const char *timebase_src_name(uint8_t src)
{
    switch (src) {
    case N2_TS_RTC: return "rtc";
    case N2_TS_NTP: return "ntp";
    default:        return "uptime";
    }
}
