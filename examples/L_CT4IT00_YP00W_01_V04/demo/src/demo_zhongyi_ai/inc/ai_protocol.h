/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hangyan AI platform protocol: message builders and parsers.
 */

#ifndef AI_PROTOCOL_H
#define AI_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

#define AI_PROTOCOL_TRACE_ID_LEN 32
#define AI_PROTOCOL_SESSION_ID_MAX 128

bool ai_protocol_extract_session_id(const char *json,
                                    size_t len,
                                    char *session_id,
                                    size_t session_id_size);
bool ai_protocol_is_hello_ack(const char *json, size_t len);
bool ai_protocol_is_pong(const char *json, size_t len);
bool ai_protocol_is_tts_start(const char *json, size_t len);
bool ai_protocol_is_tts_stop(const char *json, size_t len);
bool ai_protocol_is_stt_end(const char *json, size_t len);
bool ai_protocol_extract_error(const char *json,
                               size_t len,
                               char *state,
                               size_t state_size,
                               char *message,
                               size_t message_size,
                               int *code);

char *ai_protocol_build_hello(const char *session_id, const char *trace_id);
char *ai_protocol_build_ping(const char *session_id);
char *ai_protocol_build_wake(const char *session_id, const char *wake_word);
char *ai_protocol_build_listen_start(const char *session_id, const char *mode);
char *ai_protocol_build_listen_stop(const char *session_id);
void ai_protocol_free_json(char *json);

#endif /* AI_PROTOCOL_H */
