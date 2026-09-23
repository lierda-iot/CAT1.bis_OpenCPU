#include "ui_ai_basic.h"

#include <stddef.h>

#include "app_display_service.h"
#include "app_osal.h"
#include "app_state.h"

static ui_ai_basic_config_t s_config;
static bool s_initialized;

static const ui_ai_basic_config_t s_default_config = {
    .idle_text = "IDLE",
    .listening_text = "LISTENING",
    .thinking_text = "THINKING",
    .speaking_text = "SPEAKING",
    .error_text = "ERROR",
};

static const char *ui_text_for_state(app_state_t state)
{
    switch (state) {
    case APP_STATE_IDLE:
        return s_config.idle_text;
    case APP_STATE_CAMERA:
        return "CAMERA";
    case APP_STATE_SCAN:
        return "SCAN";
    case APP_STATE_LISTENING:
        return s_config.listening_text;
    case APP_STATE_THINKING:
        return s_config.thinking_text;
    case APP_STATE_SPEAKING:
        return s_config.speaking_text;
    case APP_STATE_ERROR:
        return s_config.error_text;
    default:
        return app_state_name(state);
    }
}

static app_emotion_t ui_emotion_for_state(app_state_t state)
{
    switch (state) {
    case APP_STATE_LISTENING:
        return APP_EMOTION_LISTENING;
    case APP_STATE_THINKING:
    case APP_STATE_CONNECTING:
        return APP_EMOTION_THINKING;
    case APP_STATE_SPEAKING:
        return APP_EMOTION_SPEAKING;
    case APP_STATE_ERROR:
        return APP_EMOTION_ERROR;
    case APP_STATE_SLEEP:
        return APP_EMOTION_SLEEP;
    case APP_STATE_IDLE:
    case APP_STATE_CAMERA:
    case APP_STATE_SCAN:
    default:
        return APP_EMOTION_NEUTRAL;
    }
}

static app_display_screen_t ui_screen_for_state(app_state_t state)
{
    switch (state) {
    case APP_STATE_CAMERA:
    case APP_STATE_SCAN:
        return APP_DISPLAY_SCREEN_CAMERA;
    case APP_STATE_LISTENING:
    case APP_STATE_THINKING:
    case APP_STATE_SPEAKING:
        return APP_DISPLAY_SCREEN_CHAT;
    case APP_STATE_ERROR:
        return APP_DISPLAY_SCREEN_ERROR;
    default:
        return APP_DISPLAY_SCREEN_HOME;
    }
}

int ui_ai_basic_init(const ui_ai_basic_config_t *config)
{
    s_config = (config != NULL) ? *config : s_default_config;
    if (s_config.idle_text == NULL) {
        s_config.idle_text = s_default_config.idle_text;
    }
    if (s_config.listening_text == NULL) {
        s_config.listening_text = s_default_config.listening_text;
    }
    if (s_config.thinking_text == NULL) {
        s_config.thinking_text = s_default_config.thinking_text;
    }
    if (s_config.speaking_text == NULL) {
        s_config.speaking_text = s_default_config.speaking_text;
    }
    if (s_config.error_text == NULL) {
        s_config.error_text = s_default_config.error_text;
    }
    s_initialized = true;
    app_log("ui basic init: state=%s", app_state_name(app_state_get()));
    {
        int ret = ui_ai_basic_show_state(app_state_get());

        if (ret != APP_OK) {
            app_log("ui basic initial render failed: %d", ret);
            return ret;
        }
    }
    app_log("ui basic initialized");
    return APP_OK;
}

int ui_ai_basic_show_state(app_state_t state)
{
    int ret;

    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }

    ret = app_display_show_screen(ui_screen_for_state(state));
    if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED) {
        app_log("ui basic screen update failed: state=%s ret=%d",
                app_state_name(state),
                ret);
        return ret;
    }
    ret = app_display_set_emotion(ui_emotion_for_state(state));
    if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED) {
        app_log("ui basic emotion update failed: state=%s ret=%d",
                app_state_name(state),
                ret);
        return ret;
    }
    ret = app_display_set_status(ui_text_for_state(state));
    if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED) {
        app_log("ui basic status update failed: state=%s ret=%d",
                app_state_name(state),
                ret);
        return ret;
    }
    return APP_OK;
}

void ui_ai_basic_on_state_changed(app_state_t old_state,
                                  app_state_t new_state,
                                  const app_event_t *event,
                                  void *user)
{
    (void)old_state;
    (void)event;
    (void)user;
    (void)ui_ai_basic_show_state(new_state);
}
