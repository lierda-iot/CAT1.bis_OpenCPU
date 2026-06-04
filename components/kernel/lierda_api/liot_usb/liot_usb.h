/**
 * @file liot_usb.h
 * @brief USB Driver Function Interface Definition
 * @details Provides USB driver initialization, connection management, status query, and other functions
 *          for the Lierda platform.
 * 
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef _LIOT_USB_H_
#define _LIOT_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*===========================================================================
 * Macro Definition
 ===========================================================================*/

/*===========================================================================
 * Enum
 ===========================================================================*/

typedef enum
{
    LIOT_USB_SUCCESS            = LIOT_SUCCESS,                          /* Operation is successful */
    LIOT_USB_INVALID_PARAM      = (LIOT_COMPONENT_BSP_USB << 16) | 1000, /* Invalid input parameter */
    LIOT_USB_SYS_ERROR          = (LIOT_COMPONENT_BSP_USB << 16) | 1001, /* System error */
    LIOT_USB_DETECT_SAVE_NV_ERR = (LIOT_COMPONENT_BSP_USB << 16) | 1002, /* Failed to save detection time to NV */
    LIOT_USB_NO_SPACE           = (LIOT_COMPONENT_BSP_USB << 16) | 1003, /* No space to store data */
    LIOT_USB_NOT_SUPPORT        = (LIOT_COMPONENT_BSP_USB << 16) | 1004, /* Current operation not supported */
} liot_usb_errcode_e;

typedef enum
{
    LIOT_USB_HOTPLUG_OUT = 0, // USB is in the unplugged state
    LIOT_USB_HOTPLUG_IN  = 1  // USB is inserted
} liot_usb_hotplug_e;

/*===========================================================================
 * Struct
 ===========================================================================*/

/**
 * @brief USB callback function
 * @param state Indicates the current state of the USB (inserted/unplugged, of type liot_usb_hotplug_e)
 * @param ctx Currently unused
 * @return int 0 (always returns 0)
 */
typedef int (*liot_usb_hotplug_cb)(liot_usb_hotplug_e state, void *ctx);

/**
 * @brief Register the USB event callback function
 * @param hotplug_callback [in] The callback function to register. Set to NULL to unregister the callback
 * @return liot_usb_errcode_e Operation result (0 indicates success)
 */
liot_usb_errcode_e liot_usb_bind_hotplug_cb(liot_usb_hotplug_cb hotplug_callback);

/**
 * @brief Get the USB plug-in/out state
 * @return liot_usb_hotplug_e USB state (LIOT_USB_HOTPLUG_OUT for unplugged, LIOT_USB_HOTPLUG_IN for inserted)
 */
liot_usb_hotplug_e liot_usb_get_hotplug_state(void);

/**
 * @brief Check if the USB driver is enabled
 * @return bool true if enabled, false if not
 */
bool liot_usb_drv_is_enabled(void);

/**
 * @brief Enable the USB driver
 * @return liot_usb_errcode_e Operation result (0 indicates success)
 */
liot_usb_errcode_e liot_usb_drv_enable(void);

/**
 * @brief Disable the USB driver
 * @return liot_usb_errcode_e Operation result (0 indicates success)
 */
liot_usb_errcode_e liot_usb_drv_disable(void);

#ifdef __cplusplus
}
#endif
#endif