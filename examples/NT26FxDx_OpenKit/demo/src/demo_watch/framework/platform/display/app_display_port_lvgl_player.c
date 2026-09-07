#include "app_display_port_lvgl_internal.h"

#include <stdio.h>
#include <string.h>

#include "app_display_service.h"
#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#define PLAYER_CONTENT_PAD_X 10
#define PLAYER_LIST_TOP 42
#define PLAYER_ITEM_HEIGHT 44
#define PLAYER_ITEM_GAP 6
#define PLAYER_NOW_TOP 48

static lv_obj_t *s_player_container;
static lv_obj_t *s_player_list_container;
static lv_obj_t *s_player_now_container;
static lv_obj_t *s_player_title_label;
static lv_obj_t *s_player_empty_label;
static lv_obj_t *s_player_buttons[APP_DISPLAY_PLAYER_FILE_MAX];
static lv_obj_t *s_player_button_labels[APP_DISPLAY_PLAYER_FILE_MAX];
static uint32_t s_player_button_indices[APP_DISPLAY_PLAYER_FILE_MAX];

static lv_obj_t *s_player_now_name_label;
static lv_obj_t *s_player_now_meta_label;
static lv_obj_t *s_player_progress_bar;
static lv_obj_t *s_player_progress_label;
static lv_obj_t *s_player_now_hint_label;
static uint32_t s_player_file_count;

static const char *display_lvgl_player_type_name(app_display_player_file_type_t type)
{
    switch (type) {
    case APP_DISPLAY_PLAYER_FILE_MP3:
        return "MP3";
    case APP_DISPLAY_PLAYER_FILE_WAV:
        return "WAV";
    case APP_DISPLAY_PLAYER_FILE_TTS:
        return "TTS";
    default:
        return "?";
    }
}

static void display_lvgl_player_item_cb(lv_event_t *event)
{
    const uint32_t *index;

    if (event == NULL || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    index = (const uint32_t *)lv_event_get_user_data(event);
    if (index == NULL) {
        return;
    }

    app_log("display player select: index=%lu", (unsigned long)*index);
    (void)app_display_emit_action_value(APP_DISPLAY_ACTION_PLAYER_SELECT, *index);
}

static void display_lvgl_player_set_hidden(lv_obj_t *obj, bool hidden)
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

static int display_lvgl_player_create_list(lv_coord_t screen_width,
                                           lv_coord_t screen_height)
{
    lv_coord_t list_h = screen_height - PLAYER_LIST_TOP - 8;
    lv_coord_t item_w = screen_width - (2 * PLAYER_CONTENT_PAD_X);
    uint32_t i;

    s_player_title_label = lv_label_create(s_player_container);
    s_player_empty_label = lv_label_create(s_player_container);
    s_player_list_container = lv_obj_create(s_player_container);
    if (s_player_title_label == NULL || s_player_empty_label == NULL ||
        s_player_list_container == NULL) {
        return APP_ERR_FAIL;
    }

    lv_label_set_text(s_player_title_label, "Audio Files");
    lv_obj_set_style_text_color(s_player_title_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_pos(s_player_title_label, PLAYER_CONTENT_PAD_X, 10);

    lv_label_set_text(s_player_empty_label, "No MP3/WAV files");
    lv_obj_set_width(s_player_empty_label, screen_width - 20);
    lv_obj_set_style_text_color(s_player_empty_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_player_empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_player_empty_label, 10, PLAYER_LIST_TOP + 40);

    lv_obj_set_size(s_player_list_container, screen_width, list_h);
    lv_obj_set_pos(s_player_list_container, 0, PLAYER_LIST_TOP);
    lv_obj_set_scroll_dir(s_player_list_container, LV_DIR_VER);
    lv_obj_set_style_pad_all(s_player_list_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_player_list_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_player_list_container, 0, LV_PART_MAIN);

    for (i = 0U; i < APP_DISPLAY_PLAYER_FILE_MAX; i++) {
        lv_coord_t y = (lv_coord_t)i * (PLAYER_ITEM_HEIGHT + PLAYER_ITEM_GAP);

        s_player_button_indices[i] = i;
        s_player_buttons[i] = lv_btn_create(s_player_list_container);
        if (s_player_buttons[i] == NULL) {
            return APP_ERR_FAIL;
        }
        lv_obj_set_size(s_player_buttons[i], item_w, PLAYER_ITEM_HEIGHT);
        lv_obj_set_pos(s_player_buttons[i], PLAYER_CONTENT_PAD_X, y);
        lv_obj_clear_flag(s_player_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s_player_buttons[i], 6, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_player_buttons[i], lv_color_hex(0xF6F8FA), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_player_buttons[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_player_buttons[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_player_buttons[i], lv_color_hex(0xD0D7DE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_player_buttons[i],
                                  lv_color_hex(0xDDF4FF),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_player_buttons[i],
                            display_lvgl_player_item_cb,
                            LV_EVENT_CLICKED,
                            &s_player_button_indices[i]);

        s_player_button_labels[i] = lv_label_create(s_player_buttons[i]);
        if (s_player_button_labels[i] == NULL) {
            return APP_ERR_FAIL;
        }
        lv_obj_set_width(s_player_button_labels[i], item_w - 12);
        lv_label_set_long_mode(s_player_button_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(s_player_button_labels[i],
                                    lv_color_hex(0x24292F),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_align(s_player_button_labels[i],
                                    LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN);
        lv_label_set_text(s_player_button_labels[i], "");
        lv_obj_center(s_player_button_labels[i]);
        lv_obj_add_flag(s_player_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
    return APP_OK;
}

static int display_lvgl_player_create_now(lv_coord_t screen_width,
                                          lv_coord_t screen_height)
{
    (void)screen_height;

    s_player_now_container = lv_obj_create(s_player_container);
    if (s_player_now_container == NULL) {
        return APP_ERR_FAIL;
    }
    lv_obj_set_size(s_player_now_container, screen_width, screen_height);
    lv_obj_set_pos(s_player_now_container, 0, 0);
    lv_obj_clear_flag(s_player_now_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_player_now_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_player_now_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_player_now_container, 0, LV_PART_MAIN);

    s_player_now_name_label = lv_label_create(s_player_now_container);
    s_player_now_meta_label = lv_label_create(s_player_now_container);
    s_player_progress_bar = lv_bar_create(s_player_now_container);
    s_player_progress_label = lv_label_create(s_player_now_container);
    s_player_now_hint_label = lv_label_create(s_player_now_container);
    if (s_player_now_name_label == NULL || s_player_now_meta_label == NULL ||
        s_player_progress_bar == NULL || s_player_progress_label == NULL ||
        s_player_now_hint_label == NULL) {
        return APP_ERR_FAIL;
    }

    lv_obj_set_width(s_player_now_name_label, screen_width - 24);
    lv_label_set_long_mode(s_player_now_name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_player_now_name_label, lv_color_hex(0x24292F), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_player_now_name_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_player_now_name_label, 12, PLAYER_NOW_TOP);

    lv_obj_set_width(s_player_now_meta_label, screen_width - 24);
    lv_label_set_long_mode(s_player_now_meta_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_player_now_meta_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_player_now_meta_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_player_now_meta_label, 12, PLAYER_NOW_TOP + 34);

    lv_obj_set_size(s_player_progress_bar, screen_width - 36, 12);
    lv_obj_set_pos(s_player_progress_bar, 18, PLAYER_NOW_TOP + 82);
    lv_bar_set_range(s_player_progress_bar, 0, 100);
    lv_bar_set_value(s_player_progress_bar, 0, LV_ANIM_OFF);

    lv_obj_set_width(s_player_progress_label, screen_width - 24);
    lv_obj_set_style_text_color(s_player_progress_label, lv_color_hex(0x0969DA), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_player_progress_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_player_progress_label, 12, PLAYER_NOW_TOP + 108);

    lv_obj_set_width(s_player_now_hint_label, screen_width - 24);
    lv_label_set_text(s_player_now_hint_label, "Playing");
    lv_obj_set_style_text_color(s_player_now_hint_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_player_now_hint_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_player_now_hint_label, 12, PLAYER_NOW_TOP + 150);
    return APP_OK;
}

int display_lvgl_player_create(lv_obj_t *root,
                               lv_coord_t screen_width,
                               lv_coord_t screen_height)
{
    int ret;

    if (root == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    s_player_container = lv_obj_create(root);
    if (s_player_container == NULL) {
        app_log("display player container create failed");
        return APP_ERR_FAIL;
    }
    lv_obj_set_size(s_player_container,
                    screen_width,
                    screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    lv_obj_set_pos(s_player_container, 0, APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    lv_obj_clear_flag(s_player_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_player_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_player_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_player_container, 0, LV_PART_MAIN);

    ret = display_lvgl_player_create_list(screen_width,
                                          screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    if (ret != APP_OK) {
        return ret;
    }
    ret = display_lvgl_player_create_now(screen_width,
                                         screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    if (ret != APP_OK) {
        return ret;
    }
    display_lvgl_player_set_visible(APP_DISPLAY_SCREEN_HOME);
    app_log("display player UI ready");
    return APP_OK;
}

void display_lvgl_player_set_visible(app_display_screen_t screen)
{
    bool show_list = screen == APP_DISPLAY_SCREEN_PLAYER_LIST;
    bool show_now = screen == APP_DISPLAY_SCREEN_PLAYER_NOW_PLAYING;

    display_lvgl_player_set_hidden(s_player_container, !(show_list || show_now));
    display_lvgl_player_set_hidden(s_player_title_label, !show_list);
    display_lvgl_player_set_hidden(s_player_empty_label, !show_list || s_player_file_count != 0U);
    display_lvgl_player_set_hidden(s_player_list_container, !show_list);
    display_lvgl_player_set_hidden(s_player_now_container, !show_now);
}

void display_lvgl_player_set_files(const app_display_player_file_t *files,
                                   uint32_t count)
{
    uint32_t i;

    if (count > APP_DISPLAY_PLAYER_FILE_MAX) {
        count = APP_DISPLAY_PLAYER_FILE_MAX;
    }
    s_player_file_count = count;
    for (i = 0U; i < APP_DISPLAY_PLAYER_FILE_MAX; i++) {
        if (i < count && files != NULL &&
            s_player_buttons[i] != NULL &&
            s_player_button_labels[i] != NULL) {
            char line[96];
            uint32_t kb = (files[i].size_bytes + 1023U) / 1024U;

            if (files[i].type == APP_DISPLAY_PLAYER_FILE_TTS) {
                (void)snprintf(line,
                               sizeof(line),
                               "%s  %s",
                               files[i].name,
                               display_lvgl_player_type_name(files[i].type));
            } else {
                (void)snprintf(line,
                               sizeof(line),
                               "%s  %s  %lu KB",
                               files[i].name,
                               display_lvgl_player_type_name(files[i].type),
                               (unsigned long)kb);
            }
            lv_label_set_text(s_player_button_labels[i], line);
            lv_obj_clear_flag(s_player_buttons[i], LV_OBJ_FLAG_HIDDEN);
        } else if (s_player_buttons[i] != NULL) {
            lv_obj_add_flag(s_player_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    display_lvgl_player_set_hidden(s_player_empty_label, count != 0U);
    app_log("display player files updated: count=%lu", (unsigned long)count);
}

void display_lvgl_player_set_status(const app_display_player_status_t *status)
{
    char meta[96];
    char progress[64];
    uint32_t kb;

    if (status == NULL) {
        return;
    }
    if (s_player_now_name_label == NULL || s_player_now_meta_label == NULL ||
        s_player_progress_bar == NULL || s_player_progress_label == NULL ||
        s_player_now_hint_label == NULL) {
        return;
    }
    s_display_lvgl.player_status = *status;
    kb = (status->size_bytes + 1023U) / 1024U;
    if (status->type == APP_DISPLAY_PLAYER_FILE_TTS) {
        meta[0] = '\0';
        (void)snprintf(progress,
                       sizeof(progress),
                       "%u%%",
                       (unsigned int)status->percent);
    } else {
        (void)snprintf(meta,
                       sizeof(meta),
                       "%s  %lu KB",
                       display_lvgl_player_type_name(status->type),
                       (unsigned long)kb);
        (void)snprintf(progress,
                       sizeof(progress),
                       "%u%%  %lu/%lu KB",
                       (unsigned int)status->percent,
                       (unsigned long)((status->bytes_done + 1023U) / 1024U),
                       (unsigned long)kb);
    }

    if (status->type == APP_DISPLAY_PLAYER_FILE_TTS) {
        lv_label_set_text(s_player_now_name_label, "TTS Play");
    } else {
        lv_label_set_text(s_player_now_name_label, status->name);
    }
    lv_label_set_text(s_player_now_meta_label, meta);
    lv_bar_set_value(s_player_progress_bar, status->percent, LV_ANIM_OFF);
    lv_label_set_text(s_player_progress_label, progress);
    if (status->done) {
        lv_label_set_text(s_player_now_hint_label, "Done");
    } else if (status->playing) {
        lv_label_set_text(s_player_now_hint_label, "Playing");
    } else {
        lv_label_set_text(s_player_now_hint_label, "Stopped");
    }
}
#endif
