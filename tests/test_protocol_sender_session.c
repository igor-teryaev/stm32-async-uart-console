#include "protocol_sender_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_SEQUENCE 42U
#define RESPONSE_TIMEOUT_MS 100U
#define MAX_ATTEMPTS 3U

static void init_session(
    ProtocolSenderSession *session
)
{
    assert(protocol_sender_session_init(
        session,
        INITIAL_SEQUENCE,
        RESPONSE_TIMEOUT_MS,
        MAX_ATTEMPTS
    ));
}

static void start_command(
    ProtocolSenderSession *session
)
{
    static const uint8_t payload[] =
        { 0x10U, 0x20U, 0x30U };

    assert(protocol_sender_session_start_command(
        session,
        payload,
        sizeof(payload)
    ));
}

static void accept_and_complete_transmission(
    ProtocolSenderSession *session,
    uint32_t now_ms
)
{
    assert(protocol_sender_session_mark_accepted(
        session
    ));

    assert(protocol_sender_session_mark_transmitted(
        session,
        now_ms
    ));
}

static void test_initialization(void)
{
    ProtocolSenderSession session;

    init_session(&session);

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );

    assert(!session.transmission_active);
    assert(session.next_sequence ==
           INITIAL_SEQUENCE);
    assert(session.response_timeout_ms ==
           RESPONSE_TIMEOUT_MS);
    assert(session.max_attempts ==
           MAX_ATTEMPTS);
    assert(session.attempt_count == 0U);

    assert(!protocol_sender_session_init(
        NULL,
        INITIAL_SEQUENCE,
        RESPONSE_TIMEOUT_MS,
        MAX_ATTEMPTS
    ));

    assert(!protocol_sender_session_init(
        &session,
        INITIAL_SEQUENCE,
        0U,
        MAX_ATTEMPTS
    ));

    assert(!protocol_sender_session_init(
        &session,
        INITIAL_SEQUENCE,
        (UINT32_MAX / 2U) + 1U,
        MAX_ATTEMPTS
    ));

    assert(!protocol_sender_session_init(
        &session,
        INITIAL_SEQUENCE,
        RESPONSE_TIMEOUT_MS,
        0U
    ));
}

static void test_start_and_initial_transmission(void)
{
    ProtocolSenderSession session;
    const uint8_t *data;
    size_t length;
    uint16_t sequence;

    init_session(&session);
    start_command(&session);

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(session.pending_sequence ==
           INITIAL_SEQUENCE);
    assert(session.attempt_count == 0U);
    assert(session.command_count == 1U);

    assert(protocol_sender_session_get_transmission(
        &session,
        &data,
        &length,
        &sequence
    ));

    assert(data == session.pending_frame);
    assert(length ==
           session.pending_frame_length);
    assert(sequence == INITIAL_SEQUENCE);

    assert(length ==
           PROTOCOL_FRAME_HEADER_SIZE +
           3U +
           PROTOCOL_FRAME_CRC_SIZE);

    assert(data[0] == PROTOCOL_FRAME_MAGIC_0);
    assert(data[1] == PROTOCOL_FRAME_MAGIC_1);
    assert(data[2] == PROTOCOL_FRAME_VERSION);
    assert(data[3] ==
           PROTOCOL_FRAME_TYPE_COMMAND);
    assert(data[4] == 0U);
    assert(data[5] == INITIAL_SEQUENCE);

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_SENDING
    );

    assert(session.transmission_active);
    assert(session.attempt_count == 1U);
    assert(session.transmission_count == 1U);
    assert(session.retry_count == 0U);

    assert(protocol_sender_session_mark_transmitted(
        &session,
        1000U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_WAITING_RESULT
    );

    assert(!session.transmission_active);
    assert(session.last_activity_ms == 1000U);
}

static void test_timeout_retries_and_failure(void)
{
    ProtocolSenderSession session;
    uint8_t first_frame[PROTOCOL_FRAME_MAX_SIZE];
    size_t first_length;

    init_session(&session);
    start_command(&session);

    first_length = session.pending_frame_length;

    memcpy(
        first_frame,
        session.pending_frame,
        first_length
    );

    accept_and_complete_transmission(
        &session,
        1000U
    );

    protocol_sender_session_poll(
        &session,
        1099U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_WAITING_RESULT
    );

    protocol_sender_session_poll(
        &session,
        1100U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(session.timeout_count == 1U);

    assert(
        memcmp(
            first_frame,
            session.pending_frame,
            first_length
        ) == 0
    );

    accept_and_complete_transmission(
        &session,
        1200U
    );

    assert(session.attempt_count == 2U);
    assert(session.retry_count == 1U);
    assert(session.transmission_count == 2U);

    protocol_sender_session_poll(
        &session,
        1300U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    accept_and_complete_transmission(
        &session,
        1400U
    );

    assert(session.attempt_count == 3U);
    assert(session.retry_count == 2U);
    assert(session.transmission_count == 3U);

    protocol_sender_session_poll(
        &session,
        1500U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED
    );

    assert(session.timeout_count == 3U);
    assert(
        session.communication_failure_count == 1U
    );

    assert(!protocol_sender_session_start_command(
        &session,
        NULL,
        0U
    ));
}

static void test_tick_wrap_around(void)
{
    ProtocolSenderSession session;

    assert(protocol_sender_session_init(
        &session,
        INITIAL_SEQUENCE,
        40U,
        MAX_ATTEMPTS
    ));

    start_command(&session);

    accept_and_complete_transmission(
        &session,
        0xFFFFFFF0U
    );

    protocol_sender_session_poll(
        &session,
        0x00000017U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_WAITING_RESULT
    );

    protocol_sender_session_poll(
        &session,
        0x00000018U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(session.timeout_count == 1U);
}

static void test_in_progress_renews_timeout(void)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    assert(!protocol_sender_session_handle_in_progress(
        &session,
        INITIAL_SEQUENCE + 1U,
        150U
    ));

    assert(protocol_sender_session_handle_in_progress(
        &session,
        INITIAL_SEQUENCE,
        150U
    ));

    assert(session.last_activity_ms == 150U);
    assert(session.in_progress_count == 1U);

    protocol_sender_session_poll(
        &session,
        249U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_WAITING_RESULT
    );

    protocol_sender_session_poll(
        &session,
        250U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );
}

static void test_late_in_progress_cancels_scheduled_retry(
    void
)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    protocol_sender_session_poll(
        &session,
        200U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(protocol_sender_session_handle_in_progress(
        &session,
        INITIAL_SEQUENCE,
        210U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_WAITING_RESULT
    );

    assert(session.attempt_count == 1U);
    assert(session.retry_count == 0U);
    assert(session.last_activity_ms == 210U);
}

static void test_terminal_result_and_next_sequence(void)
{
    ProtocolSenderSession session;
    static const uint8_t result_failed = 7U;

    init_session(&session);
    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    assert(protocol_sender_session_handle_result(
        &session,
        INITIAL_SEQUENCE,
        result_failed
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_RESULT_READY
    );

    assert(session.result_code == result_failed);
    assert(session.completed_count == 1U);
    assert(session.next_sequence ==
           INITIAL_SEQUENCE + 1U);

    assert(protocol_sender_session_release_result(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );

    assert(session.next_sequence ==
           INITIAL_SEQUENCE + 1U);

    start_command(&session);

    assert(session.pending_sequence ==
           INITIAL_SEQUENCE + 1U);
}

static void test_late_result_during_retry(void)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    protocol_sender_session_poll(
        &session,
        200U
    );

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(session.transmission_active);
    assert(session.attempt_count == 2U);

    /*
     * Це запізнілий результат першої
     * передачі під час активного retry.
     */
    assert(protocol_sender_session_handle_result(
        &session,
        INITIAL_SEQUENCE,
        0U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_RESULT_READY
    );

    assert(session.transmission_active);

    /*
     * Буфер не можна звільнити, поки
     * transport ще може його читати.
     */
    assert(!protocol_sender_session_release_result(
        &session
    ));

    assert(protocol_sender_session_mark_transmitted(
        &session,
        220U
    ));

    assert(!session.transmission_active);

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_RESULT_READY
    );

    assert(protocol_sender_session_release_result(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );
}

static void test_transmit_errors_exhaust_attempts(void)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(protocol_sender_session_mark_transmit_error(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(protocol_sender_session_mark_transmit_error(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(protocol_sender_session_mark_transmit_error(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED
    );

    assert(session.attempt_count == 3U);
    assert(session.transmission_count == 3U);
    assert(session.retry_count == 2U);
    assert(session.timeout_count == 0U);

    assert(
        session.communication_failure_count == 1U
    );
}

static void test_reset_preserves_diagnostics(void)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    assert(protocol_sender_session_handle_in_progress(
        &session,
        INITIAL_SEQUENCE,
        120U
    ));

    assert(protocol_sender_session_handle_result(
        &session,
        INITIAL_SEQUENCE,
        0U
    ));

    assert(protocol_sender_session_release_result(
        &session
    ));

    assert(protocol_sender_session_reset(
        &session,
        1000U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );

    assert(session.next_sequence == 1000U);
    assert(session.command_count == 1U);
    assert(session.transmission_count == 1U);
    assert(session.in_progress_count == 1U);
    assert(session.completed_count == 1U);

    assert(session.response_timeout_ms ==
           RESPONSE_TIMEOUT_MS);

    assert(session.max_attempts ==
           MAX_ATTEMPTS);
}

static void test_late_result_recovers_failure(void)
{
    ProtocolSenderSession session;

    assert(protocol_sender_session_init(
        &session,
        INITIAL_SEQUENCE,
        RESPONSE_TIMEOUT_MS,
        1U
    ));

    start_command(&session);

    accept_and_complete_transmission(
        &session,
        100U
    );

    protocol_sender_session_poll(
        &session,
        200U
    );

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED
    );

    assert(
        session.communication_failure_count == 1U
    );

    assert(protocol_sender_session_handle_result(
        &session,
        INITIAL_SEQUENCE,
        0U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_RESULT_READY
    );

    assert(session.next_sequence ==
           INITIAL_SEQUENCE + 1U);

    assert(session.completed_count == 1U);

    /*
     * Лічильник зберігає факт того, що
     * response timeout був вичерпаний.
     */
    assert(
        session.communication_failure_count == 1U
    );

    assert(protocol_sender_session_release_result(
        &session
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );
}

static void test_sequence_wrap_around(void)
{
    ProtocolSenderSession session;

    assert(protocol_sender_session_init(
        &session,
        UINT16_MAX,
        RESPONSE_TIMEOUT_MS,
        MAX_ATTEMPTS
    ));

    start_command(&session);

    assert(session.pending_sequence ==
           UINT16_MAX);

    accept_and_complete_transmission(
        &session,
        100U
    );

    assert(protocol_sender_session_handle_result(
        &session,
        UINT16_MAX,
        0U
    ));

    assert(session.next_sequence == 0U);

    assert(protocol_sender_session_release_result(
        &session
    ));

    start_command(&session);

    assert(session.pending_sequence == 0U);
}

static void test_reset_rejected_during_transmission(void)
{
    ProtocolSenderSession session;

    init_session(&session);
    start_command(&session);

    assert(protocol_sender_session_mark_accepted(
        &session
    ));

    assert(session.transmission_active);

    assert(!protocol_sender_session_reset(
        &session,
        1000U
    ));

    assert(
        session.state ==
        PROTOCOL_SENDER_STATE_SENDING
    );

    assert(session.transmission_active);
    assert(session.pending_sequence ==
           INITIAL_SEQUENCE);
}

int main(void)
{
    test_initialization();
    test_start_and_initial_transmission();
    test_timeout_retries_and_failure();
    test_tick_wrap_around();
    test_in_progress_renews_timeout();
    test_late_in_progress_cancels_scheduled_retry();
    test_terminal_result_and_next_sequence();
    test_late_result_during_retry();
    test_transmit_errors_exhaust_attempts();
    test_reset_preserves_diagnostics();
    test_late_result_recovers_failure();
    test_sequence_wrap_around();
    test_reset_rejected_during_transmission();
    puts("All protocol sender session tests passed.");
    return 0;
}
