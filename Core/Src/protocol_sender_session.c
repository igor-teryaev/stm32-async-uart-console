#include "protocol_sender_session.h"

#include <string.h>

static void protocol_sender_session_clear_operation(
		ProtocolSenderSession *session, uint16_t initial_sequence) {
	session->state = PROTOCOL_SENDER_STATE_IDLE;
	session->transmission_active = false;
	session->next_sequence = initial_sequence;
	session->pending_sequence = 0U;

	session->pending_frame_length = 0U;
	session->last_activity_ms = 0U;
	session->attempt_count = 0U;
	session->result_code = 0U;
	session->receiver_expected_sequence = 0U;
	session->result_data_length = 0U;
}

bool protocol_sender_session_init(ProtocolSenderSession *session,
		uint16_t initial_sequence, uint32_t response_timeout_ms,
		uint8_t max_attempts) {
	if ((session == NULL) || (response_timeout_ms == 0U)
			|| (response_timeout_ms > (UINT32_MAX / 2U))
			|| (max_attempts == 0U)) {
		return false;
	}

	memset(session, 0, sizeof(*session));

	session->response_timeout_ms = response_timeout_ms;

	session->max_attempts = max_attempts;

	protocol_sender_session_clear_operation(session, initial_sequence);

	return true;
}

bool protocol_sender_session_reset(ProtocolSenderSession *session,
		uint16_t initial_sequence) {

	if ((session == NULL) || session->transmission_active) {
		return false;
	}

	/*
	 * Конфігурація та накопичувальні діагностичні
	 * лічильники навмисно зберігаються.
	 *
	 * Перед скиданням код, що викликає функцію,
	 * повинен зовнішнім механізмом відновити
	 * узгоджений стан із receiver.
	 */
	protocol_sender_session_clear_operation(session, initial_sequence);

	return true;
}

bool protocol_sender_session_start_command(ProtocolSenderSession *session,
		const uint8_t *payload, size_t payload_length) {

	size_t encoded_length;

	if ((session == NULL) || (session->state != PROTOCOL_SENDER_STATE_IDLE)) {
		return false;
	}

	if (!protocol_frame_encode(
	PROTOCOL_FRAME_TYPE_COMMAND, session->next_sequence, payload,
			payload_length, session->pending_frame,
			sizeof(session->pending_frame), &encoded_length)) {
		return false;
	}

	session->pending_sequence = session->next_sequence;

	session->pending_frame_length = encoded_length;

	session->attempt_count = 0U;
	session->result_code = 0U;

	session->state = PROTOCOL_SENDER_STATE_READY_TO_SEND;

	session->command_count++;

	return true;
}

bool protocol_sender_session_get_transmission(
		const ProtocolSenderSession *session, const uint8_t **data,
		size_t *length, uint16_t *sequence) {
	if ((session == NULL) || (data == NULL) || (length == NULL)
			|| (sequence == NULL)
			|| (session->state != PROTOCOL_SENDER_STATE_READY_TO_SEND)) {
		return false;
	}

	*data = session->pending_frame;
	*length = session->pending_frame_length;
	*sequence = session->pending_sequence;

	return true;
}

bool protocol_sender_session_mark_accepted(ProtocolSenderSession *session) {
	if ((session == NULL)
			|| (session->state != PROTOCOL_SENDER_STATE_READY_TO_SEND)
			|| (session->attempt_count >= session->max_attempts)) {
		return false;
	}

	if (session->attempt_count > 0U) {
		session->retry_count++;
	}

	session->attempt_count++;
	session->transmission_count++;

	session->transmission_active = true;
	session->state = PROTOCOL_SENDER_STATE_SENDING;

	return true;
}

bool protocol_sender_session_mark_transmitted(ProtocolSenderSession *session,
		uint32_t now_ms) {
	if ((session == NULL) || !session->transmission_active
			|| ((session->state != PROTOCOL_SENDER_STATE_SENDING)
					&& (session->state != PROTOCOL_SENDER_STATE_RESULT_READY)
					&& (session->state != PROTOCOL_SENDER_STATE_DESYNCHRONIZED))) {
		return false;
	}

	session->transmission_active = false;

	/*
	 * Terminal result міг надійти під час
	 * повторної фізичної передачі.
	 */
	if ((session->state == PROTOCOL_SENDER_STATE_RESULT_READY)
			|| (session->state == PROTOCOL_SENDER_STATE_DESYNCHRONIZED)) {
		return true;
	}

	session->last_activity_ms = now_ms;

	session->state = PROTOCOL_SENDER_STATE_WAITING_RESULT;

	return true;
}

bool protocol_sender_session_mark_transmit_error(ProtocolSenderSession *session) {
	if ((session == NULL) || !session->transmission_active
			|| ((session->state != PROTOCOL_SENDER_STATE_SENDING)
					&& (session->state != PROTOCOL_SENDER_STATE_RESULT_READY)
					&& (session->state != PROTOCOL_SENDER_STATE_DESYNCHRONIZED))) {
		return false;
	}

	session->transmission_active = false;

	/*
	 * Якщо terminal result уже отриманий,
	 * помилка зайвої повторної передачі
	 * не змінює відомий результат команди.
	 */
	if ((session->state == PROTOCOL_SENDER_STATE_RESULT_READY)
			|| (session->state == PROTOCOL_SENDER_STATE_DESYNCHRONIZED)) {
		return true;
	}

	if (session->attempt_count < session->max_attempts) {
		session->state = PROTOCOL_SENDER_STATE_READY_TO_SEND;
	} else {
		session->state = PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED;

		session->communication_failure_count++;
	}

	return true;
}

void protocol_sender_session_poll(ProtocolSenderSession *session,
		uint32_t now_ms) {
	uint32_t elapsed_ms;

	if ((session == NULL)
			|| (session->state != PROTOCOL_SENDER_STATE_WAITING_RESULT)) {
		return;
	}

	elapsed_ms = (uint32_t) (now_ms - session->last_activity_ms);

	if (elapsed_ms < session->response_timeout_ms) {
		return;
	}

	session->timeout_count++;

	if (session->attempt_count < session->max_attempts) {
		session->state = PROTOCOL_SENDER_STATE_READY_TO_SEND;
	} else {
		session->state = PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED;

		session->communication_failure_count++;
	}
}

static bool protocol_sender_session_handle_activity_feedback(
		ProtocolSenderSession *session, uint16_t sequence, uint32_t now_ms) {
	bool valid_state;

	if ((session == NULL) || (sequence != session->pending_sequence)
			|| (session->attempt_count == 0U)) {
		return false;
	}

	valid_state = (session->state == PROTOCOL_SENDER_STATE_WAITING_RESULT)
			|| (session->state == PROTOCOL_SENDER_STATE_READY_TO_SEND)
			|| ((session->state == PROTOCOL_SENDER_STATE_SENDING)
					&& (session->attempt_count > 1U));

	if (!valid_state) {
		return false;
	}

	/*
	 * Якщо retry ще лише запланований,
	 * запізнілий IN_PROGRESS скасовує його
	 * та поновлює очікування.
	 */
	if (session->state == PROTOCOL_SENDER_STATE_READY_TO_SEND) {
		session->last_activity_ms = now_ms;

		session->state = PROTOCOL_SENDER_STATE_WAITING_RESULT;

		return true;
	}

	/*
	 * Під час активної повторної передачі
	 * її завершення пізніше запустить новий
	 * timeout. Фізичну передачу не перериваємо.
	 */
	if (session->state == PROTOCOL_SENDER_STATE_SENDING) {
		return true;
	}

	session->last_activity_ms = now_ms;

	return true;
}

bool protocol_sender_session_handle_accepted(ProtocolSenderSession *session,
		uint16_t sequence, uint32_t now_ms) {
	if (!protocol_sender_session_handle_activity_feedback(session, sequence,
			now_ms)) {
		return false;
	}

	session->accepted_ack_count++;

	return true;
}

bool protocol_sender_session_handle_in_progress(ProtocolSenderSession *session,
		uint16_t sequence, uint32_t now_ms) {
	if (!protocol_sender_session_handle_activity_feedback(session, sequence,
			now_ms)) {
		return false;
	}

	session->in_progress_count++;

	return true;
}

bool protocol_sender_session_handle_result(ProtocolSenderSession *session,
		uint16_t sequence, uint8_t result_code, const uint8_t *result_data,
		size_t result_data_length) {
	bool valid_state;

	if ((session == NULL) || (sequence != session->pending_sequence)
			|| (session->attempt_count == 0U) || (result_data_length >
			PROTOCOL_RESPONSE_MAX_DATA_SIZE)
			|| ((result_data_length > 0U) && (result_data == NULL))) {
		return false;
	}

	valid_state = (session->state == PROTOCOL_SENDER_STATE_WAITING_RESULT)
			|| (session->state == PROTOCOL_SENDER_STATE_READY_TO_SEND)
			|| (session->state == PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED)
			|| ((session->state == PROTOCOL_SENDER_STATE_SENDING)
					&& (session->attempt_count > 1U));

	if (!valid_state) {
		return false;
	}

	if (result_data_length > 0U) {
		memcpy(session->result_data, result_data, result_data_length);
	}

	session->result_data_length = result_data_length;

	session->result_code = result_code;

	session->next_sequence = (uint16_t) (session->pending_sequence + 1U);

	session->state = PROTOCOL_SENDER_STATE_RESULT_READY;

	session->completed_count++;

	return true;
}

bool protocol_sender_session_release_result(ProtocolSenderSession *session) {
	uint16_t next_sequence;

	if ((session == NULL)
			|| (session->state != PROTOCOL_SENDER_STATE_RESULT_READY)
			|| session->transmission_active) {
		return false;
	}

	next_sequence = session->next_sequence;

	protocol_sender_session_clear_operation(session, next_sequence);

	return true;
}

bool protocol_sender_session_handle_out_of_order(ProtocolSenderSession *session,
		uint16_t sequence, uint16_t expected_sequence) {
	bool valid_state;

	if ((session == NULL) || (sequence != session->pending_sequence)
			|| (session->attempt_count == 0U)) {
		return false;
	}

	valid_state = (session->state == PROTOCOL_SENDER_STATE_WAITING_RESULT)
			|| (session->state == PROTOCOL_SENDER_STATE_READY_TO_SEND)
			|| (session->state == PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED)
			|| ((session->state == PROTOCOL_SENDER_STATE_SENDING)
					&& (session->attempt_count > 1U));

	if (!valid_state) {
		return false;
	}

	session->receiver_expected_sequence = expected_sequence;

	session->state = PROTOCOL_SENDER_STATE_DESYNCHRONIZED;

	session->out_of_order_count++;

	return true;
}
