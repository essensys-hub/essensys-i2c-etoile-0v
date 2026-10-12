#pragma once

#include <stdint.h>

#include "esp_err.h"

/* Configure les broches d'écoute EN ENTRÉE uniquement (aucune autre configuration possible). */
esp_err_t n2_sniffer_config_pins(void);
/* Démarre l'interruption sur fronts et la tâche de décodage. */
esp_err_t n2_sniffer_start(void);
uint32_t n2_sniffer_edges_dropped(void);
