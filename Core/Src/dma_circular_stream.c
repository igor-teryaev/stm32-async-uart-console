/*
 * dma_circular_stream.c
 *
 *  Created on: 1 Aug 2026
 *      Author: Kto-to
 */
#include "dma_circular_stream.h"

bool dma_circular_stream_init(
    DmaCircularStream *stream,
    uint8_t *buffer,
    uint32_t capacity
)
{
    if ((stream == NULL) || (buffer == NULL))
    {
        return false;
    }

    if ((capacity <= 1U) ||
        ((capacity & (capacity - 1U)) != 0U))
    {
        return false;
    }

    stream->buffer = buffer;
    stream->capacity = capacity;
    stream->mask = capacity - 1U;

    stream->produced = 0U;
    stream->consumed = 0U;
    stream->old_position = 0U;
    stream->overflow_count = 0U;

    return true;
}

uint32_t dma_circular_stream_publish_position(
    DmaCircularStream *stream,
    uint32_t position
)
{
    if (stream == NULL)
    {
        return 0U;
    }

    if (position > stream->capacity)
    {
        return 0U;
    }

    uint32_t new_position =
        position & stream->mask;

    uint32_t received =
        (new_position - stream->old_position) &
        stream->mask;

    stream->produced += received;
    stream->old_position = new_position;

    return received;
}

uint32_t dma_circular_stream_get_produced(
    const DmaCircularStream *stream
)
{
    if (stream == NULL)
    {
        return 0U;
    }

    return stream->produced;
}

size_t dma_circular_stream_peek(
    DmaCircularStream *stream,
    const uint8_t **data,
    bool *data_lost
)
{
    if ((stream == NULL) ||
        (data == NULL) ||
        (data_lost == NULL))
    {
        return 0U;
    }

    *data = NULL;
    *data_lost = false;

    uint32_t produced_snapshot = stream->produced;
    uint32_t pending =
        produced_snapshot - stream->consumed;

    if (pending == 0U)
    {
        return 0U;
    }

    if (pending > stream->capacity)
    {
        uint32_t lost =
            pending - stream->capacity;

        stream->overflow_count += lost;
        *data_lost = true;

        stream->consumed =
            produced_snapshot - stream->capacity;

        pending = stream->capacity;
    }

    uint32_t index =
        stream->consumed & stream->mask;

    uint32_t until_end =
        stream->capacity - index;

    uint32_t contiguous_length =
        (pending < until_end) ? pending : until_end;

    *data = &stream->buffer[index];

    return (size_t)contiguous_length;
}

bool dma_circular_stream_consume(
    DmaCircularStream *stream,
    size_t length
)
{
    if (stream == NULL)
    {
        return false;
    }

    uint32_t produced_snapshot = stream->produced;
    uint32_t available =
        produced_snapshot - stream->consumed;

    if (length > (size_t)available)
    {
        return false;
    }

    stream->consumed += (uint32_t)length;

    return true;
}

uint32_t dma_circular_stream_get_overflow_count(
    const DmaCircularStream *stream
)
{
    if (stream == NULL)
    {
        return 0U;
    }

    return stream->overflow_count;
}
