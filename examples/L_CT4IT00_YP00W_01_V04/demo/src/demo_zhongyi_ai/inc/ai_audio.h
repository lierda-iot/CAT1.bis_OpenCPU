/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Audio hardware abstraction: ES8311 codec init, record, play.
 */

#ifndef AI_AUDIO_H
#define AI_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

int ai_audio_init(void);
int ai_audio_record(uint8_t *buf, int buf_size, int duration_ms);
int ai_audio_play(const uint8_t *buf, int len, int timeout_ms);
int ai_audio_play_start(const uint8_t *buf, int len);
bool ai_audio_play_is_done(void);
void ai_audio_play_wait(int timeout_ms);
void ai_audio_stop(void);
int ai_audio_test_tone(void);

#endif /* AI_AUDIO_H */
