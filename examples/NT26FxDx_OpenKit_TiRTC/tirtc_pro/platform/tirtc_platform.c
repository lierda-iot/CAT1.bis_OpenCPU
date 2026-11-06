#include "tirtc_platform.h"
#include "platform_internal.h"
#include "formal_mqtt.h"
#include "../network/tirtc_network.h"
#include "../status/tirtc_status.h"
#include "liot_rtc.h"
#include "liot_log.h"
#include "../runtime/tirtc_runtime.h"
#include <stdio.h>
#include <string.h>

static liot_task_t s_platform_task;
static bool s_platform_starting;
static bool s_platform_start_reported;
static int s_platform_start_error;
static int s_event_state = -1, s_event_error;

LiotOSStatus_t tirtc_platform_sem_delete_idle(liot_sem_t semaphore)
{
    LiotOSStatus_t result;
    if (!semaphore) return LIOT_OSI_SUCCESS;
    /* Only callback-detached objects with no waiter are passed here. This
     * gives F6D_A's delete wrapper the token it first waits for forever. */
    result = liot_rtos_semaphore_release(semaphore);
    if (result != LIOT_OSI_SUCCESS && result != LIOT_OSI_SEMA_IS_FULL) return result;
    return liot_rtos_semaphore_delete(semaphore);
}

bool tirtc_platform_utc_seconds(uint32_t *seconds)
{
    static const unsigned char lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    liot_rtc_time_s time;
    uint32_t days = 0, max_day;
    int year, month;
    if (!seconds) return false;
    *seconds = 0; memset(&time, 0, sizeof(time));
    if (liot_rtc_get_time(&time) != LIOT_RTC_SUCCESS ||
        time.tm_year < 2024 || time.tm_year > 2099 || time.tm_mon < 1 || time.tm_mon > 12 ||
        time.tm_hour < 0 || time.tm_hour > 23 || time.tm_min < 0 || time.tm_min > 59 ||
        time.tm_sec < 0 || time.tm_sec > 59) return false;
    max_day = lengths[time.tm_mon - 1];
    if (time.tm_mon == 2 && !(time.tm_year % 4)) max_day++;
    if (time.tm_mday < 1 || (uint32_t)time.tm_mday > max_day) return false;
    for (year = 1970; year < time.tm_year; year++)
        days += 365U + (!(year % 4) && (year % 100 || !(year % 400)) ? 1U : 0U);
    for (month = 1; month < time.tm_mon; month++)
        days += lengths[month - 1] + (month == 2 && !(time.tm_year % 4) ? 1U : 0U);
    days += (uint32_t)time.tm_mday - 1U;
    *seconds = days * 86400U + (uint32_t)time.tm_hour * 3600U +
               (uint32_t)time.tm_min * 60U + (uint32_t)time.tm_sec;
    return true;
}

static const char *phase_message(demo_binding_state_e state, int error)
{
    if (error == -131) return "当前云端 SDK 不支持此 SIM 卡槽";
    if (error == -130) return "4G 连接已变化，等待重新连接";
    if (error == -115) return "等待网络校时后验证设备身份";
    if (error == -111) return "设备身份保存失败，请重试";
    if (error == -109) return "绑定码已过期，正在重新获取";
    switch (state) {
    case DEMO_BIND_IDLE: return "准备连接小钛平台";
    case DEMO_BIND_WAIT_NETWORK: return "等待 4G 联网和网络时间";
    case DEMO_BIND_DISCOVERING: return "正在获取平台服务地址";
    case DEMO_BIND_REPORTING: return "正在申请设备绑定码";
    case DEMO_BIND_WAIT_GRANT: return "在小钛平台“我的设备”输入绑定码";
    case DEMO_BIND_VERIFYING: return "正在验证设备身份";
    case DEMO_BIND_BOUND: return "设备已绑定，平台连接正常";
    case DEMO_BIND_RESTARTING: return "设备已解绑，正在重新绑定";
    default: return "平台连接失败，将自动重试";
    }
}

void tirtc_platform_binding_event(int state, int error)
{
    char message[72];
    if (state == s_event_state && error == s_event_error) return;
    s_event_state = state; s_event_error = error;
    (void)snprintf(message, sizeof(message), "平台阶段 %d / 结果 %d", state, error);
    tirtc_status_event(message);
}

int tirtc_platform_start(void)
{
    liot_task_t task = NULL;
    int result;
    bool report;
    liot_rtos_enter_critical();
    if (s_platform_task || s_platform_starting) { liot_rtos_exit_critical(); return 0; }
    s_platform_starting = true;
    liot_rtos_exit_critical();
    result = (int)liot_rtos_task_create(&task, 16U * 1024U, 8, "tirtc_platform", demo_binding_task, NULL);
    if (!result && !task) result = -1;
    liot_rtos_enter_critical();
    report = !s_platform_start_reported || s_platform_start_error != result;
    s_platform_start_reported = true;
    s_platform_starting = false; s_platform_start_error = result;
    if (!result) s_platform_task = task;
    liot_rtos_exit_critical();
    if (report) tirtc_status_event(result ? "平台任务创建失败" : "平台任务已启动");
    return result ? -1 : 0;
}

int tirtc_platform_action(const tirtc_ui_action_t *action)
{
    demo_binding_snapshot_t state;
    bool started;
    if (!action || (action->type != TIRTC_ACTION_BIND && action->type != TIRTC_ACTION_NETWORK_REFRESH &&
                    action->type != TIRTC_ACTION_NETWORK_RECONNECT)) return -1;
    liot_rtos_enter_critical(); started = s_platform_task != NULL; liot_rtos_exit_critical();
    if (!started) return -1;
    demo_binding_get_snapshot(&state);
    if (state.state != DEMO_BIND_IDLE && state.state != DEMO_BIND_ERROR)
        return action->type == TIRTC_ACTION_BIND ? -1 : 0;
    demo_binding_request();
    return 0;
}

void tirtc_platform_publish(void)
{
    tirtc_ui_platform_t ui;
    demo_binding_snapshot_t state;
    tirtc_network_snapshot_t network;
    bool started;
    int start_error;
    uint32_t now, elapsed;
    memset(&ui, 0, sizeof(ui));
    demo_binding_get_snapshot(&state); tirtc_network_get_snapshot(&network);
    now = liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    started = s_platform_task != NULL || s_platform_starting;
    start_error = s_platform_start_error;
    liot_rtos_exit_critical();
    ui.known = true; ui.enabled = started;
    ui.binding_required = state.binding_required;
    ui.binding_state = TIRTC_BINDING_CHECKING;
    ui.error = state.error;
    ui.busy = state.state != DEMO_BIND_BOUND && state.state != DEMO_BIND_ERROR;
    memcpy(ui.device_id, state.device_id, sizeof(ui.device_id));
    (void)snprintf(ui.message, sizeof(ui.message), "%s", phase_message(state.state, state.error));
    if (!started) {
        ui.binding_state = start_error ? TIRTC_BINDING_ERROR : TIRTC_BINDING_DISABLED;
        ui.error = start_error; ui.busy = false;
        (void)snprintf(ui.message, sizeof(ui.message), "%s", start_error ? "平台任务启动失败" : "平台任务尚未启动");
    } else if (!network.ready || !network.time_valid || network.generation != state.generation) {
        (void)snprintf(ui.message, sizeof(ui.message), "%s", !network.ready ? "等待 4G 数据连接" :
                       (!network.time_valid ? "等待网络校时" : "正在更新平台连接"));
        ui.busy = true;
    } else {
        ui.api_ready = state.api_ready;
        ui.mqtt_connected = demo_formal_mqtt_is_online();
        if (state.state == DEMO_BIND_BOUND && ui.mqtt_connected) {
            ui.binding_state = TIRTC_BINDING_BOUND; ui.busy = false;
        } else if (state.state == DEMO_BIND_BOUND) {
            ui.busy = true;
            (void)snprintf(ui.message, sizeof(ui.message), "消息连接已断开，正在恢复");
        } else if (state.state == DEMO_BIND_WAIT_GRANT) {
            elapsed = (uint32_t)(now - state.sample_ms) / 1000U;
            ui.seconds_left = elapsed < state.seconds_left ? (uint16_t)(state.seconds_left - elapsed) : 0;
            if (ui.seconds_left) {
                ui.binding_state = TIRTC_BINDING_WAITING_USER;
                memcpy(ui.code, state.code, sizeof(state.code));
            }
        } else if (state.state == DEMO_BIND_ERROR) {
            ui.binding_state = TIRTC_BINDING_ERROR; ui.busy = false;
        }
    }
    ui.rtc_enabled = true;
    ui.rtc_ready = ui.api_ready && ui.mqtt_connected && demo_tirtc_is_ready();
    (void)tirtc_ui_publish_platform(&ui);
}

int tirtc_platform_get_identity(demo_binding_tirtc_identity_t *identity)
{ return demo_binding_get_tirtc_identity(identity); }
