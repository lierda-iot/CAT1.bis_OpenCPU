/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai_net.h"

#include <string.h>

#include "ai_app_log.h"
#include "liot_datacall.h"
#include "liot_os.h"

#define AI_NET_INFO_RETRY_COUNT 30
#define AI_NET_INFO_RETRY_DELAY_MS 1000

static bool ai_net_data_call_info_ready(uint8_t sim_id, int pdp_cid, liot_data_call_info_t *info)
{
    int ret = liot_get_data_call_info(sim_id, pdp_cid, info);
    if (ret != LIOT_DATACALL_SUCCESS) {
        return false;
    }

    return (info->v4.state == LIOT_DATACALL_STATE_ACTIVED);
}

static void ai_net_log_ip_info(const liot_data_call_info_t *info)
{
    if (info == NULL) {
        return;
    }

    liot_trace("ai_net data call cid=%d ip_type=%d v4_state=%d",
               info->cid,
               info->ip_version,
               info->v4.state);
    liot_trace("ai_net ip=%s", liot_ip4addr_ntoa((liot_ip4_addr_t *)&info->v4.addr.ip));
    liot_trace("ai_net dns1=%s", liot_ip4addr_ntoa((liot_ip4_addr_t *)&info->v4.addr.pri_dns));
    liot_trace("ai_net dns2=%s", liot_ip4addr_ntoa((liot_ip4_addr_t *)&info->v4.addr.sec_dns));
}

int ai_net_start(const ai_app_config_t *cfg)
{
    int ret = 0;
    int times = 0;

    if (cfg == NULL) {
        return -1;
    }

    liot_trace("ai_net register wait start");
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(cfg->sim_id, 60)) && times < 5) {
        times++;
        liot_trace("ai_net register retry %d ret=%d", times, ret);
        liot_rtos_task_sleep_s(1);
    }
    if (ret != LIOT_DATACALL_SUCCESS) {
        liot_trace("ai_net register failed ret=%d", ret);
        return -2;
    }
    liot_trace("ai_net network registered");

    liot_set_data_call_asyn_mode(cfg->sim_id, cfg->pdp_cid, 0);
    ret = liot_start_data_call(cfg->sim_id,
                               cfg->pdp_cid,
                               3,
                               (CHAR *)"APNTEST",
                               (CHAR *)"",
                               (CHAR *)"",
                               0);
    if (ret != LIOT_DATACALL_SUCCESS) {
        liot_trace("ai_net data call failed ret=0x%x", ret);
        return -3;
    }
    liot_rtos_task_sleep_s(4);
    liot_trace("ai_net ready");
    return 0;
}
