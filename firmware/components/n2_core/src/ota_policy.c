#include "ota_policy.h"

#include <string.h>

#include "n2_crc.h"

int n2_ota_url_ok(const char *url, const char *prefix)
{
    if (url == NULL) {
        return 0;
    }
    size_t n = strlen(url);
    if (n <= 8 || n > N2_OTA_URL_MAX || strncmp(url, "https://", 8) != 0) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 0x20 || c >= 0x7F) {
            return 0;
        }
    }
    if (prefix != NULL && prefix[0] != '\0') {
        size_t pn = strlen(prefix);
        if (strncmp(prefix, "https://", 8) != 0 || strncmp(url, prefix, pn) != 0 || n == pn) {
            return 0;
        }
    }
    return 1;
}

int n2_ota_layout_ok(size_t image_len)
{
    return image_len >= 2u * N2_SIG_SECTOR_LEN && (image_len % N2_SIG_SECTOR_LEN) == 0;
}

int n2_ota_sig_block_ok(const uint8_t *blk, size_t len)
{
    if (blk == NULL || len < N2_SIG_BLOCK_LEN) {
        return 0;
    }
    if (blk[0] != N2_SIG_BLOCK_MAGIC || blk[1] != N2_SIG_BLOCK_VER_RSA) {
        return 0;
    }
    uint32_t stored = (uint32_t)blk[N2_SIG_BLOCK_CRC_OFF] | ((uint32_t)blk[N2_SIG_BLOCK_CRC_OFF + 1] << 8) |
                      ((uint32_t)blk[N2_SIG_BLOCK_CRC_OFF + 2] << 16) |
                      ((uint32_t)blk[N2_SIG_BLOCK_CRC_OFF + 3] << 24);
    return n2_crc32(0, blk, N2_SIG_BLOCK_CRC_OFF) == stored;
}
