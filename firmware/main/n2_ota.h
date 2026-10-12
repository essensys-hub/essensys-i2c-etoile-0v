#pragma once

#include "esp_err.h"

/* Lance une mise à jour OTA signée depuis une URL https autorisée (tâche 7.3). */
esp_err_t n2_ota_start(const char *url);
