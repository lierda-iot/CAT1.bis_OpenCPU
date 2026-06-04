/**
 * @file liot_ping_demo.c
 * @brief A demo application for testing the ping functionality on a CAT1 module
 *
 * This demo demonstrates how to use the ping functionality to test network connectivity.
 * It includes network registration, data call setup (optional), ping configuration,
 * and handling of ping events through callback functions.
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.1
 * @date 2025-06-20
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
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_ping.h"

/* The first data call is activated by default at boot.
   If you need to enable the second dial-up connection, please define this macro.*/
#define LIOT_MULTIPLE_DATACALL

/* Global Variable */
static ping_session_id ping_sess_id = 0;
static liot_sem_t ping_semp;
static liot_sem_t nw_semp;

/**
 * @brief PING event callback function
 *
 * This callback function handles ping events including individual ping results
 * and summary statistics. It is called when ping operations complete or timeout.
 *
 * @param session_id  PING session ID
 * @param event_id    Event ID (LIOT_PING_STATS or LIOT_PING_SUMMARY)
 * @param evt_code    Event result code (LIOT_PING_OK for success)
 * @param evt_param   Event parameter (type depends on event_id)
 */
static void ping_event_cb(ping_session_id session_id, uint16_t event_id, int evt_code, void *evt_param)
{
    liot_trace("ping_event_cb   %d====  session_id=%d,event_id=%d,evt_code=%d", __LINE__, session_id, event_id, evt_code);

    if (session_id != ping_sess_id)
    {
        return;
    }
    switch (event_id)
    {
    case LIOT_PING_STATS:
    {
        /* Single ping succeeded */
        if (evt_code == LIOT_PING_OK)
        {
            liot_ping_stats_type_s *stats = (liot_ping_stats_type_s *)evt_param;
            liot_trace(" from %s's reply: bytes=%d, rtt=%d, ttl=%d",
                       stats->resolved_ip_addr,
                       stats->ping_size,
                       stats->ping_rtt,
                       stats->ping_ttl);
        }
    }
    break;
    case LIOT_PING_SUMMARY:
    {
        /* Ping completed successfully */
        if (evt_code == LIOT_PING_OK)
        {
            liot_ping_summary_type_s *summary = (liot_ping_summary_type_s *)evt_param;
            liot_trace(
                "statistics info: send=%d packets, recv=%d packets, lost=%d packets, max_rtt=%d, min_rtt=%d, avg_rtt=%d",
                summary->num_pkts_sent,
                summary->num_pkts_recvd,
                summary->num_pkts_lost,
                summary->max_rtt,
                summary->min_rtt,
                summary->avg_rtt);
        }
        else
        {
            liot_trace("ping timeout!!!!!");
        }
        ping_sess_id = 0;
        liot_rtos_semaphore_release(ping_semp);
    }
    break;
    default:
        liot_trace("ping_event_cb   %d====", __LINE__);
        break;
    }
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

#ifdef LIOT_MULTIPLE_DATACALL
/**
 * @brief Start a data call with provided parameters
 *
 * This function initiates a data call with the specified SIM card index,
 * PDP context identifier, and APN name. It also retrieves and displays
 * data call information after the call is established.
 *
 * @param nSim  SIM card index, value: 0-1
 * @param cid   PDP context identifier
 * @param apn   APN name
 * @return      0 for success, non-zero for failure
 *
 * @note If no APN or target address is provided, default values can be used
 *       to reduce user configuration complexity.
 */
static int start_data_call(uint8_t nSim, int cid, char *apn)
{
    int ret = 0;
    liot_data_call_info_t info;
    if (apn == NULL || strlen(apn) == 0)
    {
        apn = "default_apn";
        liot_trace("No APN provided. Using default APN: %s", apn);
    }
    ret = liot_start_data_call(nSim, cid, LIOT_DATA_TYPE_IPV4V6, apn, "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("Data call result: %d", ret);
    liot_rtos_task_sleep_ms(500);

    /* Get data call information */
    memset(&info, 0x00, sizeof(liot_data_call_info_t));
    ret = liot_get_data_call_info(nSim, cid, &info);
    if (ret != 0)
    {
        liot_trace("liot_get_data_call_info ret: %d", ret);
        liot_stop_data_call(nSim, cid);
        return ret;
    }
    liot_trace("info.cid: %d", info.cid);
    liot_trace("info.ip_version: %d", info.ip_version);

#ifdef IPV6_TEST_ENABLE
    liot_trace("info.v6.state: %d", info.v6.state);

    liot_ip6addr_ntoa(&info.v6.addr.ip);
    liot_trace("info.v6.addr.ip: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.ip));

    liot_ip6addr_ntoa(&info.v6.addr.pri_dns);
    liot_trace("info.v6.addr.pri_dns: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.pri_dns));

    liot_ip6addr_ntoa(&info.v6.addr.sec_dns);
    liot_trace("info.v6.addr.sec_dns: %s\r\n", liot_ip6addr_ntoa(&info.v6.addr.sec_dns));
#else
    liot_trace("info.v4.state: %d", info.v4.state);

    liot_ip4addr_ntoa(&info.v4.addr.ip);
    liot_trace("info.v4.addr.ip: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.ip));

    liot_ip4addr_ntoa(&info.v4.addr.pri_dns);
    liot_trace("info.v4.addr.pri_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.pri_dns));

    liot_ip4addr_ntoa(&info.v4.addr.sec_dns);
    liot_trace("info.v4.addr.sec_dns: %s\r\n", liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
#endif
    return ret;
}
#endif

/**
 * @brief PING main thread function
 *
 * This is the main thread function for the PING demonstration. It performs the following operations:
 * 1. Initializes semaphores for synchronization
 * 2. Registers network status callback
 * 3. Waits for network registration
 * 4. Starts data call (if LIOT_MULTIPLE_DATACALL is defined)
 * 5. Configures ping parameters
 * 6. Starts ping request
 * 7. Waits for ping completion
 * 8. Cleans up resources
 *
 * @param arg No argument required (unused)
 */
void liot_ping_app_thread(void *arg)
{
    uint8_t nSim = 0;
#ifdef LIOT_MULTIPLE_DATACALL
    int cid = 2;
#else
    int cid = 1;
#endif

    /* Initialize ping semaphore (initial state is unavailable, used to wait for ping completion) */
    liot_rtos_semaphore_create(&ping_semp, 0);
    /* Initialize network semaphore (initial state is unavailable, used to wait for network registration) */
    liot_rtos_semaphore_create(&nw_semp, 0);
    /* Register network status callback */
    liot_nw_register_cb(liot_nw_ind_callback);
    /* Wait until network registration is complete */
    liot_rtos_semaphore_wait(nw_semp, LIOT_WAIT_FOREVER);

#ifdef LIOT_MULTIPLE_DATACALL
    liot_trace("[PING DEMO] Starting data call...");
    int ret = start_data_call(nSim, cid, "APNTEST");
    if (ret != 0)
    {
        liot_trace("[PING DEMO] Data call failed. Possible reasons: invalid APN, no network coverage, or SIM card issue. Please verify and try again.");
        goto exit;
    }
#endif
    /* Configure ping parameters */
    liot_ping_config_type_s ping_options;
    memset(&ping_options, 0x00, sizeof(liot_ping_config_type_s));
    ping_options.num_data_bytes = 56;
    ping_options.num_pings = 4;
    ping_options.ping_response_time_out = 20000;
    ping_options.ttl = 255;

    /* Start ping request */
#ifdef IPV6_TEST_ENABLE
    ping_sess_id = liot_ping_start(cid, nSim, "2001:da8:202:10::36", &ping_options, ping_event_cb);
#else
    ping_sess_id = liot_ping_start(cid, nSim, "www.163.com", &ping_options, ping_event_cb);
    liot_trace("[PING DEMO] ping_sess_id: %d", ping_sess_id);
#endif
    if (ping_sess_id != 0)
    {
        /* Wait for ping completes */
        liot_rtos_semaphore_wait(ping_semp, LIOT_WAIT_FOREVER);
        liot_trace("[PING DEMO] Ping success.");
    }
    else
    {
        liot_trace("[PING DEMO] Ping failed.");
        goto exit;
    }

exit:
    /* Delete semaphores and exit thread */
    if (ping_semp)
    {
        liot_rtos_semaphore_release(ping_semp);
        liot_rtos_semaphore_delete(ping_semp);
    }
    if (nw_semp)
    {
        liot_rtos_semaphore_release(nw_semp);
        liot_rtos_semaphore_delete(nw_semp);
    }
    liot_trace("[PING DEMO] liot_ping_app_thread exit");
    liot_rtos_task_delete(NULL);
    return;
}
