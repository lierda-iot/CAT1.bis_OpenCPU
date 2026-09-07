#ifndef BAJI_PHOTO_FLASH_IMG_H
#define BAJI_PHOTO_FLASH_IMG_H

#include "lvgl.h"

#include "baji_photo_types.h"

int baji_photo_flash_img_load(const baji_photo_manifest_item_t *item,
                              uint32_t trace_id,
                              uint32_t dyn_seq,
                              lv_img_dsc_t *out_dsc,
                              void **out_buf);
void baji_photo_flash_img_release(void *buf);

#endif
