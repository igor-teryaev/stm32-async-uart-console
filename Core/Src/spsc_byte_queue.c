#include "spsc_byte_queue.h"

#include <string.h>

bool spsc_byte_queue_init(
    SpscByteQueue *queue,
    uint8_t *buffer,
    uint32_t capacity
)
{
    if ((queue == NULL) || (buffer == NULL))
    {
        return false;
    }

    if ((capacity <= 1U) ||
        ((capacity & (capacity - 1U)) != 0U))
    {
        return false;
    }

    queue->buffer = buffer;
    queue->capacity = capacity;
    queue->mask = capacity - 1U;
    queue->head = 0U;
    queue->tail = 0U;

    return true;
}

size_t spsc_byte_queue_get_used(
    const SpscByteQueue *queue
)
{
    if (queue == NULL)
    {
        return 0U;
    }

    uint32_t used = queue->head - queue->tail;

    if (used > queue->capacity)
    {
        return 0U;
    }

    return (size_t)used;
}

size_t spsc_byte_queue_get_free(
    const SpscByteQueue *queue
)
{
    if (queue == NULL)
    {
        return 0U;
    }

    size_t used = spsc_byte_queue_get_used(queue);

    return (size_t)queue->capacity - used;
}

bool spsc_byte_queue_write(
    SpscByteQueue *queue,
    const uint8_t *data,
    size_t length
)
{
    if (queue == NULL)
    {
        return false;
    }

    if (length == 0U)
    {
        return true;
    }

    if (data == NULL)
    {
        return false;
    }

    uint32_t head_snapshot = queue->head;
    uint32_t tail_snapshot = queue->tail;
    uint32_t used = head_snapshot - tail_snapshot;

    if (used > queue->capacity)
    {
        return false;
    }

    size_t free_space =
        (size_t)queue->capacity - (size_t)used;

    if (length > free_space)
    {
        return false;
    }

    uint32_t head_index =
        head_snapshot & queue->mask;

    size_t first_length =
        (size_t)queue->capacity - (size_t)head_index;

    if (first_length > length)
    {
        first_length = length;
    }

    memcpy(
        &queue->buffer[head_index],
        data,
        first_length
    );

    size_t second_length = length - first_length;

    if (second_length > 0U)
    {
        memcpy(
            &queue->buffer[0],
            &data[first_length],
            second_length
        );
    }

    /* Publish the complete write in one step. */
    queue->head =
        head_snapshot + (uint32_t)length;

    return true;
}

size_t spsc_byte_queue_peek(
    const SpscByteQueue *queue,
    const uint8_t **data
)
{
    if ((queue == NULL) || (data == NULL))
    {
        return 0U;
    }

    *data = NULL;

    uint32_t head_snapshot = queue->head;
    uint32_t tail_snapshot = queue->tail;
    uint32_t used = head_snapshot - tail_snapshot;

    if ((used == 0U) || (used > queue->capacity))
    {
        return 0U;
    }

    uint32_t tail_index =
        tail_snapshot & queue->mask;

    uint32_t until_end =
        queue->capacity - tail_index;

    uint32_t contiguous_length =
        (used < until_end) ? used : until_end;

    *data = &queue->buffer[tail_index];

    return (size_t)contiguous_length;
}

bool spsc_byte_queue_consume(
    SpscByteQueue *queue,
    size_t length
)
{
    if (queue == NULL)
    {
        return false;
    }

    uint32_t head_snapshot = queue->head;
    uint32_t tail_snapshot = queue->tail;
    uint32_t used = head_snapshot - tail_snapshot;

    if ((used > queue->capacity) ||
        (length > (size_t)used))
    {
        return false;
    }

    queue->tail =
        tail_snapshot + (uint32_t)length;

    return true;
}
