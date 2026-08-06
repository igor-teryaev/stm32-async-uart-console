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
- Versioned binary frame format with explicit big-endian serialization
- CRC-protected frame encoder
- Streaming frame decoder with noise rejection and resynchronization
- Stop-and-wait sequence tracking with duplicate and out-of-order detection
- Correct 16-bit sequence wrap-around
- Two-phase sequence classification and commit
- Receiver-side stop-and-wait session state machine
- Duplicate suppression while a command is executing
- Cached terminal-result retransmission for completed duplicates
- Deferred command retry without sequence advancement
- HAL-independent sender-side stop-and-wait session
- Configurable response timeout and maximum transmission attempts
- Wrap-safe timeout handling across the 32-bit tick boundary
- Retransmission of identical encoded frame bytes and sequence ID
- `IN_PROGRESS` handling with timeout renewal
- Recovery through late terminal results
- Explicit distinction between command failure and unknown communication outcome

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

### Binary frame codec

A HAL-independent binary frame codec is implemented but is not yet connected to the line-oriented UART console.

The wire format is:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Magic `0xA5` |
| 1 | 1 | Magic `0x5A` |
| 2 | 1 | Protocol version |
| 3 | 1 | Message type |
| 4 | 2 | Sequence ID, big-endian |
| 6 | 2 | Payload length, big-endian |
| 8 | 0–64 | Payload |
| 8 + N | 2 | CRC-16/CCITT-FALSE, big-endian |

The CRC covers `VERSION`, `TYPE`, `SEQUENCE`, `PAYLOAD_LENGTH`, and `PAYLOAD`. Magic bytes and the transmitted CRC are excluded.

The encoder serializes every field explicitly rather than copying a C structure, avoiding padding, alignment, ABI, and endianness dependencies.

The decoder accepts one byte at a time and supports:

- noise before a frame;
- overlapping magic candidates such as `A5 A5 5A`;
- frames split across arbitrary transport blocks;
- multiple frames in one transport block;
- zero-length payloads;
- invalid-version rejection;
- oversized-length rejection;
- CRC-error detection;
- stream resynchronization after errors.

### Protocol receiver session

A tested HAL-independent receiver session coordinates decoded command frames with the stop-and-wait sequence tracker.

A new command is first classified but not immediately committed. The session returns one of the following actions:

- `EXECUTE` — execute a new command;
- `IN_PROGRESS` — the same command is already executing;
- `RESEND_RESULT` — resend the cached terminal result without executing the command again;
- `OUT_OF_ORDER` — reject a command with an unexpected sequence;
- `IGNORED` — the frame is not a command.

Sequence advancement occurs only after `protocol_receiver_session_complete()` records a terminal success or failure.

A temporarily unavailable command may be released with `protocol_receiver_session_defer()`.
This does not advance the sequence, so retrying the same command is classified as new.

The most recent terminal result is cached because the sender may retransmit a command when its previous response was lost.
Under stop-and-wait operation, only one completed result needs to be retained.

### Protocol sender session

A tested HAL-independent sender session manages one outstanding command at a time.

The sender:

- assigns a sequence ID to a new command;
- encodes and retains the complete frame for retransmission;
- exposes the retained frame to an external transport;
- starts the response timeout only after transmission completion;
- retries the same encoded frame and sequence after timeout;
- renews the timeout after a matching `IN_PROGRESS` response;
- stores a matching terminal result;
- advances the sequence only after the command outcome becomes known.

The configured `max_attempts` includes the initial transmission. For example, `max_attempts = 3` permits one initial transmission and two retries.

If all attempts are exhausted, the sender enters `COMMUNICATION_FAILED`.
This means that the command outcome is unknown, not that the receiver definitely rejected or failed to execute the command.

A matching late terminal result may recover this state because no later command can start while the session remains failed.

The pending encoded frame remains owned by the sender session until any active transport operation completes.
This prevents a new command from overwriting memory still being read by a zero-copy transport.

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
```

### Circular DMA stream

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
### Binary frame codec

Encoder tests cover known wire bytes, big-endian serialization, empty and maximum payloads, output-capacity checks, oversized payloads, and invalid arguments.

Decoder tests use independent fixed wire vectors and cover byte-wise input, transport-block boundaries,
multiple frames, overlapping magic, empty payloads, malformed headers, CRC failures, recovery, and diagnostic preservation.

### Protocol sequence tracker

The sequence tracker tests cover initial synchronization, expected frames,
duplicate suppression, out-of-order frames, 16-bit wrap-around,
duplicate detection at the wrap boundary, session reset, and diagnostic preservation.

### Protocol receiver session

The receiver-session tests cover:

- new-command execution without premature sequence commit;
- duplicate detection while a command is in progress;
- rejection of a different command while one is pending;
- deferred retry using the same sequence;
- terminal success and failure commit;
- cached-result retransmission without duplicate execution;
- out-of-order sequence reporting;
- incorrect completion rejection;
- 16-bit sequence wrap-around;
- session reset with diagnostic preservation.

### Protocol sender session

The sender-session tests cover:

- initial command encoding and transmission;
- exact-frame retransmission;
- configurable attempt exhaustion;
- transport transmission errors;
- response timeout boundaries;
- wrap-safe timeout calculation across `UINT32_MAX`;
- matching and mismatched `IN_PROGRESS` responses;
- timeout renewal and scheduled-retry cancellation;
- terminal command success and failure;
- late terminal results during active retransmission;
- late-result recovery after communication failure;
- protection of transport-owned frame memory;
- 16-bit sequence wrap-around;
- session reset with diagnostic preservation.

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
`-- test_protocol_frame.c        Host-side frame-encoder tests
`-- test_protocol_frame_decoder.c Host-side streaming-decoder tests
`-- test_protocol_sequence_tracker.c Host-side sequence-tracker tests
`-- test_protocol_receiver_session.c Host-side receiver-session tests
`-- test_protocol_sender_session.c Host-side sender-session tests
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

Binary frame encoding is implemented in `Core/Src/protocol_frame.c`.

Streaming frame decoding and resynchronization are implemented in `Core/Src/protocol_frame_decoder.c`.

Stop-and-wait sequence classification and duplicate detection are implemented in `Core/Src/protocol_sequence_tracker.c`.

Receiver-side command execution, pending-command handling, deferred retry,
and cached terminal-result retransmission are implemented in `Core/Src/protocol_receiver_session.c`.

Sender-side timeout handling, retransmission scheduling, transmission ownership,
terminal-result handling, and late-result recovery are implemented in `Core/Src/protocol_sender_session.c`.

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
- The active UART console remains line-oriented; the tested binary frame codec is not yet integrated.
- The receiver caches only the most recent terminal result in RAM; reset or power loss clears this state.
- The tested sender and receiver sessions are not yet connected to the frame decoder, UART transport, or application command execution.
- A sender communication failure still requires session resynchronization if no late terminal result arrives.
- Session identifiers and startup/resynchronization handshake are not yet implemented.
- `IN_PROGRESS` renews the response timeout, but a separate maximum operation-duration timeout is not yet implemented.
- The project does not use an RTOS.

## Planned improvements

- Integrate the binary frame codec and sender/receiver sessions with the UART transport and application command execution
- Define framed `IN_PROGRESS` and terminal-result payloads
- Implement session identifiers and startup/resynchronization handshake
- Add an independent maximum operation-duration timeout
- Add telemetry-oriented message handling
- Evaluate FreeRTOS integration when concurrent tasks require it
- Run host-side tests automatically in continuous integration

## Author

Igor Teryaev

GitHub: [igor-teryaev](https://github.com/igor-teryaev)