#ifndef PROTOCOL_SEQUENCE_TRACKER_H
#define PROTOCOL_SEQUENCE_TRACKER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PROTOCOL_SEQUENCE_RESULT_INVALID,
    PROTOCOL_SEQUENCE_RESULT_NEW,
    PROTOCOL_SEQUENCE_RESULT_DUPLICATE,
    PROTOCOL_SEQUENCE_RESULT_OUT_OF_ORDER
} ProtocolSequenceResult;

typedef struct
{
    bool initialized;
    uint16_t last_accepted_sequence;
    uint16_t expected_sequence;

    uint32_t accepted_count;
    uint32_t duplicate_count;
    uint32_t out_of_order_count;
} ProtocolSequenceTracker;

void protocol_sequence_tracker_init(
    ProtocolSequenceTracker *tracker
);

void protocol_sequence_tracker_reset_session(
    ProtocolSequenceTracker *tracker
);

ProtocolSequenceResult
protocol_sequence_tracker_process(
    ProtocolSequenceTracker *tracker,
    uint16_t sequence
);

#endif
