#ifndef TIRTC_VIDEO_DRAW_H
#define TIRTC_VIDEO_DRAW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An lv_img-compatible widget for the remote RGB565 video only. Keep the
 * source frame immutable until the synchronous LVGL refresh finishes. All
 * methods, including the statistics getter, run on the existing UI thread.
 * Unsupported styling/render contexts retain LVGL's ordinary image path. */
lv_obj_t *tirtc_video_draw_create(lv_obj_t *parent);

typedef struct {
    uint32_t fast_calls;       /* DRAW_MAIN regions, not complete video frames */
    uint32_t fallback_calls;
    uint32_t pixels;           /* Pixels copied to the active LVGL draw buffer */
} tirtc_video_draw_stats_t;

void tirtc_video_draw_get_stats(tirtc_video_draw_stats_t *stats);

#ifdef __cplusplus
}
#endif
#endif
