#ifndef BL_CHIP_CUSTOM_H
#define BL_CHIP_CUSTOM_H
#include "liot_i2c.h"

#define     TS_CHIP          BL6XX8
#define     CTP_SLAVE_ADDR		(0x2c)
#define     BTL_IIC_CHANNEL     liot_i2c_1
#define     BTL_CHECK_CHIPID
#define     GPIO_EINT
#define     RESET_PIN_WAKEUP
//#define     INT_PIN_WAKEUP
//#define     NEED_CONFIG_EINT_RESUME
//#define     SWAP_XY
#define     BL_UPDATE_FIRMWARE_ENABLE
//#define     BL_TOUCH_PAD_PROTOCOL_SUPPORT
#define     TPD_RES_X        240
#define     TPD_RES_Y        240
#define     BL_DEBUG_SUPPORT
//#define     BL_FACTORY_SUPPORT
//#define     BL_DEBUG_NOISE
//#define     BL_DEBUG_DIFF
#endif
