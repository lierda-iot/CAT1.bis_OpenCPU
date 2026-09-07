#include "app_display_port_lvgl_internal.h"

#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD

#if APP_DISPLAY_LVGL_ENABLE_GIF
#include "app_display_port_lvgl_gif_player.h"
LV_IMG_DECLARE(dual_eye_gif_left)
#endif

static lv_obj_t *s_gif_container;

#if APP_DISPLAY_LVGL_ENABLE_GIF
static display_lvgl_gif_player_t *s_gif_player;
#endif

int display_lvgl_gif_create(lv_obj_t *root,
                            lv_coord_t screen_width,
                            lv_coord_t screen_height)
{
    lv_coord_t content_y = APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT;
    lv_coord_t content_h = screen_height - content_y;

    if (root == NULL || content_h <= 0) {
        return APP_ERR_INVALID_ARG;
    }

    s_gif_container = lv_obj_create(root);
    if (s_gif_container == NULL) {
        app_log("display gif container create failed");
        return APP_ERR_FAIL;
    }

    lv_obj_set_size(s_gif_container, screen_width, content_h);
    lv_obj_set_pos(s_gif_container, 0, content_y);
    lv_obj_clear_flag(s_gif_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_gif_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gif_container, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gif_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gif_container, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_gif_container, LV_OBJ_FLAG_HIDDEN);

    app_log("display gif create: %dx%d", (int)screen_width, (int)content_h);
    return APP_OK;
}

void display_lvgl_gif_stop(void)
{
#if APP_DISPLAY_LVGL_ENABLE_GIF
    if (s_gif_player != NULL) {
        app_log("display gif stop");
        display_lvgl_gif_player_destroy(s_gif_player);
        s_gif_player = NULL;
    }
#endif
}

int display_lvgl_gif_start(void)
{
    if (s_gif_container == NULL) {
        return APP_ERR_NOT_READY;
    }

#if APP_DISPLAY_LVGL_ENABLE_GIF
    if (s_gif_player != NULL) {
        return APP_OK;
    }

    app_log("display gif start");
    s_gif_player = display_lvgl_gif_player_create(s_gif_container,
                                                  &dual_eye_gif_left,
                                                  "watch");
    if (s_gif_player == NULL) {
        app_log("display gif start failed");
        return APP_ERR_FAIL;
    }
    return APP_OK;
#else
    app_log("display gif start rejected: disabled");
    return APP_ERR_NOT_SUPPORTED;
#endif
}

void display_lvgl_gif_set_visible(bool visible)
{
    if (s_gif_container == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_gif_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_gif_container, LV_OBJ_FLAG_HIDDEN);
    }
}

#endif
