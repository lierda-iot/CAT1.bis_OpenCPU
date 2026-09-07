#include "watch_display.h"

#include "app_config.h"
#include "watch_action.h"

#include <stdio.h>
#include <string.h>

#include "app_display_service.h"
#include "app_osal.h"
#include "app_state.h"
#include "ui_ai_basic.h"
#include "ui_headless.h"

#if WATCH_AI_ENABLE_LVGL_DISPLAY_PORT
#include "app_display_port_lvgl.h"
#endif

void watch_display_show_unavailable(const char *name)
{
#if WATCH_AI_ENABLE_DISPLAY
    char message[48];
    const char *display_name = (name != NULL) ? name : "Feature";

    (void)snprintf(message, sizeof(message), "%s unavailable", display_name);
    (void)app_display_set_status(display_name);
    (void)app_display_notify(message, 1800U);
#else
    (void)name;
#endif
}

int watch_display_init(void)
{
#if WATCH_AI_ENABLE_DISPLAY
    app_display_caps_t caps;
    int ret;

    memset(&caps, 0, sizeof(caps));

#if WATCH_AI_ENABLE_LVGL_DISPLAY_PORT
    {
        app_display_lvgl_config_t display_config;

        ret = app_display_lvgl_get_default_config(&display_config);
        if (ret != APP_OK) {
            app_log("watch init display default config failed: %d", ret);
            return ret;
        }
        display_config.caps.has_camera_preview = WATCH_AI_ENABLE_CAMERA != 0;
        display_config.tick_ms = WATCH_AI_DISPLAY_LVGL_TICK_MS;
        display_config.handler_ms = WATCH_AI_DISPLAY_LVGL_HANDLER_MS;
        display_config.queue_depth = WATCH_AI_DISPLAY_LVGL_QUEUE_DEPTH;
        display_config.draw_buf_rows = WATCH_AI_DISPLAY_LVGL_DRAW_BUF_ROWS;
        caps = display_config.caps;
        app_log("watch init display: %ux%u touch=%d camera=%d",
                (unsigned int)caps.width,
                (unsigned int)caps.height,
                caps.has_touch ? 1 : 0,
                caps.has_camera_preview ? 1 : 0);
        app_log("watch init display: LVGL port setup");
        ret = app_display_lvgl_setup(&display_config);
        if (ret != APP_OK) {
            app_log("watch init display LVGL setup failed: %d", ret);
            return ret;
        }
        ret = app_display_lvgl_register();
        if (ret != APP_OK) {
            app_log("watch init display LVGL register failed: %d", ret);
            return ret;
        }
        app_log("watch init display: LVGL port registered");
    }
#else
    caps.has_screen = true;
    caps.has_status_bar = true;
    caps.has_emotion = true;
    caps.has_chat_text = true;
    caps.has_notification = true;
    caps.has_camera_preview = WATCH_AI_ENABLE_CAMERA != 0;
#endif

    app_log("watch init display: service init");
    ret = app_display_init(&caps);
    if (ret != APP_OK) {
        app_log("watch init display service failed: %d", ret);
        return ret;
    }
    ret = app_display_set_action_cb(watch_action_display_cb, NULL);
    if (ret != APP_OK) {
        app_log("watch init display action callback failed: %d", ret);
        return ret;
    }
    app_log("watch init display complete");
    return APP_OK;
#else
    app_log("watch init display: disabled");
    return APP_OK;
#endif
}

int watch_display_init_ui(void)
{
#if WATCH_AI_ENABLE_DISPLAY
    int ret = ui_ai_basic_init(NULL);

    if (ret != APP_OK) {
        app_log("watch init UI basic init failed: %d", ret);
        return ret;
    }
    ret = app_state_add_listener(ui_ai_basic_on_state_changed, NULL);
    if (ret != APP_OK) {
        app_log("watch init UI listener failed: %d", ret);
        return ret;
    }
    app_log("watch init UI complete");
    return APP_OK;
#else
    int ret = ui_headless_init();

    if (ret != APP_OK) {
        app_log("watch init UI headless init failed: %d", ret);
        return ret;
    }
    ret = app_state_add_listener(ui_headless_on_state_changed, NULL);
    if (ret != APP_OK) {
        app_log("watch init UI headless listener failed: %d", ret);
        return ret;
    }
    app_log("watch init UI headless complete");
    return APP_OK;
#endif
}
