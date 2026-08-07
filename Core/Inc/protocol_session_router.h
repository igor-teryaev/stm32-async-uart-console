#ifndef PROTOCOL_SESSION_ROUTER_H
#define PROTOCOL_SESSION_ROUTER_H

#include "protocol_receiver_session.h"
#include "protocol_sender_session.h"

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    PROTOCOL_ROUTE_ERROR,
    PROTOCOL_ROUTE_NO_RESPONSE,
    PROTOCOL_ROUTE_FRAME_READY,
    PROTOCOL_ROUTE_FEEDBACK_HANDLED,
    PROTOCOL_ROUTE_FEEDBACK_IGNORED
} ProtocolRouteResult;

ProtocolRouteResult
protocol_session_route_receiver_decision(
    const ProtocolReceiverDecision *decision,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

ProtocolRouteResult
protocol_session_route_sender_feedback(
    ProtocolSenderSession *session,
    const ProtocolFrame *frame,
    uint32_t now_ms
);

#endif
