/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared ES8311 audio adapter for TiRTC AI/WX/DEV/LIVE media paths.
 */

#ifndef DEMO_AI_AUDIO_H
#define DEMO_AI_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEMO_AI_AUDIO_SAMPLE_RATE        16000U
#define DEMO_AI_AUDIO_CHANNELS           1U
#define DEMO_AI_AUDIO_BITS_PER_SAMPLE    16U
#define DEMO_AI_AUDIO_FRAME_MS           20U

#ifndef DEMO_AI_PCM_SAMPLES_20MS
#define DEMO_AI_PCM_SAMPLES_20MS         320U
#endif
#ifndef DEMO_AI_PCM_BYTES_20MS
#define DEMO_AI_PCM_BYTES_20MS           640U
#endif

#define DEMO_AI_AUDIO_FRAME_SAMPLES      DEMO_AI_PCM_SAMPLES_20MS
#define DEMO_AI_AUDIO_FRAME_BYTES        DEMO_AI_PCM_BYTES_20MS

#define DEMO_AI_AUDIO_LEVEL_MIN          1U
#define DEMO_AI_AUDIO_LEVEL_MAX          10U
#define DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL  8U
#define DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL  10U

/*
 * ES8311/I2S0 is a process-wide singleton.  A fixed owner plus generation
 * makes a lease immune to ABA reuse: delayed cleanup from an old session can
 * no longer stop or reconfigure the feature that acquired audio afterwards.
 * The structure is caller-owned and uses no dynamic allocation.
 */
typedef enum
{
    DEMO_AI_AUDIO_OWNER_NONE = 0,
    DEMO_AI_AUDIO_OWNER_AI,
    DEMO_AI_AUDIO_OWNER_WECHAT,
    DEMO_AI_AUDIO_OWNER_DEVICE,
    DEMO_AI_AUDIO_OWNER_LIVE
} demo_ai_audio_owner_e;

typedef struct
{
    demo_ai_audio_owner_e owner;
    uint32_t generation;
} demo_ai_audio_lease_t;

#define DEMO_AI_AUDIO_LEASE_INIT \
    { DEMO_AI_AUDIO_OWNER_NONE, 0U }

#define DEMO_AI_AUDIO_ERR_INVALID_LEASE  (-30)
#define DEMO_AI_AUDIO_ERR_BUSY           (-31)

int demo_ai_audio_acquire(demo_ai_audio_owner_e owner,
                          demo_ai_audio_lease_t *lease);
int demo_ai_audio_release(demo_ai_audio_lease_t *lease);
bool demo_ai_audio_lease_is_valid(const demo_ai_audio_lease_t *lease);

/*
 * Initializes ES8311 once. Repeated successful calls are harmless.
 * There is deliberately no deinit API: Liot_AudioDeInit() resets the F6D
 * module on the validated board/firmware combination.
 */
int demo_ai_audio_init(const demo_ai_audio_lease_t *lease);
bool demo_ai_audio_is_ready(const demo_ai_audio_lease_t *lease);

/* Prepare one session with the requested levels in a single hardware commit.
 * This avoids the duplicate volume writes caused by init() followed by
 * set_levels(), while preserving the required warm-session Pause/Resume. */
int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *lease,
                                  uint8_t speaker_level,
                                  uint8_t mic_level);

/* Synchronous capture of exactly 20 ms of signed 16-bit little-endian PCM. */
int demo_ai_audio_record_20ms(
    const demo_ai_audio_lease_t *lease,
    int16_t pcm[DEMO_AI_AUDIO_FRAME_SAMPLES]);

/*
 * Queues PCM for asynchronous playback. Liot_AudioPlay() copies the input
 * into its internal queue before returning, so the caller may immediately
 * reuse pcm. A non-zero return is the backpressure/error indication.
 */
int demo_ai_audio_play(const demo_ai_audio_lease_t *lease,
                       const int16_t *pcm, uint32_t samples);
bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *lease);
int demo_ai_audio_stop(const demo_ai_audio_lease_t *lease);

/* Ten user-facing levels; out-of-range values are clamped to 1..10. */
int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *lease,
                             uint8_t speaker_level, uint8_t mic_level);
int demo_ai_audio_get_levels(const demo_ai_audio_lease_t *lease,
                             uint8_t *speaker_level, uint8_t *mic_level);
int demo_ai_audio_apply_levels(const demo_ai_audio_lease_t *lease);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_AI_AUDIO_H */
