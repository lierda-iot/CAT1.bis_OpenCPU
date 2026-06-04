/**
 * @File Name: liot_lcdDev_GC9A01.c
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

liot_hal_lcdDev_t liot_gc9a01_dev;

static int liot_gc9a01_init(liot_hal_lcd_handle_t handle)
{
    // liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    liot_rtos_task_sleep_ms(200);
    // liot_hal_lcd_write_cmd(handle, 0xEF, 0x00);
    liot_hal_lcd_transmit_cmd(handle, 0xEF, NULL, 0);
    liot_hal_lcd_write_cmd(handle, 0xEB, 0x14);

    // liot_hal_lcd_write_cmd(handle, 0xFE, 0x00);
    // liot_hal_lcd_write_cmd(handle, 0xEF, 0x00);
    liot_hal_lcd_transmit_cmd(handle, 0xFE, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0xEF, NULL, 0);
    liot_hal_lcd_write_cmd(handle, 0xEB, 0x14);

    liot_hal_lcd_write_cmd(handle, 0x84, 0x40);
    liot_hal_lcd_write_cmd(handle, 0x85, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x86, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x87, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x88, 0x0A);
    liot_hal_lcd_write_cmd(handle, 0x89, 0x21);
    liot_hal_lcd_write_cmd(handle, 0x8A, 0x00);
    liot_hal_lcd_write_cmd(handle, 0x8B, 0x80);
    liot_hal_lcd_write_cmd(handle, 0x8C, 0x01);
    liot_hal_lcd_write_cmd(handle, 0x8D, 0x01);
    liot_hal_lcd_write_cmd(handle, 0x8E, 0xFF);
    liot_hal_lcd_write_cmd(handle, 0x8F, 0xFF);

    uint8_t REG1[] = {
                        0x00, 
                        0x20
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xB6, REG1, sizeof(REG1));

    switch(liot_gc9a01_dev.info.direction)
    {
        case LIOT_LCD_DIR_0_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x08); break;
        case LIOT_LCD_DIR_90_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0x68); break;
        case LIOT_LCD_DIR_180_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xA8); break;
        case LIOT_LCD_DIR_270_ANGLE: liot_hal_lcd_write_cmd(handle, 0x36, 0xC8); break;
        default: liot_hal_lcd_write_cmd(handle, 0x36, 0x08); break;
    }

    if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_LSPI && 
        ((liot_hal_lcd_config_t*)handle)->interface.lspi.lcd_2_data_lane == true)
    {
        liot_hal_lcd_write_cmd(handle, 0xE9, 0x08);
    }

    liot_hal_lcd_write_cmd(handle, 0x21, 0x00);

    liot_hal_lcd_write_cmd(handle, 0x3A, 0x05);

    uint8_t REG2[] = {
                        0x08, 
                        0x08, 
                        0x08, 
                        0x08
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x90, REG2, sizeof(REG2));

    liot_hal_lcd_write_cmd(handle, 0xBD, 0x06);
    liot_hal_lcd_write_cmd(handle, 0xBC, 0x00);

    uint8_t REG3[] = {
                        0x60, 
                        0x01, 
                        0x04
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xFF, REG3, sizeof(REG3));

    liot_hal_lcd_write_cmd(handle, 0xC3, 0x13);
    liot_hal_lcd_write_cmd(handle, 0xC4, 0x13);
    liot_hal_lcd_write_cmd(handle, 0xC9, 0x22);
    liot_hal_lcd_write_cmd(handle, 0xBE, 0x11);

    uint8_t REG4[] = {  
                        0x10, 
                        0x0E
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xE1, REG4, sizeof(REG4));

    uint8_t REG5[] = {
                        0x21, 
                        0x0C, 
                        0x02
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xDF, REG5, sizeof(REG5));

    uint8_t REG6[] = {
                        0x45, 
                        0x09, 
                        0x08, 
                        0x08, 
                        0x26, 
                        0x2A
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF0, REG6, sizeof(REG6));

    uint8_t REG7[] = {
                        0x43, 
                        0x70, 
                        0x72, 
                        0x36, 
                        0x37, 
                        0x6F
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF1, REG7, sizeof(REG7));

    uint8_t REG8[] = {
                        0x45, 
                        0x09, 
                        0x08, 
                        0x08, 
                        0x26, 
                        0x2A
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF2, REG8, sizeof(REG8));

    uint8_t REG9[] = {
                        0x43, 
                        0x70, 
                        0x72, 
                        0x36, 
                        0x37, 
                        0x6F
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xF3, REG9, sizeof(REG9));

    uint8_t REG10[] = {
                        0x1B, 
                        0x0B
                        };
    liot_hal_lcd_transmit_cmd(handle, 0xED, REG10, sizeof(REG10));

    liot_hal_lcd_write_cmd(handle, 0xAE, 0x77);
    liot_hal_lcd_write_cmd(handle, 0xCD, 0x63);

    uint8_t REG11[] = {
                        0x07, 
                        0x07, 
                        0x04, 
                        0x0E, 
                        0x0F, 
                        0x09, 
                        0x07, 
                        0x08, 
                        0x03
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x70, REG11, sizeof(REG11));

    liot_hal_lcd_write_cmd(handle, 0xE8, 0x34);

    uint8_t REG12[] = {
                        0x18, 
                        0x0D, 
                        0x71, 
                        0xED, 
                        0x70, 
                        0x70, 
                        0x18, 
                        0x0F, 
                        0x71, 
                        0xEF, 
                        0x70, 
                        0x70
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x62, REG12, sizeof(REG12));

    uint8_t REG13[] = {
                        0x18, 
                        0x11, 
                        0x71, 
                        0xF1, 
                        0x70, 
                        0x70, 
                        0x18, 
                        0x13, 
                        0x71, 
                        0xF3, 
                        0x70, 
                        0x70
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x63, REG13, sizeof(REG13));

    uint8_t REG14[] = {
                        0x28, 
                        0x29, 
                        0xF1, 
                        0x01, 
                        0xF1, 
                        0x00, 
                        0x07
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x64, REG14, sizeof(REG14));

    uint8_t REG15[] = {
                        0x3C, 
                        0x00, 
                        0xCD, 
                        0x67, 
                        0x45, 
                        0x45, 
                        0x10, 
                        0x00, 
                        0x00, 
                        0x00
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x66, REG15, sizeof(REG15));

    uint8_t REG16[] = {
                        0x00, 
                        0x3C, 
                        0x00, 
                        0x00, 
                        0x00, 
                        0x01, 
                        0x54, 
                        0x10, 
                        0x32, 
                        0x98
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x67, REG16, sizeof(REG16));

    uint8_t REG17[] = {
                        0x10, 
                        0x85, 
                        0x80, 
                        0x00, 
                        0x00, 
                        0x4E, 
                        0x00
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x74, REG17, sizeof(REG17));
    
    uint8_t REG18[] = {
                        0x3E, 
                        0x07
                        };
    liot_hal_lcd_transmit_cmd(handle, 0x98, REG18, sizeof(REG18));

    // liot_hal_lcd_write_cmd(handle, 0x35, 0x00);
    // liot_hal_lcd_write_cmd(handle, 0x21, 0x00);
    liot_hal_lcd_transmit_cmd(handle, 0x35, NULL, 0);
    liot_hal_lcd_transmit_cmd(handle, 0x21, NULL, 0);

    // liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    liot_hal_lcd_transmit_cmd(handle, 0x11, NULL, 0);
    liot_rtos_task_sleep_ms(120);

    liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    liot_hal_lcd_transmit_cmd(handle, 0x29, NULL, 0);
    liot_rtos_task_sleep_ms(20);
    return 0;
}

static int liot_gc9a01_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint8_t set_x_cmd[] = {sx>>8,sx,ex>>8,ex};
    liot_hal_lcd_transmit_cmd(handle, 0x2A, set_x_cmd, sizeof(set_x_cmd));
    uint8_t set_y_cmd[] = {sy>>8,sy,ey>>8,ey};
    liot_hal_lcd_transmit_cmd(handle, 0x2B, set_y_cmd, sizeof(set_y_cmd));
    liot_hal_lcd_write_cmd(handle, 0x2C, 0x00);

    return 0;
}

static int liot_gc9a01_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    int ret = 0;
    uint32_t TotalBytes = (ey-sy+1)*(ex-sx+1)*2;    //RGB565 = uint16
    liot_gc9a01_addrset(handle, sx, sy, ex, ey);         //设置填充区域横纵坐标
    ret = liot_hal_lcd_transmit_data(handle, buf, TotalBytes);
    return ret;
}

static int liot_gc9a01_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
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
    ret = liot_gc9a01_fill(handle, sx, sy, ex, ey, color_buf);
    liot_rtos_free(color_buf);
    return ret;
}

static int liot_gc9a01_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;
    if(on)  ret = liot_hal_lcd_write_cmd(handle, 0x29, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x28, 0x00);
    return ret;
}

static int liot_gc9a01_sleep_in(liot_hal_lcd_handle_t handle, bool in)
{
    int ret = 0;
    if(in)  ret = liot_hal_lcd_write_cmd(handle, 0x10, 0x00);
    else    ret = liot_hal_lcd_write_cmd(handle, 0x11, 0x00);
    return ret;
}

liot_hal_lcdDev_t liot_gc9a01_dev = {
    .func = {
        .init = liot_gc9a01_init,
        .addrSet = liot_gc9a01_addrset,
        .fill = liot_gc9a01_fill,
        .full = liot_gc9a01_full,
        .strWrite = NULL,
        .display_on = liot_gc9a01_display_on,
        .sleep_in = liot_gc9a01_sleep_in,
        .refresh = NULL,
    },
    .info = {
        .id = 0x9A01,
        .interface = LIOT_LCD_INTERFACE_LSPI 
                    | LIOT_LCD_INTERFACE_SPI
                    | LIOT_LCD_INTERFACE_8080,
        .width = 240,
        .height = 240,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_RGB565,
    },
};
