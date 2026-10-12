/*
 * Brochage du module ESP32-C3-MINI-1 (U6) de la carte N2, révision B (source unique).
 *
 * Aligné sur scripts/n2_module.py (bloc C) au 2026-10-12 :
 *   U6.12 IO0  SNF_SCL    U6.13 IO1 SNF_SDA    U6.5  IO2 (tirage haut, libre)
 *   U6.6  IO3  EXP_INT    U6.18 IO4 I2C_SDA    U6.19 IO5 I2C_SCL
 *   U6.20 IO6  SIREN_EN   U6.21 IO7 LATCH_RST  U6.22 IO8 LED_STAT
 *   U6.23 IO9  BOOT       U6.16 IO10 RTC_INT   IO18/19 USB, IO20/21 UART0 (J9)
 * TODO(recoupement schéma) : revérifier après le gel du schéma (tâche 1.1, ERC)
 * et après le routage (tâche 1.3). Toute modification se fait ICI uniquement.
 *
 * Contraintes ESP32-C3-MINI-1 : GPIO11 à 17 = flash interne ; GPIO2, 8, 9 =
 * broches de démarrage (tirages hauts R12, R13, R14 sur la carte).
 */
#ifndef N2_PINS_H
#define N2_PINS_H

/* --- Écoute I2C : sorties de l'isolateur ISO7720 U1 (bus -> module, 2/0 canaux) ---
 * TOUJOURS des entrées. Aucun code ne doit les configurer en sortie (garde
 * n2_pin_guard + test check_no_bus_write.sh). Cavalier JP4 = coupure physique. */
#define N2_PIN_SNIFF_SCL      0   /* U1.6 -> SNF_SCL */
#define N2_PIN_SNIFF_SDA      1   /* U1.7 -> SNF_SDA */

/* --- Entrées locales --- */
#define N2_PIN_EXP_INT        3   /* /INT du TCA9534 (U8), drain ouvert, tirage R19 */
#define N2_PIN_USER_BTN       9   /* SW1 BOOT/service, actif bas : appui long = debug local */
#define N2_PIN_RTC_INT       10   /* /INT du RV-3028 (U7), réservée */

/* --- Sorties locales (côté module uniquement, jamais vers le bus Essensys) --- */
#define N2_PIN_SIREN_EN       6   /* U17 EN (boost 12 V) + U18 IN (BTS4140N), actif haut, R83 tirage bas */
#define N2_PIN_LATCH_RST      7   /* /CLR du 74HC74 (U16), impulsion BASSE de réarmement, R69 tirage haut */
#define N2_PIN_STATUS_LED     8   /* LED1 verte, active BASSE (strapping : haut au démarrage) */

/* --- Bus I2C LOCAL du module : RTC 0x52, TCA9534 0x20, MCP3428 x4 ---
 * Bus distinct, physiquement séparé du bus Essensys BP<->BA (R17/R18 4,7 kΩ). */
#define N2_PIN_LOCAL_SDA      4
#define N2_PIN_LOCAL_SCL      5

/* --- Extension d'E/S TCA9534 (U8), numéros de bit du port --- */
#define N2_EXP_LATCH_0V       0   /* P0 : latch écart 0 V (Q 74HC74), actif haut ; aussi EVI du RV-3028 */
#define N2_EXP_LATCH_OV       1   /* P1 : latch +15 V > 20 V, actif haut */
#define N2_EXP_CHG_CHRG       2   /* P2 : CN3058 en charge, actif bas */
#define N2_EXP_CHG_DONE       3   /* P3 : CN3058 charge terminée, actif bas */
#define N2_EXP_ARM_IN         4   /* P4 : contact à clé ARM (J11.1), actif bas = armer */
#define N2_EXP_PG_5V          5   /* P5 : +5 V présent (secteur), actif haut */
#define N2_EXP_LED_ALM        6   /* P6 : LED2 rouge, SORTIE active basse */
#define N2_EXP_P7             7   /* P7 : libre (tirage bas R23) */
#define N2_EXP_OUTPUTS_MASK   (1u << N2_EXP_LED_ALM)

#define N2_BIT(p) (1ULL << (p))

#define N2_SNIFF_PINS_MASK  (N2_BIT(N2_PIN_SNIFF_SCL) | N2_BIT(N2_PIN_SNIFF_SDA))

/* Toutes les broches que le firmware peut piloter (sorties ou drain ouvert). */
#define N2_DRIVEN_PINS_MASK (N2_BIT(N2_PIN_SIREN_EN) | N2_BIT(N2_PIN_LATCH_RST) | \
                             N2_BIT(N2_PIN_STATUS_LED) | N2_BIT(N2_PIN_LOCAL_SDA) | \
                             N2_BIT(N2_PIN_LOCAL_SCL))

#define N2_PLAIN_OUTPUT_PINS_MASK (N2_BIT(N2_PIN_SIREN_EN) | N2_BIT(N2_PIN_LATCH_RST) | \
                                   N2_BIT(N2_PIN_STATUS_LED))

#define N2_INPUT_PINS_MASK  (N2_BIT(N2_PIN_EXP_INT) | N2_BIT(N2_PIN_USER_BTN) | N2_BIT(N2_PIN_RTC_INT))

/* Broches réservées par la flash interne du module. */
#define N2_FLASH_PINS_MASK  (N2_BIT(11) | N2_BIT(12) | N2_BIT(13) | N2_BIT(14) | \
                             N2_BIT(15) | N2_BIT(16) | N2_BIT(17))

/* Garanties vérifiées à la compilation, sur cible comme sur hôte. */
_Static_assert((N2_DRIVEN_PINS_MASK & N2_SNIFF_PINS_MASK) == 0,
               "une broche d'ecoute I2C ne peut pas etre pilotee");
_Static_assert((N2_INPUT_PINS_MASK & N2_DRIVEN_PINS_MASK) == 0,
               "conflit entree/sortie dans le brochage");
_Static_assert((N2_INPUT_PINS_MASK & N2_SNIFF_PINS_MASK) == 0,
               "conflit entree locale/ecoute dans le brochage");
_Static_assert(((N2_SNIFF_PINS_MASK | N2_DRIVEN_PINS_MASK | N2_INPUT_PINS_MASK) &
                N2_FLASH_PINS_MASK) == 0,
               "broche reservee a la flash interne");

#endif
