#ifndef UI_FACE_BACKGROUND_H
#define UI_FACE_BACKGROUND_H
#include "lvgl.h"
#ifdef TIRTC_EXTERNAL_UI_ASSETS
extern const uint16_t *ui_face_background_pixels;
int ui_face_set_background(const uint16_t *pixels, uint32_t length);
#else
extern const uint16_t ui_face_background_pixels[320 * 240];
#endif
extern lv_img_dsc_t ui_face_background_image;
#endif
