#include "app_boot_logo_liot.h"

#include <stdint.h>

#include "app_osal.h"
#include "assets/watch_boot_logo.h"

LIOT_ADD_DISPLAY(liot_st7789_dev);

#define APP_BOOT_LOGO_HOLD_MS 800U

static void app_boot_logo_get_screen_size(uint16_t *width, uint16_t *height)
{
    if (width == NULL || height == NULL) {
        return;
    }
    if (liot_st7789_dev.info.direction == LIOT_LCD_DIR_0_ANGLE ||
        liot_st7789_dev.info.direction == LIOT_LCD_DIR_180_ANGLE) {
        *width = (uint16_t)liot_st7789_dev.info.width;
        *height = (uint16_t)liot_st7789_dev.info.height;
    } else {
        *width = (uint16_t)liot_st7789_dev.info.height;
        *height = (uint16_t)liot_st7789_dev.info.width;
    }
}

int app_boot_logo_show(liot_lcd_handle_t lcd)
{
    uint16_t screen_width;
    uint16_t screen_height;
    uint16_t x;
    uint16_t y;
    liot_lcd_errcode_e lcd_ret;

    if (lcd == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    app_boot_logo_get_screen_size(&screen_width, &screen_height);
    (void)liot_lcd_clear_screen(lcd, WHITE);

    x = (uint16_t)((screen_width - WATCH_BOOT_LOGO_WIDTH) / 2U);
    y = (uint16_t)((screen_height - WATCH_BOOT_LOGO_HEIGHT) / 2U);
    lcd_ret = liot_lcd_write(lcd,
                             x,
                             y,
                             (uint16_t)(x + WATCH_BOOT_LOGO_WIDTH - 1U),
                             (uint16_t)(y + WATCH_BOOT_LOGO_HEIGHT - 1U),
                             (uint8_t *)g_watch_boot_logo_rgb565);
    if (lcd_ret != LIOT_LCD_OK) {
        app_log("boot logo write failed: %d", (int)lcd_ret);
        return APP_ERR_FAIL;
    }

    app_log("boot logo shown: %ux%u at %u,%u",
            (unsigned int)WATCH_BOOT_LOGO_WIDTH,
            (unsigned int)WATCH_BOOT_LOGO_HEIGHT,
            (unsigned int)x,
            (unsigned int)y);
    app_os_task_delay_ms(APP_BOOT_LOGO_HOLD_MS);
    return APP_OK;
}
