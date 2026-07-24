/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED
} ButtonEvent;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BUTTON_DEBOUNCE_MS 30U
#define UART_RX_BUFFER_SIZE 8U
#define UART_TX_BUFFER_SIZE 128U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define COMMAND_BUFFER_SIZE 32U
#define STATUS_RESPONSE_SIZE 96U

static volatile uint32_t raw_edge_count = 0;
static volatile uint32_t last_edge_time = 0;
static volatile bool debounce_pending = false;
static const uint8_t button_message[] = "BUTTON PRESSED\r\n";
static bool stable_pressed = false;

static uint8_t uart_rx_byte;
static volatile uint32_t uart_rx_count = 0;
static uint32_t pending_button_messages = 0;

//кільцевий буфер для прийома
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t uart_rx_head = 0;
static volatile uint8_t uart_rx_tail = 0;
static volatile uint32_t uart_rx_overflow_count = 0;

//кільцевий буфер для передачі
static uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint8_t uart_tx_head = 0;
static volatile uint8_t uart_tx_tail = 0;
static volatile bool uart_tx_active = false;
static volatile uint32_t uart_tx_overflow_count = 0;
static volatile uint32_t uart_tx_error_count = 0;

//обробка ORE (помилка переповнення під час запису)
static volatile uint32_t uart_rx_error_count = 0;
static volatile uint32_t uart_rx_last_error = HAL_UART_ERROR_NONE;
static volatile uint32_t uart_rx_restart_error_count = 0;

static volatile uint32_t uart_rx_discarded_error_byte_count = 0;

//передача строки/команди через UART

static char command_buffer[COMMAND_BUFFER_SIZE];
static size_t command_length = 0;
static bool command_discarding = false;

static volatile uint32_t command_count = 0;
static volatile uint32_t command_overflow_count = 0;

static const uint8_t response_ok[] = "OK\r\n";
static const uint8_t response_unknown[] = "ERROR: UNKNOWN COMMAND\r\n";

static const uint8_t *pending_response = NULL;
static size_t pending_response_length = 0;



static uint8_t response_status[STATUS_RESPONSE_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static ButtonEvent button_debounce_poll(void);
static bool uart_rx_push_from_isr(uint8_t byte);
static bool uart_rx_pop(uint8_t *byte);
static bool uart_tx_write(const uint8_t *data, size_t length);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

//для передачі строки/команди через UART
static bool command_feed_byte(uint8_t byte);
static void command_process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  if (HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1U) != HAL_OK)
  {
      Error_Handler();
  }

  stable_pressed =
      HAL_GPIO_ReadPin(
          USER_BUTTON_GPIO_Port,
          USER_BUTTON_Pin
      ) == GPIO_PIN_RESET;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  ButtonEvent button_event = button_debounce_poll();

	  if (button_event == BUTTON_EVENT_PRESSED)
	  {
	      HAL_GPIO_TogglePin(
	          USER_LED_GPIO_Port,
	          USER_LED_Pin
	      );

	      pending_button_messages++;
	  }

	  if (pending_response != NULL)
	  {
	      if (uart_tx_write(
	              pending_response,
	              pending_response_length
	          ))
	      {
	          pending_response = NULL;
	          pending_response_length = 0;
	      }
	  }
	  else if (pending_button_messages > 0U)
	  {
	      if (uart_tx_write(
	              button_message,
	              sizeof(button_message) - 1U
	          ))
	      {
	          pending_button_messages--;
	      }
	  }
	  else
	  {
	      uint8_t received_byte;

	      if (uart_rx_pop(&received_byte))
	      {
	          if (command_feed_byte(received_byte))
	          {
	              command_count++;
	              command_process();
	          }
	      }
	  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

//формування команд із потоку байтів, текстовий парсер
static bool command_feed_byte(uint8_t byte)
{
    if ((byte == '\r') || (byte == '\n'))
    {
        if (command_discarding)
        {
            command_discarding = false;
            command_length = 0;
            return false;
        }

        if (command_length == 0U)
        {
            return false;
        }

        command_buffer[command_length] = '\0';
        command_length = 0;

        return true;
    }

    if (command_discarding)
    {
        return false;
    }

    if (command_length >= (COMMAND_BUFFER_SIZE - 1U))
    {
        command_overflow_count++;
        command_length = 0;
        command_discarding = true;

        return false;
    }

    command_buffer[command_length] = (char)byte;
    command_length++;

    return false;
}

//обробка отриманих з UART команд
static void command_process(void)
{
    if (strcmp(command_buffer, "LED ON") == 0)
    {
        HAL_GPIO_WritePin(
            USER_LED_GPIO_Port,
            USER_LED_Pin,
            GPIO_PIN_SET
        );

        pending_response = response_ok;
        pending_response_length = sizeof(response_ok) - 1U;
    }
    else if (strcmp(command_buffer, "LED OFF") == 0)
    {
        HAL_GPIO_WritePin(
            USER_LED_GPIO_Port,
            USER_LED_Pin,
            GPIO_PIN_RESET
        );

        pending_response = response_ok;
        pending_response_length = sizeof(response_ok) - 1U;
    }
    else if (strcmp(command_buffer, "LED TOGGLE") == 0)
    {
        HAL_GPIO_TogglePin(
            USER_LED_GPIO_Port,
            USER_LED_Pin
        );

        pending_response = response_ok;
        pending_response_length = sizeof(response_ok) - 1U;
    }
    else if (strcmp(command_buffer, "STATUS") == 0)
    {
    	uint32_t rx_count_snapshot;
    	uint32_t rx_overflow_snapshot;
    	uint32_t uart_error_snapshot;
    	uint32_t command_overflow_snapshot;

    	uint32_t primask = __get_PRIMASK();
    	__disable_irq();

			rx_count_snapshot = uart_rx_count;
			rx_overflow_snapshot = uart_rx_overflow_count;
			uart_error_snapshot = uart_rx_error_count;
			command_overflow_snapshot = command_overflow_count;

    	__set_PRIMASK(primask);

    	int length = snprintf(
    	    (char *)response_status,
    	    sizeof(response_status),
    	    "RX=%" PRIu32
    	    " RX_OVF=%" PRIu32
    	    " UART_ERR=%" PRIu32
    	    " CMD_OVF=%" PRIu32
    	    "\r\n",
    	    rx_count_snapshot,
    	    rx_overflow_snapshot,
    	    uart_error_snapshot,
    	    command_overflow_snapshot
    	);

        if (length < 0)
        {
            pending_response = response_unknown;
            pending_response_length =
                sizeof(response_unknown) - 1U;
        }
        else
        {
            size_t actual_length = (size_t)length;

            if (actual_length >= sizeof(response_status))
            {
                actual_length = sizeof(response_status) - 1U;
            }

            pending_response = response_status;
            pending_response_length = actual_length;
        }
    }
    else
    {
        pending_response = response_unknown;
        pending_response_length =
            sizeof(response_unknown) - 1U;
    }
}

//Повторно запускаємо RX лише для ORE, тому що саме ця помилка викликає UART_EndRxTransfer()
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uint32_t error = huart->ErrorCode;

        uart_rx_last_error = error;
        uart_rx_error_count++;

        /*
         * Якщо HAL завершив поточний RX transfer,
         * запускаємо приймання знову.
         */
        if (huart->RxState == HAL_UART_STATE_READY)
        {
            if (HAL_UART_Receive_IT(
                    huart,
                    &uart_rx_byte,
                    1U
                ) != HAL_OK)
            {
                uart_rx_restart_error_count++;
            }
        }
    }
}

static bool uart_tx_write(
    const uint8_t *data,
    size_t length
)
{
    if (length == 0U)
    {
        return true;
    }

    if (data == NULL)
    {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    size_t used =
        (uart_tx_head - uart_tx_tail)
        & (UART_TX_BUFFER_SIZE - 1U);

    size_t free_space =
        (UART_TX_BUFFER_SIZE - 1U) - used;

    if (length > free_space)
    {
        __set_PRIMASK(primask);
        return false;
    }

    uint8_t old_head = uart_tx_head;
    uint8_t new_head = uart_tx_head;

    for (size_t i = 0; i < length; i++)
    {
        uart_tx_buffer[new_head] = data[i];

        new_head =
            (new_head + 1U)
            & (UART_TX_BUFFER_SIZE - 1U);
    }

    /* Публікуємо всі байти одночасно. */
    uart_tx_head = new_head;

    if (!uart_tx_active)
    {
        uart_tx_active = true;

        if (HAL_UART_Transmit_IT(
                &huart2,
                &uart_tx_buffer[uart_tx_tail],
                1U
            ) != HAL_OK)
        {
            uart_tx_active = false;
            uart_tx_head = old_head;
            uart_tx_error_count++;

            __set_PRIMASK(primask);
            return false;
        }
    }

    __set_PRIMASK(primask);
    return true;
}

//TX Callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart_tx_tail =
            (uart_tx_tail + 1U) & (UART_TX_BUFFER_SIZE - 1U);

        if (uart_tx_tail != uart_tx_head)
        {
            if (HAL_UART_Transmit_IT(
                    &huart2,
                    &uart_tx_buffer[uart_tx_tail],
                    1U
                ) != HAL_OK)
            {
                uart_tx_active = false;
                uart_tx_error_count++;
            }
        }
        else
        {
            uart_tx_active = false;
        }
    }
}



static bool uart_rx_push_from_isr(uint8_t byte)
{

    // 1. Обчислити next_head
	uint8_t next_head = (uart_rx_head+1U)&(UART_RX_BUFFER_SIZE-1U);
    // 2. Перевірити стан full
    if((next_head)==uart_rx_tail)
    // 3. При overflow збільшити overflow_count і повернути false
    {
        uart_rx_overflow_count++;
        return false;
    }
    // 4. Записати byte
    uart_rx_buffer[uart_rx_head] = byte;
    // 5. Оновити head
    uart_rx_head = next_head;
    // 6. Повернути true
    return true;
}

static bool uart_rx_pop(uint8_t *byte)
{
    // Вважаємо, що byte != NULL

    // 1. Перевірити empty: head == tail
    if(uart_rx_head == uart_rx_tail) return false;
    // 2. Якщо empty — повернути false
    //if(head == tail) return false; це воно і є
    // 3. Записати buffer[tail] за адресою byte
    *byte = uart_rx_buffer[uart_rx_tail];
    // 4. Пересунути tail із wrap-around
    uart_rx_tail = (uart_rx_tail+1U)&(UART_RX_BUFFER_SIZE-1U);
    // 5. Повернути true
    return true;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_BUTTON_Pin) {
        last_edge_time = HAL_GetTick();
        raw_edge_count++;
        debounce_pending = true;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart_rx_count++;

        if (huart->ErrorCode == HAL_UART_ERROR_NONE)
        {
            (void)uart_rx_push_from_isr(uart_rx_byte);

            if (HAL_UART_Receive_IT(
                    huart,
                    &uart_rx_byte,
                    1U
                ) != HAL_OK)
            {
                uart_rx_restart_error_count++;
            }
        }
        else
        {
            /*
             * Байт міг бути пошкоджений.
             * Повторний RX запустить ErrorCallback.
             */
            uart_rx_discarded_error_byte_count++;
        }
    }
}

static ButtonEvent button_debounce_poll(void)
{
    uint32_t edge_count_snapshot;
    uint32_t edge_time_snapshot;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (!debounce_pending) {
        __set_PRIMASK(primask);
        return BUTTON_EVENT_NONE;
    }

    edge_count_snapshot = raw_edge_count;
    edge_time_snapshot = last_edge_time;

    __set_PRIMASK(primask);

    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - edge_time_snapshot) < BUTTON_DEBOUNCE_MS) {
        return BUTTON_EVENT_NONE;
    }

    GPIO_PinState pin_state =
        HAL_GPIO_ReadPin(
            USER_BUTTON_GPIO_Port,
            USER_BUTTON_Pin
        );

    /*
     * Перевіряємо, чи ISR не зафіксував новий edge,
     * поки main перевіряв час і читав GPIO.
     */
    primask = __get_PRIMASK();
    __disable_irq();

    if (raw_edge_count != edge_count_snapshot) {
        __set_PRIMASK(primask);
        return BUTTON_EVENT_NONE;
    }

    debounce_pending = false;

    __set_PRIMASK(primask);

    bool raw_pressed = (pin_state == GPIO_PIN_RESET);

    if (raw_pressed == stable_pressed) {
        return BUTTON_EVENT_NONE;
    }

    stable_pressed = raw_pressed;

    return stable_pressed
        ? BUTTON_EVENT_PRESSED
        : BUTTON_EVENT_RELEASED;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


//постановка байту в TX-чергу
/*static bool uart_tx_enqueue(uint8_t byte)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint8_t next_head =
        (uart_tx_head + 1U) & (UART_TX_BUFFER_SIZE - 1U);

    if (next_head == uart_tx_tail)
    {
        uart_tx_overflow_count++;
        __set_PRIMASK(primask);
        return false;
    }

    uint8_t old_head = uart_tx_head;

    uart_tx_buffer[uart_tx_head] = byte;
    uart_tx_head = next_head;

    if (!uart_tx_active)
    {
        uart_tx_active = true;

        HAL_StatusTypeDef status = HAL_UART_Transmit_IT(
            &huart2,
            &uart_tx_buffer[uart_tx_tail],
            1U
        );

        if (status != HAL_OK)
        {
            uart_tx_active = false;
            uart_tx_head = old_head;
            uart_tx_error_count++;

            __set_PRIMASK(primask);
            return false;
        }
    }

    __set_PRIMASK(primask);
    return true;
}*/
