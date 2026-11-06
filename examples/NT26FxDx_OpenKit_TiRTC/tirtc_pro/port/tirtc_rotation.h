/* Counterclockwise RGB565 and FT6336 coordinate adapter for the Lierda V04 UI.
 * Release 04 is rotated 180 degrees relative to release 03's physical image.
 * Header-only so firmware and standalone tests compile the same implementation.
 * No LVGL calls: the display driver must keep sw_rotate=0 and rotated=NONE.
 */
#ifndef TIRTC_ROTATION_H
#define TIRTC_ROTATION_H

#include "tirtc_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TIRTC_ROTATION_VISITED_BYTES \
    ((TIRTC_SCREEN_WIDTH * TIRTC_SCREEN_HEIGHT + 7U) / 8U)

typedef struct {
    int16_t x1, y1, x2, y2;
} tirtc_rotation_area_t;

enum {
    TIRTC_ROTATION_OK = 0,
    TIRTC_ROTATION_OUTSIDE = 1,
    TIRTC_ROTATION_INVALID = -1
};

/* Pack a clipped logical dirty rectangle, then rotate it in place. The full
 * draw buffer is ordinary LVGL partial-render storage (not direct_mode).
 * For every logical point: physical_x=logical_y, physical_y=319-logical_x.
 * The visited bitmap costs 9,600 bytes at most, without a second framebuffer.
 * pixels_capacity bounds the supplied allocation, not the clipped pixel count.
 * Errors/outside rectangles leave pixels and output area untouched.
 */
static inline int tirtc_rotate_rgb565_ccw90(void *pixel_data, size_t pixels_capacity,
                                          const tirtc_rotation_area_t *logical,
                                          tirtc_rotation_area_t *physical,
                                          uint8_t *visited, size_t visited_size)
{
    int32_t x1, y1, x2, y2;
    uint8_t *pixels = (uint8_t *)pixel_data;
    size_t source_width, source_height, width, height, count, row, start;

    if (pixels == NULL || logical == NULL || physical == NULL || visited == NULL)
        return TIRTC_ROTATION_INVALID;
    x1 = logical->x1; y1 = logical->y1;
    x2 = logical->x2; y2 = logical->y2;
    if (x2 < x1 || y2 < y1) return TIRTC_ROTATION_INVALID;
    if (x2 < 0 || y2 < 0 || x1 >= (int32_t)TIRTC_SCREEN_WIDTH ||
        y1 >= (int32_t)TIRTC_SCREEN_HEIGHT) return TIRTC_ROTATION_OUTSIDE;

    source_width = (size_t)(x2 - x1 + 1);
    source_height = (size_t)(y2 - y1 + 1);
    if (source_height > pixels_capacity / source_width) return TIRTC_ROTATION_INVALID;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= (int32_t)TIRTC_SCREEN_WIDTH) x2 = TIRTC_SCREEN_WIDTH - 1U;
    if (y2 >= (int32_t)TIRTC_SCREEN_HEIGHT) y2 = TIRTC_SCREEN_HEIGHT - 1U;
    width = (size_t)(x2 - x1 + 1);
    height = (size_t)(y2 - y1 + 1);
    count = width * height;
    if (visited_size < (count + 7U) / 8U) return TIRTC_ROTATION_INVALID;

    /* Forward packing is safe: each source row begins at/after its destination. */
    for (row = 0; row < height; row++) {
        size_t source = ((size_t)(y1 - logical->y1) + row) * source_width +
                        (size_t)(x1 - logical->x1);
        if (source != row * width)
            memmove(pixels + row * width * sizeof(uint16_t), pixels + source * sizeof(uint16_t),
                    width * sizeof(uint16_t));
    }
    memset(visited, 0, (count + 7U) / 8U);

    /* Rotate the row-major array by following permutation cycles. An input
     * (x,y) becomes (y,width-1-x), in an output of size height x width. */
    for (start = 0; start < count; start++) {
        size_t current;
        uint16_t carried;
        if (visited[start >> 3] & (uint8_t)(1U << (start & 7U))) continue;
        current = start;
        /* memcpy avoids aliasing LVGL's color union as a uint16_t array. */
        memcpy(&carried, pixels + current * sizeof(uint16_t), sizeof(carried));
        do {
            size_t next = (width - 1U - current % width) * height + current / width;
            uint16_t displaced;
            memcpy(&displaced, pixels + next * sizeof(uint16_t), sizeof(displaced));
            memcpy(pixels + next * sizeof(uint16_t), &carried, sizeof(carried));
            carried = displaced;
            visited[current >> 3] |= (uint8_t)(1U << (current & 7U));
            current = next;
        } while (current != start);
    }
    physical->x1 = (int16_t)y1;
    physical->x2 = (int16_t)y2;
    physical->y1 = (int16_t)(TIRTC_PHYSICAL_HEIGHT - 1U - (uint32_t)x2);
    physical->y2 = (int16_t)(TIRTC_PHYSICAL_HEIGHT - 1U - (uint32_t)x1);
    return TIRTC_ROTATION_OK;
}

/* Keep the proven raw-FT6336 -> portrait calibration, then apply the inverse
 * of the pixel rotation. LVGL receives final logical coordinates exactly once.
 * Reject invalid raw points before subtraction; never clamp them into a press.
 */
static inline bool tirtc_touch_to_landscape(int32_t raw_x, int32_t raw_y,
                                           bool portrait_rotated_180,
                                           uint16_t *logical_x, uint16_t *logical_y)
{
    uint16_t px, py;
    if (logical_x == NULL || logical_y == NULL || raw_x < 0 || raw_y < 0 ||
        raw_x >= (int32_t)TIRTC_PHYSICAL_WIDTH || raw_y >= (int32_t)TIRTC_PHYSICAL_HEIGHT)
        return false;
    px = (uint16_t)raw_x; py = (uint16_t)raw_y;
    if (portrait_rotated_180) {
        px = TIRTC_PHYSICAL_WIDTH - 1U - px;
        py = TIRTC_PHYSICAL_HEIGHT - 1U - py;
    }
    *logical_x = TIRTC_PHYSICAL_HEIGHT - 1U - py;
    *logical_y = px;
    return true;
}

#endif
