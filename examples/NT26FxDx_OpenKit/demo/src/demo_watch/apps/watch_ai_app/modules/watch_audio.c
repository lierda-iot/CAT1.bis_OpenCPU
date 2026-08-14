#include "watch_audio.h"

#include "app_config.h"
#include "watch_protocol.h"

#include <string.h>

#include "app_event.h"
#include "app_osal.h"

#if WATCH_AI_ENABLE_MEDIA
#include "app_media_service.h"
#endif

#if WATCH_AI_ENABLE_LIOT_AUDIO_PORT
#include "app_audio_port_liot.h"
#endif

#if WATCH_AI_ENABLE_MEDIA || WATCH_AI_ENABLE_PROTOCOL
static bool watch_audio_ret_ok(int ret)
{
    return ret == APP_OK ||
           ret == APP_ERR_NOT_SUPPORTED ||
           ret == APP_ERR_NOT_READY;
}

static void watch_audio_post_event(app_event_id_t id)
{
    app_event_t event;
    int ret;

    memset(&event, 0, sizeof(event));
    event.id = id;
    ret = app_event_post(&event);
    if (ret != APP_OK) {
        app_log("watch event post failed: %s ret=%d", app_event_name(id), ret);
    }
}
#endif

#if WATCH_AI_ENABLE_MEDIA && WATCH_AI_ENABLE_PROTOCOL
static void watch_audio_media_audio_cb(const uint8_t *data,
                                       uint32_t len,
                                       uint32_t timestamp,
                                       void *user)
{
    (void)user;
    if (data == NULL || len == 0U || !watch_protocol_is_ready()) {
        return;
    }
    (void)watch_protocol_send_audio(data, len, timestamp);
}
#endif

int watch_audio_init(void)
{
#if WATCH_AI_ENABLE_MEDIA
    app_media_config_t media_config;

    memset(&media_config, 0, sizeof(media_config));
    media_config.record_format.sample_rate_hz = WATCH_AI_AUDIO_SAMPLE_RATE_HZ;
    media_config.record_format.channels = 1U;
    media_config.record_format.bits_per_sample = 16U;
    media_config.record_format.frame_ms = WATCH_AI_AUDIO_FRAME_MS;
    media_config.playback_format = media_config.record_format;
    media_config.frame_samples = WATCH_AI_AUDIO_FRAME_SAMPLES;
    media_config.max_encoded_bytes = WATCH_AI_AUDIO_MAX_ENCODED_BYTES;
    media_config.vad_enabled = WATCH_AI_ENABLE_LIOT_AUDIO_PORT != 0;

#if WATCH_AI_ENABLE_LIOT_AUDIO_PORT
    {
        app_audio_liot_config_t audio_port_config;
        int ret;

        ret = app_audio_liot_get_default_config(&audio_port_config);
        if (ret != APP_OK) {
            return ret;
        }
        audio_port_config.record_format = media_config.record_format;
        audio_port_config.playback_format = media_config.playback_format;
        audio_port_config.frame_samples = media_config.frame_samples;
        audio_port_config.max_encoded_bytes = media_config.max_encoded_bytes;
        audio_port_config.enable_record = true;
        audio_port_config.enable_playback = true;
        audio_port_config.enable_opus_encode = false;
        audio_port_config.enable_opus_decode = true;
        audio_port_config.enable_vad = media_config.vad_enabled;
        ret = app_audio_liot_setup(&audio_port_config);
        if (ret != APP_OK) {
            return ret;
        }
        ret = app_audio_liot_register();
        if (ret != APP_OK) {
            return ret;
        }
    }
#endif

    {
        int ret = app_media_init(&media_config);

        if (ret != APP_OK) {
            return ret;
        }
#if WATCH_AI_ENABLE_PROTOCOL
        return app_media_set_audio_cb(watch_audio_media_audio_cb, NULL);
#else
        return APP_OK;
#endif
    }
#else
    app_log("watch init media: disabled");
    return APP_OK;
#endif
}

int watch_audio_start_listen(void)
{
#if WATCH_AI_ENABLE_MEDIA || WATCH_AI_ENABLE_PROTOCOL
    int ret;

    watch_audio_post_event(APP_EV_BUTTON_PRESS);
#if WATCH_AI_ENABLE_PROTOCOL
    if (watch_protocol_is_ready()) {
        ret = watch_protocol_open_audio();
        if (!watch_audio_ret_ok(ret)) {
            return ret;
        }
    }
#endif
#if WATCH_AI_ENABLE_MEDIA
    ret = app_media_start_record();
    if (!watch_audio_ret_ok(ret)) {
        return ret;
    }
#endif
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

void watch_audio_stop_voice_path(void)
{
#if WATCH_AI_ENABLE_MEDIA
    int media_ret = app_media_stop_record();

    if (!watch_audio_ret_ok(media_ret)) {
        app_log("watch back stop record failed: %d", media_ret);
    }
#endif
#if WATCH_AI_ENABLE_PROTOCOL
    if (watch_protocol_is_ready()) {
        int protocol_ret = watch_protocol_close_audio();

        if (!watch_audio_ret_ok(protocol_ret)) {
            app_log("watch back close audio failed: %d", protocol_ret);
        }
    }
#endif
}

int watch_audio_stop_listen(void)
{
#if WATCH_AI_ENABLE_MEDIA || WATCH_AI_ENABLE_PROTOCOL
    int ret;

#if WATCH_AI_ENABLE_MEDIA
    ret = app_media_stop_record();
    if (!watch_audio_ret_ok(ret)) {
        return ret;
    }
#endif
#if WATCH_AI_ENABLE_PROTOCOL
    if (watch_protocol_is_ready()) {
        ret = watch_protocol_close_audio();
        if (!watch_audio_ret_ok(ret)) {
            return ret;
        }
    }
#endif
    watch_audio_post_event(APP_EV_VAD_END);
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}
