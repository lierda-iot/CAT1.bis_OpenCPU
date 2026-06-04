/**
 * @file liot_sms_demo.c
 * @brief
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.1
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_sms.h"
#include "liot_type.h"
#include "liot_nw.h"

static liot_sem_t sms_list_sem = NULL;     ///< Semaphore for SMS list indication
static liot_sem_t sms_list_end_sem = NULL; ///< Semaphore for SMS list end indication
static liot_sem_t nw_semp = NULL;          ///< Semaphore for network registration completion
uint8_t newMsgId = 0;                      ///< ID of the newest received SMS message

/**
 * @brief SMS event callback function
 *
 * This callback function handles various SMS events including new message indication,
 * message list indication, extended message list indication, and memory full indication.
 *
 * @param nSim      SIM card index
 * @param event_id  Event identifier
 * @param ctx       Context parameter (type depends on event_id)
 */
void user_sms_event_callback(uint8_t nSim, int event_id, void *ctx)
{
    switch (event_id)
    {
    case LIOT_SMS_NEW_MSG_IND:
    {
        // New SMS message received
        liot_sms_new_s *msg = (liot_sms_new_s *)ctx;
        liot_trace("sms_demo: sim=%d, index=%d, storage memory=%d", nSim, msg->index, msg->mem);
        // Store the new message ID for later processing
        newMsgId = msg->index;
        // Signal that a new SMS has been received
        liot_rtos_semaphore_release(sms_list_sem);
        break;
    }
    case LIOT_SMS_LIST_IND:
    {
        // Standard SMS message list indication
        liot_sms_msg_s *msg = (liot_sms_msg_s *)ctx;
        liot_trace("sms_demo list ind: sim=%d,index=%d, msg = %s", nSim, msg->index, msg->buf);
        break;
    }
    case LIOT_SMS_LIST_EX_IND:
    {
        // Extended SMS message list indication with more detailed information
        liot_sms_recv_s *msg = (liot_sms_recv_s *)ctx;
        liot_trace("sms_demo list ex ind: index=%d,oa=%s,tooa=%u,status=%d,fo=0x%x,dcs=0x%x",
                   msg->index,
                   msg->oa,
                   msg->tooa,
                   msg->status,
                   msg->fo,
                   msg->dcs);

        liot_trace("sms_demo list ex ind: scst=%d/%d/%d %d:%d:%d±%d",
                   msg->scts.uYear,
                   msg->scts.uMonth,
                   msg->scts.uDay,
                   msg->scts.uHour,
                   msg->scts.uMinute,
                   msg->scts.uSecond,
                   msg->scts.iZone);

        liot_trace("sms_demo list ex ind: uid=%u,total=%u,seg=%u,dataLen=%d,data=%s",
                   msg->uid,
                   msg->msg_total,
                   msg->msg_seg,
                   msg->dataLen,
                   msg->data);
        break;
    }
    case LIOT_SMS_LIST_END_IND:
    {
        // SMS list reading has completed
        liot_trace("LIOT_SMS_LIST_END_IND");
        // Signal that SMS list reading is complete
        liot_rtos_semaphore_release(sms_list_end_sem);
        break;
    }
    case LIOT_SMS_MEM_FULL_IND:
    {
        liot_sms_new_s *msg = (liot_sms_new_s *)ctx;
        liot_trace("sms_demo: LIOT_SMS_MEM_FULL_IND sim=%d, memory=%d", nSim, msg->mem);
        // Attempt to delete all messages to free up space
        if (LIOT_SMS_SUCCESS == liot_sms_delete_msg_ex(nSim, 0, LIOT_SMS_DEL_ALL))
        {
            liot_trace("sms_demo: delete msg OK");
        }
        else
        {
            liot_trace("sms_demo: delete sms FAIL");
        }
        break;
    }
    default:
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
    {
        csq = *((char *)ctx);
        liot_trace("csq=%d", csq);
        break;
    }
    case LIOT_NW_DATA_REG_STATUS_IND:
    {
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
    }
    case LIOT_NW_NITZ_TIME_UPDATE_IND:
    {
        liot_nw_nitz_time_info = (liot_nw_nitz_time_info_s *)ctx;
        liot_trace(
            "nitz_time=%s, abs_time=%ld", liot_nw_nitz_time_info->nitz_time, liot_nw_nitz_time_info->abs_time);
        break;
    }
    }
}

/**
 * @brief SMS demo main task function
 *
 * This is the main task function for the SMS demonstration. It performs the following operations:
 * 1. Initializes semaphores for synchronization
 * 2. Registers network status callback and waits for network registration
 * 3. Registers SMS event callback
 * 4. Gets SMS center address
 * 5. Sends an English text message
 * 6. Gets SMS storage information
 * 7. Sets and gets SMS storage
 * 8. Reads existing SMS messages if any
 * 9. Waits for and processes new incoming SMS messages
 *
 * @param param Task parameter (unused)
 */
void liot_sms_demo_task(void *param)
{
    char addr[20] = {0};               // Buffer for SMS center address
    uint8_t nSim = 0;                  // SIM card index (0 for first SIM)
    liot_sms_mem_info_s sms_mem = {0}; // SMS memory information structure
    uint16_t msg_len = 512;            // Length of message buffer

    liot_trace("liot_sms_demo_task enter");
    // Create semaphore for SMS list indication
    liot_rtos_semaphore_create(&sms_list_sem, 0);
    // Create semaphore for SMS list end indication
    liot_rtos_semaphore_create(&sms_list_end_sem, 0);
    // Initialize network semaphore (initial state is unavailable, used to wait for network registration)
    liot_rtos_semaphore_create(&nw_semp, 0);
    // Register network status callback
    liot_nw_register_cb(liot_nw_ind_callback);
    // Wait until network registration is complete
    liot_rtos_semaphore_wait(nw_semp, LIOT_WAIT_FOREVER);
    // Register SMS event callback function
    liot_sms_callback_register(user_sms_event_callback);
    // Get SMS center address
    if (LIOT_SMS_SUCCESS == liot_sms_get_center_address(nSim, addr, sizeof(addr)))
    {
        liot_trace("sms_demo: liot_sms_get_center_address OK, addr=%s", addr);
    }
    else
    {
        liot_trace("sms_demo: liot_sms_get_center_address FAIL");
    }

    // Send English text message
    if (LIOT_SMS_SUCCESS == liot_sms_send_msg(nSim, "+8610086", "hello,world!", LIOT_GSM))
    {
        liot_trace("sms_demo: liot_sms_send_msg OK");
    }
    else
    {
        liot_trace("sms_demo: liot_sms_send_msg FAIL");
    }

    // Get how many SMS messages can be stored in the SIM card in total and how much storage is used
    liot_sms_stor_info_s stor_info = {0};
    if (LIOT_SMS_SUCCESS == liot_sms_get_storage_info(nSim, &stor_info))
    {
        liot_trace("sms_demo: liot_sms_get_storage_info OK");
        liot_trace("sms_demo: SM used=%u,SM total=%u,ME used=%u,ME total=%u, newSmsStorId=%u",
                   stor_info.usedSlotSM,
                   stor_info.totalSlotSM,
                   stor_info.usedSlotME,
                   stor_info.totalSlotME,
                   stor_info.newSmsStorId);
    }
    else
    {
        liot_trace("sms_demo: liot_sms_get_storage_info FAIL");
    }

    // The first parameter specifies that SMS messages are read from SM
    liot_sms_set_storage(nSim, LIOT_SM, LIOT_SM, LIOT_SM);
    // Get current SMS storage configuration
    liot_sms_get_storage(nSim, &sms_mem);
    liot_trace("sms_demo: mem1=%d, mem2=%d, mem3=%d", sms_mem.mem1, sms_mem.mem2, sms_mem.mem3);

    char *msg = (char *)malloc(msg_len);
    if (msg == NULL)
    {
        liot_trace("sms_demo: malloc liot_sms_msg_s fail");
        goto exit;
    }
    liot_sms_recv_s *sms_recv = (liot_sms_recv_s *)malloc(sizeof(liot_sms_recv_s));
    if (sms_recv == NULL)
    {
        liot_trace("sms_demo: calloc FAIL");
        goto exit;
    }
    // Check if there are stored messages in SIM or ME
    if ((stor_info.usedSlotSM > 0) || (stor_info.usedSlotME > 0))
    {
        liot_trace("sms_demo: SIM has stored messages, reading all messages");
        // Read all message in SIM
        if (LIOT_SMS_SUCCESS == liot_sms_read_msg_list(nSim, LIOT_TEXT))
        {
            liot_trace("sms_demo: get msg list OK");

            // Wait for message list reading to complete (with 30 second timeout)
            if (liot_rtos_semaphore_wait(sms_list_end_sem, 30000)) // 30秒超时
            {
                liot_trace("sms_demo: wait for msg list timeout");
            }
            else
            {
                liot_trace("sms_demo: msg list read completed");
            }
        }
        else
        {
            liot_trace("sms_demo: get msg list FAIL");
        }
    }
    // Main loop to wait for and process new SMS messages
    while (1)
    {
        if (liot_rtos_semaphore_wait(sms_list_sem, LIOT_WAIT_FOREVER) == 0)
        {
            liot_trace("sms_demo: wait for new sms");
            memset(msg, 0, msg_len);
            // Read SMS messages as text
            if (LIOT_SMS_SUCCESS == liot_sms_read_msg(nSim, newMsgId, msg, msg_len, LIOT_TEXT))
            {
                liot_trace("sms_demo: read text msg OK, msg=%s", msg);
            }
            else
            {
                liot_trace("sms_demo: read text sms FAIL");
            }

            // Read SMS messages as pdu
            memset(msg, 0, msg_len);
            if (LIOT_SMS_SUCCESS == liot_sms_read_msg(nSim, newMsgId, msg, msg_len, LIOT_PDU))
            {
                liot_trace("sms_demo: read pdu msg OK, msg_len=%d msg=%s", msg_len, msg);
            }
            else
            {
                liot_trace("sms_demo: read pdu sms FAIL");
            }

            // Read SMS messages as text
            memset(sms_recv, 0, sizeof(liot_sms_recv_s));
            if (LIOT_SMS_SUCCESS == liot_sms_read_msg_ex(nSim, newMsgId, LIOT_TEXT, sms_recv))
            {
                liot_trace("sms_demo: index=%d,oa=%s,tooa=%u,status=%d,fo=0x%x,dcs=0x%x",
                           sms_recv->index,
                           sms_recv->oa,
                           sms_recv->tooa,
                           sms_recv->status,
                           sms_recv->fo,
                           sms_recv->dcs);

                liot_trace("sms_demo: scst=%d/%d/%d %d:%d:%d±%d",
                           sms_recv->scts.uYear,
                           sms_recv->scts.uMonth,
                           sms_recv->scts.uDay,
                           sms_recv->scts.uHour,
                           sms_recv->scts.uMinute,
                           sms_recv->scts.uSecond,
                           sms_recv->scts.iZone);

                liot_trace("sms_demo: uid=%u,total=%u,seg=%u,dataLen=%d,data=%s",
                           sms_recv->uid,
                           sms_recv->msg_total,
                           sms_recv->msg_seg,
                           sms_recv->dataLen,
                           sms_recv->data);
            }
            else
            {
                liot_trace("sms_demo: read sms FAIL");
            }
        }
    }
exit:
    if (sms_recv)
        free(sms_recv);
    if (msg)
        free(msg);
    // Clean up semaphores
    liot_rtos_semaphore_release(sms_list_sem);
    liot_rtos_semaphore_delete(sms_list_sem);
    liot_rtos_semaphore_release(sms_list_end_sem);
    liot_rtos_semaphore_delete(sms_list_end_sem);
    liot_rtos_semaphore_release(nw_semp);
    liot_rtos_semaphore_delete(nw_semp);
    // Delete current task
    liot_rtos_task_delete(NULL);
}
