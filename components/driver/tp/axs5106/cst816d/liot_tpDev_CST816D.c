/**
 * @File Name: liot_tp_cst816d.c
 * @brief CST816D TP sensor driver (based on liot_tp framework)
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_tp.h"
#include "liot_os.h"
#include "liot_log.h"
#include "liot_tp_cst816d_fw.h"
#include <string.h>

/* ================= CST816D 寄存器定义 ================= */

/* 正常模式寄存器 (I2C地址: 0x15) */
#define CST816D_REG_TP_STATUS     0x00    /* 触摸状态/手势ID */
#define CST816D_REG_TOUCH_NUM     0x02    /* 触摸点数 & 触摸数据起始 */
#define CST816D_REG_CHIP_ID       0xA7    /* 芯片ID */
#define CST816D_REG_FW_VER        0xA9    /* 固件版本 */
#define CST816D_REG_PROJ_ID       0xB5    /* 项目ID */
#define CST816D_REG_PRJ_INFO      0xB4    /* 项目信息 */
#define CST816D_REG_LPM           0xE5    /* 低功耗模式寄存器 */
#define CST816D_REG_IRQ_CTL       0xFA    /* 中断控制 */

/* Chip ID */
#define CST816D_CHIP_ID_VAL       0xB6

/* 触摸点数最大值 */
#define CST816D_MAX_POINTS        1

/* 低功耗模式值 */
#define CST816D_LPM_ACTIVE        0x00
#define CST816D_LPM_MONITOR       0x01
#define CST816D_LPM_STANDBY       0x02
#define CST816D_LPM_SLEEP         0x03
#define CST816D_LPM_GESTURE       0x04

/* 手势ID (从寄存器0x00读取) */
#define CST816D_GEST_NONE         0x00
#define CST816D_GEST_UP           0x01
#define CST816D_GEST_DOWN         0x02
#define CST816D_GEST_LEFT         0x03
#define CST816D_GEST_RIGHT        0x04
#define CST816D_GEST_CLICK        0x05
#define CST816D_GEST_DOUBLE_CLICK 0x0B
#define CST816D_GEST_LONG_PRESS   0x0C

/* ================= Boot模式寄存器 (I2C地址: 0x6A) ================= */
#define CST816D_BOOT_I2C_ADDR     0x6A
#define CST816D_MAIN_I2C_ADDR     0x15

#define CST816D_BOOT_REG_CMD      0xA001  /* 命令寄存器 */
#define CST816D_BOOT_REG_FLAG     0xA003  /* 标志寄存器 */
#define CST816D_BOOT_REG_START    0xA004  /* 启动烧写 */
#define CST816D_BOOT_REG_STATUS   0xA005  /* 烧写状态 */
#define CST816D_BOOT_REG_ADDR     0xA014  /* 地址寄存器 */
#define CST816D_BOOT_REG_DATA     0xA018  /* 数据寄存器 */
#define CST816D_BOOT_REG_CKSUM    0xA008  /* 校验和寄存器 */
#define CST816D_BOOT_REG_CKCTL    0xA003  /* 校验控制 */
#define CST816D_BOOT_REG_EXIT     0xA006  /* 退出boot模式 */

#define CST816D_BOOT_ENTER_CMD    0xAB
#define CST816D_BOOT_FLAG_OK      0xC1
#define CST816D_BOOT_WRITE_OK     0x55
#define CST816D_BOOT_EXIT_CMD     0xEE

#define CST816D_FW_PAGE_SIZE      512
#define CST816D_FW_MAX_SIZE       (15 * 1024)

/* ================= 底层I2C辅助 (boot模式使用2字节寄存器地址) ================= */

static int cst816d_boot_write(liot_i2c_channel_e ch, uint8_t slave,
                               uint16_t reg, const uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err;
        err = liot_I2cWrite_MultiByteAddr(ch, slave, reg, 2, data, len);

    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

static int cst816d_boot_read(liot_i2c_channel_e ch, uint8_t slave,
                              uint16_t reg, uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err;
    err = liot_I2cRead_MultiByteAddr(ch, slave, reg, 2, data, len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/* ================= 设备驱动函数 ================= */

static int cst816d_init(liot_tp_handle_t handle)
{
    uint8_t chip_id;
    uint8_t fw_ver;

    /* 读取 Chip ID */
    if (liot_tp_reg_read(handle, CST816D_REG_CHIP_ID, &chip_id, 1) != 0) {
        liot_trace("CST816D read chip_id failed");
        return -1;
    }

    if (chip_id != CST816D_CHIP_ID_VAL) {
        liot_trace("CST816D invalid chip_id: 0x%x (expect 0x%x)",
                         chip_id, CST816D_CHIP_ID_VAL);
        return -1;
    }

    /* 读取固件版本 */
    if (liot_tp_reg_read(handle, CST816D_REG_FW_VER, &fw_ver, 1) == 0) {
        liot_trace("CST816D init ok, chip_id=0x%x, fw_ver=0x%x",
                         chip_id, fw_ver);
    } else {
        liot_trace("CST816D init ok, chip_id=0x%x", chip_id);
    }

    /* 设置正常工作模式 */
    uint8_t mode = CST816D_LPM_ACTIVE;
    liot_tp_reg_write(handle, CST816D_REG_LPM, &mode, 1);

    return 0;
}

static int cst816d_deinit(liot_tp_handle_t handle)
{
    (void)handle;
    return 0;
}

static int cst816d_read_touch(liot_tp_handle_t handle, liot_tp_touch_data_t *data)
{
    /*
     * CST816D 触摸数据格式 (从寄存器0x02读取5字节):
     *   buf[0] = 手指数量 (0=无触摸, 1=触摸中)
     *   buf[1] = X坐标高4位 [3:0] + 事件标志 [7:6]
     *   buf[2] = X坐标低8位
     *   buf[3] = Y坐标高4位 [3:0]
     *   buf[4] = Y坐标低8位
     *
     * 另外寄存器0x00可以读取手势ID
     */
    uint8_t buf[5];
    uint8_t finger;
    uint8_t gesture;

    /* 读取手势ID */
    if (liot_tp_reg_read(handle, CST816D_REG_TP_STATUS, &gesture, 1) != 0) {
        return -1;
    }

    /* 读取触摸数据 */
    if (liot_tp_reg_read(handle, CST816D_REG_TOUCH_NUM, buf, 5) != 0) {
        return -1;
    }

    finger = buf[0];

    if (finger == 0 || finger > CST816D_MAX_POINTS) {
        data->touch_cnt = 0;
        return 0;
    }
    data->touch_cnt = 1;

    uint16_t x = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
    uint16_t y = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];

    data->point[0].id = 0;
    data->point[0].x = x;
    data->point[0].y = y;
    data->point[0].weight = 1;
    data->point[0].timestamp_ms = osKernelGetTickCount();

    /* 事件判断: buf[1] bit[7:6]
     *   0x00 = 按下 (DOWN)
     *   0x40 = 抬起 (UP)
     *   0x80 = 接触/移动 (MOVE)
     */
    uint8_t event_flag = (buf[1] >> 6) & 0x03;
    switch (event_flag) {
    case 0x00:
        data->point[0].event = LIOT_TP_EVT_DOWN;
        break;
    case 0x01:
        data->point[0].event = LIOT_TP_EVT_UP;
        break;
    case 0x02:
        data->point[0].event = LIOT_TP_EVT_MOVE;
        break;
    default:
        /* finger > 0 但没有明确事件标志，默认为MOVE */
        data->point[0].event = LIOT_TP_EVT_MOVE;
        break;
    }

    return 0;
}

static int cst816d_read_gesture(liot_tp_handle_t handle, liot_tp_gesture_data_t *data)
{
    uint8_t gest;

    if (liot_tp_reg_read(handle, CST816D_REG_TP_STATUS, &gest, 1) != 0) {
        return -1;
    }

    data->timestamp_ms = osKernelGetTickCount();

    switch (gest) {
    case CST816D_GEST_UP:          data->gesture = LIOT_TP_GESTURE_UP;         break;
    case CST816D_GEST_DOWN:        data->gesture = LIOT_TP_GESTURE_DOWN;       break;
    case CST816D_GEST_LEFT:        data->gesture = LIOT_TP_GESTURE_LEFT;      break;
    case CST816D_GEST_RIGHT:       data->gesture = LIOT_TP_GESTURE_RIGHT;     break;
    case CST816D_GEST_LONG_PRESS:  data->gesture = LIOT_TP_GESTURE_LONG_PRESS; break;
    case CST816D_GEST_CLICK:
    case CST816D_GEST_DOUBLE_CLICK:
        data->gesture = LIOT_TP_GESTURE_UP;
        break;
    default:
        data->gesture = LIOT_TP_GESTURE_NONE;
        break;
    }

    return 0;
}

static int cst816d_set_threshold(liot_tp_handle_t handle, uint8_t threshold)
{
    (void)handle;
    (void)threshold;
    /* CST816D 不支持直接设置触摸阈值 */
    return 0;
}

static int cst816d_enter_sleep(liot_tp_handle_t handle)
{
    uint8_t mode = CST816D_LPM_SLEEP;
    return liot_tp_reg_write(handle, CST816D_REG_LPM, &mode, 1);
}

static int cst816d_exit_sleep(liot_tp_handle_t handle)
{
    if (!handle) return -1;
        liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    int8_t rst_pin = dev->cfg.rst.pin;
    if (rst_pin < 0) {
        liot_trace("CST816D auto update: i2c_ch or rst_pin not configured");
        return -1;
    }
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(20);
    Liot_SetPinLevel(rst_pin, L_IO_LOW);
    osDelay(20);
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(20);

    return 0;
}

static int cst816d_wakeup(liot_tp_handle_t handle)
{
    /* CST816D 唤醒通常通过RST引脚复位实现，框架层已做硬件复位 */
    uint8_t mode = CST816D_LPM_ACTIVE;
    int ret = liot_tp_reg_write(handle, CST816D_REG_LPM, &mode, 1);
    if (ret != 0) {
        /* 如果I2C写失败(芯片在睡眠中)，依赖硬件复位唤醒 */
        return -1;
    }
    return 0;
}

static int cst816d_reset(liot_tp_handle_t handle)
{
    /* 软复位: CST816D无专用软复位寄存器，通过RST引脚复位由框架处理 */
    (void)handle;
    return 0;
}

/* ================= 固件升级 ================= */

static int cst816d_enter_bootmode(liot_i2c_channel_e ch, int8_t rst_pin)
{
    uint8_t cmd;
    uint8_t flag;
    int retry;

    for (retry = 0; retry < 10; retry++) {
        /* 硬件复位 */
        if (rst_pin >= 0) {
            Liot_SetPinLevel(rst_pin, L_IO_LOW);
            osDelay(10);
            Liot_SetPinLevel(rst_pin, L_IO_HIGH);
            osDelay(5);
        }

        /* 发送进入boot命令 */
        cmd = CST816D_BOOT_ENTER_CMD;
        if (cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                                CST816D_BOOT_REG_CMD, &cmd, 1) != 0) {
            osDelay(2);
            continue;
        }

        /* 读取boot标志 */
        if (cst816d_boot_read(ch, CST816D_BOOT_I2C_ADDR,
                               CST816D_BOOT_REG_FLAG, &flag, 1) != 0) {
            osDelay(2);
            continue;
        }

        if (flag == CST816D_BOOT_FLAG_OK) {
            liot_trace("CST816D enter bootmode ok");
            return 0;
        }

        osDelay(2);
    }

    liot_trace("CST816D enter bootmode failed");
    return -1;
}

static uint32_t cst816d_read_checksum_boot(liot_i2c_channel_e ch)
{
    uint8_t cmd = 0x00;
    uint8_t status;
    uint8_t cksum_buf[2];
    uint32_t checksum = 0;
    int retry;

    /* 启动校验计算 */
    if (cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                            CST816D_BOOT_REG_CKCTL, &cmd, 1) != 0) {
        return 0;
    }

    osDelay(500);

    /* 等待校验完成 */
    for (retry = 0; retry < 100; retry++) {
        if (cst816d_boot_read(ch, CST816D_BOOT_I2C_ADDR,
                               0xA000, &status, 1) != 0) {
            osDelay(10);
            continue;
        }
        if (status == 0x01) {
            break;
        }
        osDelay(10);
    }

    if (status != 0x01) {
        return 0;
    }

    /* 读取校验和 */
    if (cst816d_boot_read(ch, CST816D_BOOT_I2C_ADDR,
                           CST816D_BOOT_REG_CKSUM, cksum_buf, 2) == 0) {
        checksum = cksum_buf[0] | ((uint16_t)cksum_buf[1] << 8);
    }

    return checksum;
}

static int cst816d_update_fw(liot_i2c_channel_e ch, int8_t rst_pin,
                              const uint8_t *fw_data, uint32_t fw_len)
{
    uint16_t start_addr, length, fw_checksum;
    uint32_t chip_checksum;
    uint8_t cmd;
    int retry;

    if (!fw_data || fw_len < 6) {
        return -1;
    }

    /* 解析固件头信息 */
    start_addr = fw_data[0] | ((uint16_t)fw_data[1] << 8);
    length = fw_data[2] | ((uint16_t)fw_data[3] << 8);
    fw_checksum = fw_data[4] | ((uint16_t)fw_data[5] << 8);

    liot_trace("CST816D FW: start=0x%x, len=0x%x, cksum=0x%x",
                     start_addr, length, fw_checksum);

    if (start_addr != 0x0000 || length != 0x3C00) {
        liot_trace("CST816D invalid fw header");
        return -1;
    }

    for (retry = 1; retry <= 3; retry++) {
        if (cst816d_enter_bootmode(ch, rst_pin) != 0) {
            continue;
        }

        /* 读取芯片校验和 */
        chip_checksum = cst816d_read_checksum_boot(ch);
        if (chip_checksum == fw_checksum) {
            liot_trace("CST816D checksum match, no need update");
            break;
        }

        liot_trace("CST816D checksum diff (chip=0x%x, fw=0x%x), updating...",
                         chip_checksum, fw_checksum);

        /* 重新进入boot模式烧写 */
        if (cst816d_enter_bootmode(ch, rst_pin) != 0) {
            continue;
        }

        const uint8_t *src = fw_data + 6;
        uint16_t addr = start_addr;
        uint16_t remaining = length;
        bool write_ok = true;

        liot_trace("CST816D flash write start: total %u bytes", (unsigned)length);

        while (remaining > 0) {
            uint16_t page_len = (remaining > CST816D_FW_PAGE_SIZE) ?
                                 CST816D_FW_PAGE_SIZE : remaining;

            /* 写入地址 */
            uint8_t addr_buf[2] = { addr & 0xFF, (addr >> 8) & 0xFF };
            if (cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                                    CST816D_BOOT_REG_ADDR, addr_buf, 2) != 0) {
                liot_trace("CST816D write addr failed at 0x%x", addr);
                write_ok = false;
                break;
            }

            /* 写入数据 */
            if (cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                                    CST816D_BOOT_REG_DATA, src, page_len) != 0) {
                liot_trace("CST816D write data failed at addr=0x%x", addr);
                write_ok = false;
                break;
            }

            liot_trace("CST816D flash write progress: addr=0x%x written=%u/%u",
                       addr, (unsigned)(addr - start_addr + page_len), (unsigned)length);

            /* 触发烧写 */
            cmd = CST816D_BOOT_EXIT_CMD;  /* 0xEE = 触发写入 */
            if (cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                                    CST816D_BOOT_REG_START, &cmd, 1) != 0) {
                write_ok = false;
                break;
            }

            osDelay(100 * retry);

            /* 等待烧写完成 */
            uint8_t status;
            int wait_cnt;
            for (wait_cnt = 0; wait_cnt < 50; wait_cnt++) {
                osDelay(5);
                if (cst816d_boot_read(ch, CST816D_BOOT_I2C_ADDR,
                                       CST816D_BOOT_REG_STATUS, &status, 1) != 0) {
                    continue;
                }
                if (status == CST816D_BOOT_WRITE_OK) {
                    break;
                }
            }

            if (status != CST816D_BOOT_WRITE_OK) {
                write_ok = false;
                break;
            }

            addr += page_len;
            src += page_len;
            remaining -= page_len;
        }

        if (!write_ok) {
            liot_trace("CST816D FW write failed, retry %d", retry);
            continue;
        }

        liot_trace("CST816D flash write ok");

        /* 验证校验和 */
        chip_checksum = cst816d_read_checksum_boot(ch);
        if (chip_checksum != fw_checksum) {
            liot_trace("CST816D FW verify failed (chip=0x%x, fw=0x%x)",
                             chip_checksum, fw_checksum);
            continue;
        }

        liot_trace("CST816D FW update success");
        break;
    }

    /* 退出boot模式 */
    cmd = 0x00;
    cst816d_boot_write(ch, CST816D_BOOT_I2C_ADDR,
                        CST816D_BOOT_REG_EXIT, &cmd, 1);

    /* 硬件复位恢复正常工作 */
    if (rst_pin >= 0) {
        Liot_SetPinLevel(rst_pin, L_IO_LOW);
        osDelay(10);
        Liot_SetPinLevel(rst_pin, L_IO_HIGH);
        osDelay(50);
    }

    return (retry <= 3) ? 0 : -1;
}

/**
 * @brief 更新CST816D触摸屏固件
 * @param handle 触摸屏设备句柄（未使用）
 * @param fw_data 固件数据指针
 * @param fw_len 固件数据长度（字节）
 * @return 0表示成功，-1表示失败（I2C通道或复位引脚未配置）
 * @note 固件更新前必须通过cst816d_set_i2c_config()配置I2C通道和复位引脚
 * @note 更新过程中芯片将切换到Boot模式(0x6A地址)
 */
static int cst816d_update_firmware(liot_tp_handle_t handle,
                                    const uint8_t *fw_data, uint32_t fw_len)
{
    if (!handle || !fw_data || fw_len < 6) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;
    

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (i2c_ch < 0 || rst_pin < 0) {
        liot_trace("CST816D FW update: i2c_ch or rst_pin not configured");
        return -1;
    }

    return cst816d_update_fw(i2c_ch, rst_pin, fw_data, fw_len);
}

/* ================= 工作模式 ================= */

static int cst816d_set_work_mode(liot_tp_handle_t handle, liot_tp_work_mode_e mode)
{
    uint8_t reg_val;

    switch (mode) {
    case LIOT_TP_MODE_NORMAL:
        reg_val = CST816D_LPM_ACTIVE;
        break;
    case LIOT_TP_MODE_GESTURE:
        reg_val = CST816D_LPM_GESTURE;
        break;
    case LIOT_TP_MODE_LOW_POWER:
        reg_val = CST816D_LPM_MONITOR;
        break;
    case LIOT_TP_MODE_DEEP_SLEEP:
        reg_val = CST816D_LPM_SLEEP;
        break;
    default:
        return -1;
    }

    return liot_tp_reg_write(handle, CST816D_REG_LPM, &reg_val, 1);
}

/* ================= IC信息 ================= */

static int cst816d_get_ic_info(liot_tp_handle_t handle, uint8_t *buf, uint16_t buf_len)
{
    if (!buf || buf_len < 4) return -1;

    uint8_t info[4] = {0};

    /* 读取 Chip ID */
    if (liot_tp_reg_read(handle, CST816D_REG_CHIP_ID, &info[0], 1) != 0) {
        return -1;
    }

    /* 读取固件版本 */
    if (liot_tp_reg_read(handle, CST816D_REG_FW_VER, &info[1], 1) != 0) {
        info[1] = 0;
    }

    /* 读取项目ID */
    if (liot_tp_reg_read(handle, CST816D_REG_PROJ_ID, &info[2], 1) != 0) {
        info[2] = 0;
    }

    /* 当前低功耗模式 */
    if (liot_tp_reg_read(handle, CST816D_REG_LPM, &info[3], 1) != 0) {
        info[3] = 0;
    }

    uint16_t copy_len = (buf_len < 4) ? buf_len : 4;
    memcpy(buf, info, copy_len);

    liot_trace("CST816D IC info: chip_id=0x%x, fw_ver=0x%x, proj_id=0x%x, lpm=0x%x",
                     info[0], info[1], info[2], info[3]);

    return 0;
}

/* ================= 设备实例导出 ================= */

liot_tp_sensor_t g_liot_tp_cst816d = {
    .chip_id         = CST816D_CHIP_ID_VAL,
    .max_points      = CST816D_MAX_POINTS,
    .width           = 128,
    .height          = 128,
    .gesture_support = true,
    .func = {
        .init           = cst816d_init,
        .deinit         = cst816d_deinit,
        .read_touch     = cst816d_read_touch,
        .read_gesture   = cst816d_read_gesture,
        .set_threshold  = cst816d_set_threshold,
        .enter_sleep    = cst816d_enter_sleep,
        .exit_sleep     = cst816d_exit_sleep,
        .wakeup         = cst816d_wakeup,
        .reset          = cst816d_reset,
        .update_firmware = cst816d_update_firmware,
        .update_firmware_auto = cst816d_auto_update_firmware,
        .set_work_mode  = cst816d_set_work_mode,
        .get_ic_info    = cst816d_get_ic_info,
    },
};


/* ================= 自动固件升级 ================= */

/**
 * @brief 使用内置固件自动升级CST816D
 *        固件文件通过 liot_tp_cst816d_fw.h 选择（由 LIOT_TP_CST816D_FW_VER 宏控制）
 *        升级前需先调用 cst816d_set_i2c_config() 配置硬件信息
 * @param handle TP句柄
 * @return 0成功, -1失败
 */
int cst816d_auto_update_firmware(liot_tp_handle_t handle)
{
    if (!handle) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (i2c_ch < 0 || rst_pin < 0) {
        liot_trace("CST816D auto update: i2c_ch or rst_pin not configured");
        return -1;
    }

    liot_trace("CST816D auto update with built-in FW (ver=%d)",
                     LIOT_TP_CST816D_FW_VER);

    return cst816d_update_fw(i2c_ch, rst_pin,
                              app_bin, sizeof(app_bin));
}
