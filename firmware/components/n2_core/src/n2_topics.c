#include "n2_topics.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "timebase.h"

int n2_device_id_valid(const char *id)
{
    if (id == NULL) {
        return 0;
    }
    size_t n = strlen(id);
    if (n == 0 || n > N2_DEVICE_ID_MAX) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

int n2_topic(char *buf, size_t len, const char *device_id, const char *leaf)
{
    if (!n2_device_id_valid(device_id) || leaf == NULL || leaf[0] == '\0') {
        return -1;
    }
    int n = snprintf(buf, len, N2_TOPIC_ROOT "%s/%s", device_id, leaf);
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    return n;
}

int n2_uri_is_tls(const char *uri)
{
    return uri != NULL && strncmp(uri, "mqtts://", 8) == 0 && uri[8] != '\0';
}

static int clamp(int n, size_t len)
{
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    return n;
}

int n2_fmt_event(char *buf, size_t len, const n2_record_t *r)
{
    int n = snprintf(buf, len,
                     "{\"seq\":%" PRIu32 ",\"ts\":%" PRId64 ",\"tsrc\":\"%s\",\"type\":\"%s\","
                     "\"ch\":%u,\"value\":%" PRId32 "}",
                     r->seq, r->ts_ms, timebase_src_name(r->tsrc), n2_ev_name(r->type),
                     (unsigned)r->channel, r->value);
    return clamp(n, len);
}

static int hex_bytes(char *buf, size_t len, const uint8_t *b, size_t n)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        int k = snprintf(buf + o, len - o, i ? " %02x" : "%02x", b[i]);
        if (k < 0 || (size_t)k >= len - o) {
            return -1;
        }
        o += (size_t)k;
    }
    if (n == 0 && len > 0) {
        buf[0] = '\0';
    }
    return (int)o;
}

int n2_fmt_frame(char *buf, size_t len, const flog_entry_t *e, int detailed)
{
    const ess_frame_t *f = &e->f;
    int n = snprintf(buf, len,
                     "{\"seq\":%" PRIu32 ",\"ts\":%" PRId64 ",\"tsrc\":\"%s\",\"addr\":\"0x%02x\","
                     "\"code\":%u,\"name\":\"%s\",\"flags\":\"0x%04x\",\"ok\":%s",
                     e->seq, e->ts_ms, timebase_src_name(e->tsrc), f->addr, f->code,
                     ess_code_name(f->code), f->flags, f->flags == 0 ? "true" : "false");
    if (clamp(n, len) < 0) {
        return -1;
    }
    size_t o = (size_t)n;
    if (detailed) {
        char w[3 * ESS_MAX_WRITE + 1];
        char r[3 * ESS_MAX_READ + 1];
        if (hex_bytes(w, sizeof(w), f->w, f->wlen) < 0 || hex_bytes(r, sizeof(r), f->r, f->rlen) < 0) {
            return -1;
        }
        n = snprintf(buf + o, len - o, ",\"w\":\"%s\",\"r\":\"%s\"", w, r);
        if (clamp(n, len - o) < 0) {
            return -1;
        }
        o += (size_t)n;
        if (f->has_comm_stat) {
            n = snprintf(buf + o, len - o, ",\"comm_stat\":%u", f->comm_stat);
            if (clamp(n, len - o) < 0) {
                return -1;
            }
            o += (size_t)n;
        }
    }
    n = snprintf(buf + o, len - o, "}");
    if (clamp(n, len - o) < 0) {
        return -1;
    }
    return (int)(o + (size_t)n);
}

int n2_fmt_counters(char *buf, size_t len, const flog_counters_t *c, uint32_t edges_dropped,
                    uint32_t events_pending)
{
    int n = snprintf(buf, len,
                     "{\"frames\":%" PRIu32 ",\"ok\":%" PRIu32 ",\"errors\":%" PRIu32 ",\"nack\":%" PRIu32
                     ",\"crc\":%" PRIu32 ",\"bus\":%" PRIu32 ",\"overwritten\":%" PRIu32
                     ",\"lost\":%" PRIu32 ",\"edges_dropped\":%" PRIu32 ",\"events_pending\":%" PRIu32 "}",
                     c->total, c->ok, c->errors, c->addr_nack, c->crc_errors, c->bus_errors, c->overwritten,
                     c->lost, edges_dropped, events_pending);
    return clamp(n, len);
}
