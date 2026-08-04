#ifndef CRC16_CCITT_H
#define CRC16_CCITT_H

#include <stddef.h>
#include <stdint.h>

#define CRC16_CCITT_FALSE_INITIAL 0xFFFFU

uint16_t crc16_ccitt_false_update(
    uint16_t crc,
    const uint8_t *data,
    size_t length
);

uint16_t crc16_ccitt_false_calculate(
    const uint8_t *data,
    size_t length
);

#endif
