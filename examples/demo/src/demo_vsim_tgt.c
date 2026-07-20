/**
 * @File Name: demo_vsim_tgt.c
 * @brief  
 * @Author : ljz email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2026-07-08
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 */
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_power.h"
#include "tgt_app.h"
#include "liot_vsim.h"
#include "liot_datacall.h"
#include "liot_nw.h"

typedef enum {
    TGT_SOFTSIM_EV_MIN,
    TGT_SOFTSIM_EV_CERT_SUCC = 1,
    TGT_SOFTSIM_EV_START_SIM = 2,
    TGT_SOFTSIM_EV_NETWORK_SUCC = 3,
    TGT_SOFTSIM_EV_DATA_SUCC = 4,
    TGT_SOFTSIM_EV_START_ABNORMAL = 5,
    TGT_SOFTSIM_EV_MAX,
} softsim_ev_t;

typedef enum {
    TGT_SOFTSIM_STATUS_MIN,
    TGT_SOFTSIM_DEVICE_INACITIVE = 1000,
    TGT_SOFTSIM_ACCOUNT_ERROR = 1001,
    TGT_SOFTSIM_ALLOCATION_SIM_ABNORMAL = 1002,
    TGT_SOFTSIM_NOSUITABLE_NETWORK = 1003,
    TGT_SOFTSIM_NETWORK_BUSY = 1004,
    TGT_SOFTSIM_STATUS_MAX,
} softsim_error_t;

typedef struct {
    int error_code;
} softsim_error_detail;

#define VSIM_EVT_DATA_READY     (1U << 0)
#define VSIM_EVT_NETWORK_OK     (1U << 1)
#define VSIM_EVT_ABNORMAL       (1U << 2)

#define VSIM_REBOOT_TIMEOUT_MS      (3600 * 1000)
#define VSIM_REBOOT_TIMEOUT_24H_MS  (24 * 3600 * 1000)

static liot_flag_t g_vsim_evt = NULL;
static volatile int g_vsim_error_code = 0;
static volatile uint32_t g_vsim_abnormal_time = 0;

static void vsim_event_callback(int event, char* param, int param_len)
{
    softsim_error_detail *err_detail = NULL;

    switch (event) {
    case TGT_SOFTSIM_EV_CERT_SUCC:
        liot_trace("TGT_SOFTSIM_EV_CERT_SUCC: Certificate Valid");
        break;
    case TGT_SOFTSIM_EV_START_SIM:
        liot_trace("TGT_SOFTSIM_EV_START_SIM: Vsim Starting");
        break;
    case TGT_SOFTSIM_EV_NETWORK_SUCC:
        liot_trace("TGT_SOFTSIM_EV_NETWORK_SUCC: Vsim Registered OK");
        liot_rtos_flag_release(g_vsim_evt, VSIM_EVT_NETWORK_OK, LIOT_FLAG_OR);
        break;
    case TGT_SOFTSIM_EV_DATA_SUCC:
        liot_trace("TGT_SOFTSIM_EV_DATA_SUCC: Data OK, Certificate Valid");
        liot_rtos_flag_release(g_vsim_evt, VSIM_EVT_DATA_READY, LIOT_FLAG_OR);
        break;
    case TGT_SOFTSIM_EV_START_ABNORMAL:
        if (param != NULL && param_len >= (int)sizeof(softsim_error_detail)) {
            err_detail = (softsim_error_detail *)param;
            g_vsim_error_code = err_detail->error_code;
        }
        g_vsim_abnormal_time = liot_rtos_get_running_time();
        liot_trace("TGT_SOFTSIM_EV_START_ABNORMAL: error_code=%d", g_vsim_error_code);
        liot_rtos_flag_release(g_vsim_evt, VSIM_EVT_ABNORMAL, LIOT_FLAG_OR);
        break;
    default:
        liot_trace("vsim_event_callback: unknown event=%d", event);
        break;
    }
}

static void vsim_abnormal_recovery_check(void)
{
    uint32_t timeout_ms = VSIM_REBOOT_TIMEOUT_MS;
    if (g_vsim_error_code == TGT_SOFTSIM_DEVICE_INACITIVE) {
        timeout_ms = VSIM_REBOOT_TIMEOUT_24H_MS;
    }

    uint32_t elapsed = liot_rtos_get_running_time() - g_vsim_abnormal_time;
    if (elapsed >= timeout_ms) {
        liot_trace("vsim: abnormal timeout(%d ms), error_code=%d, rebooting",
                   elapsed, g_vsim_error_code);
        liot_power_reset(LIOT_RESET_NORMAL);
    }
}

int vsim_datacall_test(uint8_t sim, int cid)
{
    UINT32 flags = 0;
    LiotOSStatus_t ret_flag;

    liot_trace("vsim_datacall_test: waiting for vsim event (max 1h)");
    ret_flag = liot_rtos_flag_wait(g_vsim_evt,
                                   VSIM_EVT_DATA_READY | VSIM_EVT_ABNORMAL,
                                   LIOT_FLAG_OR_CLEAR,
                                   &flags,
                                   VSIM_REBOOT_TIMEOUT_MS);

    if (ret_flag != 0) {
        liot_trace("vsim_datacall_test: timeout 1h, no event, rebooting");
        liot_power_reset(LIOT_RESET_NORMAL);
        return -1;
    }

    if (flags & VSIM_EVT_ABNORMAL) {
        liot_trace("vsim_datacall_test: abnormal event, error_code=%d, wait for recovery", g_vsim_error_code);
        ret_flag = liot_rtos_flag_wait(g_vsim_evt,
                                       VSIM_EVT_DATA_READY,
                                       LIOT_FLAG_OR_CLEAR,
                                       &flags,
                                       VSIM_REBOOT_TIMEOUT_MS);
        if (ret_flag != 0) {
            liot_trace("vsim_datacall_test: recovery timeout, triggering reboot");
            vsim_abnormal_recovery_check();
            return -1;
        }
    }

    liot_trace("vsim_datacall_test: vsim data ready, checking network register");
    int ret = liot_network_register_wait(sim, 60);
    if (LIOT_DATACALL_SUCCESS != ret) {
        liot_trace("vsim_datacall_test: network register failed ret=%d", ret);
        return -1;
    }

    liot_trace("vsim_datacall_test: network register success");
    return 0;
}


/**
  \fn      void AppTask (void *argument)
  \brief   task app main entry function.
  \return
*/
void liot_vsim_tgt_demo_thread(void *argument)
{
    int nSim = 0;
    int cid = 1;
    int ret = 0;
    liot_data_call_info_t info;

    g_vsim_evt = NULL;
    liot_rtos_flag_create(&g_vsim_evt);

    liot_vsim_set("TGT", true);
    tgt_app_service_init();
    softSimEvNotifyRegisterCallback(vsim_event_callback);
    liot_trace("liot_vsim_demo_thread entry");
    liot_rtos_task_sleep_ms(2000);

    vsim_datacall_test(nSim, cid);

    while(1)
    {

        liot_vsim_info_t vsimInfo;
        memset(&vsimInfo, 0, sizeof(liot_vsim_info_t));
        liot_vsim_get(&vsimInfo);
        liot_trace("liot_vsim_get vsimInfo.vsimMode=%d,vsimInfo.name=%s",vsimInfo.vsimMode,vsimInfo.name);

        ret = liot_get_data_call_info(nSim, cid, &info);
        if (ret != 0)
        {
            liot_trace("liot_get_data_call_info ret: %d", ret);
            liot_rtos_task_delete(NULL);
            return;
        }
        liot_trace("info->cid: %d", info.cid);
        liot_trace("info->ip_version: %d", info.ip_version);

        liot_trace("info->v4.state: %d", info.v4.state);
        liot_trace("info.v4.addr.ip: %s", liot_ip4addr_ntoa(&info.v4.addr.ip));
        liot_trace("info.v4.addr.pri_dns: %s", liot_ip4addr_ntoa(&info.v4.addr.pri_dns));
        liot_trace("info.v4.addr.sec_dns: %s", liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
       
		liot_rtos_task_sleep_ms(5000);
    }
}