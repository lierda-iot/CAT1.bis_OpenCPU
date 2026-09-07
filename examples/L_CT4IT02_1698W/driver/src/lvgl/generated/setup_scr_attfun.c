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



void setup_scr_attfun(lv_ui *ui)
{
    //Write codes attfun
    ui->attfun = lv_obj_create(NULL);
    lv_obj_set_size(ui->attfun, 360, 360);
    lv_obj_set_scrollbar_mode(ui->attfun, LV_SCROLLBAR_MODE_OFF);

    //Write style for attfun, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->attfun, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui->attfun, &_attitudefun_360x360, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui->attfun, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui->attfun, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes attfun_led_1
    ui->attfun_led_1 = lv_led_create(ui->attfun);
    lv_led_set_brightness(ui->attfun_led_1, 255);
    lv_led_set_color(ui->attfun_led_1, lv_color_hex(0x007fff));
    lv_obj_set_pos(ui->attfun_led_1, 168, 168);
    lv_obj_set_size(ui->attfun_led_1, 29, 27);

    //The custom code of attfun.


    //Update current screen layout.
    lv_obj_update_layout(ui->attfun);

    //Init events for screen.
    events_init_attfun(ui);
}
