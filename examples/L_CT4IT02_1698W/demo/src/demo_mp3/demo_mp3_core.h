#ifndef DEMO_MP3_CORE_H
#define DEMO_MP3_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pure MP3 frame/state logic extracted from demo_mp3_stream.c.
 * No liot/lvgl dependency: host-testable as-is.
 */

typedef struct {
    const char *title;
    const uint8_t *mp3_start;
    const uint8_t *mp3_end;
    const uint8_t *cover_start;
    const uint8_t *cover_end;
} mp3_track_t;

typedef enum {
    MP3_STATE_STOPPED = 0,
    MP3_STATE_PLAYING,
    MP3_STATE_PAUSED,
    MP3_STATE_LOADING,
    MP3_STATE_ERROR,
} mp3_state_t;

uint32_t mp3_seed_size(const uint8_t *start, const uint8_t *end);
uint32_t mp3_frame_offset(const mp3_track_t *track);
uint32_t mp3_sample_rate(uint32_t version, uint32_t idx);
uint32_t mp3_bitrate(uint32_t version, uint32_t layer, uint32_t idx);
uint32_t mp3_estimate_duration_ms(const mp3_track_t *track, uint32_t base);
uint32_t mp3_core_current_ms(mp3_state_t state, bool play_finished,
                             uint32_t duration_ms, uint32_t pause_started_ms,
                             uint32_t started_tick, uint32_t paused_total_ms,
                             uint32_t now_ms);
void mp3_format_time(char *buf, size_t len, uint32_t ms);
const char *mp3_state_name(mp3_state_t s);

#endif /* DEMO_MP3_CORE_H */
