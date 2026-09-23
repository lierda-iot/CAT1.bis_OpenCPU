#ifndef APP_DISPLAY_PORT_LVGL_GIF_PLAYER_H
#define APP_DISPLAY_PORT_LVGL_GIF_PLAYER_H

#include "app_display_port_lvgl_internal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD && APP_DISPLAY_LVGL_ENABLE_GIF

typedef struct display_lvgl_gif_player display_lvgl_gif_player_t;

display_lvgl_gif_player_t *display_lvgl_gif_player_create(lv_obj_t *parent,
                                                          const lv_img_dsc_t *gif_src,
                                                          const char *name);
void display_lvgl_gif_player_destroy(display_lvgl_gif_player_t *player);

#endif

#endif /* APP_DISPLAY_PORT_LVGL_GIF_PLAYER_H */
