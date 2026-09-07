/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 *
 * See the NOTICE file distributed with this work for additional
 * information regarding copyright ownership.
 */

#include "liot_log.h"
#include "liot_os.h"

void user_main(void)
{
    while(1)
    {
        printf("hello word! Total Heap Size [%d] Byte, Free Heap Size [%d] Byte", liot_xPortGetTotalHeapSize(), liot_xPortGetFreeHeapSize());
        liot_rtos_task_sleep_s(1);
    }    
}