/**
 * @file liot_sim.h
 * @brief SIM card management interface
 * 
 * This header file provides functions for SIM card operations including 
 * getting SIM information, checking card status, managing SIM slots,
 * and handling dual-card preferences.
 * 
 * @email ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2025-08-15
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef _LIOT_SIM_H_
#define _LIOT_SIM_H_

#ifdef __cplusplus // this area code will compile with c program
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*========================================================================
 *  Marco Definition
 *========================================================================*/
#define LIOT_SIM_ERRCODE_BASE (LIOT_COMPONENT_SIM << 16)

#define LIOT_SIM_PIN_LEN_MAX 8 // Maximum length of PIN data

#define LIOT_SIM_INVALID 0xFF
#define LIOT_SIM_1       0
#define LIOT_SIM_2       1

#define LIOT_SIM_DUALCARD_PREFERRED_DISABLE FALSE
#define LIOT_SIM_DUALCARD_PREFERRED_ENABLE  TRUE

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
/**
 * @brief SIM component error codes
 * 4-byte error codes for SIM card operations
 */
typedef enum
{
    LIOT_SIM_SUCCESS     = 0,
    LIOT_SIM_EXECUTE_ERR = 1 | LIOT_SIM_ERRCODE_BASE,
    LIOT_SIM_MEM_ADDR_NULL_ERR,
    LIOT_SIM_INVALID_PARAM_ERR,
    LIOT_SIM_NO_MEMORY_ERR,
    LIOT_SIM_SEMAPHORE_CREATE_ERR = 5 | LIOT_SIM_ERRCODE_BASE,
    LIOT_SIM_SEMAPHORE_TIMEOUT_ERR,
    LIOT_SIM_NOT_INSERTED_ERR,
    LIOT_SIM_CFW_IMSI_GET_REQUEST_ERR,
    LIOT_SIM_CFW_IMSI_GET_RSP_ERR,
    LIOT_SIM_CFW_ICCID_GET_REQUEST_ERR = 10 | LIOT_SIM_ERRCODE_BASE,
    LIOT_SIM_CFW_PHONE_NUM_GET_REQUEST_ERR,
    LIOT_SIM_CFW_PHONE_NUM_GET_RSP_NULL_ERR,
    LIOT_SIM_CFW_STATUS_GET_REQUEST_ERR,
    LIOT_SIM_CFW_PIN_ENABLE_REQUEST_ERR,
    LIOT_SIM_CFW_PIN_DISABLE_REQUEST_ERR = 15 | LIOT_SIM_ERRCODE_BASE,
    LIOT_SIM_CFW_PIN_VERIFY_REQUEST_ERR,
    LIOT_SIM_CFW_PIN_CHANGE_REQUEST_ERR,
    LIOT_SIM_CFW_PIN_UNBLOCK_REQUEST_ERR,
    LIOT_SIM_OPERATION_NOT_ALLOWED_ERR,
    LIOT_PBK_NOT_EXIT_ERR = 100 | LIOT_SIM_ERRCODE_BASE,
    LIOT_PBK_NOT_INIT_ERR,
    LIOT_PBK_ITEM_NOT_FOUND_ERR,
} liot_sim_errcode_e;

/**
 * @brief SIM card status codes
 * Represents different states of a SIM card
 */
typedef enum
{
    LIOT_SIM_STATUS_READY = 0,
    LIOT_SIM_STATUS_PIN1_READY,
    LIOT_SIM_STATUS_SIMPIN,
    LIOT_SIM_STATUS_SIMPUK,
    LIOT_SIM_STATUS_PHONE_TO_SIMPIN,
    LIOT_SIM_STATUS_PHONE_TO_FIRST_SIMPIN = 5,
    LIOT_SIM_STATUS_PHONE_TO_FIRST_SIMPUK,
    LIOT_SIM_STATUS_SIMPIN2,
    LIOT_SIM_STATUS_SIMPUK2,
    LIOT_SIM_STATUS_NETWORKPIN,
    LIOT_SIM_STATUS_NETWORKPUK = 10,
    LIOT_SIM_STATUS_NETWORK_SUBSETPIN,
    LIOT_SIM_STATUS_NETWORK_SUBSETPUK,
    LIOT_SIM_STATUS_PROVIDERPIN,
    LIOT_SIM_STATUS_PROVIDERPUK,
    LIOT_SIM_STATUS_CORPORATEPIN = 15,
    LIOT_SIM_STATUS_CORPORATEPUK,
    LIOT_SIM_STATUS_NOSIM,
    LIOT_SIM_STATUS_PIN1BLOCK,
    LIOT_SIM_STATUS_PIN2BLOCK,
    LIOT_SIM_STATUS_PIN1_DISABLE = 20,
    LIOT_SIM_STATUS_SIM_PRESENT,
    LIOT_SIM_STATUS_UNKNOW,
} liot_sim_status_e;

/**
 * @brief Get SIM card IMSI
 * Retrieves the International Mobile Subscriber Identity (IMSI) from the specified SIM card.
 * 
 * @param nSim [in] SIM card index (0-1)
 * @param imsi [out] Buffer to store IMSI string
 * @param imsiLen [in] Length of the IMSI buffer
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_imsi(uint8_t nSim, char *imsi, size_t imsiLen);

/**
 * @brief Get SIM card ICCID
 * Retrieves the Integrated Circuit Card Identifier (ICCID) from the specified SIM card.
 * 
 * @param nSim [in] SIM card index (0-1)
 * @param iccid [out] Buffer to store ICCID string
 * @param iccidLen [in] Length of the ICCID buffer
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_iccid(uint8_t nSim, char *iccid, size_t iccidLen);

/**
 * @brief Get SIM card phone number
 * Retrieves the phone number associated with the specified SIM card.
 * 
 * @param nSim [in] SIM card index (0-1)
 * @param phonenumber [out] Buffer to store phone number string
 * @param phonenumberLen [in] Length of the phone number buffer
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_phonenumber(uint8_t nSim, char *phonenumber, size_t phonenumberLen);

/**
 * @brief Get cached SIM card status
 * Retrieves the cached status information of the specified SIM card.
 * 
 * @param nSim [in] SIM card index (0-1)
 * @param cardStatus [out] Pointer to return SIM card status (see liot_sim_status_e)
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_card_status(uint8_t nSim, liot_sim_status_e *cardStatus);

/**
 * @brief Set current SIM card slot ID
 * Configures the active SIM card slot.
 * 
 * @param nSim [in] SIM card ID (0-1)
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_set_slot(uint8_t nSim);

/**
 * @brief Get current SIM card slot ID
 * Retrieves the ID of the currently active SIM card slot.
 * 
 * @param pSim [out] Pointer to store SIM card ID
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_slot(uint8_t* pSim);

/**
 * @brief Set dual-card preferred function
 * Enables or disables the dual-card preference feature.
 * 
 * @param enable [in] Feature status (false: disable, true: enable)
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_set_dualcard_preferred(BOOL enable);

/**
 * @brief Get dual-card preferred status
 * Retrieves the current status of the dual-card preference feature.
 * 
 * @param pIsEnable [out] Pointer to store feature status
 * @return liot_sim_errcode_e LIOT_SIM_SUCCESS if successful, error code otherwise
 */
liot_sim_errcode_e liot_sim_get_dualcard_preferred(BOOL* pIsEnable);

#ifdef __cplusplus
}
#endif

#endif