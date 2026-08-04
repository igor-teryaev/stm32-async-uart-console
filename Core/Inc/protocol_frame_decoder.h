#ifndef PROTOCOL_FRAME_DECODER_H
#define PROTOCOL_FRAME_DECODER_H

#include "protocol_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    PROTOCOL_DECODER_WAIT_MAGIC_0,
    PROTOCOL_DECODER_WAIT_MAGIC_1,
    PROTOCOL_DECODER_READ_VERSION,
    PROTOCOL_DECODER_READ_TYPE,
    PROTOCOL_DECODER_READ_SEQUENCE_HIGH,
    PROTOCOL_DECODER_READ_SEQUENCE_LOW,
    PROTOCOL_DECODER_READ_LENGTH_HIGH,
    PROTOCOL_DECODER_READ_LENGTH_LOW,
    PROTOCOL_DECODER_READ_PAYLOAD,
    PROTOCOL_DECODER_READ_CRC_HIGH,
    PROTOCOL_DECODER_READ_CRC_LOW
} ProtocolFrameDecoderState;

typedef struct
{
    ProtocolFrameDecoderState state;
    ProtocolFrame frame;

    uint16_t calculated_crc;
    uint16_t received_crc;
    size_t payload_index;

    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t format_error_count;
} ProtocolFrameDecoder;

void protocol_frame_decoder_init(
    ProtocolFrameDecoder *decoder
);

void protocol_frame_decoder_reset(
    ProtocolFrameDecoder *decoder
);

bool protocol_frame_decoder_feed_byte(
    ProtocolFrameDecoder *decoder,
    uint8_t byte,
    const ProtocolFrame **frame
);

#endif
