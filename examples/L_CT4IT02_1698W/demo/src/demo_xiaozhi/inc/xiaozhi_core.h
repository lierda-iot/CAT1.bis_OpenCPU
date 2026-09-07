#ifndef XIAOZHI_CORE_H
#define XIAOZHI_CORE_H

#include <stdbool.h>
#include <stdint.h>

#define XZ_OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define XZ_DEVICE_ID_SIZE 18
#define XZ_CLIENT_ID_SIZE 37
#define XZ_WS_URL_SIZE 256
#define XZ_WS_TOKEN_SIZE 512

typedef struct {
    char device_id[XZ_DEVICE_ID_SIZE];
    char client_id[XZ_CLIENT_ID_SIZE];
    char websocket_url[XZ_WS_URL_SIZE];
    char websocket_token[XZ_WS_TOKEN_SIZE];
    int websocket_version;
    char activation_code[16];
    char activation_message[128];
    char activation_challenge[96];
    bool activation_required;
} xiaozhi_config_t;

int xiaozhi_identity_init(xiaozhi_config_t *config);
int xiaozhi_network_start(void);
int xiaozhi_ota_check(xiaozhi_config_t *config);
int xiaozhi_ota_activate(const xiaozhi_config_t *config);
int xiaozhi_ws_run(const xiaozhi_config_t *config);
int xiaozhi_audio_init(void);
int xiaozhi_audio_probe_init(void);
int xiaozhi_opus_probe_init(void);
int xiaozhi_audio_capture_opus(uint8_t *opus_data, int capacity);
int xiaozhi_audio_capture_flush_opus(uint8_t *opus_data, int capacity);
int xiaozhi_audio_capture_discard(void);
void xiaozhi_audio_capture_session_begin(void);
void xiaozhi_audio_capture_session_end(void);
int xiaozhi_capture_init(void);
void xiaozhi_capture_begin(void);
void xiaozhi_capture_end(void);
int xiaozhi_capture_read(uint8_t *pcm, int len, uint32_t timeout);
int xiaozhi_capture_read_nowait(uint8_t *pcm, int len);
int xiaozhi_audio_decode_append(const uint8_t *opus_data, int length);
void xiaozhi_audio_tts_reset(void);
void xiaozhi_audio_tts_abort(void);
int xiaozhi_audio_tts_play(void);
void xiaozhi_ui_init(void);
void xiaozhi_ui_status(const char *status);
bool xiaozhi_talk_set(bool pressed);

#endif
