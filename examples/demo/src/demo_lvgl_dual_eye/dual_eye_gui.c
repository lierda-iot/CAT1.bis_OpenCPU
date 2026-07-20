/**
 * @file dual_eye_gui.c
 * @brief LVGL GUI for the dual eye GIF display demo.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "lvgl.h"
#include "dual_eye_gui.h"
#include "dual_eye_gif_dec.h"

LV_IMG_DECLARE(dual_eye_gif_left)
LV_IMG_DECLARE(dual_eye_gif_right)

typedef struct
{
    lv_obj_t *left_gif;
    lv_obj_t *right_gif;
} dual_eye_gui_t;

static dual_eye_gui_t s_dual_eye_gui = {0};

static lv_obj_t *create_centered_gif(lv_disp_t *disp,
                                     const lv_img_dsc_t *src,
                                     const char *name)
{
    lv_obj_t *scr;
    lv_obj_t *gif;

    if (disp == NULL || src == NULL) {
        return NULL;
    }

    scr = lv_disp_get_scr_act(disp);
    if (scr == NULL) {
        return NULL;
    }

    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    gif = dual_eye_gif_dec_create(scr, src, name);
    return gif;
}

void dual_eye_gui_setup(lv_disp_t *left_disp, lv_disp_t *right_disp)
{
    s_dual_eye_gui.left_gif = create_centered_gif(left_disp,
                                                 &dual_eye_gif_left,
                                                 "left");
    s_dual_eye_gui.right_gif = create_centered_gif(right_disp,
                                                  &dual_eye_gif_right,
                                                  "right");
}
