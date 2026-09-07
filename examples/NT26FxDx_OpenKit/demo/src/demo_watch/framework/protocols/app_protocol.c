#include "app_protocol.h"

#include <stddef.h>
#include <string.h>

int app_protocol_bind(app_protocol_t *protocol,
                      app_protocol_type_t type,
                      const app_protocol_ops_t *ops,
                      void *ctx)
{
    if (protocol == NULL || ops == NULL || ctx == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(protocol, 0, sizeof(*protocol));
    protocol->type = type;
    protocol->ops = ops;
    protocol->ctx = ctx;
    return APP_OK;
}

int app_protocol_init(app_protocol_t *protocol)
{
    int ret;

    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (protocol->ops->init == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = protocol->ops->init(protocol->ctx);
    if (ret == APP_OK) {
        protocol->initialized = true;
    }
    return ret;
}

int app_protocol_deinit(app_protocol_t *protocol)
{
    int ret;

    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->initialized) {
        return APP_OK;
    }
    if (protocol->ops->deinit == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = protocol->ops->deinit(protocol->ctx);
    if (ret == APP_OK) {
        protocol->initialized = false;
        protocol->started = false;
    }
    return ret;
}

int app_protocol_start(app_protocol_t *protocol)
{
    int ret;

    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->initialized) {
        return APP_ERR_NOT_READY;
    }
    if (protocol->ops->start == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = protocol->ops->start(protocol->ctx);
    if (ret == APP_OK) {
        protocol->started = true;
    }
    return ret;
}

int app_protocol_stop(app_protocol_t *protocol)
{
    int ret;

    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->started) {
        return APP_OK;
    }
    if (protocol->ops->stop == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = protocol->ops->stop(protocol->ctx);
    if (ret == APP_OK) {
        protocol->started = false;
    }
    return ret;
}

int app_protocol_open_audio(app_protocol_t *protocol)
{
    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->started) {
        return APP_ERR_NOT_READY;
    }
    return (protocol->ops->open_audio != NULL) ? protocol->ops->open_audio(protocol->ctx) : APP_ERR_NOT_SUPPORTED;
}

int app_protocol_close_audio(app_protocol_t *protocol)
{
    if (protocol == NULL || protocol->ops == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->started) {
        return APP_ERR_NOT_READY;
    }
    return (protocol->ops->close_audio != NULL) ? protocol->ops->close_audio(protocol->ctx) : APP_ERR_NOT_SUPPORTED;
}

int app_protocol_send_audio(app_protocol_t *protocol,
                            const uint8_t *data,
                            uint32_t len,
                            uint32_t timestamp)
{
    if (protocol == NULL || protocol->ops == NULL || data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->started) {
        return APP_ERR_NOT_READY;
    }
    return (protocol->ops->send_audio != NULL) ?
           protocol->ops->send_audio(protocol->ctx, data, len, timestamp) :
           APP_ERR_NOT_SUPPORTED;
}

int app_protocol_send_json(app_protocol_t *protocol, const char *json)
{
    if (protocol == NULL || protocol->ops == NULL || json == NULL || json[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (!protocol->started) {
        return APP_ERR_NOT_READY;
    }
    return (protocol->ops->send_json != NULL) ?
           protocol->ops->send_json(protocol->ctx, json) :
           APP_ERR_NOT_SUPPORTED;
}

int app_protocol_set_event_cb(app_protocol_t *protocol, app_protocol_event_cb_t cb, void *user)
{
    if (protocol == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    protocol->event_cb = cb;
    protocol->event_user = user;
    return APP_OK;
}

int app_protocol_emit_event(app_protocol_t *protocol, const app_protocol_event_t *event)
{
    if (protocol == NULL || event == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (protocol->event_cb != NULL) {
        protocol->event_cb(event, protocol->event_user);
    }
    return APP_OK;
}

const char *app_protocol_type_name(app_protocol_type_t type)
{
    switch (type) {
    case APP_PROTOCOL_TYPE_NONE:
        return "none";
    case APP_PROTOCOL_TYPE_APP_AI_WS:
        return "app_ai_ws";
    case APP_PROTOCOL_TYPE_APP_AI_MQTT_UDP:
        return "app_ai_mqtt_udp";
    case APP_PROTOCOL_TYPE_COZE_WS:
        return "coze_ws";
    case APP_PROTOCOL_TYPE_COMPANY_PRIVATE:
        return "company_private";
    default:
        return "unknown";
    }
}
