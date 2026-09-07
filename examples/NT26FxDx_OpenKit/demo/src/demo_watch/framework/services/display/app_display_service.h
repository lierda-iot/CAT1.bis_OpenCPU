#ifndef APP_DISPLAY_SERVICE_H
#define APP_DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_DISPLAY_SCREEN_HOME = 0,
    APP_DISPLAY_SCREEN_CHAT,
    APP_DISPLAY_SCREEN_CAMERA,
    APP_DISPLAY_SCREEN_GIF,
    APP_DISPLAY_SCREEN_PLAYER_LIST,
    APP_DISPLAY_SCREEN_PLAYER_NOW_PLAYING,
    APP_DISPLAY_SCREEN_RECORDER,
    APP_DISPLAY_SCREEN_ERROR,
} app_display_screen_t;

typedef enum {
    APP_DISPLAY_ACTION_NONE = 0,
    APP_DISPLAY_ACTION_CAMERA_START,
    APP_DISPLAY_ACTION_CAMERA_STOP,
    APP_DISPLAY_ACTION_CHAT,
    APP_DISPLAY_ACTION_SCAN,
    APP_DISPLAY_ACTION_PLAY_MP3,
    APP_DISPLAY_ACTION_RECORD,
    APP_DISPLAY_ACTION_RECORDER_TOGGLE,
    APP_DISPLAY_ACTION_TTS,
    APP_DISPLAY_ACTION_MEDIA,
    APP_DISPLAY_ACTION_GIF,
    APP_DISPLAY_ACTION_SETTINGS,
    APP_DISPLAY_ACTION_TOOLS,
    APP_DISPLAY_ACTION_OTA,
    APP_DISPLAY_ACTION_ABOUT,
    APP_DISPLAY_ACTION_PLAYER_SELECT,
    APP_DISPLAY_ACTION_BACK,
} app_display_action_t;

#define APP_DISPLAY_PLAYER_FILE_MAX 16U
#define APP_DISPLAY_PLAYER_NAME_MAX 64U

typedef enum {
    APP_DISPLAY_PLAYER_FILE_MP3 = 0,
    APP_DISPLAY_PLAYER_FILE_WAV,
    APP_DISPLAY_PLAYER_FILE_TTS,
} app_display_player_file_type_t;

typedef struct {
    char name[APP_DISPLAY_PLAYER_NAME_MAX];
    uint32_t size_bytes;
    app_display_player_file_type_t type;
} app_display_player_file_t;

typedef struct {
    char name[APP_DISPLAY_PLAYER_NAME_MAX];
    uint32_t size_bytes;
    uint32_t bytes_done;
    uint8_t percent;
    bool playing;
    bool done;
    app_display_player_file_type_t type;
} app_display_player_status_t;

typedef struct {
    char name[APP_DISPLAY_PLAYER_NAME_MAX];
    uint32_t bytes_done;
    uint32_t duration_ms;
    uint8_t level;
    bool recording;
    bool saving;
    bool done;
    app_recorder_stop_reason_t stop_reason;
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
} app_display_recorder_status_t;

typedef struct {
    app_display_action_t action;
    uint32_t value;
} app_display_action_event_t;

typedef enum {
    APP_DISPLAY_ROLE_SYSTEM = 0,
    APP_DISPLAY_ROLE_USER,
    APP_DISPLAY_ROLE_ASSISTANT,
} app_display_role_t;

typedef enum {
    APP_DISPLAY_FRAME_FORMAT_RGB565 = 0,
    APP_DISPLAY_FRAME_FORMAT_GRAY,
    APP_DISPLAY_FRAME_FORMAT_YUYV,
} app_display_frame_format_t;

typedef enum {
    APP_EMOTION_NEUTRAL = 0,
    APP_EMOTION_LISTENING,
    APP_EMOTION_THINKING,
    APP_EMOTION_SPEAKING,
    APP_EMOTION_ERROR,
    APP_EMOTION_SLEEP,
} app_emotion_t;

typedef struct {
    bool has_screen;
    bool has_status_bar;
    bool has_emotion;
    bool has_chat_text;
    bool has_notification;
    bool has_touch;
    bool has_camera_preview;
    uint16_t width;
    uint16_t height;
} app_display_caps_t;

typedef struct {
    uint32_t frame_id;
    const uint8_t *data;
    uint32_t len;
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
    app_display_frame_format_t format;
} app_display_camera_frame_t;

typedef struct {
    int (*init)(const app_display_caps_t *caps);
    int (*set_status)(const char *text);
    int (*notify)(const char *text, uint32_t duration_ms);
    int (*set_emotion)(app_emotion_t emotion);
    int (*set_chat_message)(app_display_role_t role, const char *text);
    int (*show_screen)(app_display_screen_t screen);
    int (*update_status_bar)(void);
    int (*set_power_save)(bool enable);
    int (*present_camera_frame)(const app_display_camera_frame_t *frame);
    int (*set_player_files)(const app_display_player_file_t *files, uint32_t count);
    int (*set_player_status)(const app_display_player_status_t *status);
    int (*set_recorder_status)(const app_display_recorder_status_t *status);
} app_display_driver_t;

typedef void (*app_display_action_cb_t)(const app_display_action_event_t *event, void *user);

int app_display_register_driver(const app_display_driver_t *driver);
int app_display_init(const app_display_caps_t *caps);
int app_display_set_status(const char *text);
int app_display_notify(const char *text, uint32_t duration_ms);
int app_display_set_emotion(app_emotion_t emotion);
int app_display_set_chat_message(app_display_role_t role, const char *text);
int app_display_show_screen(app_display_screen_t screen);
int app_display_update_status_bar(void);
int app_display_set_power_save(bool enable);
int app_display_present_camera_frame(const app_display_camera_frame_t *frame);
int app_display_set_player_files(const app_display_player_file_t *files, uint32_t count);
int app_display_set_player_status(const app_display_player_status_t *status);
int app_display_set_recorder_status(const app_display_recorder_status_t *status);
int app_display_set_action_cb(app_display_action_cb_t cb, void *user);
int app_display_emit_action(app_display_action_t action);
int app_display_emit_action_value(app_display_action_t action, uint32_t value);

const char *app_display_emotion_name(app_emotion_t emotion);
const char *app_display_role_name(app_display_role_t role);
const char *app_display_action_name(app_display_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_SERVICE_H */
