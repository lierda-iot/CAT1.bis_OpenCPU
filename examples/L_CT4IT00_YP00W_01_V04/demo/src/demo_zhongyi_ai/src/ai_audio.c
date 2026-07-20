/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Audio hardware: ES8311 on I2C0, I2S0, PA=GPIO8, 16kHz mono.
 */

#include "ai_audio.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "ai_app_log.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define AI_AUDIO_SAMPLE_RATE    16000
#define AI_AUDIO_BYTES_PER_MS   (AI_AUDIO_SAMPLE_RATE * 2 / 1000)
#define AI_AUDIO_CHUNK_SIZE     5120
#define AI_AUDIO_PLAY_TIMEOUT_S 10

static __attribute__((aligned(16))) uint8_t s_record_chunk[AI_AUDIO_CHUNK_SIZE];
static Liot_AudHwConfig_t s_audio_cfg;
static volatile bool s_play_done;

static void ai_audio_cb(Liot_AudEvent_e event, void *context)
{
    (void)context;
    if (event == L_AUD_EVT_FINISH || event == L_AUD_EVT_ERROR) {
        s_play_done = true;
    }
}

int ai_audio_init(void)
{
    Liot_AudErr_e ret;

    liot_rtos_task_sleep_ms(2000);

    /* Power domain setup */
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL); /* 3V3 enable for codec */

    s_audio_cfg.i2cNum    = 0;
    s_audio_cfg.i2sNum    = 0;
    s_audio_cfg.paGpioNum = 11;
    s_audio_cfg.codecType = L_AUD_ES8311;
    s_audio_cfg.channel   = L_AUD_MONO_RIGHT;
    s_audio_cfg.role      = L_AUD_ROLE_SLAVE;
    s_audio_cfg.mode      = L_AUD_MODE_I2S;
    s_audio_cfg.frameSize = L_AUD_FRAMESIZE_16_16;
    s_audio_cfg.samples   = L_AUD_16K_SAMPLES;
    s_audio_cfg.callback  = ai_audio_cb;
    s_audio_cfg.cbContext = NULL;

    ret = Liot_AudioInit(&s_audio_cfg);
    if (ret != L_AUD_ERR_SUCCESS) {
        liot_trace("ai_audio init failed ret=%d", (int)ret);
        return -1;
    }

    Liot_AudioSetVolume(50);
    Liot_AudioSetCodecVolume(50);
    Liot_AudioSetMicVolume(8, 200);

    liot_trace("ai_audio init ok (ES8311 16kHz mono)");
    return 0;
}

int ai_audio_record(uint8_t *buf, int buf_size, int duration_ms)
{
    int need_bytes = duration_ms * AI_AUDIO_BYTES_PER_MS;
    int offset = 0;
    Liot_AudErr_e ret;

    if (buf == NULL || buf_size <= 0) {
        return -1;
    }
    if (need_bytes > buf_size) {
        need_bytes = buf_size;
    }

    liot_trace("ai_audio record start %d bytes (%d ms)", need_bytes, duration_ms);

    while (offset < need_bytes) {
        int chunk = need_bytes - offset;
        if (chunk > AI_AUDIO_CHUNK_SIZE) {
            chunk = AI_AUDIO_CHUNK_SIZE;
        }
        ret = Liot_AudioRecord(s_record_chunk, chunk);
        if (ret != L_AUD_ERR_SUCCESS) {
            liot_trace("ai_audio record chunk failed ret=%d offset=%d", (int)ret, offset);
            return -1;
        }
        memcpy(buf + offset, s_record_chunk, chunk);
        offset += chunk;
    }

    liot_trace("ai_audio record done %d bytes", offset);
    return offset;
}

int ai_audio_play(const uint8_t *buf, int len, int timeout_ms)
{
    Liot_AudErr_e ret;
    int timeout_s = (timeout_ms + 999) / 1000;

    if (buf == NULL || len <= 0) {
        return -1;
    }

    s_play_done = false;

    ret = Liot_AudioPlay((uint8_t *)buf, len);
    if (ret != L_AUD_ERR_SUCCESS) {
        liot_trace("ai_audio play failed ret=%d", (int)ret);
        return -1;
    }

    ret = Liot_AudioWaitPlayFinish(timeout_s);
    if (ret != L_AUD_ERR_SUCCESS) {
        liot_trace("ai_audio play wait timeout");
        Liot_AudioStop();
        return -2;
    }

    return 0;
}

int ai_audio_play_start(const uint8_t *buf, int len)
{
    Liot_AudErr_e ret;

    if (buf == NULL || len <= 0) {
        return -1;
    }

    s_play_done = false;
    ret = Liot_AudioPlay((uint8_t *)buf, len);
    if (ret != L_AUD_ERR_SUCCESS) {
        s_play_done = true;
        return -1;
    }
    return 0;
}

bool ai_audio_play_is_done(void)
{
    return s_play_done;
}

void ai_audio_play_wait(int timeout_ms)
{
    int timeout_s = (timeout_ms + 999) / 1000;
    Liot_AudErr_e ret = Liot_AudioWaitPlayFinish(timeout_s);
    if (ret != L_AUD_ERR_SUCCESS) {
        Liot_AudioStop();
    }
}

void ai_audio_stop(void)
{
    s_play_done = true;
}

int ai_audio_test_tone(void)
{
    static __attribute__((aligned(16))) int16_t tone[1600];
    int i;

    for (i = 0; i < 1600; i++) {
        /* 1kHz sine at 16kHz sample rate: period = 16 samples */
        int phase = i % 16;
        static const int16_t sin_table[16] = {
            0, 12539, 23170, 30273, 32767, 30273, 23170, 12539,
            0, -12539, -23170, -30273, -32767, -30273, -23170, -12539
        };
        tone[i] = sin_table[phase];
    }

    liot_trace("ai_audio test tone 1kHz 100ms");
    Liot_AudioPlay((uint8_t *)tone, sizeof(tone));
    Liot_AudioWaitPlayFinish(3);
    return 0;
}
