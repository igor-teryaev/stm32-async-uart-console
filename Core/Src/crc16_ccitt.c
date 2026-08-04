#include "crc16_ccitt.h"

#define CRC16_CCITT_POLYNOMIAL 0x1021U

uint16_t crc16_ccitt_false_update(
    uint16_t crc,
    const uint8_t *data,
    size_t length
)
{
    if (data == NULL)
    {
        return crc;
    }

    for (size_t i = 0U; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8U;

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)(
                    (crc << 1U) ^
                    CRC16_CCITT_POLYNOMIAL
                );
            }
            else
            {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }

    return crc;
}

uint16_t crc16_ccitt_false_calculate(
    const uint8_t *data,
    size_t length
)
{
    return crc16_ccitt_false_update(
        CRC16_CCITT_FALSE_INITIAL,
        data,
        length
    );
}
