#ifndef PROTOCOL_RECEIVER_SESSION_H
#define PROTOCOL_RECEIVER_SESSION_H

#include "protocol_feedback.h"
#include "protocol_sequence_tracker.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PROTOCOL_RECEIVER_ACTION_INVALID,
    PROTOCOL_RECEIVER_ACTION_IGNORED,
    PROTOCOL_RECEIVER_ACTION_EXECUTE,
    PROTOCOL_RECEIVER_ACTION_IN_PROGRESS,
    PROTOCOL_RECEIVER_ACTION_RESEND_RESULT,
    PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER
} ProtocolReceiverAction;

typedef struct
{
    ProtocolReceiverAction action;
    uint16_t sequence;
    uint16_t expected_sequence;
    uint8_t result_code;
    const uint8_t *result_data;
    size_t result_data_length;
} ProtocolReceiverDecision;

typedef struct
{
    ProtocolSequenceTracker command_tracker;

    bool pending_valid;
    uint16_t pending_sequence;

    bool cached_result_valid;
    uint16_t cached_result_sequence;
    uint8_t cached_result_code;
    uint8_t cached_result_data[
        PROTOCOL_RESPONSE_MAX_DATA_SIZE
    ];

    size_t cached_result_data_length;

    uint32_t execute_count;
    uint32_t in_progress_count;
    uint32_t resent_result_count;
    uint32_t ignored_frame_count;
    uint32_t deferred_count;
} ProtocolReceiverSession;

void protocol_receiver_session_init(
    ProtocolReceiverSession *session
);

void protocol_receiver_session_reset(
    ProtocolReceiverSession *session
);

bool protocol_receiver_session_handle_frame(
    ProtocolReceiverSession *session,
    const ProtocolFrame *frame,
    ProtocolReceiverDecision *decision
);

bool protocol_receiver_session_complete(
    ProtocolReceiverSession *session,
    uint16_t sequence,
    uint8_t result_code,
    const uint8_t *result_data,
    size_t result_data_length
);

bool protocol_receiver_session_defer(
    ProtocolReceiverSession *session,
    uint16_t sequence
);

#endif
