/**
 * @file liot_sleep_demo.c
 * @brief LIoT Sleep Mode Demonstration Application
 * @details This file implements a sleep mode control example for the LIoT platform, 
 *          including sleep lock management and timer control.
 * @version 1.1
 * @date 2025-08-18
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * 1. Initialize sleep mode
 * 2. Register sleep mode callback function
 * 3. Enter sleep mode
 * 4. Exit sleep mode
 */

#include <stdio.h>
#include <string.h>
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_dev.h"
#include "liot_sleep.h"
#include "liot_gpio2.h"
#include "liot_type.h"

#define LIOT_SLEEP_TIME_OUT             60 * 1000

//#define LIOT_MAX_SLEEP_EX_DEPTH   LIOT_SLEEP_MODE_NORMAL  
#define LIOT_MAX_SLEEP_EX_DEPTH   LIOT_SLEEP_MODE_LOW  
//#define LIOT_MAX_SLEEP_EX_DEPTH   LIOT_SLEEP_MODE_DEEP_LOW

#define LIOT_TEST_GPIO_ADDR       L_GPIO_26

#define LIOT_SLEEP_TIME_ENABLE      1

#define LIOT_SLEEP_CLOSE_CFUN       1

liot_queue_t g_queuehandle = NULL;

typedef enum
{
    LIOT_SLEEP_ENTER,
    LIOT_SLEEP_EXIT,
    LIOT_SLEEP_EVENT_MAX
}Liot_SleepExEvent_e;

bool g_wakeup_from_sleep = false;
void liot_sleeptimer_callback(Liot_SleepTimerID_e timeid)
{
    liot_trace("liot_sleeptimer_callback timeid %d", timeid);
    g_wakeup_from_sleep = true;

    Liot_SleepExEvent_e sleep_event = LIOT_SLEEP_EXIT;
    liot_rtos_queue_release(g_queuehandle, sizeof(Liot_SleepExEvent_e), (uint8 *)&sleep_event, 0);
}
void liot_sleepex_demo_thread(void *arvg)
{
    liot_trace("liot_sleepex_demo_thread trace1 in");
    Liot_SleepExEvent_e sleep_event;
    liot_errcode_e ret = LIOT_SUCCESS;
    LiotSleepModeCfg_t mode_cfg;

#if LIOT_SLEEP_CLOSE_CFUN 
    // Turn off radio frequency CFUN = 0;
    liot_dev_set_modem_fun(0, false, 0);
#endif
    
    
    //apgio
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_1_80V);
    
    Liot_GpioInit(LIOT_TEST_GPIO_ADDR, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioSetLevel(LIOT_TEST_GPIO_ADDR, L_IO_HIGH);  // Set GPIO output to low level

    liot_rtos_queue_create(&g_queuehandle, sizeof(Liot_SleepExEvent_e), 10);
#if defined(LIOT_MAX_SLEEP_EX_DEPTH) && (LIOT_MAX_SLEEP_EX_DEPTH == LIOT_SLEEP_MODE_DEEP_LOW)
    liot_wakeup_src_e wakeupsrc = liot_get_wakeup_src();
    if(wakeupsrc == LIOT_WAKEUP_FROM_RTC && LIOT_DEEPSLP_TIMER_ID0 == Liot_SleepTimerGetID())
    {
        sleep_event = LIOT_SLEEP_EXIT;
        while(1)
        {
            liot_trace("rtc wakeup exit sleep runing");
            liot_rtos_task_sleep_s(1);
        }
    }
    else
    {
        Liot_SleepTimerStart(LIOT_DEEPSLP_TIMER_ID0, LIOT_SLEEP_TIME_OUT, liot_sleeptimer_callback);
        sleep_event = LIOT_SLEEP_ENTER;        
    }
#else
    if(!g_wakeup_from_sleep)
    {
        Liot_SleepTimerStart(LIOT_DEEPSLP_TIMER_ID0, LIOT_SLEEP_TIME_OUT, liot_sleeptimer_callback);
        sleep_event = LIOT_SLEEP_ENTER;
    }
    else
    {
        sleep_event = LIOT_SLEEP_EXIT;
        while(1)
        {
            liot_trace("rtc wakeup exit sleep runing");
            liot_rtos_task_sleep_s(1);
        }
    }  
#endif 

    //Liot_SleepTimerCheck
    liot_trace("Liot_SleepTimerCheck LIOT_DEEPSLP_TIMER_ID0 %d", Liot_SleepTimerCheck(LIOT_DEEPSLP_TIMER_ID0)); 
    liot_trace("Liot_SleepTimerCheck LIOT_DEEPSLP_TIMER_ID1 %d", Liot_SleepTimerCheck(LIOT_DEEPSLP_TIMER_ID1));
    Liot_SleepTimerStart(LIOT_DEEPSLP_TIMER_ID1, LIOT_SLEEP_TIME_OUT, liot_sleeptimer_callback);
    
    //Liot_SleepTimerStop
    liot_sleep_errcode_e result = Liot_SleepTimerStop(LIOT_DEEPSLP_TIMER_ID1);
    if(LIOT_SLEEP_SUCCESS != result)
    {
        liot_trace("Liot_SleepTimerStop error %d", result);
    }
    else
    {
        liot_trace("Liot_SleepTimerStop success %d", result);        
    }

    liot_rtos_queue_release(g_queuehandle, sizeof(Liot_SleepExEvent_e), (uint8 *)&sleep_event, 0);
    
    liot_trace("liot_rtos_queue_wait event");
    while (1)
    {
        // Sleep when message queue is blocked; do not use sleep() or similar delay functions.
        // Timer expiration will wake the module from sleep.
        ret = liot_rtos_queue_wait(g_queuehandle, (uint8 *)&sleep_event, sizeof(sleep_event), LIOT_WAIT_FOREVER);
        if (ret != LIOT_SUCCESS)
        {
            continue;
        }

        liot_trace("event %x", sleep_event);    
        switch (sleep_event)
        {
            case LIOT_SLEEP_ENTER:
                liot_trace("ENTER sleep"); 
                mode_cfg.mode = LIOT_MAX_SLEEP_EX_DEPTH;
                Liot_SleepSetMode(&mode_cfg);
                break;
            case LIOT_SLEEP_EXIT:
                liot_trace("EXIT sleep"); 
                mode_cfg.mode = LIOT_SLEEP_MODE_NORMAL;
                Liot_SleepSetMode(&mode_cfg);
                break;
            default:
                break;
        }
    }

    liot_rtos_task_delete(NULL);
}