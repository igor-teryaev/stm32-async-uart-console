#ifndef PROTOCOL_FEEDBACK_H
#define PROTOCOL_FEEDBACK_H

#include "protocol_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_ACK_PAYLOAD_SIZE 1U
#define PROTOCOL_ACK_OUT_OF_ORDER_PAYLOAD_SIZE 3U

#define PROTOCOL_RESPONSE_RESULT_CODE_SIZE 1U

#define PROTOCOL_RESPONSE_MAX_DATA_SIZE \
    (PROTOCOL_FRAME_MAX_PAYLOAD_SIZE -  \
     PROTOCOL_RESPONSE_RESULT_CODE_SIZE)

typedef enum
{
    PROTOCOL_ACK_STATUS_ACCEPTED = 0x00U,
    PROTOCOL_ACK_STATUS_IN_PROGRESS = 0x01U,
    PROTOCOL_ACK_STATUS_OUT_OF_ORDER = 0x02U
} ProtocolAckStatus;

bool protocol_feedback_encode_ack(
    uint16_t sequence,
    ProtocolAckStatus status,
    uint16_t expected_sequence,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

bool protocol_feedback_decode_ack(
    const ProtocolFrame *frame,
    ProtocolAckStatus *status,
    uint16_t *expected_sequence
);

bool protocol_feedback_encode_response(
    uint16_t sequence,
    uint8_t result_code,
    const uint8_t *result_data,
    size_t result_data_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

bool protocol_feedback_decode_response(
    const ProtocolFrame *frame,
    uint8_t *result_code,
    const uint8_t **result_data,
    size_t *result_data_length
);

#endif
