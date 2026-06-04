/**
 * @file user_main.c
 * @brief Application entry point for L_CT4IT00_YP00W_01_V04
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_log.h"
#include "liot_os.h"

void user_main(void)
{
    liot_trace("L_CT4IT00_YP00W_01_V04 app start");

    while (1)
    {
        liot_rtos_task_sleep_s(1);
    }
}
