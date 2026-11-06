/* Packed logical rectangles for ST7789 hardware landscape (MADCTL 0x68).
 * Keep the previous bounds/clip behavior without a per-pixel permutation. */
#ifndef TIRTC_LCD_LANDSCAPE_H
#define TIRTC_LCD_LANDSCAPE_H
#include "tirtc_rotation.h"

static inline int tirtc_lcd_pack_landscape(void *pixel_data, size_t pixels_capacity,
                                         const tirtc_rotation_area_t *logical,
                                         tirtc_rotation_area_t *clipped)
{
    int32_t x1, y1, x2, y2;
    size_t source_width, source_height, width, height, row;
    uint8_t *pixels = (uint8_t *)pixel_data;
    if (pixels == NULL || logical == NULL || clipped == NULL)
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
    /* LVGL's normal clipped rectangle takes the no-copy path. For an
     * oversized caller rectangle, retain its original stride when packing. */
    if (x1 != logical->x1 || y1 != logical->y1 || width != source_width) {
        for (row = 0; row < height; ++row) {
            size_t source = ((size_t)(y1 - logical->y1) + row) * source_width +
                            (size_t)(x1 - logical->x1);
            if (source != row * width)
                memmove(pixels + row * width * sizeof(uint16_t),
                        pixels + source * sizeof(uint16_t), width * sizeof(uint16_t));
        }
    }
    clipped->x1 = (int16_t)x1; clipped->y1 = (int16_t)y1;
    clipped->x2 = (int16_t)x2; clipped->y2 = (int16_t)y2;
    return TIRTC_ROTATION_OK;
}
#endif
