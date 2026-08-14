/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Persistent WebSocket client: connect/upgrade/hello, then send/recv frames.
 */

#ifndef AI_WS_CLIENT_H
#define AI_WS_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "ai_app_config.h"
#include "ai_protocol.h"

#define AI_WS_OPCODE_TEXT   0x1
#define AI_WS_OPCODE_BINARY 0x2
#define AI_WS_OPCODE_CLOSE  0x8
#define AI_WS_OPCODE_PING   0x9
#define AI_WS_OPCODE_PONG   0xA

typedef struct {
    char trace_id[AI_PROTOCOL_TRACE_ID_LEN + 1];
    char session_id[AI_PROTOCOL_SESSION_ID_MAX];
} ai_ws_client_result_t;

int ai_ws_client_connect(const ai_app_config_t *cfg, const char *token,
                         ai_ws_client_result_t *result);
int ai_ws_client_send_text(const char *text);
int ai_ws_client_send_binary(const uint8_t *data, int len);
int ai_ws_client_recv_frame(uint8_t *buf, int buf_size, int *opcode,
                            int timeout_ms);
void ai_ws_client_close(void);
bool ai_ws_client_is_connected(void);
void ai_ws_client_flush_rx(void);
bool ai_ws_client_has_data(void);

#endif /* AI_WS_CLIENT_H */
