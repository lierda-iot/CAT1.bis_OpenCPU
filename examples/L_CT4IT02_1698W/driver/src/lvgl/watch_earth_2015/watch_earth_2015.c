#include "watch_earth_2015.h"
#include "watch_earth_2015_core.h"

static lv_obj_t *s_watch_earth_2015_bg;
static lv_obj_t *s_watch_earth_2015_hr;
static lv_obj_t *s_watch_earth_2015_min;
static lv_obj_t *s_watch_earth_2015_sec;

static void watch_earth_2015_set_img(lv_obj_t *obj, const lv_img_dsc_t *src,
                               lv_coord_t x, lv_coord_t y) {
    lv_img_set_src(obj, src);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
}

void watch_earth_2015_create(lv_obj_t *parent)
{
    s_watch_earth_2015_bg = lv_img_create(parent);
    lv_img_set_src(s_watch_earth_2015_bg, &face_earth_2015_dial_img_0_84);
    lv_obj_clear_flag(s_watch_earth_2015_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_watch_earth_2015_bg, 0, 0);

    s_watch_earth_2015_hr = lv_img_create(parent);
    watch_earth_2015_set_img(s_watch_earth_2015_hr,
                            &face_earth_2015_dial_img_hr_0, 169, 60);
    lv_img_set_pivot(s_watch_earth_2015_hr, 11, 120);

    s_watch_earth_2015_min = lv_img_create(parent);
    watch_earth_2015_set_img(s_watch_earth_2015_min,
                            &face_earth_2015_dial_img_min_0, 169, 60);
    lv_img_set_pivot(s_watch_earth_2015_min, 11, 120);

    s_watch_earth_2015_sec = lv_img_create(parent);
    watch_earth_2015_set_img(s_watch_earth_2015_sec,
                            &face_earth_2015_dial_img_sec_0, 169, 62);
    lv_img_set_pivot(s_watch_earth_2015_sec, 11, 118);
}

void watch_earth_2015_update_time(int hour, int minute, int second)
{
    if (s_watch_earth_2015_hr && lv_obj_is_valid(s_watch_earth_2015_hr)) {
        lv_img_set_angle(s_watch_earth_2015_hr,
                         watch_earth_2015_hour_angle(hour, minute, second));
    }
    if (s_watch_earth_2015_min && lv_obj_is_valid(s_watch_earth_2015_min)) {
        lv_img_set_angle(s_watch_earth_2015_min,
                         watch_earth_2015_minute_angle(minute, second));
    }
    if (s_watch_earth_2015_sec && lv_obj_is_valid(s_watch_earth_2015_sec)) {
        lv_img_set_angle(s_watch_earth_2015_sec,
                         watch_earth_2015_second_angle(second));
    }
}
