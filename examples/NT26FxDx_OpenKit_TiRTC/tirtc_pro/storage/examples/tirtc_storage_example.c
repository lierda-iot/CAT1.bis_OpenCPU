#include "tirtc_storage_example.h"
#include "../tirtc_storage.h"
#include <string.h>

int tirtc_storage_example_append_and_verify(void)
{
    static const char record[] = "TiRTC external Flash OK\n";
    char readback[sizeof(record) - 1U];
    tirtc_storage_snapshot_t snapshot;
    int fd, offset, result, close_result;

    /* Startup already owns initialization; do not initialize SPI again. */
    tirtc_storage_get_snapshot(&snapshot);
    if (!snapshot.filesystem_valid) return TIRTC_STORAGE_E_NOT_READY;
    if (snapshot.read_only) return TIRTC_STORAGE_E_PROTECTED;

    /* a+ preserves earlier records; w/w+ would truncate an existing file. */
    fd = tirtc_storage_open("/tirtc-example.txt", "a+");
    if (fd < 0) return fd;
    offset = tirtc_storage_seek(fd, 0, 2); /* SEEK_END */
    if (offset < 0) { result = offset; goto done; }
    result = tirtc_storage_write(fd, record, sizeof(record) - 1U);
    if (result < 0) goto done;
    if (result != (int)sizeof(readback)) { result = TIRTC_STORAGE_E_IO; goto done; }
    result = tirtc_storage_sync(fd);
    if (result < 0) goto done;
    result = tirtc_storage_seek(fd, offset, 0); /* SEEK_SET */
    if (result < 0) goto done;
    if (result != offset) { result = TIRTC_STORAGE_E_IO; goto done; }
    result = tirtc_storage_read(fd, readback, sizeof(readback));
    if (result < 0) goto done;
    result = result == (int)sizeof(readback) &&
             memcmp(record, readback, sizeof(readback)) == 0
                 ? 0 : TIRTC_STORAGE_E_IO;
done:
    /* A storage fault may already have invalidated this descriptor. Keep
     * the original error; never replay an uncertain append automatically. */
    close_result = tirtc_storage_close(fd);
    return result < 0 ? result : close_result;
}
