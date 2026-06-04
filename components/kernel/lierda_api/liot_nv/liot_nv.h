/**
 * @file liot_nv.h
 * @brief LIoT Non-Volatile Memory (NVM) API Interface
 * @details This header file provides function declarations and data structures for Non-Volatile Memory
 *          operations in the LIoT platform, including configuration file read/write and customer-specific
 *          NVM operations.
 * @author lierda Wireless Solutions Co., Ltd.
 * @copyright Copyright (c) 2025, lierda Wireless Solutions Co., Ltd. All rights reserved.
 */

/*========================================================================
 *	General  Definition
 *========================================================================*/
#ifndef LIOT_NV_H
#define LIOT_NV_H
#include "liot_type.h"
#include "liot_api_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def LIOT_OPEN_CUST_NVM
 * @brief Default customer NVM configuration file name
 * @details Standard filename used for customer-specific non-volatile memory storage
 */
#define LIOT_OPEN_CUST_NVM "cust_nv.dat_LIOT"

/**
 * @def LIOT_COMPONENT_STORAGE_NV_BASE
 * @brief Base value for NVM component error codes
 * @details Error code offset for distinguishing NVM subsystem errors from other components
 *          Calculated as (LIOT_COMPONENT_STORAGE_NV << 16)
 */
#define LIOT_COMPONENT_STORAGE_NV_BASE (LIOT_COMPONENT_STORAGE_NV << 16)

/**
 * @enum liot_nv_errcode_e
 * @brief NVM operation error codes
 * @details Error codes returned by NVM API functions to indicate specific failure conditions
 */
typedef enum
{
    LIOT_NV_PARAM_INVALID  = 1 | LIOT_COMPONENT_STORAGE_NV_BASE,  /**< Invalid input parameter */
    LIOT_NV_ERR_OPEN,       /**< Failed to open NVM file */
    LIOT_NV_ERR_WRITE,      /**< Failed to write to NVM file */
    LIOT_NV_ERR_READ,       /**< Failed to read from NVM file */
    LIOT_NV_FILE_NOT_EXIST, /**< NVM file does not exist */
    LIOT_NV_CRC_ERROR,      /**< CRC check failed for NVM data */
    LIOT_NV_SUB_MODULE_LENGTH_DISMATCH, /**< Sub-module length mismatch in NVM data */
    LIOT_NV_FILE_CORRUPTED, /**< NVM file is corrupted or unreadable */
    LIOT_NV_FAILED,         /**< Generic NVM operation failure */
    LIOT_NV_COMMON_ERROR    /**< Common NVM subsystem error */
} liot_nv_errcode_e;

/**
 * @brief Write data to a specified NVM configuration file
 * @details Writes binary data to a named non-volatile memory configuration file
 * @param[in] config_file_name Name of the configuration file to write to
 * @param[in] buffer Pointer to the data buffer containing bytes to write
 * @param[in] size Size of each data block in bytes
 * @param[in] num Number of blocks to write
 * @return Number of successfully written bytes if positive, error code (negative) if failed
 * @retval >0 Success: Number of bytes written
 * @retval <0 Failure: liot_nv_errcode_e error code
 * @note Ensure the target directory has write permissions before calling this function
 */
int liot_nvm_fwrite(const char *config_file_name, void *buffer, size_t size, size_t num);

/**
 * @brief Read data from a specified NVM configuration file
 * @details Reads binary data from a named non-volatile memory configuration file
 * @param[in] config_file_name Name of the configuration file to read from
 * @param[out] buffer Pointer to the buffer where read data will be stored
 * @param[in] size Size of each data block in bytes
 * @param[in] num Number of blocks to read
 * @return Number of successfully read bytes if positive, error code (negative) if failed
 * @retval >0 Success: Number of bytes read
 * @retval <0 Failure: liot_nv_errcode_e error code
 * @note Ensure buffer is large enough to hold (size * num) bytes to prevent overflow
 */
int liot_nvm_fread(const char *config_file_name, void *buffer, size_t size, size_t num);

/**
 * @brief Write data to customer-specific NVM configuration file
 * @details Writes binary data to the default customer NVM file (LIOT_OPEN_CUST_NVM)
 * @param[in] buffer Pointer to the data buffer containing bytes to write
 * @param[in] size Size of each data block in bytes
 * @param[in] num Number of blocks to write
 * @return Number of successfully written bytes if positive, error code (negative) if failed
 * @retval >0 Success: Number of bytes written
 * @retval <0 Failure: liot_nv_errcode_e error code
 * @warning Maximum buffer size is 1KB (1024 bytes). Larger data will be truncated.
 */
int liot_cust_nvm_fwrite(void *buffer, size_t size, size_t num);

/**
 * @brief Read data from customer-specific NVM configuration file
 * @details Reads binary data from the default customer NVM file (LIOT_OPEN_CUST_NVM)
 * @param[out] buffer Pointer to the buffer where read data will be stored
 * @param[in] size Size of each data block in bytes
 * @param[in] num Number of blocks to read
 * @return Number of successfully read bytes if positive, error code (negative) if failed
 * @retval >0 Success: Number of bytes read
 * @retval <0 Failure: liot_nv_errcode_e error code
 * @warning Maximum buffer size is 1KB (1024 bytes). Larger reads will return partial data.
 */
int liot_cust_nvm_fread(void *buffer, size_t size, size_t num);

#endif