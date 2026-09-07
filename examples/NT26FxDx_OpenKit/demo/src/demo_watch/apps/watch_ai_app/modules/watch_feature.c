#include "watch_feature.h"

#include "watch_camera.h"
#include "watch_gif.h"
#include "watch_player.h"
#include "watch_page.h"
#include "watch_recorder.h"
#include "watch_scan.h"
#include "watch_session.h"

#include "app_display_service.h"
#include "app_osal.h"
#include "app_state.h"

int watch_feature_start(app_display_action_t action)
{
    app_log("watch feature start: %s", app_display_action_name(action));
    switch (action) {
    case APP_DISPLAY_ACTION_CAMERA_START:
        return watch_camera_start_preview();
    case APP_DISPLAY_ACTION_CAMERA_STOP:
        return watch_camera_stop_preview();
    case APP_DISPLAY_ACTION_SCAN:
        return watch_scan_start();
    case APP_DISPLAY_ACTION_GIF:
        return watch_gif_start();
    case APP_DISPLAY_ACTION_PLAY_MP3:
        return watch_player_start();
    case APP_DISPLAY_ACTION_RECORD:
        return watch_recorder_start();
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}

int watch_feature_go_back(void)
{
    int ret;

    ret = watch_page_go_back();
    if (ret == APP_OK) {
        app_log("watch feature back: page handled");
        return APP_OK;
    }
    if (ret != APP_ERR_NOT_SUPPORTED) {
        return ret;
    }

    ret = watch_session_go_back();
    if (ret != APP_OK) {
        return ret;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch feature back: session still active");
        return APP_OK;
    }
    if (app_state_get() != APP_STATE_IDLE) {
        (void)watch_page_replace(WATCH_PAGE_HOME);
        ret = app_state_transition(APP_STATE_IDLE, NULL);
        app_log("watch feature back: state reset ret=%d", ret);
        return ret;
    }
    app_log("watch feature back: session handled");
    return APP_OK;
}
