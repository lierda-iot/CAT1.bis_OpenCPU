#ifndef APP_MEDIA_SERVICE_H
#define APP_MEDIA_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_audio_port.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    app_audio_format_t record_format;
    app_audio_format_t playback_format;
    uint32_t frame_samples;
    uint32_t max_encoded_bytes;
    bool vad_enabled;
} app_media_config_t;

typedef void (*app_media_audio_cb_t)(const uint8_t *data,
                                     uint32_t len,
                                     uint32_t timestamp,
                                     void *user);

int app_media_register_port(const app_audio_port_t *port);
int app_media_init(const app_media_config_t *config);
int app_media_deinit(void);
int app_media_start(void);
int app_media_stop(void);
int app_media_start_record(void);
int app_media_stop_record(void);
int app_media_read_pcm(int16_t *pcm, uint32_t sample_capacity, uint32_t *sample_count);
int app_media_play_pcm(const int16_t *pcm, uint32_t samples);
int app_media_play_opus(const uint8_t *data, uint32_t len);
int app_media_encode_frame(const int16_t *pcm,
                           uint32_t samples,
                           uint8_t *out,
                           uint32_t out_size,
                           uint32_t *out_len);
int app_media_decode_frame(const uint8_t *data,
                           uint32_t len,
                           int16_t *pcm,
                           uint32_t sample_capacity,
                           uint32_t *sample_count);
int app_media_play_prompt(const char *name);
int app_media_set_audio_cb(app_media_audio_cb_t cb, void *user);
int app_media_emit_audio_frame(const uint8_t *data, uint32_t len, uint32_t timestamp);
const app_media_config_t *app_media_get_config(void);
const app_audio_caps_t *app_media_get_caps(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MEDIA_SERVICE_H */
