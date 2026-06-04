/**
 * @File Name: liot_tp.h
 * @brief Touch Panel API
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.1
 */

#ifndef __LIOT_TP_H__
#define __LIOT_TP_H__

#include "liot_type.h"
#include "liot_api_common.h"
#include "liot_i2c.h"
#include "liot_spi.h"
#include "liot_gpio2.h"
#include "cmsis_os2.h"

#define LIOT_ADD_TP_DEV(tpDev)    extern liot_tp_sensor_t tpDev

typedef void *liot_tp_handle_t;

/* ================= 错误码 ================= */
typedef enum {
    LIOT_TP_SUCCESS = LIOT_SUCCESS,
    LIOT_TP_ERR_INIT = (0x10) | LIOT_COMPONENT_STATE_INFO,
    LIOT_TP_ERR_BUS,
    LIOT_TP_ERR_PARAM,
    LIOT_TP_ERR_HANDLE,
    LIOT_TP_ERR_TIMEOUT,
    LIOT_TP_ERR_NOT_SUPPORTED,
} liot_errcode_tp_e;

/* ================= 触摸事件类型 ================= */
typedef enum {
    LIOT_TP_EVT_DOWN = 0,
    LIOT_TP_EVT_UP,
    LIOT_TP_EVT_MOVE,
    LIOT_TP_EVT_NONE,
} liot_tp_event_e;

/* ================= 手势类型 ================= */
typedef enum {
    LIOT_TP_GESTURE_NONE = 0,
    LIOT_TP_GESTURE_UP,
    LIOT_TP_GESTURE_DOWN,
    LIOT_TP_GESTURE_LEFT,
    LIOT_TP_GESTURE_RIGHT,
    LIOT_TP_GESTURE_ZOOM_IN,
    LIOT_TP_GESTURE_ZOOM_OUT,
    LIOT_TP_GESTURE_LONG_PRESS,
} liot_tp_gesture_e;

/* ================= 接口类型 ================= */
typedef enum {
    LIOT_TP_IF_I2C = 0,
    LIOT_TP_IF_SPI,
} liot_tp_interface_e;

/* ================= 触摸点数据 ================= */
typedef struct {
    uint8_t  id;
    liot_tp_event_e event;
    uint16_t x;
    uint16_t y;
    uint8_t  weight;
    uint32_t timestamp_ms;
} liot_tp_point_t;

/* ================= 触摸数据 ================= */
typedef struct {
    uint8_t        touch_cnt;
    liot_tp_point_t point[5];
} liot_tp_touch_data_t;

/* ================= 手势数据 ================= */
typedef struct {
    liot_tp_gesture_e gesture;
    uint32_t timestamp_ms;
} liot_tp_gesture_data_t;

/* ================= 工作模式 ================= */
typedef enum {
    LIOT_TP_MODE_NORMAL = 0,
    LIOT_TP_MODE_GESTURE,
    LIOT_TP_MODE_LOW_POWER,
    LIOT_TP_MODE_DEEP_SLEEP,
    LIOT_TP_MODE_FACTORY_TEST,
} liot_tp_work_mode_e;

/* ================= 传感器设备接口 ================= */
typedef struct {
    int (*init)(liot_tp_handle_t handle);
    int (*deinit)(liot_tp_handle_t handle);
    int (*read_touch)(liot_tp_handle_t handle, liot_tp_touch_data_t *data);
    int (*read_gesture)(liot_tp_handle_t handle, liot_tp_gesture_data_t *data);
    int (*set_threshold)(liot_tp_handle_t handle, uint8_t threshold);
    int (*enter_sleep)(liot_tp_handle_t handle);
    int (*wakeup)(liot_tp_handle_t handle);
    int (*reset)(liot_tp_handle_t handle);
    int (*exit_sleep)(liot_tp_handle_t handle);
    int (*update_firmware)(liot_tp_handle_t handle, const uint8_t *fw_data, uint32_t fw_len);
    int (*update_firmware_auto)(liot_tp_handle_t handle);
    int (*set_work_mode)(liot_tp_handle_t handle, liot_tp_work_mode_e mode);
    int (*get_ic_info)(liot_tp_handle_t handle, uint8_t *buf, uint16_t buf_len);
} liot_tp_sensor_func_t;

/* ================= 传感器设备定义 ================= */
typedef struct {
    uint8_t  chip_id;
    uint8_t  max_points;
    uint16_t width;
    uint16_t height;
    bool     gesture_support;
    liot_tp_sensor_func_t func;
} liot_tp_sensor_t;

/* ================= I2C 接口配置 ================= */
typedef struct {
    liot_i2c_channel_e num;
    int8_t sda;
    int8_t scl;
    uint8_t addr;
    uint8_t scl_func;   /* SCL pin mux function, 0 = use default (L_PIN_FUNC_2) */
    uint8_t sda_func;   /* SDA pin mux function, 0 = use default (L_PIN_FUNC_2) */
} liot_tp_i2c_config_t;

/* ================= SPI 接口配置 ================= */
typedef struct {
    liot_spi_port_e num;
    liot_spi_cpol_pol_e cpol;
    liot_spi_cpha_pol_e cpha;
    liot_spi_clk_e speed;
} liot_tp_spi_config_t;

/* ================= 复位引脚配置 ================= */
typedef struct {
    int8_t pin;
    uint16_t delay_ms;
    bool active_low;
} liot_tp_rst_config_t;

/* ================= 中断引脚配置 ================= */
typedef struct {
    int8_t pin;
    liot_intsig_e signal;               /* 触发方式: L_INT_EDGE_FALL / L_INT_EDGE_RISE / L_INT_EDGE_BOTH 等 */
    liot_gpio_pull_mode_e pull;         /* 上下拉: LIOT_FORCE_PULL_UP 等 */
} liot_tp_int_config_t;

/* ================= TP 完整配置 ================= */
typedef struct {
    liot_tp_interface_e interface_type;

    union {
        liot_tp_i2c_config_t i2c;
        liot_tp_spi_config_t spi;
    };

    liot_tp_rst_config_t rst;
    liot_tp_int_config_t int_pin;
    liot_tp_sensor_t *sensor;
    bool fw_auto_update;             /* true=init时自动升级固件(默认), false=跳过自动升级 */
} liot_tp_config_t;


typedef struct {
    liot_tp_config_t  cfg;
    bool              is_init;
    osEventFlagsId_t  evt_flags;
    osThreadId_t      poll_thread;
    osMutexId_t       bus_mutex;
    void             *touch_cb_ctx;
    void             *gesture_cb_ctx;
    void            (*touch_cb)(liot_tp_touch_data_t *data, void *ctx);
    void            (*gesture_cb)(liot_tp_gesture_data_t *data, void *ctx);
    bool              int_polling;
} liot_tp_dev_info_t;

/* ================= 核心 API ================= */

/**
 * @brief TP初始化
 * @param config TP配置参数
 * @return TP句柄, NULL失败
 */
liot_tp_handle_t liot_tp_init(liot_tp_config_t *config);

/**
 * @brief TP反初始化
 * @param handle TP句柄
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_deinit(liot_tp_handle_t handle);

/**
 * @brief 读取触摸数据
 * @param handle TP句柄
 * @param data 触摸数据缓冲区
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_read_touch(liot_tp_handle_t handle, liot_tp_touch_data_t *data);

/**
 * @brief 读取手势数据
 * @param handle TP句柄
 * @param data 手势数据缓冲区
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_read_gesture(liot_tp_handle_t handle, liot_tp_gesture_data_t *data);

/**
 * @brief 设置触摸阈值
 * @param handle TP句柄
 * @param threshold 阈值 (值越小越灵敏)
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_set_threshold(liot_tp_handle_t handle, uint8_t threshold);

/**
 * @brief TP进入睡眠
 * @param handle TP句柄
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_sleep(liot_tp_handle_t handle);

/**
 * @brief TP唤醒
 * @param handle TP句柄
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_wakeup(liot_tp_handle_t handle);

/**
 * @brief TP软复位
 * @param handle TP句柄
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_reset(liot_tp_handle_t handle);

/**
 * @brief TP硬件复位 (通过RST引脚)
 * @param handle TP句柄
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_hard_reset(liot_tp_handle_t handle);

/* ================= 传感器驱动用寄存器读写接口 ================= */

/**
 * @brief 寄存器写入 (I2C/SPI自适应)
 * @param handle TP句柄
 * @param reg 寄存器地址
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 0成功, -1失败
 */
int liot_tp_reg_write(liot_tp_handle_t handle, uint8_t reg,
                      const uint8_t *data, uint16_t len);

/**
 * @brief 寄存器读取 (I2C/SPI自适应)
 * @param handle TP句柄
 * @param reg 寄存器地址
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 0成功, -1失败
 */
int liot_tp_reg_read(liot_tp_handle_t handle, uint8_t reg,
                     uint8_t *data, uint16_t len);

/* ================= 寄存器单字节读写辅助接口 ================= */

/**
 * @brief 寄存器单字节写入 (I2C/SPI自适应)
 * @param handle TP句柄
 * @param reg 寄存器地址
 * @param value 写入值
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_reg_write_byte(liot_tp_handle_t handle, uint8_t reg, uint8_t value);

/**
 * @brief 寄存器单字节读取 (I2C/SPI自适应)
 * @param handle TP句柄
 * @param reg 寄存器地址
 * @param value 读取值存放指针
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_reg_read_byte(liot_tp_handle_t handle, uint8_t reg, uint8_t *value);

/**
 * @brief 寄存器位操作 (RMW, I2C/SPI自适应)
 * @param handle TP句柄
 * @param reg 寄存器地址
 * @param mask 位掩码
 * @param value 写入值
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_reg_write_bit(liot_tp_handle_t handle, uint8_t reg, uint8_t mask, uint8_t value);

/* ================= FreeRTOS事件组接口 ================= */

/**
 * @brief 获取TP事件组句柄 (INT中断触发时置位对应事件位)
 * @return osEventFlagsId_t 事件组句柄
 */
osEventFlagsId_t liot_tp_get_event_flags(liot_tp_handle_t handle);

/**
 * @brief 注册TP中断回调函数 (INT引脚触发时自动读取触摸数据并置事件位)
 *        注册后无需外部轮询读取，事件组会通知触摸/手势事件
 * @param handle TP句柄
 * @param touch_cb 触摸事件回调 (可为NULL)
 * @param gesture_cb 手势事件回调 (可为NULL)
 * @param touch_ctx 触摸回调上下文
 * @param gesture_ctx 手势回调上下文
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_register_int_callback(liot_tp_handle_t handle,
        void (*touch_cb)(liot_tp_touch_data_t *data, void *ctx),
        void (*gesture_cb)(liot_tp_gesture_data_t *data, void *ctx),
        void *touch_ctx,
        void *gesture_ctx);

/**
 * @brief 启动/停止INT事件轮询
 *        启动后INT中断会自动读取数据并通过事件组通知
 * @param handle TP句柄
 * @param enable true=启动, false=停止
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_enable_int(liot_tp_handle_t handle, int enable);

/**
 * @brief 固件升级
 * @param handle TP句柄
 * @param fw_data 固件数据指针
 * @param fw_len 固件长度
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_update_firmware(liot_tp_handle_t handle,
        const uint8_t *fw_data, uint32_t fw_len);

/**
 * @brief 设置工作模式
 * @param handle TP句柄
 * @param mode 工作模式
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_set_work_mode(liot_tp_handle_t handle,
        liot_tp_work_mode_e mode);

/**
 * @brief 获取IC信息
 * @param handle TP句柄
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return 错误码
 */
liot_errcode_tp_e liot_tp_get_ic_info(liot_tp_handle_t handle,
        uint8_t *buf, uint16_t buf_len);

/* ================= CST8xxT 专用接口 ================= */

/**
 * @brief 设置CST8xxT的I2C通道和RST引脚（用于固件升级）
 *        CST8xxT固件升级需要切换到BOOT地址(0x6A)，必须使用底层I2C操作。
 *        此函数需在固件升级前调用，告知驱动硬件连接信息。
 * @param i2c_ch I2C通道号
 * @param rst_pin RST引脚号
 */
void cst8xxT_set_i2c_config(liot_i2c_channel_e i2c_ch, int8_t rst_pin);

/* ================= CST816D 专用接口 ================= */

/**
 * @brief 设置CST816D的I2C通道和RST引脚（用于固件升级）
 *        CST816D固件升级需要切换到BOOT地址(0x6A)，必须使用底层I2C操作。
 *        此函数需在固件升级前调用，告知驱动硬件连接信息。
 * @param i2c_ch I2C通道号
 * @param rst_pin RST引脚号
 */
void cst816d_set_i2c_config(liot_i2c_channel_e i2c_ch, int8_t rst_pin);

/**
 * @brief 使用内置固件自动升级CST816D
 *        固件文件通过 liot_tp_cst816d_fw.h 选择（由 LIOT_TP_CST816D_FW_VER 宏控制）
 *        升级前需先调用 cst816d_set_i2c_config() 配置硬件信息
 * @param handle TP句柄
 * @return 0成功, -1失败
 */
int cst816d_auto_update_firmware(liot_tp_handle_t handle);

/* ================= AXS5106 专用接口 ================= */

/**
 * @brief 使用内置固件自动升级AXS5106
 *        固件文件通过 liot_tp_axs5106_fw.h 选择（由 LIOT_TP_AXS5106_FW_VER 宏控制）
 *        升级流程：版本比对 → CRC校验 → 擦除Flash → 写入固件 → CRC验证
 * @param handle TP句柄
 * @return 0成功, -1失败
 */
int axs5106_auto_update_firmware(liot_tp_handle_t handle);

/* ================= 传感器设备外部声明 ================= */



#endif
