#ifndef TIRTC_VIDEO_FORMAT_H
#define TIRTC_VIDEO_FORMAT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Restore the full VGA field of view and the previously verified CSPI
 * decimation path: complete YUYV pairs become QVGA without interpolation.
 * Explicit zero offsets avoid a cropped sensor window. */
#define TIRTC_VIDEO_SENSOR_W 640U
#define TIRTC_VIDEO_SENSOR_H 480U
#define TIRTC_VIDEO_SENSOR_X 0U
#define TIRTC_VIDEO_SENSOR_Y 0U
#define TIRTC_VIDEO_SCALE 1U
#define TIRTC_VIDEO_CAP_W 320U
#define TIRTC_VIDEO_CAP_H 240U
#define TIRTC_VIDEO_ENC_W 320U
#define TIRTC_VIDEO_ENC_H 240U
#define TIRTC_VIDEO_JPEG_QUALITY 50U
#define TIRTC_VIDEO_TARGET_FPS 15U

#if TIRTC_VIDEO_SENSOR_W == 0U || TIRTC_VIDEO_SENSOR_H == 0U || \
    TIRTC_VIDEO_SENSOR_W + TIRTC_VIDEO_SENSOR_X > 640U || \
    TIRTC_VIDEO_SENSOR_H + TIRTC_VIDEO_SENSOR_Y > 480U || \
    TIRTC_VIDEO_SENSOR_X % 2U != 0U || TIRTC_VIDEO_SENSOR_Y % 2U != 0U
#error "The native sensor window must be even and remain inside VGA"
#endif
#if TIRTC_VIDEO_SCALE > 15U || \
    TIRTC_VIDEO_SENSOR_W % (TIRTC_VIDEO_SCALE + 1U) != 0U || \
    TIRTC_VIDEO_SENSOR_H % (TIRTC_VIDEO_SCALE + 1U) != 0U || \
    TIRTC_VIDEO_SENSOR_W / (TIRTC_VIDEO_SCALE + 1U) != TIRTC_VIDEO_CAP_W || \
    TIRTC_VIDEO_SENSOR_H / (TIRTC_VIDEO_SCALE + 1U) != TIRTC_VIDEO_CAP_H
#error "Sensor decimation must match the complete DMA frame"
#endif
/* Audited original F6D_A YUYV path: round (pixels/4000 + 1) up to a
 * multiple of four descriptors, clamp to 80, and transfer 7680 bytes each.
 * Several otherwise valid camera sizes do NOT match that DMA length. Fail
 * compilation instead of allowing an overrun or an incomplete last row. */
#define TIRTC_VIDEO_DMA_BLOCKS_RAW \
    (((TIRTC_VIDEO_CAP_W * TIRTC_VIDEO_CAP_H / 4000U + 1U) + 3U) & ~3U)
#define TIRTC_VIDEO_DMA_BLOCKS \
    (TIRTC_VIDEO_DMA_BLOCKS_RAW > 80U ? 80U : TIRTC_VIDEO_DMA_BLOCKS_RAW)
#define TIRTC_VIDEO_DMA_BYTES (TIRTC_VIDEO_DMA_BLOCKS * 7680U)
#if TIRTC_VIDEO_DMA_BYTES != TIRTC_VIDEO_CAP_W * TIRTC_VIDEO_CAP_H * 2U
#error "Original SDK DMA length must exactly match the raw frame"
#endif
#if TIRTC_VIDEO_ENC_W == 0U || TIRTC_VIDEO_ENC_H == 0U || \
    TIRTC_VIDEO_ENC_W > TIRTC_VIDEO_CAP_W || \
    TIRTC_VIDEO_ENC_H != TIRTC_VIDEO_CAP_H || \
    (TIRTC_VIDEO_CAP_W - TIRTC_VIDEO_ENC_W) % 4U != 0U || \
    TIRTC_VIDEO_ENC_W % 16U != 0U || TIRTC_VIDEO_ENC_H % 8U != 0U
#error "The YUYV window must preserve chroma pairs and complete JPEG MCUs"
#endif

/* raw holds a complete CAP_W*CAP_H*2 frame and must stay exclusively leased
 * through preparation and JPEG encoding. DMA must not reuse it then.
 * Matching dimensions require no copying. For a narrower output, each row
 * starts at the centre crop's even pixel offset, preserving YUYV chroma pairs.
 * Pack ENC_W*ENC_H*2 bytes at the allocation start. Rows overlap, so copy
 * forward with memmove, never memcpy. */
static inline void tirtc_video_prepare_yuyv(uint8_t *raw)
{
#if TIRTC_VIDEO_ENC_W == TIRTC_VIDEO_CAP_W
    (void)raw;
    return;
#else
    const uint32_t source_stride = TIRTC_VIDEO_CAP_W * 2U;
    const uint32_t output_stride = TIRTC_VIDEO_ENC_W * 2U;
    const uint32_t left_bytes = TIRTC_VIDEO_CAP_W - TIRTC_VIDEO_ENC_W;
    uint32_t y;
    for (y = 0; y < TIRTC_VIDEO_ENC_H; ++y) {
        memmove(raw + y * output_stride,
                raw + y * source_stride + left_bytes,
                output_stride);
    }
#endif
}

typedef struct {
    uint32_t next;
    unsigned remainder;
} tirtc_video_rate_t;

/* All comparisons use monotonic milliseconds; intervals must be <2^31 ms. */
static inline void tirtc_video_rate_reset(tirtc_video_rate_t *rate, uint32_t now)
{
    rate->next = now;
    rate->remainder = 0U;
}

static inline bool tirtc_video_rate_due(const tirtc_video_rate_t *rate,
                                      uint32_t now)
{
    return (int32_t)(now - rate->next) >= 0;
}

/* Advance once per admitted frame. Fractional milliseconds produce the
 * 66/67/67 cadence, exactly 15 periods per second. A late worker never queues
 * catch-up frames: once a whole interval elapsed, restart its cadence at now.
 * Advancing before the deadline is harmless and does not consume a slot. */
static inline void tirtc_video_rate_advance(tirtc_video_rate_t *rate,
                                          uint32_t now)
{
    unsigned fraction;
    uint32_t period;
    if (!tirtc_video_rate_due(rate, now)) return;

    fraction = rate->remainder + 1000U % TIRTC_VIDEO_TARGET_FPS;
    period = 1000U / TIRTC_VIDEO_TARGET_FPS;
    if (fraction >= TIRTC_VIDEO_TARGET_FPS) {
        fraction -= TIRTC_VIDEO_TARGET_FPS;
        ++period;
    }
    if ((uint32_t)(now - rate->next) >= period) {
        rate->next = now;
        fraction = 1000U % TIRTC_VIDEO_TARGET_FPS;
        period = 1000U / TIRTC_VIDEO_TARGET_FPS;
    }
    rate->remainder = fraction;
    rate->next += period;
}

#endif
