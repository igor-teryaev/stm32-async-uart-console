#ifndef PROTOCOL_FRAME_H
#define PROTOCOL_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_FRAME_MAGIC_0          0xA5U
#define PROTOCOL_FRAME_MAGIC_1          0x5AU
#define PROTOCOL_FRAME_VERSION          0x01U

#define PROTOCOL_FRAME_TYPE_COMMAND     0x01U
#define PROTOCOL_FRAME_TYPE_RESPONSE    0x02U
#define PROTOCOL_FRAME_TYPE_ACK         0x03U
#define PROTOCOL_FRAME_TYPE_TELEMETRY   0x10U

#define PROTOCOL_FRAME_MAX_PAYLOAD_SIZE 64U
#define PROTOCOL_FRAME_HEADER_SIZE      8U
#define PROTOCOL_FRAME_CRC_SIZE         2U
#define PROTOCOL_FRAME_MAX_SIZE         \
    (PROTOCOL_FRAME_HEADER_SIZE +       \
     PROTOCOL_FRAME_MAX_PAYLOAD_SIZE +  \
     PROTOCOL_FRAME_CRC_SIZE)

bool protocol_frame_encode(
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

#endif
