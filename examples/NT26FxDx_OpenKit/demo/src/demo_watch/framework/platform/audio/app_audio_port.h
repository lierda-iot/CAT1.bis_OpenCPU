#ifndef APP_AUDIO_PORT_H
#define APP_AUDIO_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_AUDIO_CODEC_PCM = 0,
    APP_AUDIO_CODEC_OPUS,
} app_audio_codec_t;

typedef enum {
    APP_AUDIO_EVENT_NONE = 0,
    APP_AUDIO_EVENT_RECORD_STARTED,
    APP_AUDIO_EVENT_RECORD_STOPPED,
    APP_AUDIO_EVENT_PLAYBACK_DONE,
    APP_AUDIO_EVENT_VAD_START,
    APP_AUDIO_EVENT_VAD_END,
    APP_AUDIO_EVENT_ERROR,
} app_audio_event_id_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint16_t frame_ms;
} app_audio_format_t;

typedef struct {
    bool has_record;
    bool has_playback;
    bool has_opus_encode;
    bool has_opus_decode;
    bool has_vad;
    app_audio_format_t input_format;
    app_audio_format_t output_format;
    uint32_t frame_samples;
    uint32_t max_encoded_bytes;
} app_audio_caps_t;

typedef struct {
    app_audio_event_id_t id;
    int code;
    const char *message;
} app_audio_event_t;

typedef void (*app_audio_event_cb_t)(const app_audio_event_t *event, void *user);

typedef struct {
    int (*init)(const app_audio_caps_t *caps);
    int (*deinit)(void);
    int (*start)(void);
    int (*stop)(void);
    int (*start_record)(void);
    int (*stop_record)(void);
    int (*read_pcm)(int16_t *pcm, uint32_t sample_capacity, uint32_t *sample_count);
    int (*play_pcm)(const int16_t *pcm, uint32_t samples);
    int (*play_opus)(const uint8_t *data, uint32_t len);
    int (*encode_opus)(const int16_t *pcm,
                       uint32_t samples,
                       uint8_t *out,
                       uint32_t out_size,
                       uint32_t *out_len);
    int (*decode_opus)(const uint8_t *data,
                       uint32_t len,
                       int16_t *pcm,
                       uint32_t sample_capacity,
                       uint32_t *sample_count);
    int (*play_prompt)(const char *name);
    int (*set_event_cb)(app_audio_event_cb_t cb, void *user);
} app_audio_port_ops_t;

typedef struct {
    app_audio_caps_t caps;
    const app_audio_port_ops_t *ops;
} app_audio_port_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_AUDIO_PORT_H */
