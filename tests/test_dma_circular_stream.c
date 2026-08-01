#include "dma_circular_stream.h"

#include <assert.h>
#include <stdio.h>

#define TEST_CAPACITY 256U

static void test_initialization(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];

    assert(!dma_circular_stream_init(
        NULL, buffer, TEST_CAPACITY));

    assert(!dma_circular_stream_init(
        &stream, NULL, TEST_CAPACITY));

    assert(!dma_circular_stream_init(
        &stream, buffer, 1U));

    assert(!dma_circular_stream_init(
        &stream, buffer, 100U));

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_get_produced(&stream) == 0U
    );
}

static void test_ht_tc_and_idle_sequence(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    /* Half-transfer: physical position 128. */
    assert(
        dma_circular_stream_publish_position(
            &stream, 128U
        ) == 128U
    );

    /* Transfer-complete: 256 normalizes to 0. */
    assert(
        dma_circular_stream_publish_position(
            &stream, 256U
        ) == 128U
    );

    /* IDLE after another 20 bytes. */
    assert(
        dma_circular_stream_publish_position(
            &stream, 20U
        ) == 20U
    );

    assert(
        dma_circular_stream_get_produced(&stream) ==
        276U
    );
}

static void test_position_wrap(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_publish_position(
            &stream, 240U
        ) == 240U
    );

    assert(
        dma_circular_stream_publish_position(
            &stream, 20U
        ) == 36U
    );

    assert(
        dma_circular_stream_get_produced(&stream) ==
        276U
    );
}

static void test_invalid_position_is_ignored(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_publish_position(
            &stream, TEST_CAPACITY + 1U
        ) == 0U
    );

    assert(
        dma_circular_stream_get_produced(&stream) == 0U
    );
}

static void test_peek_and_consume(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t *data = NULL;
    bool data_lost = true;

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_publish_position(
            &stream, 100U
        ) == 100U
    );

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 100U
    );
    assert(data == &buffer[0]);
    assert(!data_lost);

    assert(dma_circular_stream_consume(
        &stream, 60U));

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 40U
    );
    assert(data == &buffer[60]);

    assert(dma_circular_stream_consume(
        &stream, 40U));

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 0U
    );
    assert(data == NULL);
}

static void test_contiguous_blocks_across_wrap(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t *data = NULL;
    bool data_lost = false;

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_publish_position(
            &stream, 240U
        ) == 240U
    );
    assert(dma_circular_stream_consume(
        &stream, 240U));

    assert(
        dma_circular_stream_publish_position(
            &stream, 20U
        ) == 36U
    );

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 16U
    );
    assert(data == &buffer[240]);
    assert(!data_lost);

    assert(dma_circular_stream_consume(
        &stream, 16U));

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 20U
    );
    assert(data == &buffer[0]);

    assert(dma_circular_stream_consume(
        &stream, 20U));
}

static void test_consume_rejects_excess_length(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t *data = NULL;
    bool data_lost = false;

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    assert(
        dma_circular_stream_publish_position(
            &stream, 20U
        ) == 20U
    );

    assert(!dma_circular_stream_consume(
        &stream, 200U));

    /* Failed consume must leave all 20 bytes available. */
    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 20U
    );
    assert(data == &buffer[0]);
}

static void test_overflow_keeps_newest_buffer(void)
{
    DmaCircularStream stream;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t *data = NULL;
    bool data_lost = false;

    assert(dma_circular_stream_init(
        &stream, buffer, TEST_CAPACITY));

    /* Simulate HT, TC, HT, TC and IDLE after 601 bytes. */
    assert(
        dma_circular_stream_publish_position(
            &stream, 128U
        ) == 128U
    );
    assert(
        dma_circular_stream_publish_position(
            &stream, 256U
        ) == 128U
    );
    assert(
        dma_circular_stream_publish_position(
            &stream, 128U
        ) == 128U
    );
    assert(
        dma_circular_stream_publish_position(
            &stream, 256U
        ) == 128U
    );
    assert(
        dma_circular_stream_publish_position(
            &stream, 89U
        ) == 89U
    );

    assert(
        dma_circular_stream_get_produced(&stream) ==
        601U
    );

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 167U
    );

    assert(data_lost);
    assert(data == &buffer[89]);

    assert(
        dma_circular_stream_get_overflow_count(
            &stream
        ) == 345U
    );

    assert(dma_circular_stream_consume(
        &stream, 167U));

    assert(
        dma_circular_stream_peek(
            &stream, &data, &data_lost
        ) == 89U
    );
    assert(!data_lost);
    assert(data == &buffer[0]);

    assert(dma_circular_stream_consume(
        &stream, 89U));
}

int main(void)
{
    test_initialization();
    test_ht_tc_and_idle_sequence();
    test_position_wrap();
    test_invalid_position_is_ignored();
    test_peek_and_consume();
    test_contiguous_blocks_across_wrap();
    test_consume_rejects_excess_length();
    test_overflow_keeps_newest_buffer();

    puts("All DMA circular stream tests passed.");
    return 0;
}
