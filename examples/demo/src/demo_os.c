/**
 * @File Name: liot_os_demo.c
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 *           zhaolc
 * @Version : 1.1
 * @Creat Date : 2023-09-07
 *  ModifyDate : 2025-07-11       
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_type.h"

/*
   This project demotrates the usage of OS, in this example, we'll create two tasks, a semaphore and a message queue.
   The main task is created first, and the sub task sends a message to the main task after the main task is created.
   The main task prints the message.
   The test is completed after 10 cycles.
 */

#define MSG_MAX_SIZE 64
#define MSG_MAX_NUM  10
#define MSG_TO_QUEUE "message from sub task to main task"

static liot_sem_t rtos_test_sem      = NULL;
static liot_queue_t rtos_test_queue  = NULL;
static liot_timer_t rtos_timer_queue = NULL;
static int test_count                = 0;

void liot_os_demo_timer_cb(void *argv)
{
    liot_rtos_semaphore_release(rtos_test_sem); // tell sub task that the main task has started
    liot_trace("TIMER_task start");

    liot_rtos_queue_release(rtos_test_queue, strlen(MSG_TO_QUEUE), (u8 *)MSG_TO_QUEUE, 0);

    liot_trace("TIMER_task send msg");
}

// Define event group handle
liot_flag_t xEventGroupHandle;

// Define event bits
#define TASK_1_EVENT (1 << 0) // 0x01
#define TASK_2_EVENT (1 << 1) // 0x02
#define TASK_3_EVENT (1 << 2) // 0x04
#define ALL_TASKS_EVENT (TASK_1_EVENT | TASK_2_EVENT | TASK_3_EVENT) // 0x07


// Task 1 function
void vTask1(void *pvParameters) {

    liot_trace( "Task1 in");
    UINT32 uxBits;
    for (;;) {
        liot_trace( "Task1 1111Received event, starting task...");
        // Wait for event
        LiotOSStatus_t result = liot_rtos_flag_wait(xEventGroupHandle, TASK_1_EVENT, LIOT_FLAG_AND_CLEAR, &uxBits, LIOT_WAIT_FOREVER);
        if (0 == result)
        {
            if(uxBits & TASK_1_EVENT) {
                liot_trace( "Task1 Received event, starting task...");
                // Execute task
                // ...
            }
        }
    }
}

// Task 2 function
void vTask2(void *pvParameters) {

    liot_trace( "Task2 in");
    UINT32 uxBits;

    for (;;) {
        liot_trace( "Task2 22222Received event, starting task...");
        // Wait for event
        LiotOSStatus_t result = liot_rtos_flag_wait(xEventGroupHandle, TASK_2_EVENT, LIOT_FLAG_AND_CLEAR, &uxBits, LIOT_WAIT_FOREVER);
        if (0 == result)
        {
            if(uxBits & TASK_2_EVENT) {
                liot_trace( "Task1 Received event, starting task...");
                // Execute task
                // ...
            }
        }
    }
}

// Task 2 function
void vTask3(void *pvParameters) {

    liot_trace( "Task3 in");
    UINT32 uxBits;

    for (;;) {
        // Wait for event
        liot_trace( "Task3 333 Received event, starting task...");

        LiotOSStatus_t result = liot_rtos_flag_wait(xEventGroupHandle, TASK_3_EVENT, LIOT_FLAG_AND_CLEAR, &uxBits, LIOT_WAIT_FOREVER);
        if (0 == result)
        {
            if(uxBits & TASK_3_EVENT) {
                liot_trace( "Task1 Received event, starting task...");
                // Execute task
                // ...
            }
        }
    }
}

// Timer callback function
void vTimerCallback(void *ctx) {
    // Release event
    liot_rtos_flag_release(xEventGroupHandle, ALL_TASKS_EVENT, LIOT_FLAG_OR);
    liot_trace("Released event, notifying all tasks...");
}

/*!
 * @brief Tests the usage of event groups for task synchronization.
 *
 * @details This function demonstrates how to use event groups to synchronize multiple tasks using flags.
 *          It performs the following steps:
 *          1. Creates an event group using `liot_rtos_flag_create`.
 *          2. Creates three tasks (`vTask1`, `vTask2`, and `vTask3`) that wait for specific event flags.
 *          3. Creates a periodic timer that releases all event flags every 10 seconds to trigger the tasks.
 *          Each task waits for its respective event flag, executes upon receiving it, and clears the flag.
 *          The timer callback releases all flags using `LIOT_FLAG_OR` logic.
 *
 * @note This function is used for testing and demonstration purposes.
 *       It assumes that the event group and tasks are not reused elsewhere during execution.
 *
 * @return None
 */
void liot_group_event_test()
{
    // Create an event group
    LiotOSStatus_t result = liot_rtos_flag_create(&xEventGroupHandle);
    if (0 != result) {
        liot_trace( "main Failed to create event group");
        return;
    }

    liot_trace( "main create flag event group");

    // Create task 1
    liot_task_t task1_handle = NULL;
    result = liot_rtos_task_create(&task1_handle, 1024, LIOT_APP_TASK_PRIORITY, "Task1", &vTask1, NULL);
    if(result == 0)
    {
        liot_trace("Task1 task create success %d", result);
    }
    else
    {
        liot_trace("Task1 task create fail %d", result);
    } 
    // Create task 2
    liot_task_t task2_handle = NULL;
    result = liot_rtos_task_create(&task2_handle, 1024, LIOT_APP_TASK_PRIORITY, "Task2", &vTask2, NULL);
    if(result == 0)
    {
        liot_trace("Task2 task create success %d", result);
    }
    else
    {
        liot_trace("Task2 task create fail %d", result);
    } 
    // Create task 3
    liot_task_t task3_handle = NULL;
    result = liot_rtos_task_create(&task3_handle, 1024, LIOT_APP_TASK_PRIORITY, "Task3", &vTask3, NULL);
    if(result == 0)
    {
        liot_trace("Task3 task create success %d", result);
    }
    else
    {
        liot_trace("Task3 task create fail %d", result);
    } 

    liot_trace( "main create task success");
    
    // 创建定时器
    liot_timer_t timer_handle;
    liot_rtos_timer_create(&timer_handle, LIOT_TimerPeriodic, vTimerCallback, NULL);
    liot_rtos_timer_start(timer_handle, 10000); // 10秒
}

void liot_os_demo_thread(void *argv)
{
    test_count = 0;
    liot_rtos_task_sleep_ms(5000);
    char *msg = liot_rtos_malloc(MSG_MAX_SIZE);
    if (msg == NULL)
    {
        return;
    }
    memset(msg, 0, MSG_MAX_SIZE);

    liot_task_status_s xTaskStatus;

    liot_rtos_task_get_status(NULL, &xTaskStatus);

    //
    liot_trace("------------Get Task Info------------ ");
    liot_trace("eCurrentState = %d ", xTaskStatus.eCurrentState);
    liot_trace("pcTaskName = %s ", xTaskStatus.pcTaskName);
    liot_trace("usStackHighWaterMark = %d ", xTaskStatus.usStackHighWaterMark);
    liot_trace("uxCurrentPriority = %ld ", xTaskStatus.uxCurrentPriority);
    liot_trace("xHandle = %x ", (void *)xTaskStatus.xHandle);

    liot_trace("========== rtos systick:%ld", liot_rtos_get_system_tick());
    liot_trace("========== rtos run systick:%ld", liot_rtos_get_running_time());
    liot_trace("========== rtos rand:%ld", liot_rtos_rand());
    liot_trace("========== rtos task is running:%d", liot_rtos_is_alive(NULL));
    
    //Print out SRAM information
    liot_trace("========== rtos Get TotalHeapSize:%dKB,FreeHeapSize:%dKB,MinFreeHeapSize:%dKB,MaxFreeBlockSize:%dKB", 
        (liot_xPortGetTotalHeapSize()) >> 10, (liot_xPortGetFreeHeapSize()) >> 10,
        (liot_xPortGetMinimumEverFreeHeapSize()) >> 10, (liot_xPortGetMaximumFreeBlockSize()) >> 10);
    #if defined (PSRAM_FEATURE_ENABLE) && (PSRAM_EXIST==1)
    //Print SRAM information
    liot_trace("========== rtos GetPsram TotalHeapSize:%dKB,FreeHeapSize:%dKB,MinFreeHeapSize:%dKB,MaxFreeBlockSize:%dKB",
        (liot_psram_xPortGetTotalHeapSize()) >> 10, (liot_psram_xPortGetFreeHeapSize()) >> 10,
        (liot_psram_xPortGetMinimumEverFreeHeapSize()) >> 10, (liot_psram_xPortGetMaximumFreeBlockSize()) >> 10);
    #endif

    liot_rtos_semaphore_create(&rtos_test_sem, 0);
    liot_rtos_queue_create(&rtos_test_queue, MSG_MAX_SIZE, MSG_MAX_NUM);
    liot_rtos_timer_create(&rtos_timer_queue, 1, liot_os_demo_timer_cb, NULL);
    liot_rtos_timer_start(rtos_timer_queue, 5000);
    while (test_count <= 10)
    {
        liot_trace("main_task start");
        liot_rtos_semaphore_wait(rtos_test_sem, 0xFFFFFFFF); // wait for main task starting up

        liot_rtos_queue_wait(
            rtos_test_queue, (u8 *)msg, MSG_MAX_SIZE, 0xFFFFFFFF); // wait for the message from sub task
        liot_trace("main_task recv msg: %s,test_count %d", msg, test_count);
        test_count += 1;
        memset(msg, 0, MSG_MAX_SIZE);
        liot_rtos_task_sleep_ms(1000);
    }
    
    liot_rtos_task_get_status(NULL, &xTaskStatus);

    liot_trace("------------Get Task Info------------ ");
    liot_trace("eCurrentState = %d ", xTaskStatus.eCurrentState);
    liot_trace("pcTaskName = %s ", xTaskStatus.pcTaskName);
    liot_trace("usStackHighWaterMark = %d ", xTaskStatus.usStackHighWaterMark);
    liot_trace("uxCurrentPriority = %ld ", xTaskStatus.uxCurrentPriority);
    liot_trace("xHandle = %x ", (void *)xTaskStatus.xHandle);
    liot_trace("------------Get Task Info------------ ");

    liot_rtos_free(msg);
    liot_trace("delete main task");
    liot_rtos_semaphore_delete(rtos_test_sem);
    rtos_test_sem = NULL;
    liot_rtos_queue_delete(rtos_test_queue);
    rtos_test_queue = NULL;
    liot_rtos_timer_delete(rtos_timer_queue);
    rtos_timer_queue = NULL;

    //group event test
    liot_group_event_test();

    while(1)
    {
        liot_rtos_task_sleep_ms(1000);
    }
}
