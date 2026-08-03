#ifndef UART_TX_ADAPTER_H
#define UART_TX_ADAPTER_H

#include "spsc_byte_queue.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    UART_HandleTypeDef *huart;
    SpscByteQueue queue;

    volatile bool active;
    volatile size_t active_length;
    volatile uint32_t error_count;
} UartTxAdapter;

bool uart_tx_adapter_init(
    UartTxAdapter *adapter,
    UART_HandleTypeDef *huart,
    uint8_t *buffer,
    uint32_t capacity
);

bool uart_tx_adapter_write(
    UartTxAdapter *adapter,
    const uint8_t *data,
    size_t length
);

void uart_tx_adapter_poll(
    UartTxAdapter *adapter
);

void uart_tx_adapter_handle_tx_complete(
    UartTxAdapter *adapter,
    UART_HandleTypeDef *huart
);

uint32_t uart_tx_adapter_get_error_count(
    const UartTxAdapter *adapter
);

#endif
