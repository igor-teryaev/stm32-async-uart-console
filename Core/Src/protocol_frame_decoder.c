#include "protocol_frame_decoder.h"

#include "crc16_ccitt.h"

#include <string.h>

static void protocol_frame_decoder_update_crc(
    ProtocolFrameDecoder *decoder,
    uint8_t byte
)
{
    decoder->calculated_crc =
        crc16_ccitt_false_update(
            decoder->calculated_crc,
            &byte,
            1U
        );
}

static void protocol_frame_decoder_reset_search(
    ProtocolFrameDecoder *decoder,
    uint8_t current_byte
)
{
    decoder->payload_index = 0U;
    decoder->received_crc = 0U;
    decoder->calculated_crc =
        CRC16_CCITT_FALSE_INITIAL;

    if (current_byte == PROTOCOL_FRAME_MAGIC_0)
    {
        decoder->state =
            PROTOCOL_DECODER_WAIT_MAGIC_1;
    }
    else
    {
        decoder->state =
            PROTOCOL_DECODER_WAIT_MAGIC_0;
    }
}

static void protocol_frame_decoder_begin_frame(
    ProtocolFrameDecoder *decoder
)
{
    decoder->frame.version = 0U;
    decoder->frame.type = 0U;
    decoder->frame.sequence = 0U;
    decoder->frame.payload_length = 0U;

    decoder->payload_index = 0U;
    decoder->received_crc = 0U;
    decoder->calculated_crc =
        CRC16_CCITT_FALSE_INITIAL;

    decoder->state =
        PROTOCOL_DECODER_READ_VERSION;
}

void protocol_frame_decoder_init(
    ProtocolFrameDecoder *decoder
)
{
    if (decoder == NULL)
    {
        return;
    }

    memset(decoder, 0, sizeof(*decoder));

    decoder->state =
        PROTOCOL_DECODER_WAIT_MAGIC_0;

    decoder->calculated_crc =
        CRC16_CCITT_FALSE_INITIAL;
}

void protocol_frame_decoder_reset(
    ProtocolFrameDecoder *decoder
)
{
    if (decoder == NULL)
    {
        return;
    }

    /*
     * Reset stream state while preserving diagnostics.
     */
    decoder->state =
        PROTOCOL_DECODER_WAIT_MAGIC_0;

    decoder->payload_index = 0U;
    decoder->received_crc = 0U;
    decoder->calculated_crc =
        CRC16_CCITT_FALSE_INITIAL;
}

bool protocol_frame_decoder_feed_byte(
    ProtocolFrameDecoder *decoder,
    uint8_t byte,
    const ProtocolFrame **frame
)
{
    if ((decoder == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    *frame = NULL;

    switch (decoder->state)
    {
        case PROTOCOL_DECODER_WAIT_MAGIC_0:
            if (byte == PROTOCOL_FRAME_MAGIC_0)
            {
                decoder->state =
                    PROTOCOL_DECODER_WAIT_MAGIC_1;
            }
            break;

        case PROTOCOL_DECODER_WAIT_MAGIC_1:
            if (byte == PROTOCOL_FRAME_MAGIC_1)
            {
                protocol_frame_decoder_begin_frame(
                    decoder
                );
            }
            else if (byte != PROTOCOL_FRAME_MAGIC_0)
            {
                decoder->state =
                    PROTOCOL_DECODER_WAIT_MAGIC_0;
            }
            break;

        case PROTOCOL_DECODER_READ_VERSION:
            if (byte != PROTOCOL_FRAME_VERSION)
            {
                decoder->format_error_count++;

                protocol_frame_decoder_reset_search(
                    decoder,
                    byte
                );
                break;
            }

            decoder->frame.version = byte;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            decoder->state =
                PROTOCOL_DECODER_READ_TYPE;
            break;

        case PROTOCOL_DECODER_READ_TYPE:
            decoder->frame.type = byte;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            decoder->state =
                PROTOCOL_DECODER_READ_SEQUENCE_HIGH;
            break;

        case PROTOCOL_DECODER_READ_SEQUENCE_HIGH:
            decoder->frame.sequence =
                (uint16_t)byte << 8U;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            decoder->state =
                PROTOCOL_DECODER_READ_SEQUENCE_LOW;
            break;

        case PROTOCOL_DECODER_READ_SEQUENCE_LOW:
            decoder->frame.sequence |= byte;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            decoder->state =
                PROTOCOL_DECODER_READ_LENGTH_HIGH;
            break;

        case PROTOCOL_DECODER_READ_LENGTH_HIGH:
            decoder->frame.payload_length =
                (uint16_t)byte << 8U;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            decoder->state =
                PROTOCOL_DECODER_READ_LENGTH_LOW;
            break;

        case PROTOCOL_DECODER_READ_LENGTH_LOW:
            decoder->frame.payload_length |= byte;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            if (decoder->frame.payload_length >
                PROTOCOL_FRAME_MAX_PAYLOAD_SIZE)
            {
                decoder->format_error_count++;

                protocol_frame_decoder_reset_search(
                    decoder,
                    byte
                );
            }
            else if (decoder->frame.payload_length == 0U)
            {
                decoder->state =
                    PROTOCOL_DECODER_READ_CRC_HIGH;
            }
            else
            {
                decoder->payload_index = 0U;
                decoder->state =
                    PROTOCOL_DECODER_READ_PAYLOAD;
            }
            break;

        case PROTOCOL_DECODER_READ_PAYLOAD:
            decoder->frame.payload[
                decoder->payload_index
            ] = byte;

            decoder->payload_index++;

            protocol_frame_decoder_update_crc(
                decoder,
                byte
            );

            if (decoder->payload_index >=
                decoder->frame.payload_length)
            {
                decoder->state =
                    PROTOCOL_DECODER_READ_CRC_HIGH;
            }
            break;

        case PROTOCOL_DECODER_READ_CRC_HIGH:
            decoder->received_crc =
                (uint16_t)byte << 8U;

            decoder->state =
                PROTOCOL_DECODER_READ_CRC_LOW;
            break;

        case PROTOCOL_DECODER_READ_CRC_LOW:
            decoder->received_crc |= byte;

            if (decoder->received_crc !=
                decoder->calculated_crc)
            {
                decoder->crc_error_count++;

                protocol_frame_decoder_reset_search(
                    decoder,
                    byte
                );
                break;
            }

            decoder->valid_frame_count++;
            *frame = &decoder->frame;

            decoder->state =
                PROTOCOL_DECODER_WAIT_MAGIC_0;

            decoder->payload_index = 0U;
            decoder->received_crc = 0U;
            decoder->calculated_crc =
                CRC16_CCITT_FALSE_INITIAL;

            return true;

        default:
            protocol_frame_decoder_reset(decoder);
            break;
    }

    return false;
}
