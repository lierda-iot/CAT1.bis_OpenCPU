/**
 * @File Name: demo_timer.c
 * @brief  liot_timer hardware timer demo
 * @Author : lierda email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2026-07-16
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

/*
 * This demo shows how to use the liot_timer hardware timer wrapper:
 * 1. Periodic timer
 *    Configure a periodic timer (LIOT_TIMER_3, 500ms) whose expiry callback runs
 *    in ISR context and only increments a counter (keep the callback short).
 * 2. Runtime period change
 *    After a few seconds, Liot_TimerSetPeriod() changes the period to 200ms.
 *    Since the timer is running, the counter is reset immediately and re-times
 *    from now with the new period.
 * 3. Elapsed time read
 *    The main loop prints the tick counter and the current elapsed time (us)
 *    read back via Liot_TimerGetElapsedUs().
 *
 * NOTE: the callback runs in interrupt context - keep it short and non-blocking.
 *       In periodic mode the period should not be below ~400-500us.
 */
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_timer.h"

/* timer instance used by this demo (LIOT_TIMER_0 / LIOT_TIMER_3 are recommended) */
#define DEMO_TIMER_ID           LIOT_TIMER_3
/* initial period: 500ms (expressed in us) */
#define DEMO_TIMER_PERIOD_US    (500u * 1000u)
/* period after the runtime change: 200ms */
#define DEMO_TIMER_PERIOD2_US   (200u * 1000u)

/* incremented in ISR context, read in task context */
static volatile uint32_t s_timerTick = 0;

/**
 * @brief timer expiry callback, runs in ISR context.
 * @param arg user argument registered in the config (unused here)
 */
static void demo_timer_callback(void *arg)
{
    (void)arg;
    /* keep it short: just bump a counter, do the real work in the task */
    s_timerTick++;
}

void liot_timer_demo_thread(void *argv)
{
    (void)argv;

    liot_errcode_e ret;
    Liot_TimerCfg_t cfg;

    liot_rtos_task_sleep_s(2);

    memset(&cfg, 0x00, sizeof(cfg));
    cfg.id       = DEMO_TIMER_ID;
    cfg.mode     = LIOT_TIMER_MODE_PERIODIC;
    cfg.periodUs = DEMO_TIMER_PERIOD_US;
    cfg.callback = demo_timer_callback;
    cfg.arg      = NULL;

    ret = Liot_TimerInit(&cfg);
    if (ret != LIOT_SUCCESS)
    {
        liot_trace("liot_timer init failed: %d", ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    Liot_TimerStart(DEMO_TIMER_ID);
    liot_trace("liot_timer started, period=%ums", DEMO_TIMER_PERIOD_US / 1000u);

    /* run 10s at 500ms period */
    for (int i = 0; i < 10; i++)
    {
        liot_trace("tick=%u elapsed=%uus", s_timerTick, Liot_TimerGetElapsedUs(DEMO_TIMER_ID));
        liot_rtos_task_sleep_s(1);
    }

    /* change period to 200ms at runtime; timer resets and re-times from now */
    Liot_TimerSetPeriod(DEMO_TIMER_ID, DEMO_TIMER_PERIOD2_US);
    liot_trace("liot_timer period changed to %ums", DEMO_TIMER_PERIOD2_US / 1000u);

    while (1)
    {
        liot_trace("tick=%u elapsed=%uus", s_timerTick, Liot_TimerGetElapsedUs(DEMO_TIMER_ID));
        liot_rtos_task_sleep_s(1);
    }
}
