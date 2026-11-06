#ifndef TIRTC_CAPTURE_PIPELINE_H
#define TIRTC_CAPTURE_PIPELINE_H

#include "liot_camera.h"
#include "tirtc_video_format.h"
#include <stdbool.h>
#include <stdint.h>

enum {
    TIRTC_CAPTURE_PIPELINE_SERVICE = -1740,
    TIRTC_CAPTURE_PIPELINE_ARGUMENT = -1741,
    TIRTC_CAPTURE_PIPELINE_BUSY = -1742,
    TIRTC_CAPTURE_PIPELINE_UNAVAILABLE = -1743
};

#define TIRTC_CAPTURE_PIPELINE_RAW_BYTES (TIRTC_VIDEO_CAP_W * TIRTC_VIDEO_CAP_H * 2U)

typedef struct {
    uint32_t generation;
    uint32_t captured, taken, replaced, discarded, errors;
    uint32_t last_capture_ms, last_timestamp, consecutive_errors;
    int error;
    bool active, enabled, paused, in_flight;
    unsigned ready, held;
} tirtc_capture_pipeline_stats_t;

/* One persistent priority-11 capture task; audio tasks keep higher priority.
 * The caller still owns camera init/deinit, allocations, and all codecs. */
int tirtc_capture_pipeline_service_init(void);

/* Both buffers must be distinct, nonoverlapping, 4-byte-aligned allocations
 * of at least RAW_BYTES. Their lifetime extends until is_idle(generation).
 * Start requires the preceding generation to be fully stopped, every held
 * buffer released, and an initialized, idle camera completion fence. */
int tirtc_capture_pipeline_start(liot_camera_handle_t camera, uint32_t generation,
                                 uint8_t *raw0, uint8_t *raw1, bool enabled);

/* Disable/pause discards queued frames and invalidates a capture already in
 * progress, without aborting its DMA. Resume queues only fresh frames. HELD
 * leases remain owned by the consumer, which rechecks the switch before send.
 * enabled represents the user's switch; paused is temporary backpressure. */
int tirtc_capture_pipeline_set_enabled(uint32_t generation, bool enabled);
int tirtc_capture_pipeline_set_paused(uint32_t generation, bool paused);

/* Nonblocking latest-frame handoff. On success the consumer exclusively owns
 * the returned slot until release; capture never overwrites a held slot.
 * Output arguments are unchanged on failure. A single codec consumer calls
 * take/release, releasing each successful take exactly once. */
bool tirtc_capture_pipeline_take(uint32_t generation, unsigned *slot,
                                uint8_t **raw, uint32_t *timestamp,
                                uint32_t *capture_ms);
void tirtc_capture_pipeline_release(uint32_t generation, unsigned slot);

/* Stop is asynchronous. A capture timeout retains its buffer until the real
 * DMA completion arrives. Held buffers must still be released. Only after
 * is_idle may the caller deinitialize the camera or free these buffers.
 * A different generation has no lease in this singleton pipeline. */
void tirtc_capture_pipeline_request_stop(uint32_t generation);
bool tirtc_capture_pipeline_is_idle(uint32_t generation);
void tirtc_capture_pipeline_get_stats(tirtc_capture_pipeline_stats_t *out);

#endif
