#include "lvgl.h"
#include <stdio.h>
#include <stdarg.h>

lv_obj_t *lv_dclock_create(lv_obj_t *parent, const char *init_text)
{
    lv_obj_t *label = lv_label_create(parent);
    if (label && init_text) {
        lv_label_set_text(label, init_text);
    }
    return label;
}

void lv_dclock_set_text_fmt(lv_obj_t *obj, const char *fmt, ...)
{
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(obj, buf);
}

void clock_count_24(int *hour, int *min, int *sec)
{
    (*sec)++;
    if (*sec >= 60) {
        *sec = 0;
        (*min)++;
    }
    if (*min >= 60) {
        *min = 0;
        (*hour)++;
    }
    if (*hour >= 24) {
        *hour = 0;
    }
}
