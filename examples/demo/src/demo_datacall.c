/**
 * @file liot_datacall_demo.c
 * @brief Demonstration of data call operations using the Liot API.
 * @Version : 1.1
 * @Creat Date : 2025-06-26
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 *
 */

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "string.h"
#include "liot_dev.h"
#include "liot_log.h"
#include "liot_type.h"

/**
 * @brief Callback function for data call indication events.
 */
static void liot_datacall_ind_callback(uint8_t nSim, unsigned int ind_type, int profile_idx, bool result, void *ctx)
{
    liot_trace("nSim = %d, profile_idx=%d, ind_type=0x%x, result=%d", nSim, profile_idx, ind_type, result);

    liot_datacall_state_e *active_state = ctx;

    // Handle data call activation success event
    if (LIOT_DATACALL_ACT_RSP_IND == ind_type && true == result)
    {
        *active_state = LIOT_DATACALL_STATE_ACTIVED;
    }
    // Handle data call deactivation events
    else if (LIOT_DATACALL_DEACT_RSP_IND == ind_type || LIOT_DATACALL_PDP_DEACTIVE_IND == ind_type)
    {
        liot_trace("PDP Deactive(cid: %d)", profile_idx);
        *active_state = LIOT_DATACALL_STATE_IDLE;
    }
}


/**
 * @brief Callback function for network indication events.
 * This function is a test function for multi - task network callback.
 * 
 * @param nSim SIM card number.
 * @param ind_type Indication event type.
 * @param ctx Pointer to the context data.
 */
static void liot_nw_ind_callback(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    char csq = 99; // Default signal quality value
    liot_nw_common_reg_status_info_s *liot_nw_msg = NULL;
    liot_nw_nitz_time_info_s *liot_nw_nitz_time_info = NULL;
    liot_trace("nSim=%d, ind_type=%x", nSim, ind_type);
    switch (ind_type)
    {
        case LIOT_NW_SIGNAL_QUALITY_IND:
        {
            csq = *((char *)ctx); // Get the signal quality value
            liot_trace("csq=%d", csq);
        }
        break;

        case LIOT_NW_DATA_REG_STATUS_IND:
        {
            liot_nw_msg = (liot_nw_common_reg_status_info_s *)ctx; // Cast the context pointer
            liot_trace("regState=%d, lac=0x%X, cid=0x%X, act=%d",
                    liot_nw_msg->state,
                    liot_nw_msg->lac,
                    liot_nw_msg->cid,
                    liot_nw_msg->act);
        }
        break;

        case LIOT_NW_NITZ_TIME_UPDATE_IND:
        {
            liot_nw_nitz_time_info = (liot_nw_nitz_time_info_s *)ctx; // Cast the context pointer
            liot_trace(
                "nitz_time=%s, abs_time=%ld", liot_nw_nitz_time_info->nitz_time, liot_nw_nitz_time_info->abs_time);
        }
        break;
    }
}

void Liot_DataCallCallback(Liot_PsEvent_e eventId, void *param, UINT32 paramLen)
{
    switch (eventId)
    {
        case LIOT_PS_EVENT_BEARER_ACTED:
        {
            liot_trace("LIOT_PS_EVENT_BEARER_ACTED");
        }
        break;

        case LIOT_PS_EVENT_BEARER_DEACTED:
        {
            liot_trace("LIOT_PS_EVENT_BEARER_DEACTED");
        }
        break;

        case LIOT_PS_EVENT_NETIF_ACTIVATED:
        {
            liot_trace("LIOT_PS_EVENT_NETIF_ACTIVATED");
        }
        break;

        case LIOT_PS_EVENT_NETIF_DEACTIVATED:
        {
            liot_trace("LIOT_PS_EVENT_NETIF_DEACTIVATED");
        }
        break;

        case LIOT_PS_EVENT_NETIF_OOS:
        {
            liot_trace("LIOT_PS_EVENT_NETIF_OOS");
        }
        break;

        default:
        {
            liot_trace("unknown eventId");
        }
        break;
    }
}

void liot_apn_cfg_test()
{
    liot_datacall_errcode_e ret = LIOT_DATACALL_EXECUTE_ERR;
    Liot_DataCallCFG_t cfg;
    Liot_DataCallCFG_t default_cfg;
    memset(&cfg, 0, sizeof(Liot_DataCallCFG_t));
    memset(&default_cfg, 0, sizeof(Liot_DataCallCFG_t));
    
    //get default apn
    default_cfg.method = LIOT_DATACALL_APN_GET;
    ret = Liot_DataCallCfgDefaultEpsBearer(&default_cfg);
    if(LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("Liot_DataCallCfgDefaultEpsBearer fail");
        return;
    }

    liot_trace("get defuat apn %s, apn_len=%d, ip_version=%d", default_cfg.apn, default_cfg.apn_len, default_cfg.ip_version);

    //set 
    memcpy(cfg.apn, "cmnet", strlen("cmnet"));
    cfg.apn_len = strlen("cmnet");
    cfg.ip_version = LIOT_PS_PDN_TYPE_IP_V4V6;
    cfg.method = LIOT_DATACALL_APN_SET;
    ret = Liot_DataCallCfgDefaultEpsBearer(&cfg);
    if(LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("Liot_DataCallCfgDefaultEpsBearer fail");
        return;
    }

    liot_trace("Liot_DataCallCfgDefaultEpsBearer success");
    
    //get
    memset(&cfg, 0, sizeof(Liot_DataCallCFG_t));
    cfg.method = LIOT_DATACALL_APN_GET;
    ret = Liot_DataCallCfgDefaultEpsBearer(&cfg);
    if(LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("Liot_DataCallGetCfgDefaultEpsBearer fail");
        return;
    }

    liot_trace("get apn_name=%s, apn_len=%d, apn_type=%d", cfg.apn, cfg.apn_len, cfg.ip_version);

    //set
    default_cfg.method = LIOT_DATACALL_APN_SET;
    ret = Liot_DataCallCfgDefaultEpsBearer(&default_cfg);
    if(LIOT_DATACALL_SUCCESS != ret)
    {
        liot_trace("Liot_DataCallGetCfgDefaultEpsBearer fail");
        return;
    }

    liot_trace("set defuat apn %s, apn_len=%d, ip_version=%d success", default_cfg.apn, default_cfg.apn_len, default_cfg.ip_version);
}

/**
 * @brief Main thread function for the data call demonstration.
 */
void liot_datacall_demo_thread(void *argv)
{
    liot_datacall_errcode_e ret1 = Liot_PsEventCb(Liot_DataCallCallback);
    if(ret1 != LIOT_DATACALL_SUCCESS)
    {
        liot_trace("Liot_PsEventCb fail");
    }

    (void)argv;   

    liot_apn_cfg_test();

    // Initialize variables
    int ret = -1;
    uint8_t nSim = 0;
    int cid = 2;
    liot_datacall_state_e checkActive = LIOT_DATACALL_STATE_IDLE;
    bool isDataCallCBRegistered = false;
    bool isSim0Cid2Started = false;

    liot_rtos_task_sleep_ms(2000);
    liot_trace("==========data call demo start==========");
    liot_trace("==========wait for network register done==========");

    // Register network callback
    ret = liot_nw_register_cb(liot_nw_ind_callback);
    if (LIOT_NW_SUCCESS != ret)
    {
        liot_trace("liot_nw_register_cb failed, ret is 0x%x.", ret);
    }

    // Try to wait for network registration, up to 10 attempts
    int times = 0;
    while (LIOT_DATACALL_SUCCESS != (ret = liot_network_register_wait(nSim, 120)) && times < 10)
    {
        times++;
        liot_rtos_task_sleep_s(1);
    }

    if (LIOT_DATACALL_SUCCESS != ret)
    {
        // Clean up and exit on failure
        liot_trace("====network register failure!!!!!====");
        if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL))
        {
            liot_trace("task deleted failed");
            return;
        }
    }
    
    ret = liot_network_register_cereg_get(nSim);
    liot_trace("===liot_network_register_cereg_get: ret = %d", ret);

    liot_trace("====network registered!!!!====");
    // Process both synchronous and asynchronous modes
    for (UINT8 idx = 0; idx < 2; idx++)
    {
        // Reset state variables for each iteration
        checkActive = LIOT_DATACALL_STATE_IDLE;
        isDataCallCBRegistered = false;
        isSim0Cid2Started = false;

        // Set data call mode (sync/async)
        if (LIOT_DATACALL_SUCCESS != liot_set_data_call_asyn_mode(nSim, cid, idx))
        {
            // Clean up and exit on failure
            liot_trace("====set datacall asyn mode for nSim %d cid &d failure!!!!!====", nSim, cid);
            if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL))
            {
                liot_trace("task deleted failed");
                return;
            }
        }

        liot_trace("===%s process start====", (0 == idx ? "synchronous" : "asynchronous"));
        do
        {
            // Register data call callback
            liot_trace("===register data call callback====");
            ret = liot_datacall_register_cb(LIOT_DATACALL_REGISTER_ALL_SIM,
                                            LIOT_DATACALL_REGISTER_ALL_PDP,
                                            (liot_datacall_callback)liot_datacall_ind_callback,
                                            (void *)&checkActive);
            if (LIOT_DATACALL_SUCCESS != ret)
            {
                liot_trace("liot_datacall_register_cb failed, ret is 0x%x.", ret);
                break;
            }
            isDataCallCBRegistered = true;

            ret = liot_start_data_call(nSim, cid, LIOT_DATA_TYPE_IPV4V6, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
            if (LIOT_DATACALL_SUCCESS != ret)
            {
                liot_trace("liot_start_data_call ret: 0x%x.", ret);
                break;
            }
            isSim0Cid2Started = true;
            liot_trace("liot_start_data_call done.");

            // Wait for operation to complete based on mode
            if (0 == idx)
            {
                liot_rtos_task_sleep_ms(100);
            }
            else
            {
                liot_rtos_task_sleep_ms(500);
            }

            // Configure NAT settings
            uint32 tag_natmode = 1;
            uint32 cur_natmode = 0;
            ret = liot_datacall_get_nat(nSim, &cur_natmode);
            if (LIOT_DATACALL_SUCCESS != ret)
            {
                break;
            }

            if (cur_natmode != tag_natmode)
            {
                ret = liot_datacall_set_nat(nSim, tag_natmode);
                if (LIOT_DATACALL_SUCCESS != ret)
                {
                    break;
                }
            }

            // Check data call status and default PDN information
            liot_data_call_info_t info = {0};
            liot_data_call_default_pdn_info_s default_info = {0};
            times = 3;
            while (times--)
            {
                if (true == liot_datacall_get_sim_profile_is_active(nSim, cid))
                {
                    memset(&info, 0x00, sizeof(liot_data_call_info_t));
                    ret = liot_get_data_call_info(nSim, cid, &info);
                    liot_trace("liot_get_data_call_info ret: 0x%x", ret);
                    liot_trace("info->v4.state: %d, info.v6.state: %d", info.v4.state, info.v6.state);

                    if (LIOT_DATA_TYPE_IP == info.ip_version || LIOT_DATA_TYPE_IPV4V6 == info.ip_version)
                    {
                        liot_trace("info.v4.addr.ip: %s", liot_ip4addr_ntoa(&info.v4.addr.ip));
                    }
                    if (LIOT_DATA_TYPE_IPV6 == info.ip_version || LIOT_DATA_TYPE_IPV4V6 == info.ip_version)
                    {
                        liot_trace("info.v6.addr.ip: %s", liot_ip6addr_ntoa(&info.v6.addr.ip));
                    }
                }
                else
                {
                    liot_trace("PDP2 not activeed succeed");
                }

                liot_rtos_task_sleep_ms(100);

                // Default bearer CID is 1 by default
                if (true == liot_datacall_get_sim_profile_is_active(nSim, 1))
                {
                    memset(&default_info, 0x00, sizeof(liot_data_call_default_pdn_info_s));
                    ret = liot_datacall_get_default_pdn_info(nSim, &default_info);

                    liot_rtos_task_sleep_ms(100);
                    liot_trace("liot_datacall_get_default_pdn_info ret: 0x%x", ret);
                    liot_trace("default apn=%s", default_info.apn_name);
                    liot_trace("default ip_type=%d", default_info.ip_version);
                    
                    if (LIOT_DATA_TYPE_IP == default_info.ip_version ||
                        LIOT_DATA_TYPE_IPV4V6 == default_info.ip_version)
                    {
                        liot_trace("default ip_v4=%s", liot_ip4addr_ntoa(&default_info.ipv4));
                    }
                    if (LIOT_DATA_TYPE_IPV6 == default_info.ip_version ||
                        LIOT_DATA_TYPE_IPV4V6 == default_info.ip_version)
                    {
                        liot_trace("default ip_v6=%s", liot_ip6addr_ntoa(&default_info.ipv6));
                    }
                }
                else
                {
                    liot_trace("default PDP is not actived successfully.");
                }

                liot_rtos_task_sleep_ms(100);

                if (false == liot_datacall_get_sim_profile_is_active(nSim, 3))
                {
                    liot_trace("PDP3 is not actived successfully");
                }
            }
        } while (0);

        // Clean up: stop data call if started
        if (true == isSim0Cid2Started)
        {
            ret = liot_stop_data_call(nSim, cid);
            if (LIOT_DATACALL_SUCCESS != ret)
            {
                liot_trace("liot_stop_data_call ret: 0x%x.", ret);
            }

            liot_trace("liot_stop_data_call done.");

            // ensure liot_stop_data_call is successful for synchronous or asynchronous process
            if (0 == idx)
            {
                liot_rtos_task_sleep_ms(100);
            }
            else
            {
                liot_rtos_task_sleep_ms(500);
            }
        }

        // Clean up: unregister callback if registered
        if (true == isDataCallCBRegistered)
        {
            ret = liot_datacall_unregister_cb(LIOT_DATACALL_REGISTER_ALL_SIM,
                                              LIOT_DATACALL_REGISTER_ALL_PDP,
                                              (liot_datacall_callback)liot_datacall_ind_callback,
                                              (void *)&checkActive);
            if (LIOT_DATACALL_SUCCESS != ret)
            {
                liot_trace("liot_datacall_unregister_cb failed, ret is 0x%x.", ret);
            }
        }
    }

    // Clean up: delete task
    if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL))
    {
        liot_trace("task deleted failed");
    }
}

