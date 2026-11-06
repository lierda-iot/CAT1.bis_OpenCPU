/**
 * @file demo_float.c
 * @brief LIoT Float and Long Long Printf Demonstration
 * @details This file demonstrates floating-point and 64-bit integer printf functionality.
 *          Uses vsnprintf with -u _printf_float linker flag to enable %f/%lf formatting.
 * @version 1.0
 * @date 2026-11-05
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * Demonstration Steps:
 * 1. Create a task that performs floating-point arithmetic
 * 2. Print float/double values using liot_trace with %f/%lf format
 * 3. Print 64-bit integers with various format specifiers                                                                        
 * 4. Repeat every 4 seconds 
 */

#include "liot_log.h"
#include "liot_os.h"

#include <stdint.h>

#define DEMO_FLOAT_TASK_STACK_SIZE  2048+4

static liot_task_t s_float_print_task = NULL;
static liot_StaticTask_t s_float_print_task_mem;
static uint8_t s_float_print_task_stack[DEMO_FLOAT_TASK_STACK_SIZE] __attribute__((aligned(8)));

/**
 * @brief Float and long long print task
 * @param argv Unused task parameter
 */
static void liot_float_print_task(void *argv)
{
    float a  = 3.14f;
    double b = 3.1445566;
    double c = 50.211;
    double d = a + c;
    double e = b * c;
    double f = c / a;
    unsigned long long x1 = 0xfeabcd0011223344ULL;
    long long x2          = 17564890027654LL;

    (void)argv;

    while (1)
    {
        // Demonstrate float/double arithmetic and formatting
        liot_trace("Float: a=%f, b=%lf, c=%lf, d=%lf, e=%lf, f=%lf\r\n",
                   (double)a, b, c, d, e, f);

        // Demonstrate unsigned long long formatting
        liot_trace("ULL X1: dec=%llu, hex=0x%llx, signed=%lld\r\n",
                   x1, x1, (long long)x1);

        // Demonstrate signed long long formatting
        liot_trace("LL X2: dec=%lld, hex=0x%llx, unsigned=%llu\r\n",
                   x2, (unsigned long long)x2, (unsigned long long)x2);

        liot_rtos_task_sleep_ms(4000);
    }

    liot_rtos_task_delete(NULL);
}

/**
 * @brief Float demo thread entry point
 * @param argv Unused task parameter
 */
void liot_float_demo_thread(void *argv)
{
    LiotOSStatus_t ret;

    (void)argv;

    ret = liot_rtos_task_create_static(&s_float_print_task,
                                       DEMO_FLOAT_TASK_STACK_SIZE,
                                       LIOT_APP_TASK_PRIORITY,
                                       "liot_float_print_task",
                                       liot_float_print_task,
                                       s_float_print_task_stack,
                                       &s_float_print_task_mem,
                                       NULL);
    if (ret != 0)
    {
        liot_trace("Float demo task create failed: %d", ret);
    }

    liot_rtos_task_delete(NULL);
}
