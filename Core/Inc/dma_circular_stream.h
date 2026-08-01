/*
 * dma_circular_stream.h
 *
 *  Created on: 1 Aug 2026
 *      Author: Kto-to
 */
#ifndef DMA_CIRCULAR_STREAM_H
#define DMA_CIRCULAR_STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t mask;

    volatile uint32_t produced;
    uint32_t consumed;
    uint32_t old_position;
    uint32_t overflow_count;
} DmaCircularStream;

bool dma_circular_stream_init(
    DmaCircularStream *stream,
    uint8_t *buffer,
    uint32_t capacity
);

uint32_t dma_circular_stream_publish_position(
    DmaCircularStream *stream,
    uint32_t position
);

uint32_t dma_circular_stream_get_produced(
    const DmaCircularStream *stream
);

#endif
