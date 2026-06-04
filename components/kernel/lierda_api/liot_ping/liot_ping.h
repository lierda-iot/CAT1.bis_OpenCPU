/**
 * @File Name: liot_ping.h
 * @brief Ping API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.0
 * @date 2025-06-20
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */
#ifndef _LIOT_PING_H_
#define _LIOT_PING_H_

#include "liot_api_common.h"
#include "liot_type.h"

typedef int ping_session_id;

/*===========================================================================
 * Enum
 ===========================================================================*/

typedef enum
{
    LIOT_PING_OK = 0,
    LIOT_PING_ERR_INVALID_PARAM = (LIOT_COMPONENT_LWIP_SOCKET << 16) | 552 /* Invalid parameters   */,
    LIOT_PING_ERR_SOCKET_NEW_FAILURE = (LIOT_COMPONENT_LWIP_SOCKET << 16) | 554 /* Create socket failed */,
    LIOT_PING_ERR_SOCKET_BIND_FAILURE = (LIOT_COMPONENT_LWIP_SOCKET << 16) | 556 /* Socket bind failed   */,
    LIOT_PING_ERR_DNS_FAILURE = (LIOT_COMPONENT_LWIP_SOCKET << 16) | 565 /* DNS parse failed     */,
    LIOT_PING_ERR_TIMEOUT = (LIOT_COMPONENT_LWIP_SOCKET << 16) | 569 /* Operation timeout    */,
} liot_ping_result_code_e;

typedef enum
{
    LIOT_PING_STATS = 1,
    LIOT_PING_SUMMARY = 2,
} liot_ping_event_type_e;

/*===========================================================================
 * Struct
 ===========================================================================*/
typedef struct
{
    uint32_t num_data_bytes;         /* Data byte size of the ping packet (default: 56 bytes, range: 1-65535) */
    uint32_t ping_response_time_out; /* Wait time for ping response, ms (default: 20000 ms, range: 1-60000)   */
    uint32_t num_pings;              /* Number of times to ping (default: 4, range: 1-100)                   */
    uint32_t ttl;                    /* Time To Live for the ping packet (default: 255, range: 1-255)        */
} liot_ping_config_type_s;

typedef struct
{
    uint32_t ping_rtt;
    uint32_t ping_size;
    uint32_t ping_ttl;
    char resolved_ip_addr[256];
} liot_ping_stats_type_s;

typedef struct
{
    uint32_t min_rtt;        /* Minimum RTT so far, in millisecs           */
    uint32_t max_rtt;        /* Maximum RTT so far, in millisecs           */
    uint32_t avg_rtt;        /* Average RTT so far, in millisecs           */
    uint32_t num_pkts_sent;  /* Number of pings sent so far          */
    uint32_t num_pkts_recvd; /* Number of responses received so far  */
    uint32_t num_pkts_lost;  /* Number of responses not received     */
} liot_ping_summary_type_s;

/**
 * @brief Ping event callback function type
 *
 * This is the function pointer type for ping event callbacks.
 * The callback function is called when ping events occur.
 *
 * @param ping_session_id Ping event session ID. Obtained from liot_ping_start()
 * @param event_id        Ping event ID (refer to liot_ping_event_type_e)
 * @param evt_code        Ping result code (refer to liot_ping_result_code_e)
 * @param evt_param       Callback parameter (type depends on event_id):
 *                        - For LIOT_PING_STATS: pointer to liot_ping_stats_type_s
 *                        - For LIOT_PING_SUMMARY: pointer to liot_ping_summary_type_s
 */
typedef void (*liot_ping_event_cb)(ping_session_id ping_session_id, uint16_t event_id, int evt_code, void *evt_param);

/**
 * @brief Enable ping functionality
 *
 * This function starts a ping session to the specified remote host with the given configuration.
 *
 * @param cid          PDP context identifier
 * @param nSim         SIM card index, value: 0-1
 * @param host         Remote address for ping operation (hostname or IP address)
 * @param ping_options Settings for ping request data: request size, timeout, maximum number of requests
 *                     (refer to liot_ping_config_type_s)
 * @param cb_fcn       Event callback function for ping operation, sets the event type as ping start and handles ping results
 *                     (refer to liot_ping_event_cb)
 * @return Ping session ID on success (>0), 0 on failure to start ping
 *
 * @note This function is not thread-safe. Ensure proper synchronization in multi-threaded environments.
 */
ping_session_id liot_ping_start(
    int cid,
    int nSim,
    const char *host,
    liot_ping_config_type_s *ping_options,
    liot_ping_event_cb cb_fcn);

#endif
