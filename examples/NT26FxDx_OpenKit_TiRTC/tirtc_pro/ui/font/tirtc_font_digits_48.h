#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(tirtc_font_digits_48);
#ifdef TIRTC_EXTERNAL_UI_ASSETS
/* Resource worker only, before exposing this font to LVGL. */
int tirtc_font_digits_load_external(void (*progress)(uint32_t));
#endif
#ifdef __cplusplus
}
#endif
