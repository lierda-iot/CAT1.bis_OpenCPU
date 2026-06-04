/**
 * @file liot_usbnet.h
 * @brief USB Network Function Interface Definition
 * @details Provides USB network (ECM/RNDIS) initialization, connection management, status query, and other functions
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef _LIOT_USBNET_H_
#define _LIOT_USBNET_H_

//#include "liot_datacall.h"
#include "liot_api_common.h"
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APN_LEN_MAX
#define APN_LEN_MAX (64)
#endif

#ifndef USERNAME_LEN_MAX
#define USERNAME_LEN_MAX (64)
#endif

#ifndef PASSWORD_LEN_MAX
#define PASSWORD_LEN_MAX (64)
#endif

/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#define LIOT_USBNET_ERRCODE_BASE (LIOT_COMPONENT_NETWORK_USBNET << 16)

/*===========================================================================
 * Enum
 ===========================================================================*/

/**
 * @enum liot_usbnet_errcode_e
 * @brief USB Network Error Code Definition
 */
typedef enum
{
    LIOT_USBNET_SUCCESS     = 0,                       ///< Operation successful
    LIOT_USBNET_EXECUTE_ERR = 1 | LIOT_USBNET_ERRCODE_BASE, ///< Execution error
    LIOT_USBNET_MEM_ADDR_NULL_ERR,                     ///< Memory address is NULL
    LIOT_USBNET_INVALID_PARAM_ERR,                     ///< Invalid parameter
    LIOT_USBNET_USB_NOT_CONNECT_ERR,                   ///< USB not connected
    LIOT_USBNET_PDP_ACTIVE_ERR = 5 | LIOT_USBNET_ERRCODE_BASE, ///< PDP activation failed
    LIOT_USBNET_REPEAT_CONNECT_ERR,                    ///< Repeat connection
    LIOT_USBNET_REPEAT_DISCONNECT_ERR,                 ///< Repeat disconnection
} liot_usbnet_errcode_e;

/**
 * @enum liot_usbnet_type_e
 * @brief USB Network Type Enumeration
 * @note Only ECM and RNDIS modes are supported
 */
typedef enum
{
    LIOT_USBNET_NONE = 0,  ///< Not set
    LIOT_USBNET_ECM,       ///< Ethernet Control Model (ECM) mode
    LIOT_USBNET_MBIM,      ///< Mobile Broadband Interface Model (MBIM) mode
    LIOT_USBNET_RNDIS,     ///< Remote NDIS (RNDIS) mode
    LIOT_USBNET_MAX
} liot_usbnet_type_e;

/**
 * @enum liot_usbnet_state_e
 * @brief USB Network State Enumeration
 */
typedef enum
{
    LIOT_USBNET_STATE_NONE = 0,        ///< USB network uninitialized
    LIOT_USBNET_STATE_START,           ///< USB network connecting
    LIOT_USBNET_STATE_CONNECT,         ///< USB network connected
    LIOT_USBNET_STATE_PORT_DISCONNECT, ///< USB port disconnected
    LIOT_USBNET_STATE_MAX,
} liot_usbnet_state_e;

/**
 * @struct liot_data_call_conf_s
 * @brief PDP Context Configuration Structure
 */
typedef struct
{
    int ip_version;                ///< IP version (4 or 6)
    char apn_name[APN_LEN_MAX];    ///< APN name
    char username[USERNAME_LEN_MAX]; ///< Username
    char password[PASSWORD_LEN_MAX]; ///< Password
    int auth_type;                 ///< Authentication type (0: None, 1: PAP, 2: CHAP)
} liot_data_call_conf_s;

/*===========================================================================
 * STRUCT
 ===========================================================================*/

/*===========================================================================
 * function
 ===========================================================================*/

/**
 * @brief USB network callback function type definition
 * @param[in] ind_type Event notification type
 * @param[in] errcode Error code, 0 indicates successful execution
 * @param[in] ctx Callback reserved parameter
 * @return None
 */
typedef void (*liot_usbnet_callback)(unsigned int ind_type, liot_usbnet_errcode_e errcode, void *ctx);

/**
 * @brief Set USB network type (requires reboot to take effect)
 * @param[in] usbnet_type USB network type, only supports LIOT_USBNET_ECM and LIOT_USBNET_RNDIS
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_set_type(liot_usbnet_type_e usbnet_type);

/**
 * @brief Get the set USB network type
 * @param[out] usbnet_type Pointer to store current USB network type
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_get_type(liot_usbnet_type_e *usbnet_type);

/**
 * @brief Start USB network connection
 * @param[in] nSim SIM card index (range: 0-1)
 * @param[in] profile_idx PDP context index (range: 1-7)
 * @param[in] config PDP context configuration information, NULL indicates using default parameters
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_start(uint8_t nSim, int profile_idx, liot_data_call_conf_s *config);

/**
 * @brief Stop USB network connection
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_stop(void);

/**
 * @brief Get USB network status
 * @param[out] status USB network status (see liot_usbnet_state_e definition)
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_get_status(liot_usbnet_state_e *status);

/**
 * @brief Register USB network callback function
 * @param[in] usbnet_cb USB network callback function pointer
 * @param[in] ctx Callback reserved parameter
 * @return Operation result
 * @retval LIOT_USBNET_SUCCESS Success
 * @retval Other values Error code (see liot_usbnet_errcode_e definition)
 */
liot_usbnet_errcode_e liot_usbnet_register_cb(liot_usbnet_callback usbnet_cb, void *ctx);

#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LIOT_API_USBNET_H */