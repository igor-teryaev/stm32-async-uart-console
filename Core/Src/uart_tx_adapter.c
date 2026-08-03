#include "uart_tx_adapter.h"

static void uart_tx_adapter_start_next(
    UartTxAdapter *adapter
)
{
    const uint8_t *data = NULL;
    size_t length;
    uint32_t primask;

    if ((adapter == NULL) ||
        (adapter->huart == NULL))
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (adapter->active)
    {
        __set_PRIMASK(primask);
        return;
    }

    length = spsc_byte_queue_peek(
        &adapter->queue,
        &data
    );

    if (length == 0U)
    {
        __set_PRIMASK(primask);
        return;
    }

    adapter->active_length = length;
    adapter->active = true;

    if (HAL_UART_Transmit_IT(
            adapter->huart,
            (uint8_t *)data,
            (uint16_t)length
        ) != HAL_OK)
    {
        /*
         * The queue is not consumed. poll() can retry
         * the same contiguous block later.
         */
        adapter->active = false;
        adapter->active_length = 0U;
        adapter->error_count++;
    }

    __set_PRIMASK(primask);
}

bool uart_tx_adapter_init(
    UartTxAdapter *adapter,
    UART_HandleTypeDef *huart,
    uint8_t *buffer,
    uint32_t capacity
)
{
	/*queue може теоретично мати більшу місткість, але HAL_UART_Transmit_IT() приймає довжину як uint16_t.
	 *	Адаптер не повинен мовчки обрізати її приведенням типу.
	 */
    if ((adapter == NULL) ||
        (huart == NULL) ||
        (capacity > UINT16_MAX))
    {
        return false;
    }

    if (!spsc_byte_queue_init(
            &adapter->queue,
            buffer,
            capacity
        ))
    {
        return false;
    }

    adapter->huart = huart;
    adapter->active = false;
    adapter->active_length = 0U;
    adapter->error_count = 0U;

    return true;
}

bool uart_tx_adapter_write(
    UartTxAdapter *adapter,
    const uint8_t *data,
    size_t length
)
{
    if (adapter == NULL)
    {
        return false;
    }

    if (!spsc_byte_queue_write(
            &adapter->queue,
            data,
            length
        ))
    {
        return false;
    }

    uart_tx_adapter_start_next(adapter);
    return true;
}

void uart_tx_adapter_poll(
    UartTxAdapter *adapter
)
{
    uart_tx_adapter_start_next(adapter);
}

void uart_tx_adapter_handle_tx_complete(
    UartTxAdapter *adapter,
    UART_HandleTypeDef *huart
)
{
    size_t completed_length;

    if ((adapter == NULL) ||
        (huart != adapter->huart))
    {
        return;
    }

    if (!adapter->active ||
        (adapter->active_length == 0U))
    {
        adapter->error_count++;
        return;
    }

    completed_length = adapter->active_length;

    adapter->active_length = 0U;
    adapter->active = false;

    if (!spsc_byte_queue_consume(
            &adapter->queue,
            completed_length
        ))
    {
        adapter->error_count++;
        return;
    }

    uart_tx_adapter_start_next(adapter);
}

uint32_t uart_tx_adapter_get_error_count(
    const UartTxAdapter *adapter
)
{
    if (adapter == NULL)
    {
        return 0U;
    }

    return adapter->error_count;
}
