#ifndef TIRTC_VIDEO_H
#define TIRTC_VIDEO_H
#include <stdbool.h>
#include <stdint.h>
#include "tirtc_runtime.h"
typedef struct {
    uint32_t generation, tx_frames, rx_frames, dropped_frames;
    uint32_t capture_ms, encode_ms, decode_ms;
    bool active, ready, camera_enabled;
    int error;
} tirtc_video_snapshot_t;
/* One low-priority owner serializes camera and JPEG codec use. Commands are
 * bounded, asynchronous RAM operations. stop/is_stopped fence all in-flight
 * camera DMA, codec work and managed sends before the call releases its lease. */
int tirtc_video_start_service(void);
/* incoming is the device's call direction, not the later WX join notification.
 * Freeze display orientation for this generation; camera upload is unchanged. */
int tirtc_video_start(demo_tirtc_owner_e owner, uint32_t generation, bool wechat,
                     bool incoming);
/* Remote viewing uploads stream 11 only. Initial camera permission is committed
 * before the worker can run; later subscription changes use set_enabled.
 * Repeating an active generation preserves its current enable state. */
int tirtc_video_start_live(uint32_t generation, bool enabled);
void tirtc_video_stop(uint32_t generation);
bool tirtc_video_is_stopped(uint32_t generation);
int tirtc_video_set_enabled(uint32_t generation, bool enabled);
void tirtc_video_receive(uint32_t generation, const TIRTCFRAMEINFO *frame, const void *data);
void tirtc_video_get_snapshot(tirtc_video_snapshot_t *out);
/* Exact, complete /v1/device/profile payload: MJPEG, never simulated H.264. */
const char *tirtc_video_profile_json(void);
#endif
