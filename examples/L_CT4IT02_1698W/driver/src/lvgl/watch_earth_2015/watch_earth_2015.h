#ifndef WATCH_EARTH_2015_H
#define WATCH_EARTH_2015_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_IMG_DECLARE(face_earth_2015_dial_img_0_84);
LV_IMG_DECLARE(face_earth_2015_dial_img_hr_0);
LV_IMG_DECLARE(face_earth_2015_dial_img_min_0);
LV_IMG_DECLARE(face_earth_2015_dial_img_sec_0);

void watch_earth_2015_create(lv_obj_t *parent);
void watch_earth_2015_update_time(int hour, int minute, int second);

#ifdef __cplusplus
}
#endif

#endif
