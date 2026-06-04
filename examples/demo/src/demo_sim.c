/**
 * @file liot_sim_demo.c
 * @brief SIM Card Demonstration Application
 * @details This file implements a demo application showing SIM card operations
 * including SIM information retrieval, slot switching, and dual-card preference management.
 * 
 * @email ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2025-08-15
 * 
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 */

#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_sim.h"
#include "liot_dev.h"

/**
 * @brief SIM card demo thread function
 * This thread demonstrates SIM card operations including getting SIM information,
 * switching SIM slots, and checking dual-card preferences.
 * 
 * @param argv Thread arguments (not used)
 */
void liot_sim_demo_thread(void *argv)
{
    (void)argv;

    uint8_t nSim = LIOT_SIM_INVALID;
    liot_sim_errcode_e ret = LIOT_SIM_EXECUTE_ERR;
    uint8_t cfun = 0xFF;
    liot_sim_status_e cardStatus = LIOT_SIM_STATUS_UNKNOW;

    // Main loop for SIM demo operations
    // Execute SIM operations twice with different SIM cards
    uint8_t times = 2;
    while (times)
    {
        // Delay demonstration - print message every 10 seconds for 5 times
        for (int n = 0; n < 5; n++)
        {
            liot_trace("hello sim demo %d.", n);
            liot_rtos_task_sleep_s(10);
        }

        // Get current SIM slot
        ret = liot_sim_get_slot(&nSim);
        liot_trace("ret:0x%x, current nSim is %d", ret, nSim);

        // Get IMSI information
        char siminfo[64] = {0};
        liot_sim_errcode_e ret = liot_sim_get_imsi(nSim, siminfo, sizeof(siminfo));
        liot_trace("ret:0x%x, IMSI: %s", ret, siminfo);

        // Get ICCID information
        memset(siminfo, 0x00, strlen(siminfo));
        ret = liot_sim_get_iccid(nSim, siminfo, sizeof(siminfo));
        liot_trace("ret:0x%x, ICCID: %s", ret, siminfo);

        // Get phone number
        memset(siminfo, 0x00, strlen(siminfo));
        ret = liot_sim_get_phonenumber(nSim, siminfo, sizeof(siminfo));
        liot_trace("ret:0x%x, phonenumber: %s", ret, siminfo);

        // if sim1 is actived currently, getting sim2 phone number will be failed
        memset(siminfo, 0x00, strlen(siminfo));
        ret = liot_sim_get_phonenumber(LIOT_SIM_2 - nSim, siminfo, sizeof(siminfo));
        liot_trace("ret:0x%x, phonenumber: %s", ret, siminfo);

        // Get SIM card status
        ret = liot_sim_get_card_status(nSim, &cardStatus);
        liot_trace("ret:0x%x, card_status: %d", ret, cardStatus);

        // Decrement loop counter
        times--;

        // SIM card switching logic
        if (0 != times)
        {
            // Get current modem function mode
            liot_dev_get_modem_fun(&cfun, nSim);
            liot_trace("cfun: %d", cfun);
            
            // Set modem to minimum function mode before switching
            liot_dev_set_modem_fun(LIOT_DEV_CFUN_MIN, 0, nSim);

            // Switch to the other SIM card slot
            nSim = LIOT_SIM_2 - nSim;

            // Set the new SIM slot
            ret = liot_sim_set_slot(nSim);
            liot_trace("ret:0x%x", ret);

            // Get modem function mode after switching
            liot_dev_get_modem_fun(&cfun, nSim);
            liot_trace("cfun: %d", cfun);
            
            // Restore modem to full function mode
            liot_dev_set_modem_fun(LIOT_DEV_CFUN_FULL, 0, nSim);
        }
    }

    // Get current dual card preference status
    BOOL isDualcardPreferred = LIOT_SIM_DUALCARD_PREFERRED_DISABLE;
    ret = liot_sim_get_dualcard_preferred(&isDualcardPreferred);
    liot_trace("ret:0x%x, is dual card preferred enable: %d", ret, isDualcardPreferred);

    // Toggle dual card preference status
    ret = liot_sim_set_dualcard_preferred(!isDualcardPreferred);
    liot_trace("ret:0x%x", ret);

    // Clean up and delete the demo thread
    if (LIOT_OSI_SUCCESS != liot_rtos_task_delete(NULL))
    {
        liot_trace("task deleted failed");
    }
}