#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*audio_evt_cb_t)(int evt, const uint8_t *data, uint32_t len);

typedef struct {
    int8_t   rst_gpio;
    int8_t   boot_gpio;
    int8_t   pa_mode_gpio;
    int8_t   uart_port;
    uint32_t uart_baudrate;
    uint8_t  chat_mode;
    uint8_t  volume;
    uint8_t  vad_timeout_time;
    audio_evt_cb_t evt_cb;
} audio_config_t;

typedef enum {
    AUDIO_PROMPT_NONE = -1,
    AUDIO_PROMPT_POWERON = 0,
    AUDIO_PROMPT_POWEROFF,
    AUDIO_PROMPT_CONNECTING,
    AUDIO_PROMPT_CONNECTED,
    AUDIO_PROMPT_CLOUD_CONNECTED,
    AUDIO_PROMPT_CONNECTFAIL,
    AUDIO_PROMPT_SIM_ERROR,
    AUDIO_PROMPT_LOWPOWER,
    AUDIO_PROMPT_SEND,
    AUDIO_PROMPT_AWAKE,
    AUDIO_PROMPT_ASKTOHELP,
    AUDIO_PROMPT_WAIT,
    AUDIO_PROMPT_SLEEP,
    AUDIO_PROMPT_VOL_MAX,
    AUDIO_PROMPT_VOL_MIN,
    AUDIO_PROMPT_MAX,
} audio_prompt_e;

void audioModuleInit(const audio_config_t *cfg);
void audioModuleDeinit(void);
void audioModulePlayPrompt(audio_prompt_e prompt);
void audioModuleStop(void);
bool audioModuleWaitPlayDone(uint32_t timeout_ms);
void audioModuleStartRecord(void);
void audioModuleStopRecord(void);
int audioModuleReadRecord(uint8_t *buf, uint32_t *len);
void audioModuleWritePlayback(uint8_t *data, uint32_t len);
void audioModuleVolumeUp(void);
void audioModuleVolumeDown(void);
void audioModuleVolumeSet(uint32_t vol);
uint32_t audioModuleVolumeGet(void);

#endif /* AUDIO_MODULE_H */
