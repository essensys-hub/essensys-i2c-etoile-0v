/* SHA-256 et HMAC-SHA256 autonomes (FIPS 180-4, RFC 2104), testables sur hôte. */
#ifndef N2_SHA256_H
#define N2_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t n;
} n2_sha256_t;

void n2_sha256_init(n2_sha256_t *c);
void n2_sha256_update(n2_sha256_t *c, const void *data, size_t len);
void n2_sha256_final(n2_sha256_t *c, uint8_t out[32]);
void n2_sha256(const void *data, size_t len, uint8_t out[32]);

typedef struct {
    n2_sha256_t inner;
    uint8_t okey[64];
} n2_hmac_t;

void n2_hmac_init(n2_hmac_t *h, const uint8_t *key, size_t keylen);
void n2_hmac_update(n2_hmac_t *h, const void *data, size_t len);
void n2_hmac_final(n2_hmac_t *h, uint8_t out[32]);

/* Comparaison en temps constant ; 1 si égal. */
int n2_ct_equal(const uint8_t *a, const uint8_t *b, size_t len);

#endif
