/**
 * @brief LVGL LCD driver for dual eye GIF display demo.
 *
 * Uses two GC9A01 displays over LSPI and shows independent 240x240
 * GIF animations through LVGL.
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
#include "dual_eye_gui.h"

LIOT_ADD_DISPLAY(liot_gc9a01_dev);

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */

/* Full-screen buffer: 240 * 240 * 2 bytes (RGB565) = 115200 B per display. */
#define LCD_BUF_LINES   240
#define LVGL_FPS_LOG_INTERVAL_MS 1000

typedef enum
{
    LVGL_DISP_LEFT = 0,
    LVGL_DISP_RIGHT,
    LVGL_DISP_NUM,
} lvgl_disp_id_t;

typedef struct
{
    liot_lcd_handle_t lcd;
    lv_disp_drv_t disp_drv;
    lv_disp_draw_buf_t draw_buf;
    lv_color_t *buf;
    lv_disp_t *disp;
    lvgl_disp_id_t id;
    const char *name;
    uint32_t fps_window_start_ms;
    uint32_t fps_flush_count;
} lvgl_lcd_disp_t;

static lvgl_lcd_disp_t s_disps[LVGL_DISP_NUM] = {0};
static liot_task_t s_dual_eye_lvgl_task = NULL;
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

/**
 * @brief LSPI requires a DMA callback during initialization.
 *
 * The demo uses sync=true and marks LVGL flush completion after
 * liot_lcd_write() returns.
 */
static void lcd_event_callback(void) {}

static const char *liot_lvgl_disp_name(lvgl_disp_id_t id)
{
    return (id == LVGL_DISP_RIGHT) ? "right" : "left";
}

static void liot_lvgl_disp_fps_update(lvgl_lcd_disp_t *ctx,
                                      const lv_area_t *area)
{
    uint32_t now = lv_tick_get();
    uint32_t elapsed;
    uint32_t fps;
    uint32_t w;
    uint32_t h;
    uint32_t len;

    if (ctx->fps_window_start_ms == 0) {
        ctx->fps_window_start_ms = now;
    }

    ctx->fps_flush_count++;
    elapsed = now - ctx->fps_window_start_ms;
    if (elapsed < LVGL_FPS_LOG_INTERVAL_MS) {
        return;
    }

    w = (uint32_t)(area->x2 - area->x1 + 1);
    h = (uint32_t)(area->y2 - area->y1 + 1);
    len = w * h * (uint32_t)sizeof(lv_color_t);
    fps = (ctx->fps_flush_count * 1000U) / elapsed;

    liot_trace("lvgl %s flush fps:%lu area:%d,%d,%d,%d len:%lu lcd:%p",
               ctx->name,
               (unsigned long)fps,
               area->x1, area->y1, area->x2, area->y2,
               (unsigned long)len,
               ctx->lcd);

    ctx->fps_window_start_ms = now;
    ctx->fps_flush_count = 0;
}

/**
 * @brief LVGL flush callback writes a rectangular region to the LCD.
 *
 * @param[in] disp     LVGL display driver handle
 * @param[in] area     Region to update
 * @param[in] color_p  RGB565 pixel buffer provided by LVGL
 */
static void liot_lvgl_disp_flush(lv_disp_drv_t *disp,
                                 const lv_area_t *area,
                                 lv_color_t *color_p)
{
    lvgl_lcd_disp_t *ctx = (lvgl_lcd_disp_t *)disp->user_data;

    if (ctx == NULL || ctx->lcd == NULL) {
        lv_disp_flush_ready(disp);
        return;
    }

    liot_lcd_errcode_e ret = liot_lcd_write(ctx->lcd,
                                            area->x1, area->y1,
                                            area->x2, area->y2,
                                            (uint8_t *)color_p);
    if (ret != LIOT_LCD_OK) {
        liot_trace("lvgl %s lcd write failed: %d", ctx->name, ret);
    }

    liot_lvgl_disp_fps_update(ctx, area);

    /*
     * LSPI is configured as sync=true below, so liot_lcd_write() has
     * completed the transfer when it returns.
     */
    lv_disp_flush_ready(disp);
}

/* ------------------------------------------------------------------ */
/* LCD init                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize the GC9A01 LCD via LSPI.
 *
 * Configuration matches demo_lcd.c (LSPI port 2, 33 MHz, CS0).
 */
static bool liot_lvgl_lcd_disp_init(lvgl_lcd_disp_t *ctx, lvgl_disp_id_t id)
{
    liot_lcd_config_t cfg = {
        .interface = {
            .type                = LIOT_LCD_INTERFACE_LSPI,
            .lspi.num            = LIOT_LSPI_PORT2,
            .lspi.lcd_3_line_spi = false,
            .lspi.lcd_2_data_lane = false,
            .lspi.speed          = LIOT_LSPI_51MHZ,
            .lspi.sync           = true,
            .lspi.cb             = lcd_event_callback,
            .lspi.cs             = LIOT_LSPI_CS0,
            .blk.type            = LIOT_LCD_BACKLIGHT_GPIO,
            .blk.pin             = 97,
            .rst.pin             = 49,
            .rst.delay           = 100,
        },
        .lcdDev = &liot_gc9a01_dev,
    };

    if (id == LVGL_DISP_RIGHT) {
        cfg.interface.lspi.cs = LIOT_LSPI_CS1;
        cfg.interface.lspi.speed = LIOT_LSPI_51MHZ;
        cfg.interface.blk.type = LIOT_LCD_BACKLIGHT_PWM;
        cfg.interface.blk.pin = 102;
        cfg.interface.blk.pwm_num = LIOT_PWM_3;
        cfg.interface.rst.pin = -1;
    }

    ctx->lcd = liot_lcd_init(&cfg);
    if (ctx->lcd == NULL) {
        liot_trace("lcd %d init failed", id);
        return false;
    }

    liot_lcd_set_brightness(ctx->lcd, 100);
    liot_lcd_clear_screen(ctx->lcd, 0xffff);
    return true;
}

/**
 * @brief Initialize LVGL display driver for the GC9A01 (240x240).
 */
static bool liot_lvgl_disp_init_one(lvgl_lcd_disp_t *ctx, lvgl_disp_id_t id)
{
    ctx->id = id;
    ctx->name = liot_lvgl_disp_name(id);

    if (!liot_lvgl_lcd_disp_init(ctx, id)) {
        return false;
    }

    lv_disp_drv_init(&ctx->disp_drv);

    ctx->buf = (lv_color_t *)liot_rtos_malloc(
        liot_gc9a01_dev.info.width * LCD_BUF_LINES *
        sizeof(lv_color_t));

    if (ctx->buf == NULL) {
        liot_trace("lvgl lcd %d buf alloc failed", id);
        return false;
    }

    lv_disp_draw_buf_init(&ctx->draw_buf, ctx->buf, NULL,
        liot_gc9a01_dev.info.width * LCD_BUF_LINES);

    ctx->disp_drv.hor_res      = (lv_coord_t)liot_gc9a01_dev.info.width;
    ctx->disp_drv.ver_res      = (lv_coord_t)liot_gc9a01_dev.info.height;
    ctx->disp_drv.flush_cb     = liot_lvgl_disp_flush;
    ctx->disp_drv.draw_buf     = &ctx->draw_buf;
    ctx->disp_drv.user_data    = ctx;
    ctx->disp = lv_disp_drv_register(&ctx->disp_drv);
    if (ctx->disp == NULL) {
        liot_trace("lvgl lcd %d disp register failed", id);
        return false;
    }

    return true;
}

static bool liot_lvgl_disp_init(void)
{
    for (lvgl_disp_id_t id = LVGL_DISP_LEFT; id < LVGL_DISP_NUM; id++) {
        if (!liot_lvgl_disp_init_one(&s_disps[id], id)) {
            return false;
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* LVGL main thread                                                    */
/* ------------------------------------------------------------------ */
static void dual_eye_lvgl_thread(void *argv)
{
    liot_rtos_timer_create(&s_lvgl_tick_timer, 1, lvgl_tick_timer_cb, NULL);
    liot_rtos_timer_start(s_lvgl_tick_timer, 1);

    lv_init();
    if (!liot_lvgl_disp_init()) {
        liot_trace("lvgl disp init failed, task exit");
        liot_rtos_semaphore_release(s_lvgl_ready_sem);
        return;
    }

    dual_eye_gui_setup(s_disps[LVGL_DISP_LEFT].disp,
                       s_disps[LVGL_DISP_RIGHT].disp);
    liot_rtos_semaphore_release(s_lvgl_ready_sem);

    while (1) {
        lv_task_handler();
        liot_rtos_task_sleep_ms(2);
    }
}

/* ------------------------------------------------------------------ */
/* Public entry                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize power, GPIO and start the LVGL thread.
 *
 * Blocks until the LVGL display and GUI are ready.
 */
void dual_eye_lvgl_init(void)
{
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);

    /* PA control, same as demo_lcd.c. */
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    liot_rtos_semaphore_create(&s_lvgl_ready_sem, 0);
    liot_rtos_task_create(&s_dual_eye_lvgl_task, 32 * 1024, LIOT_APP_TASK_PRIORITY,
                          "dual_eye_lvgl", &dual_eye_lvgl_thread, NULL);
    liot_rtos_semaphore_wait(s_lvgl_ready_sem, osWaitForever);
}
