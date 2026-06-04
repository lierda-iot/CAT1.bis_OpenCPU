/**
 * @brief LVGL LCD driver for tgai application (LSDK port)
 *
 * Ported from PLAT liot_tgai_demo/hardware/src/lcd.c.
 * Replaces PLAT-only APIs (timer, clock, XIC, liot_gpio, pinmux)
 * with LSDK equivalents (liot_gpio2, liot_os).
 *
 * Tick timer: Uses liot_rtos_task_sleep_ms-based polling in the LVGL task
 *             instead of PLAT hardware timer, as LSDK does not expose
 *             TIMER/CLOCK/XIC APIs.
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
#include "font_awesome_symbols.h"
#include "pinmap.h"

LIOT_ADD_DISPLAY(liot_ssd1306_12864_dev);

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
uint8_t *liot_lvgl_buf = NULL;

liot_lcd_handle_t lcd = NULL;
liot_task_t lvglAppTaskRef = NULL;

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;

liot_sem_t lvgl_ready_sem = NULL;

/* ------------------------------------------------------------------ */
/* Tick: use a dedicated hardware timer in PLAT; fallback to a
 * software approach here. LVGL 8.x lv_tick_inc() must be called
 * every 1 ms.  We create a high-priority timer task for this.        */
/* ------------------------------------------------------------------ */
static liot_timer_t lvgl_tick_timer = NULL;

static void lvgl_tick_timer_cb(void *arg)
{
    lv_tick_inc(1);
}

/* ------------------------------------------------------------------ */
/* Display flush                                                       */
/* ------------------------------------------------------------------ */
void lcd_event_callback(void)
{
    lv_disp_flush_ready(&disp_drv);
}

void liot_lvgl_lcd_disp_flush(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t *buf)
{
    uint16_t i, j = 0;
    uint16_t sizex = x2 - x1 + 1;
    uint16_t sizey = y2 - y1 + 1;

    for (j = 0; j < sizey; j++)
    {
        for (i = 0; i < sizex; i++)
        {
            liot_lcd_draw_point(lcd, i, j, buf[i + j * sizex]);
        }
    }

    liot_lcd_refresh(lcd);
    lcd_event_callback();
}

static void liot_lvgl_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    liot_lvgl_lcd_disp_flush(area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p);
}

/* ------------------------------------------------------------------ */
/* LCD init                                                            */
/* ------------------------------------------------------------------ */
void liot_lvgl_lcd_disp_init(void)
{
    liot_lcd_config_t cfg = {
        .interface = {
            .type = LIOT_LCD_INTERFACE_I2C,
            .i2c = {
                .num = SSD1306_I2C_NUM,
                .scl = SSD1306_I2C_SCL_PIN,
                .sda = SSD1306_I2C_SDA_PIN,
                .speed = LIOT_STANDARD_MODE,
                .addr = 0x3C,
                .cb = NULL,
            },
            .blk.type = LIOT_LCD_NO_BACKLIGHT,
            .blk.pin = -1,
            .rst.pin = -1,
            .rst.delay = 0,
        },
        .lcdDev = &liot_ssd1306_12864_dev,
    };

    lcd = liot_lcd_init(&cfg);
    liot_lcd_clear_screen(lcd, RED);
    liot_lcd_refresh(lcd);
}

void liot_lvgl_disp_init(void)
{
    liot_lvgl_lcd_disp_init();

    lv_disp_drv_init(&disp_drv);

    liot_lvgl_buf = (uint8_t *)liot_rtos_malloc(liot_ssd1306_12864_dev.info.width * liot_ssd1306_12864_dev.info.height);
    lv_disp_draw_buf_init(&draw_buf, liot_lvgl_buf, NULL, liot_ssd1306_12864_dev.info.width * liot_ssd1306_12864_dev.info.height * 2);

    disp_drv.hor_res = liot_ssd1306_12864_dev.info.width;
    disp_drv.ver_res = liot_ssd1306_12864_dev.info.height;

    disp_drv.flush_cb = liot_lvgl_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);
}

/* ------------------------------------------------------------------ */
/* LVGL main thread                                                   */
/* ------------------------------------------------------------------ */
void lvgl_ai_thread(void *argv)
{
    /* Start 1ms tick timer */
    liot_rtos_timer_create(&lvgl_tick_timer, 1, lvgl_tick_timer_cb, NULL);
    liot_rtos_timer_start(lvgl_tick_timer, 1);

    lv_init();
    liot_lvgl_disp_init();

    if (liot_lvgl_buf == NULL) {
        liot_trace("lvgl buf alloc failed, task exit");
        liot_rtos_semaphore_release(lvgl_ready_sem);
        return;
    }

    // liot_lvgl_gui_setup();

    // liot_rtos_semaphore_release(lvgl_ready_sem);
    // lvgl_status_label_update("正在搜网");

    liot_lvgl_tcp_gui_setup();
    liot_rtos_semaphore_release(lvgl_ready_sem);
    while (1)
    {
        lv_task_handler();
        liot_rtos_task_sleep_ms(10);
    }
}

/* ------------------------------------------------------------------ */
/* I2C pin mux init — uses liot_gpio2 API                             */
/* ------------------------------------------------------------------ */
void lcd_port_system_init(void)
{
    Liot_SetPinFunc(SSD1306_I2C_SCL_PIN, L_PIN_FUNC_0);
    Liot_SetPinFunc(SSD1306_I2C_SDA_PIN, L_PIN_FUNC_0);

    /* SDA pin: set to GPIO output, low */
    Liot_GpioInit(SSD1306_I2C_SDA_GPIO, L_IO_OUTPUT, L_IO_NONE, NULL);

    /* SCL pin: set to GPIO output, low */
    Liot_GpioInit(SSD1306_I2C_SCL_GPIO, L_IO_OUTPUT, L_IO_NONE, NULL);


    Liot_GpioSetLevel(SSD1306_I2C_SCL_GPIO, L_IO_LOW);
    Liot_GpioSetLevel(SSD1306_I2C_SDA_GPIO, L_IO_LOW);



    /* Note: After this manual GPIO toggle, the I2C driver (liot_lcd_init)
     * will reconfigure these pins for I2C function. The sequence above
     * is a workaround to ensure the I2C bus is in a known idle state
     * before the I2C peripheral takes over. */
}

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



    /* Create semaphore before task to avoid race */
    liot_rtos_semaphore_create(&lvgl_ready_sem, 0);
    liot_rtos_task_create(&lvglAppTaskRef, 10 * 1024, LIOT_APP_TASK_PRIORITY,
                          "lvgl_ai_app", &lvgl_ai_thread, NULL);
    liot_rtos_semaphore_wait(lvgl_ready_sem, osWaitForever);
}

// void demo_tgai_csq_update_cb(uint8_t csq)
// {
//     const char *fa_icon;

//     if      (csq < 5)  fa_icon = FONT_AWESOME_SIGNAL_1;
//     else if (csq < 10) fa_icon = FONT_AWESOME_SIGNAL_2;
//     else if (csq < 15) fa_icon = FONT_AWESOME_SIGNAL_3;
//     else if (csq < 20) fa_icon = FONT_AWESOME_SIGNAL_4;
//     else if (csq < 32) fa_icon = FONT_AWESOME_SIGNAL_FULL;
//     else               fa_icon = FONT_AWESOME_SIGNAL_OFF;
   
//     lvgl_network_label_update((char *)fa_icon);
// }
