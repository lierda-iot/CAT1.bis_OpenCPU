/**
 * @File Name: liot_voice_call.h
 * @brief Voice call API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.3
 * @date 2025-07-08
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef _LIOT_VOICE_CALL_H_
#define _LIOT_VOICE_CALL_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*========================================================================
 *  Marco Definition
 *========================================================================*/
#define LIOT_VC_REASON_NOANSWER 1
#define LIOT_VC_REASON_NOCARRIER 2
#define LIOT_VC_REASON_BUSY 3
#define LIOT_VC_REASON_PWROFF 4
#define LIOT_VC_REASON_ERROR 5

#define LIOT_VC_MAX_NUM 7

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
typedef enum
{
	LIOT_VOLTE_NOT_REG = 0,	   ///< Not registered to IMS
	LIOT_VOLTE_REG_SUCCESS = 1 ///< Successfully registered to IMS
} liot_vc_reg_state_e;

typedef enum
{
	LIOT_VC_SUCCESS = 0,								   ///< Operation successful
	LIOT_VC_ERROR = 1 | (LIOT_COMPONENT_VOICE_CALL << 16), ///< General error
	LIOT_VC_NOT_INIT_ERR,								   ///< Not initialized error
	LIOT_VC_PARA_ERR,									   ///< Parameter error
	LIOT_VC_NO_MEMORY_ERR,								   ///< No memory error
	LIOT_VC_NO_CALL_ERR,								   ///< No call error
	LIOT_VC_SEM_CREATE_ERR,								   ///< Semaphore creation error
	LIOT_VC_SEM_TIMEOUT_ERR,							   ///< Semaphore timeout error
} liot_vc_errcode_e;

typedef enum
{
	LIOT_VC_INIT_OK_IND = 1 | (LIOT_COMPONENT_VOICE_CALL << 16), ///< Initialization OK indication
	LIOT_VC_RING_IND,											 ///< Incoming call ring indication
	LIOT_VC_CONNECT_IND,										 ///< Call connected indication
	LIOT_VC_NOCARRIER_IND,										 ///< Call disconnected indication
	LIOT_VC_IMSREG_IND,											 ///< IMS registration status indication
	LIOT_VC_INCOMING_NUMBER_IND,								 ///< Incoming call number indication
	LIOT_VC_SAMPLERATE_IND,										 ///< Audio sample rate indication
	LIOT_VC_ERROR_IND,											 ///< Error indication
} liot_vc_event_id_e;

/*========================================================================
 *	Struct Definition
 *========================================================================*/
typedef struct
{
	uint8_t idx;
	uint8_t direction;
	uint8_t status;
	uint8_t multiparty;
	char number[23];
} liot_vc_info_s;

/*========================================================================
 *  Callback Definition
 *========================================================================*/
typedef void (*liot_vc_event_handler_t)(uint8_t sim, liot_vc_event_id_e event_id, void *ctx);
extern liot_vc_event_handler_t liot_voice_call_callback;

/**
 * @brief Set auto answer
 *
 * This function configures the auto answer feature for incoming calls.
 *
 * @param nSim  SIM card index, value: 0-1
 * @param times Number of rings before auto answering. 0 means disable auto answer
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_auto_answer(uint8_t nSim, uint8_t times);

/**
 * @brief Make a call
 *
 * This function initiates an outgoing voice call to the specified phone number.
 *
 * @param nSim     SIM card index, value: 0-1
 * @param dial_num Phone number in string format
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_call_start(uint8_t nSim, char *dial_num);

/**
 * @brief Answer a call
 *
 * This function answers an incoming voice call.
 *
 * @param nSim SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_call_answer(uint8_t nSim);

/**
 * @brief End a call
 *
 * This function ends the current active voice call.
 *
 * @param nSim SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_call_end(uint8_t nSim);

/**
 * @brief Send DTMF tones
 *
 * This function sends DTMF (Dual-Tone Multi-Frequency) tones during a call.
 *
 * @param nSim     SIM card index, value: 0-1
 * @param dtmf     String containing DTMF characters "0-9", "#", "*", "A-D"
 * @param duration Duration in 100ms units, value 0-10. 0 means use default duration
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_call_start_dtmf(uint8_t nSim, char *dtmf, uint16_t duration);

/**
 * @brief Get current call list
 *
 * This function retrieves the list of current calls with their information.
 *
 * @param nSim     SIM card index, value: 0-1
 * @param total    Pointer to store the total number of calls
 * @param vc_info  Pointer to array of structures with call information (index, state, number etc.)
 *                 Array size should be at least LIOT_VC_MAX_NUM
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_call_clcc(uint8_t nSim, uint8_t *total, liot_vc_info_s vc_info[LIOT_VC_MAX_NUM]);

/**
 * @brief Get phone number
 *
 * This function retrieves the phone number associated with the SIM card.
 *
 * @param nSim   SIM card index, value: 0-1
 * @param number Pointer to store the phone number (ASCII string)
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_get_phone_num(uint8_t nSim, uint8_t *number);

/**
 * @brief Control audio switch during call
 *
 * This function controls the audio path during a voice call.
 *
 * @param nSim SIM card index, value: 0-1
 * @param flag Audio switch: FALSE - Audio off, TRUE - Audio on
 * @return Error code indicating success or failure
 *         - LIOT_VC_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_vc_errcode_e liot_voice_set_audio_switch(uint8_t nSim, BOOL flag);

/**
 * @brief Register callback function
 *
 * This function registers the callback function that will be called when
 * voice call events occur.
 *
 * @param cb Callback function to register
 */
void liot_voice_call_callback_register(liot_vc_event_handler_t cb);

#ifdef __cplusplus
}
#endif
#endif