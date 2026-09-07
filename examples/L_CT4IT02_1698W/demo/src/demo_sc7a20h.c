#include <stdio.h>
#include <string.h>
#include "math.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_sleep.h"
#include "liot_tp.h"
#include "liot_gpio2.h"
#include "lvgl.h"

#include "lcd.h"
#include "sc7a20h.h"
#include "pinmap.h"

/* ================= 结构体定义 ================= */
/**
 * @brief 摇晃功能配置结构体
 */
typedef struct {
    uint8_t enable;     // 功能开关 (0 关闭，1 开启)
    uint8_t level;      // 灵敏度阈值级别 (0-128)，实际阈值为 level * 32 mg
    uint8_t times;      // 摇晃次数阈值
    uint32_t timeout;   // 摇晃超时时间 (ms)
} sc7a20h_shake_config_t;

/**
 * @brief 点击功能配置结构体
 */
typedef struct {
    uint8_t enable;     // 功能开关 (0 关闭，1 开启)
    uint8_t level;      // 灵敏度阈值级别 (0-128)，实际阈值为 level * 32 mg
    uint8_t times;      // 点击次数 (1-3)
} sc7a20h_click_config_t;

/* 此处定义一个枚举类型，用于表示摇晃唤醒 摇晃打断事件 */
typedef enum {
    SC7A20H_SHAKE_WAKEUP = 0,
    SC7A20H_SHAKE_INTERRUPT,
    SC7A20H_SHAKE_EVENT_DEBUG,
    SC7A20H_SHAKE_EVENT_MAX
} sc7a20h_shake_event_E;

/* =============== 任务相关 =============== */
#define G_SENSOR_TASK_STACK_SIZE 1024

static uint8_t sc7a20h_sensorTaskStack[G_SENSOR_TASK_STACK_SIZE];
static liot_task_t sc7a20h_sensorTaskRef = NULL;
static liot_StaticTask_t sc7a20h_sensorTask;
static liot_timer_t g_shake_timerId = NULL;
static sc7a20h_shake_event_E g_shake_event = SC7A20H_SHAKE_INTERRUPT;
static sc7a20h_shake_config_t g_shake_configs[SC7A20H_SHAKE_EVENT_MAX] = {
    {.level = 40, .enable = 1, .times = 4, .timeout = 700},
    {.level = 25, .enable = 1, .times = 3, .timeout = 500},
    {.level = 20, .enable = 1, .times = 2, .timeout = 1000}
};
static sc7a20h_click_config_t g_click_config = {
    .level = 108, .enable = 0, .times = 2
};
/* =============== 外部引用 HAL =============== */
extern const sc7a20h_hal_t g_sc7a20h_hal_liot;
extern bool g_is_fac_aging_test_end;
extern bool g_is_sensor_debug;
/* ================= 设备实例 ================= */
static sc7a20h_dev_t g_acc_dev;

/* ================= 中断标志 ================= */
static volatile int g_flag_shake_count = 0;
static volatile bool g_flag_shake = false;
static volatile bool g_flag_click = false;
static volatile bool g_shake_timecb_running = false;
static volatile int16_t g_accel_x = 0;
static volatile int16_t g_accel_y = 0;
static volatile int16_t g_accel_z = 0;

#define SC7A20H_SCREEN_W 360
#define SC7A20H_SCREEN_H 360
#define SC7A20H_CENTER_X (SC7A20H_SCREEN_W / 2)
#define SC7A20H_CENTER_Y (SC7A20H_SCREEN_H / 2)
#define SC7A20H_DOT_SIZE 18
#define SC7A20H_MOVE_RADIUS 130
#define SC7A20H_MAP_RANGE_MG 500

static lv_obj_t *g_dot_obj = NULL;
static lv_obj_t *g_accel_label = NULL;

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

static void sc7a20h_lvgl_update_cb(lv_timer_t *timer)
{
    static int filtered_x = SC7A20H_CENTER_X;
    static int filtered_y = SC7A20H_CENTER_Y;
    int16_t accel_x = g_accel_x;
    int16_t accel_y = g_accel_y;
    int16_t accel_z = g_accel_z;
    int target_x;
    int target_y;

    (void)timer;

    target_x = SC7A20H_CENTER_X + accel_x * SC7A20H_MOVE_RADIUS / SC7A20H_MAP_RANGE_MG;
    target_y = SC7A20H_CENTER_Y + accel_y * SC7A20H_MOVE_RADIUS / SC7A20H_MAP_RANGE_MG;
    target_x = sc7a20h_clamp_int(target_x, SC7A20H_DOT_SIZE / 2, SC7A20H_SCREEN_W - SC7A20H_DOT_SIZE / 2);
    target_y = sc7a20h_clamp_int(target_y, SC7A20H_DOT_SIZE / 2, SC7A20H_SCREEN_H - SC7A20H_DOT_SIZE / 2);

    filtered_x = (filtered_x * 3 + target_x) / 4;
    filtered_y = (filtered_y * 3 + target_y) / 4;

    if (g_dot_obj) {
        lv_obj_set_pos(g_dot_obj, filtered_x - SC7A20H_DOT_SIZE / 2, filtered_y - SC7A20H_DOT_SIZE / 2);
    }
    if (g_accel_label) {
        lv_label_set_text_fmt(g_accel_label, "X:%d Y:%d Z:%d", accel_x, accel_y, accel_z);
    }
}

static void sc7a20h_lvgl_setup_cb(void *arg)
{
    lv_obj_t *screen;
    lv_obj_t *line_x;
    lv_obj_t *line_y;
    static lv_point_t horizontal_line[] = {{20, SC7A20H_CENTER_Y}, {SC7A20H_SCREEN_W - 20, SC7A20H_CENTER_Y}};
    static lv_point_t vertical_line[] = {{SC7A20H_CENTER_X, 20}, {SC7A20H_CENTER_X, SC7A20H_SCREEN_H - 20}};
    static lv_style_t line_style;
    static lv_style_t dot_style;

    (void)arg;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_style_init(&line_style);
    lv_style_set_line_width(&line_style, 2);
    lv_style_set_line_color(&line_style, lv_color_white());
    lv_style_set_line_rounded(&line_style, true);

    line_x = lv_line_create(screen);
    lv_line_set_points(line_x, horizontal_line, 2);
    lv_obj_add_style(line_x, &line_style, 0);

    line_y = lv_line_create(screen);
    lv_line_set_points(line_y, vertical_line, 2);
    lv_obj_add_style(line_y, &line_style, 0);

    lv_style_init(&dot_style);
    lv_style_set_radius(&dot_style, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&dot_style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&dot_style, LV_OPA_COVER);

    g_dot_obj = lv_obj_create(screen);
    lv_obj_remove_style_all(g_dot_obj);
    lv_obj_add_style(g_dot_obj, &dot_style, 0);
    lv_obj_set_size(g_dot_obj, SC7A20H_DOT_SIZE, SC7A20H_DOT_SIZE);
    lv_obj_set_pos(g_dot_obj, SC7A20H_CENTER_X - SC7A20H_DOT_SIZE / 2, SC7A20H_CENTER_Y - SC7A20H_DOT_SIZE / 2);

    g_accel_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_accel_label, lv_color_white(), 0);
    lv_obj_align(g_accel_label, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_label_set_text(g_accel_label, "X:0 Y:0 Z:0");

    lv_scr_load(screen);
    lv_timer_create(sc7a20h_lvgl_update_cb, 30, NULL);
}

static void sc7a20h_lvgl_start(void)
{
    lvgl_init();
    lv_async_call(sc7a20h_lvgl_setup_cb, NULL);
}
/* ================= ISR 回调 (轻量化) ================= */

static void shake_timeout(void *arg)
{
    g_shake_timecb_running = true;
    liot_trace("Shake detected! Count: %d, %d", g_flag_shake_count, g_shake_configs[g_shake_event].times);
    if (g_flag_shake_count >= g_shake_configs[g_shake_event].times) 
    {
        // ...
    }
    g_flag_shake_count = 0;
    g_shake_timecb_running = false;
    liot_trace("shake_timeout callback end");
}

static void sensor_int1_callback(void *arg) {
    /* 只读取并清除状态，不做打印 */
    sc7a20h_int_status_t status;
    sc7a20h_get_int_status(&g_acc_dev, &status);

    liot_trace("times: %d(%d), raw_status_aoi: %x", status.click_times, g_click_config.times, status.raw_status_aoi);
    if (status.click_times == g_click_config.times) {
        g_flag_click = true;
    }
}

static void sensor_int2_callback(void *arg) {
    sc7a20h_int_status_t status;
    sc7a20h_get_int_status(&g_acc_dev, &status);

    liot_trace("high:%d, low:%d, raw_status_aoi:%x", status.shake_high, status.shake_low, status.raw_status_aoi);
    if (status.shake_high || status.shake_low) {
        g_flag_shake_count++;
        g_flag_shake = true;
    }
}


void liot_sc7a20h_demo_thread(void *argv)
{
    sc7a20h_err_e err;
    sc7a20h_accel_data_t accel;
    liot_gpioerr_e gpio_err;
    liot_errcode_i2c_e i2c_err;

    osDelay(5000);

    sc7a20h_lvgl_start();

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    //LDO Ctrl
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    /* 0. 初始化 I2C 总线 */
    /* 1. 配置 GPIO 复用功能 */
    gpio_err = Liot_SetPinFunc(SC7A20H_I2C_SDA_PIN, SC7A20H_I2C1_SDA_FUNC);
    gpio_err |= Liot_SetPinFunc(SC7A20H_I2C_SCL_PIN, SC7A20H_I2C1_SCL_FUNC);
    if (gpio_err != L_GPIO_ERR_SUCCESS) {
        liot_trace("GPIO config failed: %d\r\n", gpio_err);
        osDelay(1000);
        liot_rtos_task_delete(NULL);
    }

    /* 2. 初始化 I2C 控制器 */
    i2c_err = liot_I2cInit(0, LIOT_STANDARD_MODE);
    if (i2c_err != LIOT_I2C_SUCCESS) {
        liot_trace("I2C init failed: %d\r\n", i2c_err);
        osDelay(1000);
        liot_rtos_task_delete(NULL);
    }

    /* 1. 初始化驱动 */
    err = sc7a20h_init(&g_acc_dev, &g_sc7a20h_hal_liot);
    if (err != SC7A20H_OK) {
        liot_trace("Sensor Init Failed: %d\r\n", err);
        osDelay(1000);
        liot_rtos_task_delete(NULL);
    }

    /* 2. 配置完整参数 */
    sc7a20h_config_t cfg = {0};
    cfg.range = SC7A20H_RANGE_4G;
    cfg.odr = SC7A20H_ODR_400HZ;
    cfg.mode = SC7A20H_MODE_HI_PERF;
    cfg.high_pass_filter = true;

    /* FIFO 配置 */
    cfg.fifo.enable = true;
    cfg.fifo.watermark = 16;

    /* Shake 配置 - 阈值 500mg, 持续 200ms, INT2 输出 */
    cfg.shake.enable = true;
    cfg.shake.int_pin = SC7A20H_INT_PIN_2;
    cfg.shake.threshold_mg = g_shake_configs[g_shake_event].level * 32;
    cfg.shake.duration_ms = 20;
    cfg.shake.aoi_and_mode = false;
    cfg.shake.detect_mode = true;
    cfg.shake.axis_x_high = true;
    cfg.shake.axis_y_high = true;
    cfg.shake.axis_z_high = true;
    cfg.shake.axis_x_low = true;
    cfg.shake.axis_y_low = true;
    cfg.shake.axis_z_low = true;

    /* Click 配置 - 多击次数 2次，阈值 300mg, INT1 输出 */
    cfg.click.enable = true;
    cfg.click.int_pin = SC7A20H_INT_PIN_1;
    cfg.click.mode = SC7A20H_CLICK_DOUBLE;
    cfg.click.threshold_mg = 3456;  // 32 * 6c = 3456mg
    cfg.click.axis_x = true;
    cfg.click.axis_y = true;
    cfg.click.axis_z = true;
    cfg.click.time_limit = 0xB2;
    cfg.click.time_latency = 0x04;
    cfg.click.time_window = 0x3;  //单击时间窗口 实际时长=(设置值*64+20)/ODR；

    err = sc7a20h_apply_config(&g_acc_dev, &cfg);
    if (err != SC7A20H_OK) {
        liot_trace("Sensor Config Failed: %d\r\n", err);
        osDelay(1000);
        liot_rtos_task_delete(NULL);
    }

    /* 3. 注册 GPIO 中断 */
    liot_wakeup_cfg_t wk_cfg = {
        .wakeup_edge = L_INT_EDGE_RISE,
        .wakeup_pull = LIOT_FORCE_PULL_NONE
    };
    Liot_WakeupIntInit(G_INT1_WKUP, wk_cfg, sensor_int1_callback, NULL);
    Liot_WakeupIntInit(G_INT2_WKUP, wk_cfg, sensor_int2_callback, NULL);

    liot_trace("Sensor Config Complete\r\n");

    /* 4. 主循环 */
    while (1) 
    {
        // 检查配置参数，如果应用层修改了全局阈值变量，则更新驱动配置（仅做测试后期删除）
        /* 5. 配置 Click 功能 */
        if (cfg.click.threshold_mg != g_click_config.level * 32) {
            cfg.click.threshold_mg = g_click_config.level * 32;
            sc7a20h_config_click(&g_acc_dev, &cfg.click);
        }

        /* 6. 配置 Shake 功能 */
        if (cfg.shake.threshold_mg != g_shake_configs[g_shake_event].level * 32) {
            cfg.shake.threshold_mg = g_shake_configs[g_shake_event].level * 32;
            sc7a20h_config_shake(&g_acc_dev, &cfg.shake);
        }

        /* 读取加速度数据 */
        if (sc7a20h_read_accel(&g_acc_dev, &accel) == SC7A20H_OK) 
        {
            g_accel_x = accel.x;
            g_accel_y = accel.y;
            g_accel_z = accel.z;
            /* 可在此添加滤波或姿态解算 */
            liot_trace("Accel: X=%d, Y=%d, Z=%d\r\n", accel.x, accel.y, accel.z);
        }

        /* 处理 Click 事件 */
        if (g_flag_click) 
        {
            g_flag_click = false;
            liot_trace("g_click_config.enable: %d", g_click_config.enable);
            if(g_click_config.enable) 
            {
                /* 执行单击业务逻辑 */
                liot_trace("Click detected");
            }
        }

        /* 处理 Shake 事件 */
        if (g_flag_shake) 
        {
            g_flag_shake = false;
            liot_trace("g_flag_shake_count: %d, g_shake_configs[%d].enable: %d", g_flag_shake_count, g_shake_event, g_shake_configs[g_shake_event].enable);
            if (g_shake_configs[g_shake_event].enable) 
            {

            } 
            else 
            {
                g_flag_shake_count = 0;
            }
        }
        osDelay(10);
    }

}