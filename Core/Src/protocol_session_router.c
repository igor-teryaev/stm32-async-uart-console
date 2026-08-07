#include "protocol_session_router.h"

#include "protocol_feedback.h"

ProtocolRouteResult protocol_session_route_receiver_decision(
		const ProtocolReceiverDecision *decision, uint8_t *output,
		size_t output_capacity, size_t *output_length) {
	size_t encoded_length;
	bool encoded;

	if ((decision == NULL) || (output_length == NULL)) {
		return PROTOCOL_ROUTE_ERROR;
	}

	if (decision->action == PROTOCOL_RECEIVER_ACTION_IGNORED) {
		*output_length = 0U;

		return PROTOCOL_ROUTE_NO_RESPONSE;
	}

	if (output == NULL) {
		return PROTOCOL_ROUTE_ERROR;
	}

	switch (decision->action) {
	case PROTOCOL_RECEIVER_ACTION_EXECUTE:
		encoded = protocol_feedback_encode_ack(decision->sequence,
				PROTOCOL_ACK_STATUS_ACCEPTED, 0U, output, output_capacity,
				&encoded_length);
		break;

	case PROTOCOL_RECEIVER_ACTION_IN_PROGRESS:
		encoded = protocol_feedback_encode_ack(decision->sequence,
				PROTOCOL_ACK_STATUS_IN_PROGRESS, 0U, output, output_capacity,
				&encoded_length);
		break;

	case PROTOCOL_RECEIVER_ACTION_RESEND_RESULT:
		encoded = protocol_feedback_encode_response(decision->sequence,
				decision->result_code,
				decision->result_data,
				decision->result_data_length, output, output_capacity, &encoded_length);
		break;

	case PROTOCOL_RECEIVER_ACTION_OUT_OF_ORDER:
		encoded = protocol_feedback_encode_ack(decision->sequence,
				PROTOCOL_ACK_STATUS_OUT_OF_ORDER, decision->expected_sequence,
				output, output_capacity, &encoded_length);
		break;

	case PROTOCOL_RECEIVER_ACTION_INVALID:
	default:
		return PROTOCOL_ROUTE_ERROR;
	}

	if (!encoded) {
		return PROTOCOL_ROUTE_ERROR;
	}

	/*
	 * Довжину публікуємо лише після
	 * успішного кодування всього frame.
	 */
	*output_length = encoded_length;

	return PROTOCOL_ROUTE_FRAME_READY;
}

ProtocolRouteResult protocol_session_route_sender_feedback(
		ProtocolSenderSession *session, const ProtocolFrame *frame,
		uint32_t now_ms) {
	bool handled;

	if ((session == NULL) || (frame == NULL)) {
		return PROTOCOL_ROUTE_ERROR;
	}

	if (frame->type ==
	PROTOCOL_FRAME_TYPE_ACK) {
		ProtocolAckStatus status;
		uint16_t expected_sequence;

		if (!protocol_feedback_decode_ack(frame, &status, &expected_sequence)) {
			return PROTOCOL_ROUTE_ERROR;
		}

		switch (status) {
		case PROTOCOL_ACK_STATUS_ACCEPTED:
			handled = protocol_sender_session_handle_accepted(session,
					frame->sequence, now_ms);
			break;

		case PROTOCOL_ACK_STATUS_IN_PROGRESS:
			handled = protocol_sender_session_handle_in_progress(session,
					frame->sequence, now_ms);
			break;

		case PROTOCOL_ACK_STATUS_OUT_OF_ORDER:
			handled = protocol_sender_session_handle_out_of_order(session,
					frame->sequence, expected_sequence);
			break;

		default:
			return PROTOCOL_ROUTE_ERROR;
		}

		return handled ?
				PROTOCOL_ROUTE_FEEDBACK_HANDLED :
				PROTOCOL_ROUTE_FEEDBACK_IGNORED;
	}

	if (frame->type ==
	PROTOCOL_FRAME_TYPE_RESPONSE) {
		uint8_t result_code;
		const uint8_t *result_data;
		size_t result_data_length;

		if (!protocol_feedback_decode_response(frame, &result_code,
				&result_data, &result_data_length)) {
			return PROTOCOL_ROUTE_ERROR;
		}

		handled = protocol_sender_session_handle_result(session,
				frame->sequence, result_code, result_data, result_data_length);

		return handled ?
				PROTOCOL_ROUTE_FEEDBACK_HANDLED :
				PROTOCOL_ROUTE_FEEDBACK_IGNORED;
	}

	return PROTOCOL_ROUTE_FEEDBACK_IGNORED;
}
