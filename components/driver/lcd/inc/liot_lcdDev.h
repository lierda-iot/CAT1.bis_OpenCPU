/**
 * @File Name: liot_lcdDev.h
 * @brief  
 * @Author : Chenhz 
 * @Email : ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-11-15
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 */

#ifndef __LIOT_LCD_DEV_H__
#define __LIOT_LCD_DEV_H__

#include "liot_lcd.h"
#include "liot_os.h"

typedef liot_lcd_handle_t liot_hal_lcd_handle_t;
typedef liot_lcd_config_t liot_hal_lcd_config_t;

inline liot_lcd_errcode_e liot_hal_lcd_write_cmd(liot_hal_lcd_handle_t handle, uint8_t cmd, uint8_t data)
{
    return liot_lcd_send_cmd(handle, cmd, &data, 1);
}

inline liot_lcd_errcode_e liot_hal_lcd_transmit_cmd(liot_hal_lcd_handle_t handle, uint8_t cmd, uint8_t *data, uint16_t size)
{
    return liot_lcd_send_cmd(handle, cmd, data, size);
}

inline liot_lcd_errcode_e liot_hal_lcd_transmit_data(liot_hal_lcd_handle_t handle, uint8_t *data, uint32_t length)
{
    return liot_lcd_send(handle, data, length);
}

inline liot_lcd_errcode_e liot_hal_lcd_set_mspi(liot_hal_lcd_handle_t handle, uint8_t enable,
                                                  uint8_t addrLane, uint8_t dataLane,
                                                  uint8_t instruction)
{
    return liot_lcd_set_mspi(handle, enable, addrLane, dataLane, instruction);
}

#endif
