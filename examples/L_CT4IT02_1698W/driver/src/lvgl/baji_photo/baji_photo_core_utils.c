#include "baji_photo_core_utils.h"

#include <stdio.h>
#include <string.h>

bool baji_photo_true_color_dims_valid(uint32_t width, uint32_t height)
{
    return (width != 0u) &&
           (height != 0u) &&
           (width <= BAJI_PHOTO_IMG_W) &&
           (height <= BAJI_PHOTO_IMG_H);
}

uint32_t baji_photo_true_color_data_size(uint32_t width, uint32_t height)
{
    return width * height * BAJI_PHOTO_IMG_BPP;
}

const char *baji_photo_skip_ws(const char *p, const char *end)
{
    while ((p < end) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))) {
        ++p;
    }
    return p;
}

const char *baji_photo_find_json_key(const char *start, const char *end, const char *key)
{
    char pattern[48];
    unsigned int key_len;
    int written;
    const char *p;

    key_len = (unsigned int)strlen(key);
    if ((key_len + 3u) > sizeof(pattern)) {
        return NULL;
    }

    written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0) {
        return NULL;
    }

    p = start;
    while ((p < end) && ((end - p) >= written)) {
        if (memcmp(p, pattern, (unsigned int)written) == 0) {
            p += written;
            p = baji_photo_skip_ws(p, end);
            if ((p < end) && (*p == ':')) {
                return baji_photo_skip_ws(p + 1, end);
            }
        }
        ++p;
    }

    return NULL;
}

int baji_photo_parse_json_string(const char *obj_start,
                                 const char *obj_end,
                                 const char *key,
                                 char *out,
                                 unsigned int out_len)
{
    const char *p;
    unsigned int len = 0;

    if ((out == NULL) || (out_len == 0u)) {
        return -1;
    }
    out[0] = '\0';

    p = baji_photo_find_json_key(obj_start, obj_end, key);
    if ((p == NULL) || (p >= obj_end) || (*p != '"')) {
        return -1;
    }
    ++p;

    while (p < obj_end) {
        char c = *p++;

        if (c == '"') {
            out[len] = '\0';
            return 0;
        }
        if (c == '\\') {
            if (p >= obj_end) {
                return -1;
            }
            c = *p++;
        }
        if ((len + 1u) >= out_len) {
            return -1;
        }
        out[len++] = c;
    }

    return -1;
}

int baji_photo_parse_json_u32(const char *obj_start,
                              const char *obj_end,
                              const char *key,
                              uint32_t *out)
{
    const char *p;
    uint32_t value = 0;
    bool any = false;

    if (out == NULL) {
        return -1;
    }

    p = baji_photo_find_json_key(obj_start, obj_end, key);
    if (p == NULL) {
        return -1;
    }
    p = baji_photo_skip_ws(p, obj_end);

    while ((p < obj_end) && (*p >= '0') && (*p <= '9')) {
        value = (value * 10u) + (uint32_t)(*p - '0');
        any = true;
        ++p;
    }

    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

int baji_photo_parse_json_hex_u32(const char *obj_start,
                                  const char *obj_end,
                                  const char *key,
                                  uint32_t *out)
{
    char hex[16];
    const char *p;
    const char *parse_end;
    uint32_t value = 0;
    bool any = false;

    if (out == NULL) {
        return -1;
    }

    if (baji_photo_parse_json_string(obj_start, obj_end, key, hex, sizeof(hex)) == 0) {
        p = hex;
        parse_end = hex + strlen(hex);
    } else {
        p = baji_photo_find_json_key(obj_start, obj_end, key);
        if (p == NULL) {
            return -1;
        }
        parse_end = obj_end;
    }

    if (((p + 2) < parse_end) && (p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X'))) {
        p += 2;
    }

    while (p < parse_end) {
        char c = *p;
        uint32_t digit;

        if ((c >= '0') && (c <= '9')) {
            digit = (uint32_t)(c - '0');
        } else if ((c >= 'a') && (c <= 'f')) {
            digit = (uint32_t)(c - 'a' + 10);
        } else if ((c >= 'A') && (c <= 'F')) {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            break;
        }

        value = (value << 4) | digit;
        any = true;
        ++p;
    }

    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

int baji_photo_parse_manifest_format(const char *obj_start,
                                     const char *obj_end,
                                     baji_photo_format_t *out)
{
    char format[16];

    if (out == NULL) {
        return -1;
    }

    if (baji_photo_parse_json_string(obj_start, obj_end, "format", format, sizeof(format)) != 0) {
        *out = BAJI_PHOTO_FORMAT_BJP;
        return 0;
    }

    if ((strcmp(format, "gif") == 0) || (strcmp(format, "GIF") == 0)) {
#if BAJI_PHOTO_ENABLE_GIF_SUPPORT
        *out = BAJI_PHOTO_FORMAT_GIF;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "jpeg") == 0) || (strcmp(format, "jpg") == 0) ||
        (strcmp(format, "JPEG") == 0) || (strcmp(format, "JPG") == 0)) {
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        *out = BAJI_PHOTO_FORMAT_JPEG;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "png") == 0) || (strcmp(format, "PNG") == 0)) {
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        *out = BAJI_PHOTO_FORMAT_PNG;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "bjp") == 0) ||
        (strcmp(format, "BJP") == 0) ||
        (strcmp(format, "rgb565") == 0) ||
        (strcmp(format, "RGB565") == 0)) {
        *out = BAJI_PHOTO_FORMAT_BJP;
        return 0;
    }

    return -1;
}

int baji_photo_manifest_id_valid(const char *id)
{
    unsigned int i;

    if ((id == NULL) || (id[0] == '\0')) {
        return -1;
    }

    for (i = 0; id[i] != '\0'; ++i) {
        char c = id[i];

        if (i >= BAJI_PHOTO_ID_MAX_LEN) {
            return -1;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') ||
            (c == '-')) {
            continue;
        }
        return -1;
    }

    return 0;
}

int baji_photo_manifest_remote_path_valid(const char *path)
{
    unsigned int i;

    if ((path == NULL) || (path[0] == '\0') || (path[0] == '/')) {
        return -1;
    }
    if (strstr(path, "..") != NULL) {
        return -1;
    }

    for (i = 0; path[i] != '\0'; ++i) {
        char c = path[i];

        if (i >= BAJI_PHOTO_PATH_MAX_LEN) {
            return -1;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') ||
            (c == '-') ||
            (c == '.') ||
            (c == '/')) {
            continue;
        }
        return -1;
    }

    return 0;
}
