#include "protocol_feedback.h"
#include <string.h>

static bool protocol_feedback_ack_status_valid(ProtocolAckStatus status) {
	return (status == PROTOCOL_ACK_STATUS_ACCEPTED)
			|| (status == PROTOCOL_ACK_STATUS_IN_PROGRESS)
			|| (status == PROTOCOL_ACK_STATUS_OUT_OF_ORDER);
}

bool protocol_feedback_encode_ack(uint16_t sequence, ProtocolAckStatus status,
		uint16_t expected_sequence, uint8_t *output, size_t output_capacity,
		size_t *output_length) {
	uint8_t payload[PROTOCOL_ACK_OUT_OF_ORDER_PAYLOAD_SIZE];

	size_t payload_length;

	if (!protocol_feedback_ack_status_valid(status)) {
		return false;
	}

	payload[0] = (uint8_t) status;

	if (status == PROTOCOL_ACK_STATUS_OUT_OF_ORDER) {
		payload[1] = (uint8_t) (expected_sequence >> 8);

		payload[2] = (uint8_t) expected_sequence;

		payload_length =
		PROTOCOL_ACK_OUT_OF_ORDER_PAYLOAD_SIZE;
	} else {
		payload_length =
		PROTOCOL_ACK_PAYLOAD_SIZE;
	}

	return protocol_frame_encode(
	PROTOCOL_FRAME_TYPE_ACK, sequence, payload, payload_length, output,
			output_capacity, output_length);
}

bool protocol_feedback_decode_ack(const ProtocolFrame *frame,
		ProtocolAckStatus *status, uint16_t *expected_sequence) {
	ProtocolAckStatus decoded_status;
	uint16_t decoded_expected_sequence = 0U;

	if ((frame == NULL) || (status == NULL) || (expected_sequence == NULL)
			|| (frame->version !=
			PROTOCOL_FRAME_VERSION) || (frame->type !=
			PROTOCOL_FRAME_TYPE_ACK) || (frame->payload_length <
			PROTOCOL_ACK_PAYLOAD_SIZE)) {
		return false;
	}

	decoded_status = (ProtocolAckStatus) frame->payload[0];

	if (!protocol_feedback_ack_status_valid(decoded_status)) {
		return false;
	}

	if (decoded_status == PROTOCOL_ACK_STATUS_OUT_OF_ORDER) {
		if (frame->payload_length !=
		PROTOCOL_ACK_OUT_OF_ORDER_PAYLOAD_SIZE) {
			return false;
		}

		decoded_expected_sequence = ((uint16_t) frame->payload[1] << 8)
				| (uint16_t) frame->payload[2];
	} else if (frame->payload_length !=
	PROTOCOL_ACK_PAYLOAD_SIZE) {
		return false;
	}

	/*
	 * Output-параметри змінюємо лише після
	 * повної перевірки frame.
	 */
	*status = decoded_status;
	*expected_sequence = decoded_expected_sequence;

	return true;
}

bool protocol_feedback_encode_response(uint16_t sequence, uint8_t result_code,
		const uint8_t *result_data, size_t result_data_length, uint8_t *output,
		size_t output_capacity, size_t *output_length) {
	uint8_t payload[PROTOCOL_FRAME_MAX_PAYLOAD_SIZE];

	size_t payload_length;

	if ((result_data_length >
	PROTOCOL_RESPONSE_MAX_DATA_SIZE)
			|| ((result_data_length > 0U) && (result_data == NULL))) {
		return false;
	}

	payload[0] = result_code;

	if (result_data_length > 0U) {
		memcpy(&payload[
		PROTOCOL_RESPONSE_RESULT_CODE_SIZE], result_data, result_data_length);
	}

	payload_length =
	PROTOCOL_RESPONSE_RESULT_CODE_SIZE + result_data_length;

	return protocol_frame_encode(
	PROTOCOL_FRAME_TYPE_RESPONSE, sequence, payload, payload_length, output,
			output_capacity, output_length);
}

bool protocol_feedback_decode_response(const ProtocolFrame *frame,
		uint8_t *result_code, const uint8_t **result_data,
		size_t *result_data_length) {
	uint8_t decoded_result_code;
	const uint8_t *decoded_result_data;
	size_t decoded_result_data_length;

	if ((frame == NULL) || (result_code == NULL) || (result_data == NULL)
			|| (result_data_length == NULL) || (frame->version !=
			PROTOCOL_FRAME_VERSION) || (frame->type !=
			PROTOCOL_FRAME_TYPE_RESPONSE) || (frame->payload_length <
			PROTOCOL_RESPONSE_RESULT_CODE_SIZE) || (frame->payload_length >
			PROTOCOL_FRAME_MAX_PAYLOAD_SIZE)) {
		return false;
	}

	decoded_result_code = frame->payload[0];

	decoded_result_data_length = frame->payload_length -
	PROTOCOL_RESPONSE_RESULT_CODE_SIZE;

	if (decoded_result_data_length == 0U) {
		decoded_result_data = NULL;
	} else {
		decoded_result_data = &frame->payload[
		PROTOCOL_RESPONSE_RESULT_CODE_SIZE];
	}

	/*
	 * Borrowed pointer залишається дійсним
	 * лише доки caller не дозволить decoder-у
	 * повторно використати цей ProtocolFrame.
	 */
	*result_code = decoded_result_code;
	*result_data = decoded_result_data;
	*result_data_length = decoded_result_data_length;

	return true;
}
