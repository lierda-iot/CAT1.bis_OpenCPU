#ifndef BAJI_PHOTO_CORE_UTILS_H
#define BAJI_PHOTO_CORE_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#include "baji_photo_types.h"

bool baji_photo_true_color_dims_valid(uint32_t width, uint32_t height);
uint32_t baji_photo_true_color_data_size(uint32_t width, uint32_t height);
const char *baji_photo_skip_ws(const char *p, const char *end);
const char *baji_photo_find_json_key(const char *start, const char *end, const char *key);
int baji_photo_parse_json_string(const char *obj_start,
                                 const char *obj_end,
                                 const char *key,
                                 char *out,
                                 unsigned int out_len);
int baji_photo_parse_json_u32(const char *obj_start,
                              const char *obj_end,
                              const char *key,
                              uint32_t *out);
int baji_photo_parse_json_hex_u32(const char *obj_start,
                                  const char *obj_end,
                                  const char *key,
                                  uint32_t *out);
int baji_photo_parse_manifest_format(const char *obj_start,
                                     const char *obj_end,
                                     baji_photo_format_t *out);
int baji_photo_manifest_id_valid(const char *id);
int baji_photo_manifest_remote_path_valid(const char *path);

#endif
