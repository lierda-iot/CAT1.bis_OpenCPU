#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char token[96];
    char botid[32];
    char voiceid[32];
    char imei[16];
} ai_client_config_t;

typedef struct {
    uint8_t *data;
    uint32_t size;
} ai_client_audio_t;

int ai_client_init(const ai_client_config_t *cfg);
void ai_client_deinit(void);
int ai_client_connect(void);
int ai_client_disconnect(void);
int ai_client_send_audio(const ai_client_audio_t *audio);
int ai_client_send_audio_complete(void);
int ai_client_recv_play(void);
int ai_client_cancel(void);
bool ai_client_is_connected(void);

#endif /* AI_CLIENT_H */
