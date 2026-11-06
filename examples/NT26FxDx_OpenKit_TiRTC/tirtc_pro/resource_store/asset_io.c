#include "asset_io.h"
#include "../storage/tirtc_storage.h"
#include "liot_os.h"
#include <stddef.h>
#include <stdbool.h>

/* Normal firmware has one resource worker; the installer is a separate image.
 * Still reject overlapping callers explicitly: a deferred close token must
 * never be overwritten by a second verification. No I/O runs under this lock. */
static bool s_asset_read_active;
static int s_asset_pending_close = -1;
static int s_asset_pending_error;

static bool asset_read_enter(void)
{
    bool available;
    liot_rtos_enter_critical();
    available = !s_asset_read_active;
    if (available) s_asset_read_active = true;
    liot_rtos_exit_critical();
    return available;
}

static void asset_read_leave(void)
{
    liot_rtos_enter_critical(); s_asset_read_active = false; liot_rtos_exit_critical();
}

uint32_t tirtc_asset_crc_update(uint32_t crc, const uint8_t *data, uint32_t size)
{
    while (size--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

int tirtc_asset_read_verified(const char *path, uint8_t *destination,
                             uint32_t size, uint32_t expected_crc,
                             void (*progress)(uint32_t bytes))
{
    uint8_t scratch[512];
    uint32_t offset = 0, crc = UINT32_MAX;
    int fd, result, closed;
    if (!path || !size || size > INT32_MAX) return TIRTC_STORAGE_E_ARGUMENT;
    if (!asset_read_enter()) return TIRTC_STORAGE_E_BUSY;
    if (s_asset_pending_close >= 0) {
        result = tirtc_storage_close(s_asset_pending_close);
        if (result == TIRTC_STORAGE_E_BUSY) goto done;
        s_asset_pending_close = -1;
        if (s_asset_pending_error) {
            /* Report the original failure only after retiring its descriptor.
             * Otherwise a terminal CRC error would stop the sole loader while
             * its BUSY close still owns a scarce storage slot. */
            result = s_asset_pending_error; s_asset_pending_error = 0;
            goto done;
        }
        /* Recovery may already have invalidated this non-reusable token. */
        if (result && result != -9) goto done;
    }
    fd = tirtc_storage_open(path, "r");
    if (fd < 0) { result = fd; goto done; }
    result = tirtc_storage_seek(fd, 0, 2);
    if (result >= 0 && (uint32_t)result != size) result = TIRTC_ASSET_E_MISMATCH;
    else if (result >= 0) result = tirtc_storage_seek(fd, 0, 0);
    if (result > 0) result = TIRTC_STORAGE_E_IO;
    while (result == 0 && offset < size) {
        uint32_t amount = size - offset;
        uint32_t limit = destination ? 4096U : (uint32_t)sizeof(scratch);
        uint8_t *buffer = destination ? destination + offset : scratch;
        if (amount > limit) amount = limit;
        int count = tirtc_storage_read(fd, buffer, amount);
        if (count <= 0 || (uint32_t)count > amount) {
            result = count < 0 ? count : TIRTC_STORAGE_E_IO;
            break;
        }
        crc = tirtc_asset_crc_update(crc, buffer, (uint32_t)count);
        offset += (uint32_t)count;
        if (progress) progress((uint32_t)count);
        /* Give normal UI/network tasks a scheduling point between chunks. */
        liot_rtos_task_sleep_ms(1U);
    }
    if (!result && (crc ^ UINT32_MAX) != expected_crc) result = TIRTC_ASSET_E_MISMATCH;
    closed = tirtc_storage_close(fd);
    if (closed == TIRTC_STORAGE_E_BUSY) {
        s_asset_pending_close = fd; s_asset_pending_error = result;
        result = TIRTC_STORAGE_E_BUSY;
        goto done;
    }
    /* A completed close never overrides the original read/CRC failure. */
    if (!result) result = closed;
done:
    asset_read_leave();
    return result;
}
