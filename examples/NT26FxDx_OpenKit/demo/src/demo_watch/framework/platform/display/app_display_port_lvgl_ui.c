#include "app_display_port_lvgl_internal.h"

#include <stdio.h>
#include <string.h>

#include "app_display_service.h"
#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
static lv_obj_t *s_root_screen;
static lv_obj_t *s_status_left_label;
static lv_obj_t *s_status_center_label;
static lv_obj_t *s_status_right_label;

static void display_lvgl_ui_copy_string(char *dst, uint32_t dst_size, const char *src)
{
    uint32_t i = 0U;

    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char *display_lvgl_status_indicator(app_emotion_t emotion)
{
    switch (emotion) {
    case APP_EMOTION_LISTENING:
        return "REC";
    case APP_EMOTION_THINKING:
    case APP_EMOTION_SPEAKING:
        return "AI";
    case APP_EMOTION_ERROR:
        return "ERR";
    case APP_EMOTION_SLEEP:
        return "ZZ";
    case APP_EMOTION_NEUTRAL:
    default:
        return "ON";
    }
}

static void display_lvgl_ui_update_content_visibility(void)
{
    bool show_camera = s_display_lvgl.screen == APP_DISPLAY_SCREEN_CAMERA;
    bool show_gif = s_display_lvgl.screen == APP_DISPLAY_SCREEN_GIF;
    bool show_player = s_display_lvgl.screen == APP_DISPLAY_SCREEN_PLAYER_LIST ||
                       s_display_lvgl.screen == APP_DISPLAY_SCREEN_PLAYER_NOW_PLAYING;
    bool show_recorder = s_display_lvgl.screen == APP_DISPLAY_SCREEN_RECORDER;

    display_lvgl_home_set_visible(!show_camera && !show_gif && !show_player && !show_recorder);
    display_lvgl_camera_set_visible(show_camera);
    display_lvgl_gif_set_visible(show_gif);
    display_lvgl_player_set_visible(s_display_lvgl.screen);
    display_lvgl_recorder_set_visible(show_recorder);
}

void display_lvgl_ui_set_status(const char *text)
{
    display_lvgl_ui_copy_string(s_display_lvgl.status, sizeof(s_display_lvgl.status), text);
    if (s_status_center_label != NULL) {
        lv_label_set_text(s_status_center_label, s_display_lvgl.status);
    }
}

void display_lvgl_ui_set_emotion(app_emotion_t emotion)
{
    s_display_lvgl.emotion = emotion;
    if (s_status_right_label != NULL) {
        lv_label_set_text(s_status_right_label, display_lvgl_status_indicator(emotion));
    }
}

void display_lvgl_ui_set_chat(app_display_role_t role, const char *text)
{
    s_display_lvgl.chat_role = role;
    display_lvgl_ui_copy_string(s_display_lvgl.chat_text, sizeof(s_display_lvgl.chat_text), text);
    display_lvgl_home_set_chat(role, s_display_lvgl.chat_text);
    display_lvgl_camera_set_message(role, s_display_lvgl.chat_text);
}

void display_lvgl_ui_set_screen(app_display_screen_t screen)
{
    app_display_screen_t next_screen = screen;
    int ret = APP_OK;

    display_lvgl_back_swipe_reset();
    if (screen == APP_DISPLAY_SCREEN_CAMERA) {
        s_display_lvgl.camera_frame_count = 0U;
        display_lvgl_camera_reset_preview();
    } else {
        display_lvgl_camera_release_preview_buffer();
    }
    if (screen == APP_DISPLAY_SCREEN_GIF) {
        ret = display_lvgl_gif_start();
        if (ret != APP_OK) {
            app_log("display gif start failed: %d, fallback home", ret);
            next_screen = APP_DISPLAY_SCREEN_HOME;
            display_lvgl_gif_stop();
        }
    } else {
        display_lvgl_gif_stop();
    }
    s_display_lvgl.screen = next_screen;
    display_lvgl_ui_update_content_visibility();
}

void display_lvgl_ui_set_notification(const char *text, uint32_t duration_ms)
{
    display_lvgl_ui_copy_string(s_display_lvgl.notification,
                                sizeof(s_display_lvgl.notification),
                                text);
    display_lvgl_home_set_notification(s_display_lvgl.notification);
    if (duration_ms == 0U) {
        s_display_lvgl.notification_ticks = 0U;
    } else {
        s_display_lvgl.notification_ticks =
            (duration_ms + s_display_lvgl.config.handler_ms - 1U) /
            s_display_lvgl.config.handler_ms;
    }
}

void display_lvgl_ui_update_notification_timer(void)
{
    if (s_display_lvgl.notification_ticks == 0U) {
        return;
    }

    s_display_lvgl.notification_ticks--;
    if (s_display_lvgl.notification_ticks == 0U) {
        display_lvgl_ui_copy_string(s_display_lvgl.notification,
                                    sizeof(s_display_lvgl.notification),
                                    "");
        display_lvgl_home_set_notification("");
    }
}

static int display_lvgl_ui_create_root_screen(void)
{
    s_root_screen = lv_obj_create(NULL);
    if (s_root_screen == NULL) {
        app_log("display lvgl UI root create failed");
        return APP_ERR_FAIL;
    }

    lv_obj_clear_flag(s_root_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_root_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_root_screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root_screen, LV_OPA_COVER, LV_PART_MAIN);
    return APP_OK;
}

static int display_lvgl_ui_create_status_bar(lv_coord_t screen_width)
{
    lv_obj_t *status_divider;

    s_status_left_label = lv_label_create(s_root_screen);
    s_status_center_label = lv_label_create(s_root_screen);
    s_status_right_label = lv_label_create(s_root_screen);
    status_divider = lv_obj_create(s_root_screen);
    if (s_status_left_label == NULL || s_status_center_label == NULL ||
        s_status_right_label == NULL || status_divider == NULL) {
        app_log("display lvgl UI label create failed");
        return APP_ERR_FAIL;
    }

    lv_label_set_text(s_status_left_label, "Watch AI");
    lv_obj_set_style_text_color(s_status_left_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_pos(s_status_left_label, 8, 7);

    lv_obj_set_width(s_status_center_label, 118);
    lv_label_set_long_mode(s_status_center_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_status_center_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_status_center_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(s_status_center_label, s_display_lvgl.status);
    lv_obj_set_pos(s_status_center_label, (screen_width - 118) / 2, 7);

    lv_obj_set_width(s_status_right_label, 44);
    lv_obj_set_style_text_color(s_status_right_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_status_right_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text(s_status_right_label, display_lvgl_status_indicator(s_display_lvgl.emotion));
    lv_obj_set_pos(s_status_right_label, screen_width - 52, 7);

    lv_obj_set_size(status_divider, screen_width, 1);
    lv_obj_set_pos(status_divider, 0, APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT - 1);
    lv_obj_clear_flag(status_divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(status_divider, lv_color_hex(0xD0D7DE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_divider, 0, LV_PART_MAIN);
    return APP_OK;
}

int display_lvgl_ui_create(void)
{
    lv_coord_t screen_width = (lv_coord_t)s_display_lvgl.active_caps.width;
    lv_coord_t screen_height = (lv_coord_t)s_display_lvgl.active_caps.height;
    int ret;

    app_log("display lvgl UI create: screen=%dx%d items=%u",
            (int)screen_width,
            (int)screen_height,
            (unsigned int)APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT);

    ret = display_lvgl_ui_create_root_screen();
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_home_create(s_root_screen, screen_width, screen_height);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_camera_create(s_root_screen, screen_width, screen_height);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_gif_create(s_root_screen, screen_width, screen_height);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_player_create(s_root_screen, screen_width, screen_height);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_recorder_create(s_root_screen, screen_width, screen_height);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_ui_create_status_bar(screen_width);
    if (ret != APP_OK) {
        return ret;
    }

    display_lvgl_ui_update_content_visibility();
    lv_scr_load(s_root_screen);
    app_log("display lvgl UI ready");
    return APP_OK;
}
#else
int display_lvgl_ui_create(void)
{
    return APP_ERR_NOT_SUPPORTED;
}

void display_lvgl_ui_set_status(const char *text)
{
    (void)text;
}

void display_lvgl_ui_set_emotion(app_emotion_t emotion)
{
    (void)emotion;
}

void display_lvgl_ui_set_chat(app_display_role_t role, const char *text)
{
    (void)role;
    (void)text;
}

void display_lvgl_ui_set_screen(app_display_screen_t screen)
{
    (void)screen;
}

void display_lvgl_ui_set_notification(const char *text, uint32_t duration_ms)
{
    (void)text;
    (void)duration_ms;
}

void display_lvgl_ui_update_notification_timer(void)
{
}
#endif
