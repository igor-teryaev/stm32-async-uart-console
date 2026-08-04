#include "protocol_frame.h"

#include "crc16_ccitt.h"

#include <string.h>

#define PROTOCOL_FRAME_CRC_DATA_OFFSET 2U

static void protocol_frame_write_u16_be(
    uint8_t *output,
    uint16_t value
)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

bool protocol_frame_encode(
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
)
{
    size_t frame_length;
    size_t crc_offset;
    size_t crc_data_length;
    uint16_t crc;

    if (output_length == NULL)
    {
        return false;
    }

    *output_length = 0U;

    if (output == NULL)
    {
        return false;
    }

    if (payload_length >
        PROTOCOL_FRAME_MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    if ((payload_length > 0U) &&
        (payload == NULL))
    {
        return false;
    }

    frame_length =
        PROTOCOL_FRAME_HEADER_SIZE +
        payload_length +
        PROTOCOL_FRAME_CRC_SIZE;

    if (output_capacity < frame_length)
    {
        return false;
    }

    output[0] = PROTOCOL_FRAME_MAGIC_0;
    output[1] = PROTOCOL_FRAME_MAGIC_1;
    output[2] = PROTOCOL_FRAME_VERSION;
    output[3] = type;

    protocol_frame_write_u16_be(
        &output[4],
        sequence
    );

    protocol_frame_write_u16_be(
        &output[6],
        (uint16_t)payload_length
    );

    if (payload_length > 0U)
    {
        memcpy(
            &output[PROTOCOL_FRAME_HEADER_SIZE],
            payload,
            payload_length
        );
    }

    crc_offset =
        PROTOCOL_FRAME_HEADER_SIZE +
        payload_length;

    crc_data_length =
        (PROTOCOL_FRAME_HEADER_SIZE -
         PROTOCOL_FRAME_CRC_DATA_OFFSET) +
        payload_length;

    crc = crc16_ccitt_false_calculate(
        &output[PROTOCOL_FRAME_CRC_DATA_OFFSET],
        crc_data_length
    );

    protocol_frame_write_u16_be(
        &output[crc_offset],
        crc
    );

    *output_length = frame_length;
    return true;
}
