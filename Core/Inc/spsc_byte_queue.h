#ifndef SPSC_BYTE_QUEUE_H
#define SPSC_BYTE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t mask;

    volatile uint32_t head;
    volatile uint32_t tail;
} SpscByteQueue;

bool spsc_byte_queue_init(
    SpscByteQueue *queue,
    uint8_t *buffer,
    uint32_t capacity
);

bool spsc_byte_queue_write(
    SpscByteQueue *queue,
    const uint8_t *data,
    size_t length
);

size_t spsc_byte_queue_peek(
    const SpscByteQueue *queue,
    const uint8_t **data
);

bool spsc_byte_queue_consume(
    SpscByteQueue *queue,
    size_t length
);

size_t spsc_byte_queue_get_used(
    const SpscByteQueue *queue
);

size_t spsc_byte_queue_get_free(
    const SpscByteQueue *queue
);

#endif
