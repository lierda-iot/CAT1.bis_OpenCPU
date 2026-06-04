/**
 * @file liot_ntp_demo.c
 * @brief NTP (Network Time Protocol) synchronization demo implementation
 *
 * This demo demonstrates how to use the NTP client to synchronize the device's time
 * with an NTP server. It includes network registration, data call setup, NTP synchronization,
 * and time management functions.
 * 
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.0
 * @date 2025-08-12
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_ntp.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_rtc.h"

static liot_sem_t ntp_semp;
static liot_sem_t nw_semp;
ntp_client_id ntp_cli_id = 0;
#define IPV6_TEST_ENABLEx

/**
 * @brief NTP synchronization event callback
 *
 * This callback function is called when an NTP synchronization event occurs.
 * It handles both successful and failed synchronization results.
 *
 * @param cli_id     NTP session ID
 * @param result     Event result code (LIOT_NTP_SUCCESS for success)
 * @param sync_time  Synchronized time structure (valid only when result is success)
 * @param arg        Event parameter (user-defined argument)
 */
static void ntp_sync_result_cb(ntp_client_id cli_id, int result, struct tm *sync_time, void *arg)
{
    if (ntp_cli_id != cli_id)
    {
        return;
    }

    if (result == LIOT_NTP_SUCCESS)
    {
        char time_str[256] = {0};

        snprintf(time_str,
                 256,
                 "%04d/%02d/%02d,%02d:%02d:%02d",
                 sync_time->tm_year + 1900,
                 sync_time->tm_mon + 1,
                 sync_time->tm_mday,
                 sync_time->tm_hour,
                 sync_time->tm_min,
                 sync_time->tm_sec);
        liot_trace("||||||||||||| ntp sync time:%s", time_str);
    }
    else
    {
        liot_trace("ntp sync failed :%d", result);
    }

    liot_rtos_semaphore_release(ntp_semp);
}

/**
 * @brief Network event callback function
 *
 * This callback function handles various network events including signal quality,
 * data registration status, and NITZ time updates.
 *
 * @param nSim      SIM card index
 * @param ind_type  Event type
 * @param ctx       Event context parameter (type depends on ind_type)
 */
static void liot_nw_ind_callback(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    char csq = 99;
    liot_nw_common_reg_status_info_s *liot_nw_msg = NULL;
    liot_nw_nitz_time_info_s *liot_nw_nitz_time_info = NULL;
    liot_trace("nSim=%d, ind_type=%x", nSim, ind_type);
    switch (ind_type)
    {
    case LIOT_NW_SIGNAL_QUALITY_IND:
        csq = *((char *)ctx);
        liot_trace("csq=%d", csq);
        break;
    case LIOT_NW_DATA_REG_STATUS_IND:
        liot_nw_msg = (liot_nw_common_reg_status_info_s *)ctx;
        liot_trace("regState=%d, lac=0x%X, cid=0x%X, act=%d",
                   liot_nw_msg->state,
                   liot_nw_msg->lac,
                   liot_nw_msg->cid,
                   liot_nw_msg->act);
        if (liot_nw_msg->state == LIOT_NW_REG_STATE_HOME_NETWORK)
        {
            liot_rtos_semaphore_release(nw_semp);
        }

        break;
    case LIOT_NW_NITZ_TIME_UPDATE_IND:
        liot_nw_nitz_time_info = (liot_nw_nitz_time_info_s *)ctx;
        liot_trace(
            "nitz_time=%s, abs_time=%ld", liot_nw_nitz_time_info->nitz_time, liot_nw_nitz_time_info->abs_time);
        break;
    }
}

/**
 * @brief NTP synchronization main function
 *
 * This is the main thread function for the NTP demo. It performs the following operations:
 * 1. Initializes semaphores for synchronization
 * 2. Registers network status callback
 * 3. Waits for network registration
 * 4. Starts data call
 * 5. Gets data call information
 * 6. Performs time management operations
 * 7. Runs NTP synchronization multiple times
 * 8. Cleans up resources
 *
 * @param arg Thread argument (unused)
 */
void liot_ntp_demo_thread(void *arg)
{
    int ret     = 0;
    int run_num = 1;
    int cid     = 1;
    liot_data_call_info_t info;
    uint8_t nSim = 0;

    liot_trace("==========ntp demo start ==========");
    /* Initialize semaphore ntp_semp, initial state is unavailable, used to wait for NTP synchronization completion */
    liot_rtos_semaphore_create(&ntp_semp, 0);
    /* Initialize network semaphore (initial state is unavailable, used to wait for network registration) */
    liot_rtos_semaphore_create(&nw_semp, 0);
    /* Register network status callback */
    liot_nw_register_cb(liot_nw_ind_callback);
    /* Wait until network registration is complete */
    liot_rtos_semaphore_wait(nw_semp, LIOT_WAIT_FOREVER);

    /* Dial connection */
    liot_trace("===start data call====");
    liot_start_data_call(nSim, cid, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("===data call result:%d", ret);
    if (ret != 0)
    {
        liot_trace("====data call failure!!!!=====");
    }

    /* Dial result */
    memset(&info, 0x00, sizeof(liot_data_call_info_t));
    ret = liot_get_data_call_info(nSim, cid, &info);
    if (ret != 0)
    {
        liot_trace("liot_get_data_call_info ret: %d", ret);
        liot_stop_data_call(nSim, cid);
        goto exit;
    }
    liot_trace("info->cid: %d", info.cid);
    liot_trace("info->ip_version: %d", info.ip_version);

#ifdef IPV6_TEST_ENABLE
    liot_trace("info->v6.state: %d", info.v6.state);

    liot_ip6addr_ntoa(&info.v6.addr.ip);
    liot_trace("info.v6.addr.ip: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.ip));

    liot_ip6addr_ntoa(&info.v6.addr.pri_dns);
    liot_trace("info.v6.addr.pri_dns: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.pri_dns));

    liot_ip6addr_ntoa(&info.v6.addr.sec_dns);
    liot_trace("info.v6.addr.sec_dns: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.sec_dns));
#else
    liot_trace("info->v4.state: %d", info.v4.state);

    liot_ip4addr_ntoa(&info.v4.addr.ip);
    liot_trace("info.v4.addr.ip: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.ip));

    liot_ip4addr_ntoa(&info.v4.addr.pri_dns);
    liot_trace("info.v4.addr.pri_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.pri_dns));

    liot_ip4addr_ntoa(&info.v4.addr.sec_dns);
    liot_trace("info.v4.addr.sec_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
#endif

    /* Get timezone */
    int timezone_value = 0;
    liot_rtc_get_timezone(&timezone_value);
    liot_trace("timezone_value_trace: %d\r\n", timezone_value);

    /* Set timezone */
    liot_rtc_set_timezone(-23);

    /* Get local time and print */
    liot_rtc_time_s tm = {0};
    liot_rtc_get_localtime(&tm);
    liot_rtc_print_time(tm);

    /* Set new time and print 2022-11-10 16:50:30 */
    tm.tm_year = 2022;
    tm.tm_mon  = 11;
    tm.tm_mday = 10;
    tm.tm_wday = 4;
    tm.tm_hour = 16;
    tm.tm_min  = 50;
    tm.tm_sec  = 30;
    liot_rtc_set_time(&tm);
    liot_rtc_get_localtime(&tm);
    liot_rtc_print_time(tm);

    while (run_num <= 4)
    {
        liot_ntp_sync_option sync_option;
        int error_num = 0;
        liot_trace("==============ntp_test[%d]================\n", run_num);
        /* Configure NTP parameters */
        memset(&sync_option, 0x00, sizeof(liot_ntp_sync_option));
        sync_option.pdp_cid           = cid;
        sync_option.sim_id            = 0;
        sync_option.retry_cnt         = 3;
        sync_option.retry_interval_tm = 60;
        /* Synchronization time request */
        ntp_cli_id = liot_ntp_sync("ntp.aliyun.com", &sync_option, ntp_sync_result_cb, NULL, &error_num);

        if (ntp_cli_id != 0)
        {
            /* Wait for synchronization to complete */
            liot_rtos_semaphore_wait(ntp_semp, LIOT_WAIT_FOREVER);
            liot_trace("ntp success");
        }
        else
        {
            liot_trace("ntp failed");
        }
        liot_trace("==============ntp_test_end[%d]================\n", run_num);
        run_num++;
        liot_rtos_task_sleep_ms(1000);
    }

    /* Get time after synchronization */
    liot_rtc_get_time(&tm);
    liot_rtc_print_time(tm);
    liot_rtc_get_timezone(&timezone_value);
    liot_trace("timezone_value_trace: %d\r\n", timezone_value);

exit:
    /* Delete semaphore, exit thread */
    liot_rtos_semaphore_delete(ntp_semp);
    liot_trace("liot_ntp_demo_thread exit");
    liot_rtos_task_delete(NULL);

    return;
}
