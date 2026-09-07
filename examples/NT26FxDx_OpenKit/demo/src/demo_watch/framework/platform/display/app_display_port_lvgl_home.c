#include "app_display_port_lvgl_internal.h"

#include <stdio.h>

#include "app_display_service.h"
#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
typedef struct {
    const char *label;
    app_display_action_t action;
} app_display_lvgl_launcher_item_t;

static lv_obj_t *s_home_container;
static lv_obj_t *s_detail_label;
static lv_obj_t *s_notification_label;
static lv_obj_t *s_launcher_buttons[APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT];
static lv_obj_t *s_launcher_button_labels[APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT];

static const app_display_lvgl_launcher_item_t s_launcher_items[APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT] = {
    {"Camera", APP_DISPLAY_ACTION_CAMERA_START},
    {"Scan", APP_DISPLAY_ACTION_SCAN},
    {"Player", APP_DISPLAY_ACTION_PLAY_MP3},
    {"Recorder", APP_DISPLAY_ACTION_RECORD},
    {"GIF", APP_DISPLAY_ACTION_GIF},
    {"Settings", APP_DISPLAY_ACTION_SETTINGS},
    {"Tools", APP_DISPLAY_ACTION_TOOLS},
    {"OTA", APP_DISPLAY_ACTION_OTA},
    {"About", APP_DISPLAY_ACTION_ABOUT},
};

static lv_coord_t display_lvgl_home_content_width(void)
{
    uint16_t width = s_display_lvgl.active_caps.width;

    if (width > 24U) {
        return (lv_coord_t)(width - 24U);
    }
    if (width != 0U) {
        return (lv_coord_t)width;
    }
    return (lv_coord_t)(APP_DISPLAY_LVGL_DEFAULT_WIDTH - 24U);
}

static lv_coord_t display_lvgl_home_grid_cell_size(void)
{
    int32_t width = s_display_lvgl.active_caps.width;
    int32_t height = s_display_lvgl.active_caps.height;
    int32_t available_width;
    int32_t available_height;
    int32_t cell_width;
    int32_t cell_height;

    available_width = width - (2 * APP_DISPLAY_LVGL_GRID_MARGIN_X) -
                      ((APP_DISPLAY_LVGL_GRID_COLUMNS - 1U) * APP_DISPLAY_LVGL_GRID_GAP);
    available_height = height - APP_DISPLAY_LVGL_GRID_TOP -
                       APP_DISPLAY_LVGL_GRID_BOTTOM_RESERVED -
                       ((APP_DISPLAY_LVGL_GRID_ROWS - 1U) * APP_DISPLAY_LVGL_GRID_GAP);
    if (available_width <= 0 || available_height <= 0) {
        return 1;
    }

    cell_width = available_width / APP_DISPLAY_LVGL_GRID_COLUMNS;
    cell_height = available_height / APP_DISPLAY_LVGL_GRID_ROWS;
    return (lv_coord_t)((cell_width < cell_height) ? cell_width : cell_height);
}

static void display_lvgl_launcher_button_cb(lv_event_t *event)
{
    const app_display_lvgl_launcher_item_t *item;

    if (event == NULL || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    item = (const app_display_lvgl_launcher_item_t *)lv_event_get_user_data(event);
    if (item == NULL) {
        return;
    }

    app_log("display launcher action: %s", app_display_action_name(item->action));
    (void)app_display_emit_action(item->action);
}

void display_lvgl_home_set_chat(app_display_role_t role, const char *text)
{
    char line[APP_DISPLAY_LVGL_CHAT_LINE_MAX];

    if (s_detail_label == NULL) {
        return;
    }
    (void)snprintf(line,
                   sizeof(line),
                   "%s: %s",
                   app_display_role_name(role),
                   (text != NULL) ? text : "");
    lv_label_set_text(s_detail_label, line);
}

void display_lvgl_home_set_notification(const char *text)
{
    if (s_notification_label != NULL) {
        lv_label_set_text(s_notification_label, (text != NULL) ? text : "");
    }
}

void display_lvgl_home_set_visible(bool visible)
{
    if (s_home_container == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_home_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_home_container, LV_OBJ_FLAG_HIDDEN);
    }
}

int display_lvgl_home_create(lv_obj_t *root,
                             lv_coord_t screen_width,
                             lv_coord_t screen_height)
{
    lv_coord_t content_width = display_lvgl_home_content_width();
    lv_coord_t cell_size = display_lvgl_home_grid_cell_size();
    lv_coord_t detail_y;
    lv_coord_t notification_y;
    uint32_t i;

    if (root == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    app_log("display lvgl home create: width=%d grid_cell=%d items=%u",
            (int)content_width,
            (int)cell_size,
            (unsigned int)APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT);
    if (cell_size < 24) {
        app_log("display lvgl UI grid rejected: cell=%d", (int)cell_size);
        return APP_ERR_INVALID_ARG;
    }

    s_home_container = lv_obj_create(root);
    if (s_home_container == NULL) {
        app_log("display lvgl UI home container create failed");
        return APP_ERR_FAIL;
    }
    lv_obj_set_size(s_home_container, screen_width, screen_height);
    lv_obj_set_pos(s_home_container, 0, 0);
    lv_obj_clear_flag(s_home_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_home_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_home_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_home_container, 0, LV_PART_MAIN);

    s_detail_label = lv_label_create(s_home_container);
    s_notification_label = lv_label_create(s_home_container);
    if (s_detail_label == NULL || s_notification_label == NULL) {
        app_log("display lvgl UI home label create failed");
        return APP_ERR_FAIL;
    }

    for (i = 0U; i < APP_DISPLAY_LVGL_LAUNCHER_ITEM_COUNT; i++) {
        lv_coord_t x = APP_DISPLAY_LVGL_GRID_MARGIN_X +
                       (lv_coord_t)(i % APP_DISPLAY_LVGL_GRID_COLUMNS) *
                       (cell_size + APP_DISPLAY_LVGL_GRID_GAP);
        lv_coord_t y = APP_DISPLAY_LVGL_GRID_TOP +
                       (lv_coord_t)(i / APP_DISPLAY_LVGL_GRID_COLUMNS) *
                       (cell_size + APP_DISPLAY_LVGL_GRID_GAP);

        s_launcher_buttons[i] = lv_btn_create(s_home_container);
        if (s_launcher_buttons[i] == NULL) {
            app_log("display lvgl UI launcher button create failed: index=%u",
                    (unsigned int)i);
            return APP_ERR_FAIL;
        }
        lv_obj_set_size(s_launcher_buttons[i], cell_size, cell_size);
        lv_obj_set_pos(s_launcher_buttons[i], x, y);
        lv_obj_clear_flag(s_launcher_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s_launcher_buttons[i], 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_launcher_buttons[i], 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_launcher_buttons[i], lv_color_hex(0xF6F8FA), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_launcher_buttons[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_launcher_buttons[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_launcher_buttons[i], lv_color_hex(0xD0D7DE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_launcher_buttons[i],
                                  lv_color_hex(0xDDF4FF),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_launcher_buttons[i],
                            display_lvgl_launcher_button_cb,
                            LV_EVENT_CLICKED,
                            (void *)&s_launcher_items[i]);

        s_launcher_button_labels[i] = lv_label_create(s_launcher_buttons[i]);
        if (s_launcher_button_labels[i] == NULL) {
            app_log("display lvgl UI launcher label create failed: index=%u",
                    (unsigned int)i);
            return APP_ERR_FAIL;
        }
        lv_obj_set_width(s_launcher_button_labels[i], cell_size - 8);
        lv_label_set_long_mode(s_launcher_button_labels[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(s_launcher_button_labels[i],
                                    lv_color_hex(0x24292F),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_align(s_launcher_button_labels[i],
                                    LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN);
        lv_label_set_text(s_launcher_button_labels[i], s_launcher_items[i].label);
        lv_obj_center(s_launcher_button_labels[i]);
    }

    detail_y = APP_DISPLAY_LVGL_GRID_TOP +
               (lv_coord_t)APP_DISPLAY_LVGL_GRID_ROWS * cell_size +
               (lv_coord_t)(APP_DISPLAY_LVGL_GRID_ROWS - 1U) * APP_DISPLAY_LVGL_GRID_GAP +
               7;
    notification_y = detail_y + 25;

    lv_obj_set_width(s_detail_label, content_width);
    lv_label_set_long_mode(s_detail_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_detail_label, lv_color_hex(0x57606A), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_detail_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    display_lvgl_home_set_chat(s_display_lvgl.chat_role, s_display_lvgl.chat_text);
    lv_obj_set_pos(s_detail_label, APP_DISPLAY_LVGL_GRID_MARGIN_X, detail_y);

    lv_obj_set_width(s_notification_label, content_width);
    lv_label_set_long_mode(s_notification_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_notification_label, lv_color_hex(0x0969DA), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_notification_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(s_notification_label, s_display_lvgl.notification);
    lv_obj_set_pos(s_notification_label, APP_DISPLAY_LVGL_GRID_MARGIN_X, notification_y);
    return APP_OK;
}
#endif
