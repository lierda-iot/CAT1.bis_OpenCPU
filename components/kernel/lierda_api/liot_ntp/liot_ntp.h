/**
 * @File Name: liot_ntp.h
 * @brief NTP (Network Time Protocol) API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef _LIOT_NTP_H
#define _LIOT_NTP_H


typedef int ntp_client_id;

/*===========================================================================
 * Struct
 ===========================================================================*/

typedef struct
{
    int pdp_cid;
    int sim_id;
    int retry_cnt;
    int retry_interval_tm;
} liot_ntp_sync_option;

/*===========================================================================
 * Enum
 ===========================================================================*/

typedef enum
{
    LIOT_NTP_SUCCESS = 0,                  ///< NTP synchronization successful
    LIOT_NTP_ERROR_UNKNOWN = -1,           ///< Unknown error
    LIOT_NTP_ERROR_WODBLOCK = -2,          ///< WOD block error
    LIOT_NTP_ERROR_INVALID_PARAM = -3,     ///< Invalid parameter
    LIOT_NTP_ERROR_OUT_OF_MEM = -4,        ///< Out of memory
    LIOT_NTP_ERROR_TIMEOUT = -5,           ///< Operation timeout
    LIOT_NTP_ERROR_DNS_FAIL = -6,          ///< DNS resolution failed
    LIOT_NTP_ERROR_SOCKET_ALLOC_FAIL = -7, ///< Socket allocation failed
    LIOT_NTP_ERROR_SOCKET_SEND_FAIL = -8,  ///< Socket send failed
    LIOT_NTP_ERROR_SOCKET_RECV_FAIL = -9,  ///< Socket receive failed
    LIOT_NTP_ERROR_INVALID_REPLY = -10     ///< Invalid NTP reply
} liot_ntp_error_code_e;

/**
 * @brief NTP synchronization result callback function type
 *
 * This is the function pointer type for NTP synchronization result callbacks.
 * The callback function is called when NTP synchronization completes or fails.
 *
 * @param cli_id    Handle of the NTP client. Obtained by liot_ntp_sync()
 * @param result    NTP result code (refer to liot_ntp_error_code_e)
 * @param sync_time Synchronized time (valid only when result is LIOT_NTP_SUCCESS)
 * @param arg       Callback parameter. Passed by liot_ntp_sync()
 */
typedef void (*liot_ntp_sync_result_cb)(ntp_client_id cli_id, int result, struct tm *sync_time, void *arg);

/**
 * @brief Enable NTP time synchronization function
 *
 * This function starts an NTP time synchronization session with the specified NTP server
 * using the provided configuration options.
 *
 * @param host         Address of the NTP server (hostname or IP address)
 * @param user_option  NTP configuration information (refer to liot_ntp_sync_option)
 * @param cb           Callback function. Used to notify NTP synchronization result and obtained time
 *                     (refer to liot_ntp_sync_result_cb)
 * @param arg          Callback parameter for NTP synchronization result notification function
 * @param error_code   Pointer to store NTP result code for function call (refer to liot_ntp_error_code_e)
 * @return Handle of the NTP client (>0 on success, 0 on failure)
 */
ntp_client_id liot_ntp_sync(
    const char *host, liot_ntp_sync_option *user_option, liot_ntp_sync_result_cb cb, void *arg, int *error_code);

#endif