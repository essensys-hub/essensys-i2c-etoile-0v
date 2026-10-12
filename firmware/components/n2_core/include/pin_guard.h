/*
 * Garde de démarrage : vérifie que les broches d'écoute I2C ne sont pas des
 * sorties. La garantie principale reste matérielle (isolateur unidirectionnel,
 * design D2) ; cette garde détecte une erreur de firmware.
 */
#ifndef PIN_GUARD_H
#define PIN_GUARD_H

#include <stdint.h>

/* Retourne 1 si la broche a sa sortie activée (GPIO ou périphérique). */
typedef int (*n2_pin_is_output_fn)(int pin, void *ctx);

/* -1 si aucune broche du masque n'est en sortie, sinon le numéro de la première. */
int n2_pin_guard_check(uint64_t mask, n2_pin_is_output_fn is_output, void *ctx);

#endif
