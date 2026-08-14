#include "app_ai_ws_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "app_event.h"

static int app_ai_ws_init(void *ctx);
static int app_ai_ws_deinit(void *ctx);
static int app_ai_ws_start(void *ctx);
static int app_ai_ws_open_audio(void *ctx);
static int app_ai_ws_close_audio(void *ctx);
static int app_ai_ws_send_audio(void *ctx, const uint8_t *data, uint32_t len, uint32_t timestamp);
static int app_ai_ws_send_json(void *ctx, const char *json);
static int app_ai_ws_stop(void *ctx);

static const app_protocol_ops_t s_app_ai_ws_ops = {
    .init = app_ai_ws_init,
    .deinit = app_ai_ws_deinit,
    .start = app_ai_ws_start,
    .open_audio = app_ai_ws_open_audio,
    .close_audio = app_ai_ws_close_audio,
    .send_audio = app_ai_ws_send_audio,
    .send_json = app_ai_ws_send_json,
    .stop = app_ai_ws_stop,
};

static void copy_string(char *dst, uint32_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static const char *skip_ws(const char *p)
{
    while (p != NULL && *p != '\0' && isspace((unsigned char)*p) != 0) {
        p++;
    }
    return p;
}

static bool json_has_string_value(const char *json, const char *key, const char *value)
{
    const char *p;
    size_t key_len;
    size_t value_len;

    if (json == NULL || key == NULL || value == NULL) {
        return false;
    }

    key_len = strlen(key);
    value_len = strlen(value);
    p = json;
    while ((p = strstr(p, key)) != NULL) {
        const char *cursor = p;

        if (p == json || p[-1] != '"') {
            p++;
            continue;
        }
        cursor += key_len;
        if (*cursor != '"') {
            p++;
            continue;
        }
        cursor++;
        cursor = skip_ws(cursor);
        if (cursor == NULL || *cursor != ':') {
            p++;
            continue;
        }
        cursor++;
        cursor = skip_ws(cursor);
        if (cursor == NULL || *cursor != '"') {
            p++;
            continue;
        }
        cursor++;
        if (strncmp(cursor, value, value_len) == 0 && cursor[value_len] == '"') {
            return true;
        }
        p++;
    }
    return false;
}

static int app_ai_ws_emit(app_ai_ws_protocol_t *ctx,
                           app_protocol_event_id_t id,
                           const void *data,
                           uint32_t len,
                           int code,
                           const char *message)
{
    app_protocol_event_t event;

    if (ctx == NULL || ctx->protocol == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(&event, 0, sizeof(event));
    event.id = id;
    event.data = data;
    event.len = len;
    event.code = code;
    event.message = message;
    return app_protocol_emit_event(ctx->protocol, &event);
}

static int app_ai_ws_init(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;

    if (ws == NULL || ws->config.endpoint[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    return APP_OK;
}

static int app_ai_ws_deinit(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;

    if (ws == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ws->audio_opened = false;
    return APP_OK;
}

static int app_ai_ws_start(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;
    char hello[APP_AI_WS_HELLO_MAX];
    int ret;

    if (ws == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (ws->transport_ops == NULL || ws->transport_ops->connect == NULL ||
        ws->transport_ops->send_text == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = ws->transport_ops->connect(ws->transport_ctx, &ws->config);
    if (ret != APP_OK) {
        return ret;
    }

    ret = app_ai_ws_protocol_build_hello(ws, hello, sizeof(hello));
    if (ret != APP_OK) {
        return ret;
    }

    return ws->transport_ops->send_text(ws->transport_ctx, hello);
}

static int app_ai_ws_open_audio(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;
    int ret;

    if (ws == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = app_ai_ws_send_json(ws, "{\"type\":\"listen\",\"state\":\"start\",\"mode\":\"manual\"}");
    if (ret == APP_OK) {
        ws->audio_opened = true;
        (void)app_ai_ws_emit(ws, APP_PROTOCOL_EVENT_AUDIO_OPENED, NULL, 0U, APP_OK, NULL);
    }
    return ret;
}

static int app_ai_ws_close_audio(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;
    int ret;

    if (ws == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = app_ai_ws_send_json(ws, "{\"type\":\"listen\",\"state\":\"stop\"}");
    if (ret == APP_OK) {
        ws->audio_opened = false;
        (void)app_ai_ws_emit(ws, APP_PROTOCOL_EVENT_AUDIO_CLOSED, NULL, 0U, APP_OK, NULL);
    }
    return ret;
}

static int app_ai_ws_send_audio(void *ctx, const uint8_t *data, uint32_t len, uint32_t timestamp)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;

    (void)timestamp;
    if (ws == NULL || data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (!ws->audio_opened) {
        return APP_ERR_NOT_READY;
    }
    if (ws->transport_ops == NULL || ws->transport_ops->send_binary == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return ws->transport_ops->send_binary(ws->transport_ctx, data, len);
}

static int app_ai_ws_send_json(void *ctx, const char *json)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;

    if (ws == NULL || json == NULL || json[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (ws->transport_ops == NULL || ws->transport_ops->send_text == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return ws->transport_ops->send_text(ws->transport_ctx, json);
}

static int app_ai_ws_stop(void *ctx)
{
    app_ai_ws_protocol_t *ws = (app_ai_ws_protocol_t *)ctx;
    int ret = APP_OK;

    if (ws == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (ws->transport_ops != NULL && ws->transport_ops->disconnect != NULL) {
        ret = ws->transport_ops->disconnect(ws->transport_ctx);
    }
    ws->audio_opened = false;
    if (ret == APP_OK) {
        (void)app_ai_ws_emit(ws, APP_PROTOCOL_EVENT_DISCONNECTED, NULL, 0U, APP_OK, NULL);
    }
    return ret;
}

int app_ai_ws_protocol_setup(app_protocol_t *protocol,
                              app_ai_ws_protocol_t *ctx,
                              const app_ai_ws_config_t *config)
{
    int ret;

    if (protocol == NULL || ctx == NULL || config == NULL || config->endpoint[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    copy_string(ctx->config.endpoint, sizeof(ctx->config.endpoint), config->endpoint);
    copy_string(ctx->config.token, sizeof(ctx->config.token), config->token);
    copy_string(ctx->config.device_id, sizeof(ctx->config.device_id), config->device_id);
    copy_string(ctx->config.client_id, sizeof(ctx->config.client_id), config->client_id);
    copy_string(ctx->config.model, sizeof(ctx->config.model), config->model);
    ctx->config.sample_rate_hz = (config->sample_rate_hz != 0U) ? config->sample_rate_hz : 16000U;
    ctx->config.frame_ms = (config->frame_ms != 0U) ? config->frame_ms : 20U;
    ctx->protocol = protocol;

    ret = app_protocol_bind(protocol, APP_PROTOCOL_TYPE_APP_AI_WS, &s_app_ai_ws_ops, ctx);
    if (ret != APP_OK) {
        memset(ctx, 0, sizeof(*ctx));
    }
    return ret;
}

int app_ai_ws_protocol_set_transport(app_ai_ws_protocol_t *ctx,
                                      const app_ai_ws_transport_ops_t *ops,
                                      void *transport_ctx)
{
    if (ctx == NULL || ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ctx->transport_ops = ops;
    ctx->transport_ctx = transport_ctx;
    return APP_OK;
}

int app_ai_ws_protocol_set_message_cb(app_ai_ws_protocol_t *ctx,
                                       app_ai_ws_message_cb_t cb,
                                       void *user)
{
    if (ctx == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ctx->message_cb = cb;
    ctx->message_user = user;
    return APP_OK;
}

int app_ai_ws_protocol_build_hello(const app_ai_ws_protocol_t *ctx,
                                    char *out,
                                    uint32_t out_size)
{
    int written;

    if (ctx == NULL || out == NULL || out_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    written = snprintf(out,
                       out_size,
                       "{\"type\":\"hello\",\"version\":1,"
                       "\"transport\":\"websocket\","
                       "\"audio\":{\"codec\":\"opus\",\"sample_rate\":%lu,\"frame_ms\":%u},"
                       "\"device\":{\"id\":\"%s\",\"client_id\":\"%s\",\"model\":\"%s\"}}",
                       (unsigned long)ctx->config.sample_rate_hz,
                       (unsigned int)ctx->config.frame_ms,
                       ctx->config.device_id,
                       ctx->config.client_id,
                       ctx->config.model);
    if (written < 0 || (uint32_t)written >= out_size) {
        out[0] = '\0';
        return APP_ERR_NO_MEMORY;
    }
    return APP_OK;
}

int app_ai_ws_protocol_handle_text(app_ai_ws_protocol_t *ctx, const char *json)
{
    app_ai_ws_message_t message;

    if (ctx == NULL || json == NULL || json[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    memset(&message, 0, sizeof(message));
    message.type = app_ai_ws_message_type_from_json(json);
    message.json = json;

    if (ctx->message_cb != NULL) {
        ctx->message_cb(&message, ctx->message_user);
    }

    if (message.type == APP_AI_WS_MSG_HELLO) {
        (void)app_ai_ws_emit(ctx, APP_PROTOCOL_EVENT_CONNECTED, json, (uint32_t)strlen(json), APP_OK, NULL);
    } else {
        (void)app_ai_ws_emit(ctx, APP_PROTOCOL_EVENT_JSON, json, (uint32_t)strlen(json), APP_OK, NULL);
    }

    return APP_OK;
}

int app_ai_ws_protocol_handle_audio(app_ai_ws_protocol_t *ctx,
                                     const uint8_t *data,
                                     uint32_t len,
                                     uint32_t timestamp)
{
    app_event_t app_event;

    if (ctx == NULL || data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    (void)app_ai_ws_emit(ctx, APP_PROTOCOL_EVENT_AUDIO_FRAME, data, len, APP_OK, NULL);

    memset(&app_event, 0, sizeof(app_event));
    app_event.id = APP_EV_AUDIO_RECV;
    app_event.data.audio.timestamp = timestamp;
    (void)app_event_post(&app_event);
    return APP_OK;
}

app_ai_ws_msg_type_t app_ai_ws_message_type_from_json(const char *json)
{
    if (json == NULL) {
        return APP_AI_WS_MSG_UNKNOWN;
    }
    if (json_has_string_value(json, "type", "hello")) {
        return APP_AI_WS_MSG_HELLO;
    }
    if (json_has_string_value(json, "type", "tts")) {
        return APP_AI_WS_MSG_TTS;
    }
    if (json_has_string_value(json, "type", "stt")) {
        return APP_AI_WS_MSG_STT;
    }
    if (json_has_string_value(json, "type", "llm")) {
        return APP_AI_WS_MSG_LLM;
    }
    if (json_has_string_value(json, "type", "mcp") ||
        json_has_string_value(json, "method", "mcp")) {
        return APP_AI_WS_MSG_MCP;
    }
    if (json_has_string_value(json, "type", "system")) {
        return APP_AI_WS_MSG_SYSTEM;
    }
    if (json_has_string_value(json, "type", "alert")) {
        return APP_AI_WS_MSG_ALERT;
    }
    return APP_AI_WS_MSG_UNKNOWN;
}

const char *app_ai_ws_msg_type_name(app_ai_ws_msg_type_t type)
{
    switch (type) {
    case APP_AI_WS_MSG_HELLO:
        return "hello";
    case APP_AI_WS_MSG_TTS:
        return "tts";
    case APP_AI_WS_MSG_STT:
        return "stt";
    case APP_AI_WS_MSG_LLM:
        return "llm";
    case APP_AI_WS_MSG_MCP:
        return "mcp";
    case APP_AI_WS_MSG_SYSTEM:
        return "system";
    case APP_AI_WS_MSG_ALERT:
        return "alert";
    case APP_AI_WS_MSG_UNKNOWN:
    default:
        return "unknown";
    }
}
