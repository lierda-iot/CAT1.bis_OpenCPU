#include "app_config.h"
#include "app_main.h"

#include <string.h>

#include "app_board.h"
#include "app_core.h"
#include "app_osal.h"
#include "board_lierda_watch.h"
#include "watch_action.h"
#include "watch_audio.h"
#include "watch_camera.h"
#include "watch_display.h"
#include "watch_gif.h"
#include "watch_network.h"
#include "watch_protocol.h"
#include "watch_player.h"
#include "watch_recorder.h"
#include "watch_scan.h"

#if WATCH_AI_ENABLE_TOOL
#include "app_tool_registry.h"
#endif

static void watch_ai_app_handle_event(const app_event_t *event, void *user)
{
    (void)user;
    if (event == NULL) {
        return;
    }

    switch (event->id) {
    case APP_EV_BOOT_DONE: {
        int ret = watch_player_start_boot_tts();

        if (ret != APP_OK) {
            app_log("watch boot tts start failed: %d", ret);
        }
        break;
    }
    case APP_EV_DISPLAY_ACTION:
        (void)watch_action_handle((app_display_action_t)event->data.display_action.action,
                                  event->data.display_action.value);
        break;
    case APP_EV_SCAN_RESULT:
        watch_scan_flush_result();
        break;
    case APP_EV_RECORDER_DONE:
        app_log("watch recorder done event");
        break;
    default:
        break;
    }
}

int watch_ai_app_init(void)
{
    app_core_config_t core_config;
    int ret;

    app_log("watch init begin");
    memset(&core_config, 0, sizeof(core_config));
    core_config.product_name = WATCH_AI_PRODUCT_NAME;
    core_config.protocol_type = WATCH_AI_PROTOCOL_TYPE;

    app_log("watch init: core");
    ret = app_core_init(&core_config);
    if (ret != APP_OK) {
        app_log("watch init core failed: %d", ret);
        return ret;
    }
    ret = app_core_set_event_cb(watch_ai_app_handle_event, NULL);
    if (ret != APP_OK) {
        app_log("watch init core event cb failed: %d", ret);
        return ret;
    }
    app_log("watch init: core complete");

#if WATCH_AI_ENABLE_TOOL
    (void)app_tool_registry_init();
#endif

    app_log("watch init: board register");
    ret = app_board_lierda_watch_register();
    if (ret != APP_OK) {
        app_log("watch init board register failed: %d", ret);
        return ret;
    }
    app_log("watch init: board init");
    ret = app_board_init();
    if (ret != APP_OK) {
        app_log("watch init board init failed: %d", ret);
        return ret;
    }
    app_log("watch init: board complete");

    ret = watch_display_init();
    if (ret != APP_OK) {
        app_log("watch init display failed: %d", ret);
        return ret;
    }
    ret = watch_display_init_ui();
    if (ret != APP_OK) {
        app_log("watch init UI failed: %d", ret);
        return ret;
    }

    ret = watch_network_init();
    if (ret != APP_OK) {
        app_log("watch init network unavailable: %d", ret);
    }
    ret = watch_audio_init();
    if (ret != APP_OK) {
        app_log("watch init media unavailable: %d", ret);
    }
    ret = watch_player_init();
    if (ret != APP_OK) {
        app_log("watch init player unavailable: %d", ret);
    }
    ret = watch_recorder_init();
    if (ret != APP_OK) {
        app_log("watch init recorder unavailable: %d", ret);
    }
    ret = watch_camera_init();
    if (ret != APP_OK) {
        app_log("watch init camera unavailable: %d", ret);
    }
    ret = watch_scan_init();
    if (ret != APP_OK) {
        app_log("watch init scan unavailable: %d", ret);
    }
    ret = watch_gif_init();
    if (ret != APP_OK) {
        app_log("watch init gif unavailable: %d", ret);
    }
    ret = watch_protocol_init();
    if (ret != APP_OK) {
        app_log("watch init protocol unavailable: %d", ret);
    }
    ret = app_core_post_boot_done();
    if (ret != APP_OK) {
        app_log("watch init boot event post failed: %d", ret);
        return ret;
    }
    app_log("watch init complete");
    return APP_OK;
}

int watch_ai_app_go_back(void)
{
    return watch_action_go_back();
}

int watch_ai_app_run_once(uint32_t timeout_ms)
{
    return app_core_process_once(timeout_ms);
}
