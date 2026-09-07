#ifndef APP_AI_WS_TRANSPORT_LWS_H
#define APP_AI_WS_TRANSPORT_LWS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_osal.h"
#include "app_ai_ws_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_AI_WS_LWS_HOST_MAX 96U
#define APP_AI_WS_LWS_PATH_MAX 160U
#define APP_AI_WS_LWS_AUTH_MAX 160U
#define APP_AI_WS_LWS_PROTOCOL_NAME_MAX 32U

typedef struct {
    app_ai_ws_protocol_t *protocol;
    bool skip_cert_verify;
    uint32_t service_timeout_ms;
    uint32_t task_stack_bytes;
    uint8_t task_priority;
    uint8_t send_queue_depth;
} app_ai_ws_lws_config_t;

typedef struct {
    uint8_t *buffer;
    uint32_t len;
    int write_protocol;
} app_ai_ws_lws_send_msg_t;

typedef struct {
    app_ai_ws_protocol_t *protocol;
    app_task_t task;
    app_queue_t send_queue;
    void *context;
    void *wsi;
    bool initialized;
    bool running;
    bool connected;
    bool use_ssl;
    bool skip_cert_verify;
    uint16_t port;
    uint32_t service_timeout_ms;
    uint32_t task_stack_bytes;
    uint8_t task_priority;
    uint8_t send_queue_depth;
    char host[APP_AI_WS_LWS_HOST_MAX];
    char path[APP_AI_WS_LWS_PATH_MAX];
    char auth_header[APP_AI_WS_LWS_AUTH_MAX];
    char protocol_name[APP_AI_WS_LWS_PROTOCOL_NAME_MAX];
} app_ai_ws_lws_transport_t;

int app_ai_ws_lws_transport_init(app_ai_ws_lws_transport_t *ctx,
                                  const app_ai_ws_lws_config_t *config);
int app_ai_ws_lws_transport_register(app_ai_ws_lws_transport_t *ctx);
const app_ai_ws_transport_ops_t *app_ai_ws_lws_transport_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AI_WS_TRANSPORT_LWS_H */
