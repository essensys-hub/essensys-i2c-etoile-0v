/*
 * État partagé de l'application N2 (cible ESP32-C3). Toute la logique est dans
 * le composant n2_core (pur, testé sur hôte) ; ce module l'orchestre.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "alarm.h"
#include "bus_health.h"
#include "cmd_handler.h"
#include "debug_mode.h"
#include "event_log.h"
#include "frame_log.h"
#include "i2c_decoder.h"
#include "n2_events.h"
#include "n2_prov.h"
#include "power_monitor.h"
#include "siren.h"
#include "surge.h"
#include "timebase.h"

#define N2_FRAME_LOG_CAP 256

typedef struct {
    SemaphoreHandle_t lock;      /* protège tous les champs ci-dessous */
    i2c_decoder_t dec;
    flog_t flog;
    bus_health_t bh;
    power_monitor_t pm;
    surge_t surge;
    alarm_t alarm;
    siren_t siren;
    n2_debug_t dbg;
    timebase_t tb;
    evlog_t evlog;
    bool evlog_ok;
    n2_cmd_ctx_t cmd;
    n2_prov_t prov;
    bool spy_running;
    uint32_t debug_ttl_s;
    TaskHandle_t publisher;
} n2_app_t;

extern n2_app_t g_n2;

#define N2_LOCK()   xSemaphoreTake(g_n2.lock, portMAX_DELAY)
#define N2_UNLOCK() xSemaphoreGive(g_n2.lock)

int64_t n2_uptime_ms(void);
static inline uint32_t n2_now_ms(void) { return (uint32_t)n2_uptime_ms(); }

/* Horodate, persiste et traite (sirène) les événements ; verrou déjà pris. */
void n2_app_post_locked(n2_evq_t *q);
void n2_app_post(n2_evq_t *q);
void n2_app_post_one(uint8_t type, uint8_t channel, int32_t value);
void n2_app_notify_publisher(void);
uint32_t n2_app_edges_dropped(void);

/* Commande MQTT reçue (chemin réseau) : authentification et exécution. */
n2_cmd_result_t n2_app_handle_cmd(const char *payload, size_t len, n2_cmd_t *out);
/* Actions locales (console, contact à clé, bouton). */
void n2_app_local_arm(bool arm, al_source_t src);
void n2_app_local_debug(bool on, uint32_t ttl_s);
int n2_app_local_clear_events(void);
/* Heure acquise (NTP). */
void n2_app_time_synced(int64_t epoch_ms);
