#include "app_media_service.h"

#include <stddef.h>
#include <string.h>

#include "app_event.h"
#include "app_osal.h"

static const app_audio_port_t *s_port;
static app_media_config_t s_config;
static bool s_initialized;
static app_media_audio_cb_t s_audio_cb;
static void *s_audio_user;

static const app_media_config_t s_default_config = {
    .record_format = {
        .sample_rate_hz = 16000U,
        .channels = 1U,
        .bits_per_sample = 16U,
        .frame_ms = 20U,
    },
    .playback_format = {
        .sample_rate_hz = 16000U,
        .channels = 1U,
        .bits_per_sample = 16U,
        .frame_ms = 20U,
    },
    .frame_samples = 320U,
    .max_encoded_bytes = 256U,
    .vad_enabled = false,
};

static const app_audio_port_ops_t *media_ops(void)
{
    return (s_port != NULL) ? s_port->ops : NULL;
}

static int media_require_ready(const app_audio_port_ops_t **ops)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    *ops = media_ops();
    if (*ops == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return APP_OK;
}

static void media_audio_event_cb(const app_audio_event_t *event, void *user)
{
    app_event_t app_event;

    (void)user;
    if (event == NULL) {
        return;
    }

    memset(&app_event, 0, sizeof(app_event));
    switch (event->id) {
    case APP_AUDIO_EVENT_VAD_START:
        app_event.id = APP_EV_VAD_START;
        (void)app_event_post(&app_event);
        break;
    case APP_AUDIO_EVENT_VAD_END:
        app_event.id = APP_EV_VAD_END;
        (void)app_event_post(&app_event);
        break;
    case APP_AUDIO_EVENT_ERROR:
        app_event.id = APP_EV_ERROR;
        app_event.data.error.code = event->code;
        app_event.data.error.message = event->message;
        (void)app_event_post(&app_event);
        break;
    default:
        break;
    }
}

int app_media_register_port(const app_audio_port_t *port)
{
    if (port == NULL || port->ops == NULL || port->ops->init == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    s_port = port;
    return APP_OK;
}

int app_media_init(const app_media_config_t *config)
{
    const app_audio_port_ops_t *ops = media_ops();
    app_audio_caps_t caps;
    int ret;

    s_config = (config != NULL) ? *config : s_default_config;
    s_initialized = false;

    if (ops == NULL || ops->init == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }

    memset(&caps, 0, sizeof(caps));
    if (s_port != NULL) {
        caps = s_port->caps;
    }
    caps.input_format = s_config.record_format;
    caps.output_format = s_config.playback_format;
    caps.frame_samples = s_config.frame_samples;
    caps.max_encoded_bytes = s_config.max_encoded_bytes;
    caps.has_vad = caps.has_vad && s_config.vad_enabled;

    if (ops->set_event_cb != NULL) {
        (void)ops->set_event_cb(media_audio_event_cb, NULL);
    }
    s_initialized = true;

    ret = ops->init(&caps);
    if (ret != APP_OK) {
        s_initialized = false;
        if (ops->set_event_cb != NULL) {
            (void)ops->set_event_cb(NULL, NULL);
        }
        return ret;
    }

    app_log("media initialized: %lu Hz, %lu samples/frame",
            (unsigned long)s_config.record_format.sample_rate_hz,
            (unsigned long)s_config.frame_samples);
    return APP_OK;
}

int app_media_deinit(void)
{
    const app_audio_port_ops_t *ops;
    int ret = media_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    if (ops->deinit == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = ops->deinit();
    if (ret == APP_OK) {
        s_initialized = false;
        s_audio_cb = NULL;
        s_audio_user = NULL;
    }
    return ret;
}

int app_media_start(void)
{
    const app_audio_port_ops_t *ops;
    int ret = media_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return (ops->start != NULL) ? ops->start() : APP_ERR_NOT_SUPPORTED;
}

int app_media_stop(void)
{
    const app_audio_port_ops_t *ops;
    int ret = media_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return (ops->stop != NULL) ? ops->stop() : APP_ERR_NOT_SUPPORTED;
}

int app_media_start_record(void)
{
    const app_audio_port_ops_t *ops;
    int ret = media_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return (ops->start_record != NULL) ? ops->start_record() : APP_ERR_NOT_SUPPORTED;
}

int app_media_stop_record(void)
{
    const app_audio_port_ops_t *ops;
    int ret = media_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return (ops->stop_record != NULL) ? ops->stop_record() : APP_ERR_NOT_SUPPORTED;
}

int app_media_read_pcm(int16_t *pcm, uint32_t sample_capacity, uint32_t *sample_count)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (pcm == NULL || sample_capacity == 0U || sample_count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->read_pcm != NULL) ? ops->read_pcm(pcm, sample_capacity, sample_count) : APP_ERR_NOT_SUPPORTED;
}

int app_media_play_pcm(const int16_t *pcm, uint32_t samples)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (pcm == NULL || samples == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->play_pcm != NULL) ? ops->play_pcm(pcm, samples) : APP_ERR_NOT_SUPPORTED;
}

int app_media_play_opus(const uint8_t *data, uint32_t len)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->play_opus != NULL) ? ops->play_opus(data, len) : APP_ERR_NOT_SUPPORTED;
}

int app_media_encode_frame(const int16_t *pcm,
                           uint32_t samples,
                           uint8_t *out,
                           uint32_t out_size,
                           uint32_t *out_len)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (pcm == NULL || samples == 0U || out == NULL || out_size == 0U || out_len == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *out_len = 0U;
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->encode_opus != NULL) ? ops->encode_opus(pcm, samples, out, out_size, out_len) : APP_ERR_NOT_SUPPORTED;
}

int app_media_decode_frame(const uint8_t *data,
                           uint32_t len,
                           int16_t *pcm,
                           uint32_t sample_capacity,
                           uint32_t *sample_count)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (data == NULL || len == 0U || pcm == NULL || sample_capacity == 0U || sample_count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *sample_count = 0U;
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->decode_opus != NULL) ? ops->decode_opus(data, len, pcm, sample_capacity, sample_count) : APP_ERR_NOT_SUPPORTED;
}

int app_media_play_prompt(const char *name)
{
    const app_audio_port_ops_t *ops;
    int ret;

    if (name == NULL || name[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    ret = media_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->play_prompt != NULL) ? ops->play_prompt(name) : APP_ERR_NOT_SUPPORTED;
}

int app_media_set_audio_cb(app_media_audio_cb_t cb, void *user)
{
    s_audio_cb = cb;
    s_audio_user = user;
    return APP_OK;
}

int app_media_emit_audio_frame(const uint8_t *data, uint32_t len, uint32_t timestamp)
{
    if (data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    if (s_audio_cb == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    s_audio_cb(data, len, timestamp, s_audio_user);
    return APP_OK;
}

const app_media_config_t *app_media_get_config(void)
{
    return s_initialized ? &s_config : NULL;
}

const app_audio_caps_t *app_media_get_caps(void)
{
    return (s_port != NULL) ? &s_port->caps : NULL;
}
