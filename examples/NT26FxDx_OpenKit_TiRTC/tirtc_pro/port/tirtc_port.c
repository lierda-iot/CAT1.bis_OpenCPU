/* SPDX-License-Identifier: Apache-2.0
 * Lierda V04 display and touch adaptation.
 * Derived from the board-tested official demo_watch ports:
 *   examples/NT26FxDx_OpenKit/demo/src/demo_watch/framework/platform/
 *     board/board_lierda_watch.c
 *     display/app_display_port_lvgl.c
 * Upstream: https://github.com/lierda-iot/CAT1.bis_OpenCPU
 * The original sources and project license remain in this complete SDK.
 * LCD/TP drivers retain their Lierda copyright notices.
 */
#include "tirtc_port.h"
#include "tirtc_rotation.h"
#include "tirtc_lcd_landscape.h"
#include "tirtc_lcd_rounder.h"
#include "tirtc_ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "liot_tp.h"
#include "lvgl.h"

#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
#error "The verified ST7789 transport requires native RGB565"
#endif

LIOT_ADD_DISPLAY(liot_st7789_dev);
LIOT_ADD_TP_DEV(g_liot_tp_ft6336);

#define PORT_TICK_MS 1U
#define PORT_HANDLER_MS 10U
#define PORT_TASK_STACK (12U * 1024U)
/* Camera/codec run at 11; audio remains above both at 12. Software scaling
 * must not delay completed camera DMA until the next redraw. */
#define PORT_TASK_PRIORITY 10U
#define PORT_TOUCH_PRESSED (1UL << 31)
#define PORT_VIDEO_REPORT_MS 5000U

static liot_task_t s_ui_task;
static liot_sem_t s_ready_sem;
static liot_timer_t s_tick_timer;
static liot_mutex_t s_state_mutex;
static liot_lcd_handle_t s_lcd;
static liot_tp_handle_t s_touch;
static lv_disp_drv_t s_display_driver;
static lv_disp_draw_buf_t s_draw_buffer;
static lv_indev_drv_t s_input_driver;
static lv_color_t *s_pixels;
/* The SDK retains this pointer. Preserve the original descriptor (also used
 * by the proven touch calibration); native dimensions do not get swapped. */
static liot_hal_lcdDev_t s_lcd_device;
/* One aligned 32-bit store publishes an entire touch sample to the UI task. */
static volatile uint32_t s_touch_sample;
static volatile uint8_t s_requested_brightness = 100U;
static volatile int s_start_result = -1;
/* GUI-owned fixed-size counters. Times include higher-priority preemption;
 * handler includes prepare/write, so these are not additive CPU costs. */
typedef struct { uint32_t sum, max; } port_elapsed_t;
static struct {
    uint32_t tick, loops, flushes, pixels, failures;
    port_elapsed_t process, handler, prepare, write;
    tirtc_ui_video_stats_t previous;
} s_video_stats;

static void port_elapsed(port_elapsed_t *value, uint32_t started)
{
    uint32_t elapsed = lv_tick_get() - started;
    value->sum += elapsed;
    if (elapsed > value->max) value->max = elapsed;
}

static void port_video_report(void)
{
    if (!TIRTC_ENABLE_DEBUG_LOG) return;
    tirtc_ui_video_stats_t current;
    uint32_t now = lv_tick_get(), dt = now - s_video_stats.tick;
    uint32_t published, consumed, lcd_frames;
    if (dt < PORT_VIDEO_REPORT_MS) return;
    tirtc_ui_video_stats(&current);
    published = current.published - s_video_stats.previous.published;
    consumed = current.consumed - s_video_stats.previous.consumed;
    lcd_frames = current.lcd_frames - s_video_stats.previous.lcd_frames;
    if (current.video_visible || s_video_stats.previous.video_visible || published || lcd_frames) {
        /* LVGL owns a separate fixed pool. Inspect it only in this existing
         * five-second report, on the GUI task, never in the frame/flush path.
         * Free/largest are bytes; fragmentation is LVGL's percentage. */
        lv_mem_monitor_t memory;
        lv_mem_monitor(&memory);
        uint32_t fps100 = (uint32_t)((uint64_t)lcd_frames * 100000U / dt);
        TIRTC_LOG_DEBUG("[GUI35] dt=%lu pub=%lu consume=%lu replace=%lu lcd_frames=%lu lcd_fps_x100=%lu flush=%lu pixels=%lu fail=%lu lv_free=%lu lv_largest=%lu lv_frag=%u draw_fast=%lu draw_fallback=%lu",
            (unsigned long)dt, (unsigned long)published, (unsigned long)consumed,
            (unsigned long)(current.replaced - s_video_stats.previous.replaced),
            (unsigned long)lcd_frames, (unsigned long)fps100,
            (unsigned long)s_video_stats.flushes, (unsigned long)s_video_stats.pixels,
            (unsigned long)s_video_stats.failures,
            (unsigned long)memory.free_size, (unsigned long)memory.free_biggest_size,
            (unsigned)memory.frag_pct,
            (unsigned long)(current.draw_fast - s_video_stats.previous.draw_fast),
            (unsigned long)(current.draw_fallback - s_video_stats.previous.draw_fallback));
        TIRTC_LOG_DEBUG("[GUI35] wall_ms=sum/max loops=%lu process=%lu/%lu handler=%lu/%lu prepare=%lu/%lu lspi=%lu/%lu (prepare/lspi within handler)",
            (unsigned long)s_video_stats.loops,
            (unsigned long)s_video_stats.process.sum, (unsigned long)s_video_stats.process.max,
            (unsigned long)s_video_stats.handler.sum, (unsigned long)s_video_stats.handler.max,
            (unsigned long)s_video_stats.prepare.sum, (unsigned long)s_video_stats.prepare.max,
            (unsigned long)s_video_stats.write.sum, (unsigned long)s_video_stats.write.max);
    }
    memset(&s_video_stats, 0, sizeof(s_video_stats));
    s_video_stats.tick = now;
    s_video_stats.previous = current;
}

static void port_flush_complete(lv_disp_drv_t *driver, bool success)
{
    if (!success) ++s_video_stats.failures;
    tirtc_ui_video_flush_done(success, lv_disp_flush_is_last(driver));
    lv_disp_flush_ready(driver);
}

/* Protect only bounded state/frame/mailbox copies, never LVGL rendering or I/O. */
void tirtc_ui_platform_lock(void)
{
    if (s_state_mutex != NULL) {
        (void)liot_rtos_mutex_lock(s_state_mutex, LIOT_WAIT_FOREVER);
    }
}

void tirtc_ui_platform_unlock(void)
{
    if (s_state_mutex != NULL) (void)liot_rtos_mutex_unlock(s_state_mutex);
}

void tirtc_port_set_brightness(uint8_t percent)
{
    s_requested_brightness = percent > 100U ? 100U : percent;
}

static int port_power_on(void)
{
    LiotSleepModeCfg_t sleep = {LIOT_SLEEP_MODE_NORMAL};

    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS ||
        Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS) {
        liot_trace("[tirtc] 3.3V domain setup failed");
        return -1;
    }
    (void)Liot_SetPinFunc(16, L_PIN_FUNC_0);
    if (Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS) {
        liot_trace("[tirtc] shared 3.3V enable failed");
        return -1;
    }
    /* This rail powers the fitted LCD/touch assembly and other peripherals. */
    liot_rtos_task_sleep_ms(500U);
    if (Liot_SleepSetMode(&sleep) != LIOT_SLEEP_SUCCESS) {
        liot_trace("[tirtc] normal sleep mode setup failed");
        return -1;
    }
    return 0;
}

static void port_lcd_event(void)
{
    /* Synchronous LSPI writes complete before the flush callback returns. */
}

static int port_lcd_init(void)
{
    liot_lcd_config_t cfg;
    /* V04/24031C12: MX+MV+BGR maps logical (x,y) to native (239-y,x),
     * exactly the former MADCTL 0xC8 + software CCW90 composition. */
    uint8_t madctl = 0x68;

    if (liot_st7789_dev.info.width != TIRTC_PHYSICAL_WIDTH ||
        liot_st7789_dev.info.height != TIRTC_PHYSICAL_HEIGHT ||
        liot_st7789_dev.info.direction != LIOT_LCD_DIR_180_ANGLE ||
        liot_st7789_dev.info.color_depth != LIOT_LCD_COLOR_RGB565) {
        liot_trace("[tirtc] unsupported LCD geometry/calibration");
        return -1;
    }
    s_lcd_device = liot_st7789_dev;
    s_lcd_device.info.direction = LIOT_LCD_DIR_90_ANGLE;

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface.type = LIOT_LCD_INTERFACE_LSPI;
    cfg.interface.lspi.num = LIOT_LSPI_PORT2;
    cfg.interface.lspi.cs = LIOT_LSPI_CS0;
    cfg.interface.lspi.speed = LIOT_LSPI_51MHZ;
    cfg.interface.lspi.sync = true;
    cfg.interface.lspi.cb = port_lcd_event;
    cfg.interface.blk.type = LIOT_LCD_BACKLIGHT_GPIO;
    cfg.interface.blk.pin = 103;
    cfg.interface.rst.pin = 78;
    cfg.interface.rst.delay = 100U;
    cfg.lcdDev = &s_lcd_device;
    s_lcd = liot_lcd_init(&cfg);
    if (s_lcd == NULL) {
        liot_trace("[tirtc] ST7789 init failed");
        return -1;
    }
    /* F6D_A transmit_cmd returns boolean 1 on success, unlike the enum
     * declared in the public header; verified against the linked SDK. */
    if ((int)liot_lcd_send_cmd(s_lcd, 0x36, &madctl, sizeof(madctl)) != 1) {
        liot_trace("[tirtc] ST7789 color order setup failed");
        return -1;
    }
    /* SDK init reads the original global direction and initially selects C0.
     * Override it before clearing; clone direction makes clear use 320x240. */
    liot_trace("[tirtc] ST7789 MADCTL=0x68; hardware landscape RGB565");
    if (liot_lcd_clear_screen(s_lcd, BLACK) != LIOT_LCD_OK) {
        liot_trace("[tirtc] ST7789 initial clear failed");
        return -1;
    }
    (void)liot_lcd_set_brightness(s_lcd, s_requested_brightness);
    return 0;
}

static void port_flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    tirtc_rotation_area_t logical, clipped;
    int prepared;
    liot_lcd_errcode_e result;
    uint32_t started;

    ++s_video_stats.flushes;
    if (s_lcd == NULL || area == NULL || pixels == NULL) {
        port_flush_complete(driver, false);
        return;
    }
    logical.x1 = area->x1; logical.y1 = area->y1;
    logical.x2 = area->x2; logical.y2 = area->y2;
    started = lv_tick_get();
    prepared = tirtc_lcd_pack_landscape(pixels,
                                       TIRTC_SCREEN_WIDTH * TIRTC_SCREEN_HEIGHT,
                                       &logical, &clipped);
    port_elapsed(&s_video_stats.prepare, started);
    if (prepared != TIRTC_ROTATION_OK) {
        if (prepared == TIRTC_ROTATION_INVALID) liot_trace("[tirtc] invalid LCD flush area");
        port_flush_complete(driver, false);
        return;
    }
    /* The controller now accepts packed logical landscape pixels directly.
     * Keep the 32-pixel rounder and the same synchronous LSPI byte lengths. */
    s_video_stats.pixels += (uint32_t)(clipped.x2 - clipped.x1 + 1) *
                           (uint32_t)(clipped.y2 - clipped.y1 + 1);
    started = lv_tick_get();
    result = liot_lcd_write(s_lcd, (uint16_t)clipped.x1, (uint16_t)clipped.y1,
                           (uint16_t)clipped.x2, (uint16_t)clipped.y2, (uint8_t *)pixels);
    port_elapsed(&s_video_stats.write, started);
    if (result != LIOT_LCD_OK) {
        liot_trace("[tirtc] LCD flush failed: %d", (int)result);
    }
    port_flush_complete(driver, result == LIOT_LCD_OK);
}

static int port_display_register(void)
{
    const uint32_t count = TIRTC_SCREEN_WIDTH * TIRTC_SCREEN_HEIGHT;

    /* Same 153,600-byte allocation; hardware landscape needs no transpose
     * scratch area, second framebuffer or LVGL rotation heap. */
    s_pixels = liot_rtos_malloc(count * sizeof(*s_pixels));
    if (s_pixels == NULL) {
        liot_trace("[tirtc] framebuffer allocation failed");
        return -1;
    }
    lv_disp_draw_buf_init(&s_draw_buffer, s_pixels, NULL, count);
    lv_disp_drv_init(&s_display_driver);
    s_display_driver.hor_res = TIRTC_SCREEN_WIDTH;
    s_display_driver.ver_res = TIRTC_SCREEN_HEIGHT;
    s_display_driver.rotated = LV_DISP_ROT_NONE;
    s_display_driver.sw_rotate = 0;
    /* LVGL 8.3.10 lv_refr.c:638-694 packs every dirty rectangle at buf_act;
     * :705-798 redraws it before flush. Neither mode may preserve old pixels. */
    s_display_driver.direct_mode = 0;
    s_display_driver.full_refresh = 0;
    s_display_driver.draw_buf = &s_draw_buffer;
    s_display_driver.flush_cb = port_flush;
    s_display_driver.rounder_cb = tirtc_lcd_rounder;
    if (lv_disp_drv_register(&s_display_driver) == NULL) {
        liot_trace("[tirtc] LVGL display registration failed");
        liot_rtos_free(s_pixels);
        s_pixels = NULL;
        return -1;
    }
    return 0;
}

static void port_touch_event(liot_tp_touch_data_t *data, void *context)
{
    uint16_t x, y;
    bool pressed;

    (void)context;
    if (data == NULL) return;
    if (data->touch_cnt == 0U) {
        s_touch_sample &= ~PORT_TOUCH_PRESSED;
        return;
    }
    /* LVGL pointer input tracks the first finger, never averages two touches. */
    if (!tirtc_touch_to_landscape(data->point[0].x, data->point[0].y,
                                  liot_st7789_dev.info.direction == LIOT_LCD_DIR_180_ANGLE,
                                  &x, &y)) {
        s_touch_sample &= ~PORT_TOUCH_PRESSED;
        return;
    }
    pressed = data->point[0].event == LIOT_TP_EVT_DOWN ||
              data->point[0].event == LIOT_TP_EVT_MOVE;
    s_touch_sample = (uint32_t)x | ((uint32_t)y << 16) |
                     (pressed ? PORT_TOUCH_PRESSED : 0U);
}

static void port_touch_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    uint32_t sample = s_touch_sample;

    (void)driver;
    data->point.x = (lv_coord_t)(sample & 0xFFFFU);
    data->point.y = (lv_coord_t)((sample >> 16) & 0x7FFFU);
    data->state = (sample & PORT_TOUCH_PRESSED) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->continue_reading = false;
}

static int port_touch_init(void)
{
    liot_tp_config_t cfg;
    uint8_t info[4] = {0};

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface_type = LIOT_TP_IF_I2C;
    cfg.i2c.num = (liot_i2c_channel_e)1;
    cfg.i2c.addr = 0x38U;
    cfg.i2c.sda = 66;
    cfg.i2c.scl = 57;
    cfg.i2c.sda_func = 2;
    cfg.i2c.scl_func = 3;
    cfg.rst.pin = 28;
    cfg.rst.delay_ms = 100U;
    cfg.rst.active_low = true;
    cfg.int_pin.pin = 6;
    cfg.int_pin.signal = L_INT_EDGE_FALL;
    cfg.int_pin.pull = LIOT_FORCE_PULL_UP;
    cfg.sensor = &g_liot_tp_ft6336;
    cfg.fw_auto_update = false;
    s_touch = liot_tp_init(&cfg);
    if (s_touch == NULL) {
        liot_trace("[tirtc] FT6336 init failed");
        return -1;
    }
    if (liot_tp_get_ic_info(s_touch, info, sizeof(info)) == LIOT_TP_SUCCESS) {
        liot_trace("[tirtc] FT6336 chip=%02x fw=%02x project=%02x", info[0], info[1], info[2]);
    }
    /* New UI receives native LVGL gestures; do not retain watch's global back swipe. */
    if (liot_tp_register_int_callback(s_touch, port_touch_event, NULL, NULL, NULL) != LIOT_TP_SUCCESS ||
        liot_tp_enable_int(s_touch, true) != LIOT_TP_SUCCESS) {
        liot_trace("[tirtc] FT6336 interrupt setup failed");
        return -1;
    }
    lv_indev_drv_init(&s_input_driver);
    s_input_driver.type = LV_INDEV_TYPE_POINTER;
    s_input_driver.read_cb = port_touch_read;
    s_input_driver.disp = lv_disp_get_default();
    if (lv_indev_drv_register(&s_input_driver) == NULL) {
        liot_trace("[tirtc] LVGL touch registration failed");
        return -1;
    }
    return 0;
}

static void port_tick(void *context)
{
    (void)context;
    lv_tick_inc(PORT_TICK_MS);
}

static void port_ui_task(void *context)
{
    int result;
    uint8_t applied_brightness;
    uint32_t heartbeat_tick;

    (void)context;
    liot_trace("[tirtc] init 1/6: LVGL");
    lv_init();
    liot_trace("[tirtc] init 2/6: shared 3.3V power");
    result = port_power_on();
    if (result == 0) {
        liot_trace("[tirtc] init 3/6: ST7789 LCD");
        result = port_lcd_init();
    }
    if (result == 0) {
        liot_trace("[tirtc] init 4/6: framebuffer and display");
        result = port_display_register();
    }
    if (result == 0) {
        liot_trace("[tirtc] init 5/6: FT6336 touch");
        result = port_touch_init();
    }
    if (result == 0) {
        if (liot_rtos_timer_create(&s_tick_timer, LIOT_TimerPeriodic, port_tick, NULL) != LIOT_OSI_SUCCESS ||
            liot_rtos_timer_start(s_tick_timer, PORT_TICK_MS) != LIOT_OSI_SUCCESS) {
            liot_trace("[tirtc] LVGL tick setup failed");
            result = -1;
        }
    }
    if (result == 0) {
        liot_trace("[tirtc] init 6/6: TiRTC home screen");
        tirtc_ui_init();
    }
    s_start_result = result;
    (void)liot_rtos_semaphore_release(s_ready_sem);
    if (result != 0) {
        if (s_tick_timer != NULL) (void)liot_rtos_timer_stop(s_tick_timer);
        if (s_touch != NULL) (void)liot_tp_deinit(s_touch);
        liot_trace("[tirtc] UI startup failed; inspect LCD/touch logs");
        (void)liot_rtos_task_delete(NULL);
        return;
    }
    /* UI init can request a different initial backlight state. */
    applied_brightness = 255U;
    heartbeat_tick = lv_tick_get();
    s_video_stats.tick = heartbeat_tick;
    tirtc_ui_video_stats(&s_video_stats.previous);
    liot_trace("[tirtc] UI ready: 320x240 hardware landscape / ST7789 RGB565 + FT6336");
    while (1) {
        uint8_t requested = s_requested_brightness;
        if (requested != applied_brightness) {
            if (liot_lcd_set_brightness(s_lcd, requested) == LIOT_LCD_OK) {
                applied_brightness = requested;
            }
        }
        /* All LVGL object operations, including business updates, run here. */
        uint32_t started = lv_tick_get();
        tirtc_ui_process();
        port_elapsed(&s_video_stats.process, started);
        started = lv_tick_get();
        (void)lv_timer_handler();
        port_elapsed(&s_video_stats.handler, started);
        ++s_video_stats.loops;
        port_video_report();
        if ((uint32_t)(lv_tick_get() - heartbeat_tick) >= 30000U) {
            heartbeat_tick = lv_tick_get();
            TIRTC_LOG_DEBUG("[tirtc] alive: uptime=%lu s page=%u", (unsigned long)(heartbeat_tick / 1000U),
                       (unsigned)tirtc_ui_current_page());
        }
        liot_rtos_task_sleep_ms(PORT_HANDLER_MS);
    }
}

int tirtc_port_start(void)
{
    if (s_ui_task != NULL) return s_start_result;
    liot_trace("[tirtc] startup: creating UI task and waiting for ready");
    /* The UI mailbox is ready before the LVGL task calls tirtc_ui_init(). */
    if (s_state_mutex == NULL &&
        liot_rtos_mutex_create(&s_state_mutex) != LIOT_OSI_SUCCESS) return -1;
    /* F6D_A liot_rtos_semaphore_delete first takes a token with an infinite
     * wait. The ready token is consumed below, so deleting this empty semaphore
     * would permanently block the app startup task. Keep this one-shot handle
     * for the application's lifetime, and reuse it if task creation fails. */
    if (s_ready_sem == NULL &&
        liot_rtos_semaphore_create(&s_ready_sem, 0U) != LIOT_OSI_SUCCESS) return -1;
    if (liot_rtos_task_create(&s_ui_task, PORT_TASK_STACK, PORT_TASK_PRIORITY,
                              "tirtc_ui", port_ui_task, NULL) != LIOT_OSI_SUCCESS) {
        return -1;
    }
    if (liot_rtos_semaphore_wait(s_ready_sem, LIOT_WAIT_FOREVER) != LIOT_OSI_SUCCESS) return -1;
    liot_trace("[tirtc] startup: ready received result=%d; F6D ready token consumed, semaphore retained", s_start_result);
    return s_start_result;
}
