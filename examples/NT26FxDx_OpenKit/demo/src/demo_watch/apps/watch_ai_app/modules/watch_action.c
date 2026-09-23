#include "watch_action.h"

#include "app_config.h"
#include "watch_display.h"
#include "watch_drawing.h"
#include "watch_feature.h"
#include "watch_player.h"
#include "watch_protocol.h"
#include "watch_recorder.h"

#include <string.h>

#include "app_display_service.h"
#include "app_event.h"
#include "app_osal.h"

static void watch_action_post_event(app_display_action_t action, uint32_t value)
{
    app_event_t event;
    int ret;

    memset(&event, 0, sizeof(event));
    event.id = APP_EV_DISPLAY_ACTION;
    event.data.display_action.action = (uint32_t)action;
    event.data.display_action.value = value;
    ret = app_event_post(&event);
    if (ret != APP_OK) {
        app_log("watch action post failed: %s ret=%d",
                app_display_action_name(action),
                ret);
    }
}

int watch_action_go_back(void)
{
    app_log("watch back requested");
    return watch_feature_go_back();
}

int watch_action_handle(app_display_action_t action, uint32_t value)
{
    int ret;

    app_log("watch display action: %s value=%lu",
            app_display_action_name(action),
            (unsigned long)value);
    switch (action) {
    case APP_DISPLAY_ACTION_BACK:
        return watch_action_go_back();
    case APP_DISPLAY_ACTION_CAMERA_START:
    case APP_DISPLAY_ACTION_CAMERA_STOP:
    case APP_DISPLAY_ACTION_SCAN:
    case APP_DISPLAY_ACTION_RECORD:
        ret = watch_feature_start(action);
        if (ret != APP_OK) {
            const char *name = "Camera";

            if (action == APP_DISPLAY_ACTION_SCAN) {
                name = "Scan";
            } else if (action == APP_DISPLAY_ACTION_RECORD) {
                name = "Recorder";
            }
            watch_display_show_unavailable(name);
        }
        return ret;
    case APP_DISPLAY_ACTION_CHAT:
        ret = watch_protocol_start();
        if (ret != APP_OK) {
            watch_display_show_unavailable("Chat");
        } else {
            (void)app_display_set_status("CHAT");
            (void)app_display_notify("Connecting", 1800U);
        }
        return ret;
    case APP_DISPLAY_ACTION_TTS:
        ret = watch_player_start_tts();
        if (ret != APP_OK) {
            watch_display_show_unavailable("TTS");
        }
        return ret;
    case APP_DISPLAY_ACTION_PLAY_MP3:
        ret = watch_feature_start(action);
        if (ret != APP_OK) {
            watch_display_show_unavailable("Player");
        }
        return ret;
    case APP_DISPLAY_ACTION_PLAYER_SELECT:
        ret = watch_player_select(value);
        if (ret != APP_OK) {
            watch_display_show_unavailable("Player");
        }
        return ret;
    case APP_DISPLAY_ACTION_RECORDER_TOGGLE:
        ret = watch_recorder_toggle();
        if (ret != APP_OK && ret != APP_ERR_TIMEOUT) {
            watch_display_show_unavailable("Recorder");
        }
        return ret;
    case APP_DISPLAY_ACTION_GIF:
        ret = watch_feature_start(action);
        if (ret != APP_OK) {
            watch_display_show_unavailable("GIF");
        }
        return ret;
    case APP_DISPLAY_ACTION_MEDIA:
        watch_display_show_unavailable("Media");
        return APP_ERR_NOT_SUPPORTED;
    case APP_DISPLAY_ACTION_DRAWING:
        ret = watch_drawing_start();
        if (ret != APP_OK) {
            watch_display_show_unavailable("Drawing");
        }
        return ret;
    case APP_DISPLAY_ACTION_TOOLS:
        watch_display_show_unavailable("Tools");
        return APP_ERR_NOT_SUPPORTED;
    case APP_DISPLAY_ACTION_OTA:
        watch_display_show_unavailable("OTA");
        return APP_ERR_NOT_SUPPORTED;
    case APP_DISPLAY_ACTION_ABOUT:
        watch_display_show_unavailable("About");
        return APP_ERR_NOT_SUPPORTED;
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}

void watch_action_display_cb(const app_display_action_event_t *event, void *user)
{
    (void)user;
    if (event == NULL) {
        return;
    }
    watch_action_post_event(event->action, event->value);
}
