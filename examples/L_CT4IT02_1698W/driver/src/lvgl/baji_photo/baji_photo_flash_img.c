#include "baji_photo_flash_img.h"

#include <string.h>

#include "liot_external_flash_fs.h"
#include "liot_log.h"
#include "liot_os.h"

#include "baji_photo_diag.h"
#include "baji_photo_store.h"

#define BAJI_PHOTO_FLASH_TRACE(fmt, ...) liot_trace("[baji_flash] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MARK_TRACE(fmt, ...)  liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)

static unsigned long baji_photo_flash_heap_free(void)
{
    return (unsigned long)liot_xPortGetFreeHeapSize();
}

static unsigned long baji_photo_flash_heap_min(void)
{
    return (unsigned long)liot_xPortGetMinimumEverFreeHeapSize();
}

static unsigned long baji_photo_flash_heap_max_block(void)
{
    return (unsigned long)liot_xPortGetMaximumFreeBlockSize();
}

int baji_photo_flash_img_load(const baji_photo_manifest_item_t *item,
                              uint32_t trace_id,
                              uint32_t dyn_seq,
                              lv_img_dsc_t *out_dsc,
                              void **out_buf)
{
    char path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    baji_photo_file_header_t header;
    LFILE_EXT fd;
    uint8_t *pixels = 0;
    unsigned long lock_wait_begin = 0u;
    unsigned long lock_wait_ms = 0u;
    int close_ret = 0;
    int ret;

    if ((item == 0) || (out_dsc == 0) || (out_buf == 0)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    *out_buf = 0;

    ret = baji_photo_store_build_photo_path(item->id, path, sizeof(path));
    if (ret != 0) {
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=build_path ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return ret;
    }

    BAJI_PHOTO_MARK_TRACE("UI_IMG_STORE_LOCK_WAIT op=%lu dyn=%lu item=%s heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    lock_wait_begin = (unsigned long)liot_rtos_get_system_tick();
    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=store_lock ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return ret;
    }

    lock_wait_ms = (unsigned long)liot_rtos_get_system_tick() - lock_wait_begin;
    BAJI_PHOTO_MARK_TRACE("UI_IMG_STORE_LOCKED op=%lu dyn=%lu item=%s wait_ms=%lu heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          lock_wait_ms,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());

    fd = liot_fopen_ext(path, "r");
    BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_OPEN op=%lu dyn=%lu item=%s fd=%ld heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (long)fd,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    if (fd <= 0) {
        baji_photo_store_access_end();
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=file_open ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_OPEN_FAIL,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    memset(&header, 0, sizeof(header));
    ret = liot_fread_ext(&header, sizeof(header), 1, fd);
    if (ret != (int)sizeof(header)) {
        close_ret = liot_fclose_ext(fd);
        BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_CLOSE op=%lu dyn=%lu item=%s ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              close_ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        baji_photo_store_access_end();
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=header_read ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_READ_FAIL,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return LIOT_EXTFLASH_READ_FAIL;
    }

    BAJI_PHOTO_FLASH_TRACE("op=%lu load start id=%s path=%s hdr=%ux%u bytes=%lu file_crc=0x%08lx expect_crc=0x%08lx "
                           "dyn=%lu heap_free=%lu heap_min=%lu heap_max=%lu",
                           (unsigned long)trace_id,
                           item->id,
                           path,
                           (unsigned int)header.width,
                           (unsigned int)header.height,
                           (unsigned long)header.data_size,
                           (unsigned long)header.crc32,
                           (unsigned long)item->crc32,
                           (unsigned long)dyn_seq,
                           baji_photo_flash_heap_free(),
                           baji_photo_flash_heap_min(),
                           baji_photo_flash_heap_max_block());

    if ((header.magic != BAJI_PHOTO_MAGIC) ||
        (header.width == 0u) ||
        (header.height == 0u) ||
        (header.width > BAJI_PHOTO_IMG_W) ||
        (header.height > BAJI_PHOTO_IMG_H) ||
        (header.data_size != ((uint32_t)header.width * (uint32_t)header.height * BAJI_PHOTO_IMG_BPP)) ||
        (header.crc32 != item->crc32)) {
        BAJI_PHOTO_FLASH_TRACE("op=%lu load reject id=%s path=%s dyn=%lu magic=0x%08lx size_ok=%d crc_match=%d",
                               (unsigned long)trace_id,
                               item->id,
                               path,
                               (unsigned long)dyn_seq,
                               (unsigned long)header.magic,
                               (header.data_size ==
                                ((uint32_t)header.width * (uint32_t)header.height * BAJI_PHOTO_IMG_BPP)) ? 1 : 0,
                               (header.crc32 == item->crc32) ? 1 : 0);
        close_ret = liot_fclose_ext(fd);
        BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_CLOSE op=%lu dyn=%lu item=%s ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              close_ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        baji_photo_store_access_end();
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=header_validate ret=%d magic=0x%08lx bytes=%lu heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_SIZE_FAIL,
                              (unsigned long)header.magic,
                              (unsigned long)header.data_size,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    BAJI_PHOTO_MARK_TRACE("UI_IMG_HEADER_OK op=%lu dyn=%lu item=%s size=%ux%u bytes=%lu heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (unsigned int)header.width,
                          (unsigned int)header.height,
                          (unsigned long)header.data_size,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());

    pixels = (uint8_t *)liot_rtos_malloc(header.data_size);
    BAJI_PHOTO_FLASH_TRACE("op=%lu alloc id=%s req=%lu buf=%p heap_free=%lu heap_min=%lu heap_max=%lu",
                           (unsigned long)trace_id,
                           item->id,
                           (unsigned long)header.data_size,
                           pixels,
                           baji_photo_flash_heap_free(),
                           baji_photo_flash_heap_min(),
                           baji_photo_flash_heap_max_block());
    BAJI_PHOTO_MARK_TRACE("UI_IMG_PIXEL_ALLOC op=%lu dyn=%lu item=%s req=%lu buf=%p heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (unsigned long)header.data_size,
                          pixels,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    if (pixels == 0) {
        close_ret = liot_fclose_ext(fd);
        BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_CLOSE op=%lu dyn=%lu item=%s ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              close_ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        baji_photo_store_access_end();
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=pixel_alloc ret=%d bytes=%lu heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_ERROR_GENERAL,
                              (unsigned long)header.data_size,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    BAJI_PHOTO_MARK_TRACE("UI_IMG_READ_BEGIN op=%lu dyn=%lu item=%s bytes=%lu buf=%p heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (unsigned long)header.data_size,
                          pixels,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    ret = liot_fread_ext(pixels, header.data_size, 1, fd);
    if (ret != (int)header.data_size) {
        close_ret = liot_fclose_ext(fd);
        BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_CLOSE op=%lu dyn=%lu item=%s ret=%d heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              close_ret,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        baji_photo_store_access_end();
        BAJI_PHOTO_FLASH_TRACE("op=%lu read fail id=%s ret=%d expect=%lu",
                               (unsigned long)trace_id,
                               item->id,
                               ret,
                               (unsigned long)header.data_size);
        liot_rtos_free(pixels);
        BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_FAIL op=%lu dyn=%lu item=%s stage=pixel_read ret=%d expect=%lu heap_free=%lu heap_max=%lu",
                              (unsigned long)trace_id,
                              (unsigned long)dyn_seq,
                              baji_photo_diag_id_tail(item->id),
                              LIOT_EXTFLASH_READ_FAIL,
                              (unsigned long)header.data_size,
                              baji_photo_flash_heap_free(),
                              baji_photo_flash_heap_max_block());
        return LIOT_EXTFLASH_READ_FAIL;
    }

    BAJI_PHOTO_MARK_TRACE("UI_IMG_READ_DONE op=%lu dyn=%lu item=%s bytes=%lu heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (unsigned long)header.data_size,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    close_ret = liot_fclose_ext(fd);
    BAJI_PHOTO_MARK_TRACE("UI_IMG_FILE_CLOSE op=%lu dyn=%lu item=%s ret=%d heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          close_ret,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    baji_photo_store_access_end();

    memset(out_dsc, 0, sizeof(*out_dsc));
    out_dsc->header.always_zero = 0;
    out_dsc->header.w = header.width;
    out_dsc->header.h = header.height;
    out_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    out_dsc->data_size = header.data_size;
    out_dsc->data = pixels;
    *out_buf = pixels;
    BAJI_PHOTO_MARK_TRACE("UI_IMG_DSC_READY op=%lu dyn=%lu item=%s size=%ux%u bytes=%lu buf=%p heap_free=%lu heap_max=%lu",
                          (unsigned long)trace_id,
                          (unsigned long)dyn_seq,
                          baji_photo_diag_id_tail(item->id),
                          (unsigned int)header.width,
                          (unsigned int)header.height,
                          (unsigned long)header.data_size,
                          pixels,
                          baji_photo_flash_heap_free(),
                          baji_photo_flash_heap_max_block());
    BAJI_PHOTO_FLASH_TRACE("op=%lu load ok id=%s size=%ux%u bytes=%lu dyn=%lu heap_free=%lu heap_min=%lu heap_max=%lu",
                           (unsigned long)trace_id,
                           item->id,
                           (unsigned int)header.width,
                           (unsigned int)header.height,
                           (unsigned long)header.data_size,
                           (unsigned long)dyn_seq,
                           baji_photo_flash_heap_free(),
                           baji_photo_flash_heap_min(),
                           baji_photo_flash_heap_max_block());
    return 0;
}

void baji_photo_flash_img_release(void *buf)
{
    if (buf != 0) {
        liot_rtos_free(buf);
    }
}
