/* V04/F6D_A application workaround for the SDK LSPI DMA segment alignment.
 * The actual 314x47 pressed-button flush (29516 bytes) is split by the SDK
 * into four 7379-byte blocks despite its 4-byte DMA transfer width.
 * Round BEFORE rendering: RGB565 byte count is then a multiple of 64.
 * For this 320x240 buffer the SDK uses at most 19 segments; both of its
 * splitting branches then retain 4-byte segment sizes/source addresses.
 * A shared X grid also preserves alignment when LVGL unions dirty areas.
 * Y is unchanged. No padding pixels, extra buffer, or vendor edits are used.
 */
#ifndef TIRTC_LCD_ROUNDER_H
#define TIRTC_LCD_ROUNDER_H
#include "tirtc_port.h"
#include "lvgl.h"

#if TIRTC_SCREEN_WIDTH != 320U || TIRTC_SCREEN_HEIGHT != 240U
#error "Recheck F6D_A LSPI segmentation before changing the display dimensions"
#endif

static inline void tirtc_lcd_rounder(lv_disp_drv_t *driver, lv_area_t *area)
{
    (void)driver;
    if (area == NULL) return;
    if (area->x1 < 0) area->x1 = 0;
    if (area->x2 >= (lv_coord_t)TIRTC_SCREEN_WIDTH)
        area->x2 = (lv_coord_t)TIRTC_SCREEN_WIDTH - 1;
    if (area->x1 > area->x2) return;
    area->x1 = (lv_coord_t)((area->x1 / 32) * 32);
    area->x2 = (lv_coord_t)((area->x2 / 32 + 1) * 32 - 1);
}
#endif
