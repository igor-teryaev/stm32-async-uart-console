#include "protocol_feedback.h"
#include "protocol_frame_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ProtocolFrame decode_single_frame(const uint8_t *wire_data,
		size_t wire_length) {
	ProtocolFrameDecoder decoder;
	ProtocolFrame decoded_copy;
	const ProtocolFrame *decoded_frame = NULL;
	bool completed = false;

	protocol_frame_decoder_init(&decoder);

	memset(&decoded_copy, 0, sizeof(decoded_copy));

	for (size_t index = 0U; index < wire_length; index++) {
		if (protocol_frame_decoder_feed_byte(&decoder, wire_data[index],
				&decoded_frame)) {
			assert(!completed);
			assert(decoded_frame != NULL);

			decoded_copy = *decoded_frame;
			completed = true;
		}
	}

	assert(completed);

	return decoded_copy;
}

static void test_accepted_ack(void) {
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	ProtocolAckStatus status;
	uint16_t expected_sequence;

	assert(
			protocol_feedback_encode_ack(42U, PROTOCOL_ACK_STATUS_ACCEPTED,
					999U, wire, sizeof(wire), &wire_length));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(frame.type == PROTOCOL_FRAME_TYPE_ACK);
	assert(frame.sequence == 42U);
	assert(frame.payload_length == PROTOCOL_ACK_PAYLOAD_SIZE);

	assert(protocol_feedback_decode_ack(&frame, &status, &expected_sequence));

	assert(status == PROTOCOL_ACK_STATUS_ACCEPTED);

	/*
	 * expected_sequence не застосовується
	 * для ACCEPTED.
	 */
	assert(expected_sequence == 0U);
}

static void test_in_progress_ack(void) {
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	ProtocolAckStatus status;
	uint16_t expected_sequence;

	assert(
			protocol_feedback_encode_ack(0x1234U,
					PROTOCOL_ACK_STATUS_IN_PROGRESS, 0U, wire, sizeof(wire),
					&wire_length));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(frame.sequence == 0x1234U);
	assert(frame.payload[0] == PROTOCOL_ACK_STATUS_IN_PROGRESS);

	assert(protocol_feedback_decode_ack(&frame, &status, &expected_sequence));

	assert(status == PROTOCOL_ACK_STATUS_IN_PROGRESS);

	assert(expected_sequence == 0U);
}

static void test_out_of_order_ack(void) {
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	ProtocolAckStatus status;
	uint16_t expected_sequence;

	assert(
			protocol_feedback_encode_ack(43U, PROTOCOL_ACK_STATUS_OUT_OF_ORDER,
					0xBEEFU, wire, sizeof(wire), &wire_length));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(frame.sequence == 43U);

	assert(frame.payload_length == PROTOCOL_ACK_OUT_OF_ORDER_PAYLOAD_SIZE);

	assert(frame.payload[0] == PROTOCOL_ACK_STATUS_OUT_OF_ORDER);

	assert(frame.payload[1] == 0xBEU);
	assert(frame.payload[2] == 0xEFU);

	assert(protocol_feedback_decode_ack(&frame, &status, &expected_sequence));

	assert(status == PROTOCOL_ACK_STATUS_OUT_OF_ORDER);

	assert(expected_sequence == 0xBEEFU);
}

static void test_invalid_ack_rejected_atomically(void) {
	ProtocolFrame frame;
	ProtocolAckStatus status = PROTOCOL_ACK_STATUS_ACCEPTED;

	uint16_t expected_sequence = 0xBEEFU;
	size_t output_length = 0U;

	memset(&frame, 0, sizeof(frame));

	frame.version = PROTOCOL_FRAME_VERSION;
	frame.type = PROTOCOL_FRAME_TYPE_ACK;
	frame.sequence = 42U;
	frame.payload_length = 2U;
	frame.payload[0] = PROTOCOL_ACK_STATUS_IN_PROGRESS;

	assert(!protocol_feedback_decode_ack(&frame, &status, &expected_sequence));

	/*
	 * Невдала операція не змінює outputs.
	 */
	assert(status == PROTOCOL_ACK_STATUS_ACCEPTED);

	assert(expected_sequence == 0xBEEFU);

	frame.payload_length = 1U;
	frame.payload[0] = 0x7FU;

	assert(!protocol_feedback_decode_ack(&frame, &status, &expected_sequence));

	assert(
			!protocol_feedback_encode_ack(42U, (ProtocolAckStatus )0x7FU, 0U,
					frame.payload, sizeof(frame.payload), &output_length));
}

static void test_response_without_data(void) {
	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	uint8_t result_code;
	const uint8_t *result_data;
	size_t result_data_length;

	assert(
			protocol_feedback_encode_response( 42U, 7U, NULL, 0U, wire, sizeof(wire), &wire_length ));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(frame.type == PROTOCOL_FRAME_TYPE_RESPONSE);

	assert(frame.sequence == 42U);
	assert(frame.payload_length == 1U);
	assert(frame.payload[0] == 7U);

	assert(
			protocol_feedback_decode_response(&frame, &result_code,
					&result_data, &result_data_length));

	assert(result_code == 7U);
	assert(result_data == NULL);
	assert(result_data_length == 0U);
}

static void test_response_with_data(void) {
	static const uint8_t source_data[] = { 0x10U, 0x20U, 0x30U, 0x40U };

	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	uint8_t result_code;
	const uint8_t *result_data;
	size_t result_data_length;

	assert(
			protocol_feedback_encode_response(0x1234U, 0U, source_data,
					sizeof(source_data), wire, sizeof(wire), &wire_length));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(
			protocol_feedback_decode_response(&frame, &result_code,
					&result_data, &result_data_length));

	assert(result_code == 0U);

	assert(result_data == &frame.payload[ PROTOCOL_RESPONSE_RESULT_CODE_SIZE ]);

	assert(result_data_length == sizeof(source_data));

	assert(memcmp(result_data, source_data, sizeof(source_data)) == 0);
}

static void test_maximum_response_data(void) {
	uint8_t source_data[PROTOCOL_RESPONSE_MAX_DATA_SIZE];

	uint8_t wire[PROTOCOL_FRAME_MAX_SIZE];
	size_t wire_length;
	uint8_t result_code;
	const uint8_t *result_data;
	size_t result_data_length;

	for (size_t index = 0U; index < sizeof(source_data); index++) {
		source_data[index] = (uint8_t) index;
	}

	assert(
			protocol_feedback_encode_response( UINT16_MAX, 0U, source_data, sizeof(source_data), wire, sizeof(wire), &wire_length ));

	ProtocolFrame frame = decode_single_frame(wire, wire_length);

	assert(frame.payload_length == PROTOCOL_FRAME_MAX_PAYLOAD_SIZE);

	assert(
			protocol_feedback_decode_response(&frame, &result_code,
					&result_data, &result_data_length));

	assert(result_data_length == PROTOCOL_RESPONSE_MAX_DATA_SIZE);

	assert(memcmp(result_data, source_data, sizeof(source_data)) == 0);
}

static void test_invalid_response_rejected_atomically(void) {
	ProtocolFrame frame;
	uint8_t result_code = 0xAAU;
	uint8_t sentinel = 0U;
	const uint8_t *result_data = &sentinel;
	size_t result_data_length = 123U;

	memset(&frame, 0, sizeof(frame));

	frame.version = PROTOCOL_FRAME_VERSION;
	frame.type = PROTOCOL_FRAME_TYPE_RESPONSE;
	frame.payload_length =
	PROTOCOL_FRAME_MAX_PAYLOAD_SIZE + 1U;

	assert(
			!protocol_feedback_decode_response(&frame, &result_code,
					&result_data, &result_data_length));

	assert(result_code == 0xAAU);
	assert(result_data == &sentinel);
	assert(result_data_length == 123U);

	frame.payload_length = 0U;

	assert(
			!protocol_feedback_decode_response(&frame, &result_code,
					&result_data, &result_data_length));

	assert(
			!protocol_feedback_encode_response( 42U, 0U, NULL, 1U, frame.payload, sizeof(frame.payload), &result_data_length ));
}

int main(void) {
	test_accepted_ack();
	test_in_progress_ack();
	test_out_of_order_ack();
	test_invalid_ack_rejected_atomically();
	test_response_without_data();
	test_response_with_data();
	test_maximum_response_data();
	test_invalid_response_rejected_atomically();

	puts("All protocol feedback tests passed.");
	return 0;
}
