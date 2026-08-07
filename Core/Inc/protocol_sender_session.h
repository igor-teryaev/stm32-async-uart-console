#ifndef PROTOCOL_SENDER_SESSION_H
#define PROTOCOL_SENDER_SESSION_H

#include "protocol_feedback.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	PROTOCOL_SENDER_STATE_IDLE,
	PROTOCOL_SENDER_STATE_READY_TO_SEND,
	PROTOCOL_SENDER_STATE_SENDING,
	PROTOCOL_SENDER_STATE_WAITING_RESULT,
	PROTOCOL_SENDER_STATE_RESULT_READY,
	PROTOCOL_SENDER_STATE_COMMUNICATION_FAILED,
	PROTOCOL_SENDER_STATE_DESYNCHRONIZED
} ProtocolSenderState;

typedef struct {
	ProtocolSenderState state;
	bool transmission_active;
	uint16_t next_sequence;
	uint16_t pending_sequence;
	uint16_t receiver_expected_sequence;

	uint8_t pending_frame[PROTOCOL_FRAME_MAX_SIZE];
	size_t pending_frame_length;

	uint32_t last_activity_ms;
	uint32_t response_timeout_ms;

	uint8_t attempt_count;
	uint8_t max_attempts;

	uint8_t result_code;
	uint8_t result_data[PROTOCOL_RESPONSE_MAX_DATA_SIZE];
	size_t result_data_length;

	uint32_t command_count;
	uint32_t transmission_count;
	uint32_t retry_count;
	uint32_t timeout_count;
	uint32_t in_progress_count;
	uint32_t completed_count;
	uint32_t communication_failure_count;
	uint32_t out_of_order_count;
	uint32_t accepted_ack_count;
} ProtocolSenderSession;
// задає конфігурацію та початковий sequence;
bool protocol_sender_session_init(ProtocolSenderSession *session,
		uint16_t initial_sequence, uint32_t response_timeout_ms,
		uint8_t max_attempts);
// викликається лише після зовнішнього відновлення сесії
bool protocol_sender_session_reset(ProtocolSenderSession *session,
		uint16_t initial_sequence);
// кодує і зберігає новий COMMAND
bool protocol_sender_session_start_command(ProtocolSenderSession *session,
		const uint8_t *payload, size_t payload_length);
// надає збережений кадр transport-рівню
bool protocol_sender_session_get_transmission(
		const ProtocolSenderSession *session, const uint8_t **data,
		size_t *length, uint16_t *sequence);
// transport прийняв кадр, спроба витрачена
bool protocol_sender_session_mark_accepted(ProtocolSenderSession *session);
// останній байт переданий, запускається timeout
bool protocol_sender_session_mark_transmitted(ProtocolSenderSession *session,
		uint32_t now_ms);
// transport повідомив про помилку передачі
bool protocol_sender_session_mark_transmit_error(ProtocolSenderSession *session);
// перевіряє timeout без блокування
void protocol_sender_session_poll(ProtocolSenderSession *session,
		uint32_t now_ms);
bool protocol_sender_session_handle_accepted(
    ProtocolSenderSession *session,
    uint16_t sequence,
    uint32_t now_ms
);
// поновлює timeout для правильного sequence
bool protocol_sender_session_handle_in_progress(ProtocolSenderSession *session,
		uint16_t sequence, uint32_t now_ms);
// зберігає terminal result
bool protocol_sender_session_handle_result(
    ProtocolSenderSession *session,
    uint16_t sequence,
    uint8_t result_code,
    const uint8_t *result_data,
    size_t result_data_length
);
// application забрала результат, можна перейти в IDLE
bool protocol_sender_session_release_result(ProtocolSenderSession *session);
// якщо прийшла команда з невірним Sequence
bool protocol_sender_session_handle_out_of_order(ProtocolSenderSession *session,
		uint16_t sequence, uint16_t expected_sequence);
#endif
