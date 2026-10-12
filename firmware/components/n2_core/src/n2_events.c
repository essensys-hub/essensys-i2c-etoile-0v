#include "n2_events.h"

void n2_evq_init(n2_evq_t *q, n2_evt_t *buf, size_t max)
{
    q->buf = buf;
    q->max = max;
    q->n = 0;
    q->dropped = 0;
}

void n2_evq_push(n2_evq_t *q, uint8_t type, uint8_t channel, int32_t value)
{
    if (q == NULL) {
        return;
    }
    if (q->n >= q->max) {
        q->dropped++;
        return;
    }
    q->buf[q->n].type = type;
    q->buf[q->n].channel = channel;
    q->buf[q->n].value = value;
    q->n++;
}

const char *n2_ev_name(uint8_t type)
{
    static const char *const names[N2_EV__COUNT] = {
        [N2_EV_NONE] = "none",
        [N2_EV_BOOT] = "boot",
        [N2_EV_BA_MUTE] = "ba_mute",
        [N2_EV_BA_BACK] = "ba_back",
        [N2_EV_LINE_STUCK] = "line_stuck",
        [N2_EV_LINE_RELEASED] = "line_released",
        [N2_EV_V15_LOW] = "v15_low",
        [N2_EV_V15_OK] = "v15_ok",
        [N2_EV_ON_BATTERY] = "on_battery",
        [N2_EV_PTC_TRIP] = "ptc_trip",
        [N2_EV_PORT_LOW] = "port_low",
        [N2_EV_PORT_OPEN] = "port_open",
        [N2_EV_PORT_OK] = "port_ok",
        [N2_EV_BATT_LOW] = "batt_low",
        [N2_EV_BATT_OK] = "batt_ok",
        [N2_EV_SURGE] = "surge",
        [N2_EV_ZERO_V_GAP] = "zero_v_gap",
        [N2_EV_ARMING] = "arming",
        [N2_EV_ARMED] = "armed",
        [N2_EV_DISARMED] = "disarmed",
        [N2_EV_ARM_REFUSED] = "arm_refused",
        [N2_EV_ENTRY_DELAY] = "entry_delay",
        [N2_EV_ALARM] = "alarm",
        [N2_EV_TAMPER] = "tamper",
        [N2_EV_TAMPER_CLEAR] = "tamper_clear",
        [N2_EV_SIREN_TRIGGER] = "siren_trigger",
        [N2_EV_SIREN_ON] = "siren_on",
        [N2_EV_SIREN_OFF] = "siren_off",
        [N2_EV_SIREN_FAULT] = "siren_fault",
        [N2_EV_CMD_REJECTED] = "cmd_rejected",
        [N2_EV_DEBUG_ON] = "debug_on",
        [N2_EV_DEBUG_OFF] = "debug_off",
        [N2_EV_OTA_REJECTED] = "ota_rejected",
        [N2_EV_OTA_APPLIED] = "ota_applied",
        [N2_EV_LOG_CLEARED] = "log_cleared",
        [N2_EV_PIN_GUARD_FAIL] = "pin_guard_fail",
    };
    if (type >= N2_EV__COUNT || names[type] == NULL) {
        return "unknown";
    }
    return names[type];
}
