#include "baji_photo_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

#include "baji_photo_diag.h"

#define BAJI_PHOTO_FLASH_SPI_PORT          1u
#define BAJI_PHOTO_FLASH_BASE_ADDR         0x000000u
#define BAJI_PHOTO_FLASH_TOTAL_SIZE        0x800000u

#define BAJI_PHOTO_LDO33_STABLE_DELAY_MS   10u
#define BAJI_PHOTO_VCC3V3_STABLE_DELAY_MS  20u

#define BAJI_PHOTO_LDO33_EN_PAD            106
#define BAJI_PHOTO_LDO33_EN_GPIO           L_GPIO_25
#define BAJI_PHOTO_VCC3V3_EN_PAD           16
#define BAJI_PHOTO_VCC3V3_EN_GPIO          L_GPIO_27

#define BAJI_PHOTO_SPI_MOSI_PAD            63
#define BAJI_PHOTO_SPI_MISO_PAD            62
#define BAJI_PHOTO_SPI_SCLK_PAD            49
#define BAJI_PHOTO_SPI_CS_PAD              64
#define BAJI_PHOTO_SPI_CS_GPIO             L_GPIO_12
#define BAJI_PHOTO_SPI_PIN_FUNC            L_PIN_FUNC_1
#define BAJI_PHOTO_STORE_TRACE(fmt, ...) liot_trace("[baji_store] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)

static const liot_ext_flash_cfg_t g_baji_photo_flash_cfg = {
    .spi_port = BAJI_PHOTO_FLASH_SPI_PORT,
    .base_addr = BAJI_PHOTO_FLASH_BASE_ADDR,
    .total_size = BAJI_PHOTO_FLASH_TOTAL_SIZE,
};

static const liot_ext_fs_cfg_t g_baji_photo_fs_cfg = {
    .base_addr = BAJI_FLASH_LFS_BASE,
    .total_size = BAJI_FLASH_LFS_TOTAL,
    .block_size = BAJI_FLASH_LFS_BLOCK,
    .read_size = BAJI_FLASH_LFS_READ,
    .prog_size = BAJI_FLASH_LFS_PROG,
};

static bool g_baji_photo_store_ready;
static liot_mutex_t g_baji_photo_store_mutex;

typedef struct {
    char id[BAJI_PHOTO_ID_MAX_LEN + 1u];
    baji_photo_format_t format;
    uint32_t crc32;
    uint32_t file_size;
    uint32_t txn_id;
} baji_photo_store_delete_journal_t;

static int baji_photo_store_parse_u32(const char *start,
                                      const char *end,
                                      const char *key,
                                      uint32_t *out);
static int baji_photo_store_parse_u64(const char *start,
                                      const char *end,
                                      const char *key,
                                      uint64_t *out);
static int baji_photo_store_save_device_meta_locked(const baji_photo_device_meta_t *meta);
static int baji_photo_store_load_device_meta_locked(baji_photo_device_meta_t *meta);
static int baji_photo_store_load_index_locked(baji_photo_manifest_item_t *items,
                                              unsigned int max_items,
                                              unsigned int *out_count);
static int baji_photo_store_save_index_locked(const baji_photo_manifest_item_t *items,
                                              unsigned int count);
static int baji_photo_store_load_delete_journal_locked(
    baji_photo_store_delete_journal_t *journal);
static int baji_photo_store_save_delete_journal_locked(
    const baji_photo_store_delete_journal_t *journal);
static int baji_photo_store_clear_delete_journal_locked(void);
static int baji_photo_store_recover_index_locked(void);
static void baji_photo_store_recover_delete_journal_locked(void);
static bool baji_photo_store_file_content_matches_locked(const char *path,
                                                         const void *data,
                                                         unsigned int data_len);
static int baji_photo_store_write_final_file_locked(const char *path,
                                                    const void *data,
                                                    unsigned int data_len);

static const char *baji_photo_store_format_name(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_GIF:
        return "gif";
    case BAJI_PHOTO_FORMAT_JPEG:
        return "jpeg";
    case BAJI_PHOTO_FORMAT_PNG:
        return "png";
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        return "bjp";
    }
}

static uint32_t baji_photo_store_max_compressed_size(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_JPEG:
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        return BAJI_PHOTO_JPEG_MAX_COMPRESSED_SIZE;
#else
        return 0u;
#endif
    case BAJI_PHOTO_FORMAT_PNG:
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        return BAJI_PHOTO_PNG_MAX_COMPRESSED_SIZE;
#else
        return 0u;
#endif
    default:
        return 0u;
    }
}

static bool baji_photo_store_item_id_matches(const baji_photo_manifest_item_t *item,
                                             const char *id)
{
    return (item != NULL) && (id != NULL) && (strcmp(item->id, id) == 0);
}

static bool baji_photo_store_item_matches(const baji_photo_manifest_item_t *item,
                                          const char *id,
                                          baji_photo_format_t format)
{
    return baji_photo_store_item_id_matches(item, id) && (item->format == format);
}

static int baji_photo_store_find_item(const baji_photo_manifest_item_t *items,
                                      unsigned int count,
                                      const char *id,
                                      baji_photo_format_t format)
{
    unsigned int i;

    if ((items == NULL) || (id == NULL)) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        if (baji_photo_store_item_matches(&items[i], id, format)) {
            return (int)i;
        }
    }

    return -1;
}

static void baji_photo_store_marker_index_reject(const char *id,
                                                 const char *format_name,
                                                 unsigned long file_size,
                                                 unsigned long expected_size,
                                                 unsigned long width,
                                                 unsigned long height,
                                                 const char *reason)
{
    BAJI_PHOTO_MARK_TRACE("STORE_INDEX_REJECT image=%s kind=%s size=%lu expect=%lu dims=%lux%lu reason=%s",
                          baji_photo_diag_id_tail(id),
                          (format_name != NULL) ? format_name : "-",
                          file_size,
                          expected_size,
                          width,
                          height,
                          (reason != NULL) ? reason : "-");
}

static void baji_photo_store_marker_index_load(unsigned int accepted_count,
                                               unsigned int rejected_count,
                                               unsigned int seen_count,
                                               unsigned int max_items)
{
    BAJI_PHOTO_MARK_TRACE("STORE_INDEX_LOAD accepted=%u rejected=%u seen=%u cap=%u",
                          accepted_count,
                          rejected_count,
                          seen_count,
                          max_items);
}

static int baji_photo_store_mutex_ensure(void)
{
    liot_mutex_t mutex = NULL;
    liot_mutex_t extra_mutex = NULL;
    int ret;

    if (g_baji_photo_store_mutex != NULL) {
        return 0;
    }

    ret = liot_rtos_mutex_create(&mutex);
    if (ret != 0) {
        return ret;
    }

    liot_rtos_enter_critical();
    if (g_baji_photo_store_mutex == NULL) {
        g_baji_photo_store_mutex = mutex;
        mutex = NULL;
    } else {
        extra_mutex = mutex;
        mutex = NULL;
    }
    liot_rtos_exit_critical();

    if (extra_mutex != NULL) {
        (void)liot_rtos_mutex_delete(extra_mutex);
    }

    return 0;
}

static int baji_photo_store_lock(void)
{
    int ret;

    ret = baji_photo_store_mutex_ensure();
    if (ret != 0) {
        return ret;
    }

    return liot_rtos_mutex_lock(g_baji_photo_store_mutex, LIOT_WAIT_FOREVER);
}

void baji_photo_store_access_end(void)
{
    if (g_baji_photo_store_mutex != NULL) {
        (void)liot_rtos_mutex_unlock(g_baji_photo_store_mutex);
    }
}

static bool baji_photo_store_id_is_valid(const char *id)
{
    unsigned int i;

    if ((id == 0) || (id[0] == '\0')) {
        return false;
    }

    for (i = 0; id[i] != '\0'; ++i) {
        char c = id[i];

        if (i >= BAJI_PHOTO_ID_MAX_LEN) {
            return false;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') ||
            (c == '-')) {
            continue;
        }
        return false;
    }

    return true;
}

int baji_photo_store_build_tmp_path(const char *id, char *out_path, unsigned int out_len)
{
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    written = snprintf(out_path, out_len, "%s/photo_%s.tmp", BAJI_PHOTO_STORE_TMP_DIR, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

static int baji_photo_store_build_tmp_path_for_format(const char *id,
                                                      baji_photo_format_t format,
                                                      char *out_path,
                                                      unsigned int out_len)
{
    const char *base = "photo";
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    switch (format) {
    case BAJI_PHOTO_FORMAT_GIF:
        base = "gif";
        break;
    case BAJI_PHOTO_FORMAT_JPEG:
        base = "jpeg";
        break;
    case BAJI_PHOTO_FORMAT_PNG:
        base = "png";
        break;
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        base = "photo";
        break;
    }

    written = snprintf(out_path, out_len, "%s/%s_%s.tmp", BAJI_PHOTO_STORE_TMP_DIR, base, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

static int baji_photo_store_parse_u8(const char *start,
                                     const char *end,
                                     const char *key,
                                     uint8_t *out)
{
    uint32_t value = 0u;

    if ((out == NULL) || (baji_photo_store_parse_u32(start, end, key, &value) != 0) ||
        (value > 255u)) {
        return -1;
    }

    *out = (uint8_t)value;
    return 0;
}

static int baji_photo_store_parse_u32_optional(const char *start,
                                               const char *end,
                                               const char *key,
                                               uint32_t default_value,
                                               uint32_t *out)
{
    if (out == NULL) {
        return -1;
    }

    *out = default_value;
    if (baji_photo_store_parse_u32(start, end, key, out) != 0) {
        *out = default_value;
    }
    return 0;
}

static int baji_photo_store_parse_u64_optional(const char *start,
                                               const char *end,
                                               const char *key,
                                               uint64_t default_value,
                                               uint64_t *out)
{
    if (out == NULL) {
        return -1;
    }

    *out = default_value;
    if (baji_photo_store_parse_u64(start, end, key, out) != 0) {
        *out = default_value;
    }
    return 0;
}

static int baji_photo_store_parse_u8_optional(const char *start,
                                              const char *end,
                                              const char *key,
                                              uint8_t default_value,
                                              uint8_t *out)
{
    uint32_t value = default_value;

    if (out == NULL) {
        return -1;
    }

    if ((baji_photo_store_parse_u32(start, end, key, &value) != 0) || (value > 255u)) {
        value = default_value;
    }
    *out = (uint8_t)value;
    return 0;
}

static const char *baji_photo_store_find_key(const char *start, const char *end, const char *key)
{
    char pattern[48];
    int written;
    const char *p;

    written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0) {
        return NULL;
    }

    p = start;
    while ((p < end) && ((end - p) >= written)) {
        if (memcmp(p, pattern, (unsigned int)written) == 0) {
            p += written;
            while ((p < end) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))) {
                ++p;
            }
            if ((p < end) && (*p == ':')) {
                ++p;
                while ((p < end) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))) {
                    ++p;
                }
                return p;
            }
        }
        ++p;
    }

    return NULL;
}

static int baji_photo_store_parse_string(const char *start,
                                         const char *end,
                                         const char *key,
                                         char *out,
                                         unsigned int out_len)
{
    const char *p;
    unsigned int len = 0;

    if ((out == 0) || (out_len == 0u)) {
        return -1;
    }
    out[0] = '\0';

    p = baji_photo_store_find_key(start, end, key);
    if ((p == NULL) || (p >= end) || (*p != '"')) {
        return -1;
    }
    ++p;

    while (p < end) {
        char c = *p++;

        if (c == '"') {
            out[len] = '\0';
            return 0;
        }
        if (c == '\\') {
            if (p >= end) {
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

static int baji_photo_store_parse_u32(const char *start,
                                      const char *end,
                                      const char *key,
                                      uint32_t *out)
{
    const char *p;
    uint32_t value = 0;
    bool any = false;

    if (out == 0) {
        return -1;
    }

    p = baji_photo_store_find_key(start, end, key);
    if (p == NULL) {
        return -1;
    }

    while ((p < end) && (*p >= '0') && (*p <= '9')) {
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

static int baji_photo_store_parse_u64(const char *start,
                                      const char *end,
                                      const char *key,
                                      uint64_t *out)
{
    const char *p;
    uint64_t value = 0u;
    bool any = false;

    if (out == 0) {
        return -1;
    }

    p = baji_photo_store_find_key(start, end, key);
    if (p == NULL) {
        return -1;
    }

    while ((p < end) && (*p >= '0') && (*p <= '9')) {
        value = (value * 10u) + (uint64_t)(*p - '0');
        any = true;
        ++p;
    }
    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

static int baji_photo_store_parse_hex_u32(const char *start,
                                          const char *end,
                                          const char *key,
                                          uint32_t *out)
{
    char hex[16];
    const char *p;
    uint32_t value = 0;
    bool any = false;

    if (baji_photo_store_parse_string(start, end, key, hex, sizeof(hex)) != 0) {
        return -1;
    }

    p = hex;
    if ((p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X'))) {
        p += 2;
    }
    while (*p != '\0') {
        char c = *p++;
        uint32_t digit;

        if ((c >= '0') && (c <= '9')) {
            digit = (uint32_t)(c - '0');
        } else if ((c >= 'a') && (c <= 'f')) {
            digit = (uint32_t)(c - 'a' + 10);
        } else if ((c >= 'A') && (c <= 'F')) {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            return -1;
        }
        value = (value << 4) | digit;
        any = true;
    }
    if (!any || (out == 0)) {
        return -1;
    }

    *out = value;
    return 0;
}

static int baji_photo_store_parse_format(const char *start,
                                         const char *end,
                                         baji_photo_format_t *out)
{
    char format[16];

    if (out == 0) {
        return -1;
    }

    if (baji_photo_store_parse_string(start, end, "format", format, sizeof(format)) != 0) {
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

static int baji_photo_store_parse_index_item(const char *start,
                                             const char *end,
                                             baji_photo_manifest_item_t *item)
{
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t expected_size = 0u;
    const char *reason = "unknown";
    const char *format_name = "-";
    bool format_parsed = false;

    if (item == 0) {
        return -1;
    }

    memset(item, 0, sizeof(*item));
    if (baji_photo_store_parse_string(start, end, "id", item->id, sizeof(item->id)) != 0) {
        reason = "field:id";
        goto reject;
    }
    if (!baji_photo_store_id_is_valid(item->id)) {
        reason = "id";
        goto reject;
    }
    if (baji_photo_store_parse_string(start, end, "name", item->name, sizeof(item->name)) != 0) {
        reason = "field:name";
        goto reject;
    }
    if (baji_photo_store_parse_string(start, end, "remote_path", item->remote_path, sizeof(item->remote_path)) != 0) {
        reason = "field:remote_path";
        goto reject;
    }
    if (baji_photo_store_parse_string(start, end, "local_path", item->local_path, sizeof(item->local_path)) != 0) {
        reason = "field:local_path";
        goto reject;
    }
    if (baji_photo_store_parse_u32(start, end, "file_size", &item->file_size) != 0) {
        reason = "field:file_size";
        goto reject;
    }
    if (baji_photo_store_parse_hex_u32(start, end, "crc32", &item->crc32) != 0) {
        reason = "field:crc32";
        goto reject;
    }
    if (baji_photo_store_parse_u32(start, end, "width", &width) != 0) {
        reason = "field:width";
        goto reject;
    }
    if (baji_photo_store_parse_u32(start, end, "height", &height) != 0) {
        reason = "field:height";
        goto reject;
    }
    if (baji_photo_store_parse_format(start, end, &item->format) != 0) {
        reason = "field:format";
        goto reject;
    }
    format_parsed = true;
    format_name = baji_photo_store_format_name(item->format);

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        if ((width == 0u) || (height == 0u) || (item->file_size == 0u)) {
            reason = "gif_meta";
            goto reject;
        }
        item->cf = LV_IMG_CF_RAW;
    } else if (item->format == BAJI_PHOTO_FORMAT_BJP) {
        if ((item->file_size < sizeof(baji_photo_file_header_t)) ||
            (item->file_size > (BAJI_PHOTO_IMG_DATA_SIZE + sizeof(baji_photo_file_header_t)))) {
            reason = "file_size";
            goto reject;
        }
        if ((width == 0u) || (height == 0u)) {
            reason = "dims_zero";
            goto reject;
        }
        if ((width > BAJI_PHOTO_IMG_W) || (height > BAJI_PHOTO_IMG_H)) {
            reason = "dims";
            goto reject;
        }
        expected_size = (uint32_t)sizeof(baji_photo_file_header_t) +
                        (width * height * BAJI_PHOTO_IMG_BPP);
        if (item->file_size != expected_size) {
            reason = "payload_size";
            goto reject;
        }
        item->cf = LV_IMG_CF_TRUE_COLOR;
    } else {
        uint32_t max_size = baji_photo_store_max_compressed_size(item->format);

        if (max_size == 0u) {
            reason = "format_disabled";
            goto reject;
        }
        if ((width == 0u) || (height == 0u)) {
            reason = "dims_zero";
            goto reject;
        }
        if ((width > BAJI_PHOTO_IMG_W) || (height > BAJI_PHOTO_IMG_H)) {
            reason = "dims";
            goto reject;
        }
        if ((item->file_size == 0u) || (item->file_size > max_size)) {
            reason = "compressed_size";
            goto reject;
        }
        item->cf = LV_IMG_CF_TRUE_COLOR;
    }

    item->width = (uint16_t)width;
    item->height = (uint16_t)height;
    if (baji_photo_store_build_item_path(item->id,
                                         item->format,
                                         item->local_path,
                                         sizeof(item->local_path)) != 0) {
        reason = "local_path";
        goto reject;
    }
    BAJI_PHOTO_STORE_TRACE("index accept id=%s fmt=%s size=%lu dims=%ux%u",
                           item->id,
                           format_name,
                           (unsigned long)item->file_size,
                           (unsigned int)item->width,
                           (unsigned int)item->height);
    return 0;

reject:
    BAJI_PHOTO_STORE_TRACE("index reject id=%s fmt=%s size=%lu expect=%lu dims=%lux%lu reason=%s",
                           (item->id[0] != '\0') ? item->id : "-",
                           format_parsed ? format_name : "-",
                           (unsigned long)item->file_size,
                           (unsigned long)expected_size,
                           (unsigned long)width,
                           (unsigned long)height,
                           reason);
    baji_photo_store_marker_index_reject((item->id[0] != '\0') ? item->id : "-",
                                         format_parsed ? format_name : "-",
                                         (unsigned long)item->file_size,
                                         (unsigned long)expected_size,
                                         (unsigned long)width,
                                         (unsigned long)height,
                                         reason);
    return -1;
}

static const char *baji_photo_store_find_char(const char *start, const char *end, char target)
{
    while (start < end) {
        if (*start == target) {
            return start;
        }
        ++start;
    }

    return NULL;
}

static int baji_photo_store_find_result_array(const char *start,
                                              const char *end,
                                              const char **array_start,
                                              const char **array_end)
{
    const char *p;
    unsigned int depth = 1u;
    bool in_string = false;
    bool escaped = false;

    if ((array_start == 0) || (array_end == 0)) {
        return -1;
    }

    p = baji_photo_store_find_key(start, end, "result");
    if ((p == NULL) || (p >= end) || (*p != '[')) {
        return -1;
    }

    *array_start = p + 1;
    ++p;
    while (p < end) {
        char c = *p++;

        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
        } else if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
            if (depth == 0u) {
                *array_end = p - 1;
                return 0;
            }
        }
    }

    return -1;
}

static int baji_photo_gpio_expect(liot_gpioerr_e ret)
{
    return (ret == L_GPIO_ERR_SUCCESS) ? 0 : (int)ret;
}

static int baji_photo_power_gpio_enable(int pad, liot_gpio_e gpio)
{
    liot_gpioerr_e ret;

    ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret == L_GPIO_ERR_SUCCESS) {
        return 0;
    }

    ret = Liot_SetPinFunc(pad, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS) {
        return (int)ret;
    }

    return baji_photo_gpio_expect(Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL));
}

static int baji_photo_flash_hw_init(void)
{
    int ret;

    ret = baji_photo_gpio_expect(Liot_AonPowerCtl(true));
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_gpio_expect(Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V));
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_power_gpio_enable(BAJI_PHOTO_LDO33_EN_PAD, BAJI_PHOTO_LDO33_EN_GPIO);
    if (ret != 0) {
        return ret;
    }
    liot_rtos_task_sleep_ms(BAJI_PHOTO_LDO33_STABLE_DELAY_MS);

    ret = baji_photo_power_gpio_enable(BAJI_PHOTO_VCC3V3_EN_PAD, BAJI_PHOTO_VCC3V3_EN_GPIO);
    if (ret != 0) {
        return ret;
    }
    liot_rtos_task_sleep_ms(BAJI_PHOTO_VCC3V3_STABLE_DELAY_MS);

    ret = baji_photo_gpio_expect(Liot_SetPinFunc(BAJI_PHOTO_SPI_MOSI_PAD, BAJI_PHOTO_SPI_PIN_FUNC));
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_gpio_expect(Liot_SetPinFunc(BAJI_PHOTO_SPI_MISO_PAD, BAJI_PHOTO_SPI_PIN_FUNC));
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_gpio_expect(Liot_SetPinFunc(BAJI_PHOTO_SPI_SCLK_PAD, BAJI_PHOTO_SPI_PIN_FUNC));
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_gpio_expect(Liot_SetPinFunc(BAJI_PHOTO_SPI_CS_PAD, L_PIN_FUNC_0));
    if (ret != 0) {
        return ret;
    }

    return baji_photo_gpio_expect(Liot_GpioInit(BAJI_PHOTO_SPI_CS_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL));
}

static int baji_photo_store_ensure_dir(const char *dir_path)
{
    liot_stat_ext_s st;
    int ret;

    ret = liot_mkdir_ext(dir_path, 0);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(dir_path, &st);
    if ((ret == LIOT_EXTFLASH_OK) && (st.type == LIOT_EXTFLASH_TYPE_DIR)) {
        return 0;
    }

    return LIOT_EXTFLASH_MKDIR_FAIL;
}

static int baji_photo_store_mount_fs(void)
{
    int ret;

    ret = liot_finit_ext(&g_baji_photo_fs_cfg);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }

#if BAJI_PHOTO_ALLOW_AUTO_FORMAT
    ret = liot_fformat_ext();
    if (ret != LIOT_EXTFLASH_OK) {
        return ret;
    }

    (void)liot_fdeinit_ext();
    ret = liot_finit_ext(&g_baji_photo_fs_cfg);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }
#endif

    return ret;
}

bool baji_photo_store_is_ready(void)
{
    return g_baji_photo_store_ready;
}

static int baji_photo_store_mount_locked(void)
{
    int ret;

    if (g_baji_photo_store_ready) {
        return 0;
    }

    ret = baji_photo_flash_hw_init();
    if (ret != 0) {
        return ret;
    }

    ret = liot_flash_init_ext(&g_baji_photo_flash_cfg);
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_mount_fs();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_ensure_dir("/flash");
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_DIR);
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_PHOTO_DIR);
    if (ret != 0) {
        return ret;
    }

#if BAJI_PHOTO_ENABLE_GIF_SUPPORT
    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_GIF_DIR);
    if (ret != 0) {
        return ret;
    }
#endif

#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_JPEG_DIR);
    if (ret != 0) {
        return ret;
    }
#endif

#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_PNG_DIR);
    if (ret != 0) {
        return ret;
    }
#endif

    ret = baji_photo_store_ensure_dir(BAJI_PHOTO_STORE_TMP_DIR);
    if (ret != 0) {
        return ret;
    }

    (void)baji_photo_store_recover_index_locked();
    baji_photo_store_recover_delete_journal_locked();

    g_baji_photo_store_ready = true;
    return 0;
}

int baji_photo_store_access_begin(void)
{
    int ret;

    ret = baji_photo_store_lock();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_mount_locked();
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    return 0;
}

int baji_photo_store_mount(void)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    baji_photo_store_access_end();
    return 0;
}

int baji_photo_store_free_size(void)
{
    int ret;
    int free_size;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    free_size = liot_exflash_free_size_get();
    baji_photo_store_access_end();
    return free_size;
}

uint32_t baji_photo_store_crc32_update(uint32_t crc, const void *data, unsigned int len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    unsigned int i;
    unsigned int bit;

    if ((bytes == 0) && (len != 0u)) {
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

uint32_t baji_photo_store_crc32_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

int baji_photo_store_build_photo_path(const char *id, char *out_path, unsigned int out_len)
{
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    written = snprintf(out_path, out_len, "%s/photo_%s.bjp", BAJI_PHOTO_STORE_PHOTO_DIR, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

int baji_photo_store_build_gif_path(const char *id, char *out_path, unsigned int out_len)
{
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    written = snprintf(out_path, out_len, "%s/%s.gif", BAJI_PHOTO_STORE_GIF_DIR, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

int baji_photo_store_build_jpeg_path(const char *id, char *out_path, unsigned int out_len)
{
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    written = snprintf(out_path, out_len, "%s/%s.jpg", BAJI_PHOTO_STORE_JPEG_DIR, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

int baji_photo_store_build_png_path(const char *id, char *out_path, unsigned int out_len)
{
    int written;

    if (!baji_photo_store_id_is_valid(id) || (out_path == 0) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    written = snprintf(out_path, out_len, "%s/%s.png", BAJI_PHOTO_STORE_PNG_DIR, id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

int baji_photo_store_build_item_path(const char *id,
                                     baji_photo_format_t format,
                                     char *out_path,
                                     unsigned int out_len)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_GIF:
        return baji_photo_store_build_gif_path(id, out_path, out_len);
    case BAJI_PHOTO_FORMAT_JPEG:
        return baji_photo_store_build_jpeg_path(id, out_path, out_len);
    case BAJI_PHOTO_FORMAT_PNG:
        return baji_photo_store_build_png_path(id, out_path, out_len);
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        return baji_photo_store_build_photo_path(id, out_path, out_len);
    }
}

static bool baji_photo_store_file_content_matches_locked(const char *path,
                                                         const void *data,
                                                         unsigned int data_len)
{
#if BAJI_PHOTO_ENABLE_FLASH_WRITE_DEDUP
    liot_stat_ext_s st;
    LFILE_EXT fd;
    const uint8_t *expect = (const uint8_t *)data;
    uint8_t buf[BAJI_FLASH_LFS_READ];
    unsigned int offset = 0u;
    int ret;

    if ((path == NULL) || (data == NULL)) {
        return false;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(path, &st);
    if ((ret != LIOT_EXTFLASH_OK) ||
        (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size != data_len)) {
        return false;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        return false;
    }

    while (offset < data_len) {
        unsigned int chunk = data_len - offset;

        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }
        ret = liot_fread_ext(buf, chunk, 1, fd);
        if (ret != (int)chunk) {
            (void)liot_fclose_ext(fd);
            return false;
        }
        if (memcmp(buf, expect + offset, chunk) != 0) {
            (void)liot_fclose_ext(fd);
            return false;
        }
        offset += chunk;
    }

    (void)liot_fclose_ext(fd);
    return true;
#else
    (void)path;
    (void)data;
    (void)data_len;
    return false;
#endif
}

static int baji_photo_store_write_final_file_locked(const char *path,
                                                    const void *data,
                                                    unsigned int data_len)
{
    LFILE_EXT fd;
    int ret;

    if ((path == NULL) || ((data == NULL) && (data_len != 0u))) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (baji_photo_store_file_content_matches_locked(path, data, data_len)) {
        BAJI_PHOTO_STORE_TRACE("write skip identical file=%s size=%u",
                               path,
                               data_len);
        return 0;
    }

    fd = liot_fopen_ext(path, "w+");
    if (fd <= 0) {
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    ret = liot_fwrite_ext((void *)data, (size_t)data_len, 1, fd);
    if (ret == (int)data_len) {
        ret = liot_fsync_ext(fd);
    } else {
        ret = LIOT_EXTFLASH_WRITE_FAIL;
    }

    if ((liot_fclose_ext(fd) != LIOT_EXTFLASH_OK) && (ret == LIOT_EXTFLASH_OK)) {
        ret = LIOT_EXTFLASH_CLOSE_FAIL;
    }

    return (ret == LIOT_EXTFLASH_OK) ? 0 : ret;
}

static int baji_photo_store_write_file_atomic(const char *tmp_path,
                                              const char *final_path,
                                              const void *data,
                                              unsigned int data_len)
{
    LFILE_EXT fd;
    int ret;

    fd = liot_fopen_ext(tmp_path, "w+");
    if (fd <= 0) {
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    ret = liot_fwrite_ext((void *)data, data_len, 1, fd);
    if (ret == (int)data_len) {
        ret = liot_fsync_ext(fd);
    } else {
        ret = LIOT_EXTFLASH_WRITE_FAIL;
    }

    if (liot_fclose_ext(fd) != LIOT_EXTFLASH_OK && ret == LIOT_EXTFLASH_OK) {
        ret = LIOT_EXTFLASH_CLOSE_FAIL;
    }

    if (ret != LIOT_EXTFLASH_OK) {
        (void)liot_remove_ext(tmp_path);
        return ret;
    }

    {
        liot_stat_ext_s tmp_st;
        liot_stat_ext_s final_st;
        int final_exist_ret;
        int stat_ret;
        int final_exist_after_ret;
        int tmp_exist_after_ret;

        memset(&tmp_st, 0, sizeof(tmp_st));
        stat_ret = liot_stat_ext(tmp_path, &tmp_st);
        if ((stat_ret != LIOT_EXTFLASH_OK) ||
            (tmp_st.type != LIOT_EXTFLASH_TYPE_FILE) ||
            (tmp_st.size != data_len)) {
            (void)liot_remove_ext(tmp_path);
            if (stat_ret != LIOT_EXTFLASH_OK) {
                return stat_ret;
            }
            return LIOT_EXTFLASH_SIZE_FAIL;
        }

        memset(&final_st, 0, sizeof(final_st));
        final_exist_ret = liot_file_exist_ext(final_path);
        stat_ret = liot_stat_ext(final_path, &final_st);
        if (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST) {
            ret = liot_remove_ext(final_path);
            final_exist_ret = liot_file_exist_ext(final_path);
            memset(&final_st, 0, sizeof(final_st));
            stat_ret = liot_stat_ext(final_path, &final_st);
            if ((ret != LIOT_EXTFLASH_OK) && (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST)) {
                (void)liot_remove_ext(tmp_path);
                return LIOT_EXTFLASH_REMOVE_FAIL;
            }
            if (final_exist_ret == LIOT_EXTFLASH_OK) {
                (void)liot_remove_ext(tmp_path);
                return LIOT_EXTFLASH_REMOVE_FAIL;
            }
        }

        ret = liot_rename_ext(tmp_path, final_path);
        if (ret != LIOT_EXTFLASH_OK) {
            final_exist_after_ret = liot_file_exist_ext(final_path);
            memset(&final_st, 0, sizeof(final_st));
            stat_ret = liot_stat_ext(final_path, &final_st);
            tmp_exist_after_ret = liot_file_exist_ext(tmp_path);
            if ((final_exist_after_ret == LIOT_EXTFLASH_OK) &&
                (stat_ret == LIOT_EXTFLASH_OK) &&
                (final_st.type == LIOT_EXTFLASH_TYPE_FILE) &&
                (final_st.size == data_len) &&
                (tmp_exist_after_ret == LIOT_EXTFLASH_NOT_EXIST)) {
                return 0;
            }
            (void)liot_remove_ext(tmp_path);
            return LIOT_EXTFLASH_RENAME_FAIL;
        }
    }
    return 0;
}

static int baji_photo_store_write_tmp_file(const char *tmp_path,
                                           const void *data,
                                           unsigned int data_len)
{
    LFILE_EXT fd;
    int ret;
    liot_stat_ext_s st;

    fd = liot_fopen_ext(tmp_path, "w+");
    if (fd <= 0) {
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    ret = liot_fwrite_ext((void *)data, data_len, 1, fd);
    if (ret == (int)data_len) {
        ret = liot_fsync_ext(fd);
    } else {
        ret = LIOT_EXTFLASH_WRITE_FAIL;
    }

    if ((liot_fclose_ext(fd) != LIOT_EXTFLASH_OK) && (ret == LIOT_EXTFLASH_OK)) {
        ret = LIOT_EXTFLASH_CLOSE_FAIL;
    }
    if (ret != LIOT_EXTFLASH_OK) {
        (void)liot_remove_ext(tmp_path);
        return ret;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(tmp_path, &st);
    if ((ret != LIOT_EXTFLASH_OK) ||
        (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size != data_len)) {
        (void)liot_remove_ext(tmp_path);
        if (ret != LIOT_EXTFLASH_OK) {
            return ret;
        }
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    return 0;
}

static int baji_photo_store_replace_index_file_locked(const void *data, unsigned int data_len)
{
    int ret;
    int index_exist_ret;
    int backup_exist_ret;
    int index_exist_after_ret;
    int backup_exist_after_ret;

    if (baji_photo_store_file_content_matches_locked(BAJI_PHOTO_INDEX_FILE, data, data_len)) {
        BAJI_PHOTO_STORE_TRACE("index save skip identical size=%u", data_len);
        return 0;
    }

    ret = baji_photo_store_write_tmp_file(BAJI_PHOTO_INDEX_TMP_FILE, data, data_len);
    if (ret != 0) {
        return ret;
    }

    index_exist_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_FILE);
    backup_exist_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
    if (backup_exist_ret == LIOT_EXTFLASH_OK) {
        ret = liot_remove_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
        if ((ret != LIOT_EXTFLASH_OK) && (ret != LIOT_EXTFLASH_NOT_EXIST)) {
            (void)liot_remove_ext(BAJI_PHOTO_INDEX_TMP_FILE);
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
    }

    if (index_exist_ret == LIOT_EXTFLASH_OK) {
        ret = liot_rename_ext(BAJI_PHOTO_INDEX_FILE, BAJI_PHOTO_INDEX_BACKUP_FILE);
        if (ret != LIOT_EXTFLASH_OK) {
            (void)liot_remove_ext(BAJI_PHOTO_INDEX_TMP_FILE);
            return LIOT_EXTFLASH_RENAME_FAIL;
        }
    }

    ret = liot_rename_ext(BAJI_PHOTO_INDEX_TMP_FILE, BAJI_PHOTO_INDEX_FILE);
    if (ret != LIOT_EXTFLASH_OK) {
        if (index_exist_ret == LIOT_EXTFLASH_OK) {
            (void)liot_rename_ext(BAJI_PHOTO_INDEX_BACKUP_FILE, BAJI_PHOTO_INDEX_FILE);
        }
        (void)liot_remove_ext(BAJI_PHOTO_INDEX_TMP_FILE);
        return LIOT_EXTFLASH_RENAME_FAIL;
    }

    if (index_exist_ret == LIOT_EXTFLASH_OK) {
        ret = liot_remove_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
        if ((ret != LIOT_EXTFLASH_OK) && (ret != LIOT_EXTFLASH_NOT_EXIST)) {
            index_exist_after_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_FILE);
            backup_exist_after_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
            if (index_exist_after_ret != LIOT_EXTFLASH_OK) {
                return LIOT_EXTFLASH_REMOVE_FAIL;
            }
            if (backup_exist_after_ret == LIOT_EXTFLASH_OK) {
                BAJI_PHOTO_STORE_TRACE("index replace keep backup for later cleanup ret=%d", ret);
            }
        }
    }

    return 0;
}

static int baji_photo_store_parse_delete_journal(const char *buf,
                                                 unsigned int len,
                                                 baji_photo_store_delete_journal_t *journal)
{
    const char *end;
    uint32_t file_size = 0u;
    uint32_t txn_id = 0u;

    if ((buf == NULL) || (len == 0u) || (journal == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(journal, 0, sizeof(*journal));
    end = buf + len;
    if ((baji_photo_store_parse_string(buf, end, "id", journal->id, sizeof(journal->id)) != 0) ||
        (baji_photo_store_parse_format(buf, end, &journal->format) != 0) ||
        (baji_photo_store_parse_hex_u32(buf, end, "crc32", &journal->crc32) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "file_size", &file_size) != 0)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }
    (void)baji_photo_store_parse_u32_optional(buf, end, "txn_id", 0u, &txn_id);
    if (!baji_photo_store_id_is_valid(journal->id)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    journal->file_size = file_size;
    journal->txn_id = txn_id;
    return 0;
}

static int baji_photo_store_load_delete_journal_locked(
    baji_photo_store_delete_journal_t *journal)
{
    liot_stat_ext_s st;
    char *buf = NULL;
    LFILE_EXT fd;
    int read_len;
    int ret;

    if (journal == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(BAJI_PHOTO_DELETE_JOURNAL_FILE, &st);
    if ((ret == LIOT_EXTFLASH_NOT_EXIST) || (ret == LIOT_EXTFLASH_STAT_FAIL)) {
        return LIOT_EXTFLASH_NOT_EXIST;
    }
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size == 0u) || (st.size >= 512u)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(BAJI_PHOTO_DELETE_JOURNAL_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }
    buf[st.size] = '\0';
    ret = baji_photo_store_parse_delete_journal(buf, st.size, journal);
    liot_rtos_free(buf);
    return ret;
}

static int baji_photo_store_save_delete_journal_locked(
    const baji_photo_store_delete_journal_t *journal)
{
    char line[256];
    const char *format_name;
    int len;

    if (journal == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    format_name = baji_photo_store_format_name(journal->format);
    len = snprintf(line,
                   sizeof(line),
                   "{\"id\":\"%s\",\"format\":\"%s\",\"crc32\":\"%08lX\","
                   "\"file_size\":%lu,\"txn_id\":%lu}\n",
                   journal->id,
                   format_name,
                   (unsigned long)journal->crc32,
                   (unsigned long)journal->file_size,
                   (unsigned long)journal->txn_id);
    if ((len <= 0) || ((unsigned int)len >= sizeof(line))) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (baji_photo_store_file_content_matches_locked(BAJI_PHOTO_DELETE_JOURNAL_FILE,
                                                     line,
                                                     (unsigned int)len)) {
        BAJI_PHOTO_STORE_TRACE("delete journal skip identical id=%s", journal->id);
        return 0;
    }

    return baji_photo_store_write_file_atomic(BAJI_PHOTO_DELETE_JOURNAL_TMP_FILE,
                                              BAJI_PHOTO_DELETE_JOURNAL_FILE,
                                              line,
                                              (unsigned int)len);
}

static int baji_photo_store_clear_delete_journal_locked(void)
{
    int ret;

    ret = liot_remove_ext(BAJI_PHOTO_DELETE_JOURNAL_FILE);
    if ((ret == LIOT_EXTFLASH_OK) || (ret == LIOT_EXTFLASH_NOT_EXIST)) {
        return 0;
    }
    return ret;
}

static int baji_photo_store_remove_item_file_locked(const char *id, baji_photo_format_t format)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    int ret;

    ret = baji_photo_store_build_item_path(id, format, path, sizeof(path));
    if (ret != 0) {
        return ret;
    }

    ret = liot_remove_ext(path);
    if ((ret == LIOT_EXTFLASH_OK) || (ret == LIOT_EXTFLASH_NOT_EXIST)) {
        return 0;
    }
    return ret;
}

static int baji_photo_store_recover_index_locked(void)
{
    int index_exist_ret;
    int backup_exist_ret;
    int ret;

    index_exist_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_FILE);
    backup_exist_ret = liot_file_exist_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
    if (backup_exist_ret != LIOT_EXTFLASH_OK) {
        return 0;
    }

    if (index_exist_ret == LIOT_EXTFLASH_NOT_EXIST) {
        ret = liot_rename_ext(BAJI_PHOTO_INDEX_BACKUP_FILE, BAJI_PHOTO_INDEX_FILE);
        if (ret != LIOT_EXTFLASH_OK) {
            BAJI_PHOTO_STORE_TRACE("index recover restore backup failed ret=%d", ret);
        } else {
            BAJI_PHOTO_STORE_TRACE("index recover restored backup");
        }
        return 0;
    }

    ret = liot_remove_ext(BAJI_PHOTO_INDEX_BACKUP_FILE);
    if ((ret != LIOT_EXTFLASH_OK) && (ret != LIOT_EXTFLASH_NOT_EXIST)) {
        BAJI_PHOTO_STORE_TRACE("index recover drop backup failed ret=%d", ret);
    }
    return 0;
}

static void baji_photo_store_recover_delete_journal_locked(void)
{
    baji_photo_store_delete_journal_t journal;
    baji_photo_manifest_item_t items[BAJI_PHOTO_DL_MAX];
    unsigned int count = 0u;
    int ret;
    int idx;

    ret = baji_photo_store_load_delete_journal_locked(&journal);
    if (ret == LIOT_EXTFLASH_NOT_EXIST) {
        return;
    }
    if (ret != 0) {
        BAJI_PHOTO_STORE_TRACE("delete journal load failed ret=%d", ret);
        (void)baji_photo_store_clear_delete_journal_locked();
        return;
    }

    memset(items, 0, sizeof(items));
    ret = baji_photo_store_load_index_locked(items, BAJI_PHOTO_DL_MAX, &count);
    if (ret != 0) {
        BAJI_PHOTO_STORE_TRACE("delete journal index load failed id=%s ret=%d",
                               journal.id,
                               ret);
        return;
    }

    idx = baji_photo_store_find_item(items, count, journal.id, journal.format);
    if (idx >= 0) {
        BAJI_PHOTO_STORE_TRACE("delete journal stale id=%s format=%s count=%u",
                               journal.id,
                               baji_photo_store_format_name(journal.format),
                               count);
        (void)baji_photo_store_clear_delete_journal_locked();
        return;
    }

    ret = baji_photo_store_remove_item_file_locked(journal.id, journal.format);
    if (ret != 0) {
        BAJI_PHOTO_STORE_TRACE("delete journal cleanup defer id=%s format=%s ret=%d",
                               journal.id,
                               baji_photo_store_format_name(journal.format),
                               ret);
        return;
    }

    BAJI_PHOTO_STORE_TRACE("delete journal cleanup done id=%s format=%s",
                           journal.id,
                           baji_photo_store_format_name(journal.format));
    (void)baji_photo_store_clear_delete_journal_locked();
}

int baji_photo_store_write_photo_file(const char *id, const void *data, unsigned int data_len)
{
    char tmp_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char final_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    int ret;

    if ((data == 0) || (data_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_build_tmp_path_for_format(id,
                                                     BAJI_PHOTO_FORMAT_BJP,
                                                     tmp_path,
                                                     sizeof(tmp_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_build_photo_path(id, final_path, sizeof(final_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_write_file_atomic(tmp_path, final_path, data, data_len);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_write_gif_file(const char *id, const void *data, unsigned int data_len)
{
    char tmp_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char final_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    int ret;

    if ((data == 0) || (data_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_build_tmp_path_for_format(id,
                                                     BAJI_PHOTO_FORMAT_GIF,
                                                     tmp_path,
                                                     sizeof(tmp_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_build_gif_path(id, final_path, sizeof(final_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_write_file_atomic(tmp_path, final_path, data, data_len);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_write_raw_file(const char *id,
                                    baji_photo_format_t format,
                                    const void *data,
                                    unsigned int data_len)
{
    char tmp_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char final_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    int ret;

    if ((data == 0) || (data_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if ((format != BAJI_PHOTO_FORMAT_JPEG) && (format != BAJI_PHOTO_FORMAT_PNG)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_build_tmp_path_for_format(id, format, tmp_path, sizeof(tmp_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_build_item_path(id, format, final_path, sizeof(final_path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = baji_photo_store_write_file_atomic(tmp_path, final_path, data, data_len);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_store_parse_task_state(const char *buf,
                                             unsigned int len,
                                             baji_photo_task_state_t *state)
{
    const char *end;

    if ((buf == NULL) || (state == NULL) || (len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(state, 0, sizeof(*state));
    end = buf + len;
    {
        uint32_t image_width = 0u;
        uint32_t image_height = 0u;

        if ((baji_photo_store_parse_string(buf, end, "msg_id", state->msg_id, sizeof(state->msg_id)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "task_id", state->task_id, sizeof(state->task_id)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "image_id", state->image_id, sizeof(state->image_id)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "image_url", state->image_url, sizeof(state->image_url)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "md5", state->md5, sizeof(state->md5)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "image_format", state->image_format, sizeof(state->image_format)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "display_mode", state->display_mode, sizeof(state->display_mode)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "tmp_path", state->tmp_path, sizeof(state->tmp_path)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "final_path", state->final_path, sizeof(state->final_path)) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "image_size", &state->image_size) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "downloaded_size", &state->downloaded_size) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "chunk_size", &state->chunk_size) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "expire_seconds", &state->expire_seconds) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "image_width", &image_width) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "image_height", &image_height) != 0) ||
        (baji_photo_store_parse_u8(buf, end, "retry_count", &state->retry_count) != 0)) {
            return LIOT_EXTFLASH_READ_FAIL;
        }
        state->image_width = (uint16_t)image_width;
        state->image_height = (uint16_t)image_height;
    }

    {
        uint32_t support_range = 0u;
        uint32_t accepted_sent = 0u;
        uint32_t completed = 0u;
        uint32_t verified = 0u;
        uint32_t waiting_network = 0u;
        uint32_t display_pending = 0u;
        uint32_t displayed = 0u;

        if ((baji_photo_store_parse_u32(buf, end, "support_range", &support_range) != 0) ||
            (baji_photo_store_parse_u32(buf, end, "accepted_sent", &accepted_sent) != 0) ||
            (baji_photo_store_parse_u32(buf, end, "completed", &completed) != 0) ||
            (baji_photo_store_parse_u32(buf, end, "verified", &verified) != 0)) {
            return LIOT_EXTFLASH_READ_FAIL;
        }
        (void)baji_photo_store_parse_u32_optional(buf,
                                                  end,
                                                  "next_retry_at_s",
                                                  0u,
                                                  &state->next_retry_at_s);
        (void)baji_photo_store_parse_u32_optional(buf,
                                                  end,
                                                  "network_wait_deadline_s",
                                                  0u,
                                                  &state->network_wait_deadline_s);
        (void)baji_photo_store_parse_u8_optional(buf,
                                                 end,
                                                 "network_wait_count",
                                                 0u,
                                                 &state->network_wait_count);
        (void)baji_photo_store_parse_u32_optional(buf,
                                                  end,
                                                  "waiting_network",
                                                  0u,
                                                  &waiting_network);
        (void)baji_photo_store_parse_u32_optional(buf,
                                                  end,
                                                  "display_pending",
                                                  0u,
                                                  &display_pending);
        (void)baji_photo_store_parse_u32_optional(buf,
                                                  end,
                                                  "displayed",
                                                  0u,
                                                  &displayed);
        (void)baji_photo_store_parse_u64_optional(buf,
                                                  end,
                                                  "display_time_ms",
                                                  0u,
                                                  &state->display_time_ms);
#if !BAJI_PHOTO_ENABLE_HTTP_RETRY_COUNT_PERSIST
        state->retry_count = 0u;
#endif
#if !BAJI_PHOTO_ENABLE_NETWORK_WAIT_STATE_PERSIST
        state->next_retry_at_s = 0u;
        state->network_wait_deadline_s = 0u;
        state->network_wait_count = 0u;
        waiting_network = 0u;
#endif
        state->support_range = (support_range != 0u);
#if BAJI_PHOTO_ENABLE_ACCEPTED_SENT_PERSIST
        state->accepted_sent = (accepted_sent != 0u);
#else
        state->accepted_sent = false;
#endif
        state->completed = (completed != 0u);
        state->verified = (verified != 0u);
        state->waiting_network = (waiting_network != 0u);
        state->display_pending = (display_pending != 0u);
        state->displayed = (displayed != 0u);
    }

    return 0;
}

int baji_photo_store_save_task_state(const baji_photo_task_state_t *state)
{
    char line[1792];
    char display_time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int len;
    int ret;

    if (state == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    len = snprintf(line,
                   sizeof(line),
                   "{\"msg_id\":\"%s\",\"task_id\":\"%s\",\"image_id\":\"%s\","
                   "\"image_url\":\"%s\",\"md5\":\"%s\",\"image_format\":\"%s\","
                   "\"display_mode\":\"%s\",\"tmp_path\":\"%s\",\"final_path\":\"%s\","
                   "\"image_size\":%lu,\"downloaded_size\":%lu,\"chunk_size\":%lu,"
                   "\"expire_seconds\":%lu,\"image_width\":%u,\"image_height\":%u,"
                   "\"retry_count\":%u,\"support_range\":%u,\"accepted_sent\":%u,"
                   "\"completed\":%u,\"verified\":%u,\"next_retry_at_s\":%lu,"
                   "\"network_wait_deadline_s\":%lu,\"network_wait_count\":%u,"
                   "\"waiting_network\":%u,\"display_pending\":%u,"
                   "\"displayed\":%u,\"display_time_ms\":%s}\n",
                   state->msg_id,
                   state->task_id,
                   state->image_id,
                   state->image_url,
                   state->md5,
                   state->image_format,
                   state->display_mode,
                   state->tmp_path,
                   state->final_path,
                   (unsigned long)state->image_size,
                   (unsigned long)state->downloaded_size,
                   (unsigned long)state->chunk_size,
                   (unsigned long)state->expire_seconds,
                   (unsigned int)state->image_width,
                   (unsigned int)state->image_height,
#if BAJI_PHOTO_ENABLE_HTTP_RETRY_COUNT_PERSIST
                   (unsigned int)state->retry_count,
#else
                   0u,
#endif
                   state->support_range ? 1u : 0u,
#if BAJI_PHOTO_ENABLE_ACCEPTED_SENT_PERSIST
                   state->accepted_sent ? 1u : 0u,
#else
                   0u,
#endif
                   state->completed ? 1u : 0u,
                   state->verified ? 1u : 0u,
#if BAJI_PHOTO_ENABLE_NETWORK_WAIT_STATE_PERSIST
                   (unsigned long)state->next_retry_at_s,
                   (unsigned long)state->network_wait_deadline_s,
                   (unsigned int)state->network_wait_count,
                   state->waiting_network ? 1u : 0u,
#else
                   0ul,
                   0ul,
                   0u,
                   0u,
#endif
                   state->display_pending ? 1u : 0u,
                   state->displayed ? 1u : 0u,
                   baji_photo_diag_u64_dec(state->display_time_ms,
                                           display_time_buf,
                                           sizeof(display_time_buf)));
    if ((len <= 0) || ((unsigned int)len >= sizeof(line))) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_write_final_file_locked(BAJI_PHOTO_TASK_STATE_FILE,
                                                   line,
                                                   (unsigned int)len);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_load_task_state(baji_photo_task_state_t *state)
{
    liot_stat_ext_s st;
    char *buf = NULL;
    LFILE_EXT fd;
    int read_len;
    int ret;

    if (state == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(BAJI_PHOTO_TASK_STATE_FILE, &st);
    if ((ret == LIOT_EXTFLASH_NOT_EXIST) || (ret == LIOT_EXTFLASH_STAT_FAIL)) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_NOT_EXIST;
    }
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size == 0u) || (st.size >= 2048u)) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == NULL) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(BAJI_PHOTO_TASK_STATE_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }
    buf[st.size] = '\0';
    ret = baji_photo_store_parse_task_state(buf, st.size, state);
    liot_rtos_free(buf);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_clear_task_state(void)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = liot_remove_ext(BAJI_PHOTO_TASK_STATE_FILE);
    baji_photo_store_access_end();
    if ((ret == LIOT_EXTFLASH_OK) || (ret == LIOT_EXTFLASH_NOT_EXIST)) {
        return 0;
    }
    return ret;
}

static int baji_photo_store_parse_pending_reply(const char *buf,
                                                unsigned int len,
                                                baji_photo_mqtt_pending_reply_t *reply)
{
    const char *end;
    uint32_t code = 0u;
    uint32_t generation = 0u;
    uint32_t attempt_count = 0u;

    if ((buf == NULL) || (reply == NULL) || (len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(reply, 0, sizeof(*reply));
    end = buf + len;
    if ((baji_photo_store_parse_string(buf, end, "reply_to", reply->reply_to, sizeof(reply->reply_to)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "task_id", reply->task_id, sizeof(reply->task_id)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "image_id", reply->image_id, sizeof(reply->image_id)) != 0) ||
        (baji_photo_store_parse_string(buf, end, "reason", reply->reason, sizeof(reply->reason)) != 0) ||
        (baji_photo_store_parse_u32(buf, end, "code", &code) != 0)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }
    (void)baji_photo_store_parse_u64_optional(buf,
                                              end,
                                              "timestamp_ms",
                                              0u,
                                              &reply->timestamp_ms);
    (void)baji_photo_store_parse_u64_optional(buf,
                                              end,
                                              "display_time_ms",
                                              0u,
                                              &reply->display_time_ms);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "generation",
                                              0u,
                                              &generation);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "attempt_count",
                                              0u,
                                              &attempt_count);

    reply->code = (int)code;
    reply->generation = generation;
    reply->attempt_count = attempt_count;
    return 0;
}

int baji_photo_store_save_pending_reply(const baji_photo_mqtt_pending_reply_t *reply)
{
    char line[704];
    char timestamp_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    char display_time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int len;
    int ret = LIOT_EXTFLASH_OK;

    if (reply == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    BAJI_PHOTO_STORE_TRACE("pending save start task=%s image=%s code=%d file=%s",
                           reply->task_id,
                           reply->image_id,
                           reply->code,
                           BAJI_PHOTO_PENDING_REPLY_FILE);

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        goto out;
    }

    len = snprintf(line,
                   sizeof(line),
                   "{\"reply_to\":\"%s\",\"task_id\":\"%s\",\"image_id\":\"%s\","
                   "\"reason\":\"%s\",\"code\":%d,\"timestamp_ms\":%s,"
                   "\"display_time_ms\":%s,\"generation\":%lu,\"attempt_count\":%lu}\n",
                   reply->reply_to,
                   reply->task_id,
                   reply->image_id,
                   reply->reason,
                   reply->code,
                   baji_photo_diag_u64_dec(reply->timestamp_ms,
                                           timestamp_buf,
                                           sizeof(timestamp_buf)),
                   baji_photo_diag_u64_dec(reply->display_time_ms,
                                           display_time_buf,
                                           sizeof(display_time_buf)),
                   (unsigned long)reply->generation,
                   (unsigned long)reply->attempt_count);
    if ((len <= 0) || ((unsigned int)len >= sizeof(line))) {
        ret = LIOT_EXTFLASH_INVALID_PARAMETER;
        goto unlock_out;
    }

    ret = baji_photo_store_write_final_file_locked(BAJI_PHOTO_PENDING_REPLY_FILE,
                                                   line,
                                                   (unsigned int)len);

unlock_out:
    baji_photo_store_access_end();

out:
    BAJI_PHOTO_STORE_TRACE("pending save done task=%s image=%s code=%d ret=%d",
                           reply->task_id,
                           reply->image_id,
                           reply->code,
                           (ret == LIOT_EXTFLASH_OK) ? 0 : ret);
    return (ret == LIOT_EXTFLASH_OK) ? 0 : ret;
}

int baji_photo_store_load_pending_reply(baji_photo_mqtt_pending_reply_t *reply)
{
    liot_stat_ext_s st;
    char *buf = NULL;
    LFILE_EXT fd;
    int read_len;
    int ret;

    if (reply == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(BAJI_PHOTO_PENDING_REPLY_FILE, &st);
    if ((ret == LIOT_EXTFLASH_NOT_EXIST) || (ret == LIOT_EXTFLASH_STAT_FAIL)) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_NOT_EXIST;
    }
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size == 0u) || (st.size >= 1024u)) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == NULL) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(BAJI_PHOTO_PENDING_REPLY_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf[st.size] = '\0';
    ret = baji_photo_store_parse_pending_reply(buf, st.size, reply);
    liot_rtos_free(buf);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_clear_pending_reply(void)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = liot_remove_ext(BAJI_PHOTO_PENDING_REPLY_FILE);
    baji_photo_store_access_end();
    if ((ret == LIOT_EXTFLASH_OK) || (ret == LIOT_EXTFLASH_NOT_EXIST)) {
        return 0;
    }
    return ret;
}

static int baji_photo_store_parse_device_meta(const char *buf,
                                              unsigned int len,
                                              baji_photo_device_meta_t *meta)
{
    const char *end;
    uint8_t timer_active = 0u;

    if ((buf == NULL) || (meta == NULL) || (len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(meta, 0, sizeof(*meta));
    end = buf + len;
    if ((baji_photo_store_parse_string(buf,
                                       end,
                                       "last_task_id",
                                       meta->last_task_id,
                                       sizeof(meta->last_task_id)) != 0) ||
        (baji_photo_store_parse_string(buf,
                                       end,
                                       "current_image_id",
                                       meta->current_image_id,
                                       sizeof(meta->current_image_id)) != 0) ||
        (baji_photo_store_parse_string(buf,
                                       end,
                                       "current_image_md5",
                                       meta->current_image_md5,
                                       sizeof(meta->current_image_md5)) != 0)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }
    (void)baji_photo_store_parse_u64_optional(buf,
                                              end,
                                              "last_display_time_ms",
                                              0u,
                                              &meta->last_display_time_ms);
    (void)baji_photo_store_parse_string(buf,
                                        end,
                                        "play_image_id",
                                        meta->playback.image_id,
                                        sizeof(meta->playback.image_id));
    (void)baji_photo_store_parse_string(buf,
                                        end,
                                        "play_trigger",
                                        meta->playback.trigger,
                                        sizeof(meta->playback.trigger));
    (void)baji_photo_store_parse_u64_optional(buf,
                                              end,
                                              "play_time_ms",
                                              0u,
                                              &meta->playback.time_ms);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "play_seq",
                                              0u,
                                              &meta->playback.seq);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "play_source_idx",
                                              UINT32_MAX,
                                              &meta->playback.source_idx);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "play_target_idx",
                                              UINT32_MAX,
                                              &meta->playback.target_idx);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "play_dyn_seq",
                                              0u,
                                              &meta->playback.dyn_seq);
    (void)baji_photo_store_parse_u32_optional(buf,
                                              end,
                                              "play_timer_period_ms",
                                              0u,
                                              &meta->playback.timer_period_ms);
    (void)baji_photo_store_parse_u8_optional(buf,
                                             end,
                                             "play_timer_active",
                                             0u,
                                             &timer_active);
    meta->playback.timer_active = timer_active != 0u;
    meta->playback.valid = (meta->playback.image_id[0] != '\0') ||
                           (meta->playback.trigger[0] != '\0') ||
                           (meta->playback.seq != 0u);

    return 0;
}

static int baji_photo_store_save_device_meta_locked(const baji_photo_device_meta_t *meta)
{
    char line[768];
    char display_time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
#if BAJI_PHOTO_ENABLE_PLAYBACK_META_PERSIST
    char play_time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
#endif
    int len;
    int ret = LIOT_EXTFLASH_OK;

    if (meta == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    BAJI_PHOTO_STORE_TRACE("meta save start task=%s image=%s file=%s",
                           meta->last_task_id,
                           meta->current_image_id,
                           BAJI_PHOTO_META_FILE);

    len = snprintf(line,
                   sizeof(line),
                   "{\"last_task_id\":\"%s\",\"current_image_id\":\"%s\","
                   "\"current_image_md5\":\"%s\",\"last_display_time_ms\":%s",
                   meta->last_task_id,
                   meta->current_image_id,
                   meta->current_image_md5,
                   baji_photo_diag_u64_dec(meta->last_display_time_ms,
                                           display_time_buf,
                                           sizeof(display_time_buf)));
#if BAJI_PHOTO_ENABLE_PLAYBACK_META_PERSIST
    if ((len > 0) && ((unsigned int)len < sizeof(line))) {
        len += snprintf(line + len,
                        sizeof(line) - (unsigned int)len,
                        ",\"play_image_id\":\"%s\",\"play_trigger\":\"%s\","
                        "\"play_time_ms\":%s,\"play_seq\":%lu,"
                        "\"play_source_idx\":%lu,\"play_target_idx\":%lu,"
                        "\"play_dyn_seq\":%lu,\"play_timer_period_ms\":%lu,"
                        "\"play_timer_active\":%u",
                        meta->playback.image_id,
                        meta->playback.trigger,
                        baji_photo_diag_u64_dec(meta->playback.time_ms,
                                                play_time_buf,
                                                sizeof(play_time_buf)),
                        (unsigned long)meta->playback.seq,
                        (unsigned long)meta->playback.source_idx,
                        (unsigned long)meta->playback.target_idx,
                        (unsigned long)meta->playback.dyn_seq,
                        (unsigned long)meta->playback.timer_period_ms,
                        meta->playback.timer_active ? 1u : 0u);
    }
#endif
    if ((len > 0) && ((unsigned int)len < sizeof(line))) {
        len += snprintf(line + len, sizeof(line) - (unsigned int)len, "}\n");
    }
    if ((len <= 0) || ((unsigned int)len >= sizeof(line))) {
        ret = LIOT_EXTFLASH_INVALID_PARAMETER;
        goto out;
    }

    ret = baji_photo_store_write_final_file_locked(BAJI_PHOTO_META_FILE,
                                                   line,
                                                   (unsigned int)len);

out:
    BAJI_PHOTO_STORE_TRACE("meta save done task=%s image=%s ret=%d",
                           meta->last_task_id,
                           meta->current_image_id,
                           (ret == LIOT_EXTFLASH_OK) ? 0 : ret);
    return (ret == LIOT_EXTFLASH_OK) ? 0 : ret;
}

static int baji_photo_store_load_device_meta_locked(baji_photo_device_meta_t *meta)
{
    liot_stat_ext_s st;
    char *buf = NULL;
    LFILE_EXT fd;
    int read_len;
    int ret;

    if (meta == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(BAJI_PHOTO_META_FILE, &st);
    if ((ret == LIOT_EXTFLASH_NOT_EXIST) || (ret == LIOT_EXTFLASH_STAT_FAIL)) {
        return LIOT_EXTFLASH_NOT_EXIST;
    }
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size == 0u) || (st.size >= 1024u)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(BAJI_PHOTO_META_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }

    buf[st.size] = '\0';
    ret = baji_photo_store_parse_device_meta(buf, st.size, meta);
    liot_rtos_free(buf);
    return ret;
}

int baji_photo_store_save_device_meta(const baji_photo_device_meta_t *meta)
{
    int ret;

    if (meta == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_save_device_meta_locked(meta);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_load_device_meta(baji_photo_device_meta_t *meta)
{
    int ret;

    if (meta == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_load_device_meta_locked(meta);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_update_device_meta_mqtt(const char *task_id,
                                             const char *image_id,
                                             const char *image_md5,
                                             uint64_t display_time_ms)
{
    baji_photo_device_meta_t meta;
    int ret;

    if ((task_id == NULL) || (image_id == NULL) || (image_md5 == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

#if BAJI_PHOTO_ENABLE_PLAYBACK_META_PERSIST
    memset(&meta, 0, sizeof(meta));
    ret = baji_photo_store_load_device_meta_locked(&meta);
    if ((ret != 0) && (ret != LIOT_EXTFLASH_NOT_EXIST)) {
        memset(&meta, 0, sizeof(meta));
    }
#else
    memset(&meta, 0, sizeof(meta));
#endif

    (void)snprintf(meta.last_task_id, sizeof(meta.last_task_id), "%s", task_id);
    (void)snprintf(meta.current_image_id, sizeof(meta.current_image_id), "%s", image_id);
    (void)snprintf(meta.current_image_md5, sizeof(meta.current_image_md5), "%s", image_md5);
    meta.last_display_time_ms = display_time_ms;

    ret = baji_photo_store_save_device_meta_locked(&meta);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_update_device_meta_playback(const baji_photo_playback_meta_t *playback)
{
    baji_photo_device_meta_t meta;
    int ret;

    if (playback == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&meta, 0, sizeof(meta));
    ret = baji_photo_store_load_device_meta_locked(&meta);
    if ((ret != 0) && (ret != LIOT_EXTFLASH_NOT_EXIST)) {
        memset(&meta, 0, sizeof(meta));
    }

    meta.playback = *playback;
    ret = baji_photo_store_save_device_meta_locked(&meta);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_read_photo_header(const char *id, baji_photo_file_header_t *out_header)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    LFILE_EXT fd;
    int ret;

    if (out_header == 0) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_build_photo_path(id, path, sizeof(path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    memset(out_header, 0, sizeof(*out_header));
    ret = liot_fread_ext(out_header, sizeof(*out_header), 1, fd);
    (void)liot_fclose_ext(fd);
    if (ret != (int)sizeof(*out_header)) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }

    baji_photo_store_access_end();
    return 0;
}

int baji_photo_store_remove_photo(const char *id)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_build_photo_path(id, path, sizeof(path));
    if (ret != 0) {
        baji_photo_store_access_end();
        return ret;
    }

    ret = liot_remove_ext(path);
    baji_photo_store_access_end();
    if ((ret == LIOT_EXTFLASH_OK) || (ret == LIOT_EXTFLASH_NOT_EXIST)) {
        return 0;
    }

    return ret;
}

int baji_photo_store_remove_item(const baji_photo_manifest_item_t *item)
{
    int ret;

    if (item == 0) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_remove_item_file_locked(item->id, item->format);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_store_save_index_locked(const baji_photo_manifest_item_t *items,
                                              unsigned int count)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char *buf = NULL;
    size_t buf_size = BAJI_PHOTO_HTTP_MANIFEST_MAX + 1u;
    size_t offset = 0u;
    unsigned int i;
    int ret = 0;

    if ((items == 0) && (count != 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    buf = (char *)liot_rtos_malloc(buf_size);
    if (buf == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    ret = snprintf(buf, buf_size, "{\"result\":[\n");
    if ((ret <= 0) || ((size_t)ret >= buf_size)) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    offset = (size_t)ret;

    for (i = 0; i < count; ++i) {
        const char *format_name = baji_photo_store_format_name(items[i].format);
        int len;

        ret = baji_photo_store_build_item_path(items[i].id,
                                               items[i].format,
                                               path,
                                               sizeof(path));
        if (ret != 0) {
            liot_rtos_free(buf);
            return ret;
        }

        len = snprintf(buf + offset,
                       buf_size - offset,
                       "%s{\"id\":\"%s\",\"name\":\"%s\",\"remote_path\":\"%s\","
                       "\"local_path\":\"%s\",\"file_size\":%lu,\"crc32\":\"%08lX\","
                       "\"width\":%u,\"height\":%u,\"format\":\"%s\"}\n",
                       (i == 0u) ? "" : ",",
                       items[i].id,
                       (items[i].name[0] != '\0') ? items[i].name : items[i].id,
                       items[i].remote_path,
                       path,
                       (unsigned long)items[i].file_size,
                       (unsigned long)items[i].crc32,
                       (unsigned int)items[i].width,
                       (unsigned int)items[i].height,
                       format_name);
        if ((len <= 0) || ((size_t)len >= (buf_size - offset))) {
            liot_rtos_free(buf);
            return LIOT_EXTFLASH_INVALID_PARAMETER;
        }
        offset += (size_t)len;
    }

    ret = snprintf(buf + offset, buf_size - offset, "]}\n");
    if ((ret <= 0) || ((size_t)ret >= (buf_size - offset))) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    offset += (size_t)ret;

    ret = baji_photo_store_replace_index_file_locked(buf, (unsigned int)offset);
    liot_rtos_free(buf);
    return ret;
}

static int baji_photo_store_load_index_locked(baji_photo_manifest_item_t *items,
                                              unsigned int max_items,
                                              unsigned int *out_count)
{
    liot_stat_ext_s st;
    char *buf = 0;
    LFILE_EXT fd;
    unsigned int count = 0;
    unsigned int reject_count = 0u;
    unsigned int seen_count = 0u;
    int read_len;
    int ret;

    if (out_count != 0) {
        *out_count = 0;
    }
    if (out_count == 0 || ((items == 0) && (max_items != 0u))) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(BAJI_PHOTO_INDEX_FILE, &st);
    if (ret == LIOT_EXTFLASH_NOT_EXIST || ret == LIOT_EXTFLASH_STAT_FAIL) {
        return 0;
    }

    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE)) {
        return ret;
    }
    if ((st.size == 0u) || (st.size > BAJI_PHOTO_HTTP_MANIFEST_MAX)) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == 0) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(BAJI_PHOTO_INDEX_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }
    buf[st.size] = '\0';

    {
        const char *end = buf + st.size;
        const char *array_start = NULL;
        const char *array_end = NULL;
        const char *p;

        if (baji_photo_store_find_result_array(buf, end, &array_start, &array_end) != 0) {
            liot_rtos_free(buf);
            return LIOT_EXTFLASH_SIZE_FAIL;
        }

        p = array_start;
        while ((p < array_end) && (count < max_items)) {
            const char *obj_start = baji_photo_store_find_char(p, array_end, '{');
            const char *obj_end;

            if (obj_start == 0 || obj_start >= array_end) {
                break;
            }
            obj_end = baji_photo_store_find_char(obj_start, array_end, '}');
            if (obj_end == 0 || obj_end > array_end) {
                break;
            }
            ++obj_end;
            ++seen_count;
            if (baji_photo_store_parse_index_item(obj_start, obj_end, &items[count]) == 0) {
                ++count;
            } else {
                ++reject_count;
            }
            p = obj_end;
        }
    }

    liot_rtos_free(buf);
    *out_count = count;
    BAJI_PHOTO_STORE_TRACE("index load done count=%u rejected=%u seen=%u ret=0",
                           count,
                           reject_count,
                           seen_count);
    baji_photo_store_marker_index_load(count,
                                       reject_count,
                                       seen_count,
                                       max_items);
    return 0;
}

int baji_photo_store_save_index(const baji_photo_manifest_item_t *items,
                                unsigned int count)
{
    int ret;

    BAJI_PHOTO_STORE_TRACE("index save start count=%u file=%s",
                           (unsigned int)count,
                           BAJI_PHOTO_INDEX_FILE);

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_save_index_locked(items, count);
    baji_photo_store_access_end();
    BAJI_PHOTO_STORE_TRACE("index save done count=%u ret=%d",
                           (unsigned int)count,
                           ret);
    return ret;
}

int baji_photo_store_load_index(baji_photo_manifest_item_t *items,
                                unsigned int max_items,
                                unsigned int *out_count)
{
    int ret;

    if (out_count != NULL) {
        *out_count = 0u;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_store_load_index_locked(items, max_items, out_count);
    baji_photo_store_access_end();
    return ret;
}

int baji_photo_store_upsert_item(const baji_photo_manifest_item_t *item,
                                 unsigned int *out_count)
{
    baji_photo_manifest_item_t *items = NULL;
    unsigned int count = 0u;
    size_t items_bytes;
    int i;
    int idx = -1;
    int ret;

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if (item == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    items_bytes = sizeof(*items) * BAJI_PHOTO_DL_MAX;
    items = (baji_photo_manifest_item_t *)liot_rtos_malloc(items_bytes);
    if (items == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }
    memset(items, 0, items_bytes);

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        liot_rtos_free(items);
        return ret;
    }

    ret = baji_photo_store_load_index_locked(items, BAJI_PHOTO_DL_MAX, &count);
    if (ret != 0) {
        goto out;
    }

    for (i = 0; i < (int)count; ) {
        if (!baji_photo_store_item_id_matches(&items[i], item->id)) {
            ++i;
            continue;
        }
        if (idx < 0) {
            items[i] = *item;
            idx = i;
            ++i;
            continue;
        }
        memmove(&items[i],
                &items[i + 1],
                (count - (unsigned int)i - 1u) * sizeof(*items));
        --count;
    }

    if (idx < 0) {
        if (count >= BAJI_PHOTO_DL_MAX) {
            ret = LIOT_EXTFLASH_NO_SPACE;
            goto out;
        }
        items[count++] = *item;
    }

    ret = baji_photo_store_save_index_locked(items, count);
    if ((ret == 0) && (out_count != NULL)) {
        *out_count = count;
    }

out:
    baji_photo_store_access_end();
    liot_rtos_free(items);
    return ret;
}

int baji_photo_store_delete_item_and_index(const baji_photo_manifest_item_t *item,
                                           baji_photo_store_delete_result_t *out_result)
{
    baji_photo_manifest_item_t *items = NULL;
    baji_photo_store_delete_journal_t journal;
    unsigned int count = 0u;
    size_t items_bytes;
    int idx;
    int ret;
    int cleanup_ret;

    if (out_result != NULL) {
        memset(out_result, 0, sizeof(*out_result));
    }
    if (item == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    items_bytes = sizeof(*items) * BAJI_PHOTO_DL_MAX;
    items = (baji_photo_manifest_item_t *)liot_rtos_malloc(items_bytes);
    if (items == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }
    memset(items, 0, items_bytes);
    memset(&journal, 0, sizeof(journal));

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        liot_rtos_free(items);
        return ret;
    }

    ret = baji_photo_store_load_index_locked(items, BAJI_PHOTO_DL_MAX, &count);
    if (ret != 0) {
        goto out;
    }

    idx = baji_photo_store_find_item(items, count, item->id, item->format);
    if (idx < 0) {
        ret = LIOT_EXTFLASH_NOT_EXIST;
        goto out;
    }

    (void)snprintf(journal.id, sizeof(journal.id), "%s", items[idx].id);
    journal.format = items[idx].format;
    journal.crc32 = items[idx].crc32;
    journal.file_size = items[idx].file_size;
    journal.txn_id = (uint32_t)baji_photo_diag_next_id();

    ret = baji_photo_store_save_delete_journal_locked(&journal);
    if (ret != 0) {
        goto out;
    }

    memmove(&items[idx],
            &items[idx + 1],
            (count - (unsigned int)idx - 1u) * sizeof(*items));
    --count;

    ret = baji_photo_store_save_index_locked(items, count);
    if (ret != 0) {
        (void)baji_photo_store_clear_delete_journal_locked();
        goto out;
    }

    cleanup_ret = baji_photo_store_remove_item_file_locked(journal.id, journal.format);
    if (cleanup_ret == 0) {
        (void)baji_photo_store_clear_delete_journal_locked();
    } else {
        BAJI_PHOTO_STORE_TRACE("delete commit pending cleanup id=%s format=%s ret=%d",
                               journal.id,
                               baji_photo_store_format_name(journal.format),
                               cleanup_ret);
        if (out_result != NULL) {
            out_result->cleanup_pending = true;
        }
    }

    if (out_result != NULL) {
        out_result->remaining_count = count;
    }
    ret = 0;

out:
    baji_photo_store_access_end();
    liot_rtos_free(items);
    return ret;
}
