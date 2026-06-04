/**
 * @file demo_lcd_ssd1306.c
 * @brief SSD1306 OLED demonstration - I2C interface display
 *
 * @Author : Chenhz
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-11-29
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 * @note : This example demonstrates how to use the LCD API to initialize the
 *         SSD1306 (128x64 OLED) via I2C and display content.
 */
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_sleep.h"


LIOT_ADD_DISPLAY(liot_ssd1306_12864_dev);   //add LCD SSD1306 driver

#define SSD1306_I2C_NUM         1

#define SSD1306_I2C_SDA_GPIO    L_GPIO_16
#define SSD1306_I2C_SDA_PIN     (66)    /* Physical pin number for I2C driver */
#define SSD1306_I2C_SDA_FUNC    (2)

#define SSD1306_I2C_SCL_GPIO    L_GPIO_17
#define SSD1306_I2C_SCL_PIN     (57)    /* Physical pin number for I2C driver */
#define SSD1306_I2C_SCL_FUNC    (2)

void demo_lcd_ssd1306_task(void *argv)
{

    liot_hal_lcdDev_t *lcdDev = &liot_ssd1306_12864_dev;

        liot_lcd_config_t cfg = {
        .interface = {
            .type = LIOT_LCD_INTERFACE_I2C,
            .i2c = {
                .num   = SSD1306_I2C_NUM,
                .scl   = SSD1306_I2C_SCL_PIN,
                .sda   = SSD1306_I2C_SDA_PIN,
                .speed = LIOT_STANDARD_MODE,
                .addr  = 0x3C,
                .cb    = NULL,
            },
            .blk.type  = LIOT_LCD_NO_BACKLIGHT,
            .blk.pin   = -1,
            .rst.pin   = -1,
            .rst.delay = 0,
        },
        .lcdDev = lcdDev,
    };

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);

    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_rtos_task_sleep_ms(500);
    liot_lcd_handle_t lcd = liot_lcd_init(&cfg);


    uint32_t width = 0;
    if(lcdDev->info.direction == LIOT_LCD_DIR_0_ANGLE || lcdDev->info.direction == LIOT_LCD_DIR_180_ANGLE)
    {
        width = lcdDev->info.width;
    }
    else
    {
        width = lcdDev->info.height;
    }
    (void)width;



    while(1)
    {
        // full screen color test
        liot_trace("lcd ssd1306 task ");
        liot_lcd_clear_screen(lcd, RED);
        liot_lcd_refresh(lcd);
        liot_rtos_task_sleep_ms(1000);
        liot_lcd_clear_screen(lcd, BLACK);
        liot_lcd_refresh(lcd);
        liot_rtos_task_sleep_ms(1000);
        liot_lcd_clear_screen(lcd, BLUE);
        liot_lcd_refresh(lcd);
        liot_rtos_task_sleep_ms(1000);
        liot_lcd_clear_screen(lcd, BLACK);
        liot_lcd_refresh(lcd);
        liot_rtos_task_sleep_ms(1000);
    }
}
