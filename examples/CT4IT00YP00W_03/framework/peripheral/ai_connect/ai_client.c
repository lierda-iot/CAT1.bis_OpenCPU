#include <string.h>
#include "ai_client.h"
#include "ws_ai.h"

static ws_ai_cfg_t g_ai_cfg;

int ai_client_init(const ai_client_config_t *cfg)
{
    if (cfg == NULL) {
        return -1;
    }

    memcpy(g_ai_cfg.token, cfg->token, sizeof(g_ai_cfg.token));
    memcpy(g_ai_cfg.botid, cfg->botid, sizeof(g_ai_cfg.botid));
    memcpy(g_ai_cfg.voiceid, cfg->voiceid, sizeof(g_ai_cfg.voiceid));
    memcpy(g_ai_cfg.imei, cfg->imei, sizeof(g_ai_cfg.imei));

    return ws_ai_init(&g_ai_cfg);
}

void ai_client_deinit(void)
{
    ws_ai_deinit();
}

int ai_client_connect(void)
{
    return ws_ai_connect(&g_ai_cfg);
}

int ai_client_disconnect(void)
{
    return ws_ai_disconnect();
}

int ai_client_send_audio(const ai_client_audio_t *audio)
{
    if (audio == NULL || audio->data == NULL) {
        return -1;
    }
    return ws_ai_send_audio(audio->data, audio->size);
}

int ai_client_send_audio_complete(void)
{
    return ws_ai_send_audio_complete(g_ai_cfg.imei);
}

int ai_client_recv_play(void)
{
    return ws_ai_recv_play(g_ai_cfg.imei);
}

int ai_client_cancel(void)
{
    return ws_ai_cancel(g_ai_cfg.imei);
}

bool ai_client_is_connected(void)
{
    return ws_ai_is_connected();
}
