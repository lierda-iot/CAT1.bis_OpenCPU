/* Two-buffer camera producer. JPEG ownership remains in the video worker.
 * No allocation, encoding, network I/O or camera deinit occurs in this task. */
#include "tirtc_capture_pipeline.h"
#include "tirtc_camera_fence.h"
#include "liot_os.h"
#include "liot_log.h"
#include <stddef.h>
#include <string.h>

#define CAPTURE_TIMEOUT_MS 400U
#define CAPTURE_INITIAL_TIMEOUT_MS 1200U
#define CAPTURE_ERROR_LIMIT 5U

typedef enum { SLOT_FREE, SLOT_CAPTURING, SLOT_READY, SLOT_HELD } slot_state_t;
typedef struct {
    uint8_t *raw;
    slot_state_t state;
    uint32_t timestamp, capture_ms;
} capture_slot_t;

static liot_task_t s_capture_task;
static bool s_creating;
static liot_camera_handle_t s_camera;
static capture_slot_t s_slots[2];
static tirtc_capture_pipeline_stats_t s_stats;
static bool s_wanted, s_call_active;
static bool s_initial_capture_pending;
static uint32_t s_accept_epoch;
static int s_capture_slot = -1;

/* All ownership metadata is protected by the RTOS critical section. The
 * fence's idle read supplies the DMA memory barrier; no blocking call occurs
 * while interrupts are disabled. */
static void discard_ready_locked(void)
{
    unsigned i;
    for (i = 0; i < 2U; i++) {
        if (s_slots[i].state == SLOT_READY) {
            s_slots[i].state = SLOT_FREE;
            s_stats.discarded++;
        }
    }
}

static void settle_locked(void)
{
    unsigned i;
    if (!s_stats.active || s_call_active) return;
    /* A timed-out SDK call may have returned while DMA still owns this slot.
     * Only a later true completion permits us to release that lease. */
    if (s_capture_slot >= 0 && tirtc_camera_fence_idle()) {
        s_slots[(unsigned)s_capture_slot].state = SLOT_FREE;
        s_capture_slot = -1;
    }
    if (s_wanted) return;
    discard_ready_locked();
    if (s_capture_slot >= 0 || !tirtc_camera_fence_idle()) return;
    for (i = 0; i < 2U; i++) {
        if (s_slots[i].state != SLOT_FREE) return;
    }
    s_stats.active = false;
    s_stats.enabled = false;
    s_stats.paused = false;
    s_camera = NULL;
    s_slots[0].raw = NULL;
    s_slots[1].raw = NULL;
}

/* One producer iteration, kept separate from task sleeping for deterministic
 * event-order tests. This is invoked only by the persistent capture task. */
static bool capture_step(void)
{
    liot_camera_handle_t camera;
    uint8_t *raw;
    uint32_t epoch, generation, began, completed_at, timeout_ms;
    int slot = -1, result;
    bool dma_idle, initial_capture;
    unsigned i;

    liot_rtos_enter_critical();
    settle_locked();
    if (!s_stats.active || !s_wanted || !s_stats.enabled || s_stats.paused ||
        s_call_active || s_capture_slot >= 0) {
        liot_rtos_exit_critical();
        return false;
    }
    for (i = 0; i < 2U; i++) {
        if (s_slots[i].state == SLOT_FREE) { slot = (int)i; break; }
    }
    if (slot < 0) {
        for (i = 0; i < 2U; i++) {
            if (s_slots[i].state == SLOT_READY) {
                slot = (int)i;
                s_stats.replaced++;
                break;
            }
        }
    }
    if (slot < 0) {
        liot_rtos_exit_critical();
        return false;
    }
    s_slots[(unsigned)slot].state = SLOT_CAPTURING;
    s_capture_slot = slot;
    s_call_active = true;
    epoch = s_accept_epoch;
    generation = s_stats.generation;
    initial_capture = s_initial_capture_pending;
    s_initial_capture_pending = false;
    /* Sensor startup can take longer than steady 64ms frames. This only
     * widens the first attempt's completion deadline; it adds no fixed wait
     * and never changes the DMA lease or subsequent frame deadline. */
    timeout_ms = initial_capture ? CAPTURE_INITIAL_TIMEOUT_MS : CAPTURE_TIMEOUT_MS;
    camera = s_camera;
    raw = s_slots[(unsigned)slot].raw;
    liot_rtos_exit_critical();

    began = liot_rtos_get_running_time();
    result = tirtc_camera_capture(camera, raw, timeout_ms);
    completed_at = liot_rtos_get_running_time();
    dma_idle = tirtc_camera_fence_idle();

    liot_rtos_enter_critical();
    s_call_active = false;
    s_stats.last_capture_ms = completed_at - began;
    if (result != LIOT_CAMERA_SUCCESS || !dma_idle) {
        s_stats.errors++;
        s_stats.consecutive_errors++;
        if (!dma_idle || s_stats.consecutive_errors >= CAPTURE_ERROR_LIMIT) {
            s_stats.error = result ? result : TIRTC_CAMERA_FENCE_TIMEOUT;
            s_wanted = false;
            s_accept_epoch++;
            discard_ready_locked();
        }
        /* An incomplete DMA keeps SLOT_CAPTURING until settle_locked observes
         * completion. A failed but complete frame is never published. */
        if (dma_idle) {
            s_slots[(unsigned)slot].state = SLOT_FREE;
            s_capture_slot = -1;
        }
    } else {
        s_stats.captured++;
        s_stats.consecutive_errors = 0;
        s_stats.last_timestamp = completed_at;
        s_capture_slot = -1;
        if (s_wanted && s_stats.enabled && !s_stats.paused && epoch == s_accept_epoch) {
            for (i = 0; i < 2U; i++) {
                if (s_slots[i].state == SLOT_READY) {
                    s_slots[i].state = SLOT_FREE;
                    s_stats.replaced++;
                }
            }
            s_slots[(unsigned)slot].timestamp = completed_at;
            s_slots[(unsigned)slot].capture_ms = completed_at - began;
            s_slots[(unsigned)slot].state = SLOT_READY;
        } else {
            s_slots[(unsigned)slot].state = SLOT_FREE;
            s_stats.discarded++;
        }
    }
    settle_locked();
    liot_rtos_exit_critical();
    if (initial_capture) {
        liot_trace("[VIDEO-CAM] first_capture gen=%lu limit_ms=%lu "
                   "elapsed_ms=%lu result=%d dma_idle=%u\r\n",
                   (unsigned long)generation, (unsigned long)timeout_ms,
                   (unsigned long)(completed_at - began), result,
                   dma_idle ? 1U : 0U);
    }
    return true;
}

static void capture_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* The sensor/DMA defines capture cadence. A short yield also lets the
         * same-priority codec consumer run after an immediately completed frame. */
        bool attempted = capture_step();
        liot_rtos_task_sleep_ms(attempted ? 1U : 5U);
    }
}

int tirtc_capture_pipeline_service_init(void)
{
    liot_task_t created = NULL;
    LiotOSStatus_t result;
    liot_rtos_enter_critical();
    if (s_capture_task) {
        liot_rtos_exit_critical();
        return 0;
    }
    if (s_creating) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_BUSY;
    }
    s_creating = true;
    liot_rtos_exit_critical();
    result = liot_rtos_task_create(&created, 4U * 1024U, 11U, "call_camera",
                                   capture_task, NULL);
    liot_rtos_enter_critical();
    s_creating = false;
    if (result == LIOT_OSI_SUCCESS && created) s_capture_task = created;
    liot_rtos_exit_critical();
    return result == LIOT_OSI_SUCCESS && created ? 0 : TIRTC_CAPTURE_PIPELINE_SERVICE;
}

int tirtc_capture_pipeline_start(liot_camera_handle_t camera, uint32_t generation,
                                 uint8_t *raw0, uint8_t *raw1, bool enabled)
{
    uintptr_t a = (uintptr_t)raw0, b = (uintptr_t)raw1;
    uintptr_t distance = a > b ? a - b : b - a;
    if (!camera || !generation || !raw0 || !raw1 || ((a | b) & 3U) ||
        distance < TIRTC_CAPTURE_PIPELINE_RAW_BYTES)
        return TIRTC_CAPTURE_PIPELINE_ARGUMENT;
    liot_rtos_enter_critical();
    settle_locked();
    if (!s_capture_task) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_SERVICE;
    }
    if (s_stats.active || s_call_active || s_capture_slot >= 0) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_BUSY;
    }
    if (!tirtc_camera_fence_ready() || !tirtc_camera_fence_idle()) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_UNAVAILABLE;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    memset(s_slots, 0, sizeof(s_slots));
    s_stats.generation = generation;
    s_stats.active = true;
    s_stats.enabled = enabled;
    s_camera = camera;
    s_slots[0].raw = raw0;
    s_slots[1].raw = raw1;
    s_wanted = true;
    s_initial_capture_pending = true;
    s_accept_epoch++;
    liot_rtos_exit_critical();
    return 0;
}

int tirtc_capture_pipeline_set_enabled(uint32_t generation, bool enabled)
{
    liot_rtos_enter_critical();
    if (!s_stats.active || !s_wanted || s_stats.generation != generation) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_UNAVAILABLE;
    }
    if (s_stats.enabled != enabled) {
        s_stats.enabled = enabled;
        s_accept_epoch++;
        if (!enabled) discard_ready_locked();
    }
    liot_rtos_exit_critical();
    return 0;
}

int tirtc_capture_pipeline_set_paused(uint32_t generation, bool paused)
{
    liot_rtos_enter_critical();
    if (!s_stats.active || !s_wanted || s_stats.generation != generation) {
        liot_rtos_exit_critical();
        return TIRTC_CAPTURE_PIPELINE_UNAVAILABLE;
    }
    if (s_stats.paused != paused) {
        s_stats.paused = paused;
        s_accept_epoch++;
        if (paused) discard_ready_locked();
    }
    liot_rtos_exit_critical();
    return 0;
}

bool tirtc_capture_pipeline_take(uint32_t generation, unsigned *slot,
                                uint8_t **raw, uint32_t *timestamp,
                                uint32_t *capture_ms)
{
    unsigned i;
    if (!slot || !raw || !timestamp || !capture_ms) return false;
    liot_rtos_enter_critical();
    if (s_stats.active && s_wanted && s_stats.enabled && !s_stats.paused &&
        s_stats.generation == generation) {
        for (i = 0; i < 2U; i++) {
            if (s_slots[i].state == SLOT_READY) {
                s_slots[i].state = SLOT_HELD;
                s_stats.taken++;
                *slot = i;
                *raw = s_slots[i].raw;
                *timestamp = s_slots[i].timestamp;
                *capture_ms = s_slots[i].capture_ms;
                liot_rtos_exit_critical();
                return true;
            }
        }
    }
    liot_rtos_exit_critical();
    return false;
}

void tirtc_capture_pipeline_release(uint32_t generation, unsigned slot)
{
    liot_rtos_enter_critical();
    if (s_stats.active && s_stats.generation == generation && slot < 2U &&
        s_slots[slot].state == SLOT_HELD) {
        s_slots[slot].state = SLOT_FREE;
        settle_locked();
    }
    liot_rtos_exit_critical();
}

void tirtc_capture_pipeline_request_stop(uint32_t generation)
{
    liot_rtos_enter_critical();
    if (s_stats.active && s_stats.generation == generation) {
        s_wanted = false;
        s_accept_epoch++;
        discard_ready_locked();
        settle_locked();
    }
    liot_rtos_exit_critical();
}

bool tirtc_capture_pipeline_is_idle(uint32_t generation)
{
    bool idle;
    liot_rtos_enter_critical();
    settle_locked();
    idle = s_stats.generation != generation || !s_stats.active;
    liot_rtos_exit_critical();
    return idle;
}

void tirtc_capture_pipeline_get_stats(tirtc_capture_pipeline_stats_t *out)
{
    unsigned i;
    if (!out) return;
    liot_rtos_enter_critical();
    settle_locked();
    *out = s_stats;
    out->in_flight = s_capture_slot >= 0 || s_call_active;
    out->ready = out->held = 0;
    for (i = 0; i < 2U; i++) {
        if (s_slots[i].state == SLOT_READY) out->ready++;
        if (s_slots[i].state == SLOT_HELD) out->held++;
    }
    liot_rtos_exit_critical();
}
