#include "baji_photo_vpu_img.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "liot_external_flash_fs.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_os.h"
#include "mm_jpeg_if.h"
#include "mm_video_if.h"

#include "baji_photo_diag.h"
#include "baji_photo_http.h"
#include "baji_photo_store.h"

#define BAJI_PHOTO_VPU_TRACE(fmt, ...) liot_trace("[baji_vpu] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)

static unsigned long baji_photo_vpu_heap_free(void)
{
    return (unsigned long)liot_xPortGetFreeHeapSize();
}

static unsigned long baji_photo_vpu_heap_min(void)
{
    return (unsigned long)liot_xPortGetMinimumEverFreeHeapSize();
}

static unsigned long baji_photo_vpu_heap_max(void)
{
    return (unsigned long)liot_xPortGetMaximumFreeBlockSize();
}

static int baji_photo_vpu_reserve_output(uint32_t pixel_bytes)
{
    unsigned long heap_max;

    if (pixel_bytes == 0u) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    heap_max = baji_photo_vpu_heap_max();
    if (heap_max < (unsigned long)pixel_bytes) {
        return LIOT_EXTFLASH_NO_SPACE;
    }

    return 0;
}

static bool baji_photo_vpu_dims_valid(uint32_t width, uint32_t height)
{
    return (width > 0u) && (height > 0u) &&
           (width <= BAJI_PHOTO_IMG_W) && (height <= BAJI_PHOTO_IMG_H);
}

static uint32_t baji_photo_vpu_max_input_size(baji_photo_format_t format)
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

static int baji_photo_vpu_rgb565_size(uint32_t width,
                                      uint32_t height,
                                      uint32_t *out_size)
{
    uint32_t pixels;

    if ((out_size == NULL) || !baji_photo_vpu_dims_valid(width, height)) {
        return -1;
    }
    if ((height != 0u) && (width > (UINT32_MAX / height))) {
        return -1;
    }

    pixels = width * height;
    if (pixels > (UINT32_MAX / BAJI_PHOTO_IMG_BPP)) {
        return -1;
    }

    *out_size = pixels * BAJI_PHOTO_IMG_BPP;
    return 0;
}

bool baji_photo_vpu_img_format_is_supported(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_JPEG:
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        return true;
#else
        return false;
#endif
    case BAJI_PHOTO_FORMAT_PNG:
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        return true;
#else
        return false;
#endif
    default:
        return false;
    }
}

#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
static bool baji_photo_vpu_jpeg_marker_has_length(uint8_t marker)
{
    if (marker == 0x01u) {
        return false;
    }
    if ((marker >= 0xD0u) && (marker <= 0xD9u)) {
        return false;
    }
    return false == ((marker == 0xD8u) || (marker == 0xD9u));
}

static bool baji_photo_vpu_jpeg_marker_is_sof(uint8_t marker)
{
    switch (marker) {
    case 0xC0u:
    case 0xC1u:
    case 0xC2u:
    case 0xC3u:
    case 0xC5u:
    case 0xC6u:
    case 0xC7u:
    case 0xC9u:
    case 0xCAu:
    case 0xCBu:
    case 0xCDu:
    case 0xCEu:
    case 0xCFu:
        return true;
    default:
        return false;
    }
}

static int baji_photo_vpu_jpeg_validate_baseline(const uint8_t *data, unsigned int len)
{
    unsigned int pos = 2u;

    if ((data == NULL) || (len < 4u)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
    }
    if ((data[0] != 0xFFu) || (data[1] != 0xD8u)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
    }

    while ((pos + 1u) < len) {
        uint8_t marker;
        unsigned int seg_len;

        while ((pos < len) && (data[pos] != 0xFFu)) {
            ++pos;
        }
        if ((pos + 1u) >= len) {
            break;
        }

        do {
            ++pos;
        } while ((pos < len) && (data[pos] == 0xFFu));
        if (pos >= len) {
            break;
        }

        marker = data[pos++];
        if (marker == 0x00u) {
            continue;
        }
        if (marker == 0xDAu) {
            return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
        }
        if (baji_photo_vpu_jpeg_marker_is_sof(marker)) {
            return (marker == 0xC0u) ? 0 : BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
        }
        if (!baji_photo_vpu_jpeg_marker_has_length(marker)) {
            continue;
        }
        if ((pos + 1u) >= len) {
            return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
        }

        seg_len = ((unsigned int)data[pos] << 8) | (unsigned int)data[pos + 1u];
        if ((seg_len < 2u) || ((len - pos) < seg_len)) {
            return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
        }
        pos += seg_len;
    }

    return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
}

static int baji_photo_vpu_jpeg_decode_info(const uint8_t *data,
                                           unsigned int len,
                                           uint32_t *out_width,
                                           uint32_t *out_height)
{
    JPEG_INFO info;
    void *decoder;
    int ret;

    if ((data == NULL) || (out_width == NULL) || (out_height == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    ret = baji_photo_vpu_jpeg_validate_baseline(data, len);
    if (ret != 0) {
        return ret;
    }

    decoder = JpegD_Create();
    if (decoder == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    memset(&info, 0, sizeof(info));
    ret = JpegD_DecodeInfo(decoder, (unsigned char *)data, len, &info);
    JpegD_Destroy(decoder);
    if (ret != 0) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
    }

    *out_width = info.uWidth;
    *out_height = info.uHeight;
    return 0;
}
#endif

#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
static int baji_photo_vpu_png_decode_info(const uint8_t *data,
                                          unsigned int len,
                                          uint32_t *out_width,
                                          uint32_t *out_height)
{
    PNG_INFO info;
    void *decoder;
    int ret;

    if ((data == NULL) || (out_width == NULL) || (out_height == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    decoder = PngD_Create();
    if (decoder == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    memset(&info, 0, sizeof(info));
    ret = PngD_DecodeInfo(decoder, (unsigned char *)data, len, &info);
    PngD_Destroy(decoder);
    if (ret != 0) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }

    *out_width = info.uWidth;
    *out_height = info.uHeight;
    return 0;
}
#endif

int baji_photo_vpu_img_validate_payload(const baji_photo_manifest_item_t *item,
                                        const uint8_t *data,
                                        unsigned int len)
{
    uint32_t crc;
    uint32_t decoded_w = 0u;
    uint32_t decoded_h = 0u;
    uint32_t max_input_size;
    int ret;

    if ((item == NULL) || (data == NULL) || (len == 0u)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (!baji_photo_vpu_img_format_is_supported(item->format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    max_input_size = baji_photo_vpu_max_input_size(item->format);
    if ((max_input_size == 0u) || (len != item->file_size) || (len > max_input_size)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }
    if (!baji_photo_vpu_dims_valid(item->width, item->height)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }

    crc = baji_photo_store_crc32_update(0xFFFFFFFFu, data, len);
    crc = baji_photo_store_crc32_finish(crc);
    if (crc != item->crc32) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }

    if (item->format == BAJI_PHOTO_FORMAT_JPEG) {
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        ret = baji_photo_vpu_jpeg_decode_info(data, len, &decoded_w, &decoded_h);
#else
        return LIOT_HTTPC_ERR_INVALID_PARAM;
#endif
    } else if (item->format == BAJI_PHOTO_FORMAT_PNG) {
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        ret = baji_photo_vpu_png_decode_info(data, len, &decoded_w, &decoded_h);
#else
        return LIOT_HTTPC_ERR_INVALID_PARAM;
#endif
    } else {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (ret != 0) {
        return ret;
    }

    if ((decoded_w != item->width) || (decoded_h != item->height)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL;
    }

    return 0;
}

static int baji_photo_vpu_img_resolve_path(const baji_photo_manifest_item_t *item,
                                           char *out_path,
                                           unsigned int out_len)
{
    int written;

    if ((item == NULL) || (out_path == NULL) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (item->local_path[0] != '\0') {
        written = snprintf(out_path, out_len, "%s", item->local_path);
        if ((written < 0) || ((unsigned int)written >= out_len)) {
            return LIOT_EXTFLASH_INVALID_PARAMETER;
        }
        return 0;
    }

    return baji_photo_store_build_item_path(item->id, item->format, out_path, out_len);
}

static int baji_photo_vpu_img_load_file(const baji_photo_manifest_item_t *item,
                                        char *path,
                                        unsigned int path_len,
                                        uint8_t **out_src_buf,
                                        unsigned int *out_src_len)
{
    liot_stat_ext_s st;
    LFILE_EXT fd = 0;
    uint8_t *src_buf = NULL;
    uint32_t max_input_size;
    int read_len;
    int ret;

    if ((item == NULL) || (path == NULL) || (out_src_buf == NULL) || (out_src_len == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    *out_src_buf = NULL;
    *out_src_len = 0u;

    ret = baji_photo_vpu_img_resolve_path(item, path, path_len);
    if (ret != 0) {
        return ret;
    }

    max_input_size = baji_photo_vpu_max_input_size(item->format);
    if ((max_input_size == 0u) || (item->file_size == 0u) || (item->file_size > max_input_size)) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(path, &st);
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size != item->file_size)) {
        baji_photo_store_access_end();
        return (ret == LIOT_EXTFLASH_OK) ? LIOT_EXTFLASH_SIZE_FAIL : ret;
    }

    src_buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
    if (src_buf == NULL) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        liot_rtos_free(src_buf);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(src_buf, (unsigned int)st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    baji_photo_store_access_end();
    if (read_len != (int)st.size) {
        liot_rtos_free(src_buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }

    *out_src_buf = src_buf;
    *out_src_len = (unsigned int)st.size;
    return 0;
}

#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
static int baji_photo_vpu_img_decode_jpeg(const baji_photo_manifest_item_t *item,
                                          const uint8_t *src_buf,
                                          unsigned int src_len,
                                          uint8_t **out_pixels,
                                          uint32_t *out_pixel_bytes)
{
    JPEG_INFO info;
    JPEG_IMAGE_BUF out;
    uint8_t *pixels = NULL;
    void *decoder = NULL;
    uint32_t pixel_bytes = 0u;
    int ret = LIOT_EXTFLASH_ERROR_GENERAL;

    if ((item == NULL) || (src_buf == NULL) || (out_pixels == NULL) || (out_pixel_bytes == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (baji_photo_vpu_rgb565_size(item->width, item->height, &pixel_bytes) != 0) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }
    ret = baji_photo_vpu_reserve_output(pixel_bytes);
    if (ret != 0) {
        return ret;
    }

    decoder = JpegD_Create();
    if (decoder == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    memset(&info, 0, sizeof(info));
    ret = JpegD_DecodeInfo(decoder, (unsigned char *)src_buf, src_len, &info);
    if ((ret != 0) || (info.uWidth != item->width) || (info.uHeight != item->height)) {
        ret = LIOT_EXTFLASH_SIZE_FAIL;
        goto cleanup;
    }

    pixels = (uint8_t *)liot_rtos_malloc(pixel_bytes);
    if (pixels == NULL) {
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }
    memset(pixels, 0, pixel_bytes);

    memset(&out, 0, sizeof(out));
    out.eFmt = JPEG_COLOR_FMT_RGB565;
    out.uWidth = info.uWidth;
    out.uHeight = info.uHeight;
    out.pData[0] = pixels;

    ret = JpegD_DecodeImage(decoder, &out);
    if (ret != 0) {
        ret = LIOT_EXTFLASH_READ_FAIL;
        goto cleanup;
    }

    *out_pixels = pixels;
    *out_pixel_bytes = pixel_bytes;
    pixels = NULL;
    ret = 0;

cleanup:
    if (decoder != NULL) {
        JpegD_Destroy(decoder);
    }
    if (pixels != NULL) {
        liot_rtos_free(pixels);
    }
    return ret;
}
#endif

#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
static int baji_photo_vpu_img_decode_png(const baji_photo_manifest_item_t *item,
                                         const uint8_t *src_buf,
                                         unsigned int src_len,
                                         uint8_t **out_pixels,
                                         uint32_t *out_pixel_bytes)
{
    PNG_INFO info;
    VIDEO_IMAGE_BUF out;
    uint8_t *pixels = NULL;
    void *decoder = NULL;
    uint32_t pixel_bytes = 0u;
    int ret = LIOT_EXTFLASH_ERROR_GENERAL;

    if ((item == NULL) || (src_buf == NULL) || (out_pixels == NULL) || (out_pixel_bytes == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (baji_photo_vpu_rgb565_size(item->width, item->height, &pixel_bytes) != 0) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    decoder = PngD_Create();
    if (decoder == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    memset(&info, 0, sizeof(info));
    ret = PngD_DecodeInfo(decoder, (unsigned char *)src_buf, src_len, &info);
    if ((ret != 0) || (info.uWidth != item->width) || (info.uHeight != item->height)) {
        ret = LIOT_EXTFLASH_SIZE_FAIL;
        goto cleanup;
    }

    pixels = (uint8_t *)liot_rtos_malloc(pixel_bytes);
    if (pixels == NULL) {
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }
    memset(pixels, 0, pixel_bytes);

    memset(&out, 0, sizeof(out));
    out.eFmt = VIDEO_COLOR_FMT_RGB565;
    out.uWidth = (unsigned short)info.uWidth;
    out.uHeight = (unsigned short)info.uHeight;
    out.pData[0] = pixels;

    ret = PngD_DecodeImage(decoder, &out);
    if (ret != 0) {
        ret = LIOT_EXTFLASH_READ_FAIL;
        goto cleanup;
    }

    *out_pixels = pixels;
    *out_pixel_bytes = pixel_bytes;
    pixels = NULL;
    ret = 0;

cleanup:
    if (decoder != NULL) {
        PngD_Destroy(decoder);
    }
    if (pixels != NULL) {
        liot_rtos_free(pixels);
    }
    return ret;
}
#endif

int baji_photo_vpu_img_load(const baji_photo_manifest_item_t *item,
                            uint32_t trace_id,
                            uint32_t dyn_seq,
                            lv_img_dsc_t *out_dsc,
                            void **out_buf)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    uint8_t *src_buf = NULL;
    uint8_t *pixels = NULL;
    unsigned int src_len = 0u;
    uint32_t pixel_bytes = 0u;
    int ret;

    if ((item == NULL) || (out_dsc == NULL) || (out_buf == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if (!baji_photo_vpu_img_format_is_supported(item->format)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if (!baji_photo_vpu_dims_valid(item->width, item->height)) {
        BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_FAIL op=%lu dyn=%lu item=%s stage=dim_invalid ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_SIZE_FAIL,
                              baji_photo_vpu_heap_free(),
                              baji_photo_vpu_heap_max());
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    memset(out_dsc, 0, sizeof(*out_dsc));
    *out_buf = NULL;

    BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_BEGIN op=%lu dyn=%lu item=%s fmt=%s file=%lu heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (item->format == BAJI_PHOTO_FORMAT_JPEG) ? "jpeg" : "png",
                          (unsigned long)item->file_size,
                          baji_photo_vpu_heap_free(),
                          baji_photo_vpu_heap_max());

    ret = baji_photo_vpu_img_load_file(item,
                                       path,
                                       sizeof(path),
                                       &src_buf,
                                       &src_len);
    if (ret != 0) {
        BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_FAIL op=%lu dyn=%lu item=%s stage=file_read ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              ret,
                              baji_photo_vpu_heap_free(),
                              baji_photo_vpu_heap_max());
        return ret;
    }

    BAJI_PHOTO_VPU_TRACE("op=%lu dyn=%lu load id=%s fmt=%s src=%u dims=%ux%u heap_free=%lu heap_min=%lu heap_max=%lu",
                         (unsigned long)trace_id,
                         (unsigned long)dyn_seq,
                         item->id,
                         (item->format == BAJI_PHOTO_FORMAT_JPEG) ? "jpeg" : "png",
                         src_len,
                         (unsigned int)item->width,
                         (unsigned int)item->height,
                         baji_photo_vpu_heap_free(),
                         baji_photo_vpu_heap_min(),
                         baji_photo_vpu_heap_max());

    ret = baji_photo_vpu_rgb565_size(item->width, item->height, &pixel_bytes);
    if (ret != 0) {
        liot_rtos_free(src_buf);
        src_buf = NULL;
        BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_FAIL op=%lu dyn=%lu item=%s stage=pixel_size ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_SIZE_FAIL,
                              baji_photo_vpu_heap_free(),
                              baji_photo_vpu_heap_max());
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    ret = baji_photo_vpu_reserve_output(pixel_bytes);
    if (ret != 0) {
        liot_rtos_free(src_buf);
        src_buf = NULL;
        BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_FAIL op=%lu dyn=%lu item=%s stage=heap_reserve ret=%d need=%lu heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              ret,
                              (unsigned long)pixel_bytes,
                              baji_photo_vpu_heap_free(),
                              baji_photo_vpu_heap_max());
        return ret;
    }

    if (item->format == BAJI_PHOTO_FORMAT_JPEG) {
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        ret = baji_photo_vpu_img_decode_jpeg(item, src_buf, src_len, &pixels, &pixel_bytes);
#else
        ret = LIOT_EXTFLASH_INVALID_PARAMETER;
#endif
    } else {
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        ret = baji_photo_vpu_img_decode_png(item, src_buf, src_len, &pixels, &pixel_bytes);
#else
        ret = LIOT_EXTFLASH_INVALID_PARAMETER;
#endif
    }
    liot_rtos_free(src_buf);
    src_buf = NULL;

    if (ret != 0) {
        BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_FAIL op=%lu dyn=%lu item=%s stage=decode ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              ret,
                              baji_photo_vpu_heap_free(),
                              baji_photo_vpu_heap_max());
        return ret;
    }

    out_dsc->header.always_zero = 0;
    out_dsc->header.w = item->width;
    out_dsc->header.h = item->height;
    out_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    out_dsc->data_size = pixel_bytes;
    out_dsc->data = pixels;
    *out_buf = pixels;

    BAJI_PHOTO_MARK_TRACE("UI_VPU_LOAD_OK op=%lu dyn=%lu item=%s fmt=%s bytes=%lu buf=%p heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (item->format == BAJI_PHOTO_FORMAT_JPEG) ? "jpeg" : "png",
                          (unsigned long)pixel_bytes,
                          pixels,
                          baji_photo_vpu_heap_free(),
                          baji_photo_vpu_heap_max());
    return 0;
}

void baji_photo_vpu_img_release(void *buf)
{
    if (buf != NULL) {
        liot_rtos_free(buf);
    }
}
