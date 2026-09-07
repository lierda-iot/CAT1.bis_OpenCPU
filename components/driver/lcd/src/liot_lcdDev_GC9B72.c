/**
 * @file liot_lcdDev_GC9B72.c
 * @brief GC9B72-like 360x360 RGB565 QSPI LCD driver, adapted from my_mod.
 */

#include <stdint.h>
#include <stdbool.h>
#include "liot_os.h"
#include "liot_lcdDev.h"
#include "liot_log.h"
liot_hal_lcdDev_t liot_gc9b72_dev;

static int liot_gc9b72_init(liot_hal_lcd_handle_t handle)
{
    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    liot_rtos_task_sleep_ms(120);
    liot_hal_lcd_transmit_cmd(handle, 0xFE, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0xEF, NULL, 0);

    liot_hal_lcd_write_cmd(handle, 0x80, 0x19);
    liot_hal_lcd_write_cmd(handle, 0x82, 0x09);
    liot_hal_lcd_write_cmd(handle, 0x83, 0x03);
    liot_hal_lcd_write_cmd(handle, 0x88, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x89, 0x38);
    liot_hal_lcd_write_cmd(handle, 0x8A, 0x40);
    liot_hal_lcd_write_cmd(handle, 0x8B, 0x0A);
    liot_hal_lcd_write_cmd(handle, 0x8C, 0x00);

    liot_hal_lcd_write_cmd(handle, 0x81, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x84, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x85, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x86, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x87, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8E, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8F, 0xFF);

    liot_hal_lcd_write_cmd(handle, 0x98, 0x3E);
    liot_hal_lcd_write_cmd(handle, 0x99, 0x3E);
    liot_hal_lcd_write_cmd(handle, 0x7D, 0x72);

    uint8_t reg1[] = {0x02, 0x03, 0x03, 0x06, 0x03, 0x03, 0x09, 0x07, 0x09, 0x03};
    liot_hal_lcd_transmit_cmd(handle, 0x70, reg1, sizeof(reg1));

    uint8_t reg2[] = {0x06, 0x06, 0x01, 0x01};
    liot_hal_lcd_transmit_cmd(handle, 0x90, reg2, sizeof(reg2));

    uint8_t reg3[] = {0x02, 0xFF, 0x00};
    liot_hal_lcd_transmit_cmd(handle, 0x93, reg3, sizeof(reg3));

    liot_hal_lcd_write_cmd(handle, 0xCB, 0x02);

    uint8_t reg4[] = {0x00, 0x00};
    liot_hal_lcd_transmit_cmd(handle, 0xFB, reg4, sizeof(reg4));

    liot_hal_lcd_write_cmd(handle, 0xF6, 0xC0);

    uint8_t reg5[] = {0x00, 0x00, 0x22, 0x00, 0xCC, 0x04, 0x58};
    liot_hal_lcd_transmit_cmd(handle, 0x6C, reg5, sizeof(reg5));

    uint8_t reg6[] = {0x0B, 0x00};
    liot_hal_lcd_transmit_cmd(handle, 0xAA, reg6, sizeof(reg6));

    liot_hal_lcd_write_cmd(handle, 0xEC, 0x07);
    liot_hal_lcd_write_cmd(handle, 0xF9, 0x40);

    uint8_t reg7[] = {0x01, 0x67};
    liot_hal_lcd_transmit_cmd(handle, 0xEB, reg7, sizeof(reg7));

    uint8_t reg8[] = {0x01, 0x60, 0x00, 0x00, 0x00, 0x00};
    liot_hal_lcd_transmit_cmd(handle, 0x74, reg8, sizeof(reg8));

    uint8_t reg_b5[] = {0x14, 0x14, 0x14};
    liot_hal_lcd_transmit_cmd(handle, 0xB5, reg_b5, sizeof(reg_b5));

    uint8_t reg9[] = {
        0x0B, 0x0B, 0x09, 0x09, 0x13, 0x13, 0x11, 0x11,
        0x16, 0x15, 0x01, 0x04, 0x00, 0x0D, 0x1D, 0x00, 0x00,
        0x1D, 0x0D, 0x00, 0x04, 0x08, 0x15, 0x16, 0x12,
        0x12, 0x14, 0x14, 0x0A, 0x0A, 0x0C, 0x0C
    };
    liot_hal_lcd_transmit_cmd(handle, 0x6E, reg9, sizeof(reg9));

    uint8_t reg10[] = {0x38, 0x1C, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x60, reg10, sizeof(reg10));

    uint8_t reg11[] = {0xF8, 0x0A, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x61, reg11, sizeof(reg11));

    uint8_t reg12[] = {0xF8, 0x0B, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x62, reg12, sizeof(reg12));

    uint8_t reg13[] = {0x38, 0x1C, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x63, reg13, sizeof(reg13));

    uint8_t reg14[] = {0x38, 0x20, 0x72, 0xF8, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x64, reg14, sizeof(reg14));

    uint8_t reg15[] = {0x78, 0x1A, 0x70, 0x0B, 0x56, 0x13};
    liot_hal_lcd_transmit_cmd(handle, 0x65, reg15, sizeof(reg15));

    uint8_t reg16[] = {0x38, 0x24, 0x72, 0xFC, 0x13, 0x56};
    liot_hal_lcd_transmit_cmd(handle, 0x66, reg16, sizeof(reg16));

    uint8_t reg17[] = {0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A};
    liot_hal_lcd_transmit_cmd(handle, 0x68, reg17, sizeof(reg17));

    uint8_t reg18[] = {0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A};
    liot_hal_lcd_transmit_cmd(handle, 0x69, reg18, sizeof(reg18));

    uint8_t reg19[] = {0x00, 0x00};
    liot_hal_lcd_transmit_cmd(handle, 0x6A, reg19, sizeof(reg19));

    liot_hal_lcd_write_cmd(handle, 0x3A, 0x05);
    liot_hal_lcd_write_cmd(handle, 0x36, 0x08);

    uint8_t reg7c[] = {0xB6, 0x29};
    liot_hal_lcd_transmit_cmd(handle, 0x7C, reg7c, sizeof(reg7c));

    liot_hal_lcd_write_cmd(handle, 0xAC, 0x40);
    liot_hal_lcd_write_cmd(handle, 0xC3, 0x1A);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x24);
    liot_hal_lcd_write_cmd(handle, 0xC9, 0x2F);

    uint8_t reg20[] = {0x11, 0x17, 0x08, 0x06, 0x05, 0x38};
    liot_hal_lcd_transmit_cmd(handle, 0xF0, reg20, sizeof(reg20));

    uint8_t reg21[] = {0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F};
    liot_hal_lcd_transmit_cmd(handle, 0xF1, reg21, sizeof(reg21));

    uint8_t reg22[] = {0x11, 0x17, 0x08, 0x06, 0x05, 0x38};
    liot_hal_lcd_transmit_cmd(handle, 0xF2, reg22, sizeof(reg22));

    uint8_t reg23[] = {0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F};
    liot_hal_lcd_transmit_cmd(handle, 0xF3, reg23, sizeof(reg23));

    liot_hal_lcd_write_cmd(handle, 0xB4, 0x0A);

    liot_hal_lcd_transmit_cmd(handle, 0xFE, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0xEE, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0x11, NULL, 0);
    liot_rtos_task_sleep_ms(20);
    liot_hal_lcd_transmit_cmd(handle, 0x29, NULL, 0);
    liot_rtos_task_sleep_ms(20);

    return 0;
}

static int liot_gc9b72_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy,
                               uint16_t ex, uint16_t ey)
{
    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    uint8_t set_x_cmd[] = {sx >> 8, sx & 0xFF, ex >> 8, ex & 0xFF};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));

    uint8_t set_y_cmd[] = {sy >> 8, sy & 0xFF, ey >> 8, ey & 0xFF};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));

    liot_hal_lcd_set_mspi(handle, 1, 0, LIOT_QSPI_DATA_LINE_4, 0x32);
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);

    return 0;
}

static int liot_gc9b72_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy,
                            uint16_t ex, uint16_t ey, void *buf)
{
    // static uint8_t first = 0;
    uint32_t total_bytes = (uint32_t)(ey - sy + 1) * (uint32_t)(ex - sx + 1) * 2U;

    // if (first <= 30) 
    {
        liot_gc9b72_addrset(handle, sx, sy, ex, ey);
        // first ++;
    }
    
    return liot_hal_lcd_transmit_data(handle, buf, total_bytes);
}

static int liot_gc9b72_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy,
                            uint16_t ex, uint16_t ey, uint16_t color)
{
    const uint16_t chunk_rows = 16;
    uint32_t width = (uint32_t)(ex - sx + 1);
    uint32_t y = sy;
    uint16_t *buf = (uint16_t *)liot_rtos_malloc(width * chunk_rows * sizeof(uint16_t));

    if (buf == NULL) {
        return -1;
    }

    while (y <= ey) {
        uint32_t rows = (ey - y + 1 > chunk_rows) ? chunk_rows : (ey - y + 1);
        uint32_t pixels = width * rows;

        for (uint32_t i = 0; i < pixels; i++) {
            buf[i] = color;
        }

        int ret = liot_gc9b72_fill(handle, sx, (uint16_t)y, ex, (uint16_t)(y + rows - 1), buf);
        if (ret != 0) {
            liot_rtos_free(buf);
            return ret;
        }

        y += rows;
    }

    liot_rtos_free(buf);
    return 0;
}

static int liot_gc9b72_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);
    return liot_hal_lcd_write_cmd(handle, on ? 0x29 : 0x28, 0x00);
}

static int liot_gc9b72_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);
    return liot_hal_lcd_write_cmd(handle, in ? 0x10 : 0x11, 0x00);
}

liot_hal_lcdDev_t liot_gc9b72_dev = {
    .func = {
        .init = liot_gc9b72_init,
        .addrSet = liot_gc9b72_addrset,
        .fill = liot_gc9b72_fill,
        .full = liot_gc9b72_full,
        .strWrite = NULL,
        .display_on = liot_gc9b72_display_on,
        .sleep_in = liot_gc9b72_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x9B72,
        .interface = LIOT_LCD_INTERFACE_QSPI,
        .width = 360,
        .height = 360,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};
