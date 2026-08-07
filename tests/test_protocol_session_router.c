#include "protocol_feedback.h"
#include "protocol_frame_decoder.h"
#include "protocol_session_router.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_SEQUENCE 42U

static ProtocolFrame decode_single_frame(const uint8_t *wire,
		size_t wire_length) {
	ProtocolFrameDecoder decoder;
	ProtocolFrame copy;
	const ProtocolFrame *frame = NULL;
	bool complete = false;

	protocol_frame_decoder_init(&decoder);
	memset(&copy, 0, sizeof(copy));

	for (size_t index = 0U; index < wire_length; index++) {
		if (protocol_frame_decoder_feed_byte(&decoder, wire[index], &frame)) {
			assert(!complete);
			assert(frame != NULL);

			copy = *frame;
			complete = true;
		}
	}

	assert(complete);

	return copy;
}

static ProtocolFrame route_receiver_decision(
		const ProtocolReceiverDecision *decision) {
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;

	assert(
			protocol_session_route_receiver_decision(decision, wire,
					sizeof(wire), &wire_length) == PROTOCOL_ROUTE_FRAME_READY);

	return decode_single_frame(wire, wire_length);
}

static void prepare_waiting_sender(ProtocolSenderSession *session) {
	assert(protocol_sender_session_init( session, TEST_SEQUENCE, 100U, 3U ));

	assert(protocol_sender_session_start_command( session, NULL, 0U ));

	assert(protocol_sender_session_mark_accepted(session));

	assert(protocol_sender_session_mark_transmitted(session, 100U));
}

static void test_execute_routes_to_accepted_ack(void) {
	ProtocolSenderSession sender;
	ProtocolReceiverDecision decision;

	prepare_waiting_sender(&sender);

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_EXECUTE;

	decision.sequence = TEST_SEQUENCE;

	ProtocolFrame ack = route_receiver_decision(&decision);

	assert(ack.type == PROTOCOL_FRAME_TYPE_ACK);

	assert(
			protocol_session_route_sender_feedback(&sender, &ack, 150U)
					== PROTOCOL_ROUTE_FEEDBACK_HANDLED);

	assert(sender.state == PROTOCOL_SENDER_STATE_WAITING_RESULT);

	assert(sender.last_activity_ms == 150U);
	assert(sender.accepted_ack_count == 1U);
	assert(sender.in_progress_count == 0U);
}

static void test_in_progress_routes_to_sender(void) {
	ProtocolSenderSession sender;
	ProtocolReceiverDecision decision;

	prepare_waiting_sender(&sender);

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_IN_PROGRESS;

	decision.sequence = TEST_SEQUENCE;

	ProtocolFrame ack = route_receiver_decision(&decision);

	ProtocolAckStatus status;
	uint16_t expected_sequence;

	assert(protocol_feedback_decode_ack(&ack, &status, &expected_sequence));

	assert(status == PROTOCOL_ACK_STATUS_IN_PROGRESS);

	assert(
			protocol_session_route_sender_feedback(&sender, &ack, 170U)
					== PROTOCOL_ROUTE_FEEDBACK_HANDLED);

	assert(sender.last_activity_ms == 170U);
	assert(sender.accepted_ack_count == 0U);
	assert(sender.in_progress_count == 1U);
}

static void test_cached_result_routes_to_sender(void) {
	ProtocolSenderSession sender;
	ProtocolReceiverDecision decision;

	prepare_waiting_sender(&sender);

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_RESEND_RESULT;

	decision.sequence = TEST_SEQUENCE;
	decision.result_code = 7U;

	ProtocolFrame response = route_receiver_decision(&decision);

	assert(response.type == PROTOCOL_FRAME_TYPE_RESPONSE);

	assert(
			protocol_session_route_sender_feedback(&sender, &response, 180U)
					== PROTOCOL_ROUTE_FEEDBACK_HANDLED);

	assert(sender.state == PROTOCOL_SENDER_STATE_RESULT_READY);

	assert(sender.result_code == 7U);
	assert(sender.result_data_length == 0U);
}

static void test_out_of_order_desynchronizes_sender(void) {
	ProtocolSenderSession sender;
	ProtocolReceiverDecision decision;

	prepare_waiting_sender(&sender);

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER;

	decision.sequence = TEST_SEQUENCE;
	decision.expected_sequence =
	TEST_SEQUENCE - 1U;

	ProtocolFrame ack = route_receiver_decision(&decision);

	assert(
			protocol_session_route_sender_feedback(&sender, &ack, 180U)
					== PROTOCOL_ROUTE_FEEDBACK_HANDLED);

	assert(sender.state == PROTOCOL_SENDER_STATE_DESYNCHRONIZED);

	assert(sender.receiver_expected_sequence == TEST_SEQUENCE - 1U);

	assert(sender.out_of_order_count == 1U);
}

static void test_response_data_is_copied(void) {
	static const uint8_t source_data[] = { 0x11U, 0x22U, 0x33U };

	ProtocolSenderSession sender;
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;

	prepare_waiting_sender(&sender);

	assert(
			protocol_feedback_encode_response( TEST_SEQUENCE, 0U, source_data, sizeof(source_data), wire, sizeof(wire), &wire_length ));

	ProtocolFrame response = decode_single_frame(wire, wire_length);

	assert(
			protocol_session_route_sender_feedback(&sender, &response, 180U)
					== PROTOCOL_ROUTE_FEEDBACK_HANDLED);

	/*
	 * Імітуємо повторне використання
	 * decoder frame storage.
	 */
	memset(response.payload, 0xFF, sizeof(response.payload));

	assert(sender.result_data_length == sizeof(source_data));

	assert(memcmp(sender.result_data, source_data, sizeof(source_data)) == 0);
}

static void test_foreign_feedback_is_ignored(void) {
	ProtocolSenderSession sender;
	ProtocolReceiverDecision decision;

	prepare_waiting_sender(&sender);

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_IN_PROGRESS;

	decision.sequence = TEST_SEQUENCE + 1U;

	ProtocolFrame ack = route_receiver_decision(&decision);

	assert(
			protocol_session_route_sender_feedback(&sender, &ack, 180U)
					== PROTOCOL_ROUTE_FEEDBACK_IGNORED);

	assert(sender.state == PROTOCOL_SENDER_STATE_WAITING_RESULT);

	assert(sender.in_progress_count == 0U);
}

static void test_ignored_receiver_decision(void) {
	ProtocolReceiverDecision decision;
	size_t output_length = 123U;

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_IGNORED;

	assert(
			protocol_session_route_receiver_decision( &decision, NULL, 0U, &output_length ) == PROTOCOL_ROUTE_NO_RESPONSE);

	assert(output_length == 0U);
}

static void test_routing_errors_are_distinct(void) {
	ProtocolReceiverDecision decision;
	ProtocolSenderSession sender;
	ProtocolFrame malformed_ack;
	uint8_t output[1];
	size_t output_length = 123U;

	memset(&decision, 0, sizeof(decision));

	decision.action = PROTOCOL_RECEIVER_ACTION_EXECUTE;

	decision.sequence = TEST_SEQUENCE;

	assert(
			protocol_session_route_receiver_decision(&decision, output,
					sizeof(output), &output_length) == PROTOCOL_ROUTE_ERROR);

	/*
	 * Error не публікує часткову довжину.
	 */
	assert(output_length == 123U);

	prepare_waiting_sender(&sender);

	memset(&malformed_ack, 0, sizeof(malformed_ack));

	malformed_ack.version =
	PROTOCOL_FRAME_VERSION;

	malformed_ack.type =
	PROTOCOL_FRAME_TYPE_ACK;

	malformed_ack.sequence = TEST_SEQUENCE;
	malformed_ack.payload_length = 2U;

	malformed_ack.payload[0] = PROTOCOL_ACK_STATUS_ACCEPTED;

	assert(
			protocol_session_route_sender_feedback(&sender, &malformed_ack,
					180U) == PROTOCOL_ROUTE_ERROR);
}

static void test_non_feedback_frame_is_ignored(void) {
	ProtocolSenderSession sender;
	ProtocolFrame telemetry;

	prepare_waiting_sender(&sender);

	memset(&telemetry, 0, sizeof(telemetry));

	telemetry.version = PROTOCOL_FRAME_VERSION;
	telemetry.type =
	PROTOCOL_FRAME_TYPE_TELEMETRY;

	telemetry.sequence = TEST_SEQUENCE;

	assert(
			protocol_session_route_sender_feedback(&sender, &telemetry, 180U)
					== PROTOCOL_ROUTE_FEEDBACK_IGNORED);
}

int main(void) {
	test_execute_routes_to_accepted_ack();
	test_in_progress_routes_to_sender();
	test_cached_result_routes_to_sender();
	test_out_of_order_desynchronizes_sender();
	test_response_data_is_copied();
	test_foreign_feedback_is_ignored();
	test_ignored_receiver_decision();
	test_routing_errors_are_distinct();
	test_non_feedback_frame_is_ignored();

	puts("All protocol session router tests passed.");
	return 0;
}
