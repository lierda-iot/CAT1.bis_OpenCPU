/**
 * @File Name: liot_tpDev_BL6178.c
 * @brief BL6178 TP sensor driver (based on liot_tp framework)
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_tp.h"
#include "liot_os.h"
#include "liot_log.h"
#include "bl_fw.h"
#include <string.h>

/* ================= BL6178 寄存器定义 ================= */

#define BL6178_REG_WORK_MODE        0x00
#define BL6178_REG_TD_STATUS        0x01
#define BL6178_REG_CHECKSUM         0x3F
#define BL6178_REG_CHECKSUM_CAL     0x8A
#define BL6178_REG_AC               0x8B
#define BL6178_REG_RESOLUTION       0x98
#define BL6178_REG_LPM              0xA5
#define BL6178_REG_CHIP_ID          0xB8
#define BL6178_REG_FWVER_PJ_ID     0xB6
#define BL6178_REG_PRJ_INFO         0xB4
#define BL6178_REG_PRJ_ID           0xB5
#define BL6178_REG_CHIP_ID2         0xE7

/* I2C地址 (8bit) */
#define BL6178_I2C_ADDR             (0x2C)
#define BL6178_FLASH_I2C_ADDR       (0x2C)

/* Chip ID */
#define BL6178_CHIP_ID_VAL          0x23

/* 触摸点数最大值 */
#define BL6178_MAX_POINTS           1

/* 触摸数据格式 */
#define BL6178_TOUCH_BUF_HEAD_LEN   2
#define BL6178_TOUCH_ONE_POINT_LEN  6

/* 睡眠命令 */
#define BL6178_SLEEP_CMD_REG        0xA5
#define BL6178_SLEEP_CMD_VAL        0x03

/* Flash相关 */
#define BL6178_FLASH_I2C_ADDR_8BIT  (0x2C << 1)
#define BL6178_FLASH_WSIZE          128
#define BL6178_FLASH_RSIZE          64
#define BL6178_MAX_FLASH_SIZE       0x8000
#define BL6178_FLASH_PAGE_SIZE      512
#define BL6178_ARGUMENT_BASE_OFFSET 0x200
#define BL6178_ARGUMENT_FLASH_SIZE  0x400
#define BL6178_FWVER_MAIN_OFFSET    0x2A
#define BL6178_FWVER_ARGU_OFFSET    0x2B
#define BL6178_FWVER_PJ_ID_OFFSET   0x2C
#define BL6178_PROJECT_ID_OFFSET    0x2C
#define BL6178_COB_ID_OFFSET        0x10
#define BL6178_VERTIFY_START_OFFSET 0x3FC
#define BL6178_VERTIFY_END_OFFSET   0x3FD
#define BL6178_FW_CHECKSUM_DELAY    250
#define BL6178_PROTECT_REG          0xB0
#define BL6178_COB_ID_REG           0xB9

/* 升级类型 */
#define BL6178_NONE_UPDATE          0
#define BL6178_FW_ARG_UPDATE        1

/* 升级文件类型 */
#define BL6178_HEADER_FILE_UPDATE   0
#define BL6178_BIN_FILE_UPDATE      1

/* ARGU标记 */
static const uint8_t BL6178_ARGU_MARK[] = "bl_tp_cfg_mark";

/* Flash命令 */
#define BL6178_CMD_ERASE_SECTOR     0x06
#define BL6178_CMD_ERASE_ALL        0x09
#define BL6178_CMD_RW_REGISTER      0x0A
#define BL6178_CMD_READ_MAIN        0x0D
#define BL6178_CMD_WRITE_MAIN       0x0F
#define BL6178_CMD_WRITE_RAM        0x11
#define BL6178_CMD_READ_RAM         0x12

/* 升级重试次数 */
#define BL6178_UPGRADE_RETRY_TIMES  3

int bl6178_auto_update_firmware(liot_tp_handle_t handle);

/* ================= 底层I2C辅助 ================= */

static int bl6178_i2c_write(liot_i2c_channel_e ch, uint8_t slave,
                             uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err = liot_I2cWrite(ch, slave, data[0], &data[1], len - 1);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

static int bl6178_i2c_read(liot_i2c_channel_e ch, uint8_t slave,
                            uint8_t reg, uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err = liot_I2cRead(ch, slave, reg, data, len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

static int bl6178_i2c_write_reg(liot_i2c_channel_e ch, uint8_t slave,
                                 uint8_t reg, uint8_t *data, uint16_t len)
{
    liot_errcode_i2c_e err = liot_I2cWrite(ch, slave, reg, data, len);
    return (err == LIOT_I2C_SUCCESS) ? 0 : -1;
}

/* ================= 硬件操作辅助 ================= */

static void bl6178_hard_reset(int8_t rst_pin)
{
    if (rst_pin < 0) return;
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(20);
    Liot_SetPinLevel(rst_pin, L_IO_LOW);
    osDelay(20);
    Liot_SetPinLevel(rst_pin, L_IO_HIGH);
    osDelay(20);
}

/* ================= 设备驱动函数 ================= */

static int bl6178_init(liot_tp_handle_t handle)
{
    uint8_t chip_id = 0;

    uint8_t cmd[3] = {0};
    cmd[0] = BL6178_CMD_RW_REGISTER;
    cmd[1] = ~cmd[0];
    cmd[2] = BL6178_REG_CHIP_ID;

    liot_tp_reg_write(handle, cmd[0], &cmd[1], 2);

    if (liot_tp_reg_read(handle, BL6178_REG_WORK_MODE, &chip_id, 1) != 0) {
        liot_trace("BL6178 read chip_id failed");
        return -1;
    }

    if (chip_id != BL6178_CHIP_ID_VAL) {
        liot_trace("BL6178 invalid chip_id: 0x%x (expect 0x%x)",
                    chip_id, BL6178_CHIP_ID_VAL);
        // return -1;
    }

    uint8_t fw_ver[3] = {0};
    if (liot_tp_reg_read(handle, BL6178_REG_FWVER_PJ_ID, fw_ver, 3) == 0) {
        liot_trace("BL6178 init ok, chip_id=0x%x, fw_main=0x%x, fw_argu=0x%x, prj_id=0x%x",
                    chip_id, fw_ver[0], fw_ver[1], fw_ver[2]);
    } else {
        liot_trace("BL6178 init ok, chip_id=0x%x", chip_id);
    }

    return 0;
}

static int bl6178_deinit(liot_tp_handle_t handle)
{
    (void)handle;
    return 0;
}
void bl_get_data_user(uint8_t *data);
static int bl6178_read_touch(liot_tp_handle_t handle, liot_tp_touch_data_t *data)
{
    int ret;
    uint8_t buf[2 + 6 * BL6178_MAX_POINTS];  // 最大支持5点：2字节头 + 6*5字节数据
    uint8_t read_cmd = 0x01; // 从0x01寄存器开始读取
    uint8_t point_num;
    int i;
    
    if (data == NULL) {
        return -1;
    }
    
    // 清空数据
    memset(data, 0, sizeof(liot_tp_touch_data_t));
    
    // 读取触摸数据：从0x01寄存器开始读取 2 + 6*MAX_POINTS 个字节
    ret = liot_tp_reg_write(handle, 0x01, &read_cmd, 1);
    if (ret < 0) {
        return ret;
    }
    
    ret = liot_tp_reg_read(handle, 0x01, buf, sizeof(buf));
    if (ret < 0) {
        return ret;
    }
    // bl_get_data_user(buf);
    
    // liot_trace("BL6178 touch data: %02x %02x %02x %02x %02x %02x %02x %02x",
    //     buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

    // buf[0] = reg 0x01: TD_STATUS, 低4位为触摸点个数
    // buf[1] = reg 0x02: 手势码（暂不使用）
    // buf[2..] = reg 0x03起: 触摸点数据（每点6字节）
    point_num = buf[0] & 0x0F;
    if (point_num > 5) {
        point_num = 5;  // 限制最大5点
    }
    
    data->touch_cnt = point_num;
    
    // 解析每个触摸点数据，每个点占用6个字节
    for (i = 0; i < point_num; i++) {
        int offset = 2 + i * 6;  // 从0x03开始每个点6字节
        // liot_trace("buf[%d] = %X %d", offset, buf[offset], buf[offset]);
        // 0x03: EventFlag (触摸状态) + XH (X坐标高6位)
        data->point[i].event = (buf[offset] >> 6) & 0x03;
        data->point[i].x = (buf[offset] & 0x3F) << 8;  // 高6位
        
        // 0x04: XL (X坐标低8位)
        data->point[i].x |= buf[offset + 1];
        
        // 0x05: TouchID + YH (Y坐标高4位)
        data->point[i].id = (buf[offset + 2] >> 4) & 0x0F;
        data->point[i].y = (buf[offset + 2] & 0x0F) << 8;  // 高4位
        
        // 0x06: YL (Y坐标低8位)
        data->point[i].y |= buf[offset + 3];
        
        // 0x07, 0x08: Reserved (保留)
        data->point[i].weight = 0;  // 默认无压力数据
        
        // 将事件码转换为标准事件
        switch (data->point[i].event) {
            case 0: // Down
                data->point[i].event = LIOT_TP_EVT_DOWN;
                break;
            case 1: // Up
                data->point[i].event = LIOT_TP_EVT_UP;
                break;
            case 2: // Contact or Move
                data->point[i].event = LIOT_TP_EVT_MOVE;
                break;
            default: // Reserved
                data->point[i].event = LIOT_TP_EVT_NONE;
                break;
        }

        // liot_trace("BL6178 touch event = %d", data->point[i].event);
    }
    
    return 0;
}

static int bl6178_read_gesture(liot_tp_handle_t handle, liot_tp_gesture_data_t *data)
{
    int ret;
    uint8_t buf[2];
    uint8_t read_cmd = 0x01;
    
    if (data == NULL) {
        return -1;
    }
    
    // 清空数据
    memset(data, 0, sizeof(liot_tp_gesture_data_t));
    
    // 读取手势码：从0x01寄存器读取1字节手势码
    ret = liot_tp_reg_write(handle, 0x01, &read_cmd, 1);
    if (ret < 0) {
        return ret;
    }
    
    ret = liot_tp_reg_read(handle, 0x01, buf, 1);
    if (ret < 0) {
        return ret;
    }
    
    // 0x01寄存器：Gesture Code (手势码)
    switch (buf[0]) {
        case 0x00:
            data->gesture = LIOT_TP_GESTURE_NONE;
            break;
        case 0x01:
            data->gesture = LIOT_TP_GESTURE_UP;
            break;
        case 0x02:
            data->gesture = LIOT_TP_GESTURE_DOWN;
            break;
        case 0x03:
            data->gesture = LIOT_TP_GESTURE_LEFT;
            break;
        case 0x04:
            data->gesture = LIOT_TP_GESTURE_RIGHT;
            break;
        case 0x05:
            data->gesture = LIOT_TP_GESTURE_ZOOM_IN;
            break;
        case 0x06:
            data->gesture = LIOT_TP_GESTURE_ZOOM_OUT;
            break;
        case 0x07:
            data->gesture = LIOT_TP_GESTURE_LONG_PRESS;
            break;
        default:
            data->gesture = LIOT_TP_GESTURE_NONE;
            break;
    }
    
    return 0;
}

static int bl6178_set_threshold(liot_tp_handle_t handle, uint8_t threshold)
{
    (void)handle;
    (void)threshold;
    return 0;
}

static int bl6178_enter_sleep(liot_tp_handle_t handle)
{
    uint8_t cmd = BL6178_SLEEP_CMD_VAL;
    return liot_tp_reg_write(handle, BL6178_REG_LPM, &cmd, 1);
}

static int bl6178_exit_sleep(liot_tp_handle_t handle)
{
    if (!handle) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    int8_t rst_pin = dev->cfg.rst.pin;
    if (rst_pin < 0) {
        liot_trace("BL6178 exit_sleep: rst_pin not configured");
        return -1;
    }

    bl6178_hard_reset(rst_pin);
    return 0;
}

static int bl6178_wakeup(liot_tp_handle_t handle)
{
    uint8_t mode = 0x00;
    int ret = liot_tp_reg_write(handle, BL6178_REG_LPM, &mode, 1);
    if (ret != 0) {
        return -1;
    }
    return 0;
}

static int bl6178_reset(liot_tp_handle_t handle)
{
    (void)handle;
    return 0;
}

/* ================= 固件升级 ================= */

static int bl6178_enter_update_mode(liot_i2c_channel_e ch, int8_t rst_pin)
{
    uint8_t cmd[16];
    uint8_t pattern[4] = {0x63, 0x75, 0x69, 0x33};

    for (int i = 0; i < 4; i++) {
        cmd[4 * i]     = pattern[0];
        cmd[4 * i + 1] = pattern[1];
        cmd[4 * i + 2] = pattern[2];
        cmd[4 * i + 3] = pattern[3];
    }

    if (bl6178_i2c_write(ch, BL6178_I2C_ADDR, cmd, sizeof(cmd)) != 0) {
        liot_trace("BL6178 enter update mode failed");
        return -1;
    }

    osDelay(50);
    return 0;
}

static void bl6178_exit_update_mode(liot_i2c_channel_e ch, int8_t rst_pin)
{
    uint8_t cmd[2] = {0x5A, 0xA5};
    osDelay(20);
    bl6178_i2c_write(ch, BL6178_I2C_ADDR, cmd, sizeof(cmd));
    osDelay(20);

    if (rst_pin >= 0) {
        bl6178_hard_reset(rst_pin);
        osDelay(30);
    }
}

static int bl6178_get_chip_id(liot_i2c_channel_e ch, uint8_t *chip_id)
{
    uint8_t cmd[3];

    cmd[0] = BL6178_CMD_RW_REGISTER;
    cmd[1] = ~cmd[0];
    cmd[2] = BL6178_REG_CHIP_ID2;

    if (bl6178_i2c_write(ch, BL6178_FLASH_I2C_ADDR, cmd, 3) != 0) {
        return -1;
    }

    if (bl6178_i2c_read(ch, BL6178_FLASH_I2C_ADDR, 0, chip_id, 1) != 0) {
        return -1;
    }

    return 0;
}

static int bl6178_flash_erase_all(liot_i2c_channel_e ch)
{
    uint8_t cmd[2];

    cmd[0] = BL6178_CMD_ERASE_ALL;
    cmd[1] = ~cmd[0];

    if (bl6178_i2c_write(ch, BL6178_FLASH_I2C_ADDR, cmd, 2) != 0) {
        liot_trace("BL6178 flash erase failed");
        return -1;
    }

    osDelay(BL6178_FW_CHECKSUM_DELAY);
    return 0;
}

static int bl6178_flash_write_page(liot_i2c_channel_e ch,
                                    uint16_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t cmd[6 + BL6178_FLASH_WSIZE];
    uint16_t offset = 0;

    if (!len) {
        return -1;
    }

    while (offset < len) {
        uint16_t write_len = (len - offset > BL6178_FLASH_WSIZE) ?
                              BL6178_FLASH_WSIZE : (len - offset);
        uint16_t flash_start_addr = addr + offset;
        uint16_t flash_end_addr = flash_start_addr + write_len - 1;

        if (flash_end_addr >= BL6178_MAX_FLASH_SIZE) {
            liot_trace("BL6178 flash write addr overflow");
            return -1;
        }

        cmd[0] = BL6178_CMD_WRITE_MAIN;
        cmd[1] = ~cmd[0];
        cmd[2] = (uint8_t)(flash_start_addr >> 8);
        cmd[3] = (uint8_t)(flash_start_addr & 0xFF);
        cmd[4] = (uint8_t)(flash_end_addr >> 8);
        cmd[5] = (uint8_t)(flash_end_addr & 0xFF);
        memcpy(&cmd[6], &data[offset], write_len);

        if (bl6178_i2c_write(ch, BL6178_FLASH_I2C_ADDR, cmd, write_len + 6) != 0) {
            liot_trace("BL6178 flash write failed at addr 0x%x", flash_start_addr);
            return -1;
        }
        liot_trace("BL6178 flash write success at addr 0x%x 0x%x %d", flash_start_addr, flash_end_addr, write_len);
        osDelay(5);
        offset += write_len;
    }

    return 0;
}

static int bl6178_flash_read(liot_i2c_channel_e ch,
                              uint16_t addr, uint8_t *data, uint16_t len)
{
    uint8_t cmd[6];
    uint16_t offset = 0;

    while (offset < len) {
        uint16_t read_len = (len - offset > BL6178_FLASH_RSIZE) ?
                             BL6178_FLASH_RSIZE : (len - offset);
        uint16_t flash_start_addr = addr + offset;
        uint16_t flash_end_addr = flash_start_addr + read_len - 1;

        cmd[0] = BL6178_CMD_READ_MAIN;
        cmd[1] = ~cmd[0];
        cmd[2] = (uint8_t)(flash_start_addr >> 8);
        cmd[3] = (uint8_t)(flash_start_addr & 0xFF);
        cmd[4] = (uint8_t)(flash_end_addr >> 8);
        cmd[5] = (uint8_t)(flash_end_addr & 0xFF);

        if (bl6178_i2c_write(ch, BL6178_FLASH_I2C_ADDR, cmd, 6) != 0) {
            liot_trace("BL6178 flash read write cmd error");
            return -1;
        }

        if (bl6178_i2c_read(ch, BL6178_FLASH_I2C_ADDR, 0, &data[offset], read_len) != 0) {
            liot_trace("BL6178 flash read data error");
            return -1;
        }

        offset += read_len;
    }

    return 0;
}

static uint16_t bl6178_calc_bin_checksum(const uint8_t *fw_data, uint32_t fw_len,
                                          uint32_t specify_arg_addr)
{
    uint32_t i;
    uint32_t temp_checksum = 0;

    for (i = 0; i < BL6178_ARGUMENT_BASE_OFFSET; i++) {
        temp_checksum += fw_data[i];
    }
    for (i = specify_arg_addr; i < specify_arg_addr + BL6178_VERTIFY_START_OFFSET; i++) {
        temp_checksum += fw_data[i];
    }
    for (i = specify_arg_addr + BL6178_VERTIFY_START_OFFSET;
         i < specify_arg_addr + BL6178_VERTIFY_START_OFFSET + 4; i++) {
        temp_checksum += fw_data[i];
    }
    for (i = BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET + 4; i < fw_len; i++) {
        temp_checksum += fw_data[i];
    }
    for (i = fw_len; i < BL6178_MAX_FLASH_SIZE; i++) {
        temp_checksum += 0xFF;
    }

    return (uint16_t)(temp_checksum & 0xFFFF);
}

static int bl6178_get_fw_checksum(liot_i2c_channel_e ch, uint16_t *fw_checksum)
{
    uint8_t cmd[2];
    uint8_t buf[3] = {0};
    uint8_t checksum_ready = 0;
    int retry = 5;

    cmd[0] = BL6178_REG_CHECKSUM_CAL;
    cmd[1] = BL6178_REG_CHECKSUM;
    if (bl6178_i2c_write_reg(ch, BL6178_I2C_ADDR, cmd[0], &cmd[1], 1) != 0) {
        return -1;
    }

    osDelay(BL6178_FW_CHECKSUM_DELAY);

    if (bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_REG_CHECKSUM, buf, 3) != 0) {
        liot_trace("BL6178 read checksum error");
        return -1;
    }

    checksum_ready = buf[0];

    while ((retry--) && (checksum_ready != 0x01)) {
        osDelay(50);
        if (bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_REG_CHECKSUM, buf, 3) != 0) {
            liot_trace("BL6178 read checksum error");
            return -1;
        }
        checksum_ready = buf[0];
    }

    if (checksum_ready != 0x01) {
        liot_trace("BL6178 checksum not ready");
        return -1;
    }

    *fw_checksum = (buf[1] << 8) + buf[2];
    return 0;
}

static int bl6178_get_protect_flag(liot_i2c_channel_e ch)
{
    uint8_t protect_flag = 0;
    if (bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_PROTECT_REG, &protect_flag, 1) != 0) {
        return 0;
    }
    return (protect_flag == 0x55) ? 1 : 0;
}

static int bl6178_get_cob_id(liot_i2c_channel_e ch, uint8_t *cob_id)
{
    return bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_COB_ID_REG, cob_id, 6);
}

static uint8_t bl6178_is_cob_project(const uint8_t *fw_data, uint32_t fw_size)
{
    uint8_t argu_key[4] = {0xAA, 0x55, 0x09, 0x09};

    if (fw_size % BL6178_FLASH_PAGE_SIZE) {
        return 0;
    }
    if (memcmp(argu_key, fw_data + fw_size - 4, 4) != 0) {
        return 0;
    }
    return 1;
}

static uint8_t bl6178_get_argument_count(const uint8_t *fw_data, uint32_t fw_size)
{
    uint8_t count = 0;
    uint32_t addr = fw_size;

    while (addr > (BL6178_ARGUMENT_BASE_OFFSET + BL6178_ARGUMENT_FLASH_SIZE)) {
        addr -= BL6178_ARGUMENT_FLASH_SIZE;
        if (memcmp(fw_data + addr, BL6178_ARGU_MARK, sizeof(BL6178_ARGU_MARK) - 1) != 0) {
            break;
        }
        count++;
    }
    liot_trace("BL6178 argument count = %d", count);
    return count;
}

static uint32_t bl6178_get_down_size(const uint8_t *fw_data, uint32_t fw_size,
                                      uint8_t *argu_cnt)
{
    *argu_cnt = bl6178_get_argument_count(fw_data, fw_size);
    return fw_size - (*argu_cnt) * BL6178_ARGUMENT_FLASH_SIZE - BL6178_FLASH_PAGE_SIZE;
}

static int bl6178_get_specific_argument(uint32_t *argu_offset, uint8_t *cob_id,
                                         const uint8_t *fw_data, uint32_t fw_size,
                                         uint8_t argu_count)
{
    uint8_t convert_cob_id[12] = {0};
    uint8_t i;
    uint32_t cob_argu_addr = fw_size - argu_count * BL6178_ARGUMENT_FLASH_SIZE;

    for (i = 0; i < sizeof(convert_cob_id); i++) {
        if (i % 2) {
            convert_cob_id[i] = cob_id[i / 2] & 0x0F;
        } else {
            convert_cob_id[i] = (cob_id[i / 2] & 0xF0) >> 4;
        }
        if (convert_cob_id[i] < 10) {
            convert_cob_id[i] = '0' + convert_cob_id[i];
        } else {
            convert_cob_id[i] = 'a' + convert_cob_id[i] - 10;
        }
    }

    for (i = 0; i < argu_count; i++) {
        if (memcmp(convert_cob_id,
                   fw_data + cob_argu_addr + i * BL6178_ARGUMENT_FLASH_SIZE + BL6178_COB_ID_OFFSET,
                   12) == 0) {
            *argu_offset = cob_argu_addr + i * BL6178_ARGUMENT_FLASH_SIZE;
            liot_trace("BL6178 found specific argu at offset 0x%x", (unsigned)*argu_offset);
            return 0;
        }
    }

    *argu_offset = BL6178_ARGUMENT_BASE_OFFSET;
    liot_trace("BL6178 specific argu not found, use default");
    return -1;
}

static int bl6178_flash_write_verify_bytes(liot_i2c_channel_e ch,
                                            const uint8_t *fw_data)
{
    uint8_t verify[2];
    uint8_t verify_read[2];
    uint16_t addr = BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET;
    int cnt = 0;

    verify[0] = fw_data[addr];
    verify[1] = fw_data[addr + 1];
    liot_trace("BL6178 write verify bytes: %02x %02x", verify[0], verify[1]);

    while (cnt < 3) {
        cnt++;
        if (bl6178_flash_write_page(ch, addr, verify, 2) != 0) {
            continue;
        }
        osDelay(10);
        if (bl6178_flash_read(ch, addr, verify_read, 2) != 0) {
            continue;
        }
        if (memcmp(verify, verify_read, 2) == 0) {
            return 0;
        }
    }
    return -1;
}

static int bl6178_flash_read_verify_bytes(liot_i2c_channel_e ch,
                                           const uint8_t *fw_data)
{
    uint8_t verify[2];
    uint8_t verify_read[2];
    uint16_t addr = BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET;
    int cnt = 0;

    verify[0] = fw_data[addr];
    verify[1] = fw_data[addr + 1];

    while (cnt < 3) {
        cnt++;
        if (bl6178_flash_read(ch, addr, verify_read, 2) != 0) {
            continue;
        }
        if (memcmp(verify, verify_read, 2) == 0) {
            return 0;
        }
    }
    return -1;
}

static int bl6178_download_fw(liot_i2c_channel_e ch,
                               const uint8_t *fw_data, uint32_t fw_len,
                               uint32_t specify_arg_addr)
{
    uint32_t i;
    uint16_t len, size;
    uint8_t verify_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    verify_buf[2] = fw_data[BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET + 2];
    verify_buf[3] = fw_data[BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET + 3];
    liot_trace("BL6178 download: verify_buf = %02x %02x %02x %02x",
               verify_buf[0], verify_buf[1], verify_buf[2], verify_buf[3]);

    if (bl6178_flash_erase_all(ch) != 0) {
        liot_trace("BL6178 erase flash fail");
        return -1;
    }
    osDelay(50);

    for (i = 0; i < BL6178_ARGUMENT_BASE_OFFSET;) {
        size = BL6178_ARGUMENT_BASE_OFFSET - i;
        len = (size > BL6178_FLASH_WSIZE) ? BL6178_FLASH_WSIZE : size;
        if (bl6178_flash_write_page(ch, (uint16_t)i, &fw_data[i], len) != 0)
            return -1;
        i += len;
    }

    for (i = BL6178_ARGUMENT_BASE_OFFSET;
         i < (BL6178_VERTIFY_START_OFFSET + BL6178_ARGUMENT_BASE_OFFSET);) {
        size = BL6178_VERTIFY_START_OFFSET + BL6178_ARGUMENT_BASE_OFFSET - i;
        len = (size > BL6178_FLASH_WSIZE) ? BL6178_FLASH_WSIZE : size;
        if (bl6178_flash_write_page(ch, (uint16_t)i,
            &fw_data[i + specify_arg_addr - BL6178_ARGUMENT_BASE_OFFSET], len) != 0)
            return -1;
        i += len;
    }

    uint16_t verify_addr = BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET;
    if (bl6178_flash_write_page(ch, verify_addr, verify_buf, 4) != 0)
        return -1;

    for (i = (BL6178_ARGUMENT_BASE_OFFSET + BL6178_VERTIFY_START_OFFSET + 4); i < fw_len;) {
        size = fw_len - i;
        len = (size > BL6178_FLASH_WSIZE) ? BL6178_FLASH_WSIZE : size;
        if (bl6178_flash_write_page(ch, (uint16_t)i, &fw_data[i], len) != 0)
            return -1;
        i += len;
    }

    return 0;
}

static int bl6178_update_flash(liot_i2c_channel_e ch, int8_t rst_pin,
                                const uint8_t *fw_data, uint32_t fw_len,
                                uint32_t specify_arg_addr)
{
    int retry = BL6178_UPGRADE_RETRY_TIMES;
    int ret;
    uint16_t fw_checksum = 0;
    uint16_t fw_bin_checksum = 0;
    uint8_t chip_id = 0;

    fw_bin_checksum = bl6178_calc_bin_checksum(fw_data, fw_len, specify_arg_addr);
    liot_trace("BL6178 fw_bin_checksum = 0x%04x", fw_bin_checksum);

    while (retry--) {
        bl6178_hard_reset(rst_pin);
        osDelay(20);

        if (bl6178_enter_update_mode(ch, rst_pin) != 0) {
            liot_trace("BL6178 enter update mode failed, retry %d", retry);
            continue;
        }

        if (bl6178_get_chip_id(ch, &chip_id) != 0 || chip_id != BL6178_CHIP_ID_VAL) {
            liot_trace("BL6178 chip_id mismatch: 0x%x, retry %d", chip_id, retry);
            bl6178_exit_update_mode(ch, rst_pin);
            continue;
        }

        ret = bl6178_download_fw(ch, fw_data, fw_len, specify_arg_addr);
        if (ret < 0) {
            liot_trace("BL6178 download fw failed, retry %d", retry);
            bl6178_exit_update_mode(ch, rst_pin);
            continue;
        }

        bl6178_exit_update_mode(ch, rst_pin);
        osDelay(50);

        ret = bl6178_get_fw_checksum(ch, &fw_checksum);
        fw_checksum -= 0xFF;
        liot_trace("BL6178 fw_checksum=0x%04x, fw_bin_checksum=0x%04x",
                   fw_checksum, fw_bin_checksum);

        if (ret < 0 || fw_checksum != fw_bin_checksum) {
            liot_trace("BL6178 checksum mismatch, retry %d", retry);
            continue;
        }

        if (bl6178_flash_write_verify_bytes(ch, fw_data) != 0) {
            liot_trace("BL6178 write verify bytes failed, retry %d", retry);
            continue;
        }

        liot_trace("BL6178 update flash success");
        return 0;
    }

    liot_trace("BL6178 update flash failed");
    return -1;
}

static int bl6178_update_fw(liot_i2c_channel_e ch, int8_t rst_pin,
                             uint8_t file_type,
                             const uint8_t *fw_data, uint32_t fw_size)
{
    uint8_t fw_arg_prj_id[3] = {0};
    int ret = 0;
    uint8_t is_blank = 0;
    uint16_t fw_checksum = 0;
    uint16_t fw_bin_checksum = 0;
    uint8_t update_type = BL6178_NONE_UPDATE;
    uint32_t down_size = 0;
    uint8_t cob_id[6] = {0};
    uint32_t specific_argu_addr = BL6178_ARGUMENT_BASE_OFFSET;
    uint8_t argu_count = 0;
    uint8_t is_cob_prj = 0;

    if (!fw_data || fw_size == 0 || fw_size > BL6178_MAX_FLASH_SIZE) {
        return -1;
    }

    liot_trace("BL6178 bl6178_update_fw start");

    if (bl6178_get_protect_flag(ch) && (file_type == BL6178_HEADER_FILE_UPDATE)) {
        liot_trace("BL6178 protect flag set, skip update");
        return 0;
    }

    is_cob_prj = bl6178_is_cob_project(fw_data, fw_size);
    liot_trace("BL6178 is_cob_prj = %d", is_cob_prj);

    osDelay(5);
    ret = bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_REG_FWVER_PJ_ID, fw_arg_prj_id, 3);
    if (ret != 0
        || (file_type == BL6178_BIN_FILE_UPDATE)
        || (ret == 0 && fw_arg_prj_id[0] == 0x00)
        || (ret == 0 && fw_arg_prj_id[0] == 0xFF)
        || (ret == 0 && fw_arg_prj_id[0] == BL6178_REG_FWVER_PJ_ID)
        || (bl6178_flash_read_verify_bytes(ch, fw_data) < 0)) {
        is_blank = 1;
        liot_trace("BL6178 blank IC: ret=%d, fwVer=0x%x, argVer=0x%x, prjId=0x%x",
                   ret, fw_arg_prj_id[0], fw_arg_prj_id[1], fw_arg_prj_id[2]);
    } else {
        is_blank = 0;
        liot_trace("BL6178 IC info: fwVer=0x%x, argVer=0x%x, prjId=0x%x",
                   fw_arg_prj_id[0], fw_arg_prj_id[1], fw_arg_prj_id[2]);
    }

    if (is_cob_prj) {
        down_size = bl6178_get_down_size(fw_data, fw_size, &argu_count);
        liot_trace("BL6178 COB: down_size=0x%x, argu_count=%d",
                   (unsigned)down_size, argu_count);
    } else {
        down_size = fw_size;
    }

UPDATE_SECOND_FOR_COB:
    if (!is_blank) {
        bl6178_i2c_read(ch, BL6178_I2C_ADDR, BL6178_REG_FWVER_PJ_ID, fw_arg_prj_id, 3);
    }

    if (is_cob_prj && !is_blank) {
        osDelay(50);
        ret = bl6178_get_cob_id(ch, cob_id);
        if (ret < 0) {
            liot_trace("BL6178 get cob_id error");
            return -1;
        }
        liot_trace("BL6178 cob_id: %02x %02x %02x %02x %02x %02x",
                   cob_id[0], cob_id[1], cob_id[2], cob_id[3], cob_id[4], cob_id[5]);

        ret = bl6178_get_specific_argument(&specific_argu_addr, cob_id,
                                            fw_data, fw_size, argu_count);
        if (ret < 0) {
            liot_trace("BL6178 specific argu not found, use default");
        }
        liot_trace("BL6178 specific_argu_addr = 0x%x", (unsigned)specific_argu_addr);
    }

    if (!is_blank) {
        fw_bin_checksum = bl6178_calc_bin_checksum(fw_data, down_size, specific_argu_addr);
        ret = bl6178_get_fw_checksum(ch, &fw_checksum);
        if (ret < 0 || fw_checksum != fw_bin_checksum) {
            liot_trace("BL6178 checksum read fail or mismatch: ic=0x%04x, bin=0x%04x",
                       fw_checksum, fw_bin_checksum);
            fw_checksum = 0;
        }
        liot_trace("BL6178 fw_checksum=0x%04x, fw_bin_checksum=0x%04x",
                   fw_checksum, fw_bin_checksum);
    }

    if (is_blank) {
        update_type = BL6178_FW_ARG_UPDATE;
        liot_trace("BL6178 update case: blank IC -> FW_ARG_UPDATE");
    } else {
        if (fw_arg_prj_id[0] != fw_data[specific_argu_addr + BL6178_FWVER_MAIN_OFFSET]
            || fw_arg_prj_id[1] != fw_data[specific_argu_addr + BL6178_FWVER_ARGU_OFFSET]
            || fw_checksum != fw_bin_checksum) {
            update_type = BL6178_FW_ARG_UPDATE;
            liot_trace("BL6178 update case: version/checksum mismatch -> FW_ARG_UPDATE");
        } else {
            update_type = BL6178_NONE_UPDATE;
            liot_trace("BL6178 update case: NONE_UPDATE");
        }
    }

    if (update_type != BL6178_NONE_UPDATE) {
        ret = bl6178_update_flash(ch, rst_pin, fw_data, down_size, specific_argu_addr);
        if (ret < 0) {
            liot_trace("BL6178 update flash failed");
            return -1;
        }
    }

    if (ret == 0 && is_cob_prj && is_blank) {
        is_blank = 0;
        liot_trace("BL6178 COB second update for blank IC");
        goto UPDATE_SECOND_FOR_COB;
    }

    liot_trace("BL6178 bl6178_update_fw exit");
    return ret;
}

static int bl6178_update_firmware(liot_tp_handle_t handle,
                                   const uint8_t *fw_data, uint32_t fw_len)
{
    if (!handle || !fw_data || fw_len == 0) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    if (dev->cfg.interface_type != LIOT_TP_IF_I2C) {
        liot_trace("BL6178 FW update: only I2C interface supported");
        return -1;
    }

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (rst_pin < 0) {
        liot_trace("BL6178 FW update: rst_pin not configured");
        return -1;
    }

    return bl6178_update_fw(i2c_ch, rst_pin, BL6178_HEADER_FILE_UPDATE, fw_data, fw_len);
}

/* ================= 工作模式 ================= */

static int bl6178_set_work_mode(liot_tp_handle_t handle, liot_tp_work_mode_e mode)
{
    uint8_t reg_val;

    switch (mode) {
    case LIOT_TP_MODE_NORMAL:
        reg_val = 0x00;
        return liot_tp_reg_write(handle, BL6178_REG_LPM, &reg_val, 1);
    case LIOT_TP_MODE_GESTURE:
    case LIOT_TP_MODE_LOW_POWER:
        reg_val = 0x01;
        return liot_tp_reg_write(handle, BL6178_REG_LPM, &reg_val, 1);
    case LIOT_TP_MODE_DEEP_SLEEP:
        return bl6178_enter_sleep(handle);
    default:
        return -1;
    }
}

/* ================= IC信息 ================= */

static int bl6178_get_ic_info(liot_tp_handle_t handle, uint8_t *buf, uint16_t buf_len)
{
    if (!buf || buf_len < 4) return -1;

    uint8_t info[4] = {0};

    if (liot_tp_reg_read(handle, BL6178_REG_CHIP_ID, &info[0], 1) != 0) {
        return -1;
    }

    if (liot_tp_reg_read(handle, BL6178_REG_FWVER_PJ_ID, &info[1], 2) != 0) {
        info[1] = 0;
        info[2] = 0;
    }

    if (liot_tp_reg_read(handle, BL6178_REG_LPM, &info[3], 1) != 0) {
        info[3] = 0;
    }

    uint16_t copy_len = (buf_len < 4) ? buf_len : 4;
    memcpy(buf, info, copy_len);

    liot_trace("BL6178 IC info: chip_id=0x%x, fw_main=0x%x, fw_argu=0x%x, lpm=0x%x",
               info[0], info[1], info[2], info[3]);

    return 0;
}

/* ================= 设备实例导出 ================= */

liot_tp_sensor_t g_liot_tp_bl6178 = {
    .chip_id         = BL6178_CHIP_ID_VAL,
    .max_points      = BL6178_MAX_POINTS,
    .width           = 360,
    .height          = 360,
    .gesture_support = true,
    .func = {
        .init               = bl6178_init,
        .deinit             = bl6178_deinit,
        .read_touch         = bl6178_read_touch,
        .read_gesture       = bl6178_read_gesture,
        .set_threshold      = bl6178_set_threshold,
        .enter_sleep        = bl6178_enter_sleep,
        .exit_sleep         = bl6178_exit_sleep,
        .wakeup             = bl6178_wakeup,
        .reset              = bl6178_reset,
        .update_firmware    = bl6178_update_firmware,
        .update_firmware_auto = bl6178_auto_update_firmware,
        .set_work_mode      = bl6178_set_work_mode,
        .get_ic_info        = bl6178_get_ic_info,
    },
};


/* ================= 自动固件升级 ================= */

int bl6178_auto_update_firmware(liot_tp_handle_t handle)
{
    if (!handle) return -1;
    liot_tp_dev_info_t *dev = (liot_tp_dev_info_t *)handle;

    if (dev->cfg.interface_type != LIOT_TP_IF_I2C) {
        liot_trace("BL6178 auto update: only I2C interface supported");
        return -1;
    }

    liot_i2c_channel_e i2c_ch = dev->cfg.i2c.num;
    int8_t rst_pin = dev->cfg.rst.pin;

    if (rst_pin < 0) {
        liot_trace("BL6178 auto update: rst_pin not configured");
        return -1;
    }

    liot_trace("BL6178 auto update with built-in FW, size=0x%x", (unsigned)sizeof(fwbin));

    bl6178_hard_reset(rst_pin);
    osDelay(10);

    return bl6178_update_fw(i2c_ch, rst_pin, BL6178_HEADER_FILE_UPDATE,
                             (const uint8_t *)fwbin, sizeof(fwbin));
}
