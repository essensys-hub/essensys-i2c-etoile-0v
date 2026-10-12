#include "cmd_handler.h"

#include <string.h>

n2_cmd_result_t n2_cmd_handle(n2_cmd_ctx_t *ctx, const char *payload, size_t len, uint32_t now_ms,
                              int64_t now64_ms, n2_evq_t *q, n2_cmd_t *out)
{
    n2_cmd_result_t r = n2_cmd_parse_verify(payload, len, ctx->key, ctx->keylen, ctx->device_id,
                                            ctx->last_seq, out);
    if (r != N2_CMDR_OK) {
        n2_evq_push(q, N2_EV_CMD_REJECTED, 0, (int32_t)r);
        return r;
    }
    ctx->last_seq = out->seq;

    switch (out->id) {
    case N2_CMD_ARM:
        alarm_arm(ctx->alarm, AL_SRC_MQTT_AUTH, now_ms, q);
        break;
    case N2_CMD_DISARM:
        alarm_disarm(ctx->alarm, AL_SRC_MQTT_AUTH, q);
        break;
    case N2_CMD_DEBUG:
        if (strcmp(out->arg, "on") == 0) {
            n2_debug_enable(ctx->dbg, N2_DBG_SRC_MQTT_AUTH, out->ttl_s, now64_ms, q);
        } else {
            n2_debug_disable(ctx->dbg, q);
        }
        break;
    default:
        /* STATUS, EVENTS, FRAMES, OTA : exécutés par la plateforme. */
        break;
    }
    return N2_CMDR_OK;
}
