#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(tirtc_font_14);
#ifdef TIRTC_EXTERNAL_UI_ASSETS
/* Resource worker only, before exposing this font to LVGL. */
int tirtc_font_load_external(void (*progress)(uint32_t));
typedef const uint8_t *(*tirtc_font_bitmap_reader_t)(uint32_t offset, uint32_t bytes);
/* LVGL owner only; install once before attaching Chinese labels. */
int tirtc_font_set_reader(tirtc_font_bitmap_reader_t reader, uint32_t length);
int tirtc_font_set_bitmap(const uint8_t *bitmap, uint32_t length);
#endif
#ifdef __cplusplus
}
#endif
