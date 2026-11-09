#include "sc7a20h.h"
#include "sc7a20h_def.h"
#include "liot_i2c.h"
#include "liot_gpio2.h"
#include "lierda_log.h"
#include "pinmap.h"

/* ================= 本地硬件配置 ================= */
#define SC7A20H_I2C_PORT		0
#define SC7A20H_I2C_ADDR		SC7A20H_IIC_ADDRESS

/* 每次寄存器读写都打印，会以 10ms 频率刷屏；日常调试置 0，仅排障时置 1 */
#define SC7A20H_I2C_TRACE_ENABLE	0

/* ================= HAL 接口实现 ================= */

/**
 * @brief 写入 SC7A20H 寄存器
 * @param addr 寄存器地址
 * @param data 要写入的数据指针
 * @param len 要写入的数据长度
 * @return int32_t 0 成功, -1 失败
 */
int32_t sc7a20h_write_reg(uint8_t addr, const uint8_t *data, uint16_t len) {
    liot_errcode_i2c_e err = liot_I2cWrite(SC7A20H_I2C_PORT, 
                                            SC7A20H_I2C_ADDR, 
                                            addr, 
                                            (uint8_t*)data, 
                                            len);
    if (SC7A20H_I2C_TRACE_ENABLE && err == LIOT_I2C_SUCCESS && data != NULL && len > 0) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "[SC7A20H] W 0x%02X <- 0x%02X", addr, data[0]);
    }
    if (err != LIOT_I2C_SUCCESS) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, "[SC7A20H] I2C write fail: reg=0x%02X err=%d", addr, err);
    }
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/**
 * @brief 读取 SC7A20H 寄存器
 * @param addr 寄存器地址
 * @param data 存储读取数据的缓冲区
 * @param len 读取数据长度
 * @return int32_t 0 成功, -1 失败
 */
static int32_t sc7a20h_read_reg(uint8_t addr, uint8_t *data, uint16_t len) {
    liot_errcode_i2c_e err = liot_I2cRead(SC7A20H_I2C_PORT, 
                                           SC7A20H_I2C_ADDR, 
                                           addr, 
                                           data, 
                                           len);
    if (SC7A20H_I2C_TRACE_ENABLE && err == LIOT_I2C_SUCCESS && data != NULL && len > 0) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_INFO, "[SC7A20H] R 0x%02X -> 0x%02X", addr, data[0]);
    }
    if (err != LIOT_I2C_SUCCESS) {
        LIOT_PRINTF(UNILOG_LIOT_OPEN, P_ERROR, "[SC7A20H] I2C read fail: reg=0x%02X err=%d", addr, err);
    }
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/**
 * @brief 延时指定毫秒数
 * @param ms 延时时间（毫秒）
 */
static void sc7a20h_delay_ms(uint32_t ms) {
    osDelay(ms);
}

/**
 * @brief 获取当前时间戳（毫秒）
 * @return uint32_t 当前时间戳（毫秒）
 */
static uint32_t sc7a20h_get_tick_ms(void) {
    return osKernelGetTickCount();
}

/* ================= 导出 HAL 实例 ================= */
const sc7a20h_hal_t g_sc7a20h_hal_liot = {
    .write_reg = sc7a20h_write_reg,
    .read_reg = sc7a20h_read_reg,
    .delay_ms = sc7a20h_delay_ms,
    .get_tick_ms = sc7a20h_get_tick_ms,
};
