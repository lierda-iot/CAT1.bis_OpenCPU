/**
 * @file liot_wifiscan.h
 * @brief WiFi Scan API header file
 * @date 2025-8-18
 * @copyright Copyright (c) 2025 Your Company. All rights reserved.
 */

#ifndef _LIOT_WIFISCAN_H_
#define _LIOT_WIFISCAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*===========================================================================
 * Macro Definition
 *===========================================================================*/
/** @defgroup WIFISCAN_MACROS WiFi Scan Macros
 *  @brief WiFi scan configuration parameters and constants
 *  @{
 */
#define LIOT_WIFISCAN_ERRCODE_BASE (LIOT_COMPONENT_BSP_WIFISCAN << 16) /**< Base value for WiFi scan error codes */

#define LIOT_WIFI_SCAN_TIME_VAL_MAX (255000) /**< Maximum scan duration in milliseconds */
#define LIOT_WIFI_SCAN_TIME_VAL_MIN (4000)    /**< Minimum scan duration in milliseconds */
#define LIOT_WIFI_SCAN_TIME_VAL_DEF (6000)    /**< Default scan duration in milliseconds */

#define LIOT_WIFI_SCAN_MAX_ROUND     (3)      /**< Maximum number of scan rounds */
#define LIOT_WIFI_SCAN_MIN_ROUND     (1)      /**< Minimum number of scan rounds */
#define LIOT_WIFI_SCAN_DEFAULT_ROUND (1)      /**< Default number of scan rounds */

#define LIOT_WIFI_SCAN_MAX_AP_CNT     (40) // max wifi ap count to scan
#define LIOT_WIFI_SCAN_MIN_AP_CNT     (4) // min wifi ap count to scan
#define LIOT_WIFI_SCAN_DEFAULT_AP_CNT (5) // default wifi ap count to scan

#define LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_MIN (1)
#define LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_MAX (255)
#define LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_DEF (5)

#define LIOT_WIFI_SCAN_PRIORITY_VAL_MIN (0) // data preferred
#define LIOT_WIFI_SCAN_PRIORITY_VAL_MAX (1) // wifiscan preferred
#define LIOT_WIFI_SCAN_PRIORITY_VAL_DEF (0)

#define LIOT_WIFISCAN_MODE_SYNCHRO (0)        /**< Synchronous scan mode */
#define LIOT_WIFISCAN_MODE_ASYNC   (1)        /**< Asynchronous scan mode */
/** @} */ // end of WIFISCAN_MACROS

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/

/** @defgroup WIFISCAN_ENUMS WiFi Scan Enumerations
 *  @brief Enumerated types used in WiFi scan functionality
 *  @{
 */

/********************  error code about wifiscan    **********************/
/**
 * @brief WiFi scan error code enumeration
 */
typedef enum
{
    LIOT_WIFISCAN_SUCCESS = 0,                /**< Operation successful */

    LIOT_WIFISCAN_EXECUTE_ERR = 1 | LIOT_WIFISCAN_ERRCODE_BASE, /**< General execution error */
    LIOT_WIFISCAN_MEM_ADDR_NULL_ERR,          /**< Memory address is NULL */
    LIOT_WIFISCAN_INVALID_PARAM_ERR,          /**< Invalid input parameter */
    LIOT_WIFISCAN_SEMAPHORE_WAIT_ERR,         /**< Semaphore wait error */
    LIOT_WIFISCAN_MUTEX_TIMEOUT_ERR,          /**< Mutex timeout error */

    LIOT_WIFISCAN_OPEN_FAIL,                  /**< Failed to open WiFi scan */
    LIOT_WIFISCAN_BUSY_ERR,                   /**< WiFi scan is busy */
    LIOT_WIFISCAN_ALREADY_OPEN_ERR,           /**< WiFi scan already open */
    LIOT_WIFISCAN_NOT_OPEN_ERR,               /**< WiFi scan not open */
    LIOT_WIFISCAN_HW_OCCUPIED_ERR,            /**< WiFi hardware occupied */
    LIOT_WIFISCAN_NO_SET_CB_ERR,              /**< No callback function set */

} liot_wifiscan_errcode_e;

/** @} */ // end of WIFISCAN_ENUMS

/*===========================================================================
 * Struct
 *===========================================================================*/
/** @defgroup WIFISCAN_STRUCTS WiFi Scan Structures
 *  @brief Data structures used in WiFi scan functionality
 *  @{
 */

/**
 * @brief WiFi Access Point information structure
 *
 * Contains information about scanned WiFi access points including SSID,
 * RSSI, channel, and BSSID (MAC address)
 */
typedef struct
{
    UINT8 bssidNum;                           /**< Number of detected BSSIDs */
    UINT8 rsvd;                               /**< Reserved field */
    UINT8 ssidHexLen[LIOT_WIFI_SCAN_MAX_AP_CNT];  /**< Length of SSID hex data for each AP */
    UINT8 ssidHex[LIOT_WIFI_SCAN_MAX_AP_CNT][32]; /**< Hex data of WiFi SSID names */
    INT8 rssi[LIOT_WIFI_SCAN_MAX_AP_CNT];         /**< RSSI values of scanned BSSIDs */
    UINT8 channel[LIOT_WIFI_SCAN_MAX_AP_CNT];     /**< Channel indices (1-13 for 2.4GHz) */
    UINT8 bssid[LIOT_WIFI_SCAN_MAX_AP_CNT][6];    /**< MAC addresses (6 bytes) */
} liot_wifi_ap_info_s;

/**
 * @brief WiFi scan indication message structure
 */
typedef struct
{
    uint32_t msg_err_code;                    /**< Message error code */
    void *msg_data;                           /**< Pointer to message data */
} liot_wifiscan_ind_msg_s;

/** @} */ // end of WIFISCAN_STRUCTS

typedef void (*liot_wifiscan_callback)(liot_wifiscan_ind_msg_s *msg_buf);

/** @defgroup WIFISCAN_FUNCS WiFi Scan Functions
 *  @brief WiFi scan API functions
 *  @{
 */

/**
 * @brief Open WiFi scan module
 *
 * Initializes and opens the WiFi scan module, allocating necessary resources
 *
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Success
 * @retval LIOT_WIFISCAN_ALREADY_OPEN_ERR Module already open
 * @retval LIOT_WIFISCAN_OPEN_FAIL Failed to initialize hardware
 * @retval LIOT_WIFISCAN_HW_OCCUPIED_ERR WiFi hardware occupied by another process
 */
extern liot_wifiscan_errcode_e liot_wifiscan_open(void);

/**
 * @brief Close WiFi scan module
 *
 * Closes the WiFi scan module and releases all allocated resources
 *
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Success
 * @retval LIOT_WIFISCAN_NOT_OPEN_ERR Module not currently open
 */
extern liot_wifiscan_errcode_e liot_wifiscan_close(void);

/**
 * @brief Set WiFi scan options
 *
 * Configures scanning parameters including timeout, rounds, and priority
 *
 * @param[in] maxTimeOut Maximum scan timeout in milliseconds
 *            - Range: @ref LIOT_WIFI_SCAN_TIME_VAL_MIN to @ref LIOT_WIFI_SCAN_TIME_VAL_MAX
 *            - Default: @ref LIOT_WIFI_SCAN_TIME_VAL_DEF
 * @param[in] round Number of scan rounds
 *            - Range: @ref LIOT_WIFI_SCAN_MIN_ROUND to @ref LIOT_WIFI_SCAN_MAX_ROUND
 *            - Default: @ref LIOT_WIFI_SCAN_DEFAULT_ROUND
 * @param[in] maxBssidNum Maximum number of access points to detect
 *            - Range: @ref LIOT_WIFI_SCAN_MIN_AP_CNT to @ref LIOT_WIFI_SCAN_MAX_AP_CNT
 *            - Default: @ref LIOT_WIFI_SCAN_DEFAULT_AP_CNT
 * @param[in] scanTimeOut Scan timeout per channel in seconds
 *            - Range: @ref LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_MIN to @ref LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_MAX
 *            - Default: @ref LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_DEF
 * @param[in] wifiPriority WiFi scan priority level
 *            - @ref LIOT_WIFI_SCAN_PRIORITY_VAL_MIN (data preferred)
 *            - @ref LIOT_WIFI_SCAN_PRIORITY_VAL_MAX (scan preferred)
 *            - Default: @ref LIOT_WIFI_SCAN_PRIORITY_VAL_DEF
 *
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Success
 * @retval LIOT_WIFISCAN_INVALID_PARAM_ERR One or more parameters are out of range
 * @retval LIOT_WIFISCAN_NOT_OPEN_ERR Module not open
 */
extern liot_wifiscan_errcode_e liot_wifiscan_option_set(
    int32 maxTimeOut, uint8 round, uint8 maxBssidNum, uint8 scanTimeOut, uint8 wifiPriority);

/**
 * @brief Perform synchronous WiFi scan
 *
 * Executes a WiFi scan in synchronous mode and returns results immediately
 *
 * @param[out] p_ap_infos Pointer to liot_wifi_ap_info_s structure to store scan results
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Success - results stored in p_ap_infos
 * @retval LIOT_WIFISCAN_MEM_ADDR_NULL_ERR p_ap_infos is NULL
 * @retval LIOT_WIFISCAN_NOT_OPEN_ERR Module not open
 * @retval LIOT_WIFISCAN_BUSY_ERR Scan operation already in progress
 * @note This function blocks until scan completes or timeout occurs
 */
extern liot_wifiscan_errcode_e liot_wifiscan_do(liot_wifi_ap_info_s *p_ap_infos);

/**
 * @brief Register WiFi scan callback function
 *
 * Sets up a callback function to receive asynchronous scan results and notifications
 *
 * @param[in] wifiscan_cb Callback function pointer
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Success
 * @retval LIOT_WIFISCAN_MEM_ADDR_NULL_ERR Callback function is NULL
 * @note Must be called before starting asynchronous scan operations
 */
extern liot_wifiscan_errcode_e liot_wifiscan_register_cb(liot_wifiscan_callback wifiscan_cb);

/**
 * @brief Start asynchronous WiFi scan
 *
 * Initiates a background WiFi scan operation that returns immediately
 * Results will be delivered via the registered callback function
 *
 * @return liot_wifiscan_errcode_e Operation result
 * @retval LIOT_WIFISCAN_SUCCESS Scan started successfully
 * @retval LIOT_WIFISCAN_NOT_OPEN_ERR Module not open
 * @retval LIOT_WIFISCAN_NO_SET_CB_ERR No callback function registered
 * @retval LIOT_WIFISCAN_BUSY_ERR Scan operation already in progress
 * @note Requires prior registration of callback function with liot_wifiscan_register_cb()
 */
extern liot_wifiscan_errcode_e liot_wifiscan_async(void);

/** @} */ // end of WIFISCAN_FUNCS

#ifdef __cplusplus
}
#endif

#endif