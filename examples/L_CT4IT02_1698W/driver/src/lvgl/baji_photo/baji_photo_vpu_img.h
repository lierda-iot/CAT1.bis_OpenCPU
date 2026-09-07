#ifndef BAJI_PHOTO_VPU_IMG_H
#define BAJI_PHOTO_VPU_IMG_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "baji_photo_types.h"

bool baji_photo_vpu_img_format_is_supported(baji_photo_format_t format);
int baji_photo_vpu_img_validate_payload(const baji_photo_manifest_item_t *item,
                                        const uint8_t *data,
                                        unsigned int len);
int baji_photo_vpu_img_load(const baji_photo_manifest_item_t *item,
                            uint32_t trace_id,
                            uint32_t dyn_seq,
                            lv_img_dsc_t *out_dsc,
                            void **out_buf);
void baji_photo_vpu_img_release(void *buf);

#endif
