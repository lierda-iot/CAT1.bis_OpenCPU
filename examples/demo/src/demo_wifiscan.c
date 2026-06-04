/**
 * @file liot_wifiscan_demo.c
 * @brief WiFi scanning demonstration application
 * @version 1.0.0
 * @date 2025-8-22
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 */
/**
 * This application showcases WiFi scanning capabilities by performing both
 * synchronous and asynchronous scans. It demonstrates how to configure scan
 * parameters, process scan results, and handle scan completion callbacks.
 *  This file demonstrates WiFi scanning functionality including:
 * - Synchronous and asynchronous scanning modes
 * - Scan parameter configuration
 * - Scan result processing and output
 * - Callback mechanism for async operations
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_wifiscan.h"

/*===========================================================================
 * Functions
 ===========================================================================*/
/**
 * @brief Outputs WiFi access point scan results in human-readable format
 * @param p_ap_infos Pointer to WiFi AP information structure containing scan results
 * @note This function will print RSSI, BSSID and channel information for each detected AP
 */
void liot_wifiscan_ap_info_output(liot_wifi_ap_info_s *p_ap_infos)
{
    if (NULL == p_ap_infos)
    {
        return;
        liot_trace("result NULL");
    }

    for (uint16 i = 0; i < p_ap_infos->bssidNum; i++)
    {
        liot_trace("WIFISCAN:(-,-,%d,%X:%X:%X:%X:%X:%X,%d",
                   p_ap_infos->rssi[i],
                   p_ap_infos->bssid[i][0],
                   p_ap_infos->bssid[i][1],
                   p_ap_infos->bssid[i][2],
                   p_ap_infos->bssid[i][3],
                   p_ap_infos->bssid[i][4],
                   p_ap_infos->bssid[i][5],
                   p_ap_infos->channel[i]);
    }
}

/**
 * @brief Callback function for asynchronous WiFi scan completion
 * @param msg_buf Pointer to message buffer containing scan results and status
 * @note This function is registered with liot_wifiscan_register_cb() and will be
 *       invoked automatically when an asynchronous scan completes
 */
void liot_wifiscan_app_callback(liot_wifiscan_ind_msg_s *msg_buf)
{
    liot_wifiscan_close();

    if ((LIOT_WIFISCAN_SUCCESS == msg_buf->msg_err_code) && (NULL != msg_buf->msg_data))
    {
        liot_wifi_ap_info_s *scan_result = msg_buf->msg_data;
        liot_trace("ap_cnt=%d", scan_result->bssidNum);
        liot_wifiscan_ap_info_output(scan_result);
    }
}

/**
 * @brief Starts an asynchronous WiFi scan operation
 * @note This function configures scan parameters, registers the callback function,
 *       opens the WiFi scanner, and initiates the asynchronous scan process
 */
void liot_wifiscan_async_start(void)
{
    // Configure scan parameters: time, rounds, max APs, timeout and priority
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_option_set(LIOT_WIFI_SCAN_TIME_VAL_DEF,
                                                          LIOT_WIFI_SCAN_DEFAULT_ROUND,
                                                          LIOT_WIFI_SCAN_DEFAULT_AP_CNT,
                                                          LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_DEF,
                                                          LIOT_WIFI_SCAN_PRIORITY_VAL_DEF))
    {
        liot_trace("option set err");
        return;
    }
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_register_cb(liot_wifiscan_app_callback))
    {
        liot_trace("register cb err");
        return;
    }
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_open())
    {
        liot_trace("device open err");
        return;
    }
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_async())
    {
        liot_wifiscan_close();
        liot_trace("to do a async scan err");
        return;
    }
}

/**
 * @brief Performs a complete synchronous WiFi scanning sequence
 * @note This function implements a blocking scan operation with the following flow:
 *       1. Set scan options
 *       2. Open WiFi scanner
 *       3. Perform scan
 *       4. Close scanner
 *       5. Output results
 */
void liot_wifiscan_synchro_complete_flow(void)
{
    // Set scan priority to high (1) for synchronous operation
    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_option_set(LIOT_WIFI_SCAN_TIME_VAL_DEF,
                                                          LIOT_WIFI_SCAN_DEFAULT_ROUND,
                                                          LIOT_WIFI_SCAN_DEFAULT_AP_CNT,
                                                          LIOT_WIFI_SCAN_SCANTIMEOUT_VAL_DEF,
                                                          1))
    {
        liot_trace("option set err");
        return;
    }

    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_open())
    {
        liot_trace("device open err");
        return;
    }

    liot_wifi_ap_info_s ap_infos = {0};  // Initialize AP info structure

    if (LIOT_WIFISCAN_SUCCESS != liot_wifiscan_do(&ap_infos))
    {
        liot_wifiscan_close();
        liot_trace("to do a scan err");
        return;
    }

    liot_trace("ap_cnt=%d", ap_infos.bssidNum);
    liot_wifiscan_close();
    liot_wifiscan_ap_info_output(&ap_infos);
}

/**
 * @brief Main thread function for WiFi scan demonstration
 * @param argv Unused parameter
 * @note This thread runs indefinitely, alternating between:
 *       - Synchronous scan with 5 second delay
 *       - Asynchronous scan with 15 second delay
 */
void liot_wifiscan_demo_thread(void *argv)
{
    liot_trace("==========wifiscandemo start==========");
    liot_rtos_task_sleep_ms(1000);  // Initial delay for system stabilization
    while (1)
    {
        liot_wifiscan_synchro_complete_flow();
        liot_rtos_task_sleep_ms(5000);   // Wait 5 seconds after synchronous scan
        liot_wifiscan_async_start();
        liot_rtos_task_sleep_ms(15000);  // Wait 15 seconds after asynchronous scan
    }
}