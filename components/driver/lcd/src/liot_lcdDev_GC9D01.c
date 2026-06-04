/**
 * @File Name: liot_lcdDev_GC9D01.c
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2025-8-5
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 */

#include "liot_lcdDev.h"

liot_hal_lcdDev_t liot_gc9d01_dev;

static int liot_gc9d01_init(liot_hal_lcd_handle_t handle)
{
    liot_rtos_task_sleep_ms(200);

    liot_hal_lcd_transmit_cmd(handle, 0xFE, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0xEF, NULL, 0);
    liot_hal_lcd_write_cmd(handle, 0x80, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x81, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x82, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x83, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x84, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x85, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x86, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x87, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x88, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x89, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8A, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8B, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8C, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8D, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8E, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8F, 0xFF);

    liot_hal_lcd_write_cmd(handle, 0x3A, 0x05);
    liot_hal_lcd_write_cmd(handle, 0xEC, 0x01);

    uint8_t REG1[] = {
                        0x02, 
                        0x0E,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x74, REG1, sizeof(REG1));

    liot_hal_lcd_write_cmd(handle, 0x98, 0x3E);
    liot_hal_lcd_write_cmd(handle, 0x99, 0x3E);

    uint8_t REG2[] = {
                        0x0D, 
                        0x0D,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB5, REG2, sizeof(REG2));

    uint8_t REG3[] = {
                        0x38, 
                        0x0F, 
                        0x79, 
                        0x67, 
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x60, REG3, sizeof(REG3));

    uint8_t REG4[] = {  
                        0x38,
                        0x11,
                        0x79,
                        0x67,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x61, REG4, sizeof(REG4));


    uint8_t REG5[] = {
                        0x38,
                        0x17,
                        0x71,
                        0x5F,
                        0x79,
                        0x67,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x64, REG5, sizeof(REG5));

    uint8_t REG6[] = {
                        0x38, 
                        0x13, 
                        0x71, 
                        0x5B, 
                        0x79, 
                        0x67,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x65, REG6, sizeof(REG6));

    uint8_t REG7[] = {
                        0x00,
                        0x00,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x6A, REG7, sizeof(REG7));

    uint8_t REG8[] = {
                        0x22,
                        0x02,
                        0x22,
                        0x02,
                        0x22,
                        0x22,
                        0x50,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x6C, REG8, sizeof(REG8));


    uint8_t REG9[] = {
                        0x03,
                        0x03,
                        0x01,
                        0x01,
                        0x00,
                        0x00,
                        0x0F,
                        0x0F,
                        0x0D,
                        0x0D,
                        0x0B,
                        0x0B,
                        0x09,
                        0x09,
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                        0x0A,
                        0x0A,
                        0x0C,
                        0x0C,
                        0x0E,
                        0x0E,
                        0x10,
                        0x10,
                        0x00,
                        0x00,
                        0x02,
                        0x02,
                        0x04,
                        0x04,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x6E, REG9, sizeof(REG9));
    
    liot_hal_lcd_write_cmd(handle, 0xBF, 0x01);

    liot_hal_lcd_write_cmd(handle, 0xF9, 0x40);

    liot_hal_lcd_write_cmd(handle, 0x9B, 0x3B);

    uint8_t REG10[] = {
                        0x33,
                        0x7F,
                        0x00,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x93, REG10, sizeof(REG10));

    liot_hal_lcd_write_cmd(handle, 0x7E, 0x30);

    uint8_t REG11[] = {
                        0x0D,
                        0x02,
                        0x08,
                        0x0D,
                        0x02,
                        0x08,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x70, REG11, sizeof(REG11));

    uint8_t REG12[] = {
                        0x0D,
                        0x02,
                        0x08,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x71, REG12, sizeof(REG12));

    uint8_t REG13[] = {
                        0x0E,
                        0x09,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x91, REG13, sizeof(REG13));

    liot_hal_lcd_write_cmd(handle, 0xC3, 0x18);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x18);
    liot_hal_lcd_write_cmd(handle, 0xC9, 0x3C);

    uint8_t REG14[] = {
                        0x13,
                        0x15,
                        0x04,
                        0x05,
                        0x01,
                        0x38,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF0, REG14, sizeof(REG14));

    uint8_t REG15[] = {
                        0x13,
                        0x15,
                        0x04,
                        0x05,
                        0x01,
                        0x34,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF2, REG15, sizeof(REG15));

    uint8_t REG16[] = {
                        0x4B,
                        0xB8,
                        0x7B,
                        0x34,
                        0x35,
                        0xEF,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF1, REG16, sizeof(REG16));

    uint8_t REG17[] = {
                        0x47,
                        0xB4,
                        0x72,
                        0x34,
                        0x35,
                        0xDA,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF3, REG17, sizeof(REG17));

    switch(liot_gc9d01_dev.info.direction)
    {
        case LIOT_LCD_DIR_0_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x00); break;
        case LIOT_LCD_DIR_90_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x60); break;
        case LIOT_LCD_DIR_180_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xA0); break;
        case LIOT_LCD_DIR_270_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xC0); break;
        default: liot_hal_lcd_write_cmd(handle, 0x36, 0x08); break;
    }

    if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_LSPI && 
        ((liot_hal_lcd_config_t*)handle)->interface.lspi.lcd_2_data_lane == true)
    {
        liot_hal_lcd_write_cmd(handle, 0xE9, 0x08);
    }

    uint8_t REG18[] = {
                        0x00, 
                        0x00,
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB4, REG18, sizeof(REG18));

    liot_hal_lcd_transmit_cmd(handle, 0x34, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0x11, NULL, 0);

    liot_rtos_task_sleep_ms(120);

    liot_hal_lcd_transmit_cmd(handle, 0x29, NULL, 0);
    liot_rtos_task_sleep_ms(20);
    return 0;
}

static int liot_gc9d01_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint8_t set_x_cmd[] = {sx>>8,sx,ex>>8,ex};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));
    uint8_t set_y_cmd[] = {sy>>8,sy,ey>>8,ey};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);

    return 0;
}

static int liot_gc9d01_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;    //RGB565 = uint16
    liot_gc9d01_addrset(handle, sx, sy, ex, ey);         //设置填充区域横纵坐标
    ret = liot_hal_lcd_transmit_data(handle, buf, TotalBytes);
    return ret;
}

static int liot_gc9d01_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
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
    ret = liot_gc9d01_fill(handle, sx, sy, ex, ey, color_buf);
    liot_rtos_free(color_buf);
    return ret;
}

static int liot_gc9d01_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;
    if(on)  ret = liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x28, 0x00);
    return ret;
}

static int liot_gc9d01_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    int ret = 0;
    if(in)  ret = liot_hal_lcd_write_cmd(handle, 0x10, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    return ret;
}

liot_hal_lcdDev_t liot_gc9d01_dev = {
    .func = {
        .init = liot_gc9d01_init,
        .addrSet = liot_gc9d01_addrset,
        .fill = liot_gc9d01_fill,
        .full = liot_gc9d01_full,
        .strWrite = NULL,
        .display_on = liot_gc9d01_display_on,
        .sleep_in = liot_gc9d01_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x9D01,
        .interface = LIOT_LCD_INTERFACE_LSPI 
                    | LIOT_LCD_INTERFACE_SPI
                    | LIOT_LCD_INTERFACE_8080,
        .width = 160,
        .height = 160,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};
