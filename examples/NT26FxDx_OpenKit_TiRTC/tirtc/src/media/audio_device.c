/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * ES8311 audio adapter: I2C0/I2S0, PA GPIO11, 16 kHz mono-right PCM.
 */

#include "audio_device.h"

#include <stdint.h>
#include <string.h>

#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

/*
 * Keep the UI at 1..10 while using only the useful hardware ranges found on
 * this board.  Speaker UI level 1 is the former level 5 (volume 30), and
 * microphone UI level 1 is the former level 6 (gain/volume 7/160).  The
 * remaining range is spread over all ten UI steps so every step still has a
 * measurable effect.
 */
static const uint8_t s_speaker_volume_by_level[DEMO_AI_AUDIO_LEVEL_MAX] = {
    30U, 33U, 37U, 40U, 43U, 47U, 50U, 53U, 57U, 60U
};

static const uint8_t s_mic_gain_by_level[DEMO_AI_AUDIO_LEVEL_MAX] = {
    7U, 7U, 8U, 8U, 8U, 9U, 9U, 9U, 10U, 10U
};

static const uint8_t s_mic_volume_by_level[DEMO_AI_AUDIO_LEVEL_MAX] = {
    160U, 168U, 176U, 183U, 191U,
    199U, 207U, 214U, 222U, 230U
};

/* The driver DMA path requires aligned storage for capture. */
static __attribute__((aligned(16))) int16_t
    s_record_frame[DEMO_AI_AUDIO_FRAME_SAMPLES];

/* Liot_AudioInit keeps the configuration pointer for later stop/deinit and
 * callback operations.  It must therefore outlive demo_ai_audio_init(). */
static Liot_AudHwConfig_t s_audio_config;

static volatile bool s_audio_ready;
static volatile bool s_play_done = true;
static volatile bool s_levels_dirty = true;
static volatile uint8_t s_speaker_level = DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static volatile uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;
static demo_ai_audio_owner_e s_audio_owner = DEMO_AI_AUDIO_OWNER_NONE;
static uint32_t s_audio_generation;

static bool demo_ai_audio_lease_valid_locked(
    const demo_ai_audio_lease_t *lease)
{
    return lease != NULL &&
           lease->owner != DEMO_AI_AUDIO_OWNER_NONE &&
           lease->owner == s_audio_owner &&
           lease->generation != 0U &&
           lease->generation == s_audio_generation;
}

bool demo_ai_audio_lease_is_valid(const demo_ai_audio_lease_t *lease)
{
    bool valid;

    liot_rtos_enter_critical();
    valid = demo_ai_audio_lease_valid_locked(lease);
    liot_rtos_exit_critical();
    return valid;
}

int demo_ai_audio_acquire(demo_ai_audio_owner_e owner,
                          demo_ai_audio_lease_t *lease)
{
    int ret = 0;
    demo_ai_audio_owner_e held_owner = DEMO_AI_AUDIO_OWNER_NONE;

    if (owner <= DEMO_AI_AUDIO_OWNER_NONE ||
        owner > DEMO_AI_AUDIO_OWNER_LIVE || lease == NULL)
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }

    liot_rtos_enter_critical();
    if (s_audio_owner != DEMO_AI_AUDIO_OWNER_NONE)
    {
        ret = DEMO_AI_AUDIO_ERR_BUSY;
        held_owner = s_audio_owner;
    }
    else
    {
        s_audio_generation++;
        if (s_audio_generation == 0U)
        {
            s_audio_generation++;
        }
        s_audio_owner = owner;
        lease->owner = owner;
        lease->generation = s_audio_generation;
    }
    liot_rtos_exit_critical();

    if (ret != 0)
    {
        liot_trace("[AI-AUDIO] acquire owner=%u rejected; held by=%u\r\n",
                   (unsigned int)owner, (unsigned int)held_owner);
    }
    else
    {
        liot_trace("[AI-AUDIO] acquired owner=%u generation=%u\r\n",
                   (unsigned int)owner,
                   (unsigned int)lease->generation);
    }
    return ret;
}

int demo_ai_audio_release(demo_ai_audio_lease_t *lease)
{
    demo_ai_audio_owner_e owner;
    uint32_t generation;

    liot_rtos_enter_critical();
    if (!demo_ai_audio_lease_valid_locked(lease))
    {
        liot_rtos_exit_critical();
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    owner = lease->owner;
    generation = lease->generation;
    s_audio_owner = DEMO_AI_AUDIO_OWNER_NONE;
    lease->owner = DEMO_AI_AUDIO_OWNER_NONE;
    lease->generation = 0U;
    liot_rtos_exit_critical();

    liot_trace("[AI-AUDIO] released owner=%u generation=%u\r\n",
               (unsigned int)owner, (unsigned int)generation);
    return 0;
}

static uint8_t demo_ai_audio_clamp_level(uint8_t level)
{
    if (level < DEMO_AI_AUDIO_LEVEL_MIN)
    {
        return DEMO_AI_AUDIO_LEVEL_MIN;
    }
    if (level > DEMO_AI_AUDIO_LEVEL_MAX)
    {
        return DEMO_AI_AUDIO_LEVEL_MAX;
    }
    return level;
}

static void demo_ai_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;

    if (event == L_AUD_EVT_FINISH ||
        event == L_AUD_EVT_ERROR ||
        event == L_AUD_EVT_CLOSE)
    {
        s_play_done = true;
    }
}

bool demo_ai_audio_is_ready(const demo_ai_audio_lease_t *lease)
{
    return demo_ai_audio_lease_is_valid(lease) && s_audio_ready;
}

int demo_ai_audio_apply_levels(const demo_ai_audio_lease_t *lease)
{
    uint8_t speaker_level;
    uint8_t mic_level;
    uint8_t speaker_volume;
    uint8_t mic_gain;
    uint8_t mic_volume;
    Liot_AudErr_e sw_ret;
    Liot_AudErr_e codec_ret;
    Liot_AudErr_e mic_ret;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (!s_audio_ready)
    {
        return -1;
    }
    if (!s_levels_dirty)
    {
        return 0;
    }

    speaker_level = demo_ai_audio_clamp_level(s_speaker_level);
    mic_level = demo_ai_audio_clamp_level(s_mic_level);

    speaker_volume = s_speaker_volume_by_level[speaker_level - 1U];
    mic_gain = s_mic_gain_by_level[mic_level - 1U];
    mic_volume = s_mic_volume_by_level[mic_level - 1U];

    sw_ret = Liot_AudioSetVolume(speaker_volume);
    codec_ret = Liot_AudioSetCodecVolume(speaker_volume);
    mic_ret = Liot_AudioSetMicVolume(mic_gain, mic_volume);

    if (sw_ret != L_AUD_ERR_SUCCESS ||
        codec_ret != L_AUD_ERR_SUCCESS ||
        mic_ret != L_AUD_ERR_SUCCESS)
    {
        s_levels_dirty = true;
        liot_trace("[AI-AUDIO] level apply failed ret=%d/%d/%d\r\n",
                   (int)sw_ret, (int)codec_ret, (int)mic_ret);
        return -2;
    }

    s_levels_dirty = false;
    liot_trace("[AI-AUDIO] levels applied: speaker L%u=%u, "
               "mic L%u gain=%u volume=%u\r\n",
               (unsigned int)speaker_level,
               (unsigned int)speaker_volume,
               (unsigned int)mic_level,
               (unsigned int)mic_gain,
               (unsigned int)mic_volume);
    /* Preserve a simultaneous UI update for the next service point. */
    if (speaker_level != s_speaker_level || mic_level != s_mic_level)
    {
        s_levels_dirty = true;
    }
    return 0;
}

int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *lease,
                             uint8_t speaker_level, uint8_t mic_level)
{
    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    s_speaker_level = demo_ai_audio_clamp_level(speaker_level);
    s_mic_level = demo_ai_audio_clamp_level(mic_level);
    s_levels_dirty = true;

    if (s_audio_ready)
    {
        return demo_ai_audio_apply_levels(lease);
    }
    return 0;
}

int demo_ai_audio_get_levels(const demo_ai_audio_lease_t *lease,
                             uint8_t *speaker_level, uint8_t *mic_level)
{
    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (speaker_level != NULL)
    {
        *speaker_level = s_speaker_level;
    }
    if (mic_level != NULL)
    {
        *mic_level = s_mic_level;
    }
    return 0;
}

int demo_ai_audio_init(const demo_ai_audio_lease_t *lease)
{
    Liot_AudErr_e ret;
    Liot_AudErr_e pause_ret;
    Liot_AudErr_e resume_ret;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }

    if (s_audio_ready)
    {
        /* Liot_AudioStop() leaves F6D_A's internal playback state at STOP.
         * Pause then resume is the supported transition back to IDLE; without
         * it a later Liot_AudioPlay() accepts data but never starts DMA. */
        pause_ret = Liot_AudioPlayPause();
        resume_ret = Liot_AudioPlayResume();
        s_play_done = true;
        s_levels_dirty = true;
        if (pause_ret != L_AUD_ERR_SUCCESS ||
            resume_ret != L_AUD_ERR_SUCCESS)
        {
            liot_trace("[AI-AUDIO] session reset failed ret=%d/%d\r\n",
                       (int)pause_ret, (int)resume_ret);
            return -21;
        }
        if (demo_ai_audio_apply_levels(lease) != 0)
        {
            return -20;
        }
        liot_trace("[AI-AUDIO] session playback state reset\r\n");
        return 0;
    }

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    memset(&s_audio_config, 0, sizeof(s_audio_config));
    s_audio_config.i2cNum = 0;
    s_audio_config.i2sNum = 0;
    s_audio_config.paGpioNum = 11;
    s_audio_config.codecType = L_AUD_ES8311;
    s_audio_config.channel = L_AUD_MONO_RIGHT;
    s_audio_config.role = L_AUD_ROLE_SLAVE;
    s_audio_config.mode = L_AUD_MODE_I2S;
    s_audio_config.frameSize = L_AUD_FRAMESIZE_16_16;
    s_audio_config.samples = L_AUD_16K_SAMPLES;
    s_audio_config.callback = demo_ai_audio_callback;
    s_audio_config.cbContext = NULL;

    ret = Liot_AudioInit(&s_audio_config);
    if (ret != L_AUD_ERR_SUCCESS)
    {
        liot_trace("[AI-AUDIO] ES8311 init failed ret=%d\r\n", (int)ret);
        return -(int)ret - 1;
    }

    s_audio_ready = true;
    s_play_done = true;
    s_levels_dirty = true;
    if (demo_ai_audio_apply_levels(lease) != 0)
    {
        return -20;
    }

    liot_trace("[AI-AUDIO] ready ES8311 I2C0/I2S0 PA11 "
               "16kHz/16bit/mono-right\r\n");
    return 0;
}

int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *lease,
                                  uint8_t speaker_level,
                                  uint8_t mic_level)
{
    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }

    /* demo_ai_audio_init() commits dirty levels on both cold and warm paths.
     * Store the desired values first so the driver is programmed once. */
    s_speaker_level = demo_ai_audio_clamp_level(speaker_level);
    s_mic_level = demo_ai_audio_clamp_level(mic_level);
    s_levels_dirty = true;
    return demo_ai_audio_init(lease);
}

int demo_ai_audio_record_20ms(
    const demo_ai_audio_lease_t *lease,
    int16_t pcm[DEMO_AI_AUDIO_FRAME_SAMPLES])
{
    Liot_AudErr_e ret;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (!s_audio_ready || pcm == NULL)
    {
        return -1;
    }

    (void)demo_ai_audio_apply_levels(lease);
    ret = Liot_AudioRecord((uint8_t *)s_record_frame,
                           (int)DEMO_AI_AUDIO_FRAME_BYTES);
    if (ret != L_AUD_ERR_SUCCESS)
    {
        return -(int)ret - 1;
    }

    memcpy(pcm, s_record_frame, DEMO_AI_AUDIO_FRAME_BYTES);
    return 0;
}

int demo_ai_audio_play(const demo_ai_audio_lease_t *lease,
                       const int16_t *pcm, uint32_t samples)
{
    Liot_AudErr_e ret;
    size_t bytes;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (!s_audio_ready || pcm == NULL || samples == 0U)
    {
        return -1;
    }
    if (samples > ((size_t)INT32_MAX / sizeof(*pcm)))
    {
        return -2;
    }

    (void)demo_ai_audio_apply_levels(lease);
    bytes = samples * sizeof(*pcm);
    s_play_done = false;
    ret = Liot_AudioPlay((uint8_t *)(uintptr_t)pcm, (int)bytes);
    if (ret != L_AUD_ERR_SUCCESS)
    {
        s_play_done = true;
        return -(int)ret - 10;
    }
    return 0;
}

bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *lease)
{
    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return true;
    }
    return s_play_done;
}

int demo_ai_audio_stop(const demo_ai_audio_lease_t *lease)
{
    Liot_AudErr_e ret;

    if (!demo_ai_audio_lease_is_valid(lease))
    {
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    }
    if (!s_audio_ready)
    {
        s_play_done = true;
        return 0;
    }

    ret = Liot_AudioStop();
    s_play_done = true;
    return (ret == L_AUD_ERR_SUCCESS) ? 0 : (-(int)ret - 1);
}
