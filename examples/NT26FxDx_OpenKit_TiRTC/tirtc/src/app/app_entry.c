/*
 * SPDX-FileCopyrightText: 2025 Lierda Technology Co., Ltd.
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_entry.c
 * @brief Product task composition root for the NT26F6D0 board.
 *
 * Starts all hardware-ready modules as independent tasks.
 * Each module is guarded by an HWDEMO_*_EN build option.
 * A module marked 'n' is excluded from compilation entirely,
 * so an unstable module cannot affect the rest of the system.
 *
 * Derived from Lierda's board-demo composition root and modified for the
 * Tange TiRTC product application.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_log.h"
#include "liot_os.h"
#include "liot_power.h"

#include <stdbool.h>

#ifdef HWDEMO_KEY_EN
#include "key_input.h"
#endif

#ifdef HWDEMO_LCD_SSD1306_EN
#include "ui_controller.h"
#endif

#ifdef HWDEMO_NET_TIME_EN
#include "network_manager.h"
#endif

#ifdef HWDEMO_BINDING_EN
#include "device_binding.h"
#endif

#ifdef HWDEMO_TIRTC_EN
#include "tirtc_runtime.h"
#endif

#ifdef HWDEMO_AI_CHAT_EN
#include "ai_chat.h"
#endif

#ifdef HWDEMO_WECHAT_EN
#include "wechat_call.h"
#endif

#ifdef HWDEMO_DEV_CHAT_EN
#include "device_call.h"
#endif

#ifdef HWDEMO_LIVE_TALK_EN
#include "platform_intercom.h"
#endif

void user_main(void)
{
#ifdef HWDEMO_BINDING_EN
    /* Binding may start only after every enabled MQTT router task and the
     * network owner have been created.  Once an owner task is running it may
     * hold queues, semaphores or callbacks, so boot never tries to roll it
     * back by deleting the task. */
    bool binding_prerequisites_created = true;
#endif
#ifdef HWDEMO_TIRTC_EN
    /* Set only after the binding owner is created, then cleared if the LIVE
     * listener cannot be made ready before TiRTC Start. */
    bool tirtc_prerequisites_ready = false;
#endif
#ifdef HWDEMO_AI_CHAT_EN
    bool tirtc_task_created = false;
#endif
    UINT8 powerup_reason = (UINT8)LIOT_PWRUP_UNKNOWN;
    liot_power_errcode_e power_ret = liot_get_powerup_reason(&powerup_reason);

    liot_trace("L_CT4IT00_YP00W_01_V04 TiRTC UI demo start\r\n");
    liot_trace("[BOOT] powerup_reason=%u ret=0x%x "
               "(5=watchdog,7=panic)\r\n",
               (unsigned int)powerup_reason,
               (unsigned int)power_ret);

#ifdef HWDEMO_LCD_SSD1306_EN
    if (demo_ui_init() != 0)
    {
        liot_trace("[BOOT] UI init failed; key events unavailable\r\n");
    }

    liot_task_t ui_handle = NULL;
    if (liot_rtos_task_create(&ui_handle, 8192, LIOT_APP_TASK_PRIORITY,
                              "demo_ui", demo_ui_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] UI task create failed; LIVE admission unavailable\r\n");
        demo_ui_deinit();
    }
#endif

#ifdef HWDEMO_KEY_EN
    liot_task_t key_handle = NULL;
    if (liot_rtos_task_create(&key_handle, 4096, LIOT_APP_TASK_PRIORITY,
                              "demo_key", demo_key_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] key task create failed\r\n");
    }
#endif

#ifdef HWDEMO_WECHAT_EN
    /* Register MQTT/TiRTC WX routers before binding can bring the permanent
     * MQTT connection online, so a boot-time call_incoming cannot be lost. */
    liot_task_t wechat_handle = NULL;
    /* Keep the validated 48 KiB worker stack; HTTP bodies use heap-owned jobs
     * and the allocator-mismatch fix below does not require extra RAM. */
    if (liot_rtos_task_create(&wechat_handle, 48 * 1024,
                              LIOT_APP_TASK_PRIORITY,
                              "demo_wechat", demo_wechat_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] WeChat task create failed; binding blocked\r\n");
        demo_wechat_mark_unavailable();
#ifdef HWDEMO_BINDING_EN
        binding_prerequisites_created = false;
#endif
    }
#endif

#ifdef HWDEMO_DEV_CHAT_EN
    /* Device-call MQTT routing must exist before formal MQTT comes online;
     * this also keeps every call HTTP/media operation off the UI task. */
    liot_task_t dev_chat_handle = NULL;
    /* HTTP cleanup runs in a separate 12 KiB worker.  The DEV owner itself
     * uses a conservative 32 KiB stack (its measured C call chain is well
     * below this), preserving heap for TiRTC connection threads. */
    if (liot_rtos_task_create(&dev_chat_handle, 32 * 1024,
                              LIOT_APP_TASK_PRIORITY,
                              "demo_dev_chat", demo_dev_chat_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] device chat task create failed; binding blocked\r\n");
        demo_dev_chat_mark_unavailable();
#ifdef HWDEMO_BINDING_EN
        binding_prerequisites_created = false;
#endif
    }
#endif

#ifdef HWDEMO_NET_TIME_EN
    liot_task_t network_handle = NULL;
    if (liot_rtos_task_create(&network_handle, 10240,
                              LIOT_APP_TASK_PRIORITY,
                              "demo_net_time", demo_net_time_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] network task create failed; binding blocked\r\n");
#ifdef HWDEMO_BINDING_EN
        binding_prerequisites_created = false;
#endif
    }
#endif

#ifdef HWDEMO_BINDING_EN
    liot_task_t binding_handle = NULL;
    if (!binding_prerequisites_created)
    {
        liot_trace("[BOOT] binding task not created; network/MQTT router "
                   "prerequisite unavailable\r\n");
    }
    else if (liot_rtos_task_create(&binding_handle, 12288,
                                   LIOT_APP_TASK_PRIORITY,
                                   "demo_binding", demo_binding_task,
                                   NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] binding task create failed; TiRTC blocked\r\n");
    }
    else
    {
#ifdef HWDEMO_TIRTC_EN
        tirtc_prerequisites_ready = true;
#endif
    }
#endif

#ifdef HWDEMO_TIRTC_EN
    /* Register the passive platform-LIVE router before the shared TiRTC
     * runtime can begin accepting device-mode connections. */
#ifdef HWDEMO_LIVE_TALK_EN
    liot_task_t live_talk_handle = NULL;
    if (!tirtc_prerequisites_ready)
    {
        liot_trace("[BOOT] platform LIVE task not created; binding owner "
                   "unavailable\r\n");
    }
    else if (liot_rtos_task_create(&live_talk_handle, 24 * 1024,
                                   LIOT_APP_TASK_PRIORITY,
                                   "demo_live_talk", demo_live_talk_task,
                                   NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] platform LIVE task create failed; TiRTC blocked\r\n");
        tirtc_prerequisites_ready = false;
    }
    else if (!demo_live_talk_wait_ready(2000U))
    {
        /* The task may already own RTOS objects.  Retain it and fail closed
         * instead of deleting it while initialization may still be active. */
        liot_trace("[BOOT] platform LIVE router readiness timeout; "
                   "task retained, TiRTC blocked\r\n");
        tirtc_prerequisites_ready = false;
    }
#endif

    /* One device-authenticated runtime is shared by AI/WX/DEV/LIVE sessions. */
    liot_task_t tirtc_demo_handle = NULL;
    if (!tirtc_prerequisites_ready)
    {
        liot_trace("[BOOT] TiRTC runtime task not created; binding/LIVE "
                   "prerequisite unavailable\r\n");
    }
    else if (liot_rtos_task_create(&tirtc_demo_handle, 64 * 1024,
                                   LIOT_APP_TASK_PRIORITY,
                                   "demo_tirtc", demo_tirtc_task,
                                   NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] TiRTC runtime task create failed; dependent "
                   "tasks blocked\r\n");
    }
    else
    {
#ifdef HWDEMO_AI_CHAT_EN
        tirtc_task_created = true;
#endif
    }
#endif

#ifdef HWDEMO_AI_CHAT_EN
    liot_task_t ai_chat_handle = NULL;
    /* WHIP/HTTP + JSON + Opus all run in this worker.  Match the validated
     * AI demo headroom instead of risking a silent task-stack overflow. */
    if (!tirtc_task_created)
    {
        liot_trace("[BOOT] AI chat task not created; TiRTC owner unavailable\r\n");
        demo_ai_chat_mark_unavailable();
    }
    else if (liot_rtos_task_create(&ai_chat_handle, 96 * 1024,
                                   LIOT_APP_TASK_PRIORITY,
                                   "demo_ai_chat", demo_ai_chat_task,
                                   NULL) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BOOT] AI chat task create failed\r\n");
        demo_ai_chat_mark_unavailable();
    }
#endif

    /* USB logging enumerates after the earliest boot messages.  Repeat the
     * reset cause after tasks are running so a reconnecting terminal captures
     * whether the preceding reset was watchdog (5) or panic (7). */
    liot_rtos_task_sleep_s(10);
    liot_trace("[BOOT] delayed powerup_reason=%u ret=0x%x "
               "(5=watchdog,7=panic)\r\n",
               (unsigned int)powerup_reason,
               (unsigned int)power_ret);
}
