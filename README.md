# STM32 Async UART Console

Non-blocking UART command console using circular DMA reception, IDLE-line detection, interrupt-driven transmission, error recovery, and debounced EXTI input on the STM32F446RE.

## Overview

This project implements an asynchronous command console for the NUCLEO-F446RE development board.

USART2 reception uses a 256-byte circular DMA buffer with half-transfer, transfer-complete, and IDLE-line events. The main loop processes received data directly from the DMA buffer in contiguous zero-copy blocks.

Transmission uses a separate interrupt-driven ring buffer. Command parsing, hardware control, error recovery, and button debounce are performed outside interrupt context.

The project demonstrates a bare-metal producer-consumer architecture without an RTOS.

## Hardware

- STMicroelectronics NUCLEO-F446RE
- STM32F446RE microcontroller
- ARM Cortex-M4 core
- On-board user LED on PA5
- On-board user button on PC13
- ST-LINK Virtual COM Port through USART2

## UART configuration

- USART2
- 115200 baud
- 8 data bits
- No parity
- 1 stop bit
- No flow control
- RX: DMA1 Stream 5, circular mode
- TX: interrupt-driven

## Features

- Circular DMA reception using `HAL_UARTEx_ReceiveToIdle_DMA()`
- RX event accounting for half-transfer, transfer-complete, and IDLE events
- Absolute `produced` and `consumed` byte counters
- Power-of-two 256-byte DMA RX buffer
- Zero-copy processing of contiguous DMA-buffer regions
- Correct processing across physical buffer wrap-around
- Detection and accounting of overwritten unread data
- Parser resynchronization after RX data loss
- Deferred UART/DMA recovery outside interrupt context
- Interrupt-driven TX ring buffer
- Line-oriented command parser
- Reusable command-parser module with explicit per-instance state
- Host-side parser tests compiled with native GCC
- Overlength-command detection and recovery
- Asynchronous command responses
- EXTI handling for both button edges
- Non-blocking software debounce
- Runtime diagnostic counters through the UART console
- Compile-time validation of the DMA buffer size

## Supported commands

Commands are terminated by `\r` or `\n`.

| Command | Action |
|---|---|
| `LED ON` | Turns the user LED on |
| `LED OFF` | Turns the user LED off |
| `LED TOGGLE` | Toggles the user LED |
| `STATUS` | Reports RX, overflow, UART error, recovery, and parser diagnostics |

Successful LED commands return:

```text
OK
```

Unknown commands return:

```text
ERROR: UNKNOWN COMMAND
```

Example status response:

```text
RX=608 RX_OVF=0 UART_ERR=0 UART_RESTART_ERR=0 UART_LAST=0x00000000 CMD_OVF=1
```

## Architecture

### Receive path

1. USART2 continuously writes incoming bytes into a 256-byte circular DMA buffer.
2. DMA half-transfer, transfer-complete, and USART IDLE events call `HAL_UARTEx_RxEventCallback()`.
3. The callback calculates how many new bytes arrived since the previous physical DMA position.
4. Absolute `produced` and `consumed` counters track the logical data stream across DMA-buffer wrap-around.
5. The main loop obtains the next contiguous region directly from the DMA buffer.
6. The command parser processes the region without an intermediate copy.
7. The main loop advances `consumed` only after the complete region has been processed.

Interrupt callbacks only perform event accounting and state publication. Command parsing and application actions remain outside interrupt context.

### Zero-copy block processing

The physical DMA-buffer index is calculated using a mask:

```c
index = consumed & (UART_DMA_RX_BUFFER_SIZE - 1U);
```

The next block is limited by both:

- the number of available bytes;
- the number of bytes remaining before the physical end of the DMA buffer.

If logical data crosses the end of the array, it is processed as two contiguous blocks during consecutive main-loop iterations.

### RX overflow handling

Circular DMA cannot stop when the consumer falls behind. If:

```text
produced - consumed > UART_DMA_RX_BUFFER_SIZE
```

DMA has overwritten unread bytes.

The firmware:

1. calculates the exact number of lost bytes;
2. increments `RX_OVF`;
3. retains the newest 256 bytes;
4. reports the stream discontinuity to the command parser;
5. discards the damaged command up to the next line ending.

Discarding to the next line boundary prevents fragments from different commands from being combined into a syntactically valid but incorrect command.

### UART error recovery

`HAL_UART_ErrorCallback()` performs only minimal interrupt-context work:

- records the HAL error flags;
- increments the UART error counter;
- requests recovery.

The main loop performs the recovery:

- discards pending RX data;
- resets DMA stream accounting;
- resets parser state;
- restarts circular DMA reception;
- records restart failures separately.

This keeps error callbacks short and avoids performing complex recovery inside an ISR.

### Transmit path

Application responses are copied into a separate TX ring buffer.

If transmission is idle, the first byte is started with `HAL_UART_Transmit_IT()`. Each transmit-complete callback advances the TX queue and starts the next byte until the buffer becomes empty.

Interrupts are briefly masked while application code publishes data to the TX queue, preventing the callback from observing partially updated queue state.

### Command parser

The parser accepts a byte stream independently of DMA event boundaries. A command may therefore arrive:

- in one DMA event;
- across several IDLE events;
- across the physical end of the DMA buffer.

Overlength commands are discarded up to the next line ending and counted separately in `CMD_OVF`.

### Button handling

PC13 is configured for EXTI interrupts on rising and falling edges.

The EXTI callback records the edge time and schedules debounce processing. The main loop waits until the input remains stable for the configured debounce interval.

A confirmed button press toggles the user LED and queues:

```text
BUTTON PRESSED
```

## Diagnostic counters

| Field | Meaning |
|---|---|
| `RX` | Total bytes reported by RX DMA events |
| `RX_OVF` | Bytes overwritten before the main loop consumed them |
| `UART_ERR` | USART receive errors reported by HAL |
| `UART_RESTART_ERR` | Failed attempts to restart DMA reception |
| `UART_LAST` | Last HAL UART error flags in hexadecimal |
| `CMD_OVF` | Commands exceeding the parser buffer size |

Transport overflow and command overflow are intentionally reported separately.

## Manual validation

The following scenarios have been tested on hardware:

- normal LED commands and status responses;
- a command split across separate DMA/IDLE events;
- reception across multiple physical DMA-buffer wraps;
- a 300-character overlength command;
- deliberate consumer blocking to force DMA overflow;
- parser resynchronization after data loss;
- incorrect UART baud rate followed by DMA recovery;
- operation after repeated UART receive errors.

A 600-character test line plus `STATUS` accounted for all 608 transmitted bytes without transport loss under normal operation.

A deliberately blocked consumer produced the expected overflow count:

```text
601 received bytes - 256 retained bytes = 345 lost bytes
```
## Host-side tests

The command parser is independent of STM32 HAL and can be compiled and tested on a development computer.

The test suite covers:

- normal command completion;
- empty line handling;
- literal backslash sequences;
- maximum-length commands;
- overlength-command discard and recovery;
- runtime reset with diagnostic preservation;
- resynchronization after transport data loss;
- independent parser instances.

Example GCC build:

```text
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ICore/Inc \
    Core/Src/command_parser.c \
    tests/test_command_parser.c \
    -o command_parser_tests
```
## Project structure

```text
Core/
|-- Inc/                         Application and generated headers
|-- Src/                         Application and generated sources
`-- Startup/                     STM32F446RE startup code
tests/
`-- test_command_parser.c        Host-side command-parser tests
Drivers/
|-- CMSIS/                       ARM and STM32 device headers
`-- STM32F4xx_HAL_Driver/        STM32 HAL drivers
nucleo_f446re_lab01.ioc          STM32CubeMX configuration
STM32F446RETX_FLASH.ld           Flash linker script
STM32F446RETX_RAM.ld             RAM linker script
```

DMA stream accounting, the TX ring buffer, debounce state machine, application orchestration, and HAL callbacks currently remain in `Core/Src/main.c`.

The reusable command parser is implemented in `Core/Src/command_parser.c` with its public API in `Core/Inc/command_parser.h`.

## Build and run

1. Clone the repository.
2. Open STM32CubeIDE.
3. Select **File > Import > Existing Projects into Workspace**.
4. Select the cloned repository.
5. Build the project.
6. Connect and flash the NUCLEO-F446RE.
7. Open the ST-LINK Virtual COM Port at `115200 8-N-1`.
8. Send a supported command followed by a newline.

Peripheral configuration changes should be made in standalone STM32CubeMX using `nucleo_f446re_lab01.ioc`, followed by code generation and rebuilding in STM32CubeIDE.

## Current limitations
- RX transport, TX transport, and application orchestration remain in `main.c`.
- TX is interrupt-driven one byte at a time rather than DMA-based.
- Host-side tests currently cover the command parser only.
- The command protocol has no framing, checksum, sequence number, or authentication.
- The project does not use an RTOS.

## Planned improvements

- Extract reusable UART RX and TX modules
- Add DMA-based UART transmission
- Introduce a framed protocol with integrity checking
- Add telemetry-oriented message handling
- Evaluate FreeRTOS integration when concurrent tasks require it
- Add host-side tests for DMA stream-accounting logic
- Run host-side tests automatically in continuous integration

## Author

Igor Teryaev

GitHub: [igor-teryaev](https://github.com/igor-teryaev)