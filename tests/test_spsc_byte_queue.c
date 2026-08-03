#include "spsc_byte_queue.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_CAPACITY 8U

static void test_initialization(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];

    assert(!spsc_byte_queue_init(
        NULL, buffer, TEST_CAPACITY));

    assert(!spsc_byte_queue_init(
        &queue, NULL, TEST_CAPACITY));

    assert(!spsc_byte_queue_init(
        &queue, buffer, 1U));

    assert(!spsc_byte_queue_init(
        &queue, buffer, 7U));

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_get_used(&queue) == 0U);
    assert(spsc_byte_queue_get_free(&queue) ==
           TEST_CAPACITY);
}

static void test_write_and_peek(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t input[] = {'A', 'B', 'C'};
    const uint8_t *data = NULL;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_write(
        &queue, input, sizeof(input)));

    assert(spsc_byte_queue_get_used(&queue) == 3U);
    assert(spsc_byte_queue_get_free(&queue) == 5U);

    assert(spsc_byte_queue_peek(
        &queue, &data) == 3U);

    assert(data == &buffer[0]);
    assert(memcmp(data, input, sizeof(input)) == 0);
}

static void test_full_capacity_is_usable(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t input[TEST_CAPACITY] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U
    };
    const uint8_t extra = 8U;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_write(
        &queue, input, sizeof(input)));

    assert(spsc_byte_queue_get_used(&queue) ==
           TEST_CAPACITY);
    assert(spsc_byte_queue_get_free(&queue) == 0U);

    assert(!spsc_byte_queue_write(
        &queue, &extra, 1U));

    assert(spsc_byte_queue_get_used(&queue) ==
           TEST_CAPACITY);
}

static void test_consume(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t input[] = {
        10U, 11U, 12U, 13U
    };
    const uint8_t *data = NULL;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_write(
        &queue, input, sizeof(input)));

    assert(spsc_byte_queue_consume(&queue, 2U));

    assert(spsc_byte_queue_get_used(&queue) == 2U);

    assert(spsc_byte_queue_peek(
        &queue, &data) == 2U);

    assert(data[0] == 12U);
    assert(data[1] == 13U);

    assert(!spsc_byte_queue_consume(&queue, 3U));
    assert(spsc_byte_queue_get_used(&queue) == 2U);
}

static void test_wrap_uses_two_contiguous_blocks(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t prefix[] = {
        0U, 1U, 2U, 3U, 4U, 5U
    };
    const uint8_t wrapped[] = {
        'A', 'B', 'C', 'D'
    };
    const uint8_t *data = NULL;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    /*
     * Move both absolute counters to 6 while leaving
     * the queue empty.
     */
    assert(spsc_byte_queue_write(
        &queue, prefix, sizeof(prefix)));

    assert(spsc_byte_queue_consume(
        &queue, sizeof(prefix)));

    assert(queue.head == 6U);
    assert(queue.tail == 6U);

    assert(spsc_byte_queue_write(
        &queue, wrapped, sizeof(wrapped)));

    assert(queue.head == 10U);
    assert(queue.tail == 6U);

    /* First contiguous block: indexes 6 and 7. */
    assert(spsc_byte_queue_peek(
        &queue, &data) == 2U);

    assert(data == &buffer[6]);
    assert(data[0] == 'A');
    assert(data[1] == 'B');

    assert(spsc_byte_queue_consume(&queue, 2U));

    /* Second contiguous block: indexes 0 and 1. */
    assert(spsc_byte_queue_peek(
        &queue, &data) == 2U);

    assert(data == &buffer[0]);
    assert(data[0] == 'C');
    assert(data[1] == 'D');

    assert(spsc_byte_queue_consume(&queue, 2U));
    assert(spsc_byte_queue_get_used(&queue) == 0U);
}

static void test_failed_write_changes_nothing(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t first[] = {
        1U, 2U, 3U, 4U, 5U, 6U
    };
    const uint8_t too_large[] = {
        7U, 8U, 9U
    };
    uint32_t old_head;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_write(
        &queue, first, sizeof(first)));

    old_head = queue.head;

    assert(!spsc_byte_queue_write(
        &queue, too_large, sizeof(too_large)));

    assert(queue.head == old_head);
    assert(spsc_byte_queue_get_used(&queue) ==
           sizeof(first));
}

static void test_empty_and_invalid_arguments(void)
{
    SpscByteQueue queue;
    uint8_t buffer[TEST_CAPACITY];
    const uint8_t *data = (const uint8_t *)1;

    assert(spsc_byte_queue_init(
        &queue, buffer, TEST_CAPACITY));

    assert(spsc_byte_queue_write(
        &queue, NULL, 0U));

    assert(!spsc_byte_queue_write(
        &queue, NULL, 1U));

    assert(spsc_byte_queue_peek(
        &queue, &data) == 0U);

    assert(data == NULL);

    assert(spsc_byte_queue_consume(&queue, 0U));
    assert(!spsc_byte_queue_consume(&queue, 1U));
}

int main(void)
{
    test_initialization();
    test_write_and_peek();
    test_full_capacity_is_usable();
    test_consume();
    test_wrap_uses_two_contiguous_blocks();
    test_failed_write_changes_nothing();
    test_empty_and_invalid_arguments();

    puts("All SPSC byte queue tests passed.");
    return 0;
}
