# STM32 Async UART Console

Interrupt-driven UART command console with RX/TX ring buffers, error recovery, and debounced EXTI input on the STM32F446RE.

## Overview

This project implements a non-blocking command console for the NUCLEO-F446RE development board. USART2 receives and transmits data through interrupts, while the main loop handles command parsing, responses, and button events.

The firmware demonstrates how interrupt-driven peripheral handling can be separated from application-level processing without an RTOS.

## Hardware

- STMicroelectronics NUCLEO-F446RE
- STM32F446RE microcontroller
- ARM Cortex-M4 core
- On-board user LED (PA5)
- On-board user button (PC13)
- ST-LINK Virtual COM Port via USART2

## Features

- USART2 configured for 115200 baud, 8 data bits, no parity, and 1 stop bit
- Interrupt-driven, byte-by-byte UART reception
- Single-producer/single-consumer RX ring buffer
- Interrupt-driven UART transmission with a separate TX ring buffer
- Power-of-two buffer sizes and mask-based index wrapping
- RX overflow detection and accounting
- UART error accounting and receive recovery
- Line-oriented command parser with overlength-command handling
- Asynchronous command responses
- EXTI handling for both button edges
- Non-blocking software debounce using the HAL system tick
- Runtime status counters available through the UART console

## Supported commands

Commands are terminated by a newline.

| Command | Action |
|---|---|
| `LED ON` | Turns the user LED on |
| `LED OFF` | Turns the user LED off |
| `LED TOGGLE` | Toggles the user LED |
| `STATUS` | Reports receive, overflow, UART error, and command-overflow counters |

Unknown commands return:

```text
ERROR: UNKNOWN COMMAND
```

Successful LED commands return:

```text
OK
```

## Architecture

### Receive path

1. USART2 receives one byte using `HAL_UART_Receive_IT()`.
2. `HAL_UART_RxCpltCallback()` places the byte in the RX ring buffer.
3. Reception is immediately armed for the next byte.
4. The main loop removes queued bytes and passes them to the command parser.
5. Complete commands are processed outside interrupt context.

The interrupt callback is the only writer of the RX `head` index, and the main loop is the only writer of the RX `tail` index. This forms a single-producer/single-consumer queue.

### Transmit path

Application responses are copied into the TX ring buffer. If transmission is idle, the first byte is started with `HAL_UART_Transmit_IT()`. Each transmit-complete callback advances the queue and starts the next byte until the buffer is empty.

Interrupts are briefly masked while application code publishes data to the TX queue, preventing the transmit callback from observing a partially updated queue.

### Ring-buffer wrapping

Both UART buffer sizes are powers of two. Indices wrap using a bit mask:

```c
next_head = (head + 1U) & (BUFFER_SIZE - 1U);
```

One slot remains unused so that:

- `head == tail` means empty;
- `next_head == tail` means full.

### Button handling

PC13 is configured for EXTI interrupts on rising and falling edges. The interrupt callback records the latest edge and schedules debounce processing. The main loop waits for the input to remain stable for the debounce interval before reporting a press or release.

A confirmed button press toggles the user LED and queues this message:

```text
BUTTON PRESSED
```

### UART error recovery

The UART error callback records the HAL error flags. When HAL has returned the receiver to the ready state, interrupt-driven reception is armed again. Diagnostic counters make receive errors, restart failures, discarded bytes, and buffer overflows observable while debugging.

## Project structure

```text
Core/
|-- Inc/                         Application and generated headers
|-- Src/                         Application and generated sources
`-- Startup/                     STM32F446RE startup code
Drivers/
|-- CMSIS/                       ARM and STM32 device headers
`-- STM32F4xx_HAL_Driver/        STM32 HAL drivers
nucleo_f446re_lab01.ioc          STM32CubeMX configuration
STM32F446RETX_FLASH.ld           Flash linker script
STM32F446RETX_RAM.ld             RAM linker script
```

The current application logic, ring buffers, command parser, debounce state machine, and HAL callbacks are located in `Core/Src/main.c`.

## Build and run

1. Clone the repository.
2. Open STM32CubeIDE.
3. Select **File > Import > Existing Projects into Workspace**.
4. Select the cloned repository.
5. Build the project.
6. Connect the NUCLEO-F446RE board and flash the firmware.
7. Open the ST-LINK Virtual COM Port at `115200 8-N-1` with no flow control.
8. Send one of the supported commands followed by a newline.

## Current limitations

- UART reception and transmission are interrupt-driven per byte rather than DMA-based.
- RX and TX ring-buffer logic is currently contained in `main.c` rather than reusable modules.
- The command parser is application-specific and not yet separated from hardware control.
- No automated host-side unit tests are included.
- The project does not currently use an RTOS.

## Planned improvements

- Extract reusable RX/TX ring-buffer modules
- Add host-side unit tests for ring-buffer and command-parser logic
- Introduce UART DMA reception with idle-line detection
- Separate the command parser from GPIO application logic
- Add a documented telemetry protocol layer
- Evaluate FreeRTOS integration when concurrent application tasks require it

## Author

Igor Teryaev

GitHub: [igor-teryaev](https://github.com/igor-teryaev)
