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

