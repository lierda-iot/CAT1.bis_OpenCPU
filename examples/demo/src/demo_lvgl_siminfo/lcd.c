/**
 * @brief LVGL LCD driver for SIM signal info display demo.
 *
 * Adapted from demo_lvgl_key_tcp/lcd.c.
 * Only difference: lvgl_ai_thread calls liot_lvgl_signal_gui_setup()
 * instead of liot_lvgl_tcp_gui_setup().
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

#include "liot_os.h"
#include "liot_lcd.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "cmsis_os2.h"
#include "liot_sleep.h"
#include "lvgl.h"
#include "lcd_gui.h"
#include "pinmap.h"

LIOT_ADD_DISPLAY(liot_ssd1306_12864_dev);

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
static uint8_t *s_lvgl_buf = NULL;

static liot_lcd_handle_t s_lcd = NULL;
static liot_task_t s_lvgl_task = NULL;

static lv_disp_drv_t s_disp_drv;
static lv_disp_draw_buf_t s_draw_buf;

static liot_sem_t s_lvgl_ready_sem = NULL;

/* ------------------------------------------------------------------ */
/* Tick timer                                                          */
/* ------------------------------------------------------------------ */
static liot_timer_t s_lvgl_tick_timer = NULL;

static void lvgl_tick_timer_cb(void *arg)
{
    lv_tick_inc(1);
}

/* ------------------------------------------------------------------ */
/* Display flush                                                       */
/* ------------------------------------------------------------------ */
static void lcd_event_callback(void)
{
    lv_disp_flush_ready(&s_disp_drv);
}

static void liot_lvgl_lcd_disp_flush(int16_t x1, int16_t y1,
                                     int16_t x2, int16_t y2,
                                     uint8_t *buf)
{
    uint16_t i, j;
    uint16_t sizex = (uint16_t)(x2 - x1 + 1);
    uint16_t sizey = (uint16_t)(y2 - y1 + 1);

    for (j = 0; j < sizey; j++) {
        for (i = 0; i < sizex; i++) {
            liot_lcd_draw_point(s_lcd, i, j, buf[i + j * sizex]);
        }
    }

    liot_lcd_refresh(s_lcd);
    lcd_event_callback();
}

static void liot_lvgl_disp_flush(lv_disp_drv_t *disp,
                                 const lv_area_t *area,
                                 lv_color_t *color_p)
{
    liot_lvgl_lcd_disp_flush(area->x1, area->y1, area->x2, area->y2,
                             (uint8_t *)color_p);
}

/* ------------------------------------------------------------------ */
/* LCD init                                                            */
/* ------------------------------------------------------------------ */
static void liot_lvgl_lcd_disp_init(void)
{
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
        .lcdDev = &liot_ssd1306_12864_dev,
    };

    s_lcd = liot_lcd_init(&cfg);
    liot_lcd_clear_screen(s_lcd, RED);
    liot_lcd_refresh(s_lcd);
}

static void liot_lvgl_disp_init(void)
{
    liot_lvgl_lcd_disp_init();

    lv_disp_drv_init(&s_disp_drv);

    s_lvgl_buf = (uint8_t *)liot_rtos_malloc(
        liot_ssd1306_12864_dev.info.width *
        liot_ssd1306_12864_dev.info.height);

    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf, NULL,
        liot_ssd1306_12864_dev.info.width *
        liot_ssd1306_12864_dev.info.height * 2);

    s_disp_drv.hor_res      = liot_ssd1306_12864_dev.info.width;
    s_disp_drv.ver_res      = liot_ssd1306_12864_dev.info.height;
    s_disp_drv.flush_cb     = liot_lvgl_disp_flush;
    s_disp_drv.draw_buf     = &s_draw_buf;
    s_disp_drv.full_refresh = 1;
    lv_disp_drv_register(&s_disp_drv);
}

/* ------------------------------------------------------------------ */
/* LVGL main thread                                                    */
/* ------------------------------------------------------------------ */
static void lvgl_siminfo_thread(void *argv)
{
    liot_rtos_timer_create(&s_lvgl_tick_timer, 1, lvgl_tick_timer_cb, NULL);
    liot_rtos_timer_start(s_lvgl_tick_timer, 1);

    lv_init();
    liot_lvgl_disp_init();

    if (s_lvgl_buf == NULL) {
        liot_trace("lvgl buf alloc failed, task exit");
        liot_rtos_semaphore_release(s_lvgl_ready_sem);
        return;
    }

    liot_lvgl_signal_gui_setup();
    liot_rtos_semaphore_release(s_lvgl_ready_sem);

    while (1) {
        lv_task_handler();
        liot_rtos_task_sleep_ms(10);
    }
}

/* ------------------------------------------------------------------ */
/* I2C pin mux init                                                    */
/* ------------------------------------------------------------------ */
void lcd_port_system_init(void)
{
    Liot_SetPinFunc(SSD1306_I2C_SCL_PIN, L_PIN_FUNC_0);
    Liot_SetPinFunc(SSD1306_I2C_SDA_PIN, L_PIN_FUNC_0);

    Liot_GpioInit(SSD1306_I2C_SDA_GPIO, L_IO_OUTPUT, L_IO_NONE, NULL);
    Liot_GpioInit(SSD1306_I2C_SCL_GPIO, L_IO_OUTPUT, L_IO_NONE, NULL);

    Liot_GpioSetLevel(SSD1306_I2C_SCL_GPIO, L_IO_LOW);
    Liot_GpioSetLevel(SSD1306_I2C_SDA_GPIO, L_IO_LOW);
}

/* ------------------------------------------------------------------ */
/* Public entry                                                        */
/* ------------------------------------------------------------------ */
void lvgl_init(void)
{
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);

    lcd_port_system_init();

    Liot_SetPinFunc(LDO3V3_CTRL_PIN, LDO3V3_CTRL_FUNC);
    Liot_GpioInit(LDO3V3_CTRL_GPIO, L_IO_OUTPUT, L_IO_NONE, NULL);
    Liot_GpioSetLevel(LDO3V3_CTRL_GPIO, 0);
    osDelay(100);
    Liot_GpioSetLevel(LDO3V3_CTRL_GPIO, 1);
    osDelay(500);

    liot_rtos_semaphore_create(&s_lvgl_ready_sem, 0);
    liot_rtos_task_create(&s_lvgl_task, 10 * 1024, LIOT_APP_TASK_PRIORITY,
                          "lvgl_siminfo", &lvgl_siminfo_thread, NULL);
    liot_rtos_semaphore_wait(s_lvgl_ready_sem, osWaitForever);
}
