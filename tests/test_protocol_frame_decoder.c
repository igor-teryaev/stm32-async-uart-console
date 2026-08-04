#include "protocol_frame_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t known_frame[] = {
    0xA5U, 0x5AU,
    0x01U,
    0x01U,
    0x12U, 0x34U,
    0x00U, 0x03U,
    0x10U, 0x20U, 0x30U,
    0x22U, 0x3DU
};

static void assert_known_frame(
    const ProtocolFrame *frame
)
{
    static const uint8_t expected_payload[] = {
        0x10U, 0x20U, 0x30U
    };

    assert(frame != NULL);
    assert(frame->version == PROTOCOL_FRAME_VERSION);
    assert(frame->type ==
           PROTOCOL_FRAME_TYPE_COMMAND);
    assert(frame->sequence == 0x1234U);
    assert(frame->payload_length ==
           sizeof(expected_payload));

    assert(memcmp(
        frame->payload,
        expected_payload,
        sizeof(expected_payload)
    ) == 0);
}

static void test_known_frame_byte_by_byte(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U;
         i < sizeof(known_frame);
         i++)
    {
        bool complete =
            protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            );

        if (i < (sizeof(known_frame) - 1U))
        {
            assert(!complete);
            assert(frame == NULL);
        }
        else
        {
            assert(complete);
            assert_known_frame(frame);
        }
    }

    assert(decoder.valid_frame_count == 1U);
    assert(decoder.crc_error_count == 0U);
    assert(decoder.format_error_count == 0U);
}

static void test_noise_and_overlapping_magic(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    static const uint8_t prefix[] = {
        0x00U, 0x7FU, 0xA5U
    };

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < sizeof(prefix); i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            prefix[i],
            &frame
        ));
    }

    /*
     * Stream now contains:
     * 00 7F A5 A5 5A ...
     *
     * The second A5 must become the new MAGIC_0.
     */
    for (size_t i = 0U;
         i < sizeof(known_frame);
         i++)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            ))
        {
            assert_known_frame(frame);
        }
    }

    assert(decoder.valid_frame_count == 1U);
}

static void test_frame_split_between_blocks(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;
    const size_t split = 7U;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < split; i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            known_frame[i],
            &frame
        ));
    }

    assert(decoder.valid_frame_count == 0U);

    for (size_t i = split;
         i < sizeof(known_frame);
         i++)
    {
        bool complete =
            protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            );

        if (complete)
        {
            assert_known_frame(frame);
        }
    }

    assert(decoder.valid_frame_count == 1U);
}

static void test_multiple_frames_in_one_block(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;
    uint32_t completed = 0U;

    protocol_frame_decoder_init(&decoder);

    for (uint32_t repetition = 0U;
         repetition < 2U;
         repetition++)
    {
        for (size_t i = 0U;
             i < sizeof(known_frame);
             i++)
        {
            if (protocol_frame_decoder_feed_byte(
                    &decoder,
                    known_frame[i],
                    &frame
                ))
            {
                assert_known_frame(frame);
                completed++;
            }
        }
    }

    assert(completed == 2U);
    assert(decoder.valid_frame_count == 2U);
}

static void test_crc_error_then_recovery(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;
    uint8_t corrupted[sizeof(known_frame)];
    uint32_t completed = 0U;

    memcpy(
        corrupted,
        known_frame,
        sizeof(corrupted)
    );

    corrupted[8] ^= 0x01U;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < sizeof(corrupted); i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            corrupted[i],
            &frame
        ));
    }

    assert(decoder.crc_error_count == 1U);
    assert(decoder.valid_frame_count == 0U);

    for (size_t i = 0U;
         i < sizeof(known_frame);
         i++)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            ))
        {
            assert_known_frame(frame);
            completed++;
        }
    }

    assert(completed == 1U);
    assert(decoder.valid_frame_count == 1U);
}

static void test_invalid_version_then_recovery(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    static const uint8_t invalid_prefix[] = {
        PROTOCOL_FRAME_MAGIC_0,
        PROTOCOL_FRAME_MAGIC_1,
        0x02U
    };

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U;
         i < sizeof(invalid_prefix);
         i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            invalid_prefix[i],
            &frame
        ));
    }

    assert(decoder.format_error_count == 1U);

    for (size_t i = 0U;
         i < sizeof(known_frame);
         i++)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            ))
        {
            assert_known_frame(frame);
        }
    }

    assert(decoder.valid_frame_count == 1U);
}

static void test_oversized_length_then_recovery(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    static const uint8_t invalid_header[] = {
        0xA5U, 0x5AU,
        0x01U,
        0x01U,
        0x00U, 0x01U,
        0x00U, 0x41U
    };

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U;
         i < sizeof(invalid_header);
         i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            invalid_header[i],
            &frame
        ));
    }

    assert(decoder.format_error_count == 1U);

    for (size_t i = 0U;
         i < sizeof(known_frame);
         i++)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                known_frame[i],
                &frame
            ))
        {
            assert_known_frame(frame);
        }
    }

    assert(decoder.valid_frame_count == 1U);
}

static void test_reset_preserves_diagnostics(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;
    uint8_t corrupted[sizeof(known_frame)];

    memcpy(
        corrupted,
        known_frame,
        sizeof(corrupted)
    );

    corrupted[8] ^= 0x01U;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < sizeof(corrupted); i++)
    {
        assert(!protocol_frame_decoder_feed_byte(
            &decoder,
            corrupted[i],
            &frame
        ));
    }

    assert(decoder.crc_error_count == 1U);

    protocol_frame_decoder_reset(&decoder);

    assert(decoder.state ==
           PROTOCOL_DECODER_WAIT_MAGIC_0);
    assert(decoder.crc_error_count == 1U);
    assert(decoder.valid_frame_count == 0U);
}

static void test_invalid_arguments(void)
{
    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    protocol_frame_decoder_init(NULL);
    protocol_frame_decoder_reset(NULL);

    protocol_frame_decoder_init(&decoder);

    assert(!protocol_frame_decoder_feed_byte(
        NULL,
        0U,
        &frame
    ));

    assert(!protocol_frame_decoder_feed_byte(
        &decoder,
        0U,
        NULL
    ));
}

static void test_zero_length_payload(void)
{
    static const uint8_t ack_frame[] = {
        0xA5U, 0x5AU,
        0x01U,
        0x03U,
        0x00U, 0x01U,
        0x00U, 0x00U,
        0x92U, 0x52U
    };

    ProtocolFrameDecoder decoder;
    const ProtocolFrame *frame = NULL;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U;
         i < sizeof(ack_frame);
         i++)
    {
        bool complete =
            protocol_frame_decoder_feed_byte(
                &decoder,
                ack_frame[i],
                &frame
            );

        if (i < (sizeof(ack_frame) - 1U))
        {
            assert(!complete);
        }
        else
        {
            assert(complete);
            assert(frame != NULL);
            assert(frame->version ==
                   PROTOCOL_FRAME_VERSION);
            assert(frame->type ==
                   PROTOCOL_FRAME_TYPE_ACK);
            assert(frame->sequence == 1U);
            assert(frame->payload_length == 0U);
        }
    }

    assert(decoder.valid_frame_count == 1U);
    assert(decoder.crc_error_count == 0U);
}

int main(void)
{
    test_known_frame_byte_by_byte();
    test_noise_and_overlapping_magic();
    test_frame_split_between_blocks();
    test_multiple_frames_in_one_block();
    test_crc_error_then_recovery();
    test_invalid_version_then_recovery();
    test_oversized_length_then_recovery();
    test_reset_preserves_diagnostics();
    test_invalid_arguments();
    test_zero_length_payload();
    puts("All protocol frame decoder tests passed.");
    return 0;
}
