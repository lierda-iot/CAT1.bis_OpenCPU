/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *time;
	bool time_del;
	lv_obj_t *time_digital_clock_1;
	lv_obj_t *baji;
	bool baji_del;
	lv_obj_t *attuition;
	bool attuition_del;
	lv_obj_t *attfun;
	bool attfun_del;
	lv_obj_t *attfun_led_1;
	lv_obj_t *salary;
	bool salary_del;
	lv_obj_t *positioning;
	bool positioning_del;
	lv_obj_t *mp3;
	bool mp3_del;
	lv_obj_t *salary1;
	bool salary1_del;
	lv_obj_t *salary1_ta_1;
	lv_obj_t *salary1_ta_2;
	lv_obj_t *salary1_spangroup_1;
	lv_span_t *salary1_spangroup_1_span;
	lv_obj_t *salary1_spangroup_2;
	lv_span_t *salary1_spangroup_2_span;
	lv_obj_t *salary1_btn_1;
	lv_obj_t *salary1_btn_1_label;
	lv_obj_t *g_kb_top_layer;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_time(lv_ui *ui);
void setup_scr_baji(lv_ui *ui);
void setup_scr_attuition(lv_ui *ui);
void setup_scr_attfun(lv_ui *ui);
void setup_scr_salary(lv_ui *ui);
void setup_scr_positioning(lv_ui *ui);
void setup_scr_mp3(lv_ui *ui);
void setup_scr_salary1(lv_ui *ui);

LV_IMG_DECLARE(_watch_360x360);

LV_IMG_DECLARE(_baji_360x360);

LV_IMG_DECLARE(_attitude_360x360);

LV_IMG_DECLARE(_attitudefun_360x360);

LV_IMG_DECLARE(_salary_360x360);

LV_IMG_DECLARE(_positioning_360x360);

LV_IMG_DECLARE(_mp3_360x360);

LV_IMG_DECLARE(_salary_360x360);

LV_FONT_DECLARE(lv_font_Antonio_Regular_41)
LV_FONT_DECLARE(lv_font_montserratMedium_23)
LV_FONT_DECLARE(lv_font_DAIMENG_28)
LV_FONT_DECLARE(lv_font_montserratMedium_18)


#ifdef __cplusplus
}
#endif
#endif
