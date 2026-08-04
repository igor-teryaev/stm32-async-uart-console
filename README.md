# STM32 Async UART Console

Non-blocking UART command console using circular DMA reception, IDLE-line detection, DMA-based transmission, error recovery, and debounced EXTI input on the STM32F446RE.

## Overview

This project implements an asynchronous command console for the NUCLEO-F446RE development board.

USART2 reception uses a 256-byte circular DMA buffer with half-transfer, transfer-complete, and IDLE-line events. The main loop processes received data directly from the DMA buffer in contiguous zero-copy blocks.

Transmission uses a tested HAL-independent SPSC byte queue, while the STM32 HAL adapter sends contiguous blocks using DMA.
Command parsing, hardware control, error recovery, and button debounce are performed outside interrupt context.

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
- TX: DMA1 Stream 6, normal mode

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
- Tested HAL-independent SPSC TX byte queue
- DMA-based transmission of contiguous TX blocks
- TX backpressure without command-response loss
- Line-oriented command parser
- Reusable command-parser module with explicit per-instance state
- Host-side tests for command parsing, DMA stream accounting, and the SPSC byte queue
- Overlength-command detection and recovery
- Asynchronous command responses
- EXTI handling for both button edges
- Non-blocking software debounce
- Runtime diagnostic counters through the UART console
- Compile-time validation of the DMA buffer size
- Reusable HAL-independent circular DMA stream module
- Incremental CRC-16/CCITT-FALSE integrity calculation

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

Application responses are copied into a HAL-independent single-producer/single-consumer byte queue.

Queue writes use up to two `memcpy()` operations when data crosses the physical end of the circular buffer.
The new absolute `head` value is published only after both copies complete, preventing the consumer from observing partially written data.

When USART2 is idle, the HAL adapter obtains the largest contiguous block with `spsc_byte_queue_peek()`
and starts the entire block using `HAL_UART_Transmit_DMA()`.
The adapter stores the active transfer length.
The DMA transfer-complete path ultimately invokes the UART transmit-complete callback,
which consumes exactly the stored active length and immediately starts the next contiguous block.

If HAL cannot start a transfer, accepted data remains queued and the main loop retries later.

If a command response does not fit in the TX queue, the completed command is not executed again. Its response remains pending, RX consumption stops at the exact processed byte boundary, and parsing resumes after TX capacity becomes available.

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

The command parser, circular DMA stream, CRC calculation, and SPSC byte queue modules are independent of STM32 HAL and can be compiled and tested on a development computer.

The test suite covers:

### Command parser

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

### Circular DMA stream

The DMA stream test suite covers:

- configuration validation and power-of-two capacities;
- HT, TC, and IDLE position accounting;
- physical position wrap-around;
- zero-copy contiguous block access;
- guarded consumption;
- exact overflow accounting;
- DMA restart alignment and recovery.

### SPSC byte queue

The SPSC byte queue test suite covers:

- configuration validation and power-of-two capacities;
- use of the full configured capacity;
- contiguous `peek()` and guarded `consume()`;
- wrap-around writes split across two physical blocks;
- rejection of writes that exceed available capacity;
- preservation of queue state after failed operations.

Example GCC build:

```text
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ICore/Inc \
    Core/Src/spsc_byte_queue.c \
    tests/test_spsc_byte_queue.c \
    -o spsc_byte_queue_tests
    
Example GCC build:

```text
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ICore/Inc \
    Core/Src/dma_circular_stream.c \
    tests/test_dma_circular_stream.c \
    -o dma_circular_stream_tests
```

### CRC-16/CCITT-FALSE

The CRC test suite covers:

- the standard `"123456789" → 0x29B1` check value;
- empty input;
- incremental calculation across multiple blocks;
- byte-order changes;
- single-bit corruption;
- zero-length state preservation.

Example GCC build:

```text
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ICore/Inc \
    Core/Src/crc16_ccitt.c \
    tests/test_crc16_ccitt.c \
    -o crc16_ccitt_tests
```

## Project structure

```text
Core/
|-- Inc/                         Application and generated headers
|-- Src/                         Application and generated sources
`-- Startup/                     STM32F446RE startup code
tests/
`-- test_command_parser.c        Host-side command-parser tests
`-- test_dma_circular_stream.c   Host-side DMA-stream tests
`-- test_spsc_byte_queue.c       Host-side SPSC byte-queue tests
`-- test_crc16_ccitt.c           Host-side CRC-16 tests
Drivers/
|-- CMSIS/                       ARM and STM32 device headers
`-- STM32F4xx_HAL_Driver/        STM32 HAL drivers
nucleo_f446re_lab01.ioc          STM32CubeMX configuration
STM32F446RETX_FLASH.ld           Flash linker script
STM32F446RETX_RAM.ld             RAM linker script
```

The reusable HAL-independent TX queue is implemented in `Core/Src/spsc_byte_queue.c`.

The STM32 UART TX adapter is implemented in `Core/Src/uart_tx_adapter.c`. The global HAL transmit-complete callback remains in `Core/Src/main.c` and routes completion events to the adapter.

The debounce state machine and application orchestration currently remain in `Core/Src/main.c`.

The reusable command parser is implemented in `Core/Src/command_parser.c`.

HAL-independent circular DMA position accounting, zero-copy block access, overflow detection, and restart alignment are implemented in `Core/Src/dma_circular_stream.c`.

Incremental CRC-16/CCITT-FALSE calculation is implemented in `Core/Src/crc16_ccitt.c`.

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
- The debounce logic and application orchestration remain in `main.c`.
- The STM32 UART TX adapter has been validated on NUCLEO-F446RE hardware but is not covered by automated host-side HAL mocks.
- The command protocol has no framing, checksum, sequence number, or authentication.
- The project does not use an RTOS.

## Planned improvements

- Introduce a framed protocol with integrity checking
- Add telemetry-oriented message handling
- Evaluate FreeRTOS integration when concurrent tasks require it
- Run host-side tests automatically in continuous integration

## Author

Igor Teryaev

GitHub: [igor-teryaev](https://github.com/igor-teryaev)