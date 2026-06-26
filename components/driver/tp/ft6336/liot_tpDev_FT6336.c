/**
 * @File Name: liot_tp_ft6336.c
 * @brief FT6336 TP sensor driver based on liot_tp framework
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_tp.h"
#include "liot_os.h"
#include "liot_log.h"
#include <string.h>

/* FT6336 normal mode registers, I2C address: 0x38 */
#define FT6336_REG_GESTURE_ID     0x01
#define FT6336_REG_TD_STATUS      0x02
#define FT6336_REG_P1_XH          0x03
#define FT6336_REG_P2_XH          0x09
#define FT6336_REG_CHIP_ID        0xA3
#define FT6336_REG_FW_VER         0xA6
#define FT6336_REG_VENDOR_ID      0xA8

#define FT6336_CHIP_ID_VAL        0x36
#define FT6336_MAX_POINTS         2
#define FT6336_WIDTH              240
#define FT6336_HEIGHT             320
#define FT6336_POINT_REG_STEP     6
#define FT6336_POINT_READ_LEN     4

static bool s_ft6336_pressed;
static liot_tp_point_t s_ft6336_last_point;

static liot_tp_event_e ft6336_parse_event(uint8_t event_flag)
{
    switch (event_flag) {
    case 0x00:
        return LIOT_TP_EVT_DOWN;
    case 0x01:
        return LIOT_TP_EVT_UP;
    case 0x02:
        return LIOT_TP_EVT_MOVE;
    default:
        return LIOT_TP_EVT_MOVE;
    }
}

static int ft6336_init(liot_tp_handle_t handle)
{
    uint8_t status = 0;
    uint8_t chip_id = FT6336_CHIP_ID_VAL;
    uint8_t fw_ver = 0;

    if (!handle) {
        return -1;
    }

    if (liot_tp_reg_read(handle, FT6336_REG_TD_STATUS, &status, 1) != 0) {
        liot_trace("FT6336 read status failed");
        //return -1;
    }

    (void)liot_tp_reg_read(handle, FT6336_REG_CHIP_ID, &chip_id, 1);
    (void)liot_tp_reg_read(handle, FT6336_REG_FW_VER, &fw_ver, 1);

    s_ft6336_pressed = false;
    memset(&s_ft6336_last_point, 0, sizeof(s_ft6336_last_point));

    liot_trace("FT6336 init ok, status=0x%x, chip_id=0x%x, fw_ver=0x%x",
               status, chip_id, fw_ver);
    return 0;
}

static int ft6336_deinit(liot_tp_handle_t handle)
{
    (void)handle;
    s_ft6336_pressed = false;
    memset(&s_ft6336_last_point, 0, sizeof(s_ft6336_last_point));
    return 0;
}

static int ft6336_read_point(liot_tp_handle_t handle, uint8_t index,
                             liot_tp_point_t *point)
{
    uint8_t buf[FT6336_POINT_READ_LEN];
    uint8_t reg;
    uint8_t event_flag;

    if (!handle || !point || index >= FT6336_MAX_POINTS) {
        return -1;
    }

    reg = FT6336_REG_P1_XH + index * FT6336_POINT_REG_STEP;
    if (liot_tp_reg_read(handle, reg, buf, sizeof(buf)) != 0) {
        return -1;
    }

    event_flag = (buf[0] >> 6) & 0x03;

    point->event = ft6336_parse_event(event_flag);
    point->x = ((uint16_t)(buf[0] & 0x0F) << 8) | buf[1];
    point->y = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
    point->id = (buf[2] >> 4) & 0x0F;
    point->weight = 1;
    point->timestamp_ms = osKernelGetTickCount();

    return 0;
}

static int ft6336_read_touch(liot_tp_handle_t handle, liot_tp_touch_data_t *data)
{
    uint8_t status;
    uint8_t point_num;

    if (!handle || !data) {
        return -1;
    }

    memset(data, 0, sizeof(*data));

    if (liot_tp_reg_read(handle, FT6336_REG_TD_STATUS, &status, 1) != 0) {
        return -1;
    }

    point_num = status & 0x0F;
    if (point_num == 0) {
        if (s_ft6336_pressed) {
            data->touch_cnt = 1;
            data->point[0] = s_ft6336_last_point;
            data->point[0].event = LIOT_TP_EVT_UP;
            data->point[0].timestamp_ms = osKernelGetTickCount();
            s_ft6336_pressed = false;
        }
        return 0;
    }

    if (point_num > FT6336_MAX_POINTS) {
        point_num = FT6336_MAX_POINTS;
    }

    data->touch_cnt = point_num;
    for (uint8_t i = 0; i < point_num; i++) {
        if (ft6336_read_point(handle, i, &data->point[i]) != 0) {
            memset(data, 0, sizeof(*data));
            return -1;
        }
    }

    s_ft6336_last_point = data->point[0];
    s_ft6336_pressed = (data->point[0].event != LIOT_TP_EVT_UP);

    return 0;
}

static int ft6336_read_gesture(liot_tp_handle_t handle, liot_tp_gesture_data_t *data)
{
    uint8_t gesture = 0;

    if (!handle || !data) {
        return -1;
    }

    if (liot_tp_reg_read(handle, FT6336_REG_GESTURE_ID, &gesture, 1) != 0) {
        return -1;
    }

    data->timestamp_ms = osKernelGetTickCount();
    switch (gesture) {
    case 0x10:
        data->gesture = LIOT_TP_GESTURE_UP;
        break;
    case 0x18:
        data->gesture = LIOT_TP_GESTURE_DOWN;
        break;
    case 0x14:
        data->gesture = LIOT_TP_GESTURE_LEFT;
        break;
    case 0x1C:
        data->gesture = LIOT_TP_GESTURE_RIGHT;
        break;
    default:
        data->gesture = LIOT_TP_GESTURE_NONE;
        break;
    }

    return 0;
}

static int ft6336_get_ic_info(liot_tp_handle_t handle, uint8_t *buf, uint16_t buf_len)
{
    uint8_t info[4] = {FT6336_CHIP_ID_VAL, 0, 0, 0};
    uint16_t copy_len;

    if (!handle || !buf || buf_len == 0) {
        return -1;
    }

    (void)liot_tp_reg_read(handle, FT6336_REG_CHIP_ID, &info[0], 1);
    (void)liot_tp_reg_read(handle, FT6336_REG_FW_VER, &info[1], 1);
    (void)liot_tp_reg_read(handle, FT6336_REG_VENDOR_ID, &info[2], 1);
    (void)liot_tp_reg_read(handle, FT6336_REG_TD_STATUS, &info[3], 1);

    copy_len = (buf_len < sizeof(info)) ? buf_len : sizeof(info);
    memcpy(buf, info, copy_len);

    liot_trace("FT6336 IC info: chip_id=0x%x, fw_ver=0x%x, vendor=0x%x, status=0x%x",
               info[0], info[1], info[2], info[3]);
    return 0;
}

liot_tp_sensor_t g_liot_tp_ft6336 = {
    .chip_id = FT6336_CHIP_ID_VAL,
    .max_points = FT6336_MAX_POINTS,
    .width = FT6336_WIDTH,
    .height = FT6336_HEIGHT,
    .gesture_support = false,
    .func = {
        .init = ft6336_init,
        .deinit = ft6336_deinit,
        .read_touch = ft6336_read_touch,
        .read_gesture = ft6336_read_gesture,
        .get_ic_info = ft6336_get_ic_info,
    },
};
