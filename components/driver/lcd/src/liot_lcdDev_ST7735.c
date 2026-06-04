/**
 * @file liot_lcdDev_ST7735.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "liot_lcdDev.h"

liot_hal_lcdDev_t liot_st7735_dev;

static int liot_st7735_init(liot_hal_lcd_handle_t handle)
{
    liot_hal_lcd_write_cmd(handle, 0x11, 0x00);

    uint8_t FRMCTR1[] = {   
                        0x05, 
                        0x3C,
                        0x3C
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB1, FRMCTR1, sizeof(FRMCTR1));

    uint8_t FRMCTR2[] = {   
                        0x05, 
                        0x3C,
                        0x3C
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB2, FRMCTR2, sizeof(FRMCTR2));

    uint8_t FRMCTR3[] = {   
                        0x05,
                        0x3C,
                        0x3C,
                        0x05,
                        0x3C,
                        0x3C
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB3, FRMCTR3, sizeof(FRMCTR3));

    uint8_t INVCTR[] = {    
                        0x03,
                        };

    liot_hal_lcd_transmit_cmd(handle, 0xB4, INVCTR, sizeof(INVCTR));

    uint8_t PWCTR1[] = {   
                        0x28,
                        0x08,
                        0x04
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xC0, PWCTR1, sizeof(PWCTR1));

    uint8_t PWCTR2[] = {   
                        0XC0
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xC1, PWCTR2, sizeof(PWCTR2));

    uint8_t PWCTR3[] = {   
                        0x0D,
                        0x00
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xC2, PWCTR3, sizeof(PWCTR3));

    uint8_t PWCTR4[] = {
                        0x8D,
                        0x2A
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xC3, PWCTR4, sizeof(PWCTR4));

    uint8_t PWCTR5[] = {
                        0x8D,
                        0xEE
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xC4, PWCTR5, sizeof(PWCTR5));

    uint8_t VMCTR1[] = {
                        0x1A,
                        };

    liot_hal_lcd_transmit_cmd(handle, 0xC5, VMCTR1, sizeof(VMCTR1));

    uint8_t MADCTL[] = {0x00};

    switch(liot_st7735_dev.info.direction)
    {
        case LIOT_LCD_DIR_0_ANGLE: MADCTL[0] = 0x08; break;
        case LIOT_LCD_DIR_90_ANGLE: MADCTL[0] = 0x78; break;
        case LIOT_LCD_DIR_180_ANGLE: MADCTL[0] = 0xA8; break;
        case LIOT_LCD_DIR_270_ANGLE: MADCTL[0] = 0xC8; break;
        default: MADCTL[0] = 0x08; break;
    }
    liot_hal_lcd_transmit_cmd(handle, 0x36, MADCTL, sizeof(MADCTL));

    uint8_t GMCTRP1[] = {
                        0x04,
                        0x22,
                        0x07,
                        0x0A,
                        0x2E,
                        0x30,
                        0x25,
                        0x2A,
                        0x28,
                        0x26,
                        0x2E,
                        0x3A,
                        0x00,
                        0x01,
                        0x03,
                        0x13,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xE0, GMCTRP1, sizeof(GMCTRP1));

    uint8_t GMCTRN1[] = {
                        0x04,
                        0x16,
                        0x06,
                        0x0D,
                        0x2D,
                        0x26,
                        0x23,
                        0x27,
                        0x27,
                        0x25,
                        0x2D,
                        0x3B,
                        0x00,
                        0x01,
                        0x04,
                        0x13,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xE1, GMCTRN1, sizeof(GMCTRN1));

    uint8_t COLMOD[] = {
                        0x05,
                        };

    liot_hal_lcd_transmit_cmd(handle, 0x3A, COLMOD, sizeof(COLMOD)); //65k mode
    liot_hal_lcd_write_cmd(handle, 0x29, 0x00);

    return 0;
}

static int liot_st7735_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    switch(liot_st7735_dev.info.direction)
    {
        case LIOT_LCD_DIR_0_ANGLE:
        case LIOT_LCD_DIR_180_ANGLE: sx = sx + 2; ex = ex + 2; sy = sy + 1; ey = ey + 1; break;
        case LIOT_LCD_DIR_90_ANGLE:
        case LIOT_LCD_DIR_270_ANGLE: sx = sx + 1; ex = ex + 1; sy = sy + 2; ey = ey + 2; break;
        default: sx = sx + 2; ex = ex + 2; sy = sy + 1; ey = ey + 1;break;
    }
    
    uint8_t set_x_cmd[] = {sx>>8,sx,ex>>8,ex};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));
    uint8_t set_y_cmd[] = {sy>>8,sy,ey>>8,ey};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);

    return 0;
}

static int liot_st7735_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;    //RGB565 = uint16
    liot_st7735_addrset(handle, sx, sy, ex, ey);         //设置填充区域横纵坐标
    ret = liot_hal_lcd_transmit_data(handle, buf, TotalBytes);
    return ret;
}

static int liot_st7735_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
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
    ret = liot_st7735_fill(handle, sx, sy, ex, ey, color_buf);
    liot_rtos_free(color_buf);
    return ret;
}

static int liot_st7735_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;
    if(on)  ret = liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x28, 0x00);
    return ret;
}

static int liot_st7735_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    int ret = 0;
    if(in)  ret = liot_hal_lcd_write_cmd(handle, 0x10, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    return ret;
}

liot_hal_lcdDev_t liot_st7735_dev = {
    .func = {
        .init = liot_st7735_init,
        .addrSet = liot_st7735_addrset,
        .fill = liot_st7735_fill,
        .full = liot_st7735_full,
        .strWrite = NULL,
        .display_on = liot_st7735_display_on,
        .sleep_in = liot_st7735_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x7735,
        .interface = LIOT_LCD_INTERFACE_LSPI 
                    | LIOT_LCD_INTERFACE_SPI 
                    | LIOT_LCD_INTERFACE_8080
                    | LIOT_LCD_INTERFACE_6800,
        .width = 128,
        .height = 160,
        .direction = LIOT_LCD_DIR_90_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};