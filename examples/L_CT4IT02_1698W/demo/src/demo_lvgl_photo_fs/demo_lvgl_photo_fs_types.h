#ifndef DEMO_LVGL_PHOTO_FS_TYPES_H
#define DEMO_LVGL_PHOTO_FS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "baji_photo/baji_photo_config.h"

#ifndef DEMO_LVGL_PHOTO_FS_ENABLE_GIF
#define DEMO_LVGL_PHOTO_FS_ENABLE_GIF 1
#endif

#ifndef DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
#define DEMO_LVGL_PHOTO_FS_ENABLE_JPEG 0
#endif

#ifndef DEMO_LVGL_PHOTO_FS_ENABLE_PNG
#define DEMO_LVGL_PHOTO_FS_ENABLE_PNG 0
#endif

#ifndef DEMO_LVGL_PHOTO_FS_FORCE_RESEED
#define DEMO_LVGL_PHOTO_FS_FORCE_RESEED 0
#endif

#define DEMO_LVGL_PHOTO_FS_MAX_ITEMS          BAJI_PHOTO_TOTAL_MAX
#define DEMO_LVGL_PHOTO_FS_ID_MAX_LEN         BAJI_PHOTO_ID_MAX_LEN
#define DEMO_LVGL_PHOTO_FS_NAME_MAX_LEN       BAJI_PHOTO_NAME_MAX_LEN
#define DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN       BAJI_PHOTO_PATH_MAX_LEN
#define DEMO_LVGL_PHOTO_FS_INDEX_MAX_SIZE     BAJI_PHOTO_HTTP_MANIFEST_MAX

#define DEMO_LVGL_PHOTO_FS_IMG_W              BAJI_PHOTO_IMG_W
#define DEMO_LVGL_PHOTO_FS_IMG_H              BAJI_PHOTO_IMG_H
#define DEMO_LVGL_PHOTO_FS_IMG_BPP            BAJI_PHOTO_IMG_BPP
#define DEMO_LVGL_PHOTO_FS_IMG_DATA_SIZE      \
    (DEMO_LVGL_PHOTO_FS_IMG_W * DEMO_LVGL_PHOTO_FS_IMG_H * DEMO_LVGL_PHOTO_FS_IMG_BPP)

#define DEMO_LVGL_PHOTO_FS_MAGIC              BAJI_PHOTO_MAGIC

#define DEMO_LVGL_PHOTO_FS_ROOT_DIR           BAJI_PHOTO_STORE_DIR
#define DEMO_LVGL_PHOTO_FS_PHOTO_DIR          BAJI_PHOTO_STORE_PHOTO_DIR
#define DEMO_LVGL_PHOTO_FS_GIF_DIR            BAJI_PHOTO_STORE_GIF_DIR
#define DEMO_LVGL_PHOTO_FS_JPEG_DIR           BAJI_PHOTO_STORE_DIR "/jpeg"
#define DEMO_LVGL_PHOTO_FS_PNG_DIR            BAJI_PHOTO_STORE_DIR "/png"
#define DEMO_LVGL_PHOTO_FS_TMP_DIR            BAJI_PHOTO_STORE_TMP_DIR
#define DEMO_LVGL_PHOTO_FS_INDEX_FILE         BAJI_PHOTO_INDEX_FILE
#define DEMO_LVGL_PHOTO_FS_INDEX_TMP_FILE     BAJI_PHOTO_INDEX_TMP_FILE

typedef enum {
    DEMO_LVGL_PHOTO_FS_FORMAT_BJP = 0,
    DEMO_LVGL_PHOTO_FS_FORMAT_GIF = 1,
    DEMO_LVGL_PHOTO_FS_FORMAT_JPEG = 2,
    DEMO_LVGL_PHOTO_FS_FORMAT_PNG = 3,
} demo_lvgl_photo_fs_format_t;

typedef struct {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint16_t cf;
    uint16_t reserved;
    uint32_t data_size;
    uint32_t crc32;
} demo_lvgl_photo_fs_file_header_t;

typedef struct {
    char id[DEMO_LVGL_PHOTO_FS_ID_MAX_LEN + 1u];
    char name[DEMO_LVGL_PHOTO_FS_NAME_MAX_LEN + 1u];
    char remote_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    char local_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    uint32_t file_size;
    uint32_t crc32;
    uint16_t width;
    uint16_t height;
    uint16_t cf;
    demo_lvgl_photo_fs_format_t format;
} demo_lvgl_photo_fs_item_t;

typedef struct {
    uint16_t total;
    uint16_t written;
    uint16_t skipped;
    uint16_t failed;
} demo_lvgl_photo_fs_seed_result_t;

#endif
