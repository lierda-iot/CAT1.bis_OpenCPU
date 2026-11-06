#include "asset_installer.h"
#include "asset_io.h"
#include "asset_manifest.h"
#include "../storage/tirtc_storage.h"
#include "liot_log.h"
#include "liot_os.h"
#include <stdbool.h>

static int write_asset(const char *path, const uint8_t *blob, uint32_t size, uint32_t crc)
{
    uint32_t offset = 0;
    int result = 0, closed;
    int fd = tirtc_storage_open(path, "w");
    if (fd < 0) return fd;
    while (offset < size) {
        uint32_t amount = size - offset;
        if (amount > 4096U) amount = 4096U;
        int count = tirtc_storage_write(fd, blob + offset, amount);
        /* A partial write is a failure; do not replay any failed operation. */
        if (count < 0 || (uint32_t)count != amount) {
            result = count < 0 ? count : TIRTC_STORAGE_E_IO;
            break;
        }
        offset += amount;
        liot_rtos_task_sleep_ms(1U);
    }
    if (!result) result = tirtc_storage_sync(fd);
    closed = tirtc_storage_close(fd);
    if (!result) result = closed;
    if (result) return result;
    return tirtc_asset_read_verified(path, NULL, size, crc, NULL);
}

typedef struct {
    const char *path;
    const uint8_t *blob;
    uint32_t size, crc;
} install_asset_t;
#define INSTALL_ENTRY(key, name, path) \
    {path, tirtc_install_##name, TIRTC_##key##_BYTES, TIRTC_##key##_CRC32},
static const install_asset_t s_install_assets[] = {
    TIRTC_UI_ASSET_LIST(INSTALL_ENTRY)
};
#undef INSTALL_ENTRY

int tirtc_assets_install(void)
{
    tirtc_storage_snapshot_t storage;
    bool replace[TIRTC_UI_ASSET_COUNT];
    int result;
    unsigned changed = 0;
    uint32_t needed = 8192U; /* Conservative allowance for metadata/COW. */
    tirtc_storage_get_snapshot(&storage);
    if (!storage.filesystem_valid || (storage.state != TIRTC_STORAGE_STATE_READY &&
        storage.state != TIRTC_STORAGE_STATE_READY_LIMITED)) return TIRTC_STORAGE_E_NOT_READY;
    /* Validate the entire embedded set before the first filesystem mutation. */
    for (unsigned i = 0; i < TIRTC_UI_ASSET_COUNT; ++i) {
        const install_asset_t *asset = &s_install_assets[i];
        if ((tirtc_asset_crc_update(UINT32_MAX, asset->blob, asset->size) ^ UINT32_MAX) != asset->crc)
            return TIRTC_ASSET_E_MISMATCH;
    }
    for (unsigned i = 0; i < TIRTC_UI_ASSET_COUNT; ++i) {
        const install_asset_t *asset = &s_install_assets[i];
        result = tirtc_asset_read_verified(asset->path, NULL, asset->size, asset->crc, NULL);
        if (result && result != TIRTC_ASSET_E_MISSING && result != TIRTC_ASSET_E_MISMATCH) return result;
        replace[i] = result != 0;
        if (replace[i]) {
            ++changed;
            needed += (asset->size + 4095U) & ~4095U;
        }
    }
    if (changed) {
        if (storage.read_only) return TIRTC_STORAGE_E_PROTECTED;
        /* Re-sample after validation; the standalone installer is the only
         * filesystem client. No auto-format or retry of failed writes. */
        tirtc_storage_poll();
        tirtc_storage_get_snapshot(&storage);
        if (!storage.filesystem_valid || storage.read_only) return TIRTC_STORAGE_E_NOT_READY;
        if (storage.filesystem_free_bytes < needed) return TIRTC_ASSET_E_SPACE;
        for (unsigned i = 0; i < TIRTC_UI_ASSET_COUNT; ++i) {
            const install_asset_t *asset = &s_install_assets[i];
            if (!replace[i]) continue;
            result = write_asset(asset->path, asset->blob, asset->size, asset->crc);
            if (result) return result;
        }
    }
    /* No success marker unless every member of the matching set verifies. */
    for (unsigned i = 0; i < TIRTC_UI_ASSET_COUNT; ++i) {
        const install_asset_t *asset = &s_install_assets[i];
        result = tirtc_asset_read_verified(asset->path, NULL, asset->size, asset->crc, NULL);
        if (result) return result;
    }
    liot_trace("[assets14] INSTALL_OK files=%u updated=%u skipped=%u\r\n",
               (unsigned)TIRTC_UI_ASSET_COUNT, changed, (unsigned)TIRTC_UI_ASSET_COUNT - changed);
    return 0;
}
