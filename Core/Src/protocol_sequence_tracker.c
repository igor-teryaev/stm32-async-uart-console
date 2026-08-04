#include "protocol_sequence_tracker.h"

#include <string.h>

static void protocol_sequence_tracker_accept(
    ProtocolSequenceTracker *tracker,
    uint16_t sequence
)
{
    tracker->initialized = true;
    tracker->last_accepted_sequence = sequence;
    tracker->expected_sequence =
        (uint16_t)(sequence + 1U);
    tracker->accepted_count++;
}

void protocol_sequence_tracker_init(
    ProtocolSequenceTracker *tracker
)
{
    if (tracker == NULL)
    {
        return;
    }

    memset(tracker, 0, sizeof(*tracker));
}

void protocol_sequence_tracker_reset_session(
    ProtocolSequenceTracker *tracker
)
{
    if (tracker == NULL)
    {
        return;
    }

    /*
     * Reset sequence synchronization while preserving
     * lifetime diagnostics.
     */
    tracker->initialized = false;
    tracker->last_accepted_sequence = 0U;
    tracker->expected_sequence = 0U;
}

ProtocolSequenceResult
protocol_sequence_tracker_process(
    ProtocolSequenceTracker *tracker,
    uint16_t sequence
)
{
    if (tracker == NULL)
    {
        return PROTOCOL_SEQUENCE_RESULT_INVALID;
    }

    if (!tracker->initialized)
    {
        protocol_sequence_tracker_accept(
            tracker,
            sequence
        );

        return PROTOCOL_SEQUENCE_RESULT_NEW;
    }

    if (sequence ==
        tracker->last_accepted_sequence)
    {
        tracker->duplicate_count++;

        return PROTOCOL_SEQUENCE_RESULT_DUPLICATE;
    }

    if (sequence == tracker->expected_sequence)
    {
        protocol_sequence_tracker_accept(
            tracker,
            sequence
        );

        return PROTOCOL_SEQUENCE_RESULT_NEW;
    }

    tracker->out_of_order_count++;

    return PROTOCOL_SEQUENCE_RESULT_OUT_OF_ORDER;
}
