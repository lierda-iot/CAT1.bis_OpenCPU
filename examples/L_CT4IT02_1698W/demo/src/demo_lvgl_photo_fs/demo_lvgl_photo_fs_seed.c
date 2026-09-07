#include "demo_lvgl_photo_fs_seed.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_external_flash_fs.h"
#include "liot_os.h"
#include "mm_jpeg_if.h"
#include "mm_video_if.h"

#include "demo_lvgl_photo_fs_store.h"

extern const lv_img_dsc_t landscape_1_data;
extern const lv_img_dsc_t landscape_2_data;
extern const lv_img_dsc_t landscape_3_data;
extern const lv_img_dsc_t landscape_4_data;
extern const lv_img_dsc_t landscape_5_data;

#if DEMO_LVGL_PHOTO_FS_ENABLE_GIF
extern const lv_img_dsc_t baji_gif_seed_5_data;
extern const lv_img_dsc_t saa_360_360_gif_data;
extern const lv_img_dsc_t saq_360_360_gif_data;
#endif

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
extern const lv_img_dsc_t landscape_6_360_360_jpg_data;
extern const lv_img_dsc_t landscape_7_360_360_jpg_data;
extern const lv_img_dsc_t landscape_8_360_360_jpg_data;
#endif

typedef struct {
    const char *id;
    const char *name;
    const char *remote_path;
    demo_lvgl_photo_fs_format_t format;
    const lv_img_dsc_t *src;
    uint16_t width;
    uint16_t height;
} demo_lvgl_photo_fs_seed_item_t;

static const demo_lvgl_photo_fs_seed_item_t g_demo_lvgl_photo_fs_seed_items[] = {
    {"builtin_001", "landscape 1", "builtin/landscape_1.bjp", DEMO_LVGL_PHOTO_FS_FORMAT_BJP, &landscape_1_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_002", "landscape 2", "builtin/landscape_2.bjp", DEMO_LVGL_PHOTO_FS_FORMAT_BJP, &landscape_2_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_003", "landscape 3", "builtin/landscape_3.bjp", DEMO_LVGL_PHOTO_FS_FORMAT_BJP, &landscape_3_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_004", "landscape 4", "builtin/landscape_4.bjp", DEMO_LVGL_PHOTO_FS_FORMAT_BJP, &landscape_4_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_005", "landscape 5", "builtin/landscape_5.bjp", DEMO_LVGL_PHOTO_FS_FORMAT_BJP, &landscape_5_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
#if DEMO_LVGL_PHOTO_FS_ENABLE_GIF
    {"builtin_gif_001", "gif seed 5", "builtin/dual_eye_gif_5.gif", DEMO_LVGL_PHOTO_FS_FORMAT_GIF, &baji_gif_seed_5_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_gif_002", "gif saa 360", "builtin/SAA_360_360.gif", DEMO_LVGL_PHOTO_FS_FORMAT_GIF, &saa_360_360_gif_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_gif_003", "gif saq 360", "builtin/SAQ_360_360.gif", DEMO_LVGL_PHOTO_FS_FORMAT_GIF, &saq_360_360_gif_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
#endif
#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
    {"builtin_jpg_001", "landscape 6 jpg", "builtin/landscape_6_360_360.jpg", DEMO_LVGL_PHOTO_FS_FORMAT_JPEG, &landscape_6_360_360_jpg_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_jpg_002", "landscape 7 jpg", "builtin/landscape_7_360_360.jpg", DEMO_LVGL_PHOTO_FS_FORMAT_JPEG, &landscape_7_360_360_jpg_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
    {"builtin_jpg_003", "landscape 8 jpg", "builtin/landscape_8_360_360.jpg", DEMO_LVGL_PHOTO_FS_FORMAT_JPEG, &landscape_8_360_360_jpg_data, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H},
#endif
};

#define DEMO_LVGL_PHOTO_FS_SEED_COUNT \
    (sizeof(g_demo_lvgl_photo_fs_seed_items) / sizeof(g_demo_lvgl_photo_fs_seed_items[0]))
#define DEMO_LVGL_PHOTO_FS_BUILTIN_ID_PREFIX "builtin_"

static bool demo_lvgl_photo_fs_seed_is_builtin_id(const char *id)
{
    return (id != NULL) &&
           (strncmp(id,
                    DEMO_LVGL_PHOTO_FS_BUILTIN_ID_PREFIX,
                    strlen(DEMO_LVGL_PHOTO_FS_BUILTIN_ID_PREFIX)) == 0);
}

static int demo_lvgl_photo_fs_seed_find_item(const demo_lvgl_photo_fs_item_t *items,
                                             unsigned int count,
                                             const char *id)
{
    unsigned int i;

    if ((items == NULL) || (id == NULL)) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        if (strcmp(items[i].id, id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool demo_lvgl_photo_fs_seed_rgb565_size(uint16_t width,
                                                uint16_t height,
                                                uint32_t *out_size)
{
    uint32_t pixels;

    if ((out_size == NULL) || (width == 0u) || (height == 0u)) {
        return false;
    }
    if ((uint32_t)width > (UINT32_MAX / (uint32_t)height)) {
        return false;
    }

    pixels = (uint32_t)width * (uint32_t)height;
    if (pixels > (UINT32_MAX / DEMO_LVGL_PHOTO_FS_IMG_BPP)) {
        return false;
    }

    *out_size = pixels * DEMO_LVGL_PHOTO_FS_IMG_BPP;
    return true;
}

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
static bool demo_lvgl_photo_fs_seed_jpeg_decode_info(const uint8_t *data,
                                                     unsigned int data_len,
                                                     uint16_t *out_width,
                                                     uint16_t *out_height)
{
    JPEG_INFO info;
    void *decoder;
    int ret;

    if ((data == NULL) || (data_len == 0u) || (out_width == NULL) || (out_height == NULL)) {
        return false;
    }

    decoder = JpegD_Create();
    if (decoder == NULL) {
        return false;
    }

    (void)memset(&info, 0, sizeof(info));
    ret = JpegD_DecodeInfo(decoder, (unsigned char *)data, data_len, &info);
    JpegD_Destroy(decoder);
    if ((ret != 0) || (info.uWidth == 0u) || (info.uHeight == 0u) ||
        (info.uWidth > UINT16_MAX) || (info.uHeight > UINT16_MAX)) {
        return false;
    }

    *out_width = (uint16_t)info.uWidth;
    *out_height = (uint16_t)info.uHeight;
    return true;
}
#endif

static bool demo_lvgl_photo_fs_seed_src_is_valid(const demo_lvgl_photo_fs_seed_item_t *seed)
{
    const lv_img_dsc_t *src;
    const uint8_t *data;
    uint32_t expected_size = 0u;

    if (seed == NULL) {
        return false;
    }

    src = seed->src;

    if ((src == NULL) || (src->data == NULL) || (src->data_size == 0u)) {
        return false;
    }

    if (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_GIF) {
        if ((src->header.cf != LV_IMG_CF_RAW) ||
            (seed->width == 0u) ||
            (seed->height == 0u) ||
            (src->header.w != seed->width) ||
            (src->header.h != seed->height) ||
            (src->data_size < 6u)) {
            return false;
        }
        data = (const uint8_t *)src->data;
        return ((memcmp(data, "GIF87a", 6u) == 0) || (memcmp(data, "GIF89a", 6u) == 0));
    }

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
    if (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_JPEG) {
        uint16_t decoded_w = 0u;
        uint16_t decoded_h = 0u;

        data = (const uint8_t *)src->data;
        if ((src->header.cf != LV_IMG_CF_RAW) ||
            (seed->width == 0u) ||
            (seed->height == 0u) ||
            (src->data_size < 4u) ||
            (data[0] != 0xFFu) ||
            (data[1] != 0xD8u)) {
            return false;
        }

        return demo_lvgl_photo_fs_seed_jpeg_decode_info(data,
                                                        (unsigned int)src->data_size,
                                                        &decoded_w,
                                                        &decoded_h) &&
               (decoded_w == seed->width) &&
               (decoded_h == seed->height);
    }
#endif

    if (!demo_lvgl_photo_fs_seed_rgb565_size(seed->width, seed->height, &expected_size)) {
        return false;
    }

    return (src->header.cf == LV_IMG_CF_TRUE_COLOR) &&
           (src->header.w == seed->width) &&
           (src->header.h == seed->height) &&
           (src->data_size == expected_size);
}

static uint32_t demo_lvgl_photo_fs_seed_crc32(const lv_img_dsc_t *src)
{
    uint32_t crc;

    crc = demo_lvgl_photo_fs_store_crc32_update(0xFFFFFFFFu, src->data, src->data_size);
    return demo_lvgl_photo_fs_store_crc32_finish(crc);
}

static void demo_lvgl_photo_fs_seed_fill_item(const demo_lvgl_photo_fs_seed_item_t *seed,
                                              uint32_t crc,
                                              demo_lvgl_photo_fs_item_t *out_item)
{
    (void)memset(out_item, 0, sizeof(*out_item));
    (void)snprintf(out_item->id, sizeof(out_item->id), "%s", seed->id);
    (void)snprintf(out_item->name, sizeof(out_item->name), "%s", seed->name);
    (void)snprintf(out_item->remote_path, sizeof(out_item->remote_path), "%s", seed->remote_path);
    out_item->format = seed->format;
    out_item->crc32 = crc;
    out_item->width = seed->width;
    out_item->height = seed->height;
    if ((seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_GIF) ||
        (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_JPEG) ||
        (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_PNG)) {
        out_item->cf = LV_IMG_CF_RAW;
        out_item->file_size = (uint32_t)seed->src->data_size;
    } else {
        uint32_t pixel_bytes = 0u;

        (void)demo_lvgl_photo_fs_seed_rgb565_size(seed->width, seed->height, &pixel_bytes);
        out_item->cf = LV_IMG_CF_TRUE_COLOR;
        out_item->file_size = (uint32_t)(sizeof(demo_lvgl_photo_fs_file_header_t) +
                                         pixel_bytes);
    }
    (void)demo_lvgl_photo_fs_store_build_path(seed->id,
                                              seed->format,
                                              out_item->local_path,
                                              sizeof(out_item->local_path));
}

static int demo_lvgl_photo_fs_seed_read_raw_file(const demo_lvgl_photo_fs_item_t *item,
                                                 uint8_t **out_buf,
                                                 unsigned int *out_size)
{
    liot_stat_ext_s st;
    LFILE_EXT fd;
    uint8_t *buf = NULL;
    int read_len;
    int ret;

    if ((item == NULL) || (out_buf == NULL) || (out_size == NULL) ||
        (item->local_path[0] == '\0') || (item->file_size == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    *out_buf = NULL;
    *out_size = 0u;

    (void)memset(&st, 0, sizeof(st));
    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = liot_stat_ext(item->local_path, &st);
    if ((ret != LIOT_EXTFLASH_OK) ||
        (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size != item->file_size) ||
        (st.size == 0u)) {
        demo_lvgl_photo_fs_store_access_end();
        return (ret == LIOT_EXTFLASH_OK) ? LIOT_EXTFLASH_SIZE_FAIL : ret;
    }

    buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
    if (buf == NULL) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(item->local_path, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, (unsigned int)st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }

    *out_buf = buf;
    *out_size = (unsigned int)st.size;
    return 0;
}

static bool demo_lvgl_photo_fs_seed_local_matches(const demo_lvgl_photo_fs_item_t *item)
{
    uint8_t *buf = NULL;
    unsigned int buf_size = 0u;
    uint32_t crc;
    int ret;

    if ((item == NULL) || (item->local_path[0] == '\0')) {
        return false;
    }

    if (item->format == DEMO_LVGL_PHOTO_FS_FORMAT_GIF) {
#if DEMO_LVGL_PHOTO_FS_ENABLE_GIF
        GIF_INFO info;
        void *decoder;

        if ((item->cf != LV_IMG_CF_RAW) || (item->file_size < 6u)) {
            return false;
        }

        ret = demo_lvgl_photo_fs_seed_read_raw_file(item, &buf, &buf_size);
        if (ret != 0) {
            return false;
        }
        if ((memcmp(buf, "GIF87a", 6u) != 0) && (memcmp(buf, "GIF89a", 6u) != 0)) {
            liot_rtos_free(buf);
            return false;
        }

        crc = demo_lvgl_photo_fs_store_crc32_update(0xFFFFFFFFu, buf, buf_size);
        crc = demo_lvgl_photo_fs_store_crc32_finish(crc);
        if (crc != item->crc32) {
            liot_rtos_free(buf);
            return false;
        }

        decoder = GifD_Create();
        if (decoder == NULL) {
            liot_rtos_free(buf);
            return false;
        }

        ret = GifD_DecodeInfo(decoder, buf, buf_size, &info);
        GifD_Destroy(decoder);
        liot_rtos_free(buf);
        if ((ret != 0) || (info.uWidth != item->width) || (info.uHeight != item->height)) {
            return false;
        }

        return true;
#else
        return false;
#endif
    }

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
    if (item->format == DEMO_LVGL_PHOTO_FS_FORMAT_JPEG) {
        uint16_t decoded_w = 0u;
        uint16_t decoded_h = 0u;

        if ((item->cf != LV_IMG_CF_RAW) || (item->file_size < 4u)) {
            return false;
        }

        ret = demo_lvgl_photo_fs_seed_read_raw_file(item, &buf, &buf_size);
        if (ret != 0) {
            return false;
        }
        if ((buf[0] != 0xFFu) || (buf[1] != 0xD8u)) {
            liot_rtos_free(buf);
            return false;
        }

        crc = demo_lvgl_photo_fs_store_crc32_update(0xFFFFFFFFu, buf, buf_size);
        crc = demo_lvgl_photo_fs_store_crc32_finish(crc);
        if (crc != item->crc32) {
            liot_rtos_free(buf);
            return false;
        }

        ret = demo_lvgl_photo_fs_seed_jpeg_decode_info(buf, buf_size, &decoded_w, &decoded_h) ? 0 : -1;
        liot_rtos_free(buf);
        return (ret == 0) &&
               (decoded_w == item->width) &&
               (decoded_h == item->height);
    }
#endif

    {
        demo_lvgl_photo_fs_file_header_t header;
        uint32_t pixel_bytes = 0u;

        if (!demo_lvgl_photo_fs_seed_rgb565_size(item->width, item->height, &pixel_bytes) ||
            (item->file_size != (uint32_t)(sizeof(demo_lvgl_photo_fs_file_header_t) + pixel_bytes)) ||
            (item->cf != LV_IMG_CF_TRUE_COLOR)) {
            return false;
        }

        if (demo_lvgl_photo_fs_store_read_photo_header(item->id, &header) != 0) {
            return false;
        }

        return (header.magic == DEMO_LVGL_PHOTO_FS_MAGIC) &&
               (header.width == item->width) &&
               (header.height == item->height) &&
               (header.cf == LV_IMG_CF_TRUE_COLOR) &&
               (header.data_size == pixel_bytes) &&
               (header.crc32 == item->crc32);
    }
}

static int demo_lvgl_photo_fs_seed_write_one(const demo_lvgl_photo_fs_seed_item_t *seed,
                                             uint32_t crc)
{
    demo_lvgl_photo_fs_file_header_t header;
    uint8_t *buf;
    unsigned int total_len;
    uint32_t pixel_bytes = 0u;
    int ret;

    if ((seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_GIF) ||
        (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_JPEG) ||
        (seed->format == DEMO_LVGL_PHOTO_FS_FORMAT_PNG)) {
        return demo_lvgl_photo_fs_store_write_raw_file(seed->id,
                                                       seed->format,
                                                       seed->src->data,
                                                       (unsigned int)seed->src->data_size);
    }

    if (!demo_lvgl_photo_fs_seed_rgb565_size(seed->width, seed->height, &pixel_bytes)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    total_len = (unsigned int)(sizeof(header) + pixel_bytes);
    buf = (uint8_t *)liot_rtos_malloc(total_len);
    if (buf == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    (void)memset(&header, 0, sizeof(header));
    header.magic = DEMO_LVGL_PHOTO_FS_MAGIC;
    header.width = seed->width;
    header.height = seed->height;
    header.cf = LV_IMG_CF_TRUE_COLOR;
    header.data_size = pixel_bytes;
    header.crc32 = crc;

    (void)memcpy(buf, &header, sizeof(header));
    (void)memcpy(buf + sizeof(header), seed->src->data, header.data_size);

    ret = demo_lvgl_photo_fs_store_write_photo_file(seed->id, buf, total_len);
    liot_rtos_free(buf);
    return ret;
}

int demo_lvgl_photo_fs_seed_run(bool force_reseed,
                                demo_lvgl_photo_fs_seed_result_t *out_result)
{
    demo_lvgl_photo_fs_seed_result_t result;
    demo_lvgl_photo_fs_item_t *local_items = NULL;
    demo_lvgl_photo_fs_item_t *result_items = NULL;
    unsigned int local_count = 0u;
    unsigned int result_count = 0u;
    unsigned int i;
    int ret = 0;

    (void)memset(&result, 0, sizeof(result));
    result.total = (uint16_t)DEMO_LVGL_PHOTO_FS_SEED_COUNT;

    local_items = (demo_lvgl_photo_fs_item_t *)liot_rtos_malloc(sizeof(*local_items) *
                                                                DEMO_LVGL_PHOTO_FS_MAX_ITEMS);
    result_items = (demo_lvgl_photo_fs_item_t *)liot_rtos_malloc(sizeof(*result_items) *
                                                                  DEMO_LVGL_PHOTO_FS_MAX_ITEMS);
    if ((local_items == NULL) || (result_items == NULL)) {
        result.failed = (uint16_t)DEMO_LVGL_PHOTO_FS_SEED_COUNT;
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }

    (void)memset(local_items, 0, sizeof(*local_items) * DEMO_LVGL_PHOTO_FS_MAX_ITEMS);
    (void)memset(result_items, 0, sizeof(*result_items) * DEMO_LVGL_PHOTO_FS_MAX_ITEMS);

    ret = demo_lvgl_photo_fs_store_mount();
    if (ret != 0) {
        result.failed = (uint16_t)DEMO_LVGL_PHOTO_FS_SEED_COUNT;
        goto cleanup;
    }

    ret = demo_lvgl_photo_fs_store_load_index(local_items,
                                              DEMO_LVGL_PHOTO_FS_MAX_ITEMS,
                                              &local_count);
    if (ret != 0) {
        local_count = 0u;
    }

    for (i = 0; i < local_count; ++i) {
        if (!demo_lvgl_photo_fs_seed_is_builtin_id(local_items[i].id)) {
            if (result_count >= (DEMO_LVGL_PHOTO_FS_MAX_ITEMS - DEMO_LVGL_PHOTO_FS_SEED_COUNT)) {
                result.failed = (uint16_t)DEMO_LVGL_PHOTO_FS_SEED_COUNT;
                ret = LIOT_EXTFLASH_NO_SPACE;
                goto cleanup;
            }
            result_items[result_count] = local_items[i];
            ++result_count;
        }
    }

    for (i = 0; i < DEMO_LVGL_PHOTO_FS_SEED_COUNT; ++i) {
        const demo_lvgl_photo_fs_seed_item_t *seed;
        demo_lvgl_photo_fs_item_t item;
        uint32_t crc;
        int local_idx;

        seed = &g_demo_lvgl_photo_fs_seed_items[i];
        (void)memset(&item, 0, sizeof(item));
        if (!demo_lvgl_photo_fs_seed_src_is_valid(seed)) {
            ++result.failed;
            continue;
        }

        crc = demo_lvgl_photo_fs_seed_crc32(seed->src);
        demo_lvgl_photo_fs_seed_fill_item(seed, crc, &item);

        local_idx = demo_lvgl_photo_fs_seed_find_item(local_items, local_count, seed->id);
        if (!force_reseed &&
            (local_idx >= 0) &&
            (local_items[local_idx].format == item.format) &&
            (local_items[local_idx].width == item.width) &&
            (local_items[local_idx].height == item.height) &&
            (local_items[local_idx].crc32 == item.crc32) &&
            (local_items[local_idx].file_size == item.file_size) &&
            demo_lvgl_photo_fs_seed_local_matches(&local_items[local_idx])) {
            if (result_count < DEMO_LVGL_PHOTO_FS_MAX_ITEMS) {
                result_items[result_count] = local_items[local_idx];
                ++result_count;
                ++result.skipped;
            } else {
                ++result.failed;
            }
            continue;
        }

        ret = demo_lvgl_photo_fs_seed_write_one(seed, crc);
        if (ret == 0) {
            if (result_count < DEMO_LVGL_PHOTO_FS_MAX_ITEMS) {
                result_items[result_count] = item;
                ++result_count;
                ++result.written;
            } else {
                ++result.failed;
            }
        } else {
            ++result.failed;
        }
    }

    ret = demo_lvgl_photo_fs_store_save_index(result_items, result_count);
    if (ret != 0) {
        ++result.failed;
    }

cleanup:
    if (local_items != NULL) {
        liot_rtos_free(local_items);
    }
    if (result_items != NULL) {
        liot_rtos_free(result_items);
    }
    if (out_result != NULL) {
        *out_result = result;
    }

    return ret;
}
