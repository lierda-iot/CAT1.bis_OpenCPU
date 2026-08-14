#include "app_display_port_lvgl_internal.h"

#include <stdio.h>
#include <string.h>

#include "app_display_service.h"
#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#define RECORDER_CONTENT_PAD_X 12
#define RECORDER_TITLE_Y 12
#define RECORDER_TIME_Y 48
#define RECORDER_LEVEL_Y 90
#define RECORDER_NAME_Y 124
#define RECORDER_META_Y 152
#define RECORDER_BYTES_Y 180
#define RECORDER_BUTTON_Y 212
#define RECORDER_BUTTON_W 132
#define RECORDER_BUTTON_H 44
#define RECORDER_HINT_Y 266

static lv_obj_t *s_recorder_container;
static lv_obj_t *s_recorder_title_label;
static lv_obj_t *s_recorder_name_label;
static lv_obj_t *s_recorder_meta_label;
static lv_obj_t *s_recorder_level_bar;
static lv_obj_t *s_recorder_time_label;
static lv_obj_t *s_recorder_bytes_label;
static lv_obj_t *s_recorder_action_button;
static lv_obj_t *s_recorder_action_label;
static lv_obj_t *s_recorder_hint_label;
static app_display_recorder_status_t s_recorder_status;

static void display_lvgl_recorder_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void display_lvgl_recorder_format_time(char *dst,
                                              uint32_t dst_size,
                                              uint32_t duration_ms)
{
    uint32_t total_sec = duration_ms / 1000U;
    uint32_t min = total_sec / 60U;
    uint32_t sec = total_sec % 60U;
    uint32_t ms = duration_ms % 1000U;

    if (dst == NULL || dst_size == 0U) {
        return;
    }
    (void)snprintf(dst,
                   dst_size,
                   "%02lu:%02lu.%03lu",
                   (unsigned long)min,
                   (unsigned long)sec,
                   (unsigned long)ms);
}

static const char *display_lvgl_recorder_hint(const app_display_recorder_status_t *status)
{
    if (status == NULL) {
        return "";
    }
    if (status->stop_reason == APP_RECORDER_STOP_REASON_NO_SPACE) {
        return "Storage full";
    }
    if (status->stop_reason == APP_RECORDER_STOP_REASON_ERROR) {
        return "Save failed";
    }
    if (status->done) {
        return "Saved";
    }
    if (status->saving) {
        return "Saving...";
    }
    if (status->recording) {
        return "Recording";
    }
    return "Ready";
}

static void display_lvgl_recorder_button_cb(lv_event_t *event)
{
    bool was_recording;

    if (event == NULL || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_recorder_status.saving) {
        app_log("display recorder action ignored: saving");
        return;
    }

    was_recording = s_recorder_status.recording;
    if (was_recording) {
        app_display_recorder_status_t saving_status = s_recorder_status;

        saving_status.recording = false;
        saving_status.saving = true;
        saving_status.level = 0U;
        display_lvgl_recorder_set_status(&saving_status);
    }

    app_log("display recorder action: %s", was_recording ? "stop" : "start");
    (void)app_display_emit_action(APP_DISPLAY_ACTION_RECORDER_TOGGLE);
}

int display_lvgl_recorder_create(lv_obj_t *root,
                                 lv_coord_t screen_width,
                                 lv_coord_t screen_height)
{
    lv_coord_t content_height = screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT;
    lv_coord_t bar_w = screen_width - 36;

    if (root == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    s_recorder_container = lv_obj_create(root);
    if (s_recorder_container == NULL) {
        app_log("display recorder container create failed");
        return APP_ERR_FAIL;
    }
    lv_obj_set_size(s_recorder_container, screen_width, content_height);
    lv_obj_set_pos(s_recorder_container, 0, APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    lv_obj_clear_flag(s_recorder_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_recorder_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_recorder_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_recorder_container, 0, LV_PART_MAIN);

    s_recorder_title_label = lv_label_create(s_recorder_container);
    s_recorder_name_label = lv_label_create(s_recorder_container);
    s_recorder_meta_label = lv_label_create(s_recorder_container);
    s_recorder_level_bar = lv_bar_create(s_recorder_container);
    s_recorder_time_label = lv_label_create(s_recorder_container);
    s_recorder_bytes_label = lv_label_create(s_recorder_container);
    s_recorder_action_button = lv_btn_create(s_recorder_container);
    s_recorder_action_label = lv_label_create(s_recorder_action_button);
    s_recorder_hint_label = lv_label_create(s_recorder_container);
    if (s_recorder_title_label == NULL ||
        s_recorder_name_label == NULL ||
        s_recorder_meta_label == NULL ||
        s_recorder_level_bar == NULL ||
        s_recorder_time_label == NULL ||
        s_recorder_bytes_label == NULL ||
        s_recorder_action_button == NULL ||
        s_recorder_action_label == NULL ||
        s_recorder_hint_label == NULL) {
        return APP_ERR_FAIL;
    }

    lv_obj_set_width(s_recorder_title_label, screen_width - 24);
    lv_label_set_text(s_recorder_title_label, "Recorder");
    lv_obj_set_style_text_color(s_recorder_title_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_title_label, RECORDER_CONTENT_PAD_X, RECORDER_TITLE_Y);

    lv_obj_set_width(s_recorder_name_label, screen_width - 24);
    lv_label_set_long_mode(s_recorder_name_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_recorder_name_label, "");
    lv_obj_set_style_text_color(s_recorder_name_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_name_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_name_label, RECORDER_CONTENT_PAD_X, RECORDER_NAME_Y);

    lv_obj_set_width(s_recorder_meta_label, screen_width - 24);
    lv_label_set_long_mode(s_recorder_meta_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_recorder_meta_label, "");
    lv_obj_set_style_text_color(s_recorder_meta_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_meta_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_meta_label, RECORDER_CONTENT_PAD_X, RECORDER_META_Y);

    lv_obj_set_size(s_recorder_level_bar, bar_w, 18);
    lv_obj_set_pos(s_recorder_level_bar, (screen_width - bar_w) / 2, RECORDER_LEVEL_Y);
    lv_bar_set_range(s_recorder_level_bar, 0, 100);
    lv_bar_set_value(s_recorder_level_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_recorder_level_bar, lv_color_hex(0xF6F8FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_recorder_level_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_recorder_level_bar, lv_color_hex(0xFBBC04), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_recorder_level_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_recorder_level_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_recorder_level_bar, lv_color_hex(0xD0D7DE), LV_PART_MAIN);

    lv_obj_set_width(s_recorder_time_label, screen_width - 24);
    lv_label_set_text(s_recorder_time_label, "00:00.000");
    lv_obj_set_style_text_color(s_recorder_time_label, lv_color_hex(0x0969DA), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_time_label, RECORDER_CONTENT_PAD_X, RECORDER_TIME_Y);

    lv_obj_set_width(s_recorder_bytes_label, screen_width - 24);
    lv_label_set_text(s_recorder_bytes_label, "0 KB");
    lv_obj_set_style_text_color(s_recorder_bytes_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_bytes_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_bytes_label, RECORDER_CONTENT_PAD_X, RECORDER_BYTES_Y);

    lv_obj_set_size(s_recorder_action_button, RECORDER_BUTTON_W, RECORDER_BUTTON_H);
    lv_obj_set_pos(s_recorder_action_button,
                   (screen_width - RECORDER_BUTTON_W) / 2,
                   RECORDER_BUTTON_Y);
    lv_obj_clear_flag(s_recorder_action_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_recorder_action_button, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_recorder_action_button, lv_color_hex(0x0969DA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_recorder_action_button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_recorder_action_button,
                              lv_color_hex(0xDDF4FF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(s_recorder_action_button,
                              lv_color_hex(0xD0D7DE),
                              LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_recorder_action_button,
                        display_lvgl_recorder_button_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    lv_label_set_text(s_recorder_action_label, "Start");
    lv_obj_set_style_text_color(s_recorder_action_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_recorder_action_label);

    lv_obj_set_width(s_recorder_hint_label, screen_width - 24);
    lv_label_set_text(s_recorder_hint_label, "Ready");
    lv_obj_set_style_text_color(s_recorder_hint_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_recorder_hint_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_recorder_hint_label, RECORDER_CONTENT_PAD_X, RECORDER_HINT_Y);

    display_lvgl_recorder_set_visible(false);
    app_log("display recorder UI ready");
    return APP_OK;
}

void display_lvgl_recorder_set_visible(bool visible)
{
    display_lvgl_recorder_set_hidden(s_recorder_container, !visible);
}

void display_lvgl_recorder_set_status(const app_display_recorder_status_t *status)
{
    char time_text[32];
    char bytes_text[32];
    char meta_text[48];
    uint32_t kb;

    if (status == NULL || s_recorder_name_label == NULL ||
        s_recorder_meta_label == NULL || s_recorder_level_bar == NULL ||
        s_recorder_time_label == NULL ||
        s_recorder_bytes_label == NULL || s_recorder_action_button == NULL ||
        s_recorder_action_label == NULL || s_recorder_hint_label == NULL) {
        return;
    }

    s_recorder_status = *status;
    lv_label_set_text(s_recorder_name_label,
                      (status->name[0] != '\0') ? status->name : "Ready");
    (void)snprintf(meta_text,
                   sizeof(meta_text),
                   "%lu Hz  %u ch  %u bit",
                   (unsigned long)status->sample_rate_hz,
                   (unsigned int)status->channels,
                   (unsigned int)status->bits_per_sample);
    lv_label_set_text(s_recorder_meta_label, meta_text);

    lv_bar_set_value(s_recorder_level_bar, status->level, LV_ANIM_OFF);

    display_lvgl_recorder_format_time(time_text, sizeof(time_text), status->duration_ms);
    lv_label_set_text(s_recorder_time_label, time_text);

    kb = (status->bytes_done + 1023U) / 1024U;
    (void)snprintf(bytes_text,
                   sizeof(bytes_text),
                   "%lu KB",
                   (unsigned long)kb);
    lv_label_set_text(s_recorder_bytes_label, bytes_text);

    if (status->saving) {
        lv_obj_add_state(s_recorder_action_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_recorder_action_button, lv_color_hex(0xD0D7DE), LV_PART_MAIN);
        lv_label_set_text(s_recorder_action_label, "Saving");
        lv_obj_set_style_text_color(s_recorder_action_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    } else {
        lv_obj_clear_state(s_recorder_action_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_recorder_action_button,
                                  status->recording ? lv_color_hex(0xCF222E) : lv_color_hex(0x0969DA),
                                  LV_PART_MAIN);
        lv_label_set_text(s_recorder_action_label, status->recording ? "Stop" : "Start");
        lv_obj_set_style_text_color(s_recorder_action_label, lv_color_white(), LV_PART_MAIN);
    }
    lv_obj_center(s_recorder_action_label);

    lv_label_set_text(s_recorder_hint_label, display_lvgl_recorder_hint(status));
}
#else
int display_lvgl_recorder_create(lv_obj_t *root,
                                 lv_coord_t screen_width,
                                 lv_coord_t screen_height)
{
    (void)root;
    (void)screen_width;
    (void)screen_height;
    return APP_ERR_NOT_SUPPORTED;
}

void display_lvgl_recorder_set_visible(bool visible)
{
    (void)visible;
}

void display_lvgl_recorder_set_status(const app_display_recorder_status_t *status)
{
    (void)status;
}
#endif
