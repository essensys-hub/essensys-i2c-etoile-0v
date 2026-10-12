#ifndef N2_CRC_H
#define N2_CRC_H

#include <stddef.h>
#include <stdint.h>

/* CRC-16/MODBUS du protocole I2C Essensys (init 0xFFFF, polynôme réfléchi
 * 0xA001), identique à us_CalculerCRCSurTrame() du BP (C/crc.c). */
uint16_t n2_crc16_modbus(const uint8_t *data, size_t len);

/* CRC-32 standard (zlib, réfléchi 0xEDB88320), utilisé par le bloc de
 * signature Secure Boot V2 et par le journal non volatil. */
uint32_t n2_crc32(uint32_t crc, const uint8_t *data, size_t len);

#endif
