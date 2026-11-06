#ifndef TIRTC_CAMERA_FENCE_H
#define TIRTC_CAMERA_FENCE_H

#include "liot_camera.h"
#include <stdbool.h>
#include <stdint.h>

enum {
    TIRTC_CAMERA_FENCE_UNAVAILABLE = -1720,
    TIRTC_CAMERA_FENCE_BUSY = -1721,
    TIRTC_CAMERA_FENCE_TIMEOUT = -1722,
    TIRTC_CAMERA_FENCE_ARGUMENT = -1723,
    TIRTC_CAMERA_FENCE_REARM_STOP = -1724,
    TIRTC_CAMERA_FENCE_REARM_FLUSH = -1725
};

/* Reserve the completion signal BEFORE Liot_CameraInit. False must abort the
 * open without entering the vendor init (which ignores low-level failures).
 * A native DMA initialization failure disables further opens until reboot. */
bool tirtc_camera_fence_prepare(void);

/* Single video-worker owner. Returns 0 only after both the SDK call succeeded
 * and this capture's true DMA completion arrived. On any error, check idle:
 * while false the DMA buffer must not be encoded, reused, or freed. */
int tirtc_camera_capture(liot_camera_handle_t camera, uint8_t *buffer,
                         uint32_t timeout_ms);
bool tirtc_camera_fence_ready(void);
bool tirtc_camera_fence_idle(void);

/* Call before Liot_CameraDeinit and before freeing the video allocation.
 * False retains the buffer/handle lease. It never invents DMA completion or
 * issues a nonblocking abort; a late DMA_END allows a later close to succeed. */
bool tirtc_camera_quiesce(uint32_t timeout_ms);

typedef struct {
    uint32_t captures, completions, timeouts, sdk_early_returns;
    uint32_t unexpected_events, last_wait_ms;
    /* Timing for the most recent capture call which actually started and
     * returned. Retained while the next capture is in_flight; invalid before
     * any successful call and after a failed/timeout call. Invalid values
     * must not be displayed as zero latency. A late DMA never publishes them.
     * dma: call start -> observed DMA ISR (after vendor callback).
     * resume: observed ISR -> end-of-wait near capture return; this includes
     * any remaining SDK wait and task scheduling delay.
     * sdk: wall time inside Liot_CameraCaptureImage.
     * These overlap and include scheduling/ISR delays, not sensor FPS or
     * exclusive CPU/hardware execution times. */
    uint32_t last_dma_ms, last_resume_ms, last_sdk_ms;
    bool last_timing_valid;
    int last_sdk_result;
    bool in_flight;
    /* Cumulative idle-only stop/flush admissions and failures. A failed
     * prepare never starts an SDK capture/DMA and invalidates timing; prior
     * last_sdk_* fields still describe the preceding SDK call. The result
     * below is the vendor ctrl result, zero after a successful preparation. */
    uint32_t rearm_prepares, rearm_stop_errors, rearm_flush_errors;
    int last_rearm_result;
} tirtc_camera_fence_stats_t;
void tirtc_camera_fence_get_stats(tirtc_camera_fence_stats_t *out);

#endif
