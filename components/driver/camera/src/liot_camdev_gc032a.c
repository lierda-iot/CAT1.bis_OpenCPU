/**
 * @File Name: liot_camdev_gc032a.c
 * @brief
 * @Author : Chenhz
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2024-1-5
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#include <string.h>
#include "liot_gc032a.h"

#define GC032A_I2C_ADDR 0x21

static int liot_gc032a_reg_init(liot_camera_handle_t handle);
static int liot_gc032a_set_output_format(liot_camera_handle_t handle, liot_camera_output_format_e format);
static int liot_gc032a_set_framesize(liot_camera_handle_t handle,
                                uint16_t width,
                                uint16_t height,
                                uint16_t width_offset,
                                uint16_t height_offset);

liot_camera_sensor_t liot_gc032a_1sdr = {
    .param = {
        .endianMode = LIOT_CAM_LSB_MODE,
        .wireNum = LIOT_WIRE_1,
        .rxSeq = LIOT_SEQ_0,
        .cpol = 0,
        .cpha = 0,
        .ddrMode = 0,
        .wordIdSeq = 0,
        .dummyAllowed = 0,
    },
    .reg = liot_gc032A_1sdrRegInfo,
    .addr = GC032A_I2C_ADDR,
    .func = {
        .init = liot_gc032a_reg_init,
        .set_output_format = liot_gc032a_set_output_format,
        .set_framesize = liot_gc032a_set_framesize,
    },
};

liot_camera_sensor_t liot_gc032a_2sdr = {
    .param = {
        .endianMode = LIOT_CAM_LSB_MODE,
        .wireNum = LIOT_WIRE_2,
        .rxSeq = LIOT_SEQ_0,
        .cpol = 0,
        .cpha = 0,
        .ddrMode = 0,
        .wordIdSeq = 0,
        .dummyAllowed = 0,
    },
    .reg = liot_gc032A_2sdrRegInfo,
    .addr = GC032A_I2C_ADDR,
    .func = {
        .init = liot_gc032a_reg_init,
        .set_output_format = liot_gc032a_set_output_format,
        .set_framesize = liot_gc032a_set_framesize,
    },
};

liot_camera_sensor_t liot_gc032a_2ddr = {
    .param = {
        .endianMode = LIOT_CAM_MSB_MODE,
        .wireNum = LIOT_WIRE_2,
        .rxSeq = LIOT_SEQ_1,
        .cpol = 0,
        .cpha = 1,
        .ddrMode = 1,
        .wordIdSeq = 1,
        .dummyAllowed = 1,
    },
    .reg = liot_gc032A_2ddrRegInfo,
    .addr = GC032A_I2C_ADDR,
    .func = {
        .init = liot_gc032a_reg_init,
        .set_output_format = liot_gc032a_set_output_format,
        .set_framesize = liot_gc032a_set_framesize,
    },
};

static uint16_t liot_gc032aGetRegCnt(char* regName)
{
    if(regName == NULL)
        return -1;

    if (strcmp(regName, "gc032a_2sdr") == 0)
    {
        return (sizeof(liot_gc032A_2sdrRegInfo) / sizeof(liot_gc032A_2sdrRegInfo[0]));
    }
    else if (strcmp(regName, "gc032a_1sdr") == 0)
    {
        return (sizeof(liot_gc032A_1sdrRegInfo) / sizeof(liot_gc032A_1sdrRegInfo[0]));
    }
    else if (strcmp(regName, "gc032a_2ddr") == 0)
    {
        return (sizeof(liot_gc032A_2ddrRegInfo) / sizeof(liot_gc032A_2ddrRegInfo[0]));
    }

    return 0;
}

static int liot_gc032a_reg_init(liot_camera_handle_t handle)
{
    int ret = 0;

    liot_camera_config_t *config = (liot_camera_config_t *)handle;

    uint32_t regCnt = 0;
    if (config->sensor == &liot_gc032a_2ddr)
        regCnt = liot_gc032aGetRegCnt("gc032a_2ddr");
    else if (config->sensor == &liot_gc032a_2sdr)
        regCnt = liot_gc032aGetRegCnt("gc032a_2sdr");
    else if (config->sensor == &liot_gc032a_1sdr)
        regCnt = liot_gc032aGetRegCnt("gc032a_1sdr");

    for (int i = 0; i < regCnt; i++)
    {
        ret = liot_camera_i2c_reg_write(handle, config->sensor->reg[i].regAddr, config->sensor->reg[i].regVal);
    }

    return ret;
}

static int liot_gc032a_set_output_format(liot_camera_handle_t handle, liot_camera_output_format_e format)
{
    int ret = 0;

    switch (format)
    {
        case LIOT_CAMERA_OUTPUT_GRAY:
        case LIOT_CAMERA_OUTPUT_YUYV:
        {
            ret = liot_camera_i2c_reg_write(handle, 0xfe, 0x00);
            ret = liot_camera_i2c_reg_write(handle, 0x44, 0x03);
            // ret = liot_camera_i2c_reg_write(handle, 0xfe, 0x03);
            // ret = liot_camera_i2c_reg_write(handle, 0x5A, 0x00);
            break;
        }
        case LIOT_CAMERA_OUTPUT_RGB565:
        {
            ret = liot_camera_i2c_reg_write(handle, 0xfe, 0x00);
            ret = liot_camera_i2c_reg_write(handle, 0x49, 0x03);
            ret = liot_camera_i2c_reg_write(handle, 0x44, 0x06);
            // ret = liot_camera_i2c_reg_write(handle, 0xfe, 0x03);
            // ret = liot_camera_i2c_reg_write(handle, 0x5A, 0x01);

            break;
        }
        default:
        {
            break;
        }
    }

    return ret;
}

static int liot_gc032a_set_framesize(liot_camera_handle_t handle,
                                uint16_t width,
                                uint16_t height,
                                uint16_t width_offset,
                                uint16_t height_offset)
{
    int ret = 0;
    ret = liot_camera_i2c_reg_write(handle, 0xfe, 0x00);

    ret = liot_camera_i2c_reg_write(handle, 0x50, 0x01);
    ret = liot_camera_i2c_reg_write(handle, 0x51, (height_offset >> 8));
    ret = liot_camera_i2c_reg_write(handle, 0x52, (height_offset & 0xff));
    ret = liot_camera_i2c_reg_write(handle, 0x53, (width_offset >> 8));
    ret = liot_camera_i2c_reg_write(handle, 0x54, (width_offset & 0xff));

    ret = liot_camera_i2c_reg_write(handle, 0x55, (height >> 8));
    ret = liot_camera_i2c_reg_write(handle, 0x56, (height & 0xff));
    ret = liot_camera_i2c_reg_write(handle, 0x57, (width >> 8));
    ret = liot_camera_i2c_reg_write(handle, 0x58, (width & 0xff));

    ret = liot_camera_i2c_reg_write(handle, 0xFE, 0x03);
    ret = liot_camera_i2c_reg_write(handle, 0x5B, (width & 0xff));
    ret = liot_camera_i2c_reg_write(handle, 0x5C, (width >> 8));
    ret = liot_camera_i2c_reg_write(handle, 0x5D, (height & 0xff));
    ret = liot_camera_i2c_reg_write(handle, 0x5E, (height >> 8));

    return ret;
}

