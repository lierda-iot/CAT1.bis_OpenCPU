/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal, stateless G.711 A-law codec.
 */

#ifndef DEMO_G711_H
#define DEMO_G711_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert one signed 16-bit linear PCM sample to/from G.711 A-law. */
uint8_t demo_g711_alaw_encode_sample(int16_t pcm_sample);
int16_t demo_g711_alaw_decode_sample(uint8_t alaw_sample);

/*
 * Convert sample_count samples. The input and output buffers must not overlap.
 * Returns sample_count on success, or 0 when a non-empty buffer is NULL.
 */
size_t demo_g711_alaw_encode(const int16_t *pcm,
                             uint8_t *alaw,
                             size_t sample_count);
size_t demo_g711_alaw_decode(const uint8_t *alaw,
                             int16_t *pcm,
                             size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_G711_H */
