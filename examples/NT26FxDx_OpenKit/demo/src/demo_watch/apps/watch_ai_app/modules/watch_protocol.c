#include "watch_protocol.h"

#include "app_config.h"

#include <string.h>

#include "app_event.h"
#include "app_osal.h"

#if WATCH_AI_ENABLE_MEDIA
#include "app_media_service.h"
#endif

#if WATCH_AI_ENABLE_PROTOCOL
#include "app_ai_ws_protocol.h"
#include "app_protocol.h"
#endif

#if WATCH_AI_ENABLE_LWS_TRANSPORT
#include "app_ai_ws_transport_lws.h"
#endif

#if WATCH_AI_ENABLE_PROTOCOL
static app_protocol_t s_protocol;
static app_ai_ws_protocol_t s_app_ai_ws;
static bool s_protocol_ready;
#endif

#if WATCH_AI_ENABLE_LWS_TRANSPORT
static app_ai_ws_lws_transport_t s_lws_transport;
#endif

#if WATCH_AI_ENABLE_PROTOCOL
static void watch_protocol_copy_string(char *dst, uint32_t dst_size, const char *src)
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

static void watch_protocol_event_cb(const app_protocol_event_t *event, void *user)
{
    app_event_t app_event;

    (void)user;
    if (event == NULL) {
        return;
    }

    memset(&app_event, 0, sizeof(app_event));
    switch (event->id) {
    case APP_PROTOCOL_EVENT_CONNECTED:
        app_event.id = APP_EV_PROTOCOL_CONNECTED;
        (void)app_event_post(&app_event);
        break;
    case APP_PROTOCOL_EVENT_DISCONNECTED:
        app_event.id = APP_EV_PROTOCOL_DISCONNECTED;
        (void)app_event_post(&app_event);
        break;
    case APP_PROTOCOL_EVENT_AUDIO_FRAME:
#if WATCH_AI_ENABLE_MEDIA
        (void)app_media_play_opus((const uint8_t *)event->data, event->len);
#endif
        break;
    case APP_PROTOCOL_EVENT_ERROR:
        app_event.id = APP_EV_ERROR;
        app_event.data.error.code = event->code;
        app_event.data.error.message = event->message;
        (void)app_event_post(&app_event);
        break;
    default:
        break;
    }
}
#endif

int watch_protocol_init(void)
{
#if WATCH_AI_ENABLE_PROTOCOL
    app_ai_ws_config_t config;
    int ret;

    if (WATCH_AI_WS_ENDPOINT[0] == '\0') {
        s_protocol_ready = false;
        return APP_ERR_NOT_SUPPORTED;
    }

    memset(&config, 0, sizeof(config));
    watch_protocol_copy_string(config.endpoint, sizeof(config.endpoint), WATCH_AI_WS_ENDPOINT);
    watch_protocol_copy_string(config.token, sizeof(config.token), WATCH_AI_WS_TOKEN);
    watch_protocol_copy_string(config.device_id, sizeof(config.device_id), WATCH_AI_DEVICE_ID);
    watch_protocol_copy_string(config.client_id, sizeof(config.client_id), WATCH_AI_CLIENT_ID);
    watch_protocol_copy_string(config.model, sizeof(config.model), WATCH_AI_MODEL);
    config.sample_rate_hz = WATCH_AI_AUDIO_SAMPLE_RATE_HZ;
    config.frame_ms = WATCH_AI_AUDIO_FRAME_MS;

    ret = app_ai_ws_protocol_setup(&s_protocol, &s_app_ai_ws, &config);
    if (ret != APP_OK) {
        s_protocol_ready = false;
        return ret;
    }
#if WATCH_AI_ENABLE_LWS_TRANSPORT
    {
        app_ai_ws_lws_config_t lws_config;

        memset(&lws_config, 0, sizeof(lws_config));
        lws_config.protocol = &s_app_ai_ws;
        lws_config.skip_cert_verify = true;
        ret = app_ai_ws_lws_transport_init(&s_lws_transport, &lws_config);
        if (ret != APP_OK) {
            s_protocol_ready = false;
            return ret;
        }
        ret = app_ai_ws_lws_transport_register(&s_lws_transport);
        if (ret != APP_OK) {
            s_protocol_ready = false;
            return ret;
        }
    }
#endif
    ret = app_protocol_set_event_cb(&s_protocol, watch_protocol_event_cb, NULL);
    if (ret != APP_OK) {
        s_protocol_ready = false;
        return ret;
    }
    ret = app_protocol_init(&s_protocol);
    s_protocol_ready = (ret == APP_OK);
    return ret;
#else
    return APP_OK;
#endif
}

int watch_protocol_start(void)
{
#if WATCH_AI_ENABLE_PROTOCOL
    if (!s_protocol_ready) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return app_protocol_start(&s_protocol);
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_protocol_open_audio(void)
{
#if WATCH_AI_ENABLE_PROTOCOL
    if (!s_protocol_ready) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return app_protocol_open_audio(&s_protocol);
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_protocol_close_audio(void)
{
#if WATCH_AI_ENABLE_PROTOCOL
    if (!s_protocol_ready) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return app_protocol_close_audio(&s_protocol);
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_protocol_send_audio(const uint8_t *data, uint32_t len, uint32_t timestamp)
{
#if WATCH_AI_ENABLE_PROTOCOL
    if (data == NULL || len == 0U || !s_protocol_ready) {
        return APP_ERR_NOT_READY;
    }
    return app_protocol_send_audio(&s_protocol, data, len, timestamp);
#else
    (void)data;
    (void)len;
    (void)timestamp;
    return APP_ERR_NOT_SUPPORTED;
#endif
}

bool watch_protocol_is_ready(void)
{
#if WATCH_AI_ENABLE_PROTOCOL
    return s_protocol_ready;
#else
    return false;
#endif
}
