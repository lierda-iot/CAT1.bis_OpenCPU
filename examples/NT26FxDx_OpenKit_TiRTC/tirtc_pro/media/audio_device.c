/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT
 *
 * Derived from xiaotai-lierda f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93.
 * V04 ES8311: I2C0/I2S0, PA GPIO11, 16 kHz mono-right PCM16.
 * The display port owns the shared 3.3V rail. No power/reset/deinit here.
 */
#include "audio_device.h"
#include <string.h>
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include "liot_os.h"

/* Match the upstream V04 demo's useful 1..10 hardware range. Keep level 0
 * as an explicit mute in this port: ADC attenuation alone is not a mute,
 * so capture is also zeroed and AI suppresses its upload at mic level 0. */
static const uint8_t s_speaker_volume[] = {0, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60};
static const uint8_t s_dev_codec_volume[] = {0, 38, 41, 46, 50, 53, 57, 61, 64, 69, 72};
/* Match Lite's lower capture curve; retain Pro's explicit L0 mute. */
static const uint8_t s_mic_gain[] = {0, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9};
static const uint8_t s_mic_volume[] = {0, 160, 164, 169, 173, 177, 182, 186, 190, 195, 199};
/* Match Lite: one 20 ms SDK transaction per public frame. */
#define CAPTURE_BATCH_FRAMES 1U
#define CAPTURE_BATCH_SAMPLES (DEMO_AI_AUDIO_FRAME_SAMPLES * CAPTURE_BATCH_FRAMES)
static __attribute__((aligned(16))) int16_t s_record_frame[CAPTURE_BATCH_SAMPLES];
/* Work around isolated full-scale capture impulses observed in SDK Record
 * blocks. This does not repair the underlying capture fault or analog
 * clipping. Limit it to DEV/GROUP uplink, before their 16k-to-8k conversion.
 * Four quiet, consistent ORIGINAL neighbours are required: checking just
 * the adjacent pair would also remove legitimate 4 kHz waveform peaks.
 * The independently captured blocks must never borrow previous-frame data. */
static uint32_t audio_guard_impulses(demo_ai_audio_owner_e owner,
                                        const int16_t *raw, int16_t *output)
{
    uint32_t cleaned = 0U;
    const uint32_t count = DEMO_AI_AUDIO_FRAME_SAMPLES;

    if (owner != DEMO_AI_AUDIO_OWNER_DEVICE
#ifdef HWDEMO_GROUP_ROOM_EN
        && owner != DEMO_AI_AUDIO_OWNER_GROUP_ROOM
#endif
        ) {
        return 0U;
    }
    if (count < 5U) {
        return 0U;
    }
    for (uint32_t i = 0U; i < count; ++i) {
        uint32_t first, neighbours = 0U;
        int16_t local[4];
        int32_t value = raw[i];
        bool quiet = true;

        if (value > -28000 && value < 28000) {
            continue;
        }
        first = i > 2U ? i - 2U : 0U;
        if (first > count - 5U) first = count - 5U;
        for (uint32_t j = first; j < first + 5U; ++j) {
            if (j == i) continue;
            if (raw[j] < -2000 || raw[j] > 2000) {
                quiet = false;
                break;
            }
            local[neighbours++] = raw[j];
        }
        if (!quiet) continue;

        if (i > 0U && i + 1U < count) {
            int32_t difference = (int32_t)raw[i - 1U] - raw[i + 1U];
            if (difference < -1000 || difference > 1000) continue;
            output[i] = (int16_t)(((int32_t)raw[i - 1U] + raw[i + 1U]) / 2);
        } else {
            /* Median of the four local samples at either block edge. */
            for (uint32_t j = 1U; j < 4U; ++j) {
                uint32_t k = j;
                int16_t item = local[j];
                while (k > 0U && local[k - 1U] > item) {
                    local[k] = local[k - 1U];
                    --k;
                }
                local[k] = item;
            }
            if ((int32_t)local[3] - local[0] > 1000) continue;
            output[i] = (int16_t)(((int32_t)local[1] + local[2]) / 2);
        }
        ++cleaned;
    }
    return cleaned;
}

/* Keep every SDK submission within one <=5120-byte queue item: a larger
 * call can enqueue a prefix before failing, hiding accepted silence from
 * WARM_PARTIAL accounting. OP_PLAY serializes this copied scratch buffer. */
#if !DEMO_AI_AUDIO_AEC_ENABLE
#define WARMUP_SAMPLES (DEMO_AI_AUDIO_WARMUP_MS * (DEMO_AI_AUDIO_SAMPLE_RATE / 1000U))
#define SESSION_CHUNK_MS 150U
#define SESSION_CHUNK_SAMPLES (SESSION_CHUNK_MS * (DEMO_AI_AUDIO_SAMPLE_RATE / 1000U))
static __attribute__((aligned(16))) int16_t s_warm_frame[SESSION_CHUNK_SAMPLES];
#endif
static bool s_session_warm_pending;
/* GROUP warms the DAC clocks even at L0; a codec-volume write does not restart
 * those clocks. Other owners retain their existing level-specific preamble. */
static bool s_session_prewarmed;
static uint32_t s_record_offset, s_record_cached_frames;
/* Captured samples omitted by controls/suppression still occupy media time.
 * This is a per-session modulo-32-bit millisecond accumulator, not wall time. */
static uint32_t s_discarded_capture_ms;
/* The real SDK retains this pointer, rather than copying the configuration. */
static Liot_AudHwConfig_t s_audio_config;
static volatile bool s_audio_ready;
static bool s_play_done = true;
static volatile bool s_stopped = true;
static volatile bool s_levels_dirty = true;
static uint8_t s_speaker_level = DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;
static demo_ai_audio_owner_e s_audio_owner;
static uint32_t s_audio_generation;
static volatile uint8_t s_active;
static int s_last_record_error, s_last_play_error;
/* PCM amplitudes and control-pin samples only: no speech bytes are logged.
 * SDK callbacks update counters; the media task emits at most one report per
 * five seconds (plus the first frame and the final stop summary). */
static struct {
    uint32_t submissions, samples, nonzero, pa_high, pa_low, pa_unknown;
    uint32_t starts, finishes, errors, closes, last_log_ms;
    uint32_t peak;
    uint32_t warm_starts, warm_ms;
    bool logged;
} s_diagnostic;
/* Capture statistics describe SDK PCM before the software mute (post-3A in
 * the AEC backend, raw ADC otherwise). All
 * timings are milliseconds. Only the record/control owner accesses this. */
static struct {
    uint32_t frames, errors, window_frames, window_samples, near_clip, zero_frames;
    uint32_t peak, reads, read_sum_ms, read_max_ms, gaps, gap_sum_ms, gap_max_ms;
    uint32_t last_end_ms, last_log_ms;
    uint64_t absolute_sum;
    bool have_end, logged;
} s_capture_diagnostic;

enum { OP_RECORD = 1, OP_PLAY = 2, OP_CONTROL = 4 };

static void discard_capture_buffer(void)
{
    s_discarded_capture_ms += s_record_cached_frames * DEMO_AI_AUDIO_FRAME_MS;
    s_record_offset = s_record_cached_frames = 0;
}

static bool lease_valid(const demo_ai_audio_lease_t *lease)
{
    return lease && lease->owner != DEMO_AI_AUDIO_OWNER_NONE &&
           lease->owner == s_audio_owner && lease->generation != 0 &&
           lease->generation == s_audio_generation;
}

bool demo_ai_audio_lease_is_valid(const demo_ai_audio_lease_t *lease)
{
    bool valid;
    liot_rtos_enter_critical();
    valid = lease_valid(lease);
    liot_rtos_exit_critical();
    return valid;
}

/* Locks cover only bookkeeping; never SDK I/O, logging or waiting. Control
 * operations are serialized with PCM calls. Record and playback may coexist. */
static int enter(const demo_ai_audio_lease_t *lease, uint8_t op, bool needs_ready)
{
    int result = 0;
    liot_rtos_enter_critical();
    if (!lease_valid(lease)) result = DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    else if ((op == OP_CONTROL && s_active) || (s_active & (op | OP_CONTROL)))
        result = DEMO_AI_AUDIO_ERR_BUSY;
    else if (needs_ready && (!s_audio_ready || s_stopped)) result = -1;
    else s_active |= op;
    liot_rtos_exit_critical();
    return result;
}

static void leave(uint8_t op)
{
    liot_rtos_enter_critical();
    s_active &= (uint8_t)~op;
    liot_rtos_exit_critical();
}

int demo_ai_audio_acquire(demo_ai_audio_owner_e owner, demo_ai_audio_lease_t *lease)
{
    int result = 0;
    if (!lease || owner <= DEMO_AI_AUDIO_OWNER_NONE || owner >= DEMO_AI_AUDIO_OWNER_COUNT)
        return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    liot_rtos_enter_critical();
    if (s_audio_owner != DEMO_AI_AUDIO_OWNER_NONE) result = DEMO_AI_AUDIO_ERR_BUSY;
    else {
        if (++s_audio_generation == 0) ++s_audio_generation;
        s_audio_owner = owner;
        discard_capture_buffer();
        s_discarded_capture_ms = 0;
        lease->owner = owner;
        lease->generation = s_audio_generation;
    }
    liot_rtos_exit_critical();
    if (!result) TIRTC_LOG_DEBUG("[AI-AUDIO] acquired owner=%u generation=%lu\r\n",
                           (unsigned)owner, (unsigned long)lease->generation);
    return result;
}

int demo_ai_audio_release(demo_ai_audio_lease_t *lease)
{
    int result = 0;
    liot_rtos_enter_critical();
    if (!lease_valid(lease)) result = DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    else if (s_active || (s_audio_ready && !s_stopped)) result = DEMO_AI_AUDIO_ERR_BUSY;
    else {
        s_audio_owner = DEMO_AI_AUDIO_OWNER_NONE;
        discard_capture_buffer();
        s_discarded_capture_ms = 0;
        lease->owner = DEMO_AI_AUDIO_OWNER_NONE;
        lease->generation = 0;
    }
    liot_rtos_exit_critical();
    if (!result) TIRTC_LOG_DEBUG("[AI-AUDIO] lease released\r\n");
    return result;
}

static uint8_t clamp(uint8_t level)
{
    return level > DEMO_AI_AUDIO_LEVEL_MAX ? DEMO_AI_AUDIO_LEVEL_MAX : level;
}

static bool group_owner(void)
{
#ifdef HWDEMO_GROUP_ROOM_EN
    return s_audio_owner == DEMO_AI_AUDIO_OWNER_GROUP_ROOM;
#else
    return false;
#endif
}

#if DEMO_AI_AUDIO_AEC_ENABLE
static int aec_apply_levels(const demo_ai_audio_lease_t *lease, bool playback_service);
#include "audio_aec.inc"
#endif

static void audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    /* The SDK invokes these from its side task, not the DMA ISR. Delayed
     * FINISH is advisory only; callers must never use it to free DMA state. */
    liot_rtos_enter_critical();
    if (event == L_AUD_EVT_START) ++s_diagnostic.starts;
    else if (event == L_AUD_EVT_FINISH) ++s_diagnostic.finishes;
    else if (event == L_AUD_EVT_ERROR) ++s_diagnostic.errors;
    else if (event == L_AUD_EVT_CLOSE) ++s_diagnostic.closes;
    if (event == L_AUD_EVT_FINISH || event == L_AUD_EVT_ERROR || event == L_AUD_EVT_CLOSE)
        s_play_done = true;
    liot_rtos_exit_critical();
}

#if !DEMO_AI_AUDIO_AEC_ENABLE
static void playback_diagnostic(const int16_t *pcm, uint32_t samples, bool final)
{
    uint32_t peak = 0, nonzero = 0;
    /* The audited F6D_D tick wrapper returns milliseconds. */
    uint32_t now_ms = liot_rtos_get_running_time();
    int pa = -1;
    for (uint32_t i = 0; i < samples; ++i) {
        int32_t value = pcm[i];
        uint32_t absolute = (uint32_t)(value < 0 ? -value : value);
        if (absolute > peak) peak = absolute;
        if (value) ++nonzero;
    }
    if (samples) {
        /* This readback is a momentary pin level, not proof of speaker output. */
        pa = (int)Liot_GpioGetLevel(L_GPIO_11);
        ++s_diagnostic.submissions;
        s_diagnostic.samples += samples;
        s_diagnostic.nonzero += nonzero;
        if (peak > s_diagnostic.peak) s_diagnostic.peak = peak;
        if (pa == L_IO_HIGH) ++s_diagnostic.pa_high;
        else if (pa == L_IO_LOW) ++s_diagnostic.pa_low;
        else ++s_diagnostic.pa_unknown;
    }
    if (!s_diagnostic.submissions || (!final && s_diagnostic.logged &&
        (uint32_t)(now_ms - s_diagnostic.last_log_ms) < 5000U)) return;
    s_diagnostic.peak = 0;
    s_diagnostic.last_log_ms = now_ms;
    s_diagnostic.logged = true;
}
#endif

static void capture_report(uint32_t now_ms, bool final)
{
    if (!s_capture_diagnostic.reads || (!final && s_capture_diagnostic.logged &&
        (uint32_t)(now_ms - s_capture_diagnostic.last_log_ms) < 5000U)) return;
    s_capture_diagnostic.window_frames = s_capture_diagnostic.window_samples = 0;
    s_capture_diagnostic.near_clip = s_capture_diagnostic.zero_frames = 0;
    s_capture_diagnostic.peak = s_capture_diagnostic.reads = s_capture_diagnostic.read_sum_ms = 0;
    s_capture_diagnostic.read_max_ms = s_capture_diagnostic.gaps = s_capture_diagnostic.gap_sum_ms = 0;
    s_capture_diagnostic.gap_max_ms = 0;
    s_capture_diagnostic.absolute_sum = 0;
    s_capture_diagnostic.last_log_ms = now_ms;
    s_capture_diagnostic.logged = true;
}

static void capture_diagnostic(const int16_t *pcm, uint32_t samples, int result,
                               uint32_t started_ms, uint32_t ended_ms)
{
    uint32_t peak = 0, elapsed = ended_ms - started_ms;
    ++s_capture_diagnostic.reads;
    s_capture_diagnostic.read_sum_ms += elapsed;
    if (elapsed > s_capture_diagnostic.read_max_ms) s_capture_diagnostic.read_max_ms = elapsed;
    if (s_capture_diagnostic.have_end) {
        uint32_t gap = started_ms - s_capture_diagnostic.last_end_ms;
        ++s_capture_diagnostic.gaps; s_capture_diagnostic.gap_sum_ms += gap;
        if (gap > s_capture_diagnostic.gap_max_ms) s_capture_diagnostic.gap_max_ms = gap;
    }
    s_capture_diagnostic.have_end = true; s_capture_diagnostic.last_end_ms = ended_ms;
    if (result) ++s_capture_diagnostic.errors;
    else {
        ++s_capture_diagnostic.frames; ++s_capture_diagnostic.window_frames;
        s_capture_diagnostic.window_samples += samples;
        for (uint32_t i = 0; i < samples; ++i) {
            int32_t value = pcm[i];
            uint32_t absolute = (uint32_t)(value < 0 ? -value : value);
            if (absolute > peak) peak = absolute;
            s_capture_diagnostic.absolute_sum += absolute;
            if (absolute >= 32760U) ++s_capture_diagnostic.near_clip;
        }
        if (!peak) ++s_capture_diagnostic.zero_frames;
        if (peak > s_capture_diagnostic.peak) s_capture_diagnostic.peak = peak;
    }
    capture_report(ended_ms, false);
}

bool demo_ai_audio_is_ready(const demo_ai_audio_lease_t *lease)
{
    bool ready;
    liot_rtos_enter_critical();
    ready = lease_valid(lease) && s_audio_ready && !s_stopped
#if DEMO_AI_AUDIO_AEC_ENABLE
        && s_aec_running && !s_aec_fault && !s_aec_stopping
#endif
        ;
    liot_rtos_exit_critical();
    return ready;
}

/* The caller owns OP_CONTROL. No capture/play path performs I2C writes. */
static int apply_levels(void)
{
#if DEMO_AI_AUDIO_AEC_ENABLE
    demo_ai_audio_lease_t lease = { s_audio_owner, s_audio_generation };
    return aec_apply_levels(&lease, false);
#else
    Liot_AudErr_e sw, codec, mic;
    uint8_t software_volume = s_speaker_volume[s_speaker_level];
    uint8_t codec_volume = s_speaker_volume[s_speaker_level];
    if (s_audio_owner == DEMO_AI_AUDIO_OWNER_DEVICE || group_owner()) {
        software_volume = 10U;
        codec_volume = s_dev_codec_volume[s_speaker_level];
    }
    if (!s_audio_ready) return -1;
    if (!s_levels_dirty) return 0;
    /* G.711 already spans PCM16: preserve its waveform at software unity.
     * Apply the saved speaker level at the codec, as in DEV playback. */
    sw = Liot_AudioSetVolume(software_volume);
    codec = Liot_AudioSetCodecVolume(codec_volume);
    mic = Liot_AudioSetMicVolume(s_mic_gain[s_mic_level], s_mic_volume[s_mic_level]);
    /* Only a real commit reaches here; unchanged settings never flood logs. */
    if (sw || codec || mic || TIRTC_ENABLE_DEBUG_LOG) liot_trace("[AI-AUDIO] levels speaker=%u sw=%u codec=%u mic=%u gain=%u adc=%u ret=%d/%d/%d\r\n",
               s_speaker_level, software_volume, codec_volume, s_mic_level,
               s_mic_gain[s_mic_level], s_mic_volume[s_mic_level], sw, codec, mic);
    if (sw || codec || mic) {
        return -20;
    }
    s_levels_dirty = false;
    return 0;
#endif
}

int demo_ai_audio_apply_levels(const demo_ai_audio_lease_t *lease)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    result = apply_levels();
    leave(OP_CONTROL);
    return result;
}

int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *lease, uint8_t speaker, uint8_t mic)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    speaker = clamp(speaker); mic = clamp(mic);
    if (s_session_prewarmed && !group_owner() && speaker != s_speaker_level) {
        s_session_warm_pending = true;
        s_session_prewarmed = false;
    }
    if (speaker != s_speaker_level || mic != s_mic_level) {
        s_levels_dirty = true;
        discard_capture_buffer();
    }
    s_speaker_level = speaker; s_mic_level = mic;
    result = s_audio_ready ? apply_levels() : 0;
    leave(OP_CONTROL);
    return result;
}

int demo_ai_audio_get_levels(const demo_ai_audio_lease_t *lease, uint8_t *speaker, uint8_t *mic)
{
    int result = 0;
    liot_rtos_enter_critical();
    if (!lease_valid(lease)) result = DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    else {
        if (speaker) *speaker = s_speaker_level;
        if (mic) *mic = s_mic_level;
    }
    liot_rtos_exit_critical();
    return result;
}

/* OP_CONTROL held. The app initializes the shared rail before reaching here. */
static int init_audio(void)
{
#if DEMO_AI_AUDIO_AEC_ENABLE
    int result;
    if (s_aec_fault) return AEC_ERR_FAULT;
    if (s_audio_ready) {
        if (!s_stopped) return apply_levels();
        result = aec_start();
        if (result) return result;
    } else {
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
        s_audio_config.callback = audio_callback;
        s_audio_config.use3A = 1;
        if (!s_aec_hw_ready) {
            Liot_AudAnsConfig_t ans = { .bypass = 0, .mode = 1 };
            Liot_AudAgcConfig_t agc = { .bypass = 1, .targetLevel = 3,
                                      .compressionGain = 6, .limiterEnable = 1 };
            Liot_AudAecConfig_t aec = { .bypass = 0, .delay = DEMO_AI_AUDIO_AEC_DELAY,
                                      .cngMode = 1, .echoMode = 1, .nlpFlag = 0 };
            if (Liot_AudioSetAns(&ans) || Liot_AudioSetAgc(&agc) || Liot_AudioSetAec(&aec))
                return AEC_ERR_FAULT;
            result = Liot_AudioInit(&s_audio_config);
            if (result) return -result - 1;
            s_aec_hw_ready = true;
        }
        result = aec_start();
        if (result) return result;
        s_audio_ready = true;
    }
    s_levels_dirty = true;
    result = apply_levels();
    if (!result) {
        s_stopped = false;
        s_play_done = true;
        s_session_warm_pending = s_session_prewarmed = false;
        s_last_record_error = s_last_play_error = 0;
        discard_capture_buffer();
        liot_rtos_enter_critical();
        memset(&s_diagnostic, 0, sizeof(s_diagnostic));
        memset(&s_capture_diagnostic, 0, sizeof(s_capture_diagnostic));
        liot_rtos_exit_critical();
    }
    liot_trace("[AI-AUDIO] Pro ES8311 F6D_D AEC ready ret=%d\r\n", result);
    return result;
#else
    int result;
    if (s_audio_ready) {
        if (!s_stopped) return apply_levels();
        /* Audited F6D_D Pause sets state=3; Resume accepts 3 and restores idle=1.
         * Stop leaves state=5, which otherwise prevents TryPlay starting DMA. */
        int paused = Liot_AudioPlayPause();
        int resumed = Liot_AudioPlayResume();
        if (paused || resumed) {
            liot_trace("[AI-AUDIO] warm reset failed ret=%d/%d\r\n", paused, resumed);
            return -21;
        }
    } else {
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
        s_audio_config.callback = audio_callback;
        s_audio_config.use3A = 0; /* Preserve ordinary PCM operation; enabling 3A is a separate change. */
        result = Liot_AudioInit(&s_audio_config);
        if (result) {
            liot_trace("[AI-AUDIO] ES8311 init failed ret=%d\r\n", result);
            return -result - 1;
        }
        s_audio_ready = true;
        liot_trace("[AI-AUDIO] ES8311 I2C0/I2S0 PA11 16000Hz/16bit/mono-right no-AEC\r\n");
    }
    s_levels_dirty = true;
    result = apply_levels();
    if (!result) {
        s_stopped = false;
        s_play_done = true;
        s_session_warm_pending = true;
        s_session_prewarmed = false;
        s_last_record_error = s_last_play_error = 0;
        discard_capture_buffer();
        liot_rtos_enter_critical();
        memset(&s_diagnostic, 0, sizeof(s_diagnostic));
        memset(&s_capture_diagnostic, 0, sizeof(s_capture_diagnostic));
        liot_rtos_exit_critical();
    }
    return result;
#endif
}

int demo_ai_audio_init(const demo_ai_audio_lease_t *lease)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    discard_capture_buffer();
    s_discarded_capture_ms = 0;
    result = init_audio();
    leave(OP_CONTROL);
    return result;
}

int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *lease, uint8_t speaker, uint8_t mic)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    discard_capture_buffer();
    s_discarded_capture_ms = 0;
    if (s_session_prewarmed && !group_owner() && clamp(speaker) != s_speaker_level) {
        s_session_warm_pending = true;
        s_session_prewarmed = false;
    }
    if (clamp(speaker) != s_speaker_level || clamp(mic) != s_mic_level) {
        s_levels_dirty = true;
        discard_capture_buffer();
    }
    s_speaker_level = clamp(speaker);
    s_mic_level = clamp(mic);
    result = init_audio();
    leave(OP_CONTROL);
    return result;
}

int demo_ai_audio_record_20ms(const demo_ai_audio_lease_t *lease,
                            int16_t pcm[DEMO_AI_AUDIO_FRAME_SAMPLES])
{
    int result;
    uint32_t started_ms, ended_ms;
    if (!pcm) return -1;
    result = enter(lease, OP_RECORD, true);
    if (result) return result;
#if DEMO_AI_AUDIO_AEC_ENABLE
    started_ms = liot_rtos_get_running_time();
    result = aec_record(lease, pcm);
    ended_ms = liot_rtos_get_running_time();
    capture_diagnostic(s_record_frame, DEMO_AI_AUDIO_FRAME_SAMPLES, result, started_ms, ended_ms);
    if (result) {
        memset(pcm, 0, DEMO_AI_AUDIO_FRAME_BYTES);
        s_last_record_error = result;
        leave(OP_RECORD);
        return result;
    }
    if (!s_mic_level) memset(pcm, 0, DEMO_AI_AUDIO_FRAME_BYTES);
#else
    if (!s_record_cached_frames) {
        started_ms = liot_rtos_get_running_time();
        result = Liot_AudioRecord((uint8_t *)s_record_frame, (int)sizeof(s_record_frame));
        ended_ms = liot_rtos_get_running_time();
        capture_diagnostic(s_record_frame, CAPTURE_BATCH_SAMPLES, result, started_ms, ended_ms);
        if (result) {
            discard_capture_buffer();
            memset(pcm, 0, DEMO_AI_AUDIO_FRAME_BYTES);
            if (result != s_last_record_error)
                liot_trace("[AI-AUDIO] record failed ret=%d\r\n", result);
            s_last_record_error = result;
            leave(OP_RECORD);
            return -result - 1;
        }
        s_record_offset = 0;
        s_record_cached_frames = CAPTURE_BATCH_FRAMES;
    }
    if (!s_mic_level) memset(pcm, 0, DEMO_AI_AUDIO_FRAME_BYTES);
    else {
        const int16_t *raw = s_record_frame + s_record_offset;
        memcpy(pcm, raw, DEMO_AI_AUDIO_FRAME_BYTES);
        (void)audio_guard_impulses(lease->owner, raw, pcm);
    }
    s_record_offset += DEMO_AI_AUDIO_FRAME_SAMPLES;
    --s_record_cached_frames;
#endif
    s_last_record_error = 0;
    leave(OP_RECORD);
    return 0;
}

int demo_ai_audio_discard_capture(const demo_ai_audio_lease_t *lease)
{
    return demo_ai_audio_discard_capture_timed(lease, NULL);
}

int demo_ai_audio_discard_capture_timed(const demo_ai_audio_lease_t *lease,
                                      uint32_t *discarded_ms)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    discard_capture_buffer();
    if (discarded_ms) *discarded_ms = s_discarded_capture_ms;
    s_discarded_capture_ms = 0;
    leave(OP_CONTROL);
    return 0;
}

/* OP_PLAY held. Queue 1200 ms without waiting for DMA: connection work can
 * run while these samples advance the codec ramp. The final 80 ms remains
 * attached to the first speech item, including after a natural queue FINISH. */
#if !DEMO_AI_AUDIO_AEC_ENABLE
static int queue_session_silence(uint32_t *accepted_ms)
{
    int result;
    *accepted_ms = 0;
    memset(s_warm_frame, 0, sizeof(s_warm_frame));
    for (uint32_t left = DEMO_AI_AUDIO_SESSION_WARMUP_MS - DEMO_AI_AUDIO_WARMUP_MS;
         left; left -= SESSION_CHUNK_MS) {
        result = Liot_AudioPlay((uint8_t *)s_warm_frame, (int)sizeof(s_warm_frame));
        if (result) return result;
        *accepted_ms += SESSION_CHUNK_MS;
    }
    return 0;
}
#endif

int demo_ai_audio_prewarm_session(const demo_ai_audio_lease_t *lease, uint32_t *queued_ms)
{
    int result;
#if !DEMO_AI_AUDIO_AEC_ENABLE
    uint32_t accepted_ms = 0;
#endif
    if (!queued_ms) return -1;
    *queued_ms = 0;
    result = enter(lease, OP_PLAY, true);
    if (result) return result;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if (s_aec_fault) { leave(OP_PLAY); return AEC_ERR_FAULT; }
    s_session_warm_pending = s_session_prewarmed = false;
    leave(OP_PLAY);
    return 0;
#else
    /* Room must finish its cold preamble before join/receive, including L0.
     * Otherwise unmuting could insert 1200ms ahead of a 500ms RX queue. */
    if ((!s_speaker_level && !group_owner()) || !s_session_warm_pending) {
        leave(OP_PLAY);
        return 0;
    }
    liot_rtos_enter_critical();
    s_play_done = false;
    liot_rtos_exit_critical();
    result = queue_session_silence(&accepted_ms);
    if (!result) {
        s_session_warm_pending = false;
        s_session_prewarmed = true;
        *queued_ms = accepted_ms;
    }
    s_last_play_error = result;
    leave(OP_PLAY);
    if (result || TIRTC_ENABLE_DEBUG_LOG) liot_trace("[AI-AUDIO] connect prewarm queued=%lums ret=%d\r\n",
               (unsigned long)accepted_ms, result);
    if (result && accepted_ms) return DEMO_AI_AUDIO_ERR_WARM_PARTIAL;
    return result ? -result - 10 : 0;
#endif
}

static int queue_playback(const demo_ai_audio_lease_t *lease, const int16_t *pcm,
                          uint32_t samples, bool warm, uint32_t *extra_ms)
{
    int result;
#if !DEMO_AI_AUDIO_AEC_ENABLE
    uint32_t queued_samples = samples;
    uint32_t extra = warm ? DEMO_AI_AUDIO_WARMUP_MS : 0;
    uint32_t silent_queued = 0;
    const int16_t *queued_pcm = pcm;
#else
    (void)warm;
#endif
    if (extra_ms) *extra_ms = 0;
    /* Speech remains bounded to 20 ms. Only the burst-start item includes
     * explicit silence, so scheduling cannot split preheat from its speech. */
    /* Audited F6D_D Liot_I2sSend still truncates bytes to a multiple of four. Reject odd
     * PCM16 counts here instead of silently losing the final speech sample;
     * 8 kHz network samples expand to aligned 16 kHz PCM sample pairs. */
    if (!pcm || !samples || (samples & 1U) || samples > DEMO_AI_AUDIO_FRAME_SAMPLES) return -1;
    result = enter(lease, OP_PLAY, true);
    if (result) return result;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if (s_aec_fault) { leave(OP_PLAY); return AEC_ERR_FAULT; }
#endif
    if (!s_speaker_level) { leave(OP_PLAY); return 0; }
#if DEMO_AI_AUDIO_AEC_ENABLE
    /* No preamble: the 3A worker supplies only its required DMA priming. */
    result = aec_play(lease, pcm, samples);
    s_last_play_error = result;
    leave(OP_PLAY);
    return result;
#else
    liot_rtos_enter_critical();
    s_play_done = false;
    liot_rtos_exit_critical();
    if (warm && s_session_warm_pending) {
        /* Retain the configured 1280 ms session settling time. The first
         * 1200 ms uses eight 150 ms / 4800-byte SDK items; the final
         * 80 ms stays attached to the first speech frame. Each call
         * fits one SDK item so a failure cannot hide a queued prefix. */
        result = queue_session_silence(&silent_queued);
        if (result) goto play_result;
        extra += silent_queued;
    }
    if (warm) {
        memset(s_warm_frame, 0, WARMUP_SAMPLES * sizeof(*s_warm_frame));
        memcpy(s_warm_frame + WARMUP_SAMPLES, pcm, samples * sizeof(*pcm));
        queued_pcm = s_warm_frame;
        queued_samples += WARMUP_SAMPLES;
    }
    result = Liot_AudioPlay((uint8_t *)(uintptr_t)queued_pcm, (int)(queued_samples * sizeof(*queued_pcm)));
play_result:
    if (result) {
        liot_rtos_enter_critical();
        s_play_done = true;
        liot_rtos_exit_critical();
        if (result != s_last_play_error) liot_trace("[AI-AUDIO] play failed ret=%d\r\n", result);
    } else {
        if (warm) {
            s_session_warm_pending = false;
            s_session_prewarmed = false;
            ++s_diagnostic.warm_starts;
            s_diagnostic.warm_ms += extra;
            if (extra_ms) *extra_ms = extra;
        }
        playback_diagnostic(pcm, samples, false);
    }
    s_last_play_error = result;
    leave(OP_PLAY);
    if (result && silent_queued) {
        liot_trace("[AI-AUDIO] session warm partial=%lums; safe stop required\r\n",
                   (unsigned long)silent_queued);
        return DEMO_AI_AUDIO_ERR_WARM_PARTIAL;
    }
    return result ? -result - 10 : 0;
#endif
}

int demo_ai_audio_play(const demo_ai_audio_lease_t *lease, const int16_t *pcm, uint32_t samples)
{
    return queue_playback(lease, pcm, samples, false, NULL);
}

int demo_ai_audio_play_warm(const demo_ai_audio_lease_t *lease, const int16_t *pcm,
                          uint32_t samples, uint32_t *queued_extra_ms)
{
    if (!queued_extra_ms) return -1;
    return queue_playback(lease, pcm, samples, true, queued_extra_ms);
}

bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *lease)
{
    bool done;
    liot_rtos_enter_critical();
    done = !lease_valid(lease);
#if !DEMO_AI_AUDIO_AEC_ENABLE
    done = done || s_play_done;
#endif
    liot_rtos_exit_critical();
#if DEMO_AI_AUDIO_AEC_ENABLE
    if (!done) done = aec_play_done();
#endif
    return done;
}

int demo_ai_audio_stop(const demo_ai_audio_lease_t *lease)
{
    int result = enter(lease, OP_CONTROL, false);
    if (result) return result;
    discard_capture_buffer();
    if (s_audio_ready && !s_stopped) {
#if DEMO_AI_AUDIO_AEC_ENABLE
        capture_report(liot_rtos_get_running_time(), true);
        result = aec_stop(lease);
        if (!result) { s_stopped = true; s_levels_dirty = true; }
#else
        capture_report(liot_rtos_get_running_time(), true);
        playback_diagnostic(NULL, 0, true);
        /* Mute before the asynchronous stop request. No shared-power toggle
         * and no attempt to deinit a blocked DMA or free SDK-owned buffers. */
        int sw = Liot_AudioSetVolume(0);
        int codec = Liot_AudioSetCodecVolume(0);
        int stopped = Liot_AudioStop();
        if (sw || codec || stopped) {
            liot_trace("[AI-AUDIO] stop failed ret=%d/%d/%d\r\n", sw, codec, stopped);
            result = -22;
        } else {
            s_stopped = true;
            s_levels_dirty = true;
        }
#endif
    }
    leave(OP_CONTROL);
    return result;
}
