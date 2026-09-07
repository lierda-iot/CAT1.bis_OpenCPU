#ifndef DEMO_LVGL_PHOTO_FS_STORE_H
#define DEMO_LVGL_PHOTO_FS_STORE_H

#include "demo_lvgl_photo_fs_types.h"

int demo_lvgl_photo_fs_store_mount(void);
int demo_lvgl_photo_fs_store_access_begin(void);
void demo_lvgl_photo_fs_store_access_end(void);

uint32_t demo_lvgl_photo_fs_store_crc32_update(uint32_t crc, const void *data, unsigned int len);
uint32_t demo_lvgl_photo_fs_store_crc32_finish(uint32_t crc);

int demo_lvgl_photo_fs_store_build_path(const char *id,
                                        demo_lvgl_photo_fs_format_t format,
                                        char *out_path,
                                        unsigned int out_len);
int demo_lvgl_photo_fs_store_build_tmp_path(const char *id,
                                            demo_lvgl_photo_fs_format_t format,
                                            char *out_path,
                                            unsigned int out_len);

int demo_lvgl_photo_fs_store_write_photo_file(const char *id,
                                              const void *data,
                                              unsigned int data_len);
int demo_lvgl_photo_fs_store_write_raw_file(const char *id,
                                            demo_lvgl_photo_fs_format_t format,
                                            const void *data,
                                            unsigned int data_len);
int demo_lvgl_photo_fs_store_read_photo_header(const char *id,
                                               demo_lvgl_photo_fs_file_header_t *out_header);

int demo_lvgl_photo_fs_store_save_index(const demo_lvgl_photo_fs_item_t *items,
                                        unsigned int count);
int demo_lvgl_photo_fs_store_load_index(demo_lvgl_photo_fs_item_t *items,
                                        unsigned int max_items,
                                        unsigned int *out_count);

#endif
