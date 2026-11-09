#ifndef DEMO_LVGL_PHOTO_LFS_CORE_H
#define DEMO_LVGL_PHOTO_LFS_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Pure store/seed logic extracted from demo_lvgl_photo_lfs_store.c and
 * demo_lvgl_photo_lfs_seed.c. Header-only on purpose: the production call sites
 * keep identical behavior with no lvgl / liot / filesystem dependency, so the
 * same source is host-testable as-is. Directory names are local macros that
 * mirror BAJI_PHOTO_STORE_* (baji_photo_config.h).
 */

#define DEMO_LVGL_PHOTO_LFS_CORE_PHOTO_DIR "/flash/baji/photos"
#define DEMO_LVGL_PHOTO_LFS_CORE_GIF_DIR "/flash/baji/gifs"
#define DEMO_LVGL_PHOTO_LFS_CORE_JPEG_DIR "/flash/baji/jpeg"
#define DEMO_LVGL_PHOTO_LFS_CORE_PNG_DIR "/flash/baji/png"
#define DEMO_LVGL_PHOTO_LFS_CORE_TMP_DIR "/flash/baji/tmp"
#define DEMO_LVGL_PHOTO_LFS_CORE_ID_MAX_LEN 32u
#define DEMO_LVGL_PHOTO_LFS_CORE_IMG_BPP 2u
#define DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER (-1)

typedef enum {
    DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP = 0,
    DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF = 1,
    DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_JPEG = 2,
    DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_PNG = 3
} demo_lvgl_photo_lfs_core_format_t;

/*
 * Map a format enum to its storage directory and path prefix/suffix.
 * Mirrors demo_lvgl_photo_lfs_format_name + the path selection in
 * demo_lvgl_photo_lfs_store_build_path.
 */
static inline const char *demo_lvgl_photo_lfs_core_format_name(
    demo_lvgl_photo_lfs_core_format_t format)
{
    switch (format) {
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF:
        return "gif";
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_JPEG:
        return "jpeg";
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_PNG:
        return "png";
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP:
    default:
        return "bjp";
    }
}

/*
 * Map a storage format name back to a format enum.
 * Mirrors demo_lvgl_photo_lfs_format_from_name.
 */
static inline demo_lvgl_photo_lfs_core_format_t
demo_lvgl_photo_lfs_core_format_from_name(const char *name)
{
    if (name == NULL) {
        return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP;
    }
    if ((strcmp(name, "gif") == 0) || (strcmp(name, "GIF") == 0)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF;
    }
    if ((strcmp(name, "rgb565") == 0) || (strcmp(name, "RGB565") == 0) ||
        (strcmp(name, "raw_rgb565") == 0) ||
        (strcmp(name, "RAW_RGB565") == 0)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP;
    }
    if ((strcmp(name, "jpeg") == 0) || (strcmp(name, "jpg") == 0) ||
        (strcmp(name, "JPEG") == 0) || (strcmp(name, "JPG") == 0)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_JPEG;
    }
    if ((strcmp(name, "png") == 0) || (strcmp(name, "PNG") == 0)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_PNG;
    }
    return DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP;
}

/*
 * Validate an item id: non-empty, <= 32 chars, [a-zA-Z0-9_-].
 * Mirrors demo_lvgl_photo_lfs_store_id_is_valid.
 */
static inline bool demo_lvgl_photo_lfs_core_id_is_valid(const char *id)
{
    unsigned int i;

    if ((id == NULL) || (id[0] == '\0')) {
        return false;
    }

    for (i = 0; id[i] != '\0'; ++i) {
        char c = id[i];

        if (i >= DEMO_LVGL_PHOTO_LFS_CORE_ID_MAX_LEN) {
            return false;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') || (c == '-')) {
            continue;
        }
        return false;
    }

    return true;
}

/*
 * Build the final storage path for an item, e.g.
 * /flash/baji/photos/photo_<id>.bjp or /flash/baji/gifs/<id>.gif.
 * Mirrors demo_lvgl_photo_lfs_store_build_path (returns -1 on invalid input
 * or truncation).
 */
static inline int demo_lvgl_photo_lfs_core_build_path(
    const char *id,
    demo_lvgl_photo_lfs_core_format_t format,
    char *out_path,
    unsigned int out_len)
{
    const char *dir = DEMO_LVGL_PHOTO_LFS_CORE_PHOTO_DIR;
    const char *prefix = "photo_";
    const char *suffix = ".bjp";
    int written;

    if (!demo_lvgl_photo_lfs_core_id_is_valid(id) || (out_path == NULL) ||
        (out_len == 0u)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER;
    }

    switch (format) {
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF:
        dir = DEMO_LVGL_PHOTO_LFS_CORE_GIF_DIR;
        prefix = "";
        suffix = ".gif";
        break;
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_JPEG:
        dir = DEMO_LVGL_PHOTO_LFS_CORE_JPEG_DIR;
        prefix = "";
        suffix = ".jpg";
        break;
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_PNG:
        dir = DEMO_LVGL_PHOTO_LFS_CORE_PNG_DIR;
        prefix = "";
        suffix = ".png";
        break;
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP:
    default:
        dir = DEMO_LVGL_PHOTO_LFS_CORE_PHOTO_DIR;
        prefix = "photo_";
        suffix = ".bjp";
        break;
    }

    written = snprintf(out_path, out_len, "%s/%s%s%s", dir, prefix, id, suffix);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER;
    }

    return 0;
}

/*
 * Build the temporary download path for an item, e.g.
 * /flash/baji/tmp/photo_<id>.tmp or /flash/baji/tmp/jpeg_<id>.tmp.
 * Mirrors demo_lvgl_photo_lfs_store_build_tmp_path.
 */
static inline int demo_lvgl_photo_lfs_core_build_tmp_path(
    const char *id,
    demo_lvgl_photo_lfs_core_format_t format,
    char *out_path,
    unsigned int out_len)
{
    const char *base = "photo";
    int written;

    if (!demo_lvgl_photo_lfs_core_id_is_valid(id) || (out_path == NULL) ||
        (out_len == 0u)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER;
    }

    switch (format) {
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_JPEG:
        base = "jpeg";
        break;
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_PNG:
        base = "png";
        break;
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF:
    case DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP:
    default:
        base = "photo";
        break;
    }

    written = snprintf(out_path, out_len, "%s/%s_%s.tmp",
                       DEMO_LVGL_PHOTO_LFS_CORE_TMP_DIR, base, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER;
    }

    return 0;
}

/*
 * Standard CRC-32 (IEEE 802.3), init with 0xFFFFFFFF and finish with a final
 * XOR. Mirrors demo_lvgl_photo_lfs_store_crc32_update/finish.
 */
static inline uint32_t demo_lvgl_photo_lfs_core_crc32_update(uint32_t crc,
                                                            const void *data,
                                                            unsigned int len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    unsigned int i;
    unsigned int bit;

    if ((bytes == NULL) && (len != 0u)) {
        return crc;
    }

    for (i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static inline uint32_t demo_lvgl_photo_lfs_core_crc32_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

/*
 * Compute the rgb565 pixel byte count with overflow checks.
 * Mirrors demo_lvgl_photo_lfs_seed_rgb565_size.
 */
static inline bool demo_lvgl_photo_lfs_core_rgb565_size(uint16_t width,
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
    if (pixels > (UINT32_MAX / DEMO_LVGL_PHOTO_LFS_CORE_IMG_BPP)) {
        return false;
    }

    *out_size = pixels * DEMO_LVGL_PHOTO_LFS_CORE_IMG_BPP;
    return true;
}

/*
 * Whether an id belongs to the built-in seed set ("builtin_" prefix).
 * Mirrors demo_lvgl_photo_lfs_seed_is_builtin_id.
 */
static inline bool demo_lvgl_photo_lfs_core_is_builtin_id(const char *id)
{
    return (id != NULL) &&
           (strncmp(id, "builtin_", 8u) == 0);
}

/*
 * Find the index of an item by id; returns -1 when not found.
 * Mirrors demo_lvgl_photo_lfs_seed_find_item. The id field must be at the
 * start of the item record, matching demo_lvgl_photo_lfs_item_t.
 */
static inline int demo_lvgl_photo_lfs_core_find_item(
    const void *items,
    unsigned int count,
    size_t item_stride,
    const char *id)
{
    unsigned int i;

    if ((items == NULL) || (id == NULL) || (item_stride == 0u)) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        const char *item_id = (const char *)items + (size_t)i * item_stride;

        if (strcmp(item_id, id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

#endif /* DEMO_LVGL_PHOTO_LFS_CORE_H */
