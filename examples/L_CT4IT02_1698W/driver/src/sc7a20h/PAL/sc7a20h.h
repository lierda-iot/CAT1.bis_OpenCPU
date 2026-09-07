#ifndef __SC7A20H_DRIVER_H__
#define __SC7A20H_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 错误码定义 ================= */
typedef enum sc7a20h_err_e
{
    SC7A20H_OK = 0,
    SC7A20H_ERR_BUS,
    SC7A20H_ERR_ID,
    SC7A20H_ERR_PARAM,
    SC7A20H_ERR_TIMEOUT,
    SC7A20H_ERR_NOT_INIT,
} sc7a20h_err_e;

/* ================= 硬件抽象层接口 ================= */
typedef struct sc7a20h_hal_t
{
    int32_t (*write_reg)(uint8_t addr, const uint8_t *data, uint16_t len);
    int32_t (*read_reg)(uint8_t addr, uint8_t *data, uint16_t len);
    void    (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);
} sc7a20h_hal_t;

/* ================= 传感器量程 ================= */
typedef enum sc7a20h_range_t
{
    SC7A20H_RANGE_2G  = 0x00,
    SC7A20H_RANGE_4G  = 0x10,
    SC7A20H_RANGE_8G  = 0x20,
    SC7A20H_RANGE_16G = 0x30,
} sc7a20h_range_t;

/* ================= 输出数据率 ================= */
typedef enum sc7a20h_odr_t
{
	SC7A20H_ODR_1_56HZ = 0x01,  //全工作模式
    SC7A20H_ODR_12_5HZ = 0x02,
    SC7A20H_ODR_25HZ   = 0x03,
    SC7A20H_ODR_50HZ   = 0x04,
    SC7A20H_ODR_100HZ  = 0x05,
    SC7A20H_ODR_200HZ  = 0x06,
    SC7A20H_ODR_400HZ  = 0x07,
    SC7A20H_ODR_800HZ  = 0x08,
	SC7A20H_ODR_1480HZ = 0x09,  //高性能模式
	SC7A20H_ODR_2660HZ = 0x0a,
	SC7A20H_ODR_4434HZ = 0x0b,
} sc7a20h_odr_t;

/* ================= 中断引脚 ================= */
typedef enum sc7a20h_int_pin_t
{
    SC7A20H_INT_PIN_NONE = 0,
    SC7A20H_INT_PIN_1    = 1,
    SC7A20H_INT_PIN_2    = 2,
} sc7a20h_int_pin_t;

/* ================= 点击模式 ================= */
typedef enum sc7a20h_click_mode_t
{
    SC7A20H_CLICK_NONE   = 0x00,
    SC7A20H_CLICK_SINGLE = 0x01,
    SC7A20H_CLICK_DOUBLE = 0x02,
    SC7A20H_CLICK_TRIPLE = 0x03,
} sc7a20h_click_mode_t;

/* ================= 工作模式 ================= */
typedef enum sc7a20h_mode_t
{
    // SC7A20H_MODE_POWER_DOWN = 0x00,
    SC7A20H_MODE_NORMAL     = 0x00,
    SC7A20H_MODE_LOW_POWER  = 0x01,
    SC7A20H_MODE_HI_PERF    = 0x02,
	SC7A20H_MODE_HI_PERF_2  = 0x03,
} sc7a20h_mode_t;

/* ================= Click 功能配置 ================= */
typedef struct sc7a20h_click_cfg_t
{
    bool enable;
    sc7a20h_int_pin_t int_pin;
    sc7a20h_click_mode_t mode;
    uint16_t threshold_mg;      /* 物理单位：mg */
    bool axis_x;
    bool axis_y;
    bool axis_z;
    uint8_t time_limit;         /* 寄存器值 (ODR 周期数) */
    uint8_t time_latency;       /* 寄存器值 (ODR 周期数) */
    uint8_t time_window;        /* 寄存器值 (ODR 周期数) */
} sc7a20h_click_cfg_t;

/* ================= Shake/AOI 功能配置 ================= */
typedef struct sc7a20h_shake_cfg_t
{
    bool enable;
    sc7a20h_int_pin_t int_pin;
    uint16_t threshold_mg;      /* 物理单位：mg */
    uint16_t duration_ms;       /* 物理单位：ms */
    bool aoi_and_mode;          /* false=OR, true=AND */
    bool detect_mode;           /* false=方向运动检测, true=方向位置检测 */
    bool axis_x_high;
    bool axis_y_high;
    bool axis_z_high;
    bool axis_x_low;
    bool axis_y_low;
    bool axis_z_low;
} sc7a20h_shake_cfg_t;

/* ================= FIFO 配置 ================= */
typedef struct sc7a20h_fifo_cfg_t
{
    bool enable;
    uint8_t watermark;          /* FIFO 水线值 */
    bool fifo_mode_bypass;      /* true=Bypass, false=FIFO Mode */
} sc7a20h_fifo_cfg_t;

/* ================= 完整设备配置 ================= */
typedef struct sc7a20h_config_t
{
    sc7a20h_range_t range;
    sc7a20h_odr_t odr;
    sc7a20h_mode_t mode;
    sc7a20h_click_cfg_t click;
    sc7a20h_shake_cfg_t shake;
    sc7a20h_fifo_cfg_t fifo;
    bool high_pass_filter;      /* 高通滤波使能 */
} sc7a20h_config_t;

/* ================= 中断状态 ================= */
typedef struct sc7a20h_int_status_t
{
    bool shake_event;
    bool shake_low;
    bool shake_high;
    bool data_ready;
	uint8_t click_times;
    uint8_t raw_status_aoi;
    uint8_t raw_status_click;
} sc7a20h_int_status_t;

/* ================= 加速度数据 ================= */
typedef struct sc7a20h_accel_data_t
{
    int16_t x;
    int16_t y;
    int16_t z;
    uint32_t timestamp_ms;
} sc7a20h_accel_data_t;

/* ================= 设备句柄 ================= */
typedef struct sc7a20h_dev_t
{
    sc7a20h_hal_t		hal;
    sc7a20h_config_t	cfg;
    
    /* 影子寄存器 (避免频繁 RMW 读操作) */
    uint8_t shadow_ctrl1;       /* 0x20 */
    uint8_t shadow_ctrl2;       /* 0x21 */
    uint8_t shadow_ctrl4;       /* 0x23 */
    uint8_t shadow_int1_ctrl;   /* 0x22 */
    uint8_t shadow_int2_ctrl;   /* 0x25 */
    uint8_t shadow_fifo_ctrl;   /* 0x2E */
    
    bool is_init;
    uint32_t init_tick;
} sc7a20h_dev_t;


/* ================= 核心 API ================= */

/**
 * @brief 检查设备是否是SC7A20H传感器
 * @param dev 设备句柄
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_check(sc7a20h_dev_t *dev);

/**
 * @brief 初始化设备
 * @param dev 设备句柄
 * @param hal HAL 操作函数指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_init(sc7a20h_dev_t *dev, const sc7a20h_hal_t *hal);

/**
 * @brief 进入低功耗模式
 * @param dev 设备句柄
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_power_down(sc7a20h_dev_t *dev);

/**
 * @brief 软件复位设备
 * @param dev 设备句柄
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_soft_reset(sc7a20h_dev_t *dev);

/**
 * @brief 设置工作模式
 * @param dev 设备句柄
 * @param mode 工作模式
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_mode_set(sc7a20h_dev_t *dev, sc7a20h_mode_t mode);

/**
 * @brief 应用完整配置
 * @param dev 设备句柄
 * @param cfg 完整配置指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_apply_config(sc7a20h_dev_t *dev, const sc7a20h_config_t *cfg);

/**
 * @brief 单独配置 Click 功能 (不影响其他功能)
 * @param dev 设备句柄
 * @param cfg Click 配置指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_config_click(sc7a20h_dev_t *dev, const sc7a20h_click_cfg_t *cfg);

/**
 * @brief 单独配置 Shake 功能 (不影响其他功能)
 * @param dev 设备句柄
 * @param cfg Shake 配置指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_config_shake(sc7a20h_dev_t *dev, const sc7a20h_shake_cfg_t *cfg);

/**
 * @brief 动态调整 Click 阈值 (单位 mg，不影响其他配置)
 * @param dev 设备句柄
 * @param mg Click 阈值 (单位 mg)
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_set_click_threshold(sc7a20h_dev_t *dev, uint16_t mg);

/**
 * @brief 动态调整 Shake 阈值 (单位 mg，不影响其他配置)
 * @param dev 设备句柄
 * @param mg Shake 阈值 (单位 mg)
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_set_shake_threshold(sc7a20h_dev_t *dev, uint16_t mg);

/**
 * @brief 动态调整 Shake 持续时间 (单位 ms)
 * @param dev 设备句柄
 * @param ms Shake 持续时间 (单位 ms)
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_set_shake_duration(sc7a20h_dev_t *dev, uint16_t ms);

/**
 * @brief 设置量程
 * @param dev 设备句柄
 * @param range 量程
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_set_range(sc7a20h_dev_t *dev, sc7a20h_range_t range);

/**
 * @brief 设置 ODR
 * @param dev 设备句柄
 * @param odr ODR 值
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_odr_set(sc7a20h_dev_t *dev, sc7a20h_odr_t odr);

/**
 * @brief 读取加速度数据
 * @param dev 设备句柄
 * @param data 加速度数据指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_read_accel(sc7a20h_dev_t *dev, sc7a20h_accel_data_t *data);

/**
 * @brief 读取并清除中断状态
 * @param dev 设备句柄
 * @param status 中断状态指针
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_get_int_status(sc7a20h_dev_t *dev, sc7a20h_int_status_t *status);

/**
 * @brief 读取 FIFO 数据
 * @param dev 设备句柄
 * @param data_buf 数据缓冲区 (结构体数组)
 * @param max_len 最大读取数量
 * @return 实际读取数量
 */
uint8_t sc7a20h_read_fifo(sc7a20h_dev_t *dev, sc7a20h_accel_data_t *data_buf, uint8_t max_len);

/**
 * @brief 进入低功耗模式
 * @param dev 设备句柄
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_enter_low_power(sc7a20h_dev_t *dev);

/**
 * @brief 唤醒设备
 * @param dev 设备句柄
 * @return SC7A20H_OK 成功; 其他错误码 SC7A20H_ERR_XXX
 */
sc7a20h_err_e sc7a20h_wakeup(sc7a20h_dev_t *dev);

/**
 * @brief 获取设备状态
 * @param dev 设备句柄
 * @return true 设备就绪; false 设备未就绪
 */
bool sc7a20h_is_ready(sc7a20h_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __SC7A20H_DRIVER_H__ */
