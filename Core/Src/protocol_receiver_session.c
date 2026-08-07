#include "protocol_receiver_session.h"

#include <string.h>

static void protocol_receiver_decision_clear(
    ProtocolReceiverDecision *decision
)
{
    decision->action =
        PROTOCOL_RECEIVER_ACTION_INVALID;

    decision->sequence = 0U;
    decision->expected_sequence = 0U;
    decision->result_code = 0U;
    decision->result_data = NULL;
    decision->result_data_length = 0U;
}

void protocol_receiver_session_init(
    ProtocolReceiverSession *session
)
{
    if (session == NULL)
    {
        return;
    }

    memset(session, 0, sizeof(*session));

    protocol_sequence_tracker_init(
        &session->command_tracker
    );
}

void protocol_receiver_session_reset(
    ProtocolReceiverSession *session
)
{
    if (session == NULL)
    {
        return;
    }

    protocol_sequence_tracker_reset_session(
        &session->command_tracker
    );

    session->pending_valid = false;
    session->pending_sequence = 0U;

    session->cached_result_valid = false;
    session->cached_result_sequence = 0U;
    session->cached_result_code = 0U;
    session->cached_result_data_length = 0U;
}

bool protocol_receiver_session_handle_frame(
    ProtocolReceiverSession *session,
    const ProtocolFrame *frame,
    ProtocolReceiverDecision *decision
)
{
    ProtocolSequenceResult sequence_result;

    if ((session == NULL) ||
        (frame == NULL) ||
        (decision == NULL))
    {
        return false;
    }

    protocol_receiver_decision_clear(decision);

    decision->sequence = frame->sequence;

    if (frame->type !=
        PROTOCOL_FRAME_TYPE_COMMAND)
    {
        decision->action =
            PROTOCOL_RECEIVER_ACTION_IGNORED;

        session->ignored_frame_count++;
        return true;
    }

    if (session->pending_valid)
    {
        if (frame->sequence ==
            session->pending_sequence)
        {
            decision->action =
                PROTOCOL_RECEIVER_ACTION_IN_PROGRESS;

            decision->expected_sequence =
                session->pending_sequence;

            session->in_progress_count++;
        }
        else
        {
            decision->action =
                PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER;

            decision->expected_sequence =
                session->pending_sequence;
        }

        return true;
    }

    sequence_result =
        protocol_sequence_tracker_classify(
            &session->command_tracker,
            frame->sequence
        );

    switch (sequence_result)
    {
        case PROTOCOL_SEQUENCE_RESULT_NEW:
            session->pending_valid = true;
            session->pending_sequence =
                frame->sequence;

            decision->action =
                PROTOCOL_RECEIVER_ACTION_EXECUTE;

            decision->expected_sequence =
                frame->sequence;

            session->execute_count++;
            return true;

        case PROTOCOL_SEQUENCE_RESULT_DUPLICATE:
            if (!session->cached_result_valid ||
                (session->cached_result_sequence !=
                 frame->sequence))
            {
                return false;
            }

            decision->action =
                PROTOCOL_RECEIVER_ACTION_RESEND_RESULT;

            decision->result_code =
                session->cached_result_code;
            decision->result_data =
                session->cached_result_data;

            decision->result_data_length =
                session->cached_result_data_length;

            decision->expected_sequence =
                session->command_tracker
                    .expected_sequence;

            session->resent_result_count++;
            return true;

        case PROTOCOL_SEQUENCE_RESULT_OUT_OF_ORDER:
            decision->action =
                PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER;

            decision->expected_sequence =
                session->command_tracker
                    .expected_sequence;

            return true;

        case PROTOCOL_SEQUENCE_RESULT_INVALID:
        default:
            return false;
    }
}

bool protocol_receiver_session_complete(
    ProtocolReceiverSession *session,
    uint16_t sequence,
    uint8_t result_code,
    const uint8_t *result_data,
    size_t result_data_length
)
{
    if ((session == NULL) ||
        !session->pending_valid ||
        (sequence != session->pending_sequence) ||
        (result_data_length >
         PROTOCOL_RESPONSE_MAX_DATA_SIZE) ||
        ((result_data_length > 0U) &&
         (result_data == NULL)))
    {
        return false;
    }

    if (!protocol_sequence_tracker_commit(
            &session->command_tracker,
            sequence
        ))
    {
        return false;
    }

    if (result_data_length > 0U)
    {
        memcpy(
            session->cached_result_data,
            result_data,
            result_data_length
        );
    }

    session->cached_result_data_length =
        result_data_length;

    session->cached_result_valid = true;
    session->cached_result_sequence = sequence;
    session->cached_result_code = result_code;

    session->pending_valid = false;
    session->pending_sequence = 0U;

    return true;
}

bool protocol_receiver_session_defer(
    ProtocolReceiverSession *session,
    uint16_t sequence
)
{
    if ((session == NULL) ||
        !session->pending_valid ||
        (sequence != session->pending_sequence))
    {
        return false;
    }

    /*
     * No sequence commit: the same command must be
     * classified as NEW when it is retried.
     */
    session->pending_valid = false;
    session->pending_sequence = 0U;
    session->deferred_count++;

    return true;
}
