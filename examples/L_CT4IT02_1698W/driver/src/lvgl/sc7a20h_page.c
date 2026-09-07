#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lvgl.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "pinmap.h"
#include "sc7a20h.h"
#include "sc7a20h_page.h"
#include "gui_guider.h"

#define SC7A20H_SCREEN_W 360
#define SC7A20H_SCREEN_H 360
#define SC7A20H_CENTER_X (SC7A20H_SCREEN_W / 2)
#define SC7A20H_CENTER_Y (SC7A20H_SCREEN_H / 2)
#define SC7A20H_DOT_SIZE 18
#define SC7A20H_MOVE_RADIUS 130
#define SC7A20H_MAP_RANGE_MG 500
#define SC7A20H_REENTER_BLOCK_MS 2000

extern const sc7a20h_hal_t g_sc7a20h_hal_liot;

static sc7a20h_dev_t g_sc7a20h_page_dev;
static liot_task_t g_sc7a20h_page_task = NULL;
static lv_obj_t *g_sc7a20h_page = NULL;
static lv_obj_t *g_sc7a20h_dot = NULL;
static lv_obj_t *g_sc7a20h_label = NULL;
static lv_timer_t *g_sc7a20h_timer = NULL;
static bool g_sc7a20h_page_del = false;
static uint32_t g_sc7a20h_last_exit_tick = 0;
static volatile bool g_sc7a20h_page_running = false;
static bool g_sc7a20h_page_exiting = false;
static volatile bool g_sc7a20h_sensor_ready = false;
static volatile int16_t g_sc7a20h_accel_x = 0;
static volatile int16_t g_sc7a20h_accel_y = 0;
static volatile int16_t g_sc7a20h_accel_z = 0;

static int sc7a20h_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool sc7a20h_page_sensor_init(void)
{
    liot_gpioerr_e gpio_err;
    liot_errcode_i2c_e i2c_err;
    sc7a20h_err_e err;
    sc7a20h_config_t cfg = {0};

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    gpio_err = Liot_SetPinFunc(SC7A20H_I2C_SDA_PIN, SC7A20H_I2C1_SDA_FUNC);
    gpio_err |= Liot_SetPinFunc(SC7A20H_I2C_SCL_PIN, SC7A20H_I2C1_SCL_FUNC);
    if (gpio_err != L_GPIO_ERR_SUCCESS) {
        liot_trace("SC7A20H page GPIO config failed: %d", gpio_err);
        return false;
    }

    i2c_err = liot_I2cInit(0, LIOT_STANDARD_MODE);
    if (i2c_err != LIOT_I2C_SUCCESS) {
        liot_trace("SC7A20H page I2C init failed: %d", i2c_err);
        return false;
    }

    err = sc7a20h_init(&g_sc7a20h_page_dev, &g_sc7a20h_hal_liot);
    if (err != SC7A20H_OK) {
        liot_trace("SC7A20H page sensor init failed: %d", err);
        return false;
    }

    cfg.range = SC7A20H_RANGE_4G;
    cfg.odr = SC7A20H_ODR_100HZ;
    cfg.mode = SC7A20H_MODE_HI_PERF;
    cfg.high_pass_filter = false;

    err = sc7a20h_apply_config(&g_sc7a20h_page_dev, &cfg);
    if (err != SC7A20H_OK) {
        liot_trace("SC7A20H page sensor config failed: %d", err);
        return false;
    }

    return true;
}

static void sc7a20h_page_sensor_task(void *argv)
{
    sc7a20h_accel_data_t accel;

    (void)argv;

    g_sc7a20h_sensor_ready = sc7a20h_page_sensor_init();
    while (g_sc7a20h_page_running) {
        if (g_sc7a20h_sensor_ready && sc7a20h_read_accel(&g_sc7a20h_page_dev, &accel) == SC7A20H_OK) {
            g_sc7a20h_accel_x = accel.x;
            g_sc7a20h_accel_y = accel.y;
            g_sc7a20h_accel_z = accel.z;
        }
        liot_rtos_task_sleep_ms(20);
    }

    g_sc7a20h_page_task = NULL;
    liot_rtos_task_delete(NULL);
}

static void sc7a20h_page_update_cb(lv_timer_t *timer)
{
    static int filtered_x = SC7A20H_CENTER_X;
    static int filtered_y = SC7A20H_CENTER_Y;
    int16_t accel_x = g_sc7a20h_accel_x;
    int16_t accel_y = g_sc7a20h_accel_y;
    int16_t accel_z = g_sc7a20h_accel_z;
    int target_x;
    int target_y;

    (void)timer;

    target_x = SC7A20H_CENTER_X + accel_x * SC7A20H_MOVE_RADIUS / SC7A20H_MAP_RANGE_MG;
    target_y = SC7A20H_CENTER_Y + accel_y * SC7A20H_MOVE_RADIUS / SC7A20H_MAP_RANGE_MG;
    target_x = sc7a20h_clamp_int(target_x, SC7A20H_DOT_SIZE / 2, SC7A20H_SCREEN_W - SC7A20H_DOT_SIZE / 2);
    target_y = sc7a20h_clamp_int(target_y, SC7A20H_DOT_SIZE / 2, SC7A20H_SCREEN_H - SC7A20H_DOT_SIZE / 2);

    filtered_x = (filtered_x * 3 + target_x) / 4;
    filtered_y = (filtered_y * 3 + target_y) / 4;

    if (g_sc7a20h_dot) {
        lv_obj_set_pos(g_sc7a20h_dot, filtered_x - SC7A20H_DOT_SIZE / 2, filtered_y - SC7A20H_DOT_SIZE / 2);
    }
    if (g_sc7a20h_label) {
        lv_label_set_text_fmt(g_sc7a20h_label, "X:%d Y:%d Z:%d", accel_x, accel_y, accel_z);
    }
}

static void sc7a20h_page_exit(void)
{
    g_sc7a20h_page_running = false;
    g_sc7a20h_last_exit_tick = lv_tick_get();

    if (g_sc7a20h_timer) {
        lv_timer_del(g_sc7a20h_timer);
        g_sc7a20h_timer = NULL;
    }

    ui_load_scr_animation(&guider_ui,
                          &guider_ui.attuition,
                          guider_ui.attuition_del,
                          &g_sc7a20h_page_del,
                          setup_scr_attuition,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          0,
                          false,
                          true);
}

static void sc7a20h_page_exit_async_cb(void *arg)
{
    (void)arg;
    sc7a20h_page_exit();
}

static void sc7a20h_page_exit_request(void)
{
    if (g_sc7a20h_page_exiting) {
        return;
    }
    g_sc7a20h_page_exiting = true;
    lv_async_call(sc7a20h_page_exit_async_cb, NULL);
}

static void sc7a20h_page_event_handler(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED) {
        lv_event_stop_bubbling(event);
        lv_event_stop_processing(event);
        sc7a20h_page_exit_request();
    }
}

void sc7a20h_page_enter(void)
{
    lv_obj_t *line_x;
    lv_obj_t *line_y;
    static lv_point_t horizontal_line[] = {{20, SC7A20H_CENTER_Y}, {SC7A20H_SCREEN_W - 20, SC7A20H_CENTER_Y}};
    static lv_point_t vertical_line[] = {{SC7A20H_CENTER_X, 20}, {SC7A20H_CENTER_X, SC7A20H_SCREEN_H - 20}};
    static lv_style_t line_style;
    static lv_style_t dot_style;
    static bool style_ready = false;

    if (g_sc7a20h_page_running) {
        return;
    }
    if (g_sc7a20h_last_exit_tick != 0 && lv_tick_elaps(g_sc7a20h_last_exit_tick) < SC7A20H_REENTER_BLOCK_MS) {
        return;
    }

    g_sc7a20h_page_exiting = false;
    g_sc7a20h_page_running = true;
    g_sc7a20h_sensor_ready = false;
    g_sc7a20h_accel_x = 0;
    g_sc7a20h_accel_y = 0;
    g_sc7a20h_accel_z = 0;

    if (!style_ready) {
        lv_style_init(&line_style);
        lv_style_set_line_width(&line_style, 2);
        lv_style_set_line_color(&line_style, lv_color_white());
        lv_style_set_line_rounded(&line_style, true);

        lv_style_init(&dot_style);
        lv_style_set_radius(&dot_style, LV_RADIUS_CIRCLE);
        lv_style_set_bg_color(&dot_style, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_bg_opa(&dot_style, LV_OPA_COVER);
        style_ready = true;
    }

    g_sc7a20h_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_sc7a20h_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_sc7a20h_page, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(g_sc7a20h_page, sc7a20h_page_event_handler, LV_EVENT_ALL, NULL);

    line_x = lv_line_create(g_sc7a20h_page);
    lv_line_set_points(line_x, horizontal_line, 2);
    lv_obj_add_style(line_x, &line_style, 0);

    line_y = lv_line_create(g_sc7a20h_page);
    lv_line_set_points(line_y, vertical_line, 2);
    lv_obj_add_style(line_y, &line_style, 0);

    g_sc7a20h_dot = lv_obj_create(g_sc7a20h_page);
    lv_obj_remove_style_all(g_sc7a20h_dot);
    lv_obj_add_style(g_sc7a20h_dot, &dot_style, 0);
    lv_obj_set_size(g_sc7a20h_dot, SC7A20H_DOT_SIZE, SC7A20H_DOT_SIZE);
    lv_obj_set_pos(g_sc7a20h_dot, SC7A20H_CENTER_X - SC7A20H_DOT_SIZE / 2, SC7A20H_CENTER_Y - SC7A20H_DOT_SIZE / 2);

    g_sc7a20h_label = lv_label_create(g_sc7a20h_page);
    lv_obj_set_style_text_color(g_sc7a20h_label, lv_color_white(), 0);
    lv_obj_align(g_sc7a20h_label, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_label_set_text(g_sc7a20h_label, "X:0 Y:0 Z:0");

    g_sc7a20h_timer = lv_timer_create(sc7a20h_page_update_cb, 30, NULL);

    liot_rtos_task_create(&g_sc7a20h_page_task,
                          10240,
                          LIOT_APP_TASK_PRIORITY,
                          "sc7a20h_page_sensor",
                          sc7a20h_page_sensor_task,
                          NULL);

    lv_scr_load_anim(g_sc7a20h_page, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    guider_ui.attuition_del = true;
    g_sc7a20h_page_del = false;
}

static void sc7a20h_page_enter_async_cb(void *arg)
{
    (void)arg;
    sc7a20h_page_enter();
}

void sc7a20h_page_enter_async(void)
{
    lv_async_call(sc7a20h_page_enter_async_cb, NULL);
}