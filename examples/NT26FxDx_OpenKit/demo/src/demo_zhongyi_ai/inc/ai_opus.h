/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Opus codec wrapper: 16kHz mono, 60ms frames (960 samples).
 */

#ifndef AI_OPUS_H
#define AI_OPUS_H

#include <stdint.h>

#define AI_OPUS_SAMPLE_RATE   16000
#define AI_OPUS_CHANNELS      1
#define AI_OPUS_FRAME_SAMPLES 960
#define AI_OPUS_FRAME_BYTES   (AI_OPUS_FRAME_SAMPLES * 2)
#define AI_OPUS_MAX_PACKET    512

int ai_opus_init(void);
void ai_opus_deinit(void);
int ai_opus_encode(const int16_t *pcm, int frame_samples, uint8_t *out, int max_out);
int ai_opus_decode(const uint8_t *opus_data, int opus_len, int16_t *pcm, int max_samples);

#endif /* AI_OPUS_H */
