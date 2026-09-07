#ifndef BAJI_PHOTO_BIND_UI_H
#define BAJI_PHOTO_BIND_UI_H

#include <stdbool.h>

#include "lvgl.h"

#include "baji_photo_types.h"

typedef struct {
    void (*hide_controls)(void);
    void (*cancel_hide_timer)(void);
    void (*refresh_buttons)(void);
    void (*refresh_play_timer)(void);
} baji_photo_bind_ui_ops_t;

void baji_photo_bind_ui_set_ops(const baji_photo_bind_ui_ops_t *ops);
void baji_photo_bind_ui_set_screen(lv_obj_t *screen);
void baji_photo_bind_ui_clear_screen(lv_obj_t *screen);
void baji_photo_bind_ui_set_bind_status(baji_photo_bind_status_t status);
void baji_photo_bind_ui_set_bind_token(const baji_photo_bind_token_info_t *token);
void baji_photo_bind_ui_set_notice(const baji_photo_bind_notice_t *notice);
void baji_photo_bind_ui_set_fatal_error(int fatal_error);
void baji_photo_bind_ui_tick(bool page_active);
bool baji_photo_bind_ui_is_visible(void);
const char *baji_photo_bind_ui_get_hint(void);

#endif
