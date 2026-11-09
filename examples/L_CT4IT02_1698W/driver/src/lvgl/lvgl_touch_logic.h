#ifndef LVGL_TOUCH_LOGIC_H
#define LVGL_TOUCH_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LVGL_TOUCH_EVENT_DOWN = 0,
    LVGL_TOUCH_EVENT_UP = 1,
    LVGL_TOUCH_EVENT_MOVE = 2,
    LVGL_TOUCH_EVENT_NONE = 3,
} lvgl_touch_event_t;

typedef struct {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    int16_t max_x;
    int16_t max_y;
} lvgl_touch_transform_cfg_t;

typedef struct {
    int16_t last_x;
    int16_t last_y;
    bool release_pending;
    bool release_deferred;
} lvgl_touch_read_state_t;

typedef struct {
    uint8_t touch_cnt;
    lvgl_touch_event_t event;
    int16_t x;
    int16_t y;
} lvgl_touch_sample_t;

static inline void lvgl_touch_read_state_init(lvgl_touch_read_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->last_x = 0;
    state->last_y = 0;
    state->release_pending = false;
    state->release_deferred = false;
}

static inline bool lvgl_touch_sample_is_pressed(const lvgl_touch_sample_t *sample)
{
    return sample != NULL &&
           sample->touch_cnt > 0 &&
           (sample->event == LVGL_TOUCH_EVENT_DOWN || sample->event == LVGL_TOUCH_EVENT_MOVE);
}

static inline bool lvgl_touch_sample_is_release(const lvgl_touch_sample_t *sample)
{
    return sample != NULL &&
           (sample->touch_cnt == 0 || sample->event == LVGL_TOUCH_EVENT_UP);
}

static inline void lvgl_touch_read_state_feed(lvgl_touch_read_state_t *state,
                                              const lvgl_touch_sample_t *sample)
{
    if (state == NULL || sample == NULL) {
        return;
    }

    if (lvgl_touch_sample_is_pressed(sample)) {
        state->last_x = sample->x;
        state->last_y = sample->y;
        state->release_pending = false;
        return;
    }

    if (lvgl_touch_sample_is_release(sample)) {
        state->release_pending = true;
    }
}

static inline bool lvgl_touch_read_state_prepare_release(lvgl_touch_read_state_t *state,
                                                         bool has_final_sample)
{
    if (state == NULL) {
        return false;
    }

    state->release_pending = false;

    if (!has_final_sample || state->release_deferred) {
        return false;
    }

    state->release_deferred = true;
    return true;
}

static inline void lvgl_touch_read_state_consume_release(lvgl_touch_read_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->release_deferred = false;
}

static inline void lvgl_touch_transform_point(const lvgl_touch_transform_cfg_t *cfg,
                                              int32_t in_x,
                                              int32_t in_y,
                                              int32_t *out_x,
                                              int32_t *out_y)
{
    int32_t x = in_x;
    int32_t y = in_y;

    if (cfg != NULL) {
        if (cfg->swap_xy) {
            int32_t tmp = x;
            x = y;
            y = tmp;
        }

        if (cfg->mirror_x && cfg->max_x >= 0) {
            x = cfg->max_x - x;
        }

        if (cfg->mirror_y && cfg->max_y >= 0) {
            y = cfg->max_y - y;
        }

        if (cfg->max_x >= 0) {
            if (x < 0) {
                x = 0;
            } else if (x > cfg->max_x) {
                x = cfg->max_x;
            }
        }

        if (cfg->max_y >= 0) {
            if (y < 0) {
                y = 0;
            } else if (y > cfg->max_y) {
                y = cfg->max_y;
            }
        }
    }

    if (out_x != NULL) {
        *out_x = x;
    }

    if (out_y != NULL) {
        *out_y = y;
    }
}

#endif
