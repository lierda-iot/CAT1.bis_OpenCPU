#include "app_event.h"

#include <stddef.h>

#include "app_osal.h"

#define APP_EVENT_QUEUE_DEPTH 16U

static app_queue_t s_event_queue;

int app_event_init(void)
{
    int ret;

    if (s_event_queue != NULL) {
        app_log("app_event queue already ready");
        return APP_OK;
    }
    ret = app_os_queue_create(&s_event_queue, APP_EVENT_QUEUE_DEPTH, sizeof(app_event_t));
    if (ret != APP_OK) {
        app_log("app_event queue create failed: depth=%u item=%u ret=%d",
                (unsigned int)APP_EVENT_QUEUE_DEPTH,
                (unsigned int)sizeof(app_event_t),
                ret);
        return ret;
    }
    app_log("app_event queue ready: depth=%u item=%u",
            (unsigned int)APP_EVENT_QUEUE_DEPTH,
            (unsigned int)sizeof(app_event_t));
    return APP_OK;
}

int app_event_post(const app_event_t *event)
{
    int ret;

    if (event == NULL || s_event_queue == NULL) {
        app_log("app_event post rejected");
        return APP_ERR_INVALID_ARG;
    }
    ret = app_os_queue_send(s_event_queue, event, sizeof(*event), APP_OS_NO_WAIT);
    if (ret != APP_OK) {
        app_log("app_event post failed: %s ret=%d", app_event_name(event->id), ret);
    }
    return ret;
}

int app_event_wait(app_event_t *event, uint32_t timeout_ms)
{
    if (event == NULL || s_event_queue == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    return app_os_queue_recv(s_event_queue, event, sizeof(*event), timeout_ms);
}

const char *app_event_name(app_event_id_t id)
{
    switch (id) {
    case APP_EV_NONE:
        return "none";
    case APP_EV_BOOT_DONE:
        return "boot_done";
    case APP_EV_NETWORK_CONNECTED:
        return "network_connected";
    case APP_EV_NETWORK_DISCONNECTED:
        return "network_disconnected";
    case APP_EV_PROTOCOL_CONNECTED:
        return "protocol_connected";
    case APP_EV_PROTOCOL_DISCONNECTED:
        return "protocol_disconnected";
    case APP_EV_CAMERA_START:
        return "camera_start";
    case APP_EV_CAMERA_STOP:
        return "camera_stop";
    case APP_EV_CAMERA_ERROR:
        return "camera_error";
    case APP_EV_SCAN_START:
        return "scan_start";
    case APP_EV_SCAN_STOP:
        return "scan_stop";
    case APP_EV_SCAN_RESULT:
        return "scan_result";
    case APP_EV_AUDIO_SEND_READY:
        return "audio_send_ready";
    case APP_EV_AUDIO_RECV:
        return "audio_recv";
    case APP_EV_WAKEUP:
        return "wakeup";
    case APP_EV_VAD_START:
        return "vad_start";
    case APP_EV_VAD_END:
        return "vad_end";
    case APP_EV_BUTTON_PRESS:
        return "button_press";
    case APP_EV_TOUCH_ACTION:
        return "touch_action";
    case APP_EV_DISPLAY_ACTION:
        return "display_action";
    case APP_EV_RECORDER_DONE:
        return "recorder_done";
    case APP_EV_OTA_PROGRESS:
        return "ota_progress";
    case APP_EV_ERROR:
        return "error";
    case APP_EV_SHUTDOWN:
        return "shutdown";
    default:
        return "unknown";
    }
}
