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

#if APP_WATCHFACE_EARTH_2015_EN
#include "watch_earth_2015.h"
#endif

int time_digital_clock_1_min_value = 0;
int time_digital_clock_1_hour_value = 0;
int time_digital_clock_1_sec_value = 0;

void setup_scr_time(lv_ui *ui)
{
    ui->time = lv_obj_create(NULL);
    lv_obj_set_size(ui->time, 360, 360);
    lv_obj_set_scrollbar_mode(ui->time, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_bg_opa(ui->time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
#if !APP_WATCHFACE_EARTH_2015_EN
    lv_obj_set_style_bg_img_src(ui->time, &_watch_360x360, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui->time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui->time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
#endif

    static bool time_update_timer_enabled = false;
#if APP_WATCHFACE_EARTH_2015_EN
    ui->time_digital_clock_1 = NULL;
    watch_earth_2015_create(ui->time);
#else
    ui->time_digital_clock_1 = lv_dclock_create(ui->time, "00:00:00");
    lv_obj_set_pos(ui->time_digital_clock_1, 81, 126);
    lv_obj_set_size(ui->time_digital_clock_1, 210, 71);

    lv_obj_set_style_radius(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->time_digital_clock_1, lv_color_hex(0x4f4ce7), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->time_digital_clock_1, &lv_font_Antonio_Regular_41, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->time_digital_clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->time_digital_clock_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->time_digital_clock_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->time_digital_clock_1, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->time_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
#endif

    if (!time_update_timer_enabled) {
        lv_timer_create(time_digital_clock_1_timer, 1000, NULL);
        time_update_timer_enabled = true;
    }

    time_digital_clock_1_timer(NULL);
    lv_obj_update_layout(ui->time);
    events_init_time(ui);
}
