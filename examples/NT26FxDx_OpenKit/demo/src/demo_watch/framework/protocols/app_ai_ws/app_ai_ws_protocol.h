#ifndef APP_AI_WS_PROTOCOL_H
#define APP_AI_WS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_AI_WS_ENDPOINT_MAX 160U
#define APP_AI_WS_TOKEN_MAX 128U
#define APP_AI_WS_ID_MAX 64U
#define APP_AI_WS_MODEL_MAX 48U
#define APP_AI_WS_HELLO_MAX 384U

typedef enum {
    APP_AI_WS_MSG_UNKNOWN = 0,
    APP_AI_WS_MSG_HELLO,
    APP_AI_WS_MSG_TTS,
    APP_AI_WS_MSG_STT,
    APP_AI_WS_MSG_LLM,
    APP_AI_WS_MSG_MCP,
    APP_AI_WS_MSG_SYSTEM,
    APP_AI_WS_MSG_ALERT,
} app_ai_ws_msg_type_t;

typedef struct {
    char endpoint[APP_AI_WS_ENDPOINT_MAX];
    char token[APP_AI_WS_TOKEN_MAX];
    char device_id[APP_AI_WS_ID_MAX];
    char client_id[APP_AI_WS_ID_MAX];
    char model[APP_AI_WS_MODEL_MAX];
    uint32_t sample_rate_hz;
    uint16_t frame_ms;
} app_ai_ws_config_t;

typedef struct {
    app_ai_ws_msg_type_t type;
    const char *json;
} app_ai_ws_message_t;

typedef void (*app_ai_ws_message_cb_t)(const app_ai_ws_message_t *message, void *user);

typedef struct {
    int (*connect)(void *ctx, const app_ai_ws_config_t *config);
    int (*disconnect)(void *ctx);
    int (*send_text)(void *ctx, const char *text);
    int (*send_binary)(void *ctx, const uint8_t *data, uint32_t len);
} app_ai_ws_transport_ops_t;

typedef struct {
    app_ai_ws_config_t config;
    app_protocol_t *protocol;
    const app_ai_ws_transport_ops_t *transport_ops;
    void *transport_ctx;
    app_ai_ws_message_cb_t message_cb;
    void *message_user;
    bool audio_opened;
} app_ai_ws_protocol_t;

int app_ai_ws_protocol_setup(app_protocol_t *protocol,
                              app_ai_ws_protocol_t *ctx,
                              const app_ai_ws_config_t *config);
int app_ai_ws_protocol_set_transport(app_ai_ws_protocol_t *ctx,
                                      const app_ai_ws_transport_ops_t *ops,
                                      void *transport_ctx);
int app_ai_ws_protocol_set_message_cb(app_ai_ws_protocol_t *ctx,
                                       app_ai_ws_message_cb_t cb,
                                       void *user);
int app_ai_ws_protocol_build_hello(const app_ai_ws_protocol_t *ctx,
                                    char *out,
                                    uint32_t out_size);
int app_ai_ws_protocol_handle_text(app_ai_ws_protocol_t *ctx, const char *json);
int app_ai_ws_protocol_handle_audio(app_ai_ws_protocol_t *ctx,
                                     const uint8_t *data,
                                     uint32_t len,
                                     uint32_t timestamp);
app_ai_ws_msg_type_t app_ai_ws_message_type_from_json(const char *json);
const char *app_ai_ws_msg_type_name(app_ai_ws_msg_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* APP_AI_WS_PROTOCOL_H */
