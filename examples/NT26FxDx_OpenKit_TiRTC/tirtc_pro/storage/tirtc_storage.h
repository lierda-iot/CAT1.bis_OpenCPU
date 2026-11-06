#ifndef TIRTC_STORAGE_H
#define TIRTC_STORAGE_H
#include <stdbool.h>
#include <stdint.h>

enum {
    TIRTC_STORAGE_STATE_OFF = 0, TIRTC_STORAGE_STATE_PROBING = 1,
    TIRTC_STORAGE_STATE_READY = 2, TIRTC_STORAGE_STATE_READY_LIMITED = 3,
    TIRTC_STORAGE_STATE_ABSENT = 4, TIRTC_STORAGE_STATE_UNSUPPORTED = 5,
    TIRTC_STORAGE_STATE_UNKNOWN_DATA = 6, TIRTC_STORAGE_STATE_ERROR = 7
};
enum {
    TIRTC_STORAGE_E_IO = -5, TIRTC_STORAGE_E_BUSY = -16,
    TIRTC_STORAGE_E_ARGUMENT = -22, TIRTC_STORAGE_E_NOT_READY = -19,
    TIRTC_STORAGE_E_TIMEOUT = -110, TIRTC_STORAGE_E_PROTECTED = -30
};
typedef struct {
    uint32_t state, jedec_id, physical_total_bytes;
    uint32_t filesystem_total_bytes, filesystem_free_bytes;
    uint32_t scanned_bytes, scan_total_bytes;
    uint32_t filesystem_sample_ms, progress_ms;
    int32_t last_error;
    bool physical_valid, filesystem_valid, preserved_existing, expanded, read_only;
    /* Runtime read faults only: ERROR remains visible while a safe remount is
     * pending. next_retry_ms is a wrapping uptime deadline, not wall clock. */
    bool recovery_pending;
    uint32_t recovery_attempts, recovery_successes, next_retry_ms;
} tirtc_storage_snapshot_t;

/* Single background owner: init once after the shared 3.3 V rail is stable,
 * then poll every 5 s. Never call init/poll/file I/O from the LVGL task.
 * A runtime read I/O failure on a previously usable volume is retried with
 * 5/10/20/40/60 s backoff. Recovery only probes and mounts the original volume;
 * it never formats, grows, or replays a failed write. Initial mount failure,
 * changed identity/geometry and write-operation failures stay unavailable.
 * Initial legacy/native-256 layout probes must have exactly one fully valid
 * view; ambiguous media stays UNKNOWN_DATA without changing any bytes. */
int tirtc_storage_init(void);
void tirtc_storage_poll(void);
/* Task context only. Nonblocking copy; independent of the filesystem mutex.
 * Safe before init; callers must not use this API from an ISR. */
void tirtc_storage_get_snapshot(tirtc_storage_snapshot_t *out);

/* Two simultaneous files, fixed buffers. Descriptors >= 1, negative errors.
 * Paths are absolute LittleFS paths, e.g. /records/001.wav.
 * Modes: r, w, a, r+, w+, a+ (optionally b). Read/write return BYTES, not items.
 * Per-call transfer limit 4096 bytes; callers stream large files in chunks.
 * Descriptors are opaque positive tokens, not array slots. A media fault
 * invalidates ALL descriptors and discards their unsynced buffers without I/O.
 * close on an invalidated token returns a bad-descriptor error; it cannot flush
 * old buffers. After recovery reopen explicitly. Tokens are never reused.
 * Mutating calls can alter existing files. No public format/erase API. */
int tirtc_storage_open(const char *path, const char *mode);
int tirtc_storage_read(int fd, void *data, uint32_t size);
int tirtc_storage_write(int fd, const void *data, uint32_t size);
int tirtc_storage_seek(int fd, int32_t offset, int whence);
int tirtc_storage_sync(int fd);
int tirtc_storage_close(int fd);
int tirtc_storage_mkdir(const char *path);
int tirtc_storage_remove(const char *path);
#endif
