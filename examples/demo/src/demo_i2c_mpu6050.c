/**
 * @File Name: demo_i2c_mpu6050.c
 * @brief
 * @Author : L email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 浙江利尔达物联网科技有限公司
 *
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_i2c.h"
#include "liot_os.h"

/*===========================================================================
 * I2C Pad Configuration (for liot_i2c_2 channel)
 * Note: Pin function values differ between EC718 and EC716
===========================================================================*/
#if defined (CHIP_EC718)
#define LIOT_I2C_SCL_BIT  67
#define LIOT_I2C_SCL_FUNC (2)

#define LIOT_I2C_SDA_BIT  66
#define LIOT_I2C_SDA_FUNC (2)
#elif defined (CHIP_EC716)
#define LIOT_I2C_SCL_BIT  67
#define LIOT_I2C_SCL_FUNC (1)

#define LIOT_I2C_SDA_BIT  66
#define LIOT_I2C_SDA_FUNC (1)
#endif

// MPU6050寄存器地址定义
#define MPU6050_ADDR            0x68
#define MPU6050_PWR_MGMT_1      0x6B
#define MPU6050_SMPLRT_DIV      0x19
#define MPU6050_CONFIG          0x1A
#define MPU6050_GYRO_CONFIG     0x1B
#define MPU6050_ACCEL_CONFIG    0x1C
#define MPU6050_INT_ENABLE      0x38
#define MPU6050_ACCEL_XOUT_H    0x3B
#define MPU6050_GYRO_XOUT_H     0x43

// MPU6050配置值定义
#define MPU6050_PWR_WAKE_UP         0x00    // 唤醒设备（退出睡眠模式）
#define MPU6050_SAMPLE_RATE_DIV     0x07    // 采样率分频：1kHz/(1+7)=125Hz
#define MPU6050_DLPF_CFG            0x00    // 数字低通滤波器配置：关闭
#define MPU6050_GYRO_FS_250         0x00    // 陀螺仪量程：±250°/s
#define MPU6050_ACCEL_FS_2G         0x00    // 加速度计量程：±2g
#define MPU6050_INT_DATA_RDY_EN     0x01    // 使能数据就绪中断

// MPU6050数据长度定义
#define MPU6050_AXIS_DATA_LEN       6       // 三轴数据长度（每轴2字节）
#define MPU6050_REG_WRITE_LEN       1       // 单字节寄存器写入长度
#define MPU6050_BYTE_SHIFT_8        8       // 高低字节拼接位移

// MPU6050采样周期定义
#define MPU6050_SAMPLE_PERIOD_MS    200     // 数据采样周期（毫秒）
#define MPU6050_INIT_DELAY_MS       200     // 初始化延迟（毫秒）

// MPU6050使用的I2C通道
#define MPU6050_I2C_CHANNEL         liot_i2c_2

// MPU6050初始化函数
static int mpu6050_init(int i2c_no) {
    int ret;
    uint8_t data;

    // 唤醒MPU6050
    data = MPU6050_PWR_WAKE_UP;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_PWR_MGMT_1, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 wake up failed[%d]", ret);
        return ret;
    }

    // 设置采样率
    data = MPU6050_SAMPLE_RATE_DIV;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_SMPLRT_DIV, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 set sample rate failed[%d]", ret);
        return ret;
    }

    // 设置配置寄存器
    data = MPU6050_DLPF_CFG;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_CONFIG, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 set config register failed[%d]", ret);
        return ret;
    }

    // 设置陀螺仪配置寄存器
    data = MPU6050_GYRO_FS_250;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_GYRO_CONFIG, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 set gyro config register failed[%d]", ret);
        return ret;
    }

    // 设置加速度计配置寄存器
    data = MPU6050_ACCEL_FS_2G;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_ACCEL_CONFIG, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 set accel config register failed[%d]", ret);
        return ret;
    }

    // 使能中断
    data = MPU6050_INT_DATA_RDY_EN;
    ret = liot_I2cWrite(i2c_no, MPU6050_ADDR, MPU6050_INT_ENABLE, &data, MPU6050_REG_WRITE_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 enable interrupt failed[%d]", ret);
        return ret;
    }

    return LIOT_I2C_SUCCESS;
}

// 读取MPU6050加速度计数据
static int mpu6050_read_accel(int i2c_no, int16_t *accel_x, int16_t *accel_y, int16_t *accel_z) {
    int ret;
    uint8_t data[MPU6050_AXIS_DATA_LEN];

    // 空指针检查
    if ((NULL == accel_x) || (NULL == accel_y) || (NULL == accel_z)) {
        liot_trace("MPU6050 read accel: invalid parameter (NULL pointer)");
        return LIOT_I2C_INVALID_PARAM_ERR;
    }

    ret = liot_I2cRead(i2c_no, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, data, MPU6050_AXIS_DATA_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 read accel data failed[%d]", ret);
        return ret;
    }

    *accel_x = (data[0] << MPU6050_BYTE_SHIFT_8) | data[1];
    *accel_y = (data[2] << MPU6050_BYTE_SHIFT_8) | data[3];
    *accel_z = (data[4] << MPU6050_BYTE_SHIFT_8) | data[5];

    return LIOT_I2C_SUCCESS;
}

// 读取MPU6050陀螺仪数据
static int mpu6050_read_gyro(int i2c_no, int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z) {
    int ret;
    uint8_t data[MPU6050_AXIS_DATA_LEN];

    // 空指针检查
    if ((NULL == gyro_x) || (NULL == gyro_y) || (NULL == gyro_z)) {
        liot_trace("MPU6050 read gyro: invalid parameter (NULL pointer)");
        return LIOT_I2C_INVALID_PARAM_ERR;
    }

    ret = liot_I2cRead(i2c_no, MPU6050_ADDR, MPU6050_GYRO_XOUT_H, data, MPU6050_AXIS_DATA_LEN);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 read gyro data failed[%d]", ret);
        return ret;
    }

    *gyro_x = (data[0] << MPU6050_BYTE_SHIFT_8) | data[1];
    *gyro_y = (data[2] << MPU6050_BYTE_SHIFT_8) | data[3];
    *gyro_z = (data[4] << MPU6050_BYTE_SHIFT_8) | data[5];

    return LIOT_I2C_SUCCESS;
}

void liot_i2c_mpu6050_demo_thread(void *arg)
{
    (void)arg;  // 未使用的参数

    int i2c_no = MPU6050_I2C_CHANNEL;
    int ret;
    int fastmode = 0;

    liot_rtos_task_sleep_ms(MPU6050_INIT_DELAY_MS);

    liot_trace("I2C DEMO TEXT !!!");
    Liot_SetPinFunc(LIOT_I2C_SCL_BIT, LIOT_I2C_SCL_FUNC);
    Liot_SetPinFunc(LIOT_I2C_SDA_BIT, LIOT_I2C_SDA_FUNC);

    ret = liot_I2cInit(i2c_no, fastmode);
    if (ret != LIOT_I2C_SUCCESS)
    {
        liot_trace("I2C INIT FAILED[%d]", ret);
    }

    // 初始化MPU6050
    ret = mpu6050_init(i2c_no);
    if (ret != LIOT_I2C_SUCCESS) {
        liot_trace("MPU6050 INIT FAILED[%d]", ret);
    }

    int16_t accel_x = 0, accel_y = 0, accel_z = 0;
    int16_t gyro_x = 0, gyro_y = 0, gyro_z = 0;

    while (1)
    {
        // 读取MPU6050数据
        ret = mpu6050_read_accel(i2c_no, &accel_x, &accel_y, &accel_z);
        if (ret == LIOT_I2C_SUCCESS) {
            liot_trace("Accel: X=%d, Y=%d, Z=%d", accel_x, accel_y, accel_z);
        }

        ret = mpu6050_read_gyro(i2c_no, &gyro_x, &gyro_y, &gyro_z);
        if (ret == LIOT_I2C_SUCCESS) {
            liot_trace("Gyro: X=%d, Y=%d, Z=%d", gyro_x, gyro_y, gyro_z);
        }

        liot_rtos_task_sleep_ms(MPU6050_SAMPLE_PERIOD_MS);
    }
}
