/**
 * @File Name: liot_tp_axs5106.c
 * @brief AXS5106 TP sensor driver (based on liot_tp framework)
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_tp.h"
#include "liot_os.h"
#include "liot_log.h"
#include "liot_tp_axs5106_fw.h"
#include <string.h>

/* ================= AXS5106 寄存器定义 ================= */

/* 正常模式寄存器 (I2C地址: 0x63) */
#define AXS5106_REG_POINT_DATA  0x01    /* 触摸数据起始寄存器 */
#define AXS5106_REG_FW_VER      0x05    /* 固件版本 (2字节, 大端) */
#define AXS5106_REG_RST_CMD     0xFF    /* 软复位命令寄存器 */
#define AXS5106_REG_SLEEP       0x19    /* 睡眠寄存器 */
#define AXS5106_REG_LPWG        0x39    /* LPWG模式寄存器 */
#define AXS5106_REG_DEBUG       0xAA    /* 进入调试模式 */
#define AXS5106_REG_DEBUG_EXIT  0xA0    /* 退出调试模式 */

/* 调试模式寄存器 (用于固件升级) */
#define AXS5106_REG_CTRL        0x90    /* 控制寄存器(写命令) */
#define AXS5106_REG_CRC_ADDR    0xCA    /* CRC校验地址 */
#define AXS5106_REG_CRC_LEN     0xCB    /* CRC校验长度 */
#define AXS5106_REG_CRC_RUN     0xCC    /* CRC运行控制 */
#define AXS5106_REG_CRC_RESULT  0xCD    /* CRC结果(2字节) */
#define AXS5106_REG_ERASE       0xCE    /* 擦除控制 */

/* Chip ID (通过调试模式读取) */
#define AXS5106_CHIP_ID_VAL     0x06

/* 触摸点数最大值 */
#define AXS5106_MAX_POINTS      1

/* 触摸数据格式偏移 */
#define AXS5106_TOUCH_BUF_HEAD_LEN   2
#define AXS5106_TOUCH_ONE_POINT_LEN  6

/* I2C地址 (7bit) */
#define AXS5106_I2C_ADDR        0x63

/* 升级配置 */
#define AXS5106_UPGRADE_FAILED_RETRY_TIMES  1

/* ================= 底层I2C辅助 ================= */

static int axs5106_i2c_write(liot_i2c_channel_e ch, uint8_t slave,
                              uint8_t reg, uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err = liot_I2cWrite(ch, slave, reg, data, len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

static int axs5106_i2c_read(liot_i2c_channel_e ch, uint8_t slave,
                             uint8_t reg, uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err = liot_I2cRead(ch, slave, reg, data, len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/* 多字节命令写入 (调试模式使用, 直接发送命令字节+数据) */
static int axs5106_i2c_write_regs(liot_i2c_channel_e ch, uint8_t slave,
                                    const uint8_t *cmd, uint16_t cmd_len,
                                    const uint8_t *data, uint16_t data_len)
{
    liot_errcode_i2c_e err = liot_I2cWrite_RawCmd(ch, slave, cmd, cmd_len, data, data_len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/* 多字节命令读取 (调试模式使用, 发送命令字节后读数据) */
static int axs5106_i2c_read_regs(liot_i2c_channel_e ch, uint8_t slave,
                                   const uint8_t *cmd, uint16_t cmd_len,
                                   uint8_t *data, uint16_t data_len)
{
    liot_errcode_i2c_e err = liot_I2cRead_RawCmd(ch, slave, cmd, cmd_len, data, data_len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/* ================= 硬件操作辅助 ================= */

static void axs5106_hard_reset(int8_t rst_pin)
{
    if (rst_pin < 0) return;
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(1);
    Liot_SetPinLevel(rst_pin, L_IO_LOW);
    osDelay(1);
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(25);
}

/* ================= 设备驱动函数 ================= */

static int axs5106_init(liot_tp_handle_t handle)
{
    uint8_t fw_ver_buf[2] = {0};

    /* 读取固件版本确认通信正常 */
    if (liot_tp_reg_read(handle, AXS5106_REG_FW_VER, fw_ver_buf, 2) != 0) {
        liot_trace("AXS5106 read fw_ver failed");
        return -1;
    }

    uint16_t fw_ver = ((uint16_t)fw_ver_buf[0] << 8) | fw_ver_buf[1];
    liot_trace("AXS5106 init ok, fw_ver=0x%x", fw_ver);

    return 0;
}

static int axs5106_deinit(liot_tp_handle_t handle)
{
    (void)handle;
    return 0;
}

static int axs5106_read_touch(liot_tp_handle_t handle, liot_tp_touch_data_t *data)
{
    /*
     * AXS5106 触摸数据格式 (从寄存器0x01读取):
     *   buf[0] = Gesture ID
     *   buf[1] = 触摸点数
     *   每个点6字节 (point_num个点依次排列):
     *     [0] = [7:6]=Event, [3:0]=X[11:8]
     *     [1] = X低字节
     *     [2] = [7:4]=ID,    [3:0]=Y[11:8]
     *     [3] = Y低字节
     *     [4] = Weight
     *     [5] = Area
     */
    uint8_t buf[AXS5106_TOUCH_BUF_HEAD_LEN +
                AXS5106_MAX_POINTS * AXS5106_TOUCH_ONE_POINT_LEN];

    if (liot_tp_reg_read(handle, AXS5106_REG_POINT_DATA, buf, sizeof(buf)) != 0) {
        return -1;
    }

    uint8_t point_num = buf[1];
    if (point_num == 0 || point_num > AXS5106_MAX_POINTS) {
        data->touch_cnt = 0;
        return 0;
    }

    data->touch_cnt = point_num;

    for (uint8_t i = 0; i < point_num; i++) {
        const uint8_t *p = &buf[AXS5106_TOUCH_BUF_HEAD_LEN +
                                 i * AXS5106_TOUCH_ONE_POINT_LEN];

        uint16_t x = ((uint16_t)(p[0] & 0x0F) << 8) | p[1];
        uint16_t y = ((uint16_t)(p[2] & 0x0F) << 8) | p[3];

        data->point[i].id           = (p[2] >> 4) & 0x0F;
        data->point[i].x            = x;
        data->point[i].y            = y;
        data->point[i].weight       = p[4];
        data->point[i].timestamp_ms = osKernelGetTickCount();

        /* 事件判断: p[0] bit[7:6]
         *   0x00 = 按下 (DOWN)
         *   0x01 = 抬起 (UP)
         *   0x02 = 接触/移动 (CONTACT/MOVE)
         */
        uint8_t event_flag = (p[0] >> 6) & 0x03;
        switch (event_flag) {
        case 0x00: data->point[i].event = LIOT_TP_EVT_DOWN; break;
        case 0x01: data->point[i].event = LIOT_TP_EVT_UP;   break;
        case 0x02: data->point[i].event = LIOT_TP_EVT_MOVE; break;
        default:   data->point[i].event = LIOT_TP_EVT_MOVE; break;
        }
    }

    return 0;
}

static int axs5106_read_gesture(liot_tp_handle_t handle, liot_tp_gesture_data_t *data)
{
    uint8_t buf[2];

    if (liot_tp_reg_read(handle, AXS5106_REG_POINT_DATA, buf, 2) != 0) {
        return -1;
    }

    data->timestamp_ms = osKernelGetTickCount();

    switch (buf[0]) {
    case 0x01: data->gesture = LIOT_TP_GESTURE_UP;          break;
    case 0x02: data->gesture = LIOT_TP_GESTURE_DOWN;        break;
    case 0x03: data->gesture = LIOT_TP_GESTURE_LEFT;        break;
    case 0x04: data->gesture = LIOT_TP_GESTURE_RIGHT;       break;
    case 0x0C: data->gesture = LIOT_TP_GESTURE_LONG_PRESS;  break;
    case 0x05:
    case 0x0B:
        data->gesture = LIOT_TP_GESTURE_UP;
        break;
    case 0x21: data->gesture = LIOT_TP_GESTURE_ZOOM_IN;    break;
    case 0x20: data->gesture = LIOT_TP_GESTURE_ZOOM_OUT;   break;
    default:
        data->gesture = LIOT_TP_GESTURE_NONE;
        break;
    }

    return 0;
}

static int axs5106_set_threshold(liot_tp_handle_t handle, uint8_t threshold)
{
    (void)handle;
    (void)threshold;
    /* AXS5106 不支持直接设置触摸阈值 */
    return 0;
}

static int axs5106_enter_sleep(liot_tp_handle_t handle)
{
    uint8_t cmd = 0x03;
    return liot_tp_reg_write(handle, AXS5106_REG_SLEEP, &cmd, 1);
}

static int axs5106_exit_sleep(liot_tp_handle_t handle)
{
    if (!handle) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    int8_t rst_pin = dev->cfg.rst.pin;
    if (rst_pin < 0) {
        liot_trace("AXS5106 exit_sleep: rst_pin not configured");
        return -1;
    }

    /* 通过硬件复位唤醒 */
    axs5106_hard_reset(rst_pin);
    return 0;
}

static int axs5106_wakeup(liot_tp_handle_t handle)
{
    uint8_t mode = 0x00;
    int ret = liot_tp_reg_write(handle, AXS5106_REG_LPWG, &mode, 1);
    if (ret != 0) {
        /* 如果I2C写失败(芯片在睡眠中)，依赖硬件复位唤醒 */
        return -1;
    }
    return 0;
}

static int axs5106_reset(liot_tp_handle_t handle)
{
    uint8_t rst_cmd = 0xFF;
    return liot_tp_reg_write(handle, AXS5106_REG_RST_CMD, &rst_cmd, 1);
}

/* ================= 固件升级 ================= */

static int axs5106_enter_debug_mode(liot_i2c_channel_e ch, int8_t rst_pin)
{
    uint8_t debug_cmd = 0x55;
    uint8_t write_buf[3] = {0x80, 0x7F, 0xD1};
    uint8_t read_buf[1] = {0x00};
    uint8_t rst_cmd = 0xFF;
    int retry;

    for (retry = 0; retry < 3; retry++) {
        /* 硬件复位 */
        if (rst_pin >= 0) {
            Liot_SetPinLevel(rst_pin, L_IO_HIGH);
            osDelay(1);
            Liot_SetPinLevel(rst_pin, L_IO_LOW);
            osDelay(1);
            axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_RST_CMD, &rst_cmd, 1);
            osDelay(2);
            Liot_SetPinLevel(rst_pin, L_IO_HIGH);
            osDelay(1);
        }

        /* 发送进入调试模式命令 */
        if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_DEBUG, &debug_cmd, 1) != 0) {
            osDelay(1);
            continue;
        }

        osDelay(1);

        /* 验证调试模式 */
        if (axs5106_i2c_read_regs(ch, AXS5106_I2C_ADDR, write_buf, 3, read_buf, 1) != 0) {
            osDelay(1);
            continue;
        }

        if (read_buf[0] == 0x28) {
            liot_trace("AXS5106 enter debug mode ok");
            return 0;
        }

        osDelay(1);
    }

    liot_trace("AXS5106 enter debug mode failed");
    return -1;
}

static int axs5106_exit_debug_mode(liot_i2c_channel_e ch)
{
    uint8_t cmd = 0x5F;
    return axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_DEBUG_EXIT, &cmd, 1);
}

static int axs5106_unlock_mtpc(liot_i2c_channel_e ch)
{
    uint8_t cmd1[3] = {0x6F, 0xFF, 0xFF};
    uint8_t cmd2[3] = {0x6F, 0xDA, 0x18};

    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd1, 3) != 0)
        return -1;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd2, 3) != 0)
        return -1;

    return 0;
}

static int axs5106_flash_erase(liot_i2c_channel_e ch)
{
    uint8_t clear_flag[3] = {0x6F, 0xD9, 0x0C};
    uint8_t erase_cmd[3] = {0x6F, 0xD6, 0x77};
    uint8_t write_buf[3] = {0x80, 0x7F, 0xD9};
    uint8_t read_buf[1] = {0x00};
    int retry = 30;

    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, clear_flag, 3) != 0)
        return -1;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, erase_cmd, 3) != 0)
        return -1;

    while (retry--) {
        osDelay(10);
        if (axs5106_i2c_read_regs(ch, AXS5106_I2C_ADDR, write_buf, 3, read_buf, 1) != 0)
            continue;
        if (read_buf[0] & 0x04) {
            erase_cmd[2] = 0x00;
            axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, erase_cmd, 3);
            return 0;
        }
    }

    erase_cmd[2] = 0x00;
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, erase_cmd, 3);
    liot_trace("AXS5106 flash erase failed");
    return -1;
}

static int axs5106_flash_write(liot_i2c_channel_e ch,
                                const uint8_t *fw_data, uint16_t fw_len)
{
    uint8_t cmd[3] = {0x6F, 0xD4, 0x00};
    uint16_t i;

    /* 配置写Flash寄存器 */
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0)
        return -1;

    cmd[1] = 0xD5;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0)
        return -1;

    cmd[1] = 0xD2;
    cmd[2] = (fw_len - 1) & 0xFF;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0)
        return -1;

    cmd[1] = 0xD3;
    cmd[2] = ((fw_len - 1) >> 8) & 0xFF;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0)
        return -1;

    cmd[1] = 0xD6;
    cmd[2] = 0xF4;
    if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0)
        return -1;

    /* 逐字节写入数据 */
    liot_trace("AXS5106 flash write start: total %u bytes", (unsigned)fw_len);
    cmd[1] = 0xD7;
    for (i = 0; i < fw_len; i++) {
        cmd[2] = fw_data[i];
        if (axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3) != 0) {
            liot_trace("AXS5106 flash write failed at offset %d/0x%02X data=0x%02X",
                       i, i, fw_data[i]);
            return -1;
        }
        if ((i & 0xFF) == 0xFF) {
            liot_trace("AXS5106 flash write progress: %u/%u bytes",
                       (unsigned)(i + 1), (unsigned)fw_len);
        }
    }

    /* 结束写操作 */
    cmd[1] = 0xD6;
    cmd[2] = 0x00;
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, cmd, 3);

    return 0;
}

static uint16_t axs5106_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0x0000;
    uint16_t poly = 0x1021;

    while (len--) {
        crc ^= ((uint16_t)*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ poly;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

static int axs5106_crc_check(liot_i2c_channel_e ch,
                              const uint8_t *fw_data, uint16_t fw_len)
{
    uint8_t write_check_program_buf[] = {
        0x02,0x00,0x03,0x78,0x7F,0xE4,0xF6,0xD8,0xFD,0x75,0x81,0x31,0x02,0x00,0x26,0xFF,
        0xFF,0xFF,0xFF,0x02,0x01,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0x02,0x01,0xE7,0x53,0xB1,0x1F,0x43,0xB2,0x20,0x12,0x02,0x29,0x75,
        0xFF,0xFF,0x75,0xD6,0x03,0x75,0xD2,0x5F,0x75,0xD3,0x14,0x75,0xD4,0x54,0x75,0xD5,
        0x0F,0x75,0xD7,0xA3,0xE4,0xF5,0xDB,0xC2,0x01,0xF5,0x26,0x75,0x10,0x10,0x75,0x11,
        0x21,0x30,0x01,0x6D,0x85,0x29,0xD4,0xE5,0x28,0xF5,0xD5,0xE5,0x2B,0x24,0xFF,0xFF,
        0xE5,0x2A,0x34,0xFF,0x8F,0xD2,0xF5,0xD3,0x75,0xFF,0xFF,0x53,0xD9,0xFC,0x75,0xD6,
        0xF1,0xE4,0xFB,0xFA,0xF5,0x82,0xF5,0x83,0xC3,0xE5,0x82,0x95,0x2B,0xE5,0x83,0x95,
        0x2A,0x50,0x32,0xE5,0xD9,0x54,0x03,0x64,0x01,0x60,0xF8,0xAD,0xD7,0xED,0xFE,0xEE,
        0x62,0x02,0xE4,0xFC,0xEA,0x30,0xE7,0x0F,0xEB,0x25,0xE0,0xFF,0xEA,0x33,0x65,0x10,
        0xFA,0xE5,0x11,0x6F,0xFB,0x80,0x07,0xEB,0x25,0xE0,0xFB,0xEA,0x33,0xFA,0x0C,0xBC,
        0x08,0xE2,0xA3,0x80,0xC3,0x75,0xFF,0xFF,0xE4,0xF5,0xD6,0x8A,0x30,0x8B,0x31,0xC2,
        0x01,0xE5,0x26,0x60,0x8C,0x75,0xFF,0xFF,0x75,0xDA,0x18,0x43,0xD9,0x04,0xE4,0xF5,
        0xD9,0xF5,0xD4,0xF5,0xD5,0x7D,0x05,0xE5,0x26,0xB4,0xFF,0x02,0x7D,0x07,0xED,0x44,
        0x70,0xF5,0xD6,0xE5,0xD9,0x30,0xE2,0xFB,0x43,0xD9,0x04,0xE4,0xF5,0xD6,0xF5,0xD9,
        0x75,0xFF,0xFF,0xF5,0xD2,0xF5,0xD3,0x75,0xD6,0xF4,0x75,0xD7,0x02,0xE5,0xD9,0x54,
        0x03,0x64,0x01,0x60,0xF8,0xE4,0xF5,0xD6,0xF5,0xD9,0xF5,0x26,0x02,0x00,0x51,0xC0,
        0xE0,0xC0,0xD0,0x75,0xD0,0x08,0xE5,0x9C,0x20,0xE0,0x03,0x02,0x01,0xE2,0x43,0x98,
        0x08,0x20,0x00,0x03,0x02,0x01,0xD4,0xE5,0x22,0x24,0x36,0x60,0x22,0x14,0x60,0x43,
        0x14,0x60,0x64,0x14,0x60,0x7B,0x14,0x70,0x03,0x02,0x01,0xBA,0x24,0xC4,0x60,0x03,
        0x02,0x01,0xCE,0x75,0x2C,0x51,0x75,0x2D,0x06,0x75,0x2E,0x03,0x02,0x01,0xD4,0xE5,
        0x27,0xB4,0x03,0x10,0xE5,0x23,0xFE,0x7C,0x00,0xE4,0x25,0x24,0xF5,0x29,0xEC,0x3E,
        0xF5,0x28,0x80,0x70,0xE5,0x27,0x64,0x01,0x70,0x6A,0xE5,0x28,0xF5,0x2C,0x85,0x29,
        0x2D,0x80,0x61,0xE5,0x27,0xB4,0x03,0x10,0xE5,0x23,0xFE,0x7C,0x00,0xE4,0x25,0x24,
        0xF5,0x2B,0xEC,0x3E,0xF5,0x2A,0x80,0x4C,0xE5,0x27,0x64,0x01,0x70,0x46,0xE5,0x2A,
        0xF5,0x2C,0x85,0x2B,0x2D,0x80,0x3D,0xE5,0x27,0xB4,0x02,0x08,0xE5,0x23,0x60,0x34,
        0xD2,0x01,0x80,0x30,0xE5,0x27,0x64,0x01,0x70,0x2A,0xA2,0x01,0x33,0xF5,0x2C,0x80,
        0x23,0xE5,0x30,0xF5,0x2C,0x85,0x31,0x2D,0x80,0x1A,0xE5,0x27,0xB4,0x02,0x05,0x85,
        0x23,0x26,0x80,0x10,0xE5,0x27,0xB4,0x01,0x0B,0x85,0x26,0x2C,0x80,0x06,0x75,0x2C,
        0xEF,0x75,0x2D,0xFE,0x75,0x27,0x00,0x75,0x21,0x01,0x85,0x2C,0x9B,0xC2,0x00,0x53,
        0x9C,0xFE,0xD0,0xD0,0xD0,0xE0,0x32,0xC0,0xE0,0xC0,0xD0,0x75,0xD0,0x08,0xE5,0x99,
        0x30,0xE1,0x13,0xAF,0x21,0x05,0x21,0x74,0x2C,0x2F,0xF8,0xE6,0xF5,0x9B,0x53,0x99,
        0xFD,0x75,0x27,0x00,0x80,0x1E,0xE5,0x99,0x30,0xE0,0x19,0xE5,0x27,0xC3,0x94,0x04,
        0x50,0x0A,0xAF,0x27,0x05,0x27,0x74,0x22,0x2F,0xF8,0xA6,0x9B,0x53,0x99,0xFE,0x75,
        0x21,0x00,0xD2,0x00,0xD0,0xD0,0xD0,0xE0,0x32,0xE4,0xF5,0x27,0xF5,0x21,0x43,0xB3,
        0x01,0x53,0x99,0xFD,0x53,0x99,0xFE,0x53,0x9C,0xFE,0x43,0x98,0x40,0x43,0x98,0x20,
        0x43,0x98,0x80,0xD2,0xAC,0xD2,0xAA,0xD2,0xAF,0xC2,0x00,0x22,0x00,0x00,0x00,0x00
    };

    uint8_t write_xram_reg[6] = {0x50, 0xAF, 0x00, 0x00,
                                  (uint8_t)((sizeof(write_check_program_buf) - 1) >> 8),
                                  (uint8_t)((sizeof(write_check_program_buf) - 1) & 0xFF)};
    uint8_t read_xram_reg[6] = {0x40, 0xBF, 0x00, 0x00,
                                 (uint8_t)((sizeof(write_check_program_buf) - 1) >> 8),
                                 (uint8_t)((sizeof(write_check_program_buf) - 1) & 0xFF)};
    uint8_t read_check_buf[sizeof(write_check_program_buf)];
    uint8_t rd_crc16_buf[2];
    uint16_t rd_crc16, host_crc16;
    int i, retry;

    /* 1. 写入CRC校验程序到XRAM */
    for (retry = 0; retry < 2; retry++) {
        if (axs5106_i2c_write_regs(ch, AXS5106_I2C_ADDR,
                write_xram_reg, 6, write_check_program_buf, sizeof(write_check_program_buf)) != 0) {
            continue;
        }

        osDelay(2);

        /* 读回验证 */
        if (axs5106_i2c_read_regs(ch, AXS5106_I2C_ADDR,
                read_xram_reg, 6, read_check_buf, sizeof(read_check_buf)) != 0) {
            continue;
        }

        bool match = true;
        for (i = 0; i < (int)sizeof(write_check_program_buf); i++) {
            if (read_check_buf[i] != write_check_program_buf[i]) {
                match = false;
                break;
            }
        }
        if (match) break;
    }

    if (retry >= 2) {
        liot_trace("AXS5106 XRAM program verify failed");
        return -1;
    }

    /* 2. 运行CRC校验程序 */
    uint8_t redirect_cmd[3] = {0x6F, 0xE6, 0x02};
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CTRL, redirect_cmd, 3);
    osDelay(1);
    axs5106_exit_debug_mode(ch);
    osDelay(5);

    uint8_t check_buf[3] = {0};
    axs5106_i2c_read(ch, AXS5106_I2C_ADDR, 0x0A, check_buf, 3);
    if (check_buf[0] != 0x51 || check_buf[1] != 0x06) {
        liot_trace("AXS5106 CRC program init failed");
        return -1;
    }

    /* 3. 启动CRC计算 */
    uint8_t addr_buf[2] = {0x00, 0x00};
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CRC_ADDR, addr_buf, 2);

    addr_buf[0] = (uint8_t)(fw_len >> 8);
    addr_buf[1] = (uint8_t)(fw_len & 0xFF);
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CRC_LEN, addr_buf, 2);

    uint8_t run_cmd = 0x01;
    axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_CRC_RUN, &run_cmd, 1);

    /* 等待CRC完成 */
    for (i = 0; i < 10; i++) {
        osDelay(300);
        uint8_t status = 0xFF;
        axs5106_i2c_read(ch, AXS5106_I2C_ADDR, AXS5106_REG_CRC_RUN, &status, 1);
        if (status == 0) break;
    }

    if (i >= 10) {
        liot_trace("AXS5106 CRC calc timeout");
        return -1;
    }

    /* 4. 读取CRC结果并比较 */
    axs5106_i2c_read(ch, AXS5106_I2C_ADDR, AXS5106_REG_CRC_RESULT, rd_crc16_buf, 2);
    rd_crc16 = ((uint16_t)rd_crc16_buf[0] << 8) | rd_crc16_buf[1];
    host_crc16 = axs5106_crc16(fw_data, fw_len);

    if (rd_crc16 != host_crc16) {
        liot_trace("AXS5106 CRC mismatch (chip=0x%x, host=0x%x)",
                         rd_crc16, host_crc16);
        return -1;
    }

    liot_trace("AXS5106 CRC verify ok (0x%x)", host_crc16);
    return 0;
}

static int axs5106_update_fw(liot_i2c_channel_e ch, int8_t rst_pin,
                              const uint8_t *fw_data, uint32_t fw_len)
{
    int retry;

    if (!fw_data || fw_len == 0) {
        return -1;
    }

    for (retry = 1; retry <= AXS5106_UPGRADE_FAILED_RETRY_TIMES; retry++) {
        /* 进入调试模式 */
        if (axs5106_enter_debug_mode(ch, rst_pin) != 0) {
            continue;
        }

        /* 解锁MTP写保护 */
        if (axs5106_unlock_mtpc(ch) != 0) {
            liot_trace("AXS5106 unlock MTPC failed");
            continue;
        }
        liot_trace("AXS5106 unlock MTPC ok");

        /* 擦除Flash */
        if (axs5106_flash_erase(ch) != 0) {
            liot_trace("AXS5106 flash erase failed, retry %d", retry);
            continue;
        }
        liot_trace("AXS5106 flash erase ok");

        /* 写入固件 */
        if (axs5106_flash_write(ch, fw_data, (uint16_t)fw_len) != 0) {
            liot_trace("AXS5106 flash write failed, retry %d", retry);
            continue;
        }
        liot_trace("AXS5106 flash write ok (%u bytes)", (unsigned)fw_len);

        /* CRC校验 */
        osDelay(1);
        if (axs5106_crc_check(ch, fw_data, (uint16_t)fw_len) != 0) {
            liot_trace("AXS5106 CRC verify failed, retry %d", retry);
            /* 擦除失败数据 */
            uint8_t erase_cmd = 0xFF;
            axs5106_i2c_write(ch, AXS5106_I2C_ADDR, AXS5106_REG_ERASE, &erase_cmd, 1);
            osDelay(10);
            continue;
        }

        liot_trace("AXS5106 FW update success");
        break;
    }

    /* 退出调试模式 */
    axs5106_exit_debug_mode(ch);

    /* 硬件复位恢复正常工作 */
    axs5106_hard_reset(rst_pin);

    return (retry <= AXS5106_UPGRADE_FAILED_RETRY_TIMES) ? 0 : -1;
}

static int axs5106_update_firmware(liot_tp_handle_t handle,
                                    const uint8_t *fw_data, uint32_t fw_len)
{
    if (!handle || !fw_data || fw_len == 0) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    if (dev->cfg.interface_type != LIOT_TP_IF_I2C) {
        liot_trace("AXS5106 FW update: only I2C interface supported");
        return -1;
    }

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (rst_pin < 0) {
        liot_trace("AXS5106 FW update: rst_pin not configured");
        return -1;
    }

    return axs5106_update_fw(i2c_ch, rst_pin, fw_data, fw_len);
}

static int axs5106_need_upgrade(liot_i2c_channel_e ch, int8_t rst_pin,
                                 const uint8_t *fw_data, uint32_t fw_len)
{
    uint16_t fw_ver_in_bin = 0;
    uint16_t fw_ver_in_tp = 0;
    uint8_t ver_buf[2];

    /* 从固件数据中提取版本号 (偏移0x400处, 2字节大端) */
    if (fw_len >= 0x402) {
        fw_ver_in_bin = ((uint16_t)fw_data[0x400] << 8) | fw_data[0x401];
    } else {
        liot_trace("AXS5106 fw too short for version check");
        return 1; /* 版本信息不足,强制升级 */
    }

    /* 读取芯片中的固件版本 */
    if (axs5106_i2c_read(ch, AXS5106_I2C_ADDR, AXS5106_REG_FW_VER, ver_buf, 2) == 0) {
        fw_ver_in_tp = ((uint16_t)ver_buf[0] << 8) | ver_buf[1];
    } else {
        return 1; /* 读取失败,需要升级 */
    }

    liot_trace("AXS5106 fw_ver: tp=0x%x, bin=0x%x",
                     fw_ver_in_tp, fw_ver_in_bin);

    if (fw_ver_in_tp != fw_ver_in_bin) {
        liot_trace("AXS5106 fw version mismatch, need upgrade");
        return 1; /* 版本不同,需要升级 */
    }

    /* 版本相同,进一步通过CRC校验Flash数据完整性 */
    if (axs5106_enter_debug_mode(ch, rst_pin) != 0) {
        liot_trace("AXS5106 enter debug mode for CRC check failed");
        return 1;
    }

    osDelay(1);
    if (axs5106_crc_check(ch, fw_data, (uint16_t)fw_len) != 0) {
        liot_trace("AXS5106 CRC check failed, need upgrade");
        axs5106_exit_debug_mode(ch);
        return 1;
    }

    liot_trace("AXS5106 CRC check ok, no need upgrade");
    axs5106_exit_debug_mode(ch);
    axs5106_hard_reset(rst_pin);
    return 0;
}

/* ================= 工作模式 ================= */

static int axs5106_set_work_mode(liot_tp_handle_t handle, liot_tp_work_mode_e mode)
{
    uint8_t reg_val;

    switch (mode) {
    case LIOT_TP_MODE_NORMAL:
        reg_val = 0x00;  /* 退出LPWG */
        return liot_tp_reg_write(handle, AXS5106_REG_LPWG, &reg_val, 1);
    case LIOT_TP_MODE_GESTURE:
    case LIOT_TP_MODE_LOW_POWER:
        reg_val = 0x01;  /* 进入LPWG */
        return liot_tp_reg_write(handle, AXS5106_REG_LPWG, &reg_val, 1);
    case LIOT_TP_MODE_DEEP_SLEEP:
        return axs5106_enter_sleep(handle);
    default:
        return -1;
    }
}

/* ================= IC信息 ================= */

static int axs5106_get_ic_info(liot_tp_handle_t handle, uint8_t *buf, uint16_t buf_len)
{
    if (!buf || buf_len < 4) return -1;

    uint8_t info[4] = {0};

    /* 芯片类型 */
    info[0] = AXS5106_CHIP_ID_VAL;

    /* 固件版本 */
    if (liot_tp_reg_read(handle, AXS5106_REG_FW_VER, &info[1], 2) != 0) {
        info[1] = 0;
        info[2] = 0;
    }

    /* LPWG模式 */
    if (liot_tp_reg_read(handle, AXS5106_REG_LPWG, &info[3], 1) != 0) {
        info[3] = 0;
    }

    uint16_t copy_len = (buf_len < 4) ? buf_len : 4;
    memcpy(buf, info, copy_len);

    liot_trace("AXS5106 IC info: chip=0x%x, fw_ver=0x%x%x, lpwg=0x%x",
                     info[0], info[1], info[2], info[3]);

    return 0;
}

/* ================= 设备实例导出 ================= */

liot_tp_sensor_t g_liot_tp_axs5106 = {
    .chip_id         = AXS5106_CHIP_ID_VAL,
    .max_points      = AXS5106_MAX_POINTS,
    .width           = 128,
    .height          = 128,
    .gesture_support = true,
    .func = {
        .init           = axs5106_init,
        .deinit         = axs5106_deinit,
        .read_touch     = axs5106_read_touch,
        .read_gesture   = axs5106_read_gesture,
        .set_threshold  = axs5106_set_threshold,
        .enter_sleep    = axs5106_enter_sleep,
        .exit_sleep     = axs5106_exit_sleep,
        .wakeup         = axs5106_wakeup,
        .reset          = axs5106_reset,
        .update_firmware = axs5106_update_firmware,
        .update_firmware_auto = axs5106_auto_update_firmware,
        .set_work_mode  = axs5106_set_work_mode,
        .get_ic_info    = axs5106_get_ic_info,
    },
};


/* ================= 自动固件升级 ================= */

/**
 * @brief 使用内置固件自动升级AXS5106
 *        固件文件通过 liot_tp_axs5106_fw.h 选择（由 LIOT_TP_AXS5106_FW_VER 宏控制）
 * @param handle TP句柄
 * @return 0成功, -1失败
 */
int axs5106_auto_update_firmware(liot_tp_handle_t handle)
{
    if (!handle) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    if (dev->cfg.interface_type != LIOT_TP_IF_I2C) {
        liot_trace("AXS5106 auto update: only I2C interface supported");
        return -1;
    }

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (rst_pin < 0) {
        liot_trace("AXS5106 auto update: rst_pin not configured");
        return -1;
    }

    liot_trace("AXS5106 auto update with built-in FW (ver=%d)",
                     LIOT_TP_AXS5106_FW_VER);

    /* 先硬件复位, 确保芯片处于正常模式 */
    axs5106_hard_reset(rst_pin);
    osDelay(10);

    /* 检查是否需要升级 */
    if (axs5106_need_upgrade(i2c_ch, rst_pin,
                              axs5106_fw_bin, sizeof(axs5106_fw_bin)) == 0) {
        liot_trace("AXS5106 no need update");
        return 0;
    }

    /* 执行升级 */
    return axs5106_update_fw(i2c_ch, rst_pin,
                              axs5106_fw_bin, sizeof(axs5106_fw_bin));
}
