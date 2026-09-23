/**
 * @file liot_lcdDev_ST77916.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "liot_lcdDev.h"
#include "lierda_app_main.h"
#include "cmsis_os2.h"

liot_hal_lcdDev_t liot_st77916_dev;

/**
 * @brief ST77916 初始化函数 (基于 BOE1.8 寸屏初始化序列)
 *
 * @param handle LCD 句柄
 * @return int 0 为成功
 */
static int liot_st77916_init(liot_hal_lcd_handle_t handle)
{
    liot_trace("%s@%d: ", __func__, __LINE__);

    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    /* Page Enable */
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x28);
    liot_hal_lcd_write_cmd(handle, 0xF2, 0x28);

    /* SPI Control */
    liot_hal_lcd_write_cmd(handle, 0x73, 0xF0);
    liot_hal_lcd_write_cmd(handle, 0x7C, 0xD1);
    liot_hal_lcd_write_cmd(handle, 0x83, 0xE0);
    liot_hal_lcd_write_cmd(handle, 0x84, 0x61);
    liot_hal_lcd_write_cmd(handle, 0xF2, 0x82);

    /* Page 0 */
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x00);

    /* Page 1 */
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xF1, 0x01);

    /* AVDD/AVCL/AVGH/AVGL */
    liot_hal_lcd_write_cmd(handle, 0xB0, 0x56);
    liot_hal_lcd_write_cmd(handle, 0xB1, 0x4D);
    liot_hal_lcd_write_cmd(handle, 0xB2, 0x24);
    liot_hal_lcd_write_cmd(handle, 0xB4, 0x87);
    liot_hal_lcd_write_cmd(handle, 0xB5, 0x44);
    liot_hal_lcd_write_cmd(handle, 0xB6, 0x8B);
    liot_hal_lcd_write_cmd(handle, 0xB7, 0x40);
    liot_hal_lcd_write_cmd(handle, 0xB8, 0x86);
    liot_hal_lcd_write_cmd(handle, 0xBA, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xBB, 0x08);
    liot_hal_lcd_write_cmd(handle, 0xBC, 0x08);
    liot_hal_lcd_write_cmd(handle, 0xBD, 0x00);

    /* VGAM/VGAM Buffer */
    liot_hal_lcd_write_cmd(handle, 0xC0, 0x80);
    liot_hal_lcd_write_cmd(handle, 0xC1, 0x10);
    liot_hal_lcd_write_cmd(handle, 0xC2, 0x37);
    liot_hal_lcd_write_cmd(handle, 0xC3, 0x80);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x10);
    liot_hal_lcd_write_cmd(handle, 0xC5, 0x37);
    liot_hal_lcd_write_cmd(handle, 0xC6, 0xA9);
    liot_hal_lcd_write_cmd(handle, 0xC7, 0x41);
    liot_hal_lcd_write_cmd(handle, 0xC8, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xC9, 0xA9);
    liot_hal_lcd_write_cmd(handle, 0xCA, 0x41);
    liot_hal_lcd_write_cmd(handle, 0xCB, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xD0, 0x91);
    liot_hal_lcd_write_cmd(handle, 0xD1, 0x68);
    liot_hal_lcd_write_cmd(handle, 0xD2, 0x68);

    /* DGVDD/EGVDD */
    uint8_t f5_data[] = {0x00, 0xA5};
    liot_hal_lcd_transmit_cmd(handle, 0xF5, f5_data, sizeof(f5_data));

    liot_hal_lcd_write_cmd(handle, 0xDD, 0x4F);
    liot_hal_lcd_write_cmd(handle, 0xDE, 0x4F);

    /* Page 1 End */
    liot_hal_lcd_write_cmd(handle, 0xF1, 0x10);
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x00);

    /* Page 2 */
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x02);

    /* P.Gamma Positive */
    uint8_t gamma_p[] = {0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34};
    liot_hal_lcd_transmit_cmd(handle, 0xE0, gamma_p, sizeof(gamma_p));

    /* P.Gamma Negative */
    uint8_t gamma_n[] = {0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33};
    liot_hal_lcd_transmit_cmd(handle, 0xE1, gamma_n, sizeof(gamma_n));

    /* Page 2 End */
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x10);

    /* Page 3 */
    liot_hal_lcd_write_cmd(handle, 0xF3, 0x10);

    liot_hal_lcd_write_cmd(handle, 0xE0, 0x07);
    liot_hal_lcd_write_cmd(handle, 0xE1, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xE2, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xE3, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xE4, 0xE0);
    liot_hal_lcd_write_cmd(handle, 0xE5, 0x06);
    liot_hal_lcd_write_cmd(handle, 0xE6, 0x21);
    liot_hal_lcd_write_cmd(handle, 0xE7, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xE8, 0x05);
    liot_hal_lcd_write_cmd(handle, 0xE9, 0x02);
    liot_hal_lcd_write_cmd(handle, 0xEA, 0xDA);
    liot_hal_lcd_write_cmd(handle, 0xEB, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xEC, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xED, 0x0F);
    liot_hal_lcd_write_cmd(handle, 0xEE, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xEF, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xF8, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xF9, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFA, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFB, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFC, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFD, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFE, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xFF, 0x00);

    /* Page 4 - GOA Bank1 */
    liot_hal_lcd_write_cmd(handle, 0x60, 0x40);
    liot_hal_lcd_write_cmd(handle, 0x61, 0x04);
    liot_hal_lcd_write_cmd(handle, 0x62, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x63, 0x42);
    liot_hal_lcd_write_cmd(handle, 0x64, 0xD9);
    liot_hal_lcd_write_cmd(handle, 0x65, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x66, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x67, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x68, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x69, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x6A, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x6B, 0x00);

    /* GOA Bank2 */
    liot_hal_lcd_write_cmd(handle, 0x70, 0x40);
    liot_hal_lcd_write_cmd(handle, 0x71, 0x03);
    liot_hal_lcd_write_cmd(handle, 0x72, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x73, 0x42);
    liot_hal_lcd_write_cmd(handle, 0x74, 0xD8);
    liot_hal_lcd_write_cmd(handle, 0x75, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x76, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x77, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x78, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x79, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x7A, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x7B, 0x00);

    /* GOA Bank3 */
    liot_hal_lcd_write_cmd(handle, 0x80, 0x48);
    liot_hal_lcd_write_cmd(handle, 0x81, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x82, 0x06);
    liot_hal_lcd_write_cmd(handle, 0x83, 0x02);
    liot_hal_lcd_write_cmd(handle, 0x84, 0xD6);
    liot_hal_lcd_write_cmd(handle, 0x85, 0x04);
    liot_hal_lcd_write_cmd(handle, 0x86, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x87, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x88, 0x48);
    liot_hal_lcd_write_cmd(handle, 0x89, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x8A, 0x08);
    liot_hal_lcd_write_cmd(handle, 0x8B, 0x02);
    liot_hal_lcd_write_cmd(handle, 0x8C, 0xD8);
    liot_hal_lcd_write_cmd(handle, 0x8D, 0x04);
    liot_hal_lcd_write_cmd(handle, 0x8E, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x8F, 0x00);

    /* GOA Bank4 */
    liot_hal_lcd_write_cmd(handle, 0x90, 0x48);
    liot_hal_lcd_write_cmd(handle, 0x91, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x92, 0x0A);
    liot_hal_lcd_write_cmd(handle, 0x93, 0x02);
    liot_hal_lcd_write_cmd(handle, 0x94, 0xDA);
    liot_hal_lcd_write_cmd(handle, 0x95, 0x04);
    liot_hal_lcd_write_cmd(handle, 0x96, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x97, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x98, 0x48);
    liot_hal_lcd_write_cmd(handle, 0x99, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x9A, 0x0C);
    liot_hal_lcd_write_cmd(handle, 0x9B, 0x02);
    liot_hal_lcd_write_cmd(handle, 0x9C, 0xDC);
    liot_hal_lcd_write_cmd(handle, 0x9D, 0x04);
    liot_hal_lcd_write_cmd(handle, 0x9E, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x9F, 0x00);

    /* GOA Bank5 */
    liot_hal_lcd_write_cmd(handle, 0xA0, 0x48);
    liot_hal_lcd_write_cmd(handle, 0xA1, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xA2, 0x05);
    liot_hal_lcd_write_cmd(handle, 0xA3, 0x02);
    liot_hal_lcd_write_cmd(handle, 0xA4, 0xD5);
    liot_hal_lcd_write_cmd(handle, 0xA5, 0x04);
    liot_hal_lcd_write_cmd(handle, 0xA6, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xA7, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xA8, 0x48);
    liot_hal_lcd_write_cmd(handle, 0xA9, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xAA, 0x07);
    liot_hal_lcd_write_cmd(handle, 0xAB, 0x02);
    liot_hal_lcd_write_cmd(handle, 0xAC, 0xD7);
    liot_hal_lcd_write_cmd(handle, 0xAD, 0x04);
    liot_hal_lcd_write_cmd(handle, 0xAE, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xAF, 0x00);

    /* GOA Bank6 */
    liot_hal_lcd_write_cmd(handle, 0xB0, 0x48);
    liot_hal_lcd_write_cmd(handle, 0xB1, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xB2, 0x09);
    liot_hal_lcd_write_cmd(handle, 0xB3, 0x02);
    liot_hal_lcd_write_cmd(handle, 0xB4, 0xD9);
    liot_hal_lcd_write_cmd(handle, 0xB5, 0x04);
    liot_hal_lcd_write_cmd(handle, 0xB6, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xB7, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xB8, 0x48);
    liot_hal_lcd_write_cmd(handle, 0xB9, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xBA, 0x0B);
    liot_hal_lcd_write_cmd(handle, 0xBB, 0x02);
    liot_hal_lcd_write_cmd(handle, 0xBC, 0xDB);
    liot_hal_lcd_write_cmd(handle, 0xBD, 0x04);
    liot_hal_lcd_write_cmd(handle, 0xBE, 0x00);
    liot_hal_lcd_write_cmd(handle, 0xBF, 0x00);

    /* VCOM Control */
    liot_hal_lcd_write_cmd(handle, 0xC0, 0x10);
    liot_hal_lcd_write_cmd(handle, 0xC1, 0x47);
    liot_hal_lcd_write_cmd(handle, 0xC2, 0x56);
    liot_hal_lcd_write_cmd(handle, 0xC3, 0x65);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x74);
    liot_hal_lcd_write_cmd(handle, 0xC5, 0x88);
    liot_hal_lcd_write_cmd(handle, 0xC6, 0x99);
    liot_hal_lcd_write_cmd(handle, 0xC7, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xC8, 0xBB);
    liot_hal_lcd_write_cmd(handle, 0xC9, 0xAA);

    liot_hal_lcd_write_cmd(handle, 0xD0, 0x10);
    liot_hal_lcd_write_cmd(handle, 0xD1, 0x47);
    liot_hal_lcd_write_cmd(handle, 0xD2, 0x56);
    liot_hal_lcd_write_cmd(handle, 0xD3, 0x65);
    liot_hal_lcd_write_cmd(handle, 0xD4, 0x74);
    liot_hal_lcd_write_cmd(handle, 0xD5, 0x88);
    liot_hal_lcd_write_cmd(handle, 0xD6, 0x99);
    liot_hal_lcd_write_cmd(handle, 0xD7, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xD8, 0xBB);
    liot_hal_lcd_write_cmd(handle, 0xD9, 0xAA);

    /* Page End */
    liot_hal_lcd_write_cmd(handle, 0xF3, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xF0, 0x00);

    /* Column Address Set */
    uint8_t col_addr[] = {0x00, 0x00, 0x01, 0x67};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, col_addr, sizeof(col_addr));

    /* Row Address Set */
    uint8_t row_addr[] = {0x00, 0x00, 0x01, 0x67};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, row_addr, sizeof(row_addr));

    /* Pixel Format */
    liot_hal_lcd_write_cmd(handle, 0x3A, 0x05);

    /* Inversion On */
    liot_hal_lcd_transmit_cmd(handle, 0x21, NULL, 0);

    /* Sleep Out */
    liot_hal_lcd_transmit_cmd(handle, 0x11, NULL, 0);

    /* Delay 120ms */
    osDelay(120);

    /* Display On */
    liot_hal_lcd_transmit_cmd(handle, 0x29, NULL, 0);

    return 0;
}

static int liot_st77916_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    // switch(liot_st77916_dev.info.direction)
    // {
    //     case LIOT_LCD_DIR_0_ANGLE:
    //     case LIOT_LCD_DIR_180_ANGLE: sx = sx + 2; ex = ex + 2; sy = sy + 1; ey = ey + 1; break;
    //     case LIOT_LCD_DIR_90_ANGLE:
    //     case LIOT_LCD_DIR_270_ANGLE: sx = sx + 1; ex = ex + 1; sy = sy + 2; ey = ey + 2; break;
    //     default: sx = sx + 2; ex = ex + 2; sy = sy + 1; ey = ey + 1;break;
    // }

    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    uint8_t set_x_cmd[] = {sx>>8,sx,ex>>8,ex};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));
    uint8_t set_y_cmd[] = {sy>>8,sy,ey>>8,ey};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));

    liot_hal_lcd_set_mspi(handle, 1, 0, 2, 0x32);
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);

    return 0;
}

static int liot_st77916_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;    //RGB565 = uint16
    liot_st77916_addrset(handle, sx, sy, ex, ey);         //设置填充区域横纵坐标
    ret = liot_hal_lcd_transmit_data(handle, buf, TotalBytes);
    return ret;
}

static int liot_st77916_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;
    uint32_t i = 0;
#if defined(PSRAM_FEATURE_ENABLE) && (PSRAM_EXIST==1)
    uint8_t *color_buf = liot_rtos_psram_malloc(TotalBytes);
#else
    uint8_t *color_buf = liot_rtos_malloc(TotalBytes);
#endif
    uint16_t *buf = (uint16_t*)color_buf;
    for(i = 0; i < TotalBytes/2; i++)
    {
        buf[i] = color;
    }
    ret = liot_st77916_fill(handle, sx, sy, ex, ey, color_buf);
#if defined(PSRAM_FEATURE_ENABLE) && (PSRAM_EXIST==1)
    liot_rtos_psram_free(color_buf);
#else
    liot_rtos_free(color_buf);
#endif
    return ret;
}

static int liot_st77916_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;

    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    if(on)  ret = liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x28, 0x00);
    return ret;
}

static int liot_st77916_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    int ret = 0;

    liot_hal_lcd_set_mspi(handle, 1, 0, 0, 0x02);

    if(in)  ret = liot_hal_lcd_write_cmd(handle, 0x10, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    return ret;
}

liot_hal_lcdDev_t liot_st77916_dev = {
    .func = {
        .init = liot_st77916_init,
        .addrSet = liot_st77916_addrset,
        .fill = liot_st77916_fill,
        .full = liot_st77916_full,
        .strWrite = NULL,
        .display_on = liot_st77916_display_on,
        .sleep_in = liot_st77916_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x00,
        .interface = LIOT_LCD_INTERFACE_QSPI,
        .width = 360,
        .height = 360,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};