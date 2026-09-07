#include "sc7a20h.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "lierda_log.h"
#include "pinmap.h"
#include "liot_i2c.h"
#include "audio.h"
#include "hardware.h"
#include "sc7a20h_service.h"
#include "fac_func.h"

#define SC7A20H_I2C_MODE        LIOT_STANDARD_MODE

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

/* ================= ISR 回调 (轻量化) ================= */

static void shake_timeout(void *arg)
{
    g_shake_timecb_running = true;
    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Shake detected! Count: %d, %d", g_flag_shake_count, g_shake_configs[g_shake_event].times);
    if (g_flag_shake_count >= g_shake_configs[g_shake_event].times) {
        if(is_fac_test_mode()) {
            if(g_is_fac_aging_test_end || g_is_sensor_debug) {
                audio_play_prompt_sound(AUDIO_PROMPT_CONNECTED, 1);
            }
        } else {
                if (device_get_sleep_status() == 1) { // 1 表示 DEVICE_MODE_SLEEP
                LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Device is in sleep mode, waking up...");
                device_sleep_enable(0);  // 唤醒设备
                osDelay(3000); // 等待系统稳定
            } else {
                key_press_msg_send(KEY_STATUS_PRESS, KEY_PRESS_HARDWARE);
            }

        }
    }
    g_flag_shake_count = 0;
    g_shake_timecb_running = false;
    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "shake_timeout callback end");
}

static void sensor_int1_callback(void *arg) {
    /* 只读取并清除状态，不做打印 */
    sc7a20h_int_status_t status;
    sc7a20h_get_int_status(&g_acc_dev, &status);

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "times: %d(%d), raw_status_aoi: %x", status.click_times, g_click_config.times, status.raw_status_aoi);
    if (status.click_times == g_click_config.times) {
        g_flag_click = true;
    }
}

static void sensor_int2_callback(void *arg) {
    sc7a20h_int_status_t status;
    sc7a20h_get_int_status(&g_acc_dev, &status);

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "high:%d, low:%d, raw_status_aoi:%x", status.shake_high, status.shake_low, status.raw_status_aoi);
    if (status.shake_high || status.shake_low) {
        g_flag_shake_count++;
        g_flag_shake = true;
    }
}

/**
 * @brief 初始化 I2C 总线 (应用层负责)
 */
static int32_t sc7a20h_sensor_i2c_init(void) {
    liot_gpioerr_e gpio_err;
    liot_errcode_i2c_e i2c_err;

    /* 1. 配置 GPIO 复用功能 */
    gpio_err = Liot_SetPinFunc(SC7A20H_I2C_SDA_PIN, SC7A20H_I2C1_SDA_FUNC);
    gpio_err |= Liot_SetPinFunc(SC7A20H_I2C_SCL_PIN, SC7A20H_I2C1_SCL_FUNC);
    if (gpio_err != L_GPIO_ERR_SUCCESS) {
        return -1;
    }

    /* 2. 初始化 I2C 控制器 */
    i2c_err = liot_I2cInit(liot_i2c_2, SC7A20H_I2C_MODE);
    if (i2c_err != LIOT_I2C_SUCCESS) {
        return -1;
    }

    liot_rtos_timer_create(&g_shake_timerId, LIOT_TimerOnce, shake_timeout, NULL);

    return 0;
}

/**
 * @brief 初始化 I2C 总线 (应用层负责)
 */
int32_t sc7a20h_sensor_i2c_deinit(void) {
    /* 2. 初始化 I2C 控制器 */
    if (liot_I2cRelease(liot_i2c_2) != LIOT_I2C_SUCCESS) {
        return -1;
    }

    if(!g_shake_timerId) {
        liot_rtos_timer_delete(g_shake_timerId);
    }

    return 0;
}

/* ================= 任务函数 ================= */
static void sc7a20h_sensor_task(void *arg) {
    sc7a20h_err_e err;
    sc7a20h_accel_data_t accel;

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Sensor Task Started\r\n");

    /* 0. 初始化 I2C 总线 */
    err = sc7a20h_sensor_i2c_init();
    if (err != 0) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, "I2C Init Failed: %d\r\n", err);
        return;
    }

    /* 1. 初始化驱动 */
    err = sc7a20h_init(&g_acc_dev, &g_sc7a20h_hal_liot);
    if (err != SC7A20H_OK) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, "Sensor Init Failed: %d\r\n", err);
        return;
    }

    if(is_fac_test_mode()) {
        sc7a20h_sensor_set_shake_event(SC7A20H_SHAKE_EVENT_DEBUG);
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
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, "Sensor Config Failed: %d\r\n", err);
        return;
    }

    /* 3. 注册 GPIO 中断 */
    liot_wakeup_cfg_t wk_cfg = {
        .wakeup_edge = L_INT_EDGE_RISE,
        .wakeup_pull = LIOT_FORCE_PULL_NONE
    };
    Liot_WakeupIntInit(G_INT1_WKUP, wk_cfg, sensor_int1_callback, NULL);
    Liot_WakeupIntInit(G_INT2_WKUP, wk_cfg, sensor_int2_callback, NULL);

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Sensor Config Complete\r\n");

    /* 4. 主循环 */
    while (1) {
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
        if (sc7a20h_read_accel(&g_acc_dev, &accel) == SC7A20H_OK) {
            /* 可在此添加滤波或姿态解算 */
            // LIOT_PRINTF(..., "Accel: X=%d, Y=%d, Z=%d\r\n", accel.x, accel.y, accel.z);
        }

        /* 处理 Click 事件 */
        if (g_flag_click) {
            g_flag_click = false;
            LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "g_click_config.enable: %d", g_click_config.enable);
            if(g_click_config.enable) {
                /* 执行点击业务逻辑 */
                if (is_fac_test_mode()) {
                    audio_play_prompt_sound(AUDIO_PROMPT_CONNECTING, 1);
                } else {
                    key_press_msg_send(KEY_STATUS_PRESS, KEY_PRESS_HARDWARE);
                }
            }
        }

        /* 处理 Shake 事件 */
        if (g_flag_shake) {
            g_flag_shake = false;
            LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "g_flag_shake_count: %d, g_shake_configs[%d].enable: %d", g_flag_shake_count, g_shake_event, g_shake_configs[g_shake_event].enable);
            if (g_shake_configs[g_shake_event].enable) {
                /* 执行摇晃业务逻辑, 此处添加g_shake_timecb_running， 主要处理，回调函数被触发时，liot_rtos_timer_is_running会检测到没有在工作，从而会新启动一个定时器 */
                if (!liot_rtos_timer_is_running(g_shake_timerId) && !g_shake_timecb_running) {
                    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Starting shake timer with timeout: %d ms", g_shake_configs[g_shake_event].timeout);
                    liot_rtos_timer_start(g_shake_timerId, g_shake_configs[g_shake_event].timeout);
                } else {
                    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "Shake timer already running");
                }
            } else {
                g_flag_shake_count = 0;
            }
        }
        osDelay(100);
    }
}

void sc7a20h_sensor_release(void)
{
    if (sc7a20h_sensorTaskRef != NULL) {
        if (sc7a20h_sensor_i2c_deinit() != 0) {
            LIOT_PRINTF(UNILOG_LIOT_OPEN, P_WARNING, "sc7a20h i2c deinit failed");
        }
        liot_rtos_task_delete(sc7a20h_sensorTaskRef);
        sc7a20h_sensorTaskRef = NULL;
    }

    g_flag_shake_count = 0;
    g_flag_shake = false;
    g_flag_click = false;

    Liot_WakeupIntDeinit(G_INT1_WKUP);
    Liot_WakeupIntDeinit(G_INT2_WKUP);

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "sc7a20h sensor released");
}

void sc7a20h_sensor_init(void)
{
    /* 使用 liot_rtos_task_create_static 创建任务 */
    if (sc7a20h_sensorTaskRef != NULL) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_WARNING, "Sensor task already running\r\n");
        sc7a20h_sensor_release(); // 确保之前的任务被删除
    }
    /* 栈大小建议 1024 以上，优先级根据系统调整 */
    LiotOSStatus_t ret = liot_rtos_task_create_static(
                            &sc7a20h_sensorTaskRef,
                            G_SENSOR_TASK_STACK_SIZE,
                            16+7,
                            "7a20h_sensor_task",
                            sc7a20h_sensor_task,
                            sc7a20h_sensorTaskStack,
                            &sc7a20h_sensorTask,
                            NULL);

    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "create sc7a20h_sensor_task: %d\r\n", ret);
}

sc7a20h_dev_t *get_sensor_device(void)
{
    return &g_acc_dev;
}

void sc7a20h_sensor_set_shake_state(bool enable)
{
    int i = 0;
    for(i = 0; i < SC7A20H_SHAKE_EVENT_MAX; i++)
    {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "set shake event: %d, enable:%d", i, enable);
        g_shake_configs[i].enable = enable;
    }

    if(0 == enable)
    {
        g_flag_shake_count = 0;
        g_flag_shake = false;
        if (liot_rtos_timer_is_running(g_shake_timerId)) {
            liot_rtos_timer_stop(g_shake_timerId);
        }
    }
}

void sc7a20h_sensor_set_shake_config(sc7a20h_shake_event_E event, const sc7a20h_shake_config_t *config)
{
    if (event >= SC7A20H_SHAKE_EVENT_MAX) {
        return;
    }

    if (config) {
        g_shake_configs[event].level = config->level;
        g_shake_configs[event].enable = config->enable;
        g_shake_configs[event].times = config->times;
        g_shake_configs[event].timeout = config->timeout;
    }
}

void sc7a20h_sensor_get_shake_config(sc7a20h_shake_event_E event, sc7a20h_shake_config_t *config)
{
    if (event >= SC7A20H_SHAKE_EVENT_MAX) {
        return;
    }

    if (config) {
        config->level = g_shake_configs[event].level;
        config->enable = g_shake_configs[event].enable;
        config->times = g_shake_configs[event].times;
        config->timeout = g_shake_configs[event].timeout;
    }
}

void sc7a20h_sensor_set_shake_event(sc7a20h_shake_event_E event)
{
    if (is_fac_test_mode()) {
        g_shake_event = SC7A20H_SHAKE_EVENT_DEBUG;
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "enter fac mode, level:%d, times:%d, timeout:%d", \
            event, g_shake_configs[g_shake_event].level, g_shake_configs[g_shake_event].times, g_shake_configs[g_shake_event].timeout);
    } else {
        g_shake_event = event;
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "set shake event: %d, level:%d, times:%d, timeout:%d", \
        event, g_shake_configs[event].level, g_shake_configs[event].times, g_shake_configs[event].timeout);
    }
}

void sc7a20h_sensor_set_click_state(bool enable)
{
    LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "set click enable:%d", enable);
    g_click_config.enable = enable;
    if(0 == enable)
    {
        g_flag_click = false;
    }
}

void sc7a20h_sensor_set_click_config(const sc7a20h_click_config_t *config)
{
    if (config) {
        g_click_config.level = config->level;
        g_click_config.enable = config->enable;
        if (config->times >= 1 && config->times <= 3) {
            g_click_config.times = config->times;
        } else {
            LIOT_PRINTF(UNILOG_LIOT_OPEN, P_WARNING, "Invalid click times: %d, should be 1-3\r\n", config->times);
        }
    }
}

void sc7a20h_sensor_get_click_config(sc7a20h_click_config_t *config)
{
    if (config) {
        config->level = g_click_config.level;
        config->enable = g_click_config.enable;
        config->times = g_click_config.times;
    }
}
