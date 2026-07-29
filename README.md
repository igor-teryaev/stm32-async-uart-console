\# STM32 UART Ring Buffer



Interrupt-driven UART reception with a lock-free ring buffer on the STM32F446RE microcontroller.



\## Overview



This project demonstrates asynchronous UART reception on the NUCLEO-F446RE board using interrupts and a circular buffer.



Received bytes are written to the ring buffer inside the UART receive callback, while the main loop processes them independently.



The implementation focuses on:



\- minimal interrupt execution time

\- non-blocking data reception

\- overflow detection

\- safe producer-consumer interaction

\- power-of-two buffer optimization

\- reliable asynchronous command processing



\## Hardware



\- STMicroelectronics NUCLEO-F446RE

\- STM32F446RE microcontroller

\- ARM Cortex-M4

\- ST-LINK Virtual COM Port



\## Development tools



\- STM32CubeIDE

\- STM32CubeMX

\- GCC ARM Embedded Toolchain

\- Git



\## Features



\- UART2 communication through ST-LINK Virtual COM Port

\- interrupt-driven byte reception

\- lock-free single-producer/single-consumer ring buffer

\- overflow counter

\- support for binary data, including `0x00`

\- asynchronous command processing

\- EXTI button handling

\- software debounce

\- TIM2 PWM configuration



\## Ring buffer design



The UART interrupt handler acts as the producer, while the main loop acts as the consumer.



The buffer size is a power of two, allowing index wrapping with a bit mask:



```c

next\_head = (head + 1U) \& (BUFFER\_SIZE - 1U);



This avoids division or modulo operations and is efficient on embedded targets.

One buffer slot is intentionally left unused so that:

head == tail means the buffer is empty

next\_head == tail means the buffer is full



Concurrency model

The implementation uses a single-producer/single-consumer model:

UART interrupt updates head

main loop updates tail

Because each index has only one writer, the design avoids global interrupt disabling during normal buffer operations.



Overflow handling

When the buffer is full:

the incoming byte is discarded

the overflow counter is incremented

unread data already stored in the buffer is preserved



Project structure

Core/

├── Inc/

└── Src/



Drivers/

├── CMSIS/

└── STM32F4xx\_HAL\_Driver/



nucleo\_f446re\_lab01.ioc

STM32F446RETX\_FLASH.ld

STM32F446RETX\_RAM.ld



How to build

Open STM32CubeIDE.

Select File → Import → Existing Projects into Workspace.

Select the repository directory.

Build the project.

Connect the NUCLEO-F446RE board.

Flash and run the firmware.



UART configuration

Peripheral: USART2

Baud rate: 115200

Data bits: 8

Stop bits: 1

Parity: none

Flow control: none



Current status

Implemented and tested:

GPIO output

EXTI interrupt

button debounce

UART transmission and reception

interrupt-driven UART RX

ring buffer

overflow protection

asynchronous command interface



Future improvements

UART reception using DMA

idle-line detection

command parser separation

unit tests for ring buffer logic

FreeRTOS integration

telemetry protocol support



Author

Igor Teryaev

GitHub: igor-teryaev

