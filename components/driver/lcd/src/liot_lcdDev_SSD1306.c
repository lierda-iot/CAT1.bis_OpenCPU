/**
 * @File Name: liot_lcdDev_ST7789.c
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2024-1-18
 * 
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 * 
 */

#include "liot_lcdDev.h"

liot_hal_lcdDev_t liot_ssd1306_12832_dev;
liot_hal_lcdDev_t liot_ssd1306_12864_dev;

typedef struct 
{
    uint8_t *gram;
    uint16_t sx;
    uint16_t sy;
    uint16_t ex;
    uint16_t ey;
}_liot_ssd1306_info_t;

static _liot_ssd1306_info_t sLiotSSD1306Info = {0};

#define SSD1306SetPoint(dev, __x, __y, __t)                                          \
    (__x >= 0 && __y >= 0 && __x < dev->info.width && __y < dev->info.height) ?      \
    ((__t) ? ((__t == 1) ? sLiotSSD1306Info.gram[((__y / 8) *dev->info.width) + __x] |= 1 << (__y % 8) : (sLiotSSD1306Info.gram[((__y / 8) *dev->info.width) + __x] ^= 1 << (__y % 8))) : (sLiotSSD1306Info.gram[((__y / 8) *dev->info.width) + __x] &= ~(1 << (__y % 8)))) : 0

static int liot_ssd1306_init(liot_hal_lcd_handle_t handle)
{
    if(((liot_hal_lcd_config_t*)handle)->lcdDev->info.height == 64)
    {
        liot_hal_lcd_transmit_cmd(handle, 0xAE, NULL, 0);//--turn off oled panel
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);//---set low column address
        liot_hal_lcd_transmit_cmd(handle, 0x10, NULL, 0);//---set high column address
        liot_hal_lcd_transmit_cmd(handle, 0x40, NULL, 0);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
        liot_hal_lcd_transmit_cmd(handle, 0x81, NULL, 0);//--set contrast control register
        liot_hal_lcd_transmit_cmd(handle, 0xCF, NULL, 0); // Set SEG Output Current Brightness
        liot_hal_lcd_transmit_cmd(handle, 0xA1, NULL, 0);//--Set SEG/Column Mapping     0xa0左右反置 0xa1正常
        liot_hal_lcd_transmit_cmd(handle, 0xC8, NULL, 0);//Set COM/Row Scan Direction   0xc0上下反置 0xc8正常
        liot_hal_lcd_transmit_cmd(handle, 0xA8, NULL, 0);//--set multiplex ratio(1 to 64)
        liot_hal_lcd_transmit_cmd(handle, 0x3f, NULL, 0);//--1/64 duty
        liot_hal_lcd_transmit_cmd(handle, 0xD3, NULL, 0);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);//-not offset
        liot_hal_lcd_transmit_cmd(handle, 0xd5, NULL, 0);//--set display clock divide ratio/oscillator frequency
        liot_hal_lcd_transmit_cmd(handle, 0x80, NULL, 0);//--set divide ratio, Set Clock as 100 Frames/Sec
        liot_hal_lcd_transmit_cmd(handle, 0xD9, NULL, 0);//--set pre-charge period
        liot_hal_lcd_transmit_cmd(handle, 0xF1, NULL, 0);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
        liot_hal_lcd_transmit_cmd(handle, 0xDA, NULL, 0);//--set com pins hardware configuration
        liot_hal_lcd_transmit_cmd(handle, 0x12, NULL, 0);
        liot_hal_lcd_transmit_cmd(handle, 0xDB, NULL, 0);//--set vcomh
        liot_hal_lcd_transmit_cmd(handle, 0x40, NULL, 0);//Set VCOM Deselect Level
        if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_SPI)
        {
            liot_hal_lcd_transmit_cmd(handle, 0x20, NULL, 0);   //-Set Page Addressing Mode (0x00/0x01/0x02)
            liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   //
        }
        else if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_I2C)
        {
            liot_hal_lcd_transmit_cmd(handle, 0x20, NULL, 0);   //-Set Page Addressing Mode (0x00/0x01/0x02)
            liot_hal_lcd_transmit_cmd(handle, 0x02, NULL, 0);   //
        }
        liot_hal_lcd_transmit_cmd(handle, 0x8D, NULL, 0);//--set Charge Pump enable/disable
        liot_hal_lcd_transmit_cmd(handle, 0x14, NULL, 0);//--set(0x10) disable
        liot_hal_lcd_transmit_cmd(handle, 0xA4, NULL, 0);// Disable Entire Display On (0xa4/0xa5)
        // liot_hal_lcd_transmit_cmd(handle, 0xA6, NULL, 0);// Disable Inverse Display On (0xa6/a7) 
        liot_hal_lcd_transmit_cmd(handle, 0xA7, NULL, 0);// Enable Inverse Display On (0xa6/a7) 
        liot_hal_lcd_transmit_cmd(handle, 0xAF, NULL, 0); /*display ON*/ 
    }
    else if(((liot_hal_lcd_config_t*)handle)->lcdDev->info.height == 32)
    {
        liot_hal_lcd_transmit_cmd(handle, 0xAE, NULL, 0);   /*display off*/
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   /*set lower column address*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x10, NULL, 0);   /*set higher column address*/
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   /*set display start line*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xB0, NULL, 0);   /*set page address*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x81, NULL, 0);   /*contract control*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xff, NULL, 0);   /*128*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xA1, NULL, 0);   /*set segment remap*/ 
        // liot_hal_lcd_transmit_cmd(handle, 0xA6, NULL, 0);   /*normal / reverse*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xA7, NULL, 0);   /*normal / reverse*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xA8, NULL, 0);   /*multiplex ratio*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x1F, NULL, 0);   /*duty = 1/32*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xC8, NULL, 0);   /*Com scan direction*/ 
        liot_hal_lcd_transmit_cmd(handle, 0xD3, NULL, 0);   /*set display offset*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   
        liot_hal_lcd_transmit_cmd(handle, 0xD5, NULL, 0);   /*set osc division*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x80, NULL, 0);   
        liot_hal_lcd_transmit_cmd(handle, 0xD9, NULL, 0);   /*set pre-charge period*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x1f, NULL, 0);   
        liot_hal_lcd_transmit_cmd(handle, 0xDA, NULL, 0);   /*set COM pins*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   
        liot_hal_lcd_transmit_cmd(handle, 0xdb, NULL, 0);   /*set vcomh*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x40, NULL, 0);   
        liot_hal_lcd_transmit_cmd(handle, 0x8d, NULL, 0);   /*set charge pump enable*/ 
        liot_hal_lcd_transmit_cmd(handle, 0x14, NULL, 0);   
        if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_SPI)
        {
            liot_hal_lcd_transmit_cmd(handle, 0x20, NULL, 0);   //-Set Page Addressing Mode (0x00/0x01/0x02)
            liot_hal_lcd_transmit_cmd(handle, 0x00, NULL, 0);   //
        }
        else if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_I2C)
        {
            liot_hal_lcd_transmit_cmd(handle, 0x20, NULL, 0);   //-Set Page Addressing Mode (0x00/0x01/0x02)
            liot_hal_lcd_transmit_cmd(handle, 0x02, NULL, 0);   //
        }
        liot_hal_lcd_transmit_cmd(handle, 0xA1, NULL, 0);//--Set SEG/Column Mapping     0xa0左右反置 0xa1正常
        liot_hal_lcd_transmit_cmd(handle, 0xC8, NULL, 0);//Set COM/Row Scan Direction   0xc0上下反置 0xc8正常


        liot_hal_lcd_transmit_cmd(handle, 0xAF, NULL, 0); /*display ON*/ 
    }
// #if PSRAM_EXIST
//     sLiotSSD1306Info.gram = liot_rtos_psram_malloc(((liot_hal_lcd_config_t*)handle)->lcdDev->info.width * ((liot_hal_lcd_config_t*)handle)->lcdDev->info.height / 8);
// #else
    sLiotSSD1306Info.gram = liot_rtos_malloc(((liot_hal_lcd_config_t*)handle)->lcdDev->info.width * ((liot_hal_lcd_config_t*)handle)->lcdDev->info.height / 8);
// #endif

    return 0;
}

static int liot_ssd1306_refresh(liot_hal_lcd_handle_t handle)
{
    uint8_t i = 0;

    if(sLiotSSD1306Info.gram == NULL)
    {
        return -1;
    }

    if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_SPI)
    {
        liot_hal_lcd_transmit_data(handle, sLiotSSD1306Info.gram, ((liot_hal_lcd_config_t*)handle)->lcdDev->info.width * ((liot_hal_lcd_config_t*)handle)->lcdDev->info.height / 8);
    }
    else if(((liot_hal_lcd_config_t*)handle)->interface.type == LIOT_LCD_INTERFACE_I2C)
    {
        for(i = 0; i < ((liot_hal_lcd_config_t*)handle)->lcdDev->info.height / 8; i++)
        {
            liot_hal_lcd_transmit_cmd(handle, 0xB0 + i, NULL, 0);
            liot_hal_lcd_transmit_data(handle, &sLiotSSD1306Info.gram[i * ((liot_hal_lcd_config_t*)handle)->lcdDev->info.width], ((liot_hal_lcd_config_t*)handle)->lcdDev->info.width);
        }
    }


    return 0;
}

static int liot_ssd1306_addrset(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    sLiotSSD1306Info.sx = sx;
    sLiotSSD1306Info.sy = sy;
    sLiotSSD1306Info.ex = ex;
    sLiotSSD1306Info.ey = ey;

    return 0;
}

static int liot_ssd1306_fill(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, void *buf)
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
                SSD1306SetPoint(((liot_hal_lcd_config_t*)handle)->lcdDev, sx, sy, (temp & 0x01) );
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
void uart_printf(const char *fmt, ...);
static int liot_ssd1306_full(liot_hal_lcd_handle_t handle, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t i, j;
    // uart_printf("full\r\n");
    for(i = sx; i <= ex; i++)
    {
        for(j = sy; j <= ey; j++)
        {
            SSD1306SetPoint(((liot_hal_lcd_config_t*)handle)->lcdDev, i, j, (color > 0) ? 1 : 0);
            // uart_printf("ssd1306:%d,%d,%d\n", i, j, (color > 0) ? 1 : 0);
        }
    }

    return 0;
}

static int liot_ssd1306_display_on(liot_hal_lcd_handle_t handle, bool on)
{
    int ret = 0;
    if(on)  
    {
        ret = liot_hal_lcd_transmit_cmd(handle, 0x8D, NULL, 0);//电荷泵使能
        ret = liot_hal_lcd_transmit_cmd(handle, 0x14, NULL, 0);//开启电荷泵
        ret = liot_hal_lcd_transmit_cmd(handle, 0xAF, NULL, 0);//点亮屏幕
    }
    else
    {
        ret = liot_hal_lcd_transmit_cmd(handle, 0x8D, NULL, 0);//电荷泵使能
        ret = liot_hal_lcd_transmit_cmd(handle, 0x10, NULL, 0);//关闭电荷泵
        ret = liot_hal_lcd_transmit_cmd(handle, 0xAE, NULL, 0);//关闭屏幕
    }
    return ret;
}

liot_hal_lcdDev_t liot_ssd1306_12832_dev = {
    .func = {
        .init = liot_ssd1306_init,
        .addrSet = liot_ssd1306_addrset,
        .fill = liot_ssd1306_fill,
        .full = liot_ssd1306_full,
        .strWrite = NULL,
        .display_on = liot_ssd1306_display_on,
        .sleep_in = NULL,
        .refresh = liot_ssd1306_refresh,
    },
    .info = {
        .id = 0x1306,
        .interface = LIOT_LCD_INTERFACE_SPI 
                    | LIOT_LCD_INTERFACE_I2C
                    | LIOT_LCD_INTERFACE_8080,
        .width = 128,
        .height = 32,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_MONO,
    },
};

liot_hal_lcdDev_t liot_ssd1306_12864_dev = {
    .func = {
        .init = liot_ssd1306_init,
        .addrSet = liot_ssd1306_addrset,
        .fill = liot_ssd1306_fill,
        .full = liot_ssd1306_full,
        .strWrite = NULL,
        .display_on = liot_ssd1306_display_on,
        .sleep_in = NULL,
        .refresh = liot_ssd1306_refresh,
    },
    .info = {
        .id = 0x1306,
        .interface = LIOT_LCD_INTERFACE_SPI 
                    | LIOT_LCD_INTERFACE_I2C
                    | LIOT_LCD_INTERFACE_8080,
        .width = 128,
        .height = 64,
        .direction = LIOT_LCD_DIR_0_ANGLE,
        .color_depth = LIOT_LCD_COLOR_MONO,
    },
};

