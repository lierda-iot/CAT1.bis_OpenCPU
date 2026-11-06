/**
 * @File Name: liot_lcdDev_ST7789.c
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-11-13
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 */

#include "liot_lcdDev.h"

liot_hal_lcdDev_t liot_st7789_dev;

static int liot_st7789_init(liot_hal_lcd_handle_t handle)
{
    
    liot_hal_lcd_write_cmd(handle, 0x11, 0x00);

    switch(liot_st7789_dev.info.direction)
    {
        case LIOT_LCD_DIR_0_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x00); break;
        case LIOT_LCD_DIR_90_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x70); break;
        case LIOT_LCD_DIR_180_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xC0); break;
        case LIOT_LCD_DIR_270_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xA0); break;
        default: liot_hal_lcd_write_cmd(handle, 0x36, 0x00); break;
    }

    if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_LSPI && 
        ((liot_hal_lcd_config_t*)handle)->interface.lspi.lcd_2_data_lane == true)
    {
        liot_hal_lcd_write_cmd(handle, 0xE7, 0x10);
    }

    // liot_hal_lcd_write_cmd(handle, 0x20, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x21, 0x00);

    liot_hal_lcd_write_cmd(handle, 0x3A, 0x05);

    uint8_t set_rate_cmd[] = {0x0c,0x0c,0x00,0x33,0x33};
    liot_hal_lcd_transmit_cmd(handle, 0xB2, set_rate_cmd, sizeof(set_rate_cmd));

    liot_hal_lcd_write_cmd(handle, 0xB7, 0x35);
    liot_hal_lcd_write_cmd(handle, 0xBB, 0x20);
    liot_hal_lcd_write_cmd(handle, 0xC0, 0x2C);
    liot_hal_lcd_write_cmd(handle, 0xC2, 0x01);
    liot_hal_lcd_write_cmd(handle, 0xC3, 0x0B);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x20);
    liot_hal_lcd_write_cmd(handle, 0xC6, 0x0F);

    uint8_t reg_PWCTRL1[] = {0xa4,0xa1};
    liot_hal_lcd_transmit_cmd(handle, 0xD0, reg_PWCTRL1, sizeof(reg_PWCTRL1));

    uint8_t reg_PVGAMCTRL[] = {0xd0,0x03,0x09,0x0e,0x11,0x3d,0x47,0x55,
                                0x53,0x1a,0x16,0x14,0x1f,0x22};  //Positive voltage gamma
    liot_hal_lcd_transmit_cmd(handle, 0xE0, reg_PVGAMCTRL, sizeof(reg_PVGAMCTRL));

    uint8_t reg_NVGAMCTRL[] = {0xd0,0x02,0x08,0x0d,0x12,0x2c,0x43,0x55,
                                0x53,0x1e,0x1b,0x19,0x20,0x22};  //Negative voltage gamma
    liot_hal_lcd_transmit_cmd(handle, 0xE1, reg_NVGAMCTRL, sizeof(reg_NVGAMCTRL));

    liot_hal_lcd_write_cmd(handle, 0x29, 0x00);

    return 0;
}

static int liot_st7789_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint8_t set_x_cmd[] = {sx>>8,sx,ex>>8,ex};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));
    uint8_t set_y_cmd[] = {sy>>8,sy,ey>>8,ey};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);
    return 0;
}

static int liot_st7789_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;    //RGB565 = uint16
    liot_st7789_addrset(handle, sx, sy, ex, ey);         //设置填充区域横纵坐标
    ret = liot_hal_lcd_transmit_data(handle, buf, TotalBytes);
    return ret;
}

static int liot_st7789_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;
    uint32_t i = 0;
    uint8_t *color_buf = liot_rtos_malloc(TotalBytes);

    uint16_t *buf = (uint16_t*)color_buf;
    for(i = 0; i < TotalBytes/2; i++)
    {
        buf[i] = color;
    }
    ret = liot_st7789_fill(handle, sx, sy, ex, ey, color_buf);
    liot_rtos_free(color_buf);
    return ret;
}

static int liot_st7789_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;
    if(on)  ret = liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x28, 0x00);
    return ret;
}

static int liot_st7789_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    int ret = 0;
    if(in)  ret = liot_hal_lcd_write_cmd(handle, 0x10, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    return ret;
}

liot_hal_lcdDev_t liot_st7789_dev = {
    .func = {
        .init = liot_st7789_init,
        .addrSet = liot_st7789_addrset,
        .fill = liot_st7789_fill,
        .full = liot_st7789_full,
        .strWrite = NULL,
        .display_on = liot_st7789_display_on,
        .sleep_in = liot_st7789_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x7789,
        .interface = LIOT_LCD_INTERFACE_LSPI 
                    | LIOT_LCD_INTERFACE_SPI 
                    | LIOT_LCD_INTERFACE_8080,
        .width = 240,
        .height = 320,
        .direction = LIOT_LCD_DIR_180_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};
