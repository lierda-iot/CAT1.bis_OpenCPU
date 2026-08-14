/**
 * @File Name: liot_tp_demo.c
 * @brief Touch Panel demo based on liot_tp framework
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "liot_os.h"
#include "liot_tp.h"
#include "lierda_app_main.h"
#include "liot_sleep.h"

/* I2C地址扫描: TP调通后可改为0关闭 */
#define TP_I2C_SCAN_ENABLE      0
#define TP_I2C_SCAN_START_ADDR  0x03
#define TP_I2C_SCAN_END_ADDR    0x77
#define TP_PIN_VALID(pin)       ((pin) >= 0 && (pin) != 255)

/* 传感器选择: 将要使用的设为1, 其余设为0 */
#define LIOT_TP_DEMO_TEST_AXS5106 0   /* AXS5106 触摸屏 */
#define LIOT_TP_DEMO_TEST_CST816D 0   /* CST816D 触摸屏 */
#define LIOT_TP_DEMO_TEST_FT6336  1   /* FT6336 触摸屏 */

/* ================= 硬件引脚配置 (根据实际硬件修改) ================= */
#if LIOT_TP_DEMO_TEST_CST816D == 1
/* I2C总线 */
#define TP_I2C_NUM          1               /* I2C通道号 */
#define TP_I2C_ADDR         0x15            /* I2C从机地址(7bit) */
#define TP_SDA_PIN          66              /* SDA引脚号 */
#define TP_SCL_PIN          67              /* SCL引脚号 */
#define TP_SCL_FUNC         L_PIN_FUNC_2    /* SCL pin mux func */
#define TP_SDA_FUNC         L_PIN_FUNC_2    /* SDA pin mux func */

/* RST复位 */
#define TP_RST_PIN          20              /* RST引脚号 */
#define TP_RST_DELAY_MS     100             /* 复位后延时(ms) */
#define TP_RST_ACTIVE_LOW   1               /* 1=低电平复位, 0=高电平复位 */

/* INT中断 */
#define TP_INT_PIN          19              /* INT引脚号 */

/* 固件自动升级 */
#define TP_FW_AUTO_UPDATE   1               /* 1=init时自动升级固件, 0=跳过自动升级 */

#elif LIOT_TP_DEMO_TEST_AXS5106 == 1
/* I2C总线 */
#define TP_I2C_NUM          0               /* I2C通道号 */
#define TP_I2C_ADDR         0x63            /* I2C从机地址(7bit) */
#define TP_SDA_PIN          62              /* SDA引脚号 */
#define TP_SCL_PIN          49              /* SCL引脚号 */
#define TP_SCL_FUNC         L_PIN_FUNC_2    /* SCL pin mux func */
#define TP_SDA_FUNC         L_PIN_FUNC_2    /* SDA pin mux func */

/* RST复位 */
#define TP_RST_PIN          56              /* RST引脚号 */
#define TP_RST_DELAY_MS     100             /* 复位后延时(ms) */
#define TP_RST_ACTIVE_LOW   1               /* 1=低电平复位, 0=高电平复位 */

/* INT中断 */
#define TP_INT_PIN          55              /* INT引脚号 */

/* 固件自动升级 */
#define TP_FW_AUTO_UPDATE   1               /* 1=init时自动升级固件, 0=跳过自动升级 */

#elif LIOT_TP_DEMO_TEST_FT6336 == 1
/* I2C总线 */
#define TP_I2C_NUM          1               /* I2C通道号 */
#define TP_I2C_ADDR         0x38            /* I2C从机地址(7bit) */
#define TP_SDA_PIN          66              /* SDA引脚号 */
#define TP_SCL_PIN          57              /* SCL引脚号 */
#define TP_SCL_FUNC         L_PIN_FUNC_3    /* SCL pin mux func */
#define TP_SDA_FUNC         L_PIN_FUNC_2    /* SDA pin mux func */

/* RST复位 */
#define TP_RST_PIN          28              /* RST引脚号, 根据FT6336实物板修改 */
#define TP_RST_DELAY_MS     100             /* 复位后延时(ms) */
#define TP_RST_ACTIVE_LOW   1               /* 1=低电平复位, 0=高电平复位 */

/* INT中断 */
#define TP_INT_PIN          6              /* INT引脚号, 根据FT6336实物板修改 */

/* 固件自动升级 */
#define TP_FW_AUTO_UPDATE   0               /* FT6336暂未接入固件升级 */
#endif

/* ================= 传感器选择 ================= */
LIOT_ADD_TP_DEV(g_liot_tp_cst816d);
LIOT_ADD_TP_DEV(g_liot_tp_axs5106);
LIOT_ADD_TP_DEV(g_liot_tp_ft6336);

/* ================= 传感器选择宏 ================= */
#if LIOT_TP_DEMO_TEST_CST816D == 1
#define TP_SENSOR  g_liot_tp_cst816d
#elif LIOT_TP_DEMO_TEST_AXS5106 == 1
#define TP_SENSOR  g_liot_tp_axs5106
#elif LIOT_TP_DEMO_TEST_FT6336 == 1
#define TP_SENSOR  g_liot_tp_ft6336
#endif

/* ================= 辅助函数 ================= */

static const char *tp_event_str(liot_tp_event_e evt)
{
    switch (evt) {
    case LIOT_TP_EVT_DOWN:  return "DOWN";
    case LIOT_TP_EVT_UP:    return "UP";
    case LIOT_TP_EVT_MOVE:  return "MOVE";
    case LIOT_TP_EVT_NONE:  return "NONE";
    default:                return "???";
    }
}

static const char *tp_gesture_str(liot_tp_gesture_e gest)
{
    switch (gest) {
    case LIOT_TP_GESTURE_NONE:       return "NONE";
    case LIOT_TP_GESTURE_UP:        return "UP";
    case LIOT_TP_GESTURE_DOWN:      return "DOWN";
    case LIOT_TP_GESTURE_LEFT:      return "LEFT";
    case LIOT_TP_GESTURE_RIGHT:     return "RIGHT";
    case LIOT_TP_GESTURE_ZOOM_IN:   return "ZOOM_IN";
    case LIOT_TP_GESTURE_ZOOM_OUT:  return "ZOOM_OUT";
    case LIOT_TP_GESTURE_LONG_PRESS:return "LONG_PRESS";
    default:                         return "???";
    }
}

#if TP_I2C_SCAN_ENABLE
static void tp_i2c_scan(void)
{
    int found = 0;
    uint8_t val = 0;
    liot_errcode_i2c_e ret;

    liot_trace("[TP][I2C_SCAN] start: bus=%d, range=0x%02x-0x%02x",
               TP_I2C_NUM, TP_I2C_SCAN_START_ADDR, TP_I2C_SCAN_END_ADDR);

    if (TP_PIN_VALID(TP_SCL_PIN)) {
        Liot_SetPinFunc(TP_SCL_PIN, TP_SCL_FUNC);
    }
    if (TP_PIN_VALID(TP_SDA_PIN)) {
        Liot_SetPinFunc(TP_SDA_PIN, TP_SDA_FUNC);
    }

    ret = liot_I2cInit(TP_I2C_NUM, LIOT_STANDARD_MODE);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("[TP][I2C_SCAN] i2c init failed, ret=%d", ret);
        return;
    }

    for (uint8_t addr = TP_I2C_SCAN_START_ADDR; addr <= TP_I2C_SCAN_END_ADDR; addr++) {
        liot_trace("[TP][I2C_SCAN] try addr=0x%02x", addr);
        ret = liot_I2cRead(TP_I2C_NUM, addr, 0xA3, &val, 1);
        liot_trace("[TP][I2C_SCAN] addr=0x%02x ret=%d", addr, ret);
        //ret = liot_I2cRead(TP_I2C_NUM, 0x38, 0xA3, &val, 1);
        if (ret == LIOT_I2C_SUCCESS) {
            liot_trace("[TP][I2C_SCAN] found 7bit=0x%02x write8=0x%02x read8=0x%02x val=0x%02x",
                       addr, (addr << 1), ((addr << 1) | 1), val);
            found++;
        }
        osDelay(20);
    }

    //liot_I2cRelease(TP_I2C_NUM);
    liot_trace("[TP][I2C_SCAN] done, found=%d", found);
}
#endif

/* ================= 回调函数 ================= */

static void tp_touch_callback(liot_tp_touch_data_t *data, void *ctx)
{
    (void)ctx;
    for (uint8_t i = 0; i < data->touch_cnt; i++) {
        liot_trace("[TP] touch%d: evt=%s x=%d y=%d",
                         data->point[i].id,
                         tp_event_str(data->point[i].event),
                         data->point[i].x,
                         data->point[i].y);
    }
}

static void tp_gesture_callback(liot_tp_gesture_data_t *data, void *ctx)
{
    (void)ctx;
    liot_trace("[TP] gesture: %s @%lu",
                     tp_gesture_str(data->gesture), (unsigned long)data->timestamp_ms);
}

/* ================= Demo主线程 ================= */

void liot_tp_demo_thread(void *argv)
{
    (void)argv;
    osDelay(500);
    liot_trace("==== liot_tp demo start ====");

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL); // 3V3

#if TP_I2C_SCAN_ENABLE
    while (1) {
        tp_i2c_scan();
        osDelay(1000);
    }

#endif

    /* 1. 构造TP配置 */
    liot_tp_config_t tp_cfg = {
        .interface_type = LIOT_TP_IF_I2C,
        .i2c = {
            .num      = TP_I2C_NUM,
            .sda      = TP_SDA_PIN,
            .scl      = TP_SCL_PIN,
            .addr     = TP_I2C_ADDR,
            .scl_func = TP_SCL_FUNC,
            .sda_func = TP_SDA_FUNC,
        },
        .rst = {
            .pin        = TP_RST_PIN,
            .delay_ms   = TP_RST_DELAY_MS,
            .active_low = TP_RST_ACTIVE_LOW,
        },
        .int_pin = {
            .pin    = TP_INT_PIN,
            .signal = L_INT_EDGE_FALL,
            .pull   = LIOT_FORCE_PULL_UP,
        },
        .sensor = &TP_SENSOR,
        .fw_auto_update = TP_FW_AUTO_UPDATE,
    };

    /* 2. 初始化TP (失败则重试) */
    liot_tp_handle_t tp;
reinit:
    tp = liot_tp_init(&tp_cfg);
    if (!tp) {
        liot_trace("[TP] init failed!");
        osDelay(1000);
        goto reinit;
    }
    liot_trace("[TP] init ok");

    /* 3. 读取IC信息 */
    uint8_t ic_info[4] = {0};
    if (liot_tp_get_ic_info(tp, ic_info, sizeof(ic_info)) == LIOT_TP_SUCCESS) {
        liot_trace("[TP] IC info: chip_id=0x%x fw_ver=0x%x proj_id=0x%x lpm=0x%x",
                         ic_info[0], ic_info[1], ic_info[2], ic_info[3]);
    }

    /* 4. 注册触摸/手势回调 */
    liot_tp_register_int_callback(tp,
                                  tp_touch_callback,
                                  tp_gesture_callback,
                                  NULL, NULL);

    /* 5. 启动INT中断轮询 */
    liot_tp_enable_int(tp, true);
    liot_trace("[TP] INT enabled, waiting for touch...");

    while (1) {
        osDelay(1000);
    }
}
