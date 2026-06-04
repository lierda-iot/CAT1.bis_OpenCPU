/**
 * @file csq.c
 * @brief SIM signal strength polling task for siminfo display demo.
 *
 * Polls CSQ/RSRP/SNR every 2 seconds via liot_nw API and updates
 * the LVGL signal display.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_os.h"
#include "liot_nw.h"
#include "liot_log.h"
#include "lcd_gui.h"

#define CSQ_POLL_INTERVAL_MS    2000

static liot_task_t s_csq_task = NULL;

static void siminfo_csq_task(void *arg)
{
    liot_nw_signal_strength_info_s info = {0};

    while (1) {
        liot_nw_get_signal_strength(0, &info);
        liot_trace("siminfo csq:%d rsrp:%d snr:%d",
                   info.rssi, info.rsrp, info.snr);

        lvgl_signal_update(info.rssi, info.rsrp, info.snr);

        liot_rtos_task_sleep_ms(CSQ_POLL_INTERVAL_MS);
    }
}

/**
 * @brief Start the CSQ polling task.
 *
 * Safe to call multiple times — only creates the task once.
 */
void demo_siminfo_csq_start(void)
{
    if (s_csq_task == NULL) {
        liot_rtos_task_create(&s_csq_task, 10 * 1024,
                              LIOT_APP_TASK_PRIORITY,
                              "siminfo_csq_task",
                              siminfo_csq_task, NULL);
    }
}

/**
 * @brief Stop the CSQ polling task.
 */
void demo_siminfo_csq_stop(void)
{
    if (s_csq_task != NULL) {
        liot_rtos_task_delete(s_csq_task);
        s_csq_task = NULL;
    }
}
