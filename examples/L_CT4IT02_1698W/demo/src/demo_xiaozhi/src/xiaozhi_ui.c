#include "xiaozhi_core.h"

#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "lvgl.h"
#include "liot_log.h"
#include "liot_os.h"

LV_IMG_DECLARE(g_xiaozhi_portrait_gif);

static lv_obj_t *g_xz_screen;
static lv_obj_t *g_xz_status;
static lv_obj_t *g_xz_animation;
static bool g_xz_ui_talking;
static char g_xz_pending_status[32];

static void xz_ui_status_async(void *argument)
{
    (void)argument;
    if (g_xz_status != NULL)
        lv_label_set_text(g_xz_status, g_xz_pending_status);
}

void xiaozhi_ui_status(const char *status)
{
    if (status == NULL) return;
    if (strcmp(status, "Connecting...") != 0 &&
        strcmp(status, "Hold to talk") != 0 &&
        strcmp(status, "Listening...") != 0 &&
        strcmp(status, "Thinking...") != 0 &&
        strcmp(status, "Speaking...") != 0 &&
        strcmp(status, "Reconnecting...") != 0)
        return;
    snprintf(g_xz_pending_status, sizeof(g_xz_pending_status), "%s", status);
    lv_async_call(xz_ui_status_async, NULL);
}

static void xz_ui_talk_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_LONG_PRESSED) {
        g_xz_ui_talking = xiaozhi_talk_set(true);
        if (g_xz_ui_talking) {
            lv_obj_add_state(g_xz_screen, LV_STATE_CHECKED);
            xiaozhi_ui_status("Listening...");
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_clear_state(g_xz_screen, LV_STATE_CHECKED);
        if (g_xz_ui_talking) {
            xiaozhi_ui_status("Thinking...");
            xiaozhi_talk_set(false);
            g_xz_ui_talking = false;
        }
    }
}

static void xz_ui_create(void *argument)
{
    lv_obj_t *screen;
    (void)argument;
    screen = lv_obj_create(NULL);
    g_xz_screen = screen;
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x172B34), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, xz_ui_talk_event, LV_EVENT_ALL, NULL);

    g_xz_animation = lv_gif_create(screen);
    lv_gif_set_src(g_xz_animation, &g_xiaozhi_portrait_gif);
    lv_obj_center(g_xz_animation);
    lv_obj_clear_flag(g_xz_animation, LV_OBJ_FLAG_CLICKABLE);

    g_xz_status = lv_label_create(screen);
    lv_label_set_text(g_xz_status, "Connecting...");
    lv_obj_set_width(g_xz_status, 180);
    lv_obj_set_style_text_align(g_xz_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_xz_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_xz_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(g_xz_status, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_xz_status, LV_OPA_60, 0);
    lv_obj_set_style_radius(g_xz_status, 6, 0);
    lv_obj_set_style_pad_ver(g_xz_status, 5, 0);
    lv_obj_align(g_xz_status, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_clear_flag(g_xz_status, LV_OBJ_FLAG_CLICKABLE);

    lv_scr_load(screen);
}
void xiaozhi_ui_init(void)
{
    lvgl_init();
    lv_async_call(xz_ui_create, NULL);
    liot_trace("[xiaozhi] UI initialized");
}
