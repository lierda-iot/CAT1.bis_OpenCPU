#ifndef BAJI_GIF_PLAYER_H
#define BAJI_GIF_PLAYER_H

#include "lvgl.h"

lv_obj_t *baji_gif_player_create(lv_obj_t *parent,
                                 const char *gif_path,
                                 const char *name);

#endif
