#include "mqtt_cmd.h"

#include <string.h>

#include "n2_sha256.h"

/* Liste blanche exhaustive. Aucune entrée n'agit sur le bus I2C. */
static const struct {
    const char *name;
    n2_cmd_id_t id;
} k_cmds[] = {
    { "arm", N2_CMD_ARM },
    { "disarm", N2_CMD_DISARM },
    { "debug", N2_CMD_DEBUG },
    { "status", N2_CMD_STATUS },
    { "events", N2_CMD_EVENTS },
    { "frames", N2_CMD_FRAMES },
    { "ota", N2_CMD_OTA },
};

const char *n2_cmd_name(n2_cmd_id_t id)
{
    for (size_t i = 0; i < sizeof(k_cmds) / sizeof(k_cmds[0]); i++) {
        if (k_cmds[i].id == id) {
            return k_cmds[i].name;
        }
    }
    return "none";
}

const char *n2_cmd_result_name(n2_cmd_result_t r)
{
    switch (r) {
    case N2_CMDR_OK:        return "ok";
    case N2_CMDR_TOO_LONG:  return "too_long";
    case N2_CMDR_NO_MAC:    return "no_mac";
    case N2_CMDR_MALFORMED: return "malformed";
    case N2_CMDR_BAD_MAC:   return "bad_mac";
    case N2_CMDR_REPLAY:    return "replay";
    case N2_CMDR_UNKNOWN:   return "unknown_cmd";
    case N2_CMDR_BAD_ARG:   return "bad_arg";
    case N2_CMDR_NO_KEY:    return "no_key";
    default:                return "error";
    }
}

static const char HEX[] = "0123456789abcdef";

void n2_cmd_mac_hex(const uint8_t *key, size_t keylen, const char *device_id, const char *signed_part,
                    size_t signed_len, char out_hex[65])
{
    n2_hmac_t h;
    uint8_t mac[32];
    n2_hmac_init(&h, key, keylen);
    n2_hmac_update(&h, device_id, strlen(device_id));
    n2_hmac_update(&h, "|", 1);
    n2_hmac_update(&h, signed_part, signed_len);
    n2_hmac_final(&h, mac);
    for (int i = 0; i < 32; i++) {
        out_hex[2 * i] = HEX[mac[i] >> 4];
        out_hex[2 * i + 1] = HEX[mac[i] & 0x0F];
    }
    out_hex[64] = '\0';
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_u32(const char *s, size_t n, uint32_t *out)
{
    if (n == 0 || n > 10) {
        return -1;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        v = v * 10u + (uint64_t)(s[i] - '0');
    }
    if (v > 0xFFFFFFFFu) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int key_is_usable(const uint8_t *key, size_t keylen)
{
    if (key == NULL || keylen < 16) {
        return 0;
    }
    uint8_t acc = 0;
    for (size_t i = 0; i < keylen; i++) {
        acc |= key[i];
    }
    return acc != 0;  /* clé nulle de l'exemple de provisionnement = non provisionné */
}

n2_cmd_result_t n2_cmd_parse_verify(const char *p, size_t len, const uint8_t *key, size_t keylen,
                                    const char *device_id, uint32_t last_seq, n2_cmd_t *out)
{
    memset(out, 0, sizeof(*out));
    if (len > N2_CMD_MAX_LEN) {
        return N2_CMDR_TOO_LONG;
    }
    if (!key_is_usable(key, keylen) || device_id == NULL || device_id[0] == '\0') {
        return N2_CMDR_NO_KEY;
    }

    /* 1. Le MAC termine obligatoirement le message. */
    static const char MACK[] = ";mac=";
    const size_t mk = sizeof(MACK) - 1u;
    if (len < mk + 64u || memcmp(p + len - 64u - mk, MACK, mk) != 0) {
        return N2_CMDR_NO_MAC;
    }
    size_t signed_len = len - 64u - mk;
    uint8_t got[32];
    for (int i = 0; i < 32; i++) {
        int hi = hexval(p[signed_len + mk + 2u * (size_t)i]);
        int lo = hexval(p[signed_len + mk + 2u * (size_t)i + 1u]);
        if (hi < 0 || lo < 0) {
            return N2_CMDR_MALFORMED;
        }
        got[i] = (uint8_t)((hi << 4) | lo);
    }

    /* 2. Vérification avant toute interprétation du contenu. */
    char exp_hex[65];
    n2_cmd_mac_hex(key, keylen, device_id, p, signed_len, exp_hex);
    uint8_t exp[32];
    for (int i = 0; i < 32; i++) {
        exp[i] = (uint8_t)((hexval(exp_hex[2 * i]) << 4) | hexval(exp_hex[2 * i + 1]));
    }
    if (!n2_ct_equal(got, exp, 32)) {
        return N2_CMDR_BAD_MAC;
    }

    /* 3. Champs de la partie signée. */
    int have_v = 0, have_seq = 0, have_cmd = 0, have_arg = 0, have_ttl = 0;
    char cmd[16] = { 0 };
    size_t i = 0;
    while (i < signed_len) {
        size_t j = i;
        while (j < signed_len && p[j] != ';') {
            j++;
        }
        const char *f = p + i;
        size_t fl = j - i;
        const char *eq = memchr(f, '=', fl);
        if (eq == NULL) {
            return N2_CMDR_MALFORMED;
        }
        size_t kl = (size_t)(eq - f);
        const char *val = eq + 1;
        size_t vl = fl - kl - 1u;
        for (size_t k = 0; k < vl; k++) {
            if ((unsigned char)val[k] < 0x21 || (unsigned char)val[k] > 0x7E) {
                return N2_CMDR_MALFORMED;
            }
        }
        if (kl == 1 && f[0] == 'v') {
            if (have_v++ || vl != 1 || val[0] != '1') return N2_CMDR_MALFORMED;
        } else if (kl == 3 && memcmp(f, "seq", 3) == 0) {
            if (have_seq++ || parse_u32(val, vl, &out->seq) != 0) return N2_CMDR_MALFORMED;
        } else if (kl == 3 && memcmp(f, "cmd", 3) == 0) {
            if (have_cmd++ || vl == 0 || vl >= sizeof(cmd)) return N2_CMDR_MALFORMED;
            memcpy(cmd, val, vl);
        } else if (kl == 3 && memcmp(f, "arg", 3) == 0) {
            if (have_arg++ || vl == 0 || vl > N2_CMD_ARG_MAX) return N2_CMDR_MALFORMED;
            memcpy(out->arg, val, vl);
            out->arg[vl] = '\0';
        } else if (kl == 3 && memcmp(f, "ttl", 3) == 0) {
            if (have_ttl++ || parse_u32(val, vl, &out->ttl_s) != 0) return N2_CMDR_MALFORMED;
        } else {
            return N2_CMDR_MALFORMED;
        }
        i = j + 1u;
    }
    if (!have_v || !have_seq || !have_cmd) {
        return N2_CMDR_MALFORMED;
    }

    /* 4. Anti-rejeu. */
    if (out->seq <= last_seq) {
        return N2_CMDR_REPLAY;
    }

    /* 5. Liste blanche. */
    for (size_t k = 0; k < sizeof(k_cmds) / sizeof(k_cmds[0]); k++) {
        if (strcmp(cmd, k_cmds[k].name) == 0) {
            out->id = k_cmds[k].id;
            break;
        }
    }
    if (out->id == N2_CMD_NONE) {
        return N2_CMDR_UNKNOWN;
    }

    /* 6. Arguments. */
    switch (out->id) {
    case N2_CMD_DEBUG:
        if (strcmp(out->arg, "on") != 0 && strcmp(out->arg, "off") != 0) return N2_CMDR_BAD_ARG;
        if (have_ttl && (out->ttl_s == 0 || out->ttl_s > N2_CMD_TTL_MAX)) return N2_CMDR_BAD_ARG;
        break;
    case N2_CMD_OTA:
        if (!have_arg) return N2_CMDR_BAD_ARG;
        break;
    default:
        if (have_arg || have_ttl) return N2_CMDR_BAD_ARG;
        break;
    }
    return N2_CMDR_OK;
}
