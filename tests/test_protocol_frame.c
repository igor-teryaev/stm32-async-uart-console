#include "protocol_frame.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_known_frame(void)
{
    static const uint8_t payload[] = {
        0x10U, 0x20U, 0x30U
    };

    static const uint8_t expected[] = {
        0xA5U, 0x5AU,
        0x01U,
        0x01U,
        0x12U, 0x34U,
        0x00U, 0x03U,
        0x10U, 0x20U, 0x30U,
		0x22U, 0x3DU
    };

    uint8_t output[PROTOCOL_FRAME_MAX_SIZE];
    size_t output_length = 0U;

    assert(protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        0x1234U,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        &output_length
    ));

    assert(output_length == sizeof(expected));
    assert(memcmp(
        output,
        expected,
        sizeof(expected)
    ) == 0);
}

static void test_empty_payload(void)
{
    uint8_t output[PROTOCOL_FRAME_MAX_SIZE];
    size_t output_length = 0U;

    assert(protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_ACK,
        0x0001U,
        NULL,
        0U,
        output,
        sizeof(output),
        &output_length
    ));

    assert(
        output_length ==
        PROTOCOL_FRAME_HEADER_SIZE +
        PROTOCOL_FRAME_CRC_SIZE
    );

    assert(output[0] == PROTOCOL_FRAME_MAGIC_0);
    assert(output[1] == PROTOCOL_FRAME_MAGIC_1);
    assert(output[2] == PROTOCOL_FRAME_VERSION);
    assert(output[3] == PROTOCOL_FRAME_TYPE_ACK);
    assert(output[4] == 0x00U);
    assert(output[5] == 0x01U);
    assert(output[6] == 0x00U);
    assert(output[7] == 0x00U);
}

static void test_maximum_payload(void)
{
    uint8_t payload[
        PROTOCOL_FRAME_MAX_PAYLOAD_SIZE
    ];

    uint8_t output[PROTOCOL_FRAME_MAX_SIZE];
    size_t output_length = 0U;

    for (size_t i = 0U; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)i;
    }

    assert(protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_TELEMETRY,
        0xFFFFU,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        &output_length
    ));

    assert(output_length ==
           PROTOCOL_FRAME_MAX_SIZE);

    assert(memcmp(
        &output[PROTOCOL_FRAME_HEADER_SIZE],
        payload,
        sizeof(payload)
    ) == 0);
}

static void test_insufficient_output_capacity(void)
{
    static const uint8_t payload[] = {
        0x01U, 0x02U, 0x03U
    };

    uint8_t output[12U];
    size_t output_length = 123U;

    assert(!protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        1U,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        &output_length
    ));

    assert(output_length == 0U);
}

static void test_rejects_oversized_payload(void)
{
	uint8_t payload[
	    PROTOCOL_FRAME_MAX_PAYLOAD_SIZE + 1U
	] = {0U};

    uint8_t output[PROTOCOL_FRAME_MAX_SIZE];
    size_t output_length = 123U;

    assert(!protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        1U,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        &output_length
    ));

    assert(output_length == 0U);
}

static void test_invalid_arguments(void)
{
    uint8_t output[PROTOCOL_FRAME_MAX_SIZE];
    size_t output_length = 123U;

    assert(!protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        1U,
        NULL,
        1U,
        output,
        sizeof(output),
        &output_length
    ));

    assert(output_length == 0U);

    assert(!protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        1U,
        NULL,
        0U,
        NULL,
        0U,
        &output_length
    ));

    assert(output_length == 0U);

    assert(!protocol_frame_encode(
        PROTOCOL_FRAME_TYPE_COMMAND,
        1U,
        NULL,
        0U,
        output,
        sizeof(output),
        NULL
    ));
}

int main(void)
{
    test_known_frame();
    test_empty_payload();
    test_maximum_payload();
    test_insufficient_output_capacity();
    test_rejects_oversized_payload();
    test_invalid_arguments();

    puts("All protocol frame encoder tests passed.");
    return 0;
}
