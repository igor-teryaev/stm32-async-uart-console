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

int main(void)
{
    test_initialization();
    test_ht_tc_and_idle_sequence();
    test_position_wrap();
    test_invalid_position_is_ignored();

    puts("All DMA circular stream tests passed.");
    return 0;
}
