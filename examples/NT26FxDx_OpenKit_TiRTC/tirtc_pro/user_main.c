#include "user_main.h"
#include "tirtc_port.h"
#include "tirtc_network.h"
#include "tirtc_ui.h"
#include "tirtc_resources.h"
#include "tirtc_status.h"
#include "tirtc_platform.h"
#include "tirtc_runtime.h"
#include "tirtc_ai.h"
#include "tirtc_contacts.h"
#include "tirtc_calls.h"
#include "tirtc_video.h"
#include "remote/tirtc_remote.h"
#include "group/tirtc_group.h"
#include "tirtc_preferences.h"
#include "tirtc_keys.h"
#include "assets.h"
#include "liot_log.h"
#include "tirtc_log.h"
#include "liot_os.h"
#include "liot_power.h"
#include <stdio.h>

enum app_service {
    APP_STATUS, APP_RESOURCES, APP_NETWORK, APP_PLATFORM, APP_RUNTIME,
    APP_CONTACTS, APP_VIDEO, APP_CALLS, APP_REMOTE, APP_AI, APP_GROUP,
    APP_SERVICE_COUNT
};
static int s_service_result[APP_SERVICE_COUNT];

static int app_start_service(enum app_service service, int result,
                             const char *name, bool report_change)
{
    if (report_change && result != s_service_result[service]) {
        char event[80];
        liot_trace("[app] service=%s startup %s ret=%d\r\n",
                   name, result ? "failed" : "recovered", result);
        (void)snprintf(event, sizeof(event), "后台服务 %s 启动%s (%d)",
                       name, result ? "失败" : "已恢复", result);
        tirtc_status_event(event);
    }
    s_service_result[service] = result;
    return result;
}

static void retry_background_services(void)
{
    /* One app owner calls these idempotent entries at 5 s intervals. Always
     * check runtime too: its task can return from creation and subsequently
     * fail semaphore/queue setup. No live service is stopped or recreated. */
    (void)app_start_service(APP_STATUS, tirtc_status_start(), "status", true);
    (void)app_start_service(APP_RESOURCES, tirtc_resources_start(), "resources", true);
    (void)app_start_service(APP_NETWORK, tirtc_network_start(), "network", true);
    (void)app_start_service(APP_PLATFORM, tirtc_platform_start(), "platform", true);
    (void)app_start_service(APP_RUNTIME, tirtc_runtime_start_service(), "runtime", true);
    (void)app_start_service(APP_CONTACTS, tirtc_contacts_start(), "contacts", true);
    (void)app_start_service(APP_VIDEO, tirtc_video_start_service(), "video", true);
    (void)app_start_service(APP_CALLS, tirtc_calls_start_service(), "calls", true);
    (void)app_start_service(APP_REMOTE, tirtc_remote_start_service(), "remote", true);
    (void)app_start_service(APP_AI, tirtc_ai_start_service(), "AI", true);
    (void)app_start_service(APP_GROUP, tirtc_group_start_service(), "group", true);
}

static uint32_t s_applied_audio_revision;
static liot_mutex_t s_audio_apply_mutex;
static void apply_audio_preferences_locked(void)
{
    tirtc_preferences_snapshot_t snapshot;
    /* App mutex -> preference RAM critical -> UI mailbox mutex. Backend
     * actions are called outside the UI mailbox lock. Never hold this app
     * mutex across NVM poll/read or codec work, and never wait in a critical. */
    tirtc_preferences_get_snapshot(&snapshot);
    if (snapshot.value_revision != s_applied_audio_revision) {
        const tirtc_preferences_t *p=&snapshot.value;
        tirtc_ai_set_audio_config(p->volume, p->mic_gain, p->speaker_enabled, p->mic_enabled);
        tirtc_calls_set_audio_config(p->volume, p->mic_gain, p->speaker_enabled, p->mic_enabled);
        tirtc_group_set_audio_config(p->volume, p->mic_gain, p->speaker_enabled, p->mic_enabled);
        (void)tirtc_ui_publish_audio_settings(p->volume, p->mic_gain, p->speaker_enabled, p->mic_enabled);
        s_applied_audio_revision=snapshot.value_revision;
    }
}
static void apply_audio_preferences(void)
{
    if (!s_audio_apply_mutex || liot_rtos_mutex_lock(s_audio_apply_mutex,LIOT_NO_WAIT)!=LIOT_OSI_SUCCESS) return;
    apply_audio_preferences_locked();
    (void)liot_rtos_mutex_unlock(s_audio_apply_mutex);
}

static int app_ui_action(const tirtc_ui_action_t *action, void *context)
{
    if (!action) return -1;
    if (action->type >= TIRTC_ACTION_ROOM_OPEN && action->type <= TIRTC_ACTION_ROOM_PTT)
        return tirtc_group_action(action);
    if (action->type == TIRTC_ACTION_REMOTE_END || action->type == TIRTC_ACTION_REMOTE_SET_MIC ||
        action->type == TIRTC_ACTION_REMOTE_SET_SPEAKER || action->type == TIRTC_ACTION_REMOTE_SET_CAMERA)
        return tirtc_remote_action(action);
    if ((action->type == TIRTC_ACTION_AI_START || action->type == TIRTC_ACTION_CALL_AUDIO ||
         action->type == TIRTC_ACTION_CALL_VIDEO) && tirtc_remote_has_session()) return -2;
    if (action->type == TIRTC_ACTION_SET_SPEAKER_VOLUME || action->type == TIRTC_ACTION_SET_MIC_GAIN ||
        action->type == TIRTC_ACTION_SET_SPEAKER_ENABLED || action->type == TIRTC_ACTION_SET_MIC_ENABLED) {
        tirtc_preferences_field_t field;
        bool level = action->type == TIRTC_ACTION_SET_SPEAKER_VOLUME || action->type == TIRTC_ACTION_SET_MIC_GAIN;
        if (action->value < 0 || action->value > (level ? 10 : 1)) return -1;
        switch (action->type) {
        case TIRTC_ACTION_SET_SPEAKER_VOLUME: field=TIRTC_PREFERENCES_VOLUME; break;
        case TIRTC_ACTION_SET_MIC_GAIN: field=TIRTC_PREFERENCES_MIC_GAIN; break;
        case TIRTC_ACTION_SET_SPEAKER_ENABLED: field=TIRTC_PREFERENCES_SPEAKER_ENABLED; break;
        case TIRTC_ACTION_SET_MIC_ENABLED: field=TIRTC_PREFERENCES_MIC_ENABLED; break;
        default: return -1;
        }
        uint32_t now=liot_rtos_get_running_time();
        if (!s_audio_apply_mutex || liot_rtos_mutex_lock(s_audio_apply_mutex,LIOT_NO_WAIT)!=LIOT_OSI_SUCCESS) return -3;
        int result = tirtc_preferences_update(field, action->value, now, NULL);
        if (!result) apply_audio_preferences_locked();
        (void)liot_rtos_mutex_unlock(s_audio_apply_mutex);
        return result;
    }
    /* These preferences affect presentation only; media actions remain explicit. */
    if (action->type == TIRTC_ACTION_SET_IDLE_EXPRESSION ||
        action->type == TIRTC_ACTION_SET_SLEEP_MINUTES ||
        action->type == TIRTC_ACTION_SET_ACK_VOICE) return 0;
    if (action->type == TIRTC_ACTION_NETWORK_REFRESH ||
        action->type == TIRTC_ACTION_NETWORK_RECONNECT) {
        int result = tirtc_network_action(action, context);
        (void)tirtc_platform_action(action);
        return result;
    }
    if (action->type == TIRTC_ACTION_REFRESH_CONTACTS) return tirtc_contacts_action(action);
    if (action->type == TIRTC_ACTION_CALL_AUDIO || action->type == TIRTC_ACTION_CALL_VIDEO ||
        action->type == TIRTC_ACTION_ACCEPT_CALL || action->type == TIRTC_ACTION_REJECT_CALL ||
        action->type == TIRTC_ACTION_HANGUP || action->type == TIRTC_ACTION_SET_VIDEO_ENABLED)
        return tirtc_calls_action(action);
    if (action->type == TIRTC_ACTION_AI_START) {
        int result;
        if (tirtc_calls_has_session()) return -2;
        /* Room exit may still be draining after navigating home. Commit the
         * AI intent atomically, then let both workers hand off the real lease. */
        demo_tirtc_connection_snapshot_t connection;
        liot_rtos_enter_critical();
        demo_tirtc_get_connection_snapshot(&connection);
        if (connection.owner == DEMO_TIRTC_OWNER_GROUP_ROOM) {
            result = (tirtc_remote_has_session() || tirtc_calls_has_session()) ?
                     -2 : tirtc_ai_action(action);
            if (!result) tirtc_group_suspend();
            liot_rtos_exit_critical();
            return result;
        }
        liot_rtos_exit_critical();
        /* Even an existing AI session can finish cleanup before this action
         * reaches its worker. Every START must reserve incoming admission. */
        if (!demo_tirtc_admission_try_enter()) return -2;
        result=(tirtc_remote_has_session() || tirtc_calls_has_session()) ?
               -2 : tirtc_ai_action(action);
        demo_tirtc_admission_leave();
        return result;
    }
    if (action->type == TIRTC_ACTION_AI_STOP) return tirtc_ai_action(action);
    if (action->type == TIRTC_ACTION_ENTER_PAGE || action->type == TIRTC_ACTION_RETURN_HOME) {
        (void)tirtc_remote_action(action);
        (void)tirtc_group_action(action);
        int contacts_result = tirtc_contacts_action(action);
        int ai_result = tirtc_ai_action(action);
        return contacts_result == 0 ? 0 : ai_result;
    }
    return tirtc_platform_action(action);
}
void user_main(void)
{
    bool ui_ready;
    tirtc_preferences_t preferences;
    tirtc_preferences_snapshot_t preferences_snapshot;
    uint32_t published_save_revision, data_tick, preferences_tick;
    uint32_t boot_preferences_sequence;
    bool boot_preferences_reported = false;
    bool boot_load_pending, previous_load_pending;
    int32_t reported_save_error = 0;
    int32_t reported_load_error = 0;
    UINT8 boot_reason = 0xff;
    int boot_reason_result = liot_get_powerup_reason(&boot_reason);
    uint32_t boot_report_tick = liot_rtos_get_running_time() - 30000U;
    liot_trace("==== TiRTC UI 45 / remote cleanup and camera first frame / V04 NT26F6D0 ====");
    /* Keep the original classification for this boot and repeat it after USB
     * enumeration. It is not an exception stack or proof of a root cause. */
    liot_trace("[BOOT45] powerup_reason=%u ret=%d panic=%u\r\n",
               (unsigned)boot_reason, boot_reason_result,
               (unsigned)(boot_reason_result == 0 && boot_reason == LIOT_PWRUP_PANIC));
    (void)tirtc_preferences_init();
    tirtc_preferences_get(&preferences);
    if (liot_rtos_mutex_create(&s_audio_apply_mutex)!=LIOT_OSI_SUCCESS) {
        s_audio_apply_mutex=NULL;
        liot_trace("[audio-prefs] apply mutex failed; controls unavailable until retry\r\n");
    }
    apply_audio_preferences();
    tirtc_preferences_get_snapshot(&preferences_snapshot);
    published_save_revision = preferences_snapshot.save_revision;
    boot_preferences_sequence = preferences_snapshot.sequence;
    boot_load_pending=previous_load_pending=preferences_snapshot.load_pending;
    tirtc_ui_set_backend(app_ui_action, NULL);
    ui_ready = tirtc_port_start() == 0;
    if (!ui_ready) {
        liot_trace("[tirtc] startup failed; UI was not started");
    } else {
        if (tirtc_keys_init() != 0) liot_trace("[keys14] KEY0/KEY1 initialization failed; touch volume remains available\r\n");
        liot_trace("[app13] display startup returned; starting data services");
        tirtc_status_event("界面启动完成");
        /* Show APP Flash and real heap immediately. No network/storage call
         * may delay the first resource snapshot. */
        tirtc_resources_publish();
        tirtc_status_publish();
        liot_trace("[app13] first RAM/APP/time snapshots published");
        if (app_start_service(APP_STATUS, tirtc_status_start(), "status", false) != 0) {
            tirtc_status_event("模组信息服务启动失败");
            liot_trace("[app13] identity worker failed; fast sampling continues");
        }
        if (app_start_service(APP_RESOURCES, tirtc_resources_start(), "resources", false) != 0)
            liot_trace("[app13] one or more storage workers failed; RAM sampling continues");
        if (tirtc_assets_start() != 0)
            liot_trace("[app13] external UI resource worker failed");
        if (app_start_service(APP_NETWORK, tirtc_network_start(), "network", false) != 0) {
            tirtc_ui_notify("4G 状态服务启动失败");
            liot_trace("[tirtc] cellular status task failed to start");
            tirtc_status_event("4G 状态服务启动失败");
        }
        if (app_start_service(APP_PLATFORM, tirtc_platform_start(), "platform", false) != 0) {
            tirtc_status_event("平台服务启动失败");
            tirtc_ui_notify("平台服务启动失败，请查看运行状态");
            liot_trace("[app13] platform worker failed to start");
        }
        tirtc_platform_publish();
        if (app_start_service(APP_RUNTIME, tirtc_runtime_start_service(), "runtime", false) != 0)
            tirtc_status_event("实时音频服务启动失败");
        if (app_start_service(APP_CONTACTS, tirtc_contacts_start(), "contacts", false) != 0)
            tirtc_status_event("联系人服务启动失败");
        if (app_start_service(APP_VIDEO, tirtc_video_start_service(), "video", false) != 0)
            tirtc_status_event("视频服务启动失败");
        if (app_start_service(APP_CALLS, tirtc_calls_start_service(), "calls", false) != 0)
            tirtc_status_event("通话服务启动失败");
        if (app_start_service(APP_REMOTE, tirtc_remote_start_service(), "remote", false) != 0)
            tirtc_status_event("远程监控服务启动失败");
        tirtc_contacts_publish();
        if (app_start_service(APP_AI, tirtc_ai_start_service(), "AI", false) != 0)
            tirtc_status_event("AI 对讲服务启动失败");
        if (app_start_service(APP_GROUP, tirtc_group_start_service(), "group", false) != 0)
            tirtc_status_event("多人对讲服务启动失败");
        liot_trace("[app13] periodic data loop entered: 5000 ms");
    }
    /* AI and RTC own their workers; tirtc_port owns the only LVGL task. */
    /* Bulk storage and resource loading have separate workers. This task
     * services the small preference NVM records every 500 ms and copies
     * published system snapshots every 5 s; the LVGL task never does NVM I/O. */
    data_tick = preferences_tick = liot_rtos_get_running_time();
    while (1) {
        liot_rtos_task_sleep_ms(20U);
        uint32_t now = liot_rtos_get_running_time();
        int volume_delta = tirtc_keys_poll(now);
        if (ui_ready && volume_delta) (void)tirtc_ui_request_volume_delta(volume_delta);
        if ((uint32_t)(now - preferences_tick) < 500U) continue;
        preferences_tick = now;
        /* Cache-only expiry: keep stale bars off screen even if the modem
         * worker is waiting. Run before potentially blocking preference I/O. */
        if (ui_ready) tirtc_network_publish();
        tirtc_preferences_poll(now);
        if (!s_audio_apply_mutex && liot_rtos_mutex_create(&s_audio_apply_mutex)!=LIOT_OSI_SUCCESS)
            s_audio_apply_mutex=NULL;
        apply_audio_preferences();
        tirtc_preferences_get_snapshot(&preferences_snapshot);
        if (ui_ready && preferences_snapshot.load_error != reported_load_error) {
            reported_load_error=preferences_snapshot.load_error;
            if (reported_load_error) (void)tirtc_ui_notify("音量设置读取失败，稍后重试");
        }
        if (ui_ready && previous_load_pending && !preferences_snapshot.load_pending)
            (void)tirtc_ui_notify("音量设置读取完成");
        previous_load_pending=preferences_snapshot.load_pending;
        if (ui_ready && preferences_snapshot.save_error != reported_save_error) {
            reported_save_error = preferences_snapshot.save_error;
            if (reported_save_error) (void)tirtc_ui_notify("音量已调整，保存失败，将重试");
        }
        if (ui_ready && preferences_snapshot.save_revision != published_save_revision && !preferences_snapshot.dirty) {
            published_save_revision = preferences_snapshot.save_revision;
            (void)tirtc_ui_notify("音量设置已保存");
        }
        if ((uint32_t)(now - data_tick) < 5000U) continue;
        data_tick = now;
        if ((uint32_t)(now - boot_report_tick) >= 30000U) {
            boot_report_tick = now;
            TIRTC_LOG_DEBUG("[BOOT45] uptime=%lu powerup_reason=%u ret=%d panic=%u\r\n",
                       (unsigned long)(now / 1000U), (unsigned)boot_reason,
                       boot_reason_result,
                       (unsigned)(boot_reason_result == 0 && boot_reason == LIOT_PWRUP_PANIC));
        }
        if (!boot_preferences_reported) {
            /* USB AT enumerates after very early startup prints. Repeat the
             * exact boot-loaded values once after enumeration for verification. */
            liot_trace("[audio-prefs] %s seq=%lu speaker=%u mic_gain=%u spk_enabled=%u mic_enabled=%u\r\n",
                boot_load_pending ? "BOOT_LOAD_PENDING" : boot_preferences_sequence ? "BOOT_RESTORE" : "BOOT_DEFAULTS",
                (unsigned long)boot_preferences_sequence, preferences.volume, preferences.mic_gain,
                (unsigned)preferences.speaker_enabled, (unsigned)preferences.mic_enabled);
            boot_preferences_reported = true;
        }
        if (ui_ready) {
            retry_background_services();
            /* A failed task allocation may be retried. The loader itself
             * owns transient I/O retries and never restarts terminal data errors. */
            if (!tirtc_assets_ready()) (void)tirtc_assets_start();
            tirtc_resources_publish();
            tirtc_status_publish();
            tirtc_platform_publish();
            tirtc_contacts_publish();
        }
    }
}
