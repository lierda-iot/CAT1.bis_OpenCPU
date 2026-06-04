/**
 * @file liot_volte.h
 * @brief VoLTE (Voice over LTE) API for Lierda modules
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.3
 * @date 2025-07-08
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#ifndef _LIOT_VOLTE_H_
#define _LIOT_VOLTE_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*========================================================================
 *  Marco Definition
 *========================================================================*/

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
typedef enum
{
    LIOT_VOLTE_SUCCESS = 0,
    LIOT_VOLTE_ERROR = 1 | (LIOT_COMPONENT_IMS << 16),
    LIOT_VOLTE_PARA_ERR,
} liot_volte_errcode_e;

typedef enum
{
    LIOT_VOLTE_DISABLE_REPORTING = 0, // Do not report
    LIOT_VOLTE_ENABLE_REG_INFO,       // Report IMS registration status
    LIOT_VOLTE_ENABLE_EXTEND_INFO,    // Report IMS registration status and IMS function status
} liot_volte_ims_type_e;

/*========================================================================
 *	Struct Definition
 *========================================================================*/
typedef struct
{
    uint8_t ims_type;  // liot_volte_ims_type_e
    uint8_t reg_info;  // 0:Not registered 1:Registered
    uint32_t ext_info; /* 1:RTP-based transfer of voice according to MMTEL
                        2:RTP-based transfer of text according to MMTEL
                        4:SMS using IMS
                        5:RTP-based transfer of voice according to MMTEL and SMS using IMS
                        8:RTP-based transfer of video according to MMTEL
                        10~0XFFFFFFFF:Reserved, not supported*/
} liot_volte_ims_info_s;

typedef struct
{
    uint8_t dialNumStrLen; // dial number len
    uint8_t dialNumStr[80]; // dial number
} liot_volte_incoming_num_s;

typedef struct
{
    uint8_t lrclk; /*1:8kHz 2:16kHz other：error*/
    uint8_t mclk;  /*1:2MHz 2:4MHz other：error*/
} liot_volte_samplerate_s;
/*========================================================================
 *  Callback Definition
 *========================================================================*/

/**
 * @brief Set IMS registration status
 *
 * This function enables or disables IMS protocol registration.
 *
 * @param nSim  SIM card index, value: 0-1
 * @param onoff 1: Register IMS protocol
 *              0: Deregister IMS protocol
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_ims_set(uint8_t nSim, uint8_t onoff);

/**
 * @brief Set IMS registration status reporting type
 *
 * This function configures the type of IMS registration status reporting.
 *
 * @param nSim SIM card index, value: 0-1
 * @param mode Reporting mode (refer to liot_volte_ims_type_e)
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_ims_reg_set(uint8_t nSim, liot_volte_ims_type_e mode);

/**
 * @brief Get IMS registration status
 *
 * This function retrieves the current IMS registration status.
 *
 * @param nSim SIM card index, value: 0-1
 * @param info Pointer to store registration status:
 *             - 0: Not registered
 *             - 1: Registered
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_ims_reg_get(uint8_t nSim, uint8_t *info);

/**
 * @brief Set voice domain option
 *
 * This function sets the voice domain preference for call routing.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param setting Voice domain preference:
 *                - 1: Use CS domain only
 *                - 2: Prefer CS domain, PS as second option
 *                - 3: Prefer PS domain, CS as second option
 *                - 4: Use PS domain only
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_vdp_set(uint8_t nSim, uint8_t setting);

/**
 * @brief Get voice domain option
 *
 * This function retrieves the current voice domain preference setting.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param setting Pointer to store voice domain preference:
 *                - 1: Use CS domain only
 *                - 2: Prefer CS domain, PS as second option
 *                - 3: Prefer PS domain, CS as second option
 *                - 4: Use PS domain only
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_vdp_get(uint8_t nSim, uint8_t *setting);

/**
 * @brief Set module usage priority
 *
 * This function sets the module's usage priority between voice and data.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param setting Usage priority:
 *                - 0: Voice Centric
 *                - 1: Data Centric
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_usage_set(uint8_t nSim, uint8_t setting);

/**
 * @brief Get module usage priority
 *
 * This function retrieves the current module usage priority setting.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param setting Pointer to store usage priority:
 *                - 0: Voice Centric
 *                - 1: Data Centric
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_usage_get(uint8_t nSim, uint8_t *setting);

/**
 * @brief Set codec type
 *
 * This function sets the audio codec type for VoLTE calls.
 *
 * @param nSim      SIM card index, value: 0-1
 * @param codectype Codec type:
 *                  - 0: No I2C codec (default)
 *                  - 1: With I2C codec (only support ES8311)
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_codec_type_set(uint8_t nSim, uint8_t codectype);

/**
 * @brief Set call volume via I2C
 *
 * This function sets the call volume through I2C interface when using an external codec.
 *
 * @param nSim   SIM card index, value: 0-1
 * @param volume Volume level: 0 ~ 100
 * @return Error code indicating success or failure
 *         - LIOT_VOLTE_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_volte_errcode_e liot_volte_volume_set(uint8_t nSim, uint8_t volume);

#ifdef __cplusplus
}
#endif
#endif
