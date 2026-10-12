#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "event_log.h"

#define N2_EVLOG_PARTITION "evlog"

esp_err_t n2_store_open(evlog_t *l, uint16_t cap);
