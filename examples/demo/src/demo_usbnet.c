/**
 * @file       liot_usb_demo.c
 * @brief      Example code for USB network
 * @version    1.0
 * @date       2025-8-22
 * @copyright  Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * USB network demonstration application to implement RNDIS/ECM protocol management. 
 * 1. Contains USB hot swap, 
 * 2. network event and USB network event callback functions, 
 * 3. the main thread is responsible for initialization, 
 * 4. network registration check 
 * 5. periodic USB network connection start-stop loop.
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_usb.h"
#include "liot_usbnet.h"

liot_sem_t liot_nw_reg_sem = NULL;

int liot_usb_hotplug_callback(liot_usb_hotplug_e state, void *ctx)
{
    liot_trace("USB change hotplug state(%d)", state);
    return 0;
}

static void liot_nw_event_callback(uint8_t sim_id, unsigned int event_type, void *ind_msg_buf)
{
    if (LIOT_NW_DATA_REG_STATUS_IND == event_type)
    {
        liot_nw_common_reg_status_info_s *data_reg_status = (liot_nw_common_reg_status_info_s *)ind_msg_buf;
        liot_trace("Sim%d data reg status changed, current status is %d", sim_id, data_reg_status->state);
        if ((LIOT_NW_REG_STATE_HOME_NETWORK == data_reg_status->state) ||
            (LIOT_NW_REG_STATE_ROAMING == data_reg_status->state))
        {
            liot_rtos_semaphore_release(liot_nw_reg_sem);
        }
    }
}

static void liot_usbnet_event_callback(unsigned int event_type, liot_usbnet_errcode_e errcode, void *ctx)
{
    liot_trace("event_type: 0x%x, errcode: 0x%x", event_type, errcode);

    switch (event_type)
    {
        case LIOT_USBNET_START_RSP_IND:
        {
            if (LIOT_USBNET_SUCCESS == errcode)
            {
                liot_trace("usbnet connect success");
                // run the following blocked code, the RNDIS/ECM dial will be disconnected after two minutes
            }
            else
            {
                liot_trace("usbnet connect fail, err: 0x%x", errcode);
            }
            break;
        }
        case LIOT_USBNET_DEACTIVE_IND:
        {
            liot_trace("PDP deactived");

            break;
        }
    }
}

void liot_usbnet_demo_thread(void *arvg)
{
    liot_usbnet_errcode_e ret         = 0;
    liot_usbnet_type_e saved_type     = 0;
    liot_usbnet_type_e target_type    = LIOT_USBNET_RNDIS;
    liot_nw_reg_status_info_s nw_info = {0};
    liot_usbnet_state_e status        = LIOT_USBNET_STATE_NONE;
    int profile_idx                   = 1; // range 1 to 7

    liot_usb_bind_hotplug_cb(liot_usb_hotplug_callback);

    liot_rtos_task_sleep_ms(10000);
    ret = liot_usbnet_get_type(&saved_type);
    liot_trace("get usbnet saved type ret: 0x%x, %d", ret, saved_type);
    if (0 != ret)
    {
        goto exit;
    }

    if (saved_type != target_type)
    {
        ret = liot_usbnet_set_type(target_type);
        liot_trace("liot_usbnet_set_type err, ret=0x%x", ret);
        if (0 != ret)
        {
            goto exit;
        }
    }

    ret = liot_nw_register_cb(liot_nw_event_callback);
    liot_trace("register network callback ret=0x%x", ret);
    if (0 != ret)
    {
        goto exit;
    }

    ret = liot_nw_get_reg_status(0, &nw_info);
    liot_trace("ret: 0x%x, current data reg status=%d", ret, nw_info.data_reg.state);
    if ((LIOT_NW_REG_STATE_HOME_NETWORK != nw_info.data_reg.state) &&
        (LIOT_NW_REG_STATE_ROAMING != nw_info.data_reg.state))
    {
        liot_rtos_semaphore_wait(liot_nw_reg_sem, LIOT_WAIT_FOREVER); // wait network registered
    }

    ret = liot_usbnet_register_cb(liot_usbnet_event_callback, NULL);
    liot_trace("register usbnet callback ret=0x%x", ret);
    if (0 != ret)
    {
        goto exit;
    }

    while (1)
    {
        liot_rtos_task_sleep_ms(10000);
        ret = liot_usbnet_get_status(&status);
        liot_trace("ret: 0x%x, usbnet status: %d", ret, status);
        if ((LIOT_USBNET_SUCCESS == ret) && (LIOT_USBNET_STATE_NONE == status))
        {
            liot_trace("liot_usbnet_start ret=0x%x", ret);
            ret = liot_usbnet_start(0, profile_idx, NULL);
        }

        liot_rtos_task_sleep_ms(10000);
        ret = liot_usbnet_get_status(&status);
        liot_trace("ret: 0x%x, usbnet status: %d", ret, status);
        if ((LIOT_USBNET_SUCCESS == ret) && (LIOT_USBNET_STATE_CONNECT == status))
        {
            liot_trace("liot_usbnet_stop ret=0x%x", ret);
            ret = liot_usbnet_stop();
        }
    }
exit:
    if (liot_nw_reg_sem != NULL)
    {
        liot_rtos_semaphore_delete(liot_nw_reg_sem);
    }
}