/* Runtime statistics use the actual F6D_A SDK implementations:
 * liot_xPortGet* -> Cust heap (the pool used by liot_rtos_malloc);
 * Slow filesystem queries live in independent background workers.
 * No modem/system heap is combined with the application heap.
 */
#include "tirtc_resources.h"
#include <string.h>
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include "../status/tirtc_status.h"

/* The F6D_A getter returns tick_count * 1000 / configTICK_RATE_HZ, i.e. ms,
 * despite the SDK header documenting seconds. Preserve a normal 32-bit wrap
 * when this single-owner collector is called at least once every 49.7 days. */
static bool s_uptime_started;
static uint32_t s_previous_uptime_ms;
static uint64_t s_elapsed_uptime_ms;
static uint32_t s_sample_sequence;
static bool s_diagnostic_started;
static uint32_t s_diagnostic_ms;
static tirtc_ui_storage_state_t s_reported_external_state;
static int32_t s_reported_external_error, s_reported_internal_error;

static void report_sample(const tirtc_ui_resources_t *sample, uint32_t now_ms)
{
    bool first = !s_diagnostic_started;
    if (first) tirtc_status_event("内存资源采集已启动");
    if (sample->external_state != s_reported_external_state ||
        sample->external_error != s_reported_external_error) {
        const char *event;
        switch (sample->external_state) {
        case TIRTC_STORAGE_INITIALIZING: event = "外置存储正在初始化"; break;
        case TIRTC_STORAGE_READY: event = "外置存储已就绪"; break;
        case TIRTC_STORAGE_READ_ONLY: event = "外置存储只读挂载"; break;
        case TIRTC_STORAGE_UNRECOGNIZED: event = "外置存储未识别，保留数据"; break;
        case TIRTC_STORAGE_UNSUPPORTED: event = "外置存储芯片不支持"; break;
        case TIRTC_STORAGE_ERROR: event = "外置存储读取失败"; break;
        default: event = "外置存储等待初始化"; break;
        }
        tirtc_status_event(event);
        s_reported_external_state = sample->external_state;
        s_reported_external_error = sample->external_error;
    }
    if (sample->filesystem_error != s_reported_internal_error) {
        tirtc_status_event(sample->filesystem_error ? "内部文件区读取失败" : "内部文件区恢复正常");
        s_reported_internal_error = sample->filesystem_error;
    }
    /* The first sample and one summary per 30 s suffice to prove progress.
     * No I/O is performed while holding the UI copy lock. Values are bytes. */
    if (first || (uint32_t)(now_ms - s_diagnostic_ms) >= 30000U) {
        TIRTC_LOG_DEBUG("[resources09] seq=%lu uptime=%lu heap_valid=%u free=%lu total=%lu app_valid=%u app_free=%lu ifs_valid=%u ifs_err=%ld ext_state=%u ext_valid=%u ext_free=%lu ext_err=%ld",
            (unsigned long)sample->sample_sequence, (unsigned long)sample->uptime_seconds,
            (unsigned)sample->heap_valid, (unsigned long)sample->heap_free_bytes,
            (unsigned long)sample->heap_total_bytes, (unsigned)sample->flash_valid,
            (unsigned long)(sample->flash_total_bytes - sample->flash_used_bytes),
            (unsigned)sample->filesystem_valid, (long)sample->filesystem_error,
            (unsigned)sample->external_state, (unsigned)sample->external_filesystem_valid,
            (unsigned long)sample->external_filesystem_free_bytes,
            (long)sample->external_error);
        s_diagnostic_started = true;
        s_diagnostic_ms = now_ms;
    }
}

static bool fits_u32(size_t value)
{
    return value == (size_t)(uint32_t)value;
}

static void validate_layout(tirtc_ui_resources_t *sample)
{
    if (!sample->flash_valid || !sample->flash_total_bytes ||
        sample->flash_used_bytes > sample->flash_total_bytes) {
        sample->flash_valid = false;
        sample->flash_used_bytes = 0;
        sample->flash_total_bytes = 0;
    }
    if (!sample->static_ram_valid || !sample->static_ram_total_bytes ||
        sample->static_ram_used_bytes > sample->static_ram_total_bytes) {
        sample->static_ram_valid = false;
        sample->static_ram_used_bytes = 0;
        sample->static_ram_total_bytes = 0;
    }
}

void tirtc_resources_publish(void)
{
    tirtc_ui_resources_t sample;
    size_t heap_total, heap_free, heap_min, heap_max;
    uint32_t uptime_ms;
    uint64_t uptime_seconds;

    memset(&sample, 0, sizeof(sample));
    tirtc_resources_read_layout(&sample);
    validate_layout(&sample);

    /* Each SDK heap getter protects its own read. They are nearby samples,
     * not an atomic transaction: another task may allocate/free between them.
     * Validate each figure against the fixed pool capacity; do not reject a
     * later max-block reading merely because an earlier free reading was less. */
    heap_total = liot_xPortGetTotalHeapSize();
    heap_free = liot_xPortGetFreeHeapSize();
    heap_min = liot_xPortGetMinimumEverFreeHeapSize();
    heap_max = liot_xPortGetMaximumFreeBlockSize();
    if (heap_total && fits_u32(heap_total) && fits_u32(heap_free) && heap_free <= heap_total) {
        sample.heap_valid = true;
        sample.heap_total_bytes = (uint32_t)heap_total;
        sample.heap_free_bytes = (uint32_t)heap_free;
        if (fits_u32(heap_min) && heap_min <= heap_total) {
            sample.heap_min_valid = true;
            sample.heap_min_free_bytes = (uint32_t)heap_min;
        }
        if (fits_u32(heap_max) && heap_max <= heap_total) {
            sample.heap_max_valid = true;
            sample.heap_max_free_block_bytes = (uint32_t)heap_max;
        }
    }

    /* These inexpensive statistics must not wait for any filesystem I/O. */
    uptime_ms = liot_rtos_get_running_time();
    if (!s_uptime_started) {
        s_elapsed_uptime_ms = uptime_ms;
        s_uptime_started = true;
    } else {
        s_elapsed_uptime_ms += (uint32_t)(uptime_ms - s_previous_uptime_ms);
    }
    s_previous_uptime_ms = uptime_ms;
    uptime_seconds = s_elapsed_uptime_ms / 1000U;
    if (uptime_seconds <= UINT32_MAX) {
        sample.uptime_valid = true;
        sample.uptime_seconds = (uint32_t)uptime_seconds;
    }

    /* Only copy already completed storage samples. A blocked filesystem
     * worker cannot suppress this first publication or subsequent RAM updates. */
    tirtc_resources_read_storage(&sample);
    if (++s_sample_sequence == 0U) ++s_sample_sequence;
    sample.sample_sequence = s_sample_sequence;
    /* This public API owns the short copy lock; all SDK calls have completed. */
    (void)tirtc_ui_publish_resources(&sample);
    report_sample(&sample, uptime_ms);
}
