#ifndef BAJI_PHOTO_STORE_H
#define BAJI_PHOTO_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "baji_photo_types.h"

typedef struct {
    bool cleanup_pending;
    unsigned int remaining_count;
} baji_photo_store_delete_result_t;

bool baji_photo_store_is_ready(void);
int baji_photo_store_mount(void);
int baji_photo_store_access_begin(void);
void baji_photo_store_access_end(void);
int baji_photo_store_free_size(void);
uint32_t baji_photo_store_crc32_update(uint32_t crc, const void *data, unsigned int len);
uint32_t baji_photo_store_crc32_finish(uint32_t crc);
int baji_photo_store_build_tmp_path(const char *id, char *out_path, unsigned int out_len);
int baji_photo_store_build_photo_path(const char *id, char *out_path, unsigned int out_len);
int baji_photo_store_build_gif_path(const char *id, char *out_path, unsigned int out_len);
int baji_photo_store_build_jpeg_path(const char *id, char *out_path, unsigned int out_len);
int baji_photo_store_build_png_path(const char *id, char *out_path, unsigned int out_len);
int baji_photo_store_build_item_path(const char *id,
                                     baji_photo_format_t format,
                                     char *out_path,
                                     unsigned int out_len);
int baji_photo_store_write_photo_file(const char *id, const void *data, unsigned int data_len);
int baji_photo_store_write_gif_file(const char *id, const void *data, unsigned int data_len);
int baji_photo_store_write_raw_file(const char *id,
                                    baji_photo_format_t format,
                                    const void *data,
                                    unsigned int data_len);
int baji_photo_store_read_photo_header(const char *id, baji_photo_file_header_t *out_header);
int baji_photo_store_remove_photo(const char *id);
int baji_photo_store_remove_item(const baji_photo_manifest_item_t *item);
int baji_photo_store_save_task_state(const baji_photo_task_state_t *state);
int baji_photo_store_load_task_state(baji_photo_task_state_t *state);
int baji_photo_store_clear_task_state(void);
int baji_photo_store_save_pending_reply(const baji_photo_mqtt_pending_reply_t *reply);
int baji_photo_store_load_pending_reply(baji_photo_mqtt_pending_reply_t *reply);
int baji_photo_store_clear_pending_reply(void);
int baji_photo_store_save_device_meta(const baji_photo_device_meta_t *meta);
int baji_photo_store_load_device_meta(baji_photo_device_meta_t *meta);
int baji_photo_store_update_device_meta_mqtt(const char *task_id,
                                             const char *image_id,
                                             const char *image_md5,
                                             uint64_t display_time_ms);
int baji_photo_store_update_device_meta_playback(const baji_photo_playback_meta_t *playback);
int baji_photo_store_save_index(const baji_photo_manifest_item_t *items,
                                unsigned int count);
int baji_photo_store_load_index(baji_photo_manifest_item_t *items,
                                unsigned int max_items,
                                unsigned int *out_count);
int baji_photo_store_upsert_item(const baji_photo_manifest_item_t *item,
                                 unsigned int *out_count);
int baji_photo_store_delete_item_and_index(const baji_photo_manifest_item_t *item,
                                           baji_photo_store_delete_result_t *out_result);

#endif
