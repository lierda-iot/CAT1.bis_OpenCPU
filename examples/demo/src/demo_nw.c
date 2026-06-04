/**
 * @file liot_nw_demo.c
 * @brief Demonstration code for network-related operations.
 * @version 2.0
 * @date 2025-08-06
 * 
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_type.h"

/* Global Variable */
static liot_sem_t nw_semp;

/**
 * @brief Callback function for network indication events.
 * 
 * This function is called when a network indication event occurs, such as signal quality change,
 * data registration status update, or NITZ time update. It logs relevant information based on the event type.
 * 
 * @param nSim SIM card number.
 * @param ind_type Network indication event type.
 * @param ctx Pointer to the context data related to the event.
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
            if (liot_nw_msg->state == LIOT_NW_REG_STATE_HOME_NETWORK)
            {
                liot_rtos_semaphore_release(nw_semp);
            }
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

/**
 * @brief Callback function for data call indication events.
 * 
 * This function is called when a data call indication event occurs. It logs a test message for multi-task data call.
 * 
 * @param nSim SIM card number.
 * @param ind_type Data call indication event type.
 * @param profile_idx Data call profile index.
 * @param result Data call result.
 * @param ctx Pointer to the context data related to the event.
 */
void liot_datacall_ind_callback_1(uint8_t nSim, unsigned int ind_type, int profile_idx, bool result, void *ctx)
{
    liot_trace("The multi-task datacall callback test function");
}

/**
 * @brief Callback function for cell information events.
 * 
 * This function is called when cell information is available. It logs the LTE cell information if valid.
 * 
 * @param cell_info Pointer to the cell information structure.
 */
void liot_nw_cellinfo_callback(liot_nw_cell_info_s *cell_info)
{
    int cell_index = 0;
    liot_trace("liot_nw_cellinfo_callback,lte_info_num:%d,lte_info_valid:%d",cell_info->lte_info_num,cell_info->lte_info_valid);
    if (cell_info->lte_info_valid)
    {
        for (cell_index = 0; cell_index < cell_info->lte_info_num; cell_index++)
        {
            liot_trace("Cell_%d", cell_index);
            liot_trace("[LTE] flag:%d, cid:0x%X, mcc:%d, mnc:%d, tac:0x%X, pci:%d, earfcn:%d, rssi:%d",
                       cell_info->lte_info[cell_index].flag,
                       cell_info->lte_info[cell_index].cid,
                       cell_info->lte_info[cell_index].mcc,
                       cell_info->lte_info[cell_index].mnc,
                       cell_info->lte_info[cell_index].tac,
                       cell_info->lte_info[cell_index].pci,
                       cell_info->lte_info[cell_index].earfcn,
                       cell_info->lte_info[cell_index].rssi);
        }
    }
}

/**
 * @brief Main thread function for the network demonstration.
 * 
 * This function initializes network settings, registers callback functions, and periodically retrieves
 * various network-related information, such as registration status, signal quality, and cell information.
 * 
 * @param arvg Thread argument (not used in this function).
 */
void liot_nw_demo_thread(void *arvg)
{
    int ret = 0; // Return value for network operation functions
    int datacount_reset = 0; // Counter for resetting data count
    unsigned char csq = 0; // Signal quality value
    liot_nw_reg_status_info_s reg_info = {0}; // Network registration status information
    liot_nw_signal_strength_info_s signal_info = {0}; // Signal strength information
    liot_nw_operator_info_s oper_info = {0}; // Network operator information
    liot_nw_seclection_info_s select_info = {0}; // Network selection information
    liot_nw_nitz_time_info_s nitz_info = {0}; // NITZ time information
    liot_nw_cell_info_s cell_info = {0}; // Cell information
    liot_nw_data_count_info_s datacount_info = {0}; // Data count information

    // Register the network indication callback function
    liot_nw_register_cb(liot_nw_ind_callback);
    // Initialize network semaphore (initial state is unavailable, used to wait for network registration)
    liot_rtos_semaphore_create(&nw_semp, 0);
    // Set network selection parameters
    select_info.act = LIOT_NW_ACCESS_TECH_E_UTRAN;
    sprintf((char *)&select_info.mcc, "%s", "460");
    sprintf((char *)&select_info.mnc, "%s", "11");
    select_info.nw_selection_mode = 1;

    // Set CTZU switch status
    liot_nw_set_ctzu_switch(0);

    liot_trace("liot_nw_get_ctzu_switch = %d", liot_nw_get_ctzu_switch());   
    
    liot_rtos_task_sleep_ms(2000);
        
    // Set network selection
    ret = liot_nw_set_selection(0, &select_info);
    liot_trace("liot_nw_set_selection = %x", ret); 

    // Wait until network registration is complete
    liot_rtos_semaphore_wait(nw_semp, LIOT_WAIT_FOREVER);
    while (1)
    {
        // Sleep for 5 seconds
        liot_rtos_task_sleep_ms(5000);

        // Get the network registration status
        ret = liot_nw_get_reg_status(0, &reg_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("data:  state:%d, lac:0x%X, cid:0x%X, act:%d",
                        reg_info.data_reg.state,
                        reg_info.data_reg.lac,
                        reg_info.data_reg.cid,
                        reg_info.data_reg.act);
        }

        liot_trace("liot_nw_get_reg_status ret=0x%x", ret);

        // Get the signal quality
        ret = liot_nw_get_csq(0, &csq);

        liot_trace("ret=0x%x, csq:%d", ret, csq);
        
        // Get the signal strength
        ret = liot_nw_get_signal_strength(0, &signal_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("rssi:%d, bitErrorRate:%d, rsrp:%d, rsrq:%d,snr:%d",
                        signal_info.rssi,
                        signal_info.bitErrorRate,
                        signal_info.rsrp,
                        signal_info.rsrq,
                        signal_info.snr);
        }
        liot_trace("liot_nw_get_signal_strength ret=0x%x", ret);

        // Get the network operator name
        ret = liot_nw_get_operator_name(0, &oper_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("long_oper_name:%s, short_oper_name:%s, mcc:%s, mnc:%s",
                        oper_info.long_oper_name,
                        oper_info.short_oper_name,
                        oper_info.mcc,
                        oper_info.mnc);
        }
        liot_trace("liot_nw_get_operator_name ret=0x%x", ret);

        // Get the network selection information
        ret = liot_nw_get_selection(0, &select_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("nw_selection_mode:%d, mcc:%s, mnc:%s, act:%d",
                        select_info.nw_selection_mode,
                        select_info.mcc,
                        select_info.mnc,
                        select_info.act);
        }
        liot_trace("liot_nw_get_selection ret=0x%x", ret);

        // Get the NITZ time information
        ret = liot_nw_get_nitz_time_info(&nitz_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("nitz_time:%s, abs_time:%ld", nitz_info.nitz_time, nitz_info.abs_time);
        }
        liot_trace("liot_nw_get_nitz_time_info ret=0x%x", ret);    

        // Get the cell information
        ret = liot_nw_get_cell_info(0, &cell_info);
        liot_trace("liot_nw_get_cell_info ret=0x%x", ret);
        if (cell_info.lte_info_valid)
        {
            for (unsigned char cell_index = 0; cell_index < cell_info.lte_info_num; cell_index++)
            {
                liot_trace("Cell_%d", cell_index);
                liot_trace("[LTE] flag:%d, cid:0x%X, mcc:%x, mnc:%x, tac:0x%X, pci:%d, earfcn:%d, rssi:%d",
                           cell_info.lte_info[cell_index].flag,
                           cell_info.lte_info[cell_index].cid,
                           cell_info.lte_info[cell_index].mcc,
                           cell_info.lte_info[cell_index].mnc,
                           cell_info.lte_info[cell_index].tac,
                           cell_info.lte_info[cell_index].pci,
                           cell_info.lte_info[cell_index].earfcn,
                           cell_info.lte_info[cell_index].rssi);
            }
        }

        // Get the data count information
        ret = liot_nw_get_data_count(0, &datacount_info);
        if (ret == LIOT_NW_SUCCESS)
        {
            liot_trace("uplink_data_count:%ld, downlink_data_count:%ld",
                        datacount_info.uplink_data_count,
                        datacount_info.downlink_data_count);
            datacount_reset++;

            if (datacount_reset == 2)
            {
                datacount_reset = 0;
                // Reset the data count
                liot_nw_reset_data_count(0);
            }        
        }
        liot_trace("liot_nw_get_data_count ret=0x%x", ret);
    }
}
