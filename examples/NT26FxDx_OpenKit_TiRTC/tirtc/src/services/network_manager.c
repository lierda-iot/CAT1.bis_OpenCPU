/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file network_manager.c
 * @brief Single owner for LTE registration, CID1 data call and network time.
 *
 * TiRTC must reuse this PDP connection instead of starting a second CID1
 * owner.  This task never draws the OLED, so the UI stays responsive while
 * registration or carrier time synchronization is slow.
 *
 * Time comes from the modem system clock updated by the cellular network
 * (CTZU/NITZ), matching the EC718 reference application's time_time() /
 * time_localtime() path.  It does not contact an external time server.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "liot_datacall.h"
#include "liot_log.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_rtc.h"
#include "network_manager.h"

#define NET_SIM_ID               0
#define NET_PDP_CID              1
#define CHINA_TIMEZONE_QUARTERS  32
#define NET_RETRY_SECONDS        10
#define TIME_RETRY_SECONDS       30
#define TIME_WAIT_SECONDS        60
#define NET_MONITOR_SECONDS      5
#define NETWORK_TIME_MIN_YEAR    2024

static volatile demo_net_state_e s_net_state = DEMO_NET_INIT;
/* PDP readiness is independent of carrier/system-time readiness. */
static volatile bool s_data_ready;
/* Once synchronized, the modem RTC remains usable while 4G reconnects. */
static volatile bool s_time_ready;
static volatile bool s_nitz_seen;
static volatile long s_nitz_abs_time;

static bool net_ipv4_call_is_ready(const liot_data_call_info_t *info)
{
    if (info == NULL)
    {
        return false;
    }

    if (info->ip_version != LIOT_DATA_TYPE_IP &&
        info->ip_version != LIOT_DATA_TYPE_IPV4V6)
    {
        return false;
    }

    /* F6D_A's generic CID1 getter reports state=ACTIVING (1) after it has
     * already supplied a usable IPv4 address; requiring ACTIVED (2) therefore
     * rejects a live bearer forever.  The caller separately verifies that the
     * profile is active, so a non-zero address is the reliable readiness
     * boundary on this base package. */
    return info->v4.addr.ip.addr != 0U;
}

static bool net_default_pdn_is_ready(
    const liot_data_call_default_pdn_info_s *info)
{
    if (info == NULL)
    {
        return false;
    }

    if (info->ip_version != LIOT_DATA_TYPE_IP &&
        info->ip_version != LIOT_DATA_TYPE_IPV4V6)
    {
        return false;
    }

    return info->ipv4.addr != 0U;
}

static void net_set_state(demo_net_state_e state)
{
    s_net_state = state;
    liot_trace("[NET] state=%d\r\n", (int)state);
}

demo_net_state_e demo_net_time_get_state(void)
{
    return s_net_state;
}

bool demo_net_time_is_data_ready(void)
{
    return s_data_ready;
}

bool demo_net_time_get_local(liot_rtc_time_s *time_now)
{
    if (time_now == NULL || !s_time_ready)
    {
        return false;
    }
    return liot_rtc_get_localtime(time_now) == LIOT_RTC_SUCCESS;
}

static bool net_local_time_is_valid(const liot_rtc_time_s *time_now)
{
    return time_now != NULL &&
           time_now->tm_year >= NETWORK_TIME_MIN_YEAR &&
           time_now->tm_year <= 2100 &&
           time_now->tm_mon >= 1 && time_now->tm_mon <= 12 &&
           time_now->tm_mday >= 1 && time_now->tm_mday <= 31 &&
           time_now->tm_hour >= 0 && time_now->tm_hour <= 23 &&
           time_now->tm_min >= 0 && time_now->tm_min <= 59 &&
           time_now->tm_sec >= 0 && time_now->tm_sec <= 59;
}

static void net_network_event_cb(uint8_t sim_id, unsigned int ind_type,
                                 void *ctx)
{
    const liot_nw_nitz_time_info_s *nitz;

    if (sim_id != NET_SIM_ID ||
        ind_type != LIOT_NW_NITZ_TIME_UPDATE_IND || ctx == NULL)
    {
        return;
    }

    nitz = (const liot_nw_nitz_time_info_s *)ctx;
    if (nitz->abs_time > 0)
    {
        s_nitz_abs_time = nitz->abs_time;
        s_nitz_seen = true;
        liot_trace("[NET] NITZ update time='%s' abs=%ld\r\n",
                   nitz->nitz_time, nitz->abs_time);
    }
}

static int net_start_data(void)
{
    int ret;
    int retry;
    const char *apn = "";
    unsigned int apn_len = 0U;
    bool profile_active;
    Liot_DataCallCFG_t default_cfg;
    liot_data_call_info_t info;
    liot_data_call_default_pdn_info_s default_pdn;

    s_data_ready = false;
    net_set_state(DEMO_NET_REGISTERING);
    ret = liot_network_register_wait(NET_SIM_ID, 120);
    if (ret != LIOT_DATACALL_SUCCESS)
    {
        liot_trace("[NET] LTE registration failed ret=0x%x\r\n", ret);
        return -1;
    }
    liot_trace("[NET] LTE registered\r\n");

    memset(&default_cfg, 0, sizeof(default_cfg));
    default_cfg.method = LIOT_DATACALL_APN_GET;
    ret = Liot_DataCallCfgDefaultEpsBearer(&default_cfg);
    if (ret == LIOT_DATACALL_SUCCESS && default_cfg.apn_len > 0)
    {
        apn = default_cfg.apn;
        apn_len = (unsigned int)default_cfg.apn_len;
    }
    /* Private APN names may identify a customer's carrier network.  Length
     * and source are sufficient for diagnostics; never print the value. */
    liot_trace("[NET] default APN from modem len=%u ret=0x%x\r\n",
               apn_len, ret);

    net_set_state(DEMO_NET_DATA_CALL);
    profile_active = liot_datacall_get_sim_profile_is_active(
        NET_SIM_ID, NET_PDP_CID);

    /* CID1 is the modem's default EPS bearer.  Its authoritative address is
     * exposed by liot_datacall_get_default_pdn_info().  On F6D_A the generic
     * liot_get_data_call_info(CID1) path can remain in ACTIVING even while the
     * default bearer already has a usable IPv4 address. */
    if (profile_active)
    {
        memset(&default_pdn, 0, sizeof(default_pdn));
        ret = liot_datacall_get_default_pdn_info(NET_SIM_ID, &default_pdn);
        if (ret == LIOT_DATACALL_SUCCESS &&
            net_default_pdn_is_ready(&default_pdn))
        {
            liot_trace("[NET] CID%d default PDN ready type=%d IP=%s\r\n",
                       NET_PDP_CID, default_pdn.ip_version,
                       liot_ip4addr_ntoa(&default_pdn.ipv4));
            s_data_ready = true;
            return 0;
        }
        liot_trace("[NET] CID%d default PDN pending ret=0x%x type=%d\r\n",
                   NET_PDP_CID, ret, default_pdn.ip_version);
    }

    if (!profile_active)
    {
        /* Keep an empty configured APN empty: CID1 then asks the modem/SIM
         * profile to select its provisioned default.  APNTEST in Lierda's
         * generic data-call demo is a CID2 test value, not a product fallback. */
        liot_set_data_call_asyn_mode(NET_SIM_ID, NET_PDP_CID, false);
        ret = liot_start_data_call(NET_SIM_ID, NET_PDP_CID,
                                   LIOT_DATA_TYPE_IP, (CHAR *)apn,
                                   (CHAR *)"", (CHAR *)"",
                                   LIOT_DATA_AUTH_TYPE_NONE);
        liot_trace("[NET] start CID%d APN len=%u ret=0x%x\r\n",
                   NET_PDP_CID, apn_len, ret);
        if (ret != LIOT_DATACALL_SUCCESS &&
            ret != LIOT_DATACALL_REPEAT_ACTIVE_ERR)
        {
            return -2;
        }
    }

    for (retry = 0; retry < 30; retry++)
    {
        profile_active = liot_datacall_get_sim_profile_is_active(
            NET_SIM_ID, NET_PDP_CID);

        if (profile_active)
        {
            memset(&default_pdn, 0, sizeof(default_pdn));
            ret = liot_datacall_get_default_pdn_info(
                NET_SIM_ID, &default_pdn);
            if (ret == LIOT_DATACALL_SUCCESS &&
                net_default_pdn_is_ready(&default_pdn))
            {
                liot_trace("[NET] CID%d default PDN ready type=%d IP=%s\r\n",
                           NET_PDP_CID, default_pdn.ip_version,
                           liot_ip4addr_ntoa(&default_pdn.ipv4));
                s_data_ready = true;
                return 0;
            }
        }

        memset(&info, 0, sizeof(info));
        ret = liot_get_data_call_info(NET_SIM_ID, NET_PDP_CID, &info);
        if (profile_active && ret == LIOT_DATACALL_SUCCESS &&
            net_ipv4_call_is_ready(&info))
        {
            liot_trace("[NET] CID%d data ready type=%d state=%d IP=%s\r\n",
                       NET_PDP_CID, info.ip_version, info.v4.state,
                       liot_ip4addr_ntoa(&info.v4.addr.ip));
            s_data_ready = true;
            return 0;
        }

        if ((retry % 5) == 0)
        {
            liot_trace("[NET] CID%d waiting active=%d info_ret=0x%x "
                       "type=%d state=%d\r\n",
                       NET_PDP_CID, profile_active ? 1 : 0, ret,
                       info.ip_version, info.v4.state);
        }
        liot_rtos_task_sleep_s(1);
    }

    liot_trace("[NET] CID%d activation timeout\r\n", NET_PDP_CID);
    return -2;
}

static int net_wait_network_time(void)
{
    unsigned int waited;
    liot_errcode_rtc_e rtc_ret;
    liot_rtc_time_s local_time;

    net_set_state(DEMO_NET_TIME_SYNC);
    for (waited = 0; waited < TIME_WAIT_SECONDS; waited++)
    {
        if (!liot_datacall_get_sim_profile_is_active(
                NET_SIM_ID, NET_PDP_CID))
        {
            return -2;
        }

        /* Read the modem-maintained RTC directly, like the EC718 reference
         * time_time()/time_localtime() path.  Do not query the additional
         * network-time helper here because this base package can trigger an
         * external-server fallback internally when the clock is not ready. */
        memset(&local_time, 0, sizeof(local_time));
        rtc_ret = liot_rtc_get_localtime(&local_time);
        if (rtc_ret == LIOT_RTC_SUCCESS &&
            net_local_time_is_valid(&local_time))
        {
            liot_trace("[NET] network time ready source=%s NITZ=%ld "
                       "Beijing=%04d-%02d-%02d %02d:%02d:%02d\r\n",
                       s_nitz_seen ? "NITZ" : "system-rtc",
                       s_nitz_abs_time,
                       local_time.tm_year, local_time.tm_mon,
                       local_time.tm_mday, local_time.tm_hour,
                       local_time.tm_min, local_time.tm_sec);
            return 0;
        }

        if ((waited % 10U) == 0U)
        {
            liot_trace("[NET] waiting carrier time elapsed=%us "
                       "nitz_seen=%d rtc_ret=0x%x year=%d\r\n",
                       waited, s_nitz_seen ? 1 : 0,
                       (unsigned int)rtc_ret, local_time.tm_year);
        }
        liot_rtos_task_sleep_s(1);
    }

    liot_trace("[NET] carrier time unavailable; network may not provide NITZ\r\n");
    return -2;
}

void demo_net_time_task(void *argv)
{
    liot_nw_errcode_e nw_ret;
    liot_errcode_rtc_e rtc_ret;

    (void)argv;

    liot_trace("[NET] owner task started: SIM0 CID1\r\n");
    nw_ret = liot_nw_register_cb(net_network_event_cb);
    liot_trace("[NET] NITZ callback register ret=0x%x\r\n",
               (unsigned int)nw_ret);

    /* Enable automatic network-provided time before waiting for registration.
     * The setting takes effect immediately and is persisted by the modem. */
    nw_ret = liot_nw_set_ctzu_switch(true);
    liot_trace("[NET] CTZU enable ret=0x%x state=%d\r\n",
               (unsigned int)nw_ret,
               liot_nw_get_ctzu_switch() ? 1 : 0);

    rtc_ret = liot_rtc_set_timezone(CHINA_TIMEZONE_QUARTERS);
    liot_trace("[NET] timezone UTC+8 ret=0x%x\r\n",
               (unsigned int)rtc_ret);

    while (1)
    {
        while (net_start_data() != 0)
        {
            s_data_ready = false;
            net_set_state(DEMO_NET_RETRY);
            liot_rtos_task_sleep_s(NET_RETRY_SECONDS);
        }

        if (!s_time_ready)
        {
            while (s_data_ready && net_wait_network_time() != 0)
            {
                if (!liot_datacall_get_sim_profile_is_active(
                        NET_SIM_ID, NET_PDP_CID))
                {
                    s_data_ready = false;
                    break;
                }
                net_set_state(DEMO_NET_RETRY);
                liot_rtos_task_sleep_s(TIME_RETRY_SECONDS);
            }

            if (!s_data_ready)
            {
                net_set_state(DEMO_NET_RETRY);
                liot_rtos_task_sleep_s(NET_RETRY_SECONDS);
                continue;
            }
            s_time_ready = true;
        }

        net_set_state(DEMO_NET_READY);
        while (liot_datacall_get_sim_profile_is_active(
                   NET_SIM_ID, NET_PDP_CID))
        {
            liot_rtos_task_sleep_s(NET_MONITOR_SECONDS);
        }

        s_data_ready = false;
        net_set_state(DEMO_NET_RETRY);
        liot_trace("[NET] CID%d lost; reconnecting\r\n", NET_PDP_CID);
        liot_rtos_task_sleep_s(NET_RETRY_SECONDS);
    }
}
