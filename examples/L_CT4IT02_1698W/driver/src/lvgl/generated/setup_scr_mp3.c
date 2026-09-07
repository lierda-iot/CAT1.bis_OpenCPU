/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"

void setup_scr_mp3(lv_ui *ui)
{
    ui->mp3 = lv_obj_create(NULL);
    lv_obj_set_size(ui->mp3, 360, 360);
    lv_obj_set_scrollbar_mode(ui->mp3, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ui->mp3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(ui->mp3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui->mp3, &_mp3_360x360, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui->mp3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui->mp3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_update_layout(ui->mp3);
    events_init_mp3(ui);
}