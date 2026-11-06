/* Slow storage is isolated from the five-second RAM/layout publisher.
 * Never terminate a blocked SDK filesystem request: its daemon may still hold
 * the caller's stack pointers. Mark a stale cache explicitly instead. */
#include "tirtc_resources.h"
#include "tirtc_storage.h"
#include "liot_os.h"
#include "liot_fs_api.h"
#include "liot_log.h"
#include <string.h>

#define RESOURCE_PERIOD_MS 5000U
#define RESOURCE_STALE_MS 15000U
#define RESOURCE_INTERNAL_TASK_PRIORITY 8U
/* External LittleFS work shares GUI priority instead of always running behind
 * redraws. Keep it below video11/audio12 and leave the independent internal-FS
 * query at its existing priority; operation budgets remain unchanged. */
#define RESOURCE_EXTERNAL_TASK_PRIORITY 10U
#define RESOURCE_INTERNAL_STACK (4U * 1024U)
#define RESOURCE_EXTERNAL_STACK (8U * 1024U)
#define RESOURCE_WORKER_ERROR (-1001)
#define RESOURCE_STALE_ERROR (-1002)
#define RESOURCE_RANGE_ERROR (-1003)

extern void tirtc_ui_platform_lock(void);
extern void tirtc_ui_platform_unlock(void);

typedef struct {
    bool started, completed, pending;
    int result, start_error;
    uint32_t started_ms, completed_ms;
} internal_sample_t;

typedef struct {
    bool started, completed;
    int start_error;
    uint32_t started_ms, completed_ms;
} external_sample_t;

static liot_task_t s_internal_task, s_external_task;
static bool s_internal_create_failed, s_external_create_failed;
static internal_sample_t s_internal;
static external_sample_t s_external;

static void internal_worker(void *context)
{
    bool first = true;
    int previous = 0;
    (void)context;
    liot_trace("[resources09] internal FS worker started");
    for (;;) {
        int result;
        uint32_t completed_ms;
        tirtc_ui_platform_lock(); s_internal.pending = true; tirtc_ui_platform_unlock();
        if (first) liot_trace("[resources09] internal FS query begin");
        result = liot_internal_fs_free_size_get();
        completed_ms = liot_rtos_get_running_time();
        tirtc_ui_platform_lock();
        s_internal.result = result;
        s_internal.completed_ms = completed_ms;
        s_internal.completed = true;
        s_internal.pending = false;
        tirtc_ui_platform_unlock();
        if (first || (result < 0 && result != previous) || (previous < 0 && result >= 0))
            liot_trace("[resources09] internal FS query returned: %d bytes/error", result);
        previous = result; first = false;
        liot_rtos_task_sleep_ms(RESOURCE_PERIOD_MS);
    }
}

static void external_worker(void *context)
{
    uint32_t previous_state = UINT32_MAX;
    int32_t previous_error = 0;
    (void)context;
    liot_trace("[resources09] external Flash worker started");
    (void)tirtc_storage_init();
    for (;;) {
        tirtc_storage_snapshot_t snapshot;
        uint32_t completed_ms;
        tirtc_storage_poll();
        tirtc_storage_get_snapshot(&snapshot);
        completed_ms = liot_rtos_get_running_time();
        tirtc_ui_platform_lock();
        s_external.completed = true;
        s_external.completed_ms = completed_ms;
        tirtc_ui_platform_unlock();
        if (snapshot.state != previous_state || snapshot.last_error != previous_error) {
            liot_trace("[resources09] external state=%lu id=%06lx chip=%lu fs=%lu free=%lu err=%ld valid=%u ro=%u retry=%u attempts=%lu recovered=%lu next_ms=%lu",
                       (unsigned long)snapshot.state, (unsigned long)snapshot.jedec_id,
                       (unsigned long)snapshot.physical_total_bytes,
                       (unsigned long)snapshot.filesystem_total_bytes,
                       (unsigned long)snapshot.filesystem_free_bytes,
                       (long)snapshot.last_error, (unsigned)snapshot.filesystem_valid,
                       (unsigned)snapshot.read_only, (unsigned)snapshot.recovery_pending,
                       (unsigned long)snapshot.recovery_attempts,
                       (unsigned long)snapshot.recovery_successes,
                       (unsigned long)snapshot.next_retry_ms);
            previous_state = snapshot.state; previous_error = snapshot.last_error;
        }
        liot_rtos_task_sleep_ms(RESOURCE_PERIOD_MS);
    }
}

int tirtc_resources_start(void)
{
    int errors = 0;
    uint32_t started_ms = liot_rtos_get_running_time();
    /* Called by the sole app owner; retry only missing tasks. They never share an I/O
     * queue or file lock; either can fail while the other continues normally. */
    if (!s_internal_task) {
        tirtc_ui_platform_lock();
        s_internal.started = true; s_internal.started_ms = started_ms;
        s_internal.start_error = 0;
        tirtc_ui_platform_unlock();
        if (liot_rtos_task_create(&s_internal_task, RESOURCE_INTERNAL_STACK, RESOURCE_INTERNAL_TASK_PRIORITY,
                                 "tirtc_ifs", internal_worker, NULL) != LIOT_OSI_SUCCESS) {
            s_internal_task = NULL; ++errors;
            tirtc_ui_platform_lock(); s_internal.start_error = RESOURCE_WORKER_ERROR; tirtc_ui_platform_unlock();
            if (!s_internal_create_failed) liot_trace("[resources09] internal FS worker creation failed");
            s_internal_create_failed = true;
        } else s_internal_create_failed = false;
    }
    if (!s_external_task) {
        tirtc_ui_platform_lock(); s_external.started = true; s_external.start_error = 0;
        s_external.started_ms = started_ms; tirtc_ui_platform_unlock();
        if (liot_rtos_task_create(&s_external_task, RESOURCE_EXTERNAL_STACK, RESOURCE_EXTERNAL_TASK_PRIORITY,
                                 "tirtc_efs", external_worker, NULL) != LIOT_OSI_SUCCESS) {
            s_external_task = NULL; ++errors;
            tirtc_ui_platform_lock(); s_external.start_error = RESOURCE_WORKER_ERROR; tirtc_ui_platform_unlock();
            if (!s_external_create_failed) liot_trace("[resources09] external Flash worker creation failed");
            s_external_create_failed = true;
        } else s_external_create_failed = false;
    }
    return errors ? -errors : 0;
}

static tirtc_ui_storage_state_t storage_state(uint32_t state)
{
    switch (state) {
    case TIRTC_STORAGE_STATE_OFF: return TIRTC_STORAGE_UNKNOWN;
    case TIRTC_STORAGE_STATE_PROBING: return TIRTC_STORAGE_INITIALIZING;
    case TIRTC_STORAGE_STATE_READY:
    case TIRTC_STORAGE_STATE_READY_LIMITED: return TIRTC_STORAGE_READY;
    case TIRTC_STORAGE_STATE_UNKNOWN_DATA: return TIRTC_STORAGE_UNRECOGNIZED;
    case TIRTC_STORAGE_STATE_UNSUPPORTED: return TIRTC_STORAGE_UNSUPPORTED;
    default: return TIRTC_STORAGE_ERROR;
    }
}

void tirtc_resources_read_storage(tirtc_ui_resources_t *out)
{
    internal_sample_t internal;
    external_sample_t external;
    tirtc_storage_snapshot_t storage;
    uint32_t now, age;
    if (!out) return;
    /* Copy-only locks. In particular, do not acquire storage's file I/O lock. */
    tirtc_storage_get_snapshot(&storage);
    tirtc_ui_platform_lock();
    internal = s_internal; external = s_external;
    tirtc_ui_platform_unlock();
    /* Capture time AFTER the copies so a just-completed sample cannot appear
     * to be 49 days old through unsigned subtraction of a newer timestamp. */
    now = liot_rtos_get_running_time();
    out->filesystem_valid = false;
    out->filesystem_free_bytes = 0;
    out->filesystem_error = internal.start_error;
    out->filesystem_pending = internal.started && !internal.completed && !internal.start_error;
    if (out->filesystem_pending && (uint32_t)(now - internal.started_ms) > RESOURCE_STALE_MS)
        out->filesystem_error = RESOURCE_STALE_ERROR;
    if (internal.completed) {
        age = (uint32_t)(now - internal.completed_ms);
        out->filesystem_sample_uptime_seconds = out->uptime_seconds >= age / 1000U ?
            out->uptime_seconds - age / 1000U : 0U;
        if (age > RESOURCE_STALE_MS) {
            out->filesystem_error = RESOURCE_STALE_ERROR;
            out->filesystem_pending = internal.pending;
        } else if (internal.result < 0) {
            out->filesystem_error = internal.result;
        } else if (!out->filesystem_total_bytes || (uint32_t)internal.result > out->filesystem_total_bytes) {
            out->filesystem_error = RESOURCE_RANGE_ERROR;
        } else {
            out->filesystem_valid = true;
            out->filesystem_free_bytes = (uint32_t)internal.result;
            out->filesystem_error = 0;
        }
    }
    out->external_flash_valid = storage.physical_valid && storage.physical_total_bytes != 0U;
    out->external_flash_total_bytes = storage.physical_total_bytes;
    out->external_jedec_id = storage.jedec_id;
    out->external_state = storage_state(storage.state);
    if (out->external_state == TIRTC_STORAGE_READY && storage.read_only)
        out->external_state = TIRTC_STORAGE_READ_ONLY;
    if (external.started && storage.state == TIRTC_STORAGE_STATE_OFF)
        out->external_state = TIRTC_STORAGE_INITIALIZING;
    out->external_error = storage.last_error;
    out->external_filesystem_total_bytes = storage.filesystem_total_bytes;
    out->external_filesystem_free_bytes = storage.filesystem_free_bytes;
    out->external_filesystem_valid = storage.filesystem_valid && out->external_flash_valid &&
        (out->external_state == TIRTC_STORAGE_READY || out->external_state == TIRTC_STORAGE_READ_ONLY) &&
        storage.filesystem_total_bytes != 0U &&
        storage.filesystem_total_bytes <= storage.physical_total_bytes &&
        storage.filesystem_free_bytes <= storage.filesystem_total_bytes;
    if (external.start_error) {
        out->external_state = TIRTC_STORAGE_ERROR;
        out->external_error = external.start_error;
        out->external_filesystem_valid = false;
    } else if (out->external_state == TIRTC_STORAGE_INITIALIZING) {
        /* Long blank scans keep publishing progress. Only a genuinely stalled
         * initializer is marked timed out; never cancel its task underneath I/O. */
        uint32_t progress_ms = storage.state == TIRTC_STORAGE_STATE_OFF ?
            external.started_ms : storage.progress_ms;
        if ((uint32_t)(now - progress_ms) > RESOURCE_STALE_MS) {
            out->external_state = TIRTC_STORAGE_ERROR;
            out->external_error = RESOURCE_STALE_ERROR;
            out->external_filesystem_valid = false;
        }
    } else if (storage.filesystem_valid || storage.filesystem_sample_ms) {
        /* poll() may skip a busy filesystem. Only the driver's actual completed
         * usage timestamp can make a previous free-space figure fresh again. */
        age = (uint32_t)(now - storage.filesystem_sample_ms);
        out->external_sample_uptime_seconds = out->uptime_seconds >= age / 1000U ?
            out->uptime_seconds - age / 1000U : 0U;
        if (age > RESOURCE_STALE_MS && out->external_filesystem_valid) {
            out->external_state = TIRTC_STORAGE_ERROR;
            out->external_error = RESOURCE_STALE_ERROR;
            out->external_filesystem_valid = false;
        }
    }
}
