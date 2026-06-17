#ifndef WS_AI_H
#define WS_AI_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WS_AI_OK = 0,
    WS_AI_ERROR,
    WS_AI_TIMEOUT,
    WS_AI_DISCONNECT,
    WS_AI_NO_MEMORY,
} ws_ai_err_e;

typedef struct {
    char token[96];
    char botid[32];
    char voiceid[32];
    char imei[16];
} ws_ai_cfg_t;

int ws_ai_init(const ws_ai_cfg_t *cfg);
void ws_ai_deinit(void);
int ws_ai_connect(const ws_ai_cfg_t *cfg);
int ws_ai_disconnect(void);
int ws_ai_send_audio(uint8_t *data, uint32_t len);
int ws_ai_send_audio_complete(const char *imei);
int ws_ai_recv_play(const char *imei);
int ws_ai_cancel(const char *imei);
bool ws_ai_is_connected(void);

int ws_ai_send_raw(uint8_t *data, uint32_t len, uint8_t protocol, bool sync);

#endif /* WS_AI_H */
