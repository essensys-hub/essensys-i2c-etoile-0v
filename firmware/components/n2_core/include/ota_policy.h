/*
 * Politique OTA : URL https uniquement (et préfixe provisionné), image signée
 * Secure Boot V2 (RSA-3072). La vérification cryptographique est faite par
 * ESP-IDF (esp_https_ota_finish -> esp_ota_end, signature RSA-PSS) ; ces
 * contrôles de forme rejettent plus tôt une image non signée.
 */
#ifndef OTA_POLICY_H
#define OTA_POLICY_H

#include <stddef.h>
#include <stdint.h>

#define N2_OTA_URL_MAX       255u
#define N2_SIG_SECTOR_LEN    4096u
#define N2_SIG_BLOCK_LEN     1216u
#define N2_SIG_BLOCK_CRC_OFF 1196u
#define N2_SIG_BLOCK_MAGIC   0xE7u
#define N2_SIG_BLOCK_VER_RSA 0x02u

typedef enum {
    N2_OTA_OK = 0,
    N2_OTA_URL_REJECTED,
    N2_OTA_LAYOUT,      /* longueur incompatible avec une image signée */
    N2_OTA_NO_SIG,      /* bloc de signature absent ou corrompu */
    N2_OTA_VERIFY,      /* refus par la vérification cryptographique ESP-IDF */
    N2_OTA_IO,
} n2_ota_result_t;

int n2_ota_url_ok(const char *url, const char *required_prefix);
/* Image signée : longueur multiple de 4096 octets, secteur de signature compris. */
int n2_ota_layout_ok(size_t image_len);
/* Vérifie la forme du 1er bloc de signature (magic, version, CRC-32). */
int n2_ota_sig_block_ok(const uint8_t *blk, size_t len);

#endif
