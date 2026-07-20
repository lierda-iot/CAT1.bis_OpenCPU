#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "hw_network.h"
#include "liot_datacall.h"
#include "liot_sim.h"
#include "liot_nw.h"
#include "liot_dev.h"
#include "liot_os.h"
#include "liot_log.h"

static bool s_network_ready_posted = false;
static bool s_network_init_done = false;

static void ps_event_cb(Liot_PsEvent_e eventId, void *param, UINT32 paramLen)
{
    (void)param;
    (void)paramLen;
    event_t evt = {0};

    if (!s_network_init_done) {
        liot_trace("[NET] PS event ignored during init (0x%x)", eventId);
        return;
    }

    switch (eventId) {
    case LIOT_PS_EVENT_BEARER_ACTED:
    case LIOT_PS_EVENT_NETIF_ACTIVATED:
        liot_trace("[NET] PS event: network ready (0x%x)", eventId);
        if (!s_network_ready_posted) {
            s_network_ready_posted = true;
            evt.eventId = EVT_NETWORK_READY;
            frameworkPostEvent(&evt);
        }
        break;
    case LIOT_PS_EVENT_BEARER_DEACTED:
    case LIOT_PS_EVENT_NETIF_DEACTIVATED:
    case LIOT_PS_EVENT_NETIF_OOS:
        liot_trace("[NET] PS event: network fail (0x%x)", eventId);
        s_network_ready_posted = false;
        evt.eventId = EVT_NETWORK_FAIL;
        frameworkPostEvent(&evt);
        break;
    default:
        break;
    }
}

bool networkModuleInit(const network_config_t *cfg)
{
    liot_datacall_errcode_e ret = Liot_PsEventCb(ps_event_cb);
    if (ret != LIOT_DATACALL_SUCCESS) {
        liot_trace("[NET] Liot_PsEventCb register FAILED (%d)", ret);
    } else {
        liot_trace("[NET] PsEventCb registered");
    }

    liot_rtos_task_sleep_ms(1000);

    liot_sim_status_e simStatus = LIOT_SIM_STATUS_UNKNOW;
    liot_sim_get_card_status(0, &simStatus);
    liot_trace("[NET] SIM status=%d", simStatus);

    if (simStatus == LIOT_SIM_STATUS_NOSIM || simStatus == LIOT_SIM_STATUS_UNKNOW) {
        liot_trace("[NET] SIM error");
        event_t evt = {.eventId = EVT_SIM_ERROR};
        frameworkPostEvent(&evt);
        return false;
    }

    int times = 0;
    int r;
    while (LIOT_DATACALL_SUCCESS != (r = liot_network_register_wait(0, 120)) && times < 10) {
        times++;
        liot_rtos_task_sleep_s(1);
    }
    if (r != LIOT_DATACALL_SUCCESS) {
        liot_trace("[NET] network register failed");
        event_t evt = {.eventId = EVT_NETWORK_FAIL};
        frameworkPostEvent(&evt);
        return false;
    }

    liot_set_data_call_asyn_mode(0, 1, 0);

    r = liot_start_data_call(0, 1, LIOT_DATA_TYPE_IP,
                             (char *)cfg->apn, (char *)cfg->username, (char *)cfg->password,
                             LIOT_DATA_AUTH_TYPE_NONE);
    if (r != 0) {
        liot_trace("[NET] data call failed");
        s_network_init_done = true;
        event_t evt = {.eventId = EVT_NETWORK_FAIL};
        frameworkPostEvent(&evt);
        return false;
    }

    s_network_init_done = true;

    if (!s_network_ready_posted && liot_datacall_get_sim_profile_is_active(0, 1)) {
        liot_trace("[NET] network already active, post EVT_NETWORK_READY");
        s_network_ready_posted = true;
        event_t evt = {.eventId = EVT_NETWORK_READY};
        frameworkPostEvent(&evt);
    }

    return true;
}

void networkDisconnect(void)
{
    liot_stop_data_call(0, 1);
    liot_dev_set_modem_fun(0, 0, 0);
}
