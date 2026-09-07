/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_salary1(lv_ui *ui)
{
    //Write codes salary1
    ui->salary1 = lv_obj_create(NULL);
    lv_obj_set_size(ui->salary1, 360, 360);
    lv_obj_set_scrollbar_mode(ui->salary1, LV_SCROLLBAR_MODE_OFF);

    //Write style for salary1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->salary1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui->salary1, &_salary_360x360, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui->salary1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor(ui->salary1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui->salary1, 199, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes salary1_ta_1
    ui->salary1_ta_1 = lv_textarea_create(ui->salary1);
    lv_textarea_set_text(ui->salary1_ta_1, "99999");
    lv_textarea_set_placeholder_text(ui->salary1_ta_1, "");
    lv_textarea_set_password_bullet(ui->salary1_ta_1, "*");
    lv_textarea_set_password_mode(ui->salary1_ta_1, false);
    lv_textarea_set_one_line(ui->salary1_ta_1, false);
    lv_textarea_set_accepted_chars(ui->salary1_ta_1, "");
    lv_textarea_set_max_length(ui->salary1_ta_1, 32);
#if LV_USE_KEYBOARD != 0 || LV_USE_ZH_KEYBOARD != 0
    lv_obj_add_event_cb(ui->salary1_ta_1, ta_event_cb, LV_EVENT_ALL, ui->g_kb_top_layer);
#endif
    lv_obj_set_pos(ui->salary1_ta_1, 143, 108);
    lv_obj_set_size(ui->salary1_ta_1, 153, 37);

    //Write style for salary1_ta_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->salary1_ta_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->salary1_ta_1, &lv_font_montserratMedium_23, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->salary1_ta_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->salary1_ta_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->salary1_ta_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->salary1_ta_1, 179, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->salary1_ta_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->salary1_ta_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->salary1_ta_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->salary1_ta_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->salary1_ta_1, lv_color_hex(0x6a6363), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->salary1_ta_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->salary1_ta_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->salary1_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->salary1_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->salary1_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->salary1_ta_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for salary1_ta_1, Part: LV_PART_SCROLLBAR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->salary1_ta_1, 255, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->salary1_ta_1, lv_color_hex(0x2195f6), LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->salary1_ta_1, LV_GRAD_DIR_NONE, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->salary1_ta_1, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes salary1_ta_2
    ui->salary1_ta_2 = lv_textarea_create(ui->salary1);
    lv_textarea_set_text(ui->salary1_ta_2, "99999");
    lv_textarea_set_placeholder_text(ui->salary1_ta_2, "");
    lv_textarea_set_password_bullet(ui->salary1_ta_2, "*");
    lv_textarea_set_password_mode(ui->salary1_ta_2, false);
    lv_textarea_set_one_line(ui->salary1_ta_2, false);
    lv_textarea_set_accepted_chars(ui->salary1_ta_2, "");
    lv_textarea_set_max_length(ui->salary1_ta_2, 32);
#if LV_USE_KEYBOARD != 0 || LV_USE_ZH_KEYBOARD != 0
    lv_obj_add_event_cb(ui->salary1_ta_2, ta_event_cb, LV_EVENT_ALL, ui->g_kb_top_layer);
#endif
    lv_obj_set_pos(ui->salary1_ta_2, 143, 175);
    lv_obj_set_size(ui->salary1_ta_2, 153, 37);

    //Write style for salary1_ta_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->salary1_ta_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->salary1_ta_2, &lv_font_montserratMedium_23, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->salary1_ta_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->salary1_ta_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->salary1_ta_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->salary1_ta_2, 179, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->salary1_ta_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->salary1_ta_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->salary1_ta_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->salary1_ta_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->salary1_ta_2, lv_color_hex(0x6e6565), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->salary1_ta_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->salary1_ta_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->salary1_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->salary1_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->salary1_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->salary1_ta_2, 6, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for salary1_ta_2, Part: LV_PART_SCROLLBAR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->salary1_ta_2, 255, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->salary1_ta_2, lv_color_hex(0x2195f6), LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->salary1_ta_2, LV_GRAD_DIR_NONE, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->salary1_ta_2, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes salary1_spangroup_1
    ui->salary1_spangroup_1 = lv_spangroup_create(ui->salary1);
    lv_spangroup_set_align(ui->salary1_spangroup_1, LV_TEXT_ALIGN_LEFT);
    lv_spangroup_set_overflow(ui->salary1_spangroup_1, LV_SPAN_OVERFLOW_CLIP);
    lv_spangroup_set_mode(ui->salary1_spangroup_1, LV_SPAN_MODE_BREAK);
    //create span
    ui->salary1_spangroup_1_span = lv_spangroup_new_span(ui->salary1_spangroup_1);
    lv_span_set_text(ui->salary1_spangroup_1_span, "每月草料");
    lv_style_set_text_color(&ui->salary1_spangroup_1_span->style, lv_color_hex(0x797070));
    lv_style_set_text_decor(&ui->salary1_spangroup_1_span->style, LV_TEXT_DECOR_NONE);
    lv_style_set_text_font(&ui->salary1_spangroup_1_span->style, &lv_font_DAIMENG_28);
    lv_obj_set_pos(ui->salary1_spangroup_1, 27, 116);
    lv_obj_set_size(ui->salary1_spangroup_1, 119, 27);

    //Write style state: LV_STATE_DEFAULT for &style_salary1_spangroup_1_main_main_default
    static lv_style_t style_salary1_spangroup_1_main_main_default;
    ui_init_style(&style_salary1_spangroup_1_main_main_default);

    lv_style_set_border_width(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_radius(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_bg_opa(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_pad_top(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_pad_right(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_pad_bottom(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_pad_left(&style_salary1_spangroup_1_main_main_default, 0);
    lv_style_set_shadow_width(&style_salary1_spangroup_1_main_main_default, 0);
    lv_obj_add_style(ui->salary1_spangroup_1, &style_salary1_spangroup_1_main_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_spangroup_refr_mode(ui->salary1_spangroup_1);

    //Write codes salary1_spangroup_2
    ui->salary1_spangroup_2 = lv_spangroup_create(ui->salary1);
    lv_spangroup_set_align(ui->salary1_spangroup_2, LV_TEXT_ALIGN_LEFT);
    lv_spangroup_set_overflow(ui->salary1_spangroup_2, LV_SPAN_OVERFLOW_CLIP);
    lv_spangroup_set_mode(ui->salary1_spangroup_2, LV_SPAN_MODE_BREAK);
    //create span
    ui->salary1_spangroup_2_span = lv_spangroup_new_span(ui->salary1_spangroup_2);
    lv_span_set_text(ui->salary1_spangroup_2_span, "工作时长");
    lv_style_set_text_color(&ui->salary1_spangroup_2_span->style, lv_color_hex(0x707070));
    lv_style_set_text_decor(&ui->salary1_spangroup_2_span->style, LV_TEXT_DECOR_NONE);
    lv_style_set_text_font(&ui->salary1_spangroup_2_span->style, &lv_font_DAIMENG_28);
    lv_obj_set_pos(ui->salary1_spangroup_2, 27, 183);
    lv_obj_set_size(ui->salary1_spangroup_2, 119, 37);

    //Write style state: LV_STATE_DEFAULT for &style_salary1_spangroup_2_main_main_default
    static lv_style_t style_salary1_spangroup_2_main_main_default;
    ui_init_style(&style_salary1_spangroup_2_main_main_default);

    lv_style_set_border_width(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_radius(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_bg_opa(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_pad_top(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_pad_right(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_pad_bottom(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_pad_left(&style_salary1_spangroup_2_main_main_default, 0);
    lv_style_set_shadow_width(&style_salary1_spangroup_2_main_main_default, 0);
    lv_obj_add_style(ui->salary1_spangroup_2, &style_salary1_spangroup_2_main_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_spangroup_refr_mode(ui->salary1_spangroup_2);

    //Write codes salary1_btn_1
    ui->salary1_btn_1 = lv_btn_create(ui->salary1);
    ui->salary1_btn_1_label = lv_label_create(ui->salary1_btn_1);
    lv_label_set_text(ui->salary1_btn_1_label, "确定");
    lv_label_set_long_mode(ui->salary1_btn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->salary1_btn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->salary1_btn_1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->salary1_btn_1_label, LV_PCT(100));
    lv_obj_set_pos(ui->salary1_btn_1, 129, 250);
    lv_obj_set_size(ui->salary1_btn_1, 100, 50);

    //Write style for salary1_btn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->salary1_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->salary1_btn_1, lv_color_hex(0xc6d6d7), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->salary1_btn_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->salary1_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->salary1_btn_1, 25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->salary1_btn_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->salary1_btn_1, lv_color_hex(0x0d4b3b), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->salary1_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->salary1_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->salary1_btn_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->salary1_btn_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->salary1_btn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->salary1_btn_1, &lv_font_montserratMedium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->salary1_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->salary1_btn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of salary1.


    //Update current screen layout.
    lv_obj_update_layout(ui->salary1);

}
