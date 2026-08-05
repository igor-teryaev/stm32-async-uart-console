#include "protocol_receiver_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define RESULT_SUCCESS 0x00U
#define RESULT_FAILED  0x07U

static ProtocolFrame make_frame(
    uint8_t type,
    uint16_t sequence
)
{
    ProtocolFrame frame;

    memset(&frame, 0, sizeof(frame));

    frame.version = PROTOCOL_FRAME_VERSION;
    frame.type = type;
    frame.sequence = sequence;

    return frame;
}

static ProtocolReceiverDecision handle_frame(
    ProtocolReceiverSession *session,
    const ProtocolFrame *frame
)
{
    ProtocolReceiverDecision decision;

    assert(protocol_receiver_session_handle_frame(
        session,
        frame,
        &decision
    ));

    return decision;
}

static void test_initial_state(void)
{
    ProtocolReceiverSession session;

    protocol_receiver_session_init(&session);

    assert(!session.pending_valid);
    assert(!session.cached_result_valid);
    assert(!session.command_tracker.initialized);
}

static void test_non_command_is_ignored(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_TELEMETRY,
            100U
        );

    protocol_receiver_session_init(&session);

    ProtocolReceiverDecision decision =
        handle_frame(&session, &frame);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_IGNORED
    );

    assert(!session.pending_valid);
    assert(!session.command_tracker.initialized);
    assert(session.ignored_frame_count == 1U);
}

static void test_new_command_requests_execution(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    ProtocolReceiverDecision decision =
        handle_frame(&session, &frame);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(decision.sequence == 42U);
    assert(session.pending_valid);
    assert(session.pending_sequence == 42U);

    /*
     * Classification alone must not commit.
     */
    assert(!session.command_tracker.initialized);
    assert(
        session.command_tracker.accepted_count == 0U
    );
}

static void test_pending_duplicate_is_in_progress(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    ProtocolReceiverDecision decision =
        handle_frame(&session, &frame);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_IN_PROGRESS
    );

    assert(decision.sequence == 42U);
    assert(decision.expected_sequence == 42U);
    assert(session.in_progress_count == 1U);
    assert(
        session.command_tracker.duplicate_count == 0U
    );
}

static void test_other_command_while_pending(void)
{
    ProtocolReceiverSession session;

    ProtocolFrame first =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    ProtocolFrame second =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            43U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &first).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    ProtocolReceiverDecision decision =
        handle_frame(&session, &second);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER
    );

    assert(decision.expected_sequence == 42U);
    assert(session.pending_sequence == 42U);
}

static void test_defer_allows_same_sequence_retry(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_defer(
        &session,
        42U
    ));

    assert(!session.pending_valid);
    assert(!session.command_tracker.initialized);
    assert(session.deferred_count == 1U);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );
}

static void test_success_is_committed(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_complete(
        &session,
        42U,
        RESULT_SUCCESS
    ));

    assert(!session.pending_valid);
    assert(session.cached_result_valid);
    assert(session.cached_result_sequence == 42U);
    assert(session.cached_result_code ==
           RESULT_SUCCESS);

    assert(
        session.command_tracker
            .last_accepted_sequence == 42U
    );

    assert(
        session.command_tracker
            .expected_sequence == 43U
    );
}

static void test_duplicate_resends_cached_result(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_complete(
        &session,
        42U,
        RESULT_SUCCESS
    ));

    ProtocolReceiverDecision decision =
        handle_frame(&session, &frame);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_RESEND_RESULT
    );

    assert(decision.sequence == 42U);
    assert(decision.result_code == RESULT_SUCCESS);
    assert(decision.expected_sequence == 43U);

    assert(session.resent_result_count == 1U);
    assert(session.execute_count == 1U);
}

static void test_terminal_failure_is_cached(void)
{
    ProtocolReceiverSession session;

    ProtocolFrame first =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            100U
        );

    ProtocolFrame next =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            101U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &first).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_complete(
        &session,
        100U,
        RESULT_FAILED
    ));

    ProtocolReceiverDecision duplicate =
        handle_frame(&session, &first);

    assert(
        duplicate.action ==
        PROTOCOL_RECEIVER_ACTION_RESEND_RESULT
    );

    assert(duplicate.result_code == RESULT_FAILED);

    /*
     * A new attempt uses the next sequence.
     */
    assert(
        handle_frame(&session, &next).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );
}

static void test_out_of_order_reports_expected(void)
{
    ProtocolReceiverSession session;

    ProtocolFrame first =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            10U
        );

    ProtocolFrame skipped =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            12U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &first).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_complete(
        &session,
        10U,
        RESULT_SUCCESS
    ));

    ProtocolReceiverDecision decision =
        handle_frame(&session, &skipped);

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER
    );

    assert(decision.expected_sequence == 11U);
    assert(!session.pending_valid);
}

static void test_wrong_completion_is_rejected(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(!protocol_receiver_session_complete(
        &session,
        43U,
        RESULT_SUCCESS
    ));

    assert(session.pending_valid);
    assert(session.pending_sequence == 42U);

    assert(!protocol_receiver_session_defer(
        &session,
        43U
    ));

    assert(session.pending_valid);
}

static void test_sequence_wrap_around(void)
{
    ProtocolReceiverSession session;

    ProtocolFrame last =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            UINT16_MAX
        );

    ProtocolFrame wrapped =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            0U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &last).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_complete(
        &session,
        UINT16_MAX,
        RESULT_SUCCESS
    ));

    assert(
        session.command_tracker
            .expected_sequence == 0U
    );

    assert(
        handle_frame(&session, &wrapped).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );
}

static void test_reset_preserves_diagnostics(void)
{
    ProtocolReceiverSession session;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            42U
        );

    protocol_receiver_session_init(&session);

    assert(
        handle_frame(&session, &frame).action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(protocol_receiver_session_defer(
        &session,
        42U
    ));

    assert(session.execute_count == 1U);
    assert(session.deferred_count == 1U);

    protocol_receiver_session_reset(&session);

    assert(!session.pending_valid);
    assert(!session.cached_result_valid);
    assert(!session.command_tracker.initialized);

    assert(session.execute_count == 1U);
    assert(session.deferred_count == 1U);
}

static void test_invalid_arguments(void)
{
    ProtocolReceiverSession session;
    ProtocolReceiverDecision decision;
    ProtocolFrame frame =
        make_frame(
            PROTOCOL_FRAME_TYPE_COMMAND,
            1U
        );

    protocol_receiver_session_init(NULL);
    protocol_receiver_session_reset(NULL);

    protocol_receiver_session_init(&session);

    assert(!protocol_receiver_session_handle_frame(
        NULL,
        &frame,
        &decision
    ));

    assert(!protocol_receiver_session_handle_frame(
        &session,
        NULL,
        &decision
    ));

    assert(!protocol_receiver_session_handle_frame(
        &session,
        &frame,
        NULL
    ));

    assert(!protocol_receiver_session_complete(
        NULL,
        1U,
        RESULT_SUCCESS
    ));

    assert(!protocol_receiver_session_defer(
        NULL,
        1U
    ));
}

int main(void)
{
    test_initial_state();
    test_non_command_is_ignored();
    test_new_command_requests_execution();
    test_pending_duplicate_is_in_progress();
    test_other_command_while_pending();
    test_defer_allows_same_sequence_retry();
    test_success_is_committed();
    test_duplicate_resends_cached_result();
    test_terminal_failure_is_cached();
    test_out_of_order_reports_expected();
    test_wrong_completion_is_rejected();
    test_sequence_wrap_around();
    test_reset_preserves_diagnostics();
    test_invalid_arguments();

    puts("All protocol receiver session tests passed.");
    return 0;
}
