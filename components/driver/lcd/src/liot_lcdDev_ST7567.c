/**
 * @File Name: liot_lcdDev_ST7567.c
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2024-09-06
 * 
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 * 
 */

#include "liot_lcdDev.h"

liot_hal_lcdDev_t liot_st7567_dev;

typedef struct 
{
    uint8_t *gram;
    uint16_t sx;
    uint16_t sy;
    uint16_t ex;
    uint16_t ey;
}_liot_st7567_info_t;

static _liot_st7567_info_t sLiotST7567Info = {0};

#define ST7567SetPoint(dev, __x, __y, __t)                                          \
    (__x >= 0 && __y >= 0 && __x < dev->info.width && __y < dev->info.height) ?      \
    ((__t) ? ((__t == 1) ? sLiotST7567Info.gram[((__y / 8) *dev->info.width) + __x] |= 1 << (__y % 8) : (sLiotST7567Info.gram[((__y / 8) *dev->info.width) + __x] ^= 1 << (__y % 8))) : (sLiotST7567Info.gram[((__y / 8) *dev->info.width) + __x] &= ~(1 << (__y % 8)))) : 0

static int liot_st7567_init(liot_hal_lcd_handle_t handle)
{
    liot_rtos_task_sleep_ms(100);

    liot_hal_lcd_transmit_cmd(handle, 0xE2, NULL, 0);
    liot_rtos_task_sleep_ms(10);
    liot_hal_lcd_transmit_cmd(handle, 0xA2, NULL, 0);   // 0xa2设置偏压比为1/9
    liot_hal_lcd_transmit_cmd(handle, 0xA0, NULL, 0);   // 0xA0设置列地址从00H开始
    liot_hal_lcd_transmit_cmd(handle, 0xC8, NULL, 0);   // 0xc8设置com扫描方向，从COM(n-1)到 COM0
    liot_hal_lcd_transmit_cmd(handle, 0x23, NULL, 0);   // vop粗调
    liot_hal_lcd_transmit_cmd(handle, 0x81, NULL, 0);   // vop双指令
    liot_hal_lcd_transmit_cmd(handle, 0x32, NULL, 0);   // vop微调
    liot_hal_lcd_transmit_cmd(handle, 0x2F, NULL, 0);   // 0x2f电源状态、输出缓冲开、内部电压调整开，电压调节开关
    liot_hal_lcd_transmit_cmd(handle, 0xB0, NULL, 0);   // 0xb0设置页列地址
    liot_hal_lcd_transmit_cmd(handle, 0xAF, NULL, 0);   // 0xaf设置显示LCD开关
    liot_hal_lcd_transmit_cmd(handle, 0xA6, NULL, 0);   // 0xa6设置正常显开关

    sLiotST7567Info.gram = liot_rtos_malloc(liot_st7567_dev.info.width * liot_st7567_dev.info.height / 8);

    return 0;
}

static int liot_st7567_refresh(liot_hal_lcd_handle_t handle)
{
    if(sLiotST7567Info.gram == NULL)
    {
        return -1;
    }

    uint8_t y = 0;

    for(y = 0; y < liot_st7567_dev.info.height / 8; y++)
    {
        liot_hal_lcd_transmit_cmd(handle, 0xB0 + y, NULL, 0);
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);
        liot_hal_lcd_transmit_cmd(handle, 0x10, sLiotST7567Info.gram + y * liot_st7567_dev.info.width, liot_st7567_dev.info.width/2);
        
        liot_hal_lcd_transmit_cmd(handle, 0x00 + (64 & 0x0f), NULL, 0);
        liot_hal_lcd_transmit_cmd(handle, 0x10 + (64 >> 4), sLiotST7567Info.gram + y * liot_st7567_dev.info.width + liot_st7567_dev.info.width/2, liot_st7567_dev.info.width/2);

        // liot_hal_lcd_transmit_data(handle, sLiotST7567Info.gram + y * liot_st7567_dev.info.width, liot_st7567_dev.info.width);
    }

    return 0;
}

static int liot_st7567_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    sLiotST7567Info.sx = sx;
    sLiotST7567Info.sy = sy;
    sLiotST7567Info.ex = ex;
    sLiotST7567Info.ey = ey;

    return 0;
}

static int liot_st7567_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
{
    uint16_t j = 0;
    uint8_t i, n, temp, m;
    int x0 = sx, y0 = sy;
    int sizex = ex - sx + 1;
    int sizey = ey - sy + 1;
    int sizeyl = sizey % 8;
    if(sizeyl == 0) sizeyl = 8;
    sizey = sizey / 8 + ((sizey % 8) ? 1 : 0);
    uint8_t *bmp = (uint8_t *)buf;

    for (n = 0; n < sizey; n++)
    {
        for (i = 0; i < sizex; i++)
        {
            temp = bmp[j];
            j++;
            for (m = 0; m < ((n == sizey - 1) ? sizeyl : 8); m++)
            {
                ST7567SetPoint(((liot_hal_lcd_config_t*)handle)->lcdDev, sx, sy, (temp & 0x01) );
                temp >>= 1;
                sy++;
            }
            sx++;
            if ((sx - x0) == sizex)
            {
                sx = x0;
                y0 = y0 + 8;
            }
            sy = y0;
        }
    }
    return 0;
}

static int liot_st7567_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t x = 0;
    uint16_t y = 0;

    for(y = sy; y <= ey; y++)
    {
        for(x = sx; x <= ex; x++)
        {
            ST7567SetPoint(((liot_hal_lcd_config_t*)handle)->lcdDev, x, y, (color >= 0x00FF) ? 1 : 0);
        }
    }

    return 0;
}

static int liot_st7567_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    if(on)
    {
        liot_hal_lcd_transmit_cmd(handle, 0xAF, NULL, 0);
    }
    else
    {
        liot_hal_lcd_transmit_cmd(handle, 0xAE, NULL, 0);
    }

    return 0;
}

liot_hal_lcdDev_t liot_st7567_dev = {
    .func = {
        .init = liot_st7567_init,
        .addrSet = liot_st7567_addrset,
        .fill = liot_st7567_fill,
        .full = liot_st7567_full,
        .strWrite = NULL,
        .display_on = liot_st7567_display_on,
        .sleep_in = NULL,
        .refresh = liot_st7567_refresh,
    },
    .info = {
        .id = 0x7567,
        .interface = LIOT_LCD_INTERFACE_LSPI 
                    | LIOT_LCD_INTERFACE_SPI 
                    | LIOT_LCD_INTERFACE_8080,
        .width = 128,
        .height = 64,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_MONO,
    },
};