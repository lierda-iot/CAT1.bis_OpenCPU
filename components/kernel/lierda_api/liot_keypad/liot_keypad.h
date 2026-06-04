/**
 * @File Name: liot_keypad.h
 * @brief Keypad controller API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef _LIOT_KEYPAD_H_
#define _LIOT_KEYPAD_H_

#include "liot_api_common.h"
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Macro Definition
 ===========================================================================*/

#define LIOT_KEYPAD_ERRCODE_BASE (LIOT_COMPONENT_BSP_KEYPAD<<16)
#define LIOT_KEYPAD_ROW_LENGTH  4
#define LIOT_KEYPAD_COL_LENGTH  4
/*Alternate function number for keypad pins*/
#define KEYPAD_ALT_FUNC       (PAD_MUX_ALT6)


/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
/**
 * @enum liot_errcode_keypad_e
 * @brief List of kpc errcode
 * @details None
 */
typedef enum
{
    LIOT_KEYPAD_SUCCESS = LIOT_SUCCESS,
    LIOT_KEYPAD_INVALID_PARAM_ERR                 = 1|LIOT_KEYPAD_ERRCODE_BASE
}liot_errcode_keypad_e;

/**
  * @enum LIOT_KPCREPORTVALUE
  * @brief List of kpc report key value
  * @details None
  */
typedef enum
{
    LIOT_KPC_REPORT_KEY_RELEASE      = 0U, //!< Key is released 
    LIOT_KPC_REPORT_KEY_PRESS        = 1U, //!< Key is pressed  
    LIOT_KPC_REPORT_KEY_REPEAT       = 2U, //!< Key holds pressed 
}liot_kpc_report_value_e;

/**
  * @struct liot_kpc_report_event_t
  * @brief List of kpc report key event
  * @details None
  */
typedef struct
{
	uint8_t    value  : 2;	//!<Key value, liot_kpc_report_value_e
	uint8_t    column : 3;	//!< Key column 
	uint8_t    row	  : 3;	//!<Key row 
}liot_kpc_report_event_t;



/*========================================================================
 *	function Definition
 *========================================================================*/

/**
 * @brief Keypad event callback function type
 *
 * This is the function pointer type for keypad event callbacks.
 * The callback function is called whenever a key event occurs.
 *
 * @param key Keypad report event structure containing row, column, and value information
 */
typedef void (*liot_keyeventcb_t)(liot_kpc_report_event_t key);

/**
 * @brief Initialize matrix keypad
 *
 * @param cb      Callback function to be called on key events
 * @param keyrow  Array of module pins connected to keypad rows
 * @param keycol  Array of module pins connected to keypad columns
 * @return        Error code indicating success or failure
 *                - LIOT_KEYPAD_SUCCESS: Initialization successful
 *                - LIOT_KEYPAD_INVALID_PARAM_ERR: Invalid parameter
 */
liot_errcode_keypad_e liot_keypad_init(liot_keyeventcb_t cb, uint8 keyrow[LIOT_KEYPAD_ROW_LENGTH], uint8 keycol[LIOT_KEYPAD_COL_LENGTH]);

/**
 * @brief Get keypad state
 *
 * @param pressed Pointer to store the key state (refer to liot_kpc_report_value_e)
 * @param id      Pointer to store the key identifier
 * @return        Error code indicating success or failure
 *                - LIOT_KEYPAD_SUCCESS: State retrieval successful
 *                - LIOT_KEYPAD_INVALID_PARAM_ERR: Invalid parameter
 */
liot_errcode_keypad_e liot_keypad_state(uint8_t *pressed, uint8_t *id);

#ifdef __cplusplus
}
#endif

#endif

