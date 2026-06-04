/**
 * @file liot_fota2.h
 * @author  lierda
 * @brief 
 * @version 1.0
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2025
 * 
 * @note The current FOTA storage location is shared with the file system, and the maximum space of 
 *        the file system represents the maximum size supported by the FOTA package.
 */

#ifndef LIOT_FOTA2_H
#define LIOT_FOTA2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

typedef void (*liot_fota_callback)(uint8_t progess);
/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#ifndef bool
#define bool unsigned char
#endif

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
/**
 * @brief FOTA Upgrade Error Code Definition
 */
typedef enum
{
    L_FOTA_UPGRADE_SUCCESS             = 0,   /*!< Indicates that the FOTA upgrade was successful.*/
    L_FOTA_UPGRADE_FAIL                = 1,   /*!< General FOTA upgrade failure.*/
    L_FOTA_UPGRADE_CHECK_FAIL          = 2,   /*!< FOTA upgrade check failed.*/
    L_FOTA_UPGRADE_MD5_FAIL            = 3,   /*!< MD5 checksum verification of the FOTA package failed.*/
    L_FOTA_UPGRADE_MATCH_FAIL          = 4,   /*!< FOTA package does not match the device requirements.*/
    L_FOTA_UPGRADE_NO_FILE_FAIL        = 5,   /*!< FOTA file not found or missing.*/
    L_FOTA_UPGRADE_OPENFILE_FAIL       = 6,   /*!< Failed to open the FOTA upgrade file.*/
    L_FOTA_UPGRADE_FILESIZE_FAIL       = 7,   /*!< Invalid or unsupported FOTA file size.*/
    L_FOTA_UPGRADE_LFS_MOUNT_FAIL      = 8,   /*!< Failed to mount LittleFS (LFS) for FOTA.*/
    L_FOTA_UPGRADE_PARAM_FAIL          = 9,   /*!< Invalid input parameters for FOTA upgrade.*/
    L_FOTA_UPGRADE_PROJECT_MATCH_FAIL  = 10,   /*!< Project name in FOTA package does not match the device.*/
    L_FOTA_UPGRADE_BASELINE_MATCH_FAIL = 11,   /*!< Baseline version in FOTA package does not match the device.*/
    L_FOTA_UPGRADE_POINT_NULL_ERR      = 12,   /*!< Null pointer error during FOTA upgrade.*/
    L_FOTA_UPGRADE_FLAG_SET_ERR        = 13,   /*!< Failed to set the upgrade flag during FOTA.*/
} liot_fota_err_e;

typedef struct
{
  char *url;        // 可以是三种https、ftp、本地文件
  bool enable;      // 下载完成后是否进行重启升级
  uint32_t timeout; // 超时时间
  liot_fota_callback callback;
} Liot_FotaConfig_t;
/*===========================================================================
 * function
 ===========================================================================*/
/**
 * @brief   Write data to NVM
 *
 * @param   fota_config  Liot_FotaConfig_t
 * @return * liot_fota_err_e
 *
 */
liot_fota_err_e Liot_FotaUpgrade(Liot_FotaConfig_t *fota_config);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
