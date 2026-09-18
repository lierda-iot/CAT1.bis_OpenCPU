/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal Opus adapter for TiRTC AI audio.
 */

#ifndef DEMO_AI_OPUS_H
#define DEMO_AI_OPUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEMO_AI_OPUS_SAMPLE_RATE    16000
#define DEMO_AI_OPUS_CHANNELS       1
#define DEMO_AI_OPUS_FRAME_MS       20

#ifndef DEMO_AI_PCM_SAMPLES_20MS
#define DEMO_AI_PCM_SAMPLES_20MS    320U
#endif
#ifndef DEMO_AI_PCM_BYTES_20MS
#define DEMO_AI_PCM_BYTES_20MS      640U
#endif

#define DEMO_AI_OPUS_FRAME_SAMPLES  DEMO_AI_PCM_SAMPLES_20MS
#define DEMO_AI_OPUS_FRAME_BYTES    DEMO_AI_PCM_BYTES_20MS
#define DEMO_AI_OPUS_BITRATE        24000
/* RFC 6716 maximum Opus packet size; output bitrate is server-controlled. */
#define DEMO_AI_OPUS_MAX_PACKET     1275

/* Initialization is idempotent. Encoder/decoder instances are reused. */
int demo_ai_opus_init(void);
void demo_ai_opus_reset(void);
void demo_ai_opus_deinit(void);

/* Returns encoded byte count, or a negative Opus/custom error. */
int demo_ai_opus_encode_20ms(
    const int16_t pcm[DEMO_AI_OPUS_FRAME_SAMPLES],
    uint8_t *packet,
    int packet_capacity);

/* Returns decoded samples per channel, or a negative Opus/custom error. */
int demo_ai_opus_decode(
    const uint8_t *packet,
    int packet_size,
    int16_t *pcm,
    int pcm_capacity_samples);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_AI_OPUS_H */
