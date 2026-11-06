#ifndef MODERN_ICONS_H
#define MODERN_ICONS_H
#include "lvgl.h"
extern const lv_img_dsc_t ui_phone_image;
extern const lv_img_dsc_t ui_wechat_qr_image;
#ifdef TIRTC_EXTERNAL_UI_ASSETS
/* Resource worker only, before the UI is attached. */
int tirtc_icons_load_external(void (*progress)(uint32_t));
#endif
#endif
