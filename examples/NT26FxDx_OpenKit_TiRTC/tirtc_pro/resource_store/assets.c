#include "assets.h"
#include "asset_io.h"
#include "asset_manifest.h"
#include "font_cache.h"
#include "../ui/font/tirtc_font_14.h"
#include "../ui/font/tirtc_font_digits_48.h"
#include "../ui/assets/modern_icons.h"
#include "../ui/ui_face.h"
#include "../storage/tirtc_storage.h"
#include "liot_log.h"
#include "liot_os.h"
#include <stddef.h>

#define ASSET_WAIT_MS 120000U
#define ASSET_WAIT_STEP_MS 250U
#define ASSET_RETRY_MIN_MS 5000U
#define ASSET_RETRY_MAX_MS 60000U
/* Match GUI and external-FS priority: pending glyph I/O must get CPU time
 * during video redraws, while video11/audio12 still preempt this worker.
 * Existing chunk/job sleeps and zero-wait LVGL callbacks stay unchanged. */
#define ASSET_TASK_PRIORITY 10U
static liot_task_t s_asset_task;
static bool s_asset_started;
static uint8_t *s_cache_allocation, *s_background;
static tirtc_assets_snapshot_t s_asset_snapshot;

static void publish(uint32_t state, int error)
{
    liot_rtos_enter_critical();
    s_asset_snapshot.state = state;
    s_asset_snapshot.error = error;
    liot_rtos_exit_critical();
}

static void loaded(uint32_t amount)
{
    liot_rtos_enter_critical();
    s_asset_snapshot.loaded_bytes += amount;
    liot_rtos_exit_critical();
}

void tirtc_assets_get_snapshot(tirtc_assets_snapshot_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical();
    *out = s_asset_snapshot;
    liot_rtos_exit_critical();
}

bool tirtc_assets_ready(void)
{
    bool ready;
    liot_rtos_enter_critical();
    ready = s_asset_snapshot.state == TIRTC_ASSETS_READY;
    liot_rtos_exit_critical();
    return ready;
}

int tirtc_assets_error(void)
{
    int error;
    liot_rtos_enter_critical();
    error = s_asset_snapshot.error;
    liot_rtos_exit_critical();
    return error;
}

const uint8_t *tirtc_assets_font(void)
{
    /* No contiguous bitmap is resident in cached mode. Kept as a fail-closed
     * compatibility getter; consumers must attach the glyph reader below. */
    return NULL;
}

const uint8_t *tirtc_assets_glyph(uint32_t offset, uint32_t bytes)
{ return tirtc_font_cache_get(offset, bytes); }

bool tirtc_assets_font_process(void)
{ return tirtc_font_cache_process(); }

const uint16_t *tirtc_assets_background(void)
{
    const uint16_t *background;
    liot_rtos_enter_critical();
    background = s_asset_snapshot.state == TIRTC_ASSETS_READY ? (const uint16_t *)s_background : NULL;
    liot_rtos_exit_critical();
    return background;
}

static int wait_storage(void)
{
    uint32_t start = liot_rtos_get_running_time();
    for (unsigned n = 0; n <= ASSET_WAIT_MS / ASSET_WAIT_STEP_MS; ++n) {
        tirtc_storage_snapshot_t storage;
        tirtc_storage_get_snapshot(&storage);
        if (storage.filesystem_valid &&
            (storage.state == TIRTC_STORAGE_STATE_READY || storage.state == TIRTC_STORAGE_STATE_READY_LIMITED)) return 0;
        if (storage.state != TIRTC_STORAGE_STATE_OFF && storage.state != TIRTC_STORAGE_STATE_PROBING &&
            !storage.recovery_pending) return storage.last_error ? storage.last_error : TIRTC_STORAGE_E_NOT_READY;
        if ((uint32_t)(liot_rtos_get_running_time() - start) >= ASSET_WAIT_MS) break;
        liot_rtos_task_sleep_ms(ASSET_WAIT_STEP_MS);
    }
    return TIRTC_STORAGE_E_TIMEOUT;
}

static void asset_worker(void *unused)
{
    uint8_t *font = NULL, *background = NULL;
    uint32_t retry_ms = ASSET_RETRY_MIN_MS;
    uint32_t recovery_generation = 0;
    bool storage_was_ready = false, retryable;
    int result;
    (void)unused;
retry:
    storage_was_ready = false;
    liot_rtos_enter_critical();
    ++s_asset_snapshot.attempts;
    s_asset_snapshot.loaded_bytes = 0;
    s_asset_snapshot.retry_delay_ms = 0;
    liot_rtos_exit_critical();
    publish(TIRTC_ASSETS_WAIT_STORAGE, 0);
    result = wait_storage();
    if (result) goto failed;
    {
        tirtc_storage_snapshot_t storage;
        tirtc_storage_get_snapshot(&storage);
        recovery_generation = storage.recovery_successes;
        storage_was_ready = true;
    }
    publish(TIRTC_ASSETS_LOADING, 0);
    font = liot_rtos_malloc(TIRTC_FONT_CACHE_BYTES);
    if (!font) { result = TIRTC_ASSET_E_NOMEM; goto failed; }
    background = liot_rtos_malloc(TIRTC_BACKGROUND_BYTES);
    if (!background) { result = TIRTC_ASSET_E_NOMEM; goto failed; }
    result = tirtc_asset_read_verified(TIRTC_ASSET_FONT_PATH, NULL, TIRTC_FONT_BYTES, TIRTC_FONT_CRC32, loaded);
    if (result) goto failed;
    result = tirtc_asset_read_verified(TIRTC_ASSET_BACKGROUND_PATH, background,
                                      TIRTC_BACKGROUND_BYTES, TIRTC_BACKGROUND_CRC32, loaded);
    if (result) goto failed;
    /* Load immutable UI data into fixed BSS before publishing READY. No file
     * access is added to drawing or media callbacks, and no extra heap is used. */
    result = tirtc_font_load_external(loaded);
    if (result) goto failed;
    result = tirtc_font_digits_load_external(loaded);
    if (result) goto failed;
    result = tirtc_icons_load_external(loaded);
    if (result) goto failed;
    result = ui_face_load_external(loaded);
    if (result) goto failed;
    tirtc_font_cache_init(font, recovery_generation);
    liot_rtos_enter_critical();
    s_cache_allocation = font;
    s_background = background;
    s_asset_snapshot.state = TIRTC_ASSETS_READY;
    s_asset_snapshot.error = 0;
    liot_rtos_exit_critical();
    liot_trace("[assets14] LOAD_OK font=%lu bg=%lu\r\n", (unsigned long)TIRTC_FONT_BYTES,
               (unsigned long)TIRTC_BACKGROUND_BYTES);
    liot_trace("[assets24] FONT_CACHE bytes=%lu slots=%lu background=%lu\r\n",
               (unsigned long)TIRTC_FONT_CACHE_BYTES, (unsigned long)TIRTC_FONT_CACHE_SLOTS,
               (unsigned long)TIRTC_BACKGROUND_BYTES);
    /* Permanent single file owner. All glyph I/O yields independently of LVGL.
     * A failed read keeps the already-bound background and cache allocations. */
    for (;;) {
        bool progress = tirtc_font_cache_worker_step();
        liot_rtos_task_sleep_ms(progress ? 1U : 10U);
    }
failed:
    if (background) liot_rtos_free(background);
    if (font) liot_rtos_free(font);
    background = font = NULL;
    /* Retry the complete read only; storage owns remount/recovery and all
     * writes. Never retain a partly verified allocation or a stale fd. The
     * sole worker remains alive, so concurrent start calls cannot duplicate it. */
    retryable = result == TIRTC_STORAGE_E_IO || result == TIRTC_STORAGE_E_TIMEOUT ||
        result == TIRTC_STORAGE_E_BUSY || result == TIRTC_ASSET_E_NOMEM;
    /* The statistics worker can detect the fault between our chunks and
     * invalidate our fd first. In that case read/close sees NOT_READY or
     * LittleFS BADF, not the original SPI error. Reopen only with evidence of
     * storage recovery; an ordinary bad handle/unknown volume stays terminal. */
    if (storage_was_ready && (result == TIRTC_STORAGE_E_NOT_READY || result == -9)) {
        tirtc_storage_snapshot_t storage;
        tirtc_storage_get_snapshot(&storage);
        retryable = storage.recovery_pending || storage.recovery_successes != recovery_generation;
    }
    if (retryable) {
        liot_rtos_enter_critical();
        s_asset_snapshot.retry_delay_ms = retry_ms;
        liot_rtos_exit_critical();
        publish(TIRTC_ASSETS_RETRY_WAIT, result);
        liot_trace("[assets14] LOAD_RETRY error=%d delay_ms=%lu\r\n", result, (unsigned long)retry_ms);
        liot_rtos_task_sleep_ms(retry_ms);
        retry_ms = retry_ms >= 30000U ? ASSET_RETRY_MAX_MS : retry_ms * 2U;
        goto retry;
    }
    publish(TIRTC_ASSETS_ERROR, result);
    liot_trace("[assets14] LOAD_FAILED error=%d; matching resource installation required\r\n", result);
    /* No public task handle is reused; buffers live until reboot. */
    (void)liot_rtos_task_delete(NULL);
}

int tirtc_assets_start(void)
{
    int result;
    liot_rtos_enter_critical();
    if (s_asset_started) {
        result = s_asset_snapshot.state == TIRTC_ASSETS_ERROR ? s_asset_snapshot.error : 0;
        liot_rtos_exit_critical();
        return result;
    }
    s_asset_started = true;
    s_asset_snapshot.state = TIRTC_ASSETS_WAIT_STORAGE;
    s_asset_snapshot.total_bytes = TIRTC_UI_ASSET_TOTAL_BYTES;
    liot_rtos_exit_critical();
    result = liot_rtos_task_create(&s_asset_task, 4096U, ASSET_TASK_PRIORITY, "tirtc_assets", asset_worker, NULL, 0U);
    if (result) {
        liot_rtos_enter_critical();
        s_asset_started = false;
        s_asset_task = NULL;
        s_asset_snapshot.state = TIRTC_ASSETS_ERROR;
        s_asset_snapshot.error = result > 0 ? -result : result;
        liot_rtos_exit_critical();
        liot_trace("[assets14] loader task failed ret=%d\r\n", result);
    }
    return result;
}
