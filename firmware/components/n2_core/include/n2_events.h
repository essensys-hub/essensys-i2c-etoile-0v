/*
 * Événements produits par la logique pure (surveillance, alarme, surtension,
 * sécurité). La plateforme les horodate, les persiste (event_log) et les
 * publie en MQTT.
 */
#ifndef N2_EVENTS_H
#define N2_EVENTS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    N2_EV_NONE = 0,
    N2_EV_BOOT,
    /* bus-health-monitoring */
    N2_EV_BA_MUTE,         /* channel = adresse BA, value = ms depuis le premier NACK */
    N2_EV_BA_BACK,         /* channel = adresse BA */
    N2_EV_LINE_STUCK,      /* channel = N2_LINE_SDA/SCL, value = ms à l'état bas */
    N2_EV_LINE_RELEASED,   /* channel = ligne, value = durée totale ms */
    N2_EV_V15_LOW,         /* value = mV */
    N2_EV_V15_OK,          /* value = mV */
    N2_EV_ON_BATTERY,      /* +15 V absent (coupé par le BP) ; value = mV */
    N2_EV_PTC_TRIP,        /* channel = port 1..3, value = mV aval */
    N2_EV_PORT_LOW,        /* channel = port, value = mV aval */
    N2_EV_PORT_OPEN,       /* channel = port (cavalier ouvert) */
    N2_EV_PORT_OK,         /* channel = port, value = mV aval */
    N2_EV_BATT_LOW,        /* value = mV */
    N2_EV_BATT_OK,         /* value = mV */
    /* surge-event-logging */
    N2_EV_SURGE,           /* latch surtension : voir SURGE_EV_CHANNEL (surge.h) */
    N2_EV_ZERO_V_GAP,      /* channel = port, value = écart signé mV */
    /* intrusion-alarm */
    N2_EV_ARMING,          /* channel = source, value = délai de sortie ms */
    N2_EV_ARMED,
    N2_EV_DISARMED,        /* channel = source */
    N2_EV_ARM_REFUSED,     /* channel = zone fautive (0xFF = boîtier), value = source */
    N2_EV_ENTRY_DELAY,     /* channel = zone */
    N2_EV_ALARM,           /* channel = zone (0xFF = boîtier) */
    N2_EV_TAMPER,          /* channel = zone (0xFF = boîtier), value = état de boucle */
    N2_EV_TAMPER_CLEAR,    /* channel = zone */
    N2_EV_SIREN_TRIGGER,   /* demande de sirène émise par l'alarme ; channel = zone */
    N2_EV_SIREN_ON,
    N2_EV_SIREN_OFF,       /* value = raison (N2_SIREN_OFF_*) */
    N2_EV_SIREN_FAULT,     /* value = nombre de reprises */
    /* sécurité / exploitation */
    N2_EV_CMD_REJECTED,    /* value = code de rejet (n2_cmd_result_t) */
    N2_EV_DEBUG_ON,        /* channel = source, value = durée s */
    N2_EV_DEBUG_OFF,
    N2_EV_OTA_REJECTED,    /* value = raison */
    N2_EV_OTA_APPLIED,
    N2_EV_LOG_CLEARED,     /* value = premier numéro conservé */
    N2_EV_PIN_GUARD_FAIL,  /* channel = broche */
    N2_EV__COUNT
} n2_ev_type_t;

#define N2_LINE_SDA 0
#define N2_LINE_SCL 1
#define N2_CH_BOX   0xFF

typedef struct {
    uint8_t type;
    uint8_t channel;
    int32_t value;
} n2_evt_t;

/* File d'événements bornée remplie par les modules purs. */
typedef struct {
    n2_evt_t *buf;
    size_t max;
    size_t n;
    uint32_t dropped;
} n2_evq_t;

void n2_evq_init(n2_evq_t *q, n2_evt_t *buf, size_t max);
void n2_evq_push(n2_evq_t *q, uint8_t type, uint8_t channel, int32_t value);
const char *n2_ev_name(uint8_t type);

#endif
