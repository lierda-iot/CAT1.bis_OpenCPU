/**
 * @file liot_fota.h
 * @author  lierda
 * @brief 
 * @version 0.1
 * @date 2025-05-21
 * 
 * @copyright Copyright (c) 2025
 * 
 * @note The current FOTA storage location is shared with the file system, and the maximum space of 
 *        the file system represents the maximum size supported by the FOTA package.
 */

#ifndef LIOT_FOTA_H
#define LIOT_FOTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"
/*===========================================================================
 * Macro Definition
 ===========================================================================*/

#ifndef bool
#define bool unsigned char
#endif

typedef enum
{
  LIOT_FOTA_RESET_QUICK,
  LIOT_FOTA_RESET_NORMAL
}liot_fota_reset_mode_e;

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
typedef enum
{
    LIOT_FOTA_FINISHED = 0,   /*!<FOTA upgrade operation completed*/
    LIOT_FOTA_NOT_EXIST,      /*!< No FOTA update package exists; this is not an error status and requires downloading the FOTA update*/
    LIOT_FOTA_UPGRADE_READY,  /*!< FOTA update package has been prepared and is waiting for the update*/
    LIOT_FOTA_STATUS_INVALID, /*!< FOTA status is invalid*/
    LIOT_FOTA_PACK_CHECK_ERR, /*!< FOTA package verification failed*/
} liot_fota_result_e;

// Fota Upgrade Result Code

#define LIOT_FOTA_ERRCODE_BASE (LIOT_COMPONENT_FOTA << 16)
/**
 * @brief FOTA Upgrade Error Code Definition
 */
typedef enum
{
    LIOT_FOTA_UPGRADE_SUCCESS             = 0,                              /*!< Indicates that the FOTA upgrade was successful.*/
    LIOT_FOTA_UPGRADE_FAIL                = 504 | LIOT_FOTA_ERRCODE_BASE,   /*!< General FOTA upgrade failure.*/
    LIOT_FOTA_UPGRADE_CHECK_FAIL          = 505 | LIOT_FOTA_ERRCODE_BASE,   /*!< FOTA upgrade check failed.*/
    LIOT_FOTA_UPGRADE_MD5_FAIL            = 506 | LIOT_FOTA_ERRCODE_BASE,   /*!< MD5 checksum verification of the FOTA package failed.*/
    LIOT_FOTA_UPGRADE_MATCH_FAIL          = 507 | LIOT_FOTA_ERRCODE_BASE,   /*!< FOTA package does not match the device requirements.*/
    LIOT_FOTA_UPGRADE_NO_FILE_FAIL        = 508 | LIOT_FOTA_ERRCODE_BASE,   /*!< FOTA file not found or missing.*/
    LIOT_FOTA_UPGRADE_OPENFILE_FAIL       = 509 | LIOT_FOTA_ERRCODE_BASE,   /*!< Failed to open the FOTA upgrade file.*/
    LIOT_FOTA_UPGRADE_FILESIZE_FAIL       = 510 | LIOT_FOTA_ERRCODE_BASE,   /*!< Invalid or unsupported FOTA file size.*/
    LIOT_FOTA_UPGRADE_LFS_MOUNT_FAIL      = 511 | LIOT_FOTA_ERRCODE_BASE,   /*!< Failed to mount LittleFS (LFS) for FOTA.*/
    LIOT_FOTA_UPGRADE_PARAM_FAIL          = 512 | LIOT_FOTA_ERRCODE_BASE,   /*!< Invalid input parameters for FOTA upgrade.*/
    LIOT_FOTA_UPGRADE_PROJECT_MATCH_FAIL  = 552 | LIOT_FOTA_ERRCODE_BASE,   /*!< Project name in FOTA package does not match the device.*/
    LIOT_FOTA_UPGRADE_BASELINE_MATCH_FAIL = 553 | LIOT_FOTA_ERRCODE_BASE,   /*!< Baseline version in FOTA package does not match the device.*/
    LIOT_FOTA_UPGRADE_POINT_NULL_ERR      = 570 | LIOT_FOTA_ERRCODE_BASE,   /*!< Null pointer error during FOTA upgrade.*/
    LIOT_FOTA_UPGRADE_FLAG_SET_ERR        = 571 | LIOT_FOTA_ERRCODE_BASE,   /*!< Failed to set the upgrade flag during FOTA.*/
} liot_fota_errcode_e;
/*===========================================================================
 * function
 ===========================================================================*/

 /**
  * @brief  set fota verify flag 
  * 
  * @param PackFileName  Pointer to the firmware file name
  * @return liot_fota_errcode_e 
  */
liot_fota_errcode_e liot_fota_image_verify_without_setflag(const char *PackFileName);

/**
 * @brief  Verify the specified FOTA package
 * 
 * @param PackFileName  Pointer to the firmware file name
 * @return liot_fota_errcode_e 
 */
liot_fota_errcode_e liot_fota_image_verify(char *PackFileName);

/**
 * @brief  Verify the specified app FOTA package
 * 
 * @param PackFileName  Pointer to the app firmware file name
 * @return liot_fota_errcode_e 
 */
liot_fota_errcode_e liot_fota_app_image_verify(char *PackFileName);

/**
 * @brief   Clear the specified FOTA package
 * 
 * @param PackFileName Pointer to the app firmware file name
 * @param clear_flag   1: clear the specified FOTA package; 0: clear the specified FOTA package and the FOTA package verification flag
 * @return liot_fota_errcode_e 
 * 
 * @note The PackFileName parameter can be empty. If it is empty, the FOTA package in the default area will be cleared. 
 *       The clear_flag parameter is generally set to 1.
 */
liot_fota_errcode_e liot_fota_clear(const char *PackFileName, int clear_flag);

/**
 * @brief   Get the FOTA upgrade result
 * 
 * @param p_fota_result  Pointer to the FOTA upgrade result
 * @return liot_fota_errcode_e 
 */ 
liot_fota_errcode_e liot_fota_get_result(liot_fota_result_e *p_fota_result);

/**
 * @brief   Perform a power reset after FOTA upgrade
 * 
 * @param reset_mode  Reset mode, fill in the parameter LIOT_RESET_NORMAL and LIOT_RESET_QUICK, default parameter LIOT_RESET_NORMAL
 * @return liot_fota_errcode_e  Return the error code of the FOTA upgrade result
 */
liot_fota_errcode_e liot_fota_power_reset(int reset_mode);

/**
 * @brief   Initialize the NVM for FOTA
 * 
 * @return * liot_fota_errcode_e 
 * 
 * @note This function is used to initialize the FOTA (NVM) area. It can only be called once and must not be called repeatedly..
 */
liot_fota_errcode_e liot_fota_nvm_init(void);

/**
 * @brief   Write data to NVM
 * 
 * @param   offset  Offset to write
 * @param   buf     Buffer to write
 * @param   bufLen  Length of buffer
 * @return * liot_fota_errcode_e 
 * 
 */
liot_fota_errcode_e liot_fota_nvm_write(UINT32 offset, uint8_t *buf, UINT32 bufLen);

/** 
 * @brief   Get free size of NVM
 *
 * @return *  unsigned long  Free size of fota NVM
 *  
*/
unsigned long liot_fota_nvm_free_size_get(void);

/**
 * @brief   Verify image
 * 
 * @return *  liot_fota_errcode_e  Return the error code of the FOTA upgrade result  
 * 
 */
liot_fota_errcode_e liot_fota_nvm_image_verify(void);

/**
 * @brief Perform package format checks and SHA256 verification on the input file, 
 *        and decide whether to perform a reset upgrade based on the input parameter.
 * 
 * The main functionalities of this function include:
 * 1. Checking whether the package format of the input file is correct.
 * 2. Performing SHA256 verification on the file data to ensure data integrity.
 * 3. Deciding whether to immediately perform a reset upgrade based on the second parameter:
 *    - If an immediate upgrade is confirmed, set the second parameter to [true].
 *    - If not upgrading immediately, set it to [false].
 * 4. After the checks pass, the file will be renamed to the upgrade file name required by the underlying system.
 * 5. The differential package for the entire APP package is automatically generated by the system. 
 *    It can be enabled by setting BUILD_OTA_PACKAGE_EN=y.
 * 
 * @param file_name The input file name, pointing to the path of the firmware file that needs to be checked.
 *                   The file path must be valid and the file must exist; otherwise, the check may fail.
 * @param is_reboot Whether to immediately perform a reset upgrade:
 *                   - [true] Immediately triggers a reset upgrade after the check passes.
 *                   - [false] Only performs the check without triggering a reset upgrade.
 * 
 * @return liot_fota_errcode_e Returns the error code of the FOTA upgrade operation result:
 *         - LIOT_FOTA_UPGRADE_SUCCESS: The operation was successful.
 *         - Other error codes (such as LIOT_FOTA_UPGRADE_CHECK_FAIL, LIOT_FOTA_UPGRADE_MD5_FAIL, etc.) indicate specific failure reasons.
 * 
 * @note This function should be called after the firmware file has been downloaded to ensure file integrity and correctness.
 */
liot_fota_errcode_e Liot_FotaAppUpgradeCheck(const char *file_name, BOOL is_reboot);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
