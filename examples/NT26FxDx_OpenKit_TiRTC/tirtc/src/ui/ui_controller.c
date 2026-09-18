/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ui_controller.c
 * @brief TiRTC menu state machine and the only OLED rendering owner.
 *
 * KEY0 moves down cyclically, KEY1 confirms and KEY2 returns.  TiRTC feature
 * actions are exposed through non-blocking facades, keeping UI policy separate
 * from the AI, WeChat and device-call workers.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "liot_log.h"
#include "liot_os.h"
#include "ssd1306_display.h"
#ifdef HWDEMO_NET_TIME_EN
#include "network_manager.h"
#endif
#ifdef HWDEMO_BINDING_EN
#include "device_binding.h"
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
#ifdef HWDEMO_TIRTC_EN
#include "tirtc_runtime.h"
#endif
#ifdef HWDEMO_LIVE_TALK_EN
#include "platform_intercom.h"
#endif
#include "ui_controller.h"

#define UI_QUEUE_DEPTH          16U
#define UI_WAIT_MS              500U
#define UI_AUDIO_HANDOFF_MS    6000U
#define UI_MAIN_ITEM_COUNT      4U
#define UI_MAIN_AI_INDEX        0U
#define UI_SETTINGS_ITEM_COUNT  4U

typedef enum
{
    UI_PAGE_HOME = 0,
    UI_PAGE_AI_CHAT,
    UI_PAGE_WECHAT,
    UI_PAGE_DEV_CHAT,
    UI_PAGE_SETTINGS,
} ui_page_e;

/*
 * Startup is a small gate in front of the normal page state machine.  This
 * keeps networking/binding policy out of the feature pages and leaves those
 * pages ready for the later TiRTC aichat/wechat/devchat handlers.
 */
typedef enum
{
    UI_STARTUP_WAIT_NETWORK = 0,
    UI_STARTUP_WAIT_BINDING,
    UI_STARTUP_READY,
} ui_startup_state_e;

static const char *const s_main_items[UI_MAIN_ITEM_COUNT] = {
    "AI",
    "WX",
    "DEV",
    "SET",
};

static const char *const s_settings_items[UI_SETTINGS_ITEM_COUNT] = {
    "V+",
    "V-",
    "M+",
    "M-",
};

static const char *const s_feature_titles[3] = {
    "AI CHAT",
    "WECHAT",
    "DEV CHAT",
};

static liot_queue_t s_ui_queue;
static volatile bool s_ui_accept_keys;
static ui_page_e s_page = UI_PAGE_HOME;
static ui_startup_state_e s_startup_state = UI_STARTUP_WAIT_NETWORK;
static uint8_t s_main_selected;
static uint8_t s_settings_selected;
static uint8_t s_speaker_level = 8U;
static uint8_t s_mic_level = 10U;
static bool s_dirty = true;
#ifdef HWDEMO_AI_CHAT_EN
static uint32_t s_ai_request_id;
#endif
#ifdef HWDEMO_WECHAT_EN
static uint32_t s_wx_page_session;
#endif
#ifdef HWDEMO_DEV_CHAT_EN
static uint32_t s_dev_page_session;
#endif

static void ui_draw_grid_item(uint8_t index, const char *label,
                              uint8_t selected)
{
    char text[8];
    uint32_t x = (index & 1U) ? 68U : 0U;
    uint32_t y = (index < 2U) ? 12U : 40U;

    snprintf(text, sizeof(text), "%c%s", selected ? '>' : ' ', label);
    demo_oled_draw_text(x, y, text, 2);
}

static void ui_draw_startup_page(const char *title,
                                 const char *main_text,
                                 const char *footer)
{
    demo_oled_draw_text_centered(0, title, 1);
    demo_oled_draw_text_centered(20, main_text, 2);
    if (footer != NULL && footer[0] != '\0')
    {
        demo_oled_draw_text_centered(50, footer, 1);
    }
}

int demo_ui_init(void)
{
    if (s_ui_queue != NULL)
    {
        return 0;
    }

    if (liot_rtos_queue_create(&s_ui_queue, sizeof(demo_ui_key_e),
                               UI_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[UI] key queue create failed\r\n");
        return -1;
    }

    liot_trace("[UI] key queue ready depth=%u\r\n",
               (unsigned int)UI_QUEUE_DEPTH);
    return 0;
}

void demo_ui_deinit(void)
{
    /* GPIO/wakeup interrupts have no unregister operation in this product
     * layer.  Keep the small queue alive for the process lifetime and only
     * close its admission gate.  Deleting it here would race an ISR that had
     * already observed the old handle. */
    liot_rtos_enter_critical();
    s_ui_accept_keys = false;
    liot_rtos_exit_critical();
}

void demo_ui_post_key_from_isr(demo_ui_key_e key)
{
    if (s_ui_accept_keys && s_ui_queue != NULL && key <= DEMO_UI_KEY_BACK)
    {
        liot_rtos_queue_release_isr(s_ui_queue, sizeof(key), (uint8 *)&key);
    }
}

static void ui_notify_feature(demo_ui_feature_e feature, bool entering)
{
    liot_trace("[UI] feature=%d %s\r\n", (int)feature,
               entering ? "ENTER" : "LEAVE");
#ifdef HWDEMO_AI_CHAT_EN
    if (feature == DEMO_UI_FEATURE_AI_CHAT)
    {
        if (entering)
        {
            s_ai_request_id = demo_ai_chat_start();
        }
        else
        {
            demo_ai_chat_stop();
            s_ai_request_id = 0U;
        }
        return;
    }
#endif
#ifdef HWDEMO_WECHAT_EN
    if (feature == DEMO_UI_FEATURE_WECHAT)
    {
        if (entering)
        {
            demo_wechat_enter();
        }
        else
        {
            demo_wechat_leave();
            s_wx_page_session = 0U;
        }
        return;
    }
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    if (feature == DEMO_UI_FEATURE_DEV_CHAT)
    {
        if (entering)
        {
            demo_dev_chat_enter();
        }
        else
        {
            demo_dev_chat_leave();
            s_dev_page_session = 0U;
        }
        return;
    }
#endif
    /* Every valid feature is handled above when its build switch is enabled.
     * A disabled feature intentionally has no runtime side effect. */
    (void)entering;
}

static void ui_sync_live_policy(void)
{
    bool ai_idle = true;
    bool wechat_idle = true;
    bool dev_idle = true;
    bool feature_audio_idle;
#ifdef HWDEMO_WECHAT_EN
    bool wechat_signal_ready;
#endif

#ifdef HWDEMO_AI_CHAT_EN
    ai_idle = demo_ai_chat_is_idle();
#endif
#ifdef HWDEMO_WECHAT_EN
    wechat_idle = demo_wechat_is_idle();
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    dev_idle = demo_dev_chat_is_idle();
#endif
#ifdef HWDEMO_WECHAT_EN
    /* Incoming WeChat calls are allowed while the user is either on HOME or
     * browsing the idle WX page.  Other feature pages still reject them so
     * microphone/speaker ownership remains unambiguous. */
    wechat_signal_ready = s_startup_state == UI_STARTUP_READY &&
                          (s_page == UI_PAGE_HOME ||
                           s_page == UI_PAGE_WECHAT) &&
                          ai_idle && dev_idle;
    demo_wechat_set_home_allowed(wechat_signal_ready);
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    /* Device calls may ring on HOME or while browsing the idle DEV page.
     * A call still owns the same single ES8311/TiRTC connection budget. */
    demo_dev_chat_set_home_allowed(
        s_startup_state == UI_STARTUP_READY &&
        (s_page == UI_PAGE_HOME || s_page == UI_PAGE_DEV_CHAT) &&
        ai_idle && wechat_idle);
#endif
    feature_audio_idle = ai_idle && wechat_idle && dev_idle;
#ifdef HWDEMO_LIVE_TALK_EN
    /* Platform LIVE may own the microphone/speaker only on the fully ready
     * HOME page.  Every other page closes an existing passive session and
     * rejects new incoming connections. */
    demo_live_talk_set_home_allowed(
        s_startup_state == UI_STARTUP_READY && s_page == UI_PAGE_HOME &&
        feature_audio_idle);
#endif
}

static uint32_t ui_handoff_remaining(uint32_t deadline)
{
    int32_t remaining = (int32_t)(deadline -
                                  liot_rtos_get_running_time());

    return remaining > 0 ? (uint32_t)remaining : 0U;
}

/* All voice features share one TiRTC connection budget and one ES8311/I2S
 * adapter.  Feature stop is asynchronous, so a quick KEY2 -> KEY1 sequence
 * must wait for the previous owner before starting the next one.  An incoming
 * WX ring is the one exception: keep that signaling session while waiting for
 * AI/LIVE to release their media ownership before answering it. */
static bool ui_wait_audio_released(bool keep_wechat_ring,
                                   bool keep_dev_ring)
{
    uint32_t deadline = liot_rtos_get_running_time() + UI_AUDIO_HANDOFF_MS;
#if defined(HWDEMO_LIVE_TALK_EN) || defined(HWDEMO_TIRTC_EN)
    uint32_t remaining;
#endif
    bool idle;

    (void)keep_wechat_ring;
    (void)keep_dev_ring;

    while (1)
    {
        idle = true;
#ifdef HWDEMO_AI_CHAT_EN
        idle = idle && demo_ai_chat_is_idle();
#endif
#ifdef HWDEMO_WECHAT_EN
        if (!keep_wechat_ring)
        {
            idle = idle && demo_wechat_is_idle();
        }
#endif
#ifdef HWDEMO_DEV_CHAT_EN
        if (!keep_dev_ring)
        {
            idle = idle && demo_dev_chat_is_idle();
        }
#endif
        if (idle)
        {
            break;
        }
        if (ui_handoff_remaining(deadline) == 0U)
        {
            liot_trace("[UI] feature cleanup timeout; entry cancelled\r\n");
            return false;
        }
        liot_rtos_task_sleep_ms(20U);
    }

#ifdef HWDEMO_LIVE_TALK_EN
    remaining = ui_handoff_remaining(deadline);
    if (remaining == 0U || !demo_live_talk_wait_idle(remaining))
    {
        liot_trace("[UI] LIVE cleanup timeout; feature entry cancelled\r\n");
        return false;
    }
#endif
#ifdef HWDEMO_TIRTC_EN
    remaining = ui_handoff_remaining(deadline);
    if (remaining == 0U || !demo_tirtc_wait_connections_idle(remaining))
    {
        liot_trace("[UI] rejected TiRTC connection still closing; "
                   "feature entry cancelled\r\n");
        return false;
    }
#endif
    return true;
}

#ifdef HWDEMO_WECHAT_EN
static void ui_wx_open_incoming_page(const demo_wechat_snapshot_t *wx)
{
    if (wx == NULL || wx->state != DEMO_WECHAT_RINGING)
    {
        return;
    }
    if (s_page != UI_PAGE_WECHAT)
    {
        s_page = UI_PAGE_WECHAT;
        ui_sync_live_policy();
    }
    s_wx_page_session = wx->session_sequence;
    s_dirty = true;
}

/* A ringing call uses one input policy on both HOME and the WX page:
 * KEY1 answers immediately; KEY2 rejects/hangs up. */
static bool ui_wx_handle_ringing_key(demo_ui_key_e key,
                                     const demo_wechat_snapshot_t *wx)
{
    if (wx == NULL || wx->state != DEMO_WECHAT_RINGING)
    {
        return false;
    }
    ui_wx_open_incoming_page(wx);
    if (key == DEMO_UI_KEY_BACK)
    {
        demo_wechat_hangup();
        liot_trace("[UI] WX incoming rejected generation=%u\r\n",
                   (unsigned int)wx->incoming_generation);
        return true;
    }
    if (key != DEMO_UI_KEY_CONFIRM)
    {
        return true;
    }

    ui_sync_live_policy();
    if (ui_wait_audio_released(true, false) &&
        demo_wechat_answer(wx->incoming_generation))
    {
        s_wx_page_session = wx->session_sequence;
        liot_trace("[UI] WX incoming accepted generation=%u\r\n",
                   (unsigned int)wx->incoming_generation);
    }
    else
    {
        liot_trace("[UI] WX incoming answer deferred/failed generation=%u\r\n",
                   (unsigned int)wx->incoming_generation);
    }
    s_dirty = true;
    return true;
}
#endif

#ifdef HWDEMO_DEV_CHAT_EN
static void ui_dev_open_incoming_page(const demo_dev_chat_snapshot_t *dev)
{
    if (dev == NULL || dev->state != DEMO_DEV_CHAT_RINGING)
    {
        return;
    }
    if (s_page != UI_PAGE_DEV_CHAT)
    {
        s_page = UI_PAGE_DEV_CHAT;
        ui_sync_live_policy();
    }
    s_dev_page_session = dev->session_sequence;
    s_dirty = true;
}

/* Device calls use the common three-key policy: KEY1 answers, KEY2 rejects.
 * The worker still validates incoming_generation so a late key cannot answer
 * a replaced/expired call. */
static bool ui_dev_handle_ringing_key(demo_ui_key_e key,
                                      const demo_dev_chat_snapshot_t *dev)
{
    if (dev == NULL || dev->state != DEMO_DEV_CHAT_RINGING)
    {
        return false;
    }
    ui_dev_open_incoming_page(dev);
    if (key == DEMO_UI_KEY_BACK)
    {
        demo_dev_chat_hangup();
        liot_trace("[UI] DEV incoming rejected generation=%u\r\n",
                   (unsigned int)dev->incoming_generation);
        return true;
    }
    if (key == DEMO_UI_KEY_CONFIRM)
    {
        ui_sync_live_policy();
        if (ui_wait_audio_released(false, true) &&
            demo_dev_chat_answer(dev->incoming_generation))
        {
            s_dev_page_session = dev->session_sequence;
            liot_trace("[UI] DEV incoming accepted generation=%u\r\n",
                       (unsigned int)dev->incoming_generation);
        }
        else
        {
            liot_trace("[UI] DEV incoming answer deferred/failed "
                       "generation=%u\r\n",
                       (unsigned int)dev->incoming_generation);
        }
        s_dirty = true;
        return true;
    }
    return true;
}
#endif

static void ui_update_startup_state(void)
{
    ui_startup_state_e next = UI_STARTUP_READY;

#ifdef HWDEMO_NET_TIME_EN
    if (!demo_net_time_is_data_ready())
    {
        next = UI_STARTUP_WAIT_NETWORK;
    }
#endif

#ifdef HWDEMO_BINDING_EN
    if (next == UI_STARTUP_READY)
    {
        demo_binding_snapshot_t binding;

        demo_binding_get_snapshot(&binding);
        if (binding.state != DEMO_BIND_BOUND)
        {
            next = UI_STARTUP_WAIT_BINDING;
        }
    }
#endif

    if (next != s_startup_state)
    {
        liot_trace("[UI] startup state %d -> %d\r\n",
                   (int)s_startup_state, (int)next);
        if (s_startup_state == UI_STARTUP_READY &&
            s_page >= UI_PAGE_AI_CHAT && s_page <= UI_PAGE_DEV_CHAT)
        {
            ui_notify_feature(
                (demo_ui_feature_e)(s_page - UI_PAGE_AI_CHAT), false);
        }
        s_startup_state = next;
        s_page = UI_PAGE_HOME;
        ui_sync_live_policy();
        s_dirty = true;
        if (next == UI_STARTUP_READY)
        {
            liot_trace("[UI] network and binding ready -> MENU\r\n");
#ifdef HWDEMO_AI_CHAT_EN
            if (s_main_selected == UI_MAIN_AI_INDEX)
            {
                demo_ai_chat_prepare();
            }
#endif
        }
    }
}

static void ui_apply_audio_setting(void)
{
#ifdef HWDEMO_AI_CHAT_EN
    demo_ai_chat_set_audio_levels(s_speaker_level, s_mic_level);
#endif
#ifdef HWDEMO_LIVE_TALK_EN
    demo_live_talk_set_audio_levels(s_speaker_level, s_mic_level);
#endif
#ifdef HWDEMO_WECHAT_EN
    demo_wechat_set_audio_levels(s_speaker_level, s_mic_level);
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    demo_dev_chat_set_audio_levels(s_speaker_level, s_mic_level);
#endif
    liot_trace("[UI] audio levels speaker=%u mic=%u\r\n",
               (unsigned int)s_speaker_level,
               (unsigned int)s_mic_level);
}

static void ui_handle_key(demo_ui_key_e key)
{
    if (s_startup_state != UI_STARTUP_READY)
    {
#ifdef HWDEMO_BINDING_EN
        if (s_startup_state == UI_STARTUP_WAIT_BINDING &&
            key == DEMO_UI_KEY_CONFIRM)
        {
            /* Allows a manual retry after an error; duplicates are ignored. */
            demo_binding_request();
        }
#endif
        s_dirty = true;
        liot_trace("[UI] startup key=%d state=%d\r\n",
                   (int)key, (int)s_startup_state);
        return;
    }

    if (s_page == UI_PAGE_HOME)
    {
#ifdef HWDEMO_WECHAT_EN
        demo_wechat_snapshot_t wx;

        demo_wechat_get_snapshot(&wx);
        if (ui_wx_handle_ringing_key(key, &wx))
        {
            return;
        }
#endif
#ifdef HWDEMO_DEV_CHAT_EN
        {
            demo_dev_chat_snapshot_t dev;

            demo_dev_chat_get_snapshot(&dev);
            if (ui_dev_handle_ringing_key(key, &dev))
            {
                return;
            }
        }
#endif
        if (key == DEMO_UI_KEY_NEXT)
        {
            s_main_selected =
                (uint8_t)((s_main_selected + 1U) % UI_MAIN_ITEM_COUNT);
#ifdef HWDEMO_AI_CHAT_EN
            if (s_main_selected == UI_MAIN_AI_INDEX)
            {
                demo_ai_chat_prepare();
            }
#endif
        }
        else if (key == DEMO_UI_KEY_CONFIRM)
        {
#ifdef HWDEMO_AI_CHAT_EN
            /* If an old one-shot credential expired while the menu was
             * idle, start refreshing it before the shared-audio handoff.
             * The AI worker can overlap that HTTP work with LIVE teardown. */
            if (s_main_selected == UI_MAIN_AI_INDEX)
            {
                demo_ai_chat_prepare();
            }
#endif
            s_page = (ui_page_e)(UI_PAGE_AI_CHAT + s_main_selected);
            ui_sync_live_policy();
            if (!ui_wait_audio_released(false, false))
            {
                s_page = UI_PAGE_HOME;
                ui_sync_live_policy();
                s_dirty = true;
                return;
            }
            if (s_page != UI_PAGE_SETTINGS)
            {
                ui_notify_feature(
                    (demo_ui_feature_e)(s_page - UI_PAGE_AI_CHAT), true);
            }
        }
    }
#ifdef HWDEMO_WECHAT_EN
    else if (s_page == UI_PAGE_WECHAT)
    {
        demo_wechat_snapshot_t wx;

        demo_wechat_get_snapshot(&wx);
        if (ui_wx_handle_ringing_key(key, &wx))
        {
            return;
        }
        if (key == DEMO_UI_KEY_NEXT && wx.state == DEMO_WECHAT_READY)
        {
            demo_wechat_select_next();
        }
        else if (key == DEMO_UI_KEY_CONFIRM &&
                 wx.state == DEMO_WECHAT_READY)
        {
            if (demo_wechat_call_selected())
            {
                s_wx_page_session = 0U;
            }
        }
        else if (key == DEMO_UI_KEY_BACK)
        {
            if (demo_wechat_blocks_live())
            {
                /* Keep the call page visible until the worker confirms
                 * reject/hangup cleanup, then the completion path goes HOME. */
                demo_wechat_hangup();
                liot_trace("[UI] WX KEY2 hangup session=%u\r\n",
                           (unsigned int)s_wx_page_session);
            }
            else
            {
                demo_wechat_leave();
                s_wx_page_session = 0U;
                s_page = UI_PAGE_HOME;
                ui_sync_live_policy();
            }
        }
    }
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    else if (s_page == UI_PAGE_DEV_CHAT)
    {
        demo_dev_chat_snapshot_t dev;

        demo_dev_chat_get_snapshot(&dev);
        if (ui_dev_handle_ringing_key(key, &dev))
        {
            return;
        }
        if (key == DEMO_UI_KEY_NEXT &&
            dev.state == DEMO_DEV_CHAT_READY)
        {
            demo_dev_chat_select_next();
        }
        else if (key == DEMO_UI_KEY_CONFIRM &&
                 dev.state == DEMO_DEV_CHAT_READY)
        {
            uint32_t sequence = demo_dev_chat_call_selected();

            if (sequence != 0U)
            {
                s_dev_page_session = sequence;
            }
        }
        else if (key == DEMO_UI_KEY_BACK)
        {
            if (demo_dev_chat_blocks_live())
            {
                /* The completion sequence keeps this page alive until the
                 * room and P2P connection have both been released. */
                demo_dev_chat_hangup();
                liot_trace("[UI] DEV KEY2 hangup session=%u\r\n",
                           (unsigned int)s_dev_page_session);
            }
            else
            {
                demo_dev_chat_leave();
                s_dev_page_session = 0U;
                s_page = UI_PAGE_HOME;
                ui_sync_live_policy();
            }
        }
    }
#endif
    else if (s_page == UI_PAGE_SETTINGS)
    {
        if (key == DEMO_UI_KEY_NEXT)
        {
            s_settings_selected =
                (uint8_t)((s_settings_selected + 1U) %
                          UI_SETTINGS_ITEM_COUNT);
        }
        else if (key == DEMO_UI_KEY_CONFIRM)
        {
            switch (s_settings_selected)
            {
            case 0:
                if (s_speaker_level < 10U)
                {
                    s_speaker_level++;
                }
                break;
            case 1:
                if (s_speaker_level > 1U)
                {
                    s_speaker_level--;
                }
                break;
            case 2:
                if (s_mic_level < 10U)
                {
                    s_mic_level++;
                }
                break;
            case 3:
                if (s_mic_level > 1U)
                {
                    s_mic_level--;
                }
                break;
            default:
                break;
            }
            ui_apply_audio_setting();
        }
        else if (key == DEMO_UI_KEY_BACK)
        {
            s_page = UI_PAGE_HOME;
            ui_sync_live_policy();
        }
    }
    else if (key == DEMO_UI_KEY_BACK)
    {
#ifdef HWDEMO_AI_CHAT_EN
        bool prepare_ai = s_page == UI_PAGE_AI_CHAT;
#endif

        ui_notify_feature(
            (demo_ui_feature_e)(s_page - UI_PAGE_AI_CHAT), false);
        s_page = UI_PAGE_HOME;
        ui_sync_live_policy();
#ifdef HWDEMO_AI_CHAT_EN
        if (prepare_ai && s_main_selected == UI_MAIN_AI_INDEX)
        {
            demo_ai_chat_prepare();
        }
#endif
    }
    s_dirty = true;
    liot_trace("[UI] key=%d page=%d main=%u settings=%u\r\n",
               (int)key, (int)s_page,
               (unsigned int)s_main_selected,
               (unsigned int)s_settings_selected);
}

static void ui_draw_network_startup(void)
{
    const char *status = "NET INIT";

#ifdef HWDEMO_NET_TIME_EN
    switch (demo_net_time_get_state())
    {
    case DEMO_NET_REGISTERING:
        status = "WAIT LTE";
        break;
    case DEMO_NET_DATA_CALL:
        status = "START PDP";
        break;
    case DEMO_NET_TIME_SYNC:
        status = "SYNC TIME";
        break;
    case DEMO_NET_RETRY:
        status = "NET RETRY";
        break;
    case DEMO_NET_READY:
        status = "NET OK";
        break;
    case DEMO_NET_INIT:
    default:
        status = "NET INIT";
        break;
    }
#endif
    ui_draw_startup_page("NETWORK", status, "4G REQUIRED");
}

static void ui_draw_binding_startup(void)
{
#ifdef HWDEMO_BINDING_EN
    demo_binding_snapshot_t binding;
    char status[20];

    demo_binding_get_snapshot(&binding);
    switch (binding.state)
    {
    case DEMO_BIND_IDLE:
        ui_draw_startup_page("BINDING", "START BIND", NULL);
        break;
    case DEMO_BIND_WAIT_NETWORK:
        ui_draw_startup_page("BINDING", "WAIT 4G", "4G REQUIRED");
        break;
    case DEMO_BIND_DISCOVERING:
        ui_draw_startup_page("BINDING", "SERVICE", "GET SERVER");
        break;
    case DEMO_BIND_REPORTING:
        ui_draw_startup_page("BINDING", "GET CODE", NULL);
        break;
    case DEMO_BIND_WAIT_GRANT:
        snprintf(status, sizeof(status), "BIND WEB %u",
                 (unsigned int)binding.seconds_left);
        ui_draw_startup_page("BINDING", binding.code, status);
        break;
    case DEMO_BIND_VERIFYING:
        ui_draw_startup_page("BINDING", "CHECK", "SERVER");
        break;
    case DEMO_BIND_BOUND:
        ui_draw_startup_page("BINDING", "BIND OK", "OPEN MENU");
        break;
    case DEMO_BIND_RESTARTING:
        ui_draw_startup_page("UNBOUND", "RESTART", "KEEP DEVICE ID");
        break;
    case DEMO_BIND_ERROR:
    default:
        snprintf(status, sizeof(status), "ERR %d KEY1", binding.error);
        ui_draw_startup_page("BINDING", "BIND ERR", status);
        break;
    }
#else
    ui_draw_startup_page("BINDING", "DISABLED", NULL);
#endif
}

static void ui_draw_home(void)
{
    const char *network_text = "4G:--";
    const char *wx_text = "WX:--";
#ifdef HWDEMO_NET_TIME_EN
    liot_rtc_time_s now;
    demo_net_state_e net_state = demo_net_time_get_state();
    bool has_time = demo_net_time_get_local(&now);
#endif

#ifdef HWDEMO_WECHAT_EN
    {
        demo_wechat_snapshot_t wx;
        demo_wechat_get_snapshot(&wx);
        switch (wx.state)
        {
        case DEMO_WECHAT_SYNCING:
            wx_text = "WX:SYNC";
            break;
        case DEMO_WECHAT_READY:
            wx_text = "WX:OK";
            break;
        case DEMO_WECHAT_RINGING:
            wx_text = "WX:RING";
            break;
        case DEMO_WECHAT_DIALING:
        case DEMO_WECHAT_CONNECTING:
        case DEMO_WECHAT_WAIT_START:
        case DEMO_WECHAT_IN_CALL:
        case DEMO_WECHAT_STOPPING:
            wx_text = "WX:CALL";
            break;
        case DEMO_WECHAT_ERROR:
            wx_text = "WX:ERR";
            break;
        case DEMO_WECHAT_OFFLINE:
        default:
            wx_text = "WX:OFF";
            break;
        }
    }
#endif
    uint8_t i;

#ifdef HWDEMO_NET_TIME_EN
    if (demo_net_time_is_data_ready())
    {
        network_text = "4G:OK";
    }
    else if (net_state == DEMO_NET_REGISTERING)
    {
        network_text = "4G:REG";
    }
    else if (net_state == DEMO_NET_DATA_CALL)
    {
        network_text = "4G:PDP";
    }
    else if (net_state == DEMO_NET_RETRY)
    {
        network_text = "4G:ERR";
    }
#endif

    /* Network status stays at the top-left; time, when valid, is top-right. */
    demo_oled_draw_text(0, 0, network_text, 1);
    demo_oled_draw_text(42, 0, wx_text, 1);
#ifdef HWDEMO_NET_TIME_EN
    if (has_time)
    {
        char clock_text[6];
        snprintf(clock_text, sizeof(clock_text), "%02d:%02d",
                 now.tm_hour, now.tm_min);
        demo_oled_draw_text(96, 0, clock_text, 1);
    }
#endif
    for (i = 0; i < UI_MAIN_ITEM_COUNT; i++)
    {
        ui_draw_grid_item(i, s_main_items[i], i == s_main_selected);
    }
}

static void ui_draw_settings(void)
{
    char text[8];
    char level[4];
    uint8_t i;
    uint32_t x;
    uint32_t y;

    demo_oled_draw_text_centered(0, "SETTINGS", 1);
    for (i = 0; i < UI_SETTINGS_ITEM_COUNT; i++)
    {
        x = (i & 1U) ? 48U : 0U;
        y = (i < 2U) ? 12U : 40U;
        snprintf(text, sizeof(text), "%c%s",
                 (i == s_settings_selected) ? '>' : ' ',
                 s_settings_items[i]);
        demo_oled_draw_text(x, y, text, 2);
    }

    snprintf(level, sizeof(level), "%u",
             (unsigned int)s_speaker_level);
    demo_oled_draw_text(104, 12, level, 2);
    snprintf(level, sizeof(level), "%u", (unsigned int)s_mic_level);
    demo_oled_draw_text(104, 40, level, 2);
}

static void ui_draw_feature(void)
{
    uint8_t index = (uint8_t)(s_page - UI_PAGE_AI_CHAT);

#ifdef HWDEMO_AI_CHAT_EN
    if (s_page == UI_PAGE_AI_CHAT)
    {
        demo_ai_chat_snapshot_t ai;
        const char *status = "READY";
        char error_text[20];

        demo_ai_chat_get_snapshot(&ai);
        switch (ai.state)
        {
        case DEMO_AI_CHAT_GET_TOKEN:
            status = "GET TOKEN";
            break;
        case DEMO_AI_CHAT_START_SDK:
            status = "START RTC";
            break;
        case DEMO_AI_CHAT_CONNECTING:
            status = "CONNECT";
            break;
        case DEMO_AI_CHAT_NEGOTIATING:
            status = "START CHAT";
            break;
        case DEMO_AI_CHAT_LISTENING:
            status = "LISTENING";
            break;
        case DEMO_AI_CHAT_SPEAKING:
            status = "AI SPEAK";
            break;
        case DEMO_AI_CHAT_STOPPING:
            status = "STOPPING";
            break;
        case DEMO_AI_CHAT_ERROR:
            snprintf(error_text, sizeof(error_text), "ERR %d", ai.error);
            status = error_text;
            break;
        case DEMO_AI_CHAT_IDLE:
        default:
            status = "READY";
            break;
        }

        demo_oled_draw_text_centered(8, "AI CHAT", 2);
        demo_oled_draw_text_centered(36, status, 1);
        demo_oled_draw_text_centered(50, "KEY2 BACK", 1);
        return;
    }
#endif

#ifdef HWDEMO_WECHAT_EN
    if (s_page == UI_PAGE_WECHAT)
    {
        demo_wechat_snapshot_t wx;
        const char *status = "READY";
        const char *footer = "K2 HANGUP";
        char detail[22];
        char header[16];
        uint8_t name_scale;

        demo_wechat_get_snapshot(&wx);
        if (wx.state == DEMO_WECHAT_READY)
        {
            if (wx.contact_count > 0U)
            {
                snprintf(header, sizeof(header), "WX %u/%u",
                         (unsigned int)(wx.selected_contact + 1U),
                         (unsigned int)wx.contact_count);
                name_scale = strlen(wx.display_name) <= 10U ? 2U : 1U;
                demo_oled_draw_text_centered(3, header, 1);
                demo_oled_draw_text_centered(name_scale == 2U ? 21U : 24U,
                                             wx.display_name, name_scale);
                demo_oled_draw_text_centered(53, "K0 NEXT K1 CALL", 1);
            }
            else
            {
                demo_oled_draw_text_centered(3, "WECHAT", 1);
                demo_oled_draw_text_centered(23, "NO CONTACT", 2);
                demo_oled_draw_text_centered(53, "K2 BACK", 1);
            }
            return;
        }
        switch (wx.state)
        {
        case DEMO_WECHAT_OFFLINE:
            status = "WX OFFLINE";
            break;
        case DEMO_WECHAT_SYNCING:
            status = "SYNC CONTACT";
            break;
        case DEMO_WECHAT_RINGING:
            status = "INCOMING";
            footer = "K1 ANS K2 END";
            break;
        case DEMO_WECHAT_DIALING:
            status = "CALLING";
            break;
        case DEMO_WECHAT_CONNECTING:
            status = "CONNECT";
            break;
        case DEMO_WECHAT_WAIT_START:
            status = "WAIT START";
            break;
        case DEMO_WECHAT_IN_CALL:
            status = "WX TALK";
            break;
        case DEMO_WECHAT_STOPPING:
            status = "STOPPING";
            break;
        case DEMO_WECHAT_ERROR:
            snprintf(detail, sizeof(detail), "ERR %d", wx.error);
            status = detail;
            break;
        default:
            footer = "K2 BACK";
            break;
        }
        demo_oled_draw_text_centered(2, "WECHAT", 1);
        demo_oled_draw_text_centered(18, wx.display_name, 2);
        demo_oled_draw_text_centered(43, status, 1);
        demo_oled_draw_text_centered(54, footer, 1);
        return;
    }
#endif

#ifdef HWDEMO_DEV_CHAT_EN
    if (s_page == UI_PAGE_DEV_CHAT)
    {
        demo_dev_chat_snapshot_t dev;
        const char *status = "READY";
        const char *footer = "K2 HANGUP";
        char detail[22];
        char header[16];
        uint8_t name_scale;

        demo_dev_chat_get_snapshot(&dev);
        if (dev.state == DEMO_DEV_CHAT_READY)
        {
            if (dev.contact_count > 0U)
            {
                snprintf(header, sizeof(header), "DEV %u/%u",
                         (unsigned int)(dev.selected_contact + 1U),
                         (unsigned int)dev.contact_count);
                name_scale = strlen(dev.display_name) <= 10U ? 2U : 1U;
                demo_oled_draw_text_centered(3, header, 1);
                demo_oled_draw_text_centered(name_scale == 2U ? 21U : 24U,
                                             dev.display_name, name_scale);
                demo_oled_draw_text_centered(43,
                                             dev.selected_online ?
                                                 "ONLINE" : "OFFLINE",
                                             1);
                demo_oled_draw_text_centered(53, "K0 NEXT K1 CALL", 1);
            }
            else
            {
                demo_oled_draw_text_centered(3, "DEVICE", 1);
                demo_oled_draw_text_centered(23, "NO DEVICE", 2);
                demo_oled_draw_text_centered(53, "K2 BACK", 1);
            }
            return;
        }
        switch (dev.state)
        {
        case DEMO_DEV_CHAT_OFFLINE:
            status = "DEV OFFLINE";
            footer = "K2 BACK";
            break;
        case DEMO_DEV_CHAT_SYNCING:
            status = "SYNC DEVICE";
            footer = "K2 BACK";
            break;
        case DEMO_DEV_CHAT_RINGING:
            status = "INCOMING";
            footer = "K1 ANSWER K2 END";
            break;
        case DEMO_DEV_CHAT_DIALING:
            status = "CALLING";
            break;
        case DEMO_DEV_CHAT_CONNECTING:
            status = "CONNECT";
            break;
        case DEMO_DEV_CHAT_WAIT_CONFIRM:
            status = "WAIT PEER";
            break;
        case DEMO_DEV_CHAT_IN_CALL:
            status = "DEV TALK";
            break;
        case DEMO_DEV_CHAT_STOPPING:
            status = "STOPPING";
            break;
        case DEMO_DEV_CHAT_ERROR:
            snprintf(detail, sizeof(detail), "ERR %d", dev.error);
            status = detail;
            footer = "K2 BACK";
            break;
        default:
            break;
        }
        demo_oled_draw_text_centered(2, "DEVICE CALL", 1);
        demo_oled_draw_text_centered(18, dev.display_name, 2);
        demo_oled_draw_text_centered(43, status, 1);
        demo_oled_draw_text_centered(54, footer, 1);
        return;
    }
#endif

    demo_oled_draw_text_centered(
        8, index < 3U ? s_feature_titles[index] : "FEATURE", 2);
    demo_oled_draw_text_centered(36, "COMING SOON", 1);
    demo_oled_draw_text_centered(50, "KEY2 BACK", 1);
}

static void ui_render(void)
{
    if (!demo_oled_is_ready())
    {
        return;
    }

    s_dirty = false;
    demo_oled_begin_frame();
    if (s_startup_state == UI_STARTUP_WAIT_NETWORK)
    {
        ui_draw_network_startup();
    }
    else if (s_startup_state == UI_STARTUP_WAIT_BINDING)
    {
        ui_draw_binding_startup();
    }
    else if (s_page == UI_PAGE_HOME)
    {
        ui_draw_home();
    }
    else if (s_page == UI_PAGE_SETTINGS)
    {
        ui_draw_settings();
    }
    else
    {
        ui_draw_feature();
    }
    demo_oled_end_frame();
}

void demo_ui_task(void *argv)
{
    demo_ui_key_e key;
#ifdef HWDEMO_NET_TIME_EN
    demo_net_state_e previous_net_state = (demo_net_state_e)-1;
    bool previous_data_ready = false;
    int previous_minute = -1;
    liot_rtc_time_s now;
#endif
#ifdef HWDEMO_BINDING_EN
    demo_binding_snapshot_t previous_binding;
    demo_binding_snapshot_t binding;
    memset(&previous_binding, 0xff, sizeof(previous_binding));
#endif
#ifdef HWDEMO_AI_CHAT_EN
    demo_ai_chat_snapshot_t previous_ai;
    demo_ai_chat_snapshot_t ai;
    memset(&previous_ai, 0xff, sizeof(previous_ai));
#endif
#ifdef HWDEMO_WECHAT_EN
    demo_wechat_snapshot_t previous_wx;
    demo_wechat_snapshot_t wx;
    memset(&previous_wx, 0xff, sizeof(previous_wx));
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    demo_dev_chat_snapshot_t previous_dev;
    demo_dev_chat_snapshot_t dev;
    memset(&previous_dev, 0xff, sizeof(previous_dev));
#endif
    (void)argv;

    if (s_ui_queue == NULL && demo_ui_init() != 0)
    {
        liot_rtos_task_delete(NULL);
        return;
    }

    if (demo_oled_init() != 0)
    {
        liot_trace("[UI] OLED init failed, UI task stopped\r\n");
        demo_ui_deinit();
        liot_rtos_task_delete(NULL);
        return;
    }

    liot_rtos_enter_critical();
    s_ui_accept_keys = true;
    liot_rtos_exit_critical();
    liot_trace("[UI] ready: KEY0=NEXT KEY1=OK KEY2=BACK\r\n");
    ui_sync_live_policy();
    s_dirty = true;

    while (1)
    {
        if (liot_rtos_queue_wait(s_ui_queue, (uint8 *)&key, sizeof(key),
                                 UI_WAIT_MS) == LIOT_OSI_SUCCESS)
        {
            ui_handle_key(key);
        }

#ifdef HWDEMO_NET_TIME_EN
        if (previous_net_state != demo_net_time_get_state())
        {
            previous_net_state = demo_net_time_get_state();
            s_dirty = true;
        }

        if (previous_data_ready != demo_net_time_is_data_ready())
        {
            previous_data_ready = demo_net_time_is_data_ready();
            s_dirty = true;
        }

        if (s_page == UI_PAGE_HOME && demo_net_time_get_local(&now) &&
            previous_minute != now.tm_min)
        {
            previous_minute = now.tm_min;
            s_dirty = true;
        }
#endif

#ifdef HWDEMO_BINDING_EN
        demo_binding_get_snapshot(&binding);
        if (binding.state != previous_binding.state ||
            binding.seconds_left != previous_binding.seconds_left ||
            binding.error != previous_binding.error ||
            strcmp(binding.code, previous_binding.code) != 0)
        {
            previous_binding = binding;
            s_dirty = true;
        }
#endif

#ifdef HWDEMO_AI_CHAT_EN
        demo_ai_chat_get_snapshot(&ai);
        if (ai.state != previous_ai.state || ai.error != previous_ai.error ||
            ai.rx_dropped != previous_ai.rx_dropped ||
            ai.tx_dropped != previous_ai.tx_dropped ||
            ai.completed_request_id != previous_ai.completed_request_id ||
            ai.completed_result != previous_ai.completed_result)
        {
            previous_ai = ai;
            if (s_page == UI_PAGE_AI_CHAT)
            {
                s_dirty = true;
            }
        }
        if (s_page == UI_PAGE_AI_CHAT && s_ai_request_id != 0U &&
            ai.completed_request_id == s_ai_request_id &&
            ai.completed_result == 0)
        {
            liot_trace("[UI] AI request=%u complete -> MENU\r\n",
                       (unsigned int)s_ai_request_id);
            s_ai_request_id = 0U;
            s_page = UI_PAGE_HOME;
            ui_sync_live_policy();
            if (s_main_selected == UI_MAIN_AI_INDEX)
            {
                demo_ai_chat_prepare();
            }
            s_dirty = true;
        }
#endif

#ifdef HWDEMO_WECHAT_EN
        demo_wechat_get_snapshot(&wx);
        if (wx.state == DEMO_WECHAT_RINGING &&
            wx.incoming_generation != previous_wx.incoming_generation &&
            (s_page == UI_PAGE_HOME || s_page == UI_PAGE_WECHAT))
        {
            ui_wx_open_incoming_page(&wx);
            liot_trace("[UI] WX incoming page generation=%u session=%u\r\n",
                       (unsigned int)wx.incoming_generation,
                       (unsigned int)wx.session_sequence);
        }
        if (wx.state != previous_wx.state ||
            wx.error != previous_wx.error ||
            wx.contact_count != previous_wx.contact_count ||
            wx.selected_contact != previous_wx.selected_contact ||
            wx.contacts_revision != previous_wx.contacts_revision ||
            wx.incoming_generation != previous_wx.incoming_generation ||
            wx.session_sequence != previous_wx.session_sequence ||
            wx.completed_sequence != previous_wx.completed_sequence ||
            strcmp(wx.display_name, previous_wx.display_name) != 0)
        {
            previous_wx = wx;
            if (s_page == UI_PAGE_HOME || s_page == UI_PAGE_WECHAT)
            {
                s_dirty = true;
            }
        }
        if (s_page == UI_PAGE_WECHAT)
        {
            bool has_session =
                wx.state == DEMO_WECHAT_RINGING ||
                wx.state == DEMO_WECHAT_DIALING ||
                wx.state == DEMO_WECHAT_CONNECTING ||
                wx.state == DEMO_WECHAT_WAIT_START ||
                wx.state == DEMO_WECHAT_IN_CALL ||
                wx.state == DEMO_WECHAT_STOPPING;

            if (has_session && s_wx_page_session == 0U)
            {
                s_wx_page_session = wx.session_sequence;
            }
            if (!has_session && s_wx_page_session != 0U &&
                wx.completed_sequence == s_wx_page_session)
            {
                liot_trace("[UI] WX session=%u complete -> MENU\r\n",
                           (unsigned int)s_wx_page_session);
                s_wx_page_session = 0U;
                s_page = UI_PAGE_HOME;
                ui_sync_live_policy();
                s_dirty = true;
            }
        }
#endif

#ifdef HWDEMO_DEV_CHAT_EN
        demo_dev_chat_get_snapshot(&dev);
        if (dev.state == DEMO_DEV_CHAT_RINGING &&
            dev.incoming_generation != previous_dev.incoming_generation &&
            (s_page == UI_PAGE_HOME || s_page == UI_PAGE_DEV_CHAT))
        {
            ui_dev_open_incoming_page(&dev);
            liot_trace("[UI] DEV incoming page generation=%u session=%u\r\n",
                       (unsigned int)dev.incoming_generation,
                       (unsigned int)dev.session_sequence);
        }
        else if (s_page == UI_PAGE_HOME &&
                 dev.session_sequence != previous_dev.session_sequence &&
                 (dev.state == DEMO_DEV_CHAT_DIALING ||
                  dev.state == DEMO_DEV_CHAT_WAIT_CONFIRM ||
                  dev.state == DEMO_DEV_CHAT_IN_CALL))
        {
            /* A caller room can be recovered after reboot without a key
             * event.  Surface that restored session instead of leaving an
             * active microphone/connection hidden behind the HOME page. */
            s_page = UI_PAGE_DEV_CHAT;
            s_dev_page_session = dev.session_sequence;
            ui_sync_live_policy();
            s_dirty = true;
            liot_trace("[UI] DEV recovered caller session=%u -> page\r\n",
                       (unsigned int)dev.session_sequence);
        }
        if (dev.state != previous_dev.state ||
            dev.error != previous_dev.error ||
            dev.contact_count != previous_dev.contact_count ||
            dev.selected_contact != previous_dev.selected_contact ||
            dev.selected_online != previous_dev.selected_online ||
            dev.contacts_revision != previous_dev.contacts_revision ||
            dev.incoming_generation != previous_dev.incoming_generation ||
            dev.session_sequence != previous_dev.session_sequence ||
            dev.completed_sequence != previous_dev.completed_sequence ||
            dev.rx_dropped != previous_dev.rx_dropped ||
            dev.tx_dropped != previous_dev.tx_dropped ||
            strcmp(dev.display_name, previous_dev.display_name) != 0)
        {
            previous_dev = dev;
            if (s_page == UI_PAGE_HOME || s_page == UI_PAGE_DEV_CHAT)
            {
                s_dirty = true;
            }
        }
        if (s_page == UI_PAGE_DEV_CHAT)
        {
            bool has_session =
                dev.state == DEMO_DEV_CHAT_RINGING ||
                dev.state == DEMO_DEV_CHAT_DIALING ||
                dev.state == DEMO_DEV_CHAT_CONNECTING ||
                dev.state == DEMO_DEV_CHAT_WAIT_CONFIRM ||
                dev.state == DEMO_DEV_CHAT_IN_CALL ||
                dev.state == DEMO_DEV_CHAT_STOPPING;

            if (has_session && s_dev_page_session == 0U)
            {
                s_dev_page_session = dev.session_sequence;
            }
            if (!has_session && s_dev_page_session != 0U &&
                dev.completed_sequence == s_dev_page_session)
            {
                liot_trace("[UI] DEV session=%u complete -> MENU\r\n",
                           (unsigned int)s_dev_page_session);
                s_dev_page_session = 0U;
                s_page = UI_PAGE_HOME;
                ui_sync_live_policy();
                s_dirty = true;
            }
        }
#endif

        ui_update_startup_state();
        ui_sync_live_policy();

        if (s_dirty)
        {
            ui_render();
        }
    }
}
