#include "app_display_service.h"

#include <stddef.h>

#include "app_osal.h"

static app_display_caps_t s_caps;
static const app_display_driver_t *s_driver;
static bool s_initialized;
static app_display_action_cb_t s_action_cb;
static void *s_action_user;

static int display_noop_init(const app_display_caps_t *caps)
{
    (void)caps;
    return APP_OK;
}

static int display_noop_set_status(const char *text)
{
    app_log("display status: %s", (text != NULL) ? text : "");
    return APP_OK;
}

static int display_noop_notify(const char *text, uint32_t duration_ms)
{
    app_log("display notify: %s (%lu ms)",
            (text != NULL) ? text : "",
            (unsigned long)duration_ms);
    return APP_OK;
}

static int display_noop_set_emotion(app_emotion_t emotion)
{
    app_log("display emotion: %s", app_display_emotion_name(emotion));
    return APP_OK;
}

static int display_noop_set_chat_message(app_display_role_t role, const char *text)
{
    app_log("display chat[%s]: %s",
            app_display_role_name(role),
            (text != NULL) ? text : "");
    return APP_OK;
}

static int display_noop_show_screen(app_display_screen_t screen)
{
    app_log("display screen: %d", (int)screen);
    return APP_OK;
}

static int display_noop_update_status_bar(void)
{
    return APP_OK;
}

static int display_noop_set_power_save(bool enable)
{
    app_log("display power_save: %d", enable ? 1 : 0);
    return APP_OK;
}

static int display_noop_present_camera_frame(const app_display_camera_frame_t *frame)
{
    if (frame == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    app_log("display camera frame: id=%lu %ux%u len=%lu format=%d",
            (unsigned long)frame->frame_id,
            (unsigned int)frame->width,
            (unsigned int)frame->height,
            (unsigned long)frame->len,
            (int)frame->format);
    return APP_OK;
}

static int display_noop_set_player_files(const app_display_player_file_t *files, uint32_t count)
{
    (void)files;
    app_log("display player files: count=%lu", (unsigned long)count);
    return APP_OK;
}

static int display_noop_set_player_status(const app_display_player_status_t *status)
{
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    app_log("display player status: %s %u%% %lu/%lu",
            status->name,
            (unsigned int)status->percent,
            (unsigned long)status->bytes_done,
            (unsigned long)status->size_bytes);
    return APP_OK;
}

static int display_noop_set_recorder_status(const app_display_recorder_status_t *status)
{
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    app_log("display recorder status: %s %lu bytes %lu ms level=%u rec=%d saving=%d done=%d reason=%d",
            status->name,
            (unsigned long)status->bytes_done,
            (unsigned long)status->duration_ms,
            (unsigned int)status->level,
            status->recording ? 1 : 0,
            status->saving ? 1 : 0,
            status->done ? 1 : 0,
            (int)status->stop_reason);
    return APP_OK;
}

static const app_display_driver_t s_noop_driver = {
    .init = display_noop_init,
    .set_status = display_noop_set_status,
    .notify = display_noop_notify,
    .set_emotion = display_noop_set_emotion,
    .set_chat_message = display_noop_set_chat_message,
    .show_screen = display_noop_show_screen,
    .update_status_bar = display_noop_update_status_bar,
    .set_power_save = display_noop_set_power_save,
    .present_camera_frame = display_noop_present_camera_frame,
    .set_player_files = display_noop_set_player_files,
    .set_player_status = display_noop_set_player_status,
    .set_recorder_status = display_noop_set_recorder_status,
};

int app_display_register_driver(const app_display_driver_t *driver)
{
    if (driver == NULL || driver->init == NULL) {
        app_log("display driver register rejected");
        return APP_ERR_INVALID_ARG;
    }
    s_driver = driver;
    app_log("display driver registered");
    return APP_OK;
}

int app_display_init(const app_display_caps_t *caps)
{
    int ret;

    if (caps != NULL) {
        s_caps = *caps;
    }
    if (s_driver == NULL) {
        s_driver = &s_noop_driver;
        app_log("display service using noop driver");
    }
    app_log("display service init: %ux%u touch=%d camera=%d",
            (unsigned int)s_caps.width,
            (unsigned int)s_caps.height,
            s_caps.has_touch ? 1 : 0,
            s_caps.has_camera_preview ? 1 : 0);
    s_initialized = true;
    ret = s_driver->init(&s_caps);
    if (ret != APP_OK) {
        s_initialized = false;
        app_log("display service init failed: %d", ret);
        return ret;
    }
    app_log("display service initialized");
    return APP_OK;
}

int app_display_set_status(const char *text)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->set_status != NULL) ? s_driver->set_status(text) : APP_ERR_NOT_SUPPORTED;
}

int app_display_notify(const char *text, uint32_t duration_ms)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->notify != NULL) ? s_driver->notify(text, duration_ms) : APP_ERR_NOT_SUPPORTED;
}

int app_display_set_emotion(app_emotion_t emotion)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->set_emotion != NULL) ? s_driver->set_emotion(emotion) : APP_ERR_NOT_SUPPORTED;
}

int app_display_set_chat_message(app_display_role_t role, const char *text)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->set_chat_message != NULL) ? s_driver->set_chat_message(role, text) : APP_ERR_NOT_SUPPORTED;
}

int app_display_show_screen(app_display_screen_t screen)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->show_screen != NULL) ? s_driver->show_screen(screen) : APP_ERR_NOT_SUPPORTED;
}

int app_display_update_status_bar(void)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->update_status_bar != NULL) ? s_driver->update_status_bar() : APP_ERR_NOT_SUPPORTED;
}

int app_display_set_power_save(bool enable)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    return (s_driver->set_power_save != NULL) ? s_driver->set_power_save(enable) : APP_ERR_NOT_SUPPORTED;
}

int app_display_present_camera_frame(const app_display_camera_frame_t *frame)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    if (frame == NULL || frame->data == NULL || frame->len == 0U ||
        frame->width == 0U || frame->height == 0U ||
        frame->bytes_per_pixel == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    return (s_driver->present_camera_frame != NULL) ?
           s_driver->present_camera_frame(frame) :
           APP_ERR_NOT_SUPPORTED;
}

int app_display_set_player_files(const app_display_player_file_t *files, uint32_t count)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    if (count > 0U && files == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    return (s_driver->set_player_files != NULL) ?
           s_driver->set_player_files(files, count) :
           APP_ERR_NOT_SUPPORTED;
}

int app_display_set_player_status(const app_display_player_status_t *status)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    return (s_driver->set_player_status != NULL) ?
           s_driver->set_player_status(status) :
           APP_ERR_NOT_SUPPORTED;
}

int app_display_set_recorder_status(const app_display_recorder_status_t *status)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    return (s_driver->set_recorder_status != NULL) ?
           s_driver->set_recorder_status(status) :
           APP_ERR_NOT_SUPPORTED;
}

int app_display_set_action_cb(app_display_action_cb_t cb, void *user)
{
    s_action_cb = cb;
    s_action_user = user;
    app_log("display action callback %s", (cb != NULL) ? "registered" : "cleared");
    return APP_OK;
}

int app_display_emit_action(app_display_action_t action)
{
    return app_display_emit_action_value(action, 0U);
}

int app_display_emit_action_value(app_display_action_t action, uint32_t value)
{
    app_display_action_event_t event;

    app_log("display action emitted: %s", app_display_action_name(action));
    event.action = action;
    event.value = value;
    if (s_action_cb != NULL) {
        s_action_cb(&event, s_action_user);
    }
    return APP_OK;
}

const char *app_display_emotion_name(app_emotion_t emotion)
{
    switch (emotion) {
    case APP_EMOTION_NEUTRAL:
        return "neutral";
    case APP_EMOTION_LISTENING:
        return "listening";
    case APP_EMOTION_THINKING:
        return "thinking";
    case APP_EMOTION_SPEAKING:
        return "speaking";
    case APP_EMOTION_ERROR:
        return "error";
    case APP_EMOTION_SLEEP:
        return "sleep";
    default:
        return "unknown";
    }
}

const char *app_display_action_name(app_display_action_t action)
{
    switch (action) {
    case APP_DISPLAY_ACTION_CAMERA_START:
        return "camera_start";
    case APP_DISPLAY_ACTION_CAMERA_STOP:
        return "camera_stop";
    case APP_DISPLAY_ACTION_CHAT:
        return "chat";
    case APP_DISPLAY_ACTION_SCAN:
        return "scan";
    case APP_DISPLAY_ACTION_PLAY_MP3:
        return "play_mp3";
    case APP_DISPLAY_ACTION_RECORD:
        return "record";
    case APP_DISPLAY_ACTION_RECORDER_TOGGLE:
        return "recorder_toggle";
    case APP_DISPLAY_ACTION_TTS:
        return "tts";
    case APP_DISPLAY_ACTION_MEDIA:
        return "media";
    case APP_DISPLAY_ACTION_GIF:
        return "gif";
    case APP_DISPLAY_ACTION_SETTINGS:
        return "settings";
    case APP_DISPLAY_ACTION_TOOLS:
        return "tools";
    case APP_DISPLAY_ACTION_OTA:
        return "ota";
    case APP_DISPLAY_ACTION_ABOUT:
        return "about";
    case APP_DISPLAY_ACTION_PLAYER_SELECT:
        return "player_select";
    case APP_DISPLAY_ACTION_BACK:
        return "back";
    case APP_DISPLAY_ACTION_NONE:
    default:
        return "none";
    }
}

const char *app_display_role_name(app_display_role_t role)
{
    switch (role) {
    case APP_DISPLAY_ROLE_SYSTEM:
        return "system";
    case APP_DISPLAY_ROLE_USER:
        return "user";
    case APP_DISPLAY_ROLE_ASSISTANT:
        return "assistant";
    default:
        return "unknown";
    }
}
