#include "font_cache.h"
#include "asset_io.h"
#include "asset_manifest.h"
#include "../storage/tirtc_storage.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include <string.h>

#define FONT_JOBS 32U
#define FONT_BUCKETS 128U
#define FONT_NONE UINT16_MAX
#define FONT_RETRY_MIN_MS 5000U
#define FONT_RETRY_MAX_MS 60000U
#define FONT_BUSY_RETRY_MS 50U
enum { FONT_FREE, FONT_REQUESTED, FONT_WORKING, FONT_DONE };
typedef struct { uint32_t offset, used; uint16_t size, next; } font_slot_t;
typedef struct {
    uint32_t offset;
    uint16_t size;
    uint8_t state;
    uint8_t data[TIRTC_FONT_CACHE_GLYPH_BYTES];
} font_job_t;
static uint8_t *s_font_cache_data;
/* These three objects are UI-owned after initialization. */
static font_slot_t s_font_slots[TIRTC_FONT_CACHE_SLOTS];
static uint16_t s_font_buckets[FONT_BUCKETS];
static uint32_t s_font_use;
/* Job ownership: UI FREE->REQUESTED, worker REQUESTED->WORKING->DONE,
 * UI copies DONE pixels into its cache before returning the job to FREE. */
static font_job_t s_font_jobs[FONT_JOBS];
static tirtc_font_cache_snapshot_t s_font_stats;
/* The resource worker alone owns the descriptor and retry state. */
static int s_font_fd = -1;
static uint32_t s_font_generation, s_font_retry_start, s_font_retry_ms;
static bool s_font_retry_wait, s_font_reverify;
static bool s_font_busy_wait, s_font_close_pending;
static uint32_t s_font_busy_start;
static unsigned s_font_next_job;
static uint32_t s_font_diag_ms;
static tirtc_font_cache_snapshot_t s_font_diag_snapshot;

static unsigned font_bucket(uint32_t offset)
{ return (unsigned)((offset ^ (offset >> 7) ^ (offset >> 14)) & (FONT_BUCKETS - 1U)); }

void tirtc_font_cache_init(uint8_t *allocation, uint32_t storage_generation)
{
    s_font_cache_data = allocation;
    memset(s_font_slots, 0, sizeof(s_font_slots));
    memset(s_font_buckets, 0xff, sizeof(s_font_buckets));
    memset(s_font_jobs, 0, sizeof(s_font_jobs));
    memset(&s_font_stats, 0, sizeof(s_font_stats));
    s_font_use = 0; s_font_fd = -1;
    s_font_generation = storage_generation;
    s_font_retry_start = 0; s_font_retry_ms = FONT_RETRY_MIN_MS;
    s_font_retry_wait = s_font_reverify = false;
    s_font_busy_wait = s_font_close_pending = false; s_font_busy_start = 0;
    s_font_next_job = 0;
    s_font_diag_ms = liot_rtos_get_running_time();
    memset(&s_font_diag_snapshot, 0, sizeof(s_font_diag_snapshot));
}

void tirtc_font_cache_get_snapshot(tirtc_font_cache_snapshot_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical(); *out = s_font_stats; liot_rtos_exit_critical();
}

const uint8_t *tirtc_font_cache_get(uint32_t offset, uint32_t bytes)
{
    if (!s_font_cache_data || !bytes || bytes > TIRTC_FONT_CACHE_GLYPH_BYTES ||
        offset > TIRTC_FONT_BYTES || bytes > TIRTC_FONT_BYTES - offset) return NULL;
    unsigned bucket = font_bucket(offset);
    for (uint16_t i = s_font_buckets[bucket]; i != FONT_NONE; i = s_font_slots[i].next) {
        if (s_font_slots[i].offset == offset && s_font_slots[i].size == bytes) {
            s_font_slots[i].used = ++s_font_use;
            liot_rtos_enter_critical(); ++s_font_stats.hits; liot_rtos_exit_critical();
            return s_font_cache_data + (uint32_t)i * TIRTC_FONT_CACHE_GLYPH_BYTES;
        }
    }
    /* No sleep, allocation, mutex wait or Flash call in the glyph callback. */
    liot_rtos_enter_critical();
    ++s_font_stats.misses;
    unsigned free_job = FONT_JOBS;
    for (unsigned i = 0; i < FONT_JOBS; ++i) {
        if (s_font_jobs[i].state == FONT_FREE) { if (free_job == FONT_JOBS) free_job = i; }
        else if (s_font_jobs[i].offset == offset && s_font_jobs[i].size == bytes) {
            liot_rtos_exit_critical(); return NULL;
        }
    }
    if (free_job != FONT_JOBS) {
        font_job_t *job = &s_font_jobs[free_job];
        job->offset = offset; job->size = (uint16_t)bytes;
        job->state = FONT_REQUESTED; ++s_font_stats.pending;
    }
    liot_rtos_exit_critical();
    /* A full queue is retried by the redraw triggered by its completions. */
    return NULL;
}

static void font_cache_insert(const font_job_t *job)
{
    unsigned victim = 0;
    uint32_t oldest = 0;
    bool evicted = true;
    for (unsigned i = 0; i < TIRTC_FONT_CACHE_SLOTS; ++i) {
        if (!s_font_slots[i].size) { victim = i; evicted = false; break; }
        uint32_t age = s_font_use - s_font_slots[i].used;
        if (age >= oldest) { oldest = age; victim = i; }
    }
    font_slot_t *slot = &s_font_slots[victim];
    if (evicted) {
        uint16_t *link = &s_font_buckets[font_bucket(slot->offset)];
        while (*link != FONT_NONE && *link != victim) link = &s_font_slots[*link].next;
        if (*link == victim) *link = slot->next;
    }
    memcpy(s_font_cache_data + victim * TIRTC_FONT_CACHE_GLYPH_BYTES, job->data, job->size);
    slot->offset = job->offset; slot->size = job->size; slot->used = ++s_font_use;
    unsigned bucket = font_bucket(slot->offset);
    slot->next = s_font_buckets[bucket]; s_font_buckets[bucket] = (uint16_t)victim;
    liot_rtos_enter_critical();
    ++s_font_stats.loads;
    if (evicted) ++s_font_stats.evictions; else ++s_font_stats.entries;
    liot_rtos_exit_critical();
}

bool tirtc_font_cache_process(void)
{
    bool changed = false;
    if (!s_font_cache_data) return false;
    for (unsigned i = 0; i < FONT_JOBS; ++i) {
        font_job_t complete;
        bool done;
        liot_rtos_enter_critical();
        done = s_font_jobs[i].state == FONT_DONE;
        if (done) {
            complete = s_font_jobs[i];
            s_font_jobs[i].state = FONT_FREE; --s_font_stats.pending;
        }
        liot_rtos_exit_critical();
        if (done) { font_cache_insert(&complete); changed = true; }
    }
    return changed;
}

static int font_close_owned(void)
{
    int result = 0;
    if (s_font_fd >= 0) result = tirtc_storage_close(s_font_fd);
    /* The application storage layer leaves the token owned ONLY on BUSY:
     * BADF has no slot; NOT_READY and all lfs_close outcomes retire the slot. */
    s_font_close_pending = result == TIRTC_STORAGE_E_BUSY;
    if (!s_font_close_pending) s_font_fd = -1;
    return result;
}

static void font_read_failed(int error, font_job_t *job)
{
    uint32_t now = liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    if (job) { memset(job->data, 0, sizeof(job->data)); job->state = FONT_REQUESTED; }
    s_font_stats.error = error; ++s_font_stats.retries;
    liot_rtos_exit_critical();
    if (error == TIRTC_STORAGE_E_BUSY) {
        /* Lock contention has not damaged the file or advanced a failed read.
         * Keep the descriptor and verification status. Even after a partial
         * read, the next attempt seeks the original glyph offset from scratch.
         * This delay belongs only to the background worker; never to LVGL. */
        s_font_busy_start = now; s_font_busy_wait = true;
        return; /* Existing five-second aggregate log observes BUSY too. */
    }
    (void)font_close_owned();
    s_font_reverify = true;
    s_font_retry_start = now;
    s_font_retry_wait = true;
    liot_trace("[assets24] FONT_RETRY error=%d delay_ms=%lu\r\n", error, (unsigned long)s_font_retry_ms);
}

bool tirtc_font_cache_worker_step(void)
{
    font_job_t *job = NULL;
    int result = 0;
    tirtc_storage_snapshot_t storage;
    uint32_t now = liot_rtos_get_running_time();
    if ((uint32_t)(now - s_font_diag_ms) >= 5000U) {
        tirtc_font_cache_snapshot_t snapshot;
        s_font_diag_ms = now;
        tirtc_font_cache_get_snapshot(&snapshot);
        if (memcmp(&snapshot, &s_font_diag_snapshot, sizeof(snapshot))) {
            s_font_diag_snapshot = snapshot;
            TIRTC_LOG_DEBUG("[assets24] FONT_CACHE hit=%lu miss=%lu load=%lu pending=%lu retry=%lu error=%ld\r\n",
                       (unsigned long)snapshot.hits, (unsigned long)snapshot.misses,
                       (unsigned long)snapshot.loads, (unsigned long)snapshot.pending,
                       (unsigned long)snapshot.retries, (long)snapshot.error);
        }
    }
    if (s_font_busy_wait) {
        if ((uint32_t)(now - s_font_busy_start) < FONT_BUSY_RETRY_MS) return false;
        s_font_busy_wait = false;
    }
    if (s_font_retry_wait) {
        if ((uint32_t)(now - s_font_retry_start) < s_font_retry_ms) return false;
        s_font_retry_wait = false;
        s_font_retry_ms = s_font_retry_ms >= 30000U ? FONT_RETRY_MAX_MS : s_font_retry_ms * 2U;
    }
    liot_rtos_enter_critical();
    for (unsigned n = 0; n < FONT_JOBS; ++n) {
        unsigned i = (s_font_next_job + n) % FONT_JOBS;
        if (s_font_jobs[i].state == FONT_REQUESTED) {
            job = &s_font_jobs[i]; job->state = FONT_WORKING;
            s_font_next_job = (i + 1U) % FONT_JOBS; break;
        }
    }
    liot_rtos_exit_critical();
    if (!job) return false;
    /* A previous close that could not get the storage lock still owns its
     * token. Finish that close before verification or any fresh open. */
    if (s_font_close_pending && font_close_owned() == TIRTC_STORAGE_E_BUSY) {
        result = TIRTC_STORAGE_E_BUSY; goto failed;
    }
    tirtc_storage_get_snapshot(&storage);
    if (!storage.filesystem_valid) { result = TIRTC_STORAGE_E_NOT_READY; goto failed; }
    if (storage.recovery_successes != s_font_generation) s_font_reverify = true;
    if (s_font_reverify) {
        if (font_close_owned() == TIRTC_STORAGE_E_BUSY) {
            result = TIRTC_STORAGE_E_BUSY; goto failed;
        }
        result = tirtc_asset_read_verified(TIRTC_ASSET_FONT_PATH, NULL,
                                          TIRTC_FONT_BYTES, TIRTC_FONT_CRC32, NULL);
        if (result) goto failed;
        s_font_generation = storage.recovery_successes; s_font_reverify = false;
    }
    if (s_font_fd < 0) {
        s_font_fd = tirtc_storage_open(TIRTC_ASSET_FONT_PATH, "r");
        if (s_font_fd < 0) { result = s_font_fd; goto failed; }
    }
    result = tirtc_storage_seek(s_font_fd, (int32_t)job->offset, 0);
    if (result < 0) goto failed;
    if ((uint32_t)result != job->offset) { result = TIRTC_STORAGE_E_IO; goto failed; }
    memset(job->data, 0, sizeof(job->data));
    for (uint32_t read = 0; read < job->size;) {
        result = tirtc_storage_read(s_font_fd, job->data + read, job->size - read);
        if (result <= 0 || (uint32_t)result > job->size - read) {
            if (result >= 0) result = TIRTC_STORAGE_E_IO;
            goto failed;
        }
        read += (uint32_t)result;
    }
    s_font_retry_ms = FONT_RETRY_MIN_MS;
    liot_rtos_enter_critical();
    s_font_stats.error = 0; job->state = FONT_DONE;
    liot_rtos_exit_critical();
    return true;
failed:
    font_read_failed(result, job);
    return false;
}
