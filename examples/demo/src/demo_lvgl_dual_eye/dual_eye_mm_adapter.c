/**
 * @file dual_eye_mm_adapter.c
 * @brief Minimal platform multimedia compatibility layer for GIF decoding.
 */

#include <stdint.h>
#include <string.h>

#include "liot_os.h"

typedef struct
{
    uint8_t *base;
    uint32_t size;
    uint32_t offset;
    uint32_t len;
} dual_eye_dbuf_t;

void *Sys_MemAlloc2(uint32_t size)
{
    return liot_rtos_malloc(size);
}

void *Sys_MemAllocClean2(uint32_t size)
{
    void *ptr = liot_rtos_malloc(size);

    if (ptr != NULL) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void Sys_MemFree2(void *ptr)
{
    if (ptr != NULL) {
        liot_rtos_free(ptr);
    }
}

void *Sys_MemCpy(void *dst, const void *src, uint32_t len)
{
    return memcpy(dst, src, len);
}

void *Sys_MemSet(void *dst, int val, uint32_t len)
{
    return memset(dst, val, len);
}

int Sys_MemCmp(const void *buf1, const void *buf2, uint32_t len)
{
    return (memcmp(buf1, buf2, len) == 0) ? 0 : -1;
}

void Dbuf_MoveToHead(void *dbuf)
{
    dual_eye_dbuf_t *buf = (dual_eye_dbuf_t *)dbuf;

    if (buf == NULL || buf->base == NULL || buf->offset == 0U ||
        buf->len == 0U) {
        if (buf != NULL) {
            buf->offset = 0U;
        }
        return;
    }

    memmove(buf->base, buf->base + buf->offset, buf->len);
    buf->offset = 0U;
}
