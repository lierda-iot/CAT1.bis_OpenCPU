#ifndef APP_DISPLAY_PORT_LVGL_INTERNAL_H
#define APP_DISPLAY_PORT_LVGL_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_display_port_lvgl.h"
#include "app_osal.h"

#ifndef APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#define APP_DISPLAY_LVGL_ENABLE_LIOT_LCD 1
#endif

#ifndef APP_DISPLAY_LVGL_ENABLE_GIF
#define APP_DISPLAY_LVGL_ENABLE_GIF 0
#endif

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#include "lvgl.h"
#endif

#define APP_DISPLAY_LVGL_DEFAULT_WIDTH 240U
#define APP_DISPLAY_LVGL_DEFAULT_HEIGHT 320U
#define APP_DISPLAY_LVGL_DEFAULT_STACK_BYTES (10U * 1024U)
#define APP_DISPLAY_LVGL_DEFAULT_TASK_PRIORITY 12U
#define APP_DISPLAY_LVGL_DEFAULT_TICK_MS 1U
#define APP_DISPLAY_LVGL_DEFAULT_HANDLER_MS 10U
#define APP_DISPLAY_LVGL_DEFAULT_QUEUE_DEPTH 10U
#define APP_DISPLAY_LVGL_DEFAULT_DRAW_BUF_ROWS 20U
#define APP_DISPLAY_LVGL_DEFAULT_LSPI_SPEED_HZ (51U * 1024U * 1024U)
#define APP_DISPLAY_LVGL_DEFAULT_LSPI_NUM 2
#define APP_DISPLAY_LVGL_DEFAULT_LSPI_CS 52
#define APP_DISPLAY_LVGL_DEFAULT_BACKLIGHT_PIN 103
#define APP_DISPLAY_LVGL_DEFAULT_RST_PIN 78
#define APP_DISPLAY_LVGL_DEFAULT_RST_DELAY_MS 100U
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_I2C_NUM 1
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_I2C_ADDR 0x38U
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_SDA_PIN 66
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_SCL_PIN 57
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_SDA_FUNC 2
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_SCL_FUNC 3
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_RST_PIN 28
#define APP_DISPLAY_LVGL_DEFAULT_TOUCH_INT_PIN 6
#define APP_DISPLAY_LVGL_TEXT_MAX 96U
#define APP_DISPLAY_LVGL_CHAT_LINE_MAX 128U
#define APP_DISPLAY_LVGL_TOUCH_POINT_MAX 5U
#define APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT 30
#define APP_DISPLAY_LVGL_GRID_MARGIN_X 12
#define APP_DISPLAY_LVGL_GRID_TOP 44
#define APP_DISPLAY_LVGL_GRID_GAP 8
#define APP_DISPLAY_LVGL_GRID_COLUMNS 3U
#define APP_DISPLAY_LVGL_GRID_ROWS 3U
#define APP_DISPLAY_LVGL_GRID_BOTTOM_RESERVED 52
#define APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT 9U
#define APP_DISPLAY_LVGL_TOUCH_LOG_FIRST_COUNT 8U
#define APP_DISPLAY_LVGL_TOUCH_LOG_INTERVAL 30U
#define APP_DISPLAY_LVGL_TOUCH_LOG_MOVE_DELTA 12U
#define APP_DISPLAY_LVGL_BACK_SWIPE_MIN_DELTA 50U

typedef struct {
    app_display_lvgl_config_t config;
    app_display_caps_t active_caps;
    app_display_screen_t screen;
    app_emotion_t emotion;
    app_display_role_t chat_role;
    app_task_t task;
    app_queue_t queue;
    app_sem_t ready_sem;
    app_sem_t camera_frame_done_sem;
    app_timer_t tick_timer;
    bool configured;
    bool initialized;
    bool task_started;
    bool power_save;
    uint32_t notification_ticks;
    uint32_t camera_frame_count;
    int init_result;
    char status[APP_DISPLAY_LVGL_TEXT_MAX];
    char notification[APP_DISPLAY_LVGL_TEXT_MAX];
    char chat_text[APP_DISPLAY_LVGL_TEXT_MAX];
    app_display_player_status_t player_status;
    app_display_recorder_status_t recorder_status;
} app_display_lvgl_ctx_t;

extern app_display_lvgl_ctx_t s_display_lvgl;

void display_lvgl_back_swipe_reset(void);
void display_lvgl_input_emit_back_swipe_from_gesture(int dir);

int display_lvgl_ui_create(void);
void display_lvgl_ui_set_status(const char *text);
void display_lvgl_ui_set_emotion(app_emotion_t emotion);
void display_lvgl_ui_set_chat(app_display_role_t role, const char *text);
void display_lvgl_ui_set_screen(app_display_screen_t screen);
void display_lvgl_ui_set_notification(const char *text, uint32_t duration_ms);
void display_lvgl_ui_update_notification_timer(void);

bool display_lvgl_camera_frame_is_valid(const app_display_camera_frame_t *frame);
int display_lvgl_camera_present_frame(const app_display_camera_frame_t *frame);
void display_lvgl_camera_release_preview_buffer(void);

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
int display_lvgl_home_create(lv_obj_t *root,
                             lv_coord_t screen_width,
                             lv_coord_t screen_height);
void display_lvgl_home_set_visible(bool visible);
void display_lvgl_home_set_chat(app_display_role_t role, const char *text);
void display_lvgl_home_set_notification(const char *text);

int display_lvgl_camera_create(lv_obj_t *root,
                               lv_coord_t screen_width,
                               lv_coord_t screen_height);
void display_lvgl_camera_set_visible(bool visible);
void display_lvgl_camera_reset_preview(void);
void display_lvgl_camera_set_message(app_display_role_t role, const char *text);
int display_lvgl_gif_create(lv_obj_t *root,
                            lv_coord_t screen_width,
                            lv_coord_t screen_height);
int display_lvgl_gif_start(void);
void display_lvgl_gif_stop(void);
void display_lvgl_gif_set_visible(bool visible);
int display_lvgl_player_create(lv_obj_t *root,
                               lv_coord_t screen_width,
                               lv_coord_t screen_height);
void display_lvgl_player_set_visible(app_display_screen_t screen);
void display_lvgl_player_set_files(const app_display_player_file_t *files,
                                   uint32_t count);
void display_lvgl_player_set_status(const app_display_player_status_t *status);
int display_lvgl_recorder_create(lv_obj_t *root,
                                 lv_coord_t screen_width,
                                 lv_coord_t screen_height);
void display_lvgl_recorder_set_visible(bool visible);
void display_lvgl_recorder_set_status(const app_display_recorder_status_t *status);
	#endif

#endif /* APP_DISPLAY_PORT_LVGL_INTERNAL_H */
