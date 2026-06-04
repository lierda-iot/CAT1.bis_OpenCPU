/**
 * @File Name: liot_apwm_demo.c
 * @brief  
 * @Author : zlc email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2025-12-25
 * 
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * 
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-12-25 <td>1.0     <td>zhaoliechang     <td>new create
 * </table>
 */

/*
 * This demo implements: 
 * 1.PWM Initialization and Configuration
 *  The structure Liot_ApwmCfg_t is used to configure PWM (Pulse Width Modulation), including setting the index to LIOT_APWM_INDEX_2, the period to APWM_MAX_PERIOD_1024MS, high level time to 70, low level time to 20, and other parameters.
 *  The function Liot_ApwmCfg(&cfg) is called to configure the PWM parameters, and Liot_ApwmEnable(cfg.idx, true) is used to enable PWM.
 * 2.Low Power Settings
 *  The function Liot_SleepSetMode(&mode_cfg) is called to allow the device to enter automatic sleep mode to reduce power consumption.
 * 3.Main Loop Functionality
 *  In the while(1) main loop, the current duty cycle information of PWM2 (fixed output "LIOT_PWM_2 duty cycle: 1") is printed regularly using liot_trace.
 *  After each print, the task sleeps for 5 seconds by calling liot_rtos_task_sleep_s(5).
 */
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "lierda_app_main.h"
#include "liot_apwm.h"
#include "liot_sleep.h"

void liot_apwm_demo_thread(void *argv)
{
    LiotSleepModeCfg_t mode_cfg;
    liot_rtos_task_sleep_s(10);

    Liot_ApwmCfg_t cfg;
    memset(&cfg, 0x00, sizeof(cfg));

    //config APWM index and period
    cfg.idx = LIOT_APWM_INDEX_2;
    cfg.Period = LIOT_APWM_MAX_PERIOD_1024MS;
    cfg.accuracy = 0;  
    cfg.highPeriod = 70;
    cfg.lowPeriod = 20;

    Liot_ApwmCfg(&cfg);

    Liot_ApwmEnable(cfg.idx, true);
    
    mode_cfg.mode = LIOT_SLEEP_MODE_LOW;
    Liot_SleepSetMode(&mode_cfg);

    while (1)
    {
        liot_trace("LIOT_PWM_2 duty cycle: 1");    
        liot_rtos_task_sleep_s(5);
    }
}