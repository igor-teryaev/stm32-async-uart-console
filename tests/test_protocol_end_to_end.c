#include "protocol_feedback.h"
#include "protocol_frame_decoder.h"
#include "protocol_receiver_session.h"
#include "protocol_sender_session.h"
#include "protocol_session_router.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_SEQUENCE 42U
#define TEST_TIMEOUT_MS 100U

static ProtocolFrame decode_single_frame(
    const uint8_t *wire,
    size_t wire_length
)
{
    ProtocolFrameDecoder decoder;
    ProtocolFrame copy;
    const ProtocolFrame *frame = NULL;
    bool complete = false;

    protocol_frame_decoder_init(&decoder);
    memset(&copy, 0, sizeof(copy));

    for (size_t index = 0U;
         index < wire_length;
         index++)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                wire[index],
                &frame
            ))
        {
            assert(!complete);
            assert(frame != NULL);

            copy = *frame;
            complete = true;
        }
    }

    assert(complete);

    return copy;
}

static void test_lost_ack_and_response(void)
{
    static const uint8_t command_payload[] =
        { 0x10U, 0x20U, 0x30U };

    uint8_t application_result[] =
        { 0xAAU, 0xBBU, 0xCCU };

    ProtocolSenderSession sender;
    ProtocolReceiverSession receiver;
    ProtocolReceiverDecision decision;

    const uint8_t *sender_wire;
    size_t sender_wire_length;
    uint16_t sender_sequence;

    uint8_t original_command_wire[
        PROTOCOL_FRAME_MAX_SIZE
    ];

    size_t original_command_length;

    uint8_t dropped_ack_wire[
        PROTOCOL_FRAME_MAX_SIZE
    ];

    size_t dropped_ack_length;

    uint8_t dropped_response_wire[
        PROTOCOL_FRAME_MAX_SIZE
    ];

    size_t dropped_response_length;

    uint8_t resent_response_wire[
        PROTOCOL_FRAME_MAX_SIZE
    ];

    size_t resent_response_length;

    assert(protocol_sender_session_init(
        &sender,
        TEST_SEQUENCE,
        TEST_TIMEOUT_MS,
        3U
    ));

    protocol_receiver_session_init(&receiver);

    assert(protocol_sender_session_start_command(
        &sender,
        command_payload,
        sizeof(command_payload)
    ));

    assert(protocol_sender_session_get_transmission(
        &sender,
        &sender_wire,
        &sender_wire_length,
        &sender_sequence
    ));

    assert(sender_sequence == TEST_SEQUENCE);

    original_command_length =
        sender_wire_length;

    memcpy(
        original_command_wire,
        sender_wire,
        sender_wire_length
    );

    assert(protocol_sender_session_mark_accepted(
        &sender
    ));

    assert(protocol_sender_session_mark_transmitted(
        &sender,
        0U
    ));

    /*
     * Receiver отримує першу передачу.
     */
    ProtocolFrame command =
        decode_single_frame(
            original_command_wire,
            original_command_length
        );

    assert(protocol_receiver_session_handle_frame(
        &receiver,
        &command,
        &decision
    ));

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_EXECUTE
    );

    assert(receiver.execute_count == 1U);
    assert(receiver.pending_valid);

    /*
     * ACK ACCEPTED успішно формується,
     * але мережа його втрачає.
     */
    assert(
        protocol_session_route_receiver_decision(
            &decision,
            dropped_ack_wire,
            sizeof(dropped_ack_wire),
            &dropped_ack_length
        ) == PROTOCOL_ROUTE_FRAME_READY
    );

    assert(dropped_ack_length > 0U);

    /*
     * Application виконує команду один раз.
     */
    assert(protocol_receiver_session_complete(
        &receiver,
        TEST_SEQUENCE,
        0U,
        application_result,
        sizeof(application_result)
    ));

    assert(!receiver.pending_valid);

    /*
     * Початковий terminal RESPONSE також
     * успішно формується, але губиться.
     */
    assert(protocol_feedback_encode_response(
        TEST_SEQUENCE,
        0U,
        application_result,
        sizeof(application_result),
        dropped_response_wire,
        sizeof(dropped_response_wire),
        &dropped_response_length
    ));

    /*
     * Змінюємо application buffer.
     * Receiver cache має залишитися незмінним.
     */
    application_result[0] = 0xFFU;

    /*
     * Sender не отримав ні ACK, ні RESPONSE.
     */
    protocol_sender_session_poll(
        &sender,
        TEST_TIMEOUT_MS
    );

    assert(
        sender.state ==
        PROTOCOL_SENDER_STATE_READY_TO_SEND
    );

    assert(sender.timeout_count == 1U);

    /*
     * Retry має містити точно ті самі
     * wire bytes та sequence.
     */
    assert(protocol_sender_session_get_transmission(
        &sender,
        &sender_wire,
        &sender_wire_length,
        &sender_sequence
    ));

    assert(sender_sequence == TEST_SEQUENCE);

    assert(sender_wire_length ==
           original_command_length);

    assert(
        memcmp(
            sender_wire,
            original_command_wire,
            original_command_length
        ) == 0
    );

    assert(protocol_sender_session_mark_accepted(
        &sender
    ));

    assert(protocol_sender_session_mark_transmitted(
        &sender,
        200U
    ));

    /*
     * Receiver отримує duplicate command.
     */
    command = decode_single_frame(
        sender_wire,
        sender_wire_length
    );

    assert(protocol_receiver_session_handle_frame(
        &receiver,
        &command,
        &decision
    ));

    assert(
        decision.action ==
        PROTOCOL_RECEIVER_ACTION_RESEND_RESULT
    );

    /*
     * Команда не виконується вдруге.
     */
    assert(receiver.execute_count == 1U);
    assert(receiver.resent_result_count == 1U);

    assert(decision.result_data_length == 3U);
    assert(decision.result_data[0] == 0xAAU);
    assert(decision.result_data[1] == 0xBBU);
    assert(decision.result_data[2] == 0xCCU);

    /*
     * Router повторно формує cached RESPONSE.
     */
    assert(
        protocol_session_route_receiver_decision(
            &decision,
            resent_response_wire,
            sizeof(resent_response_wire),
            &resent_response_length
        ) == PROTOCOL_ROUTE_FRAME_READY
    );

    /*
     * Повторний RESPONSE має бути
     * wire-identical початковому.
     */
    assert(resent_response_length ==
           dropped_response_length);

    assert(
        memcmp(
            resent_response_wire,
            dropped_response_wire,
            dropped_response_length
        ) == 0
    );

    ProtocolFrame response =
        decode_single_frame(
            resent_response_wire,
            resent_response_length
        );

    assert(
        protocol_session_route_sender_feedback(
            &sender,
            &response,
            220U
        ) == PROTOCOL_ROUTE_FEEDBACK_HANDLED
    );

    assert(
        sender.state ==
        PROTOCOL_SENDER_STATE_RESULT_READY
    );

    assert(sender.result_code == 0U);
    assert(sender.result_data_length == 3U);
    assert(sender.result_data[0] == 0xAAU);
    assert(sender.result_data[1] == 0xBBU);
    assert(sender.result_data[2] == 0xCCU);

    assert(sender.attempt_count == 2U);
    assert(sender.retry_count == 1U);

    assert(
        receiver.command_tracker
            .last_accepted_sequence ==
        TEST_SEQUENCE
    );

    assert(
        receiver.command_tracker
            .expected_sequence ==
        TEST_SEQUENCE + 1U
    );

    assert(protocol_sender_session_release_result(
        &sender
    ));

    assert(
        sender.state ==
        PROTOCOL_SENDER_STATE_IDLE
    );

    assert(sender.next_sequence ==
           TEST_SEQUENCE + 1U);
}

int main(void)
{
    test_lost_ack_and_response();

    puts("All protocol end-to-end tests passed.");
    return 0;
}
