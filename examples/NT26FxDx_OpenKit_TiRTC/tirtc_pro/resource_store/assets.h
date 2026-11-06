#ifndef TIRTC_ASSETS_H
#define TIRTC_ASSETS_H
#include <stdbool.h>
#include <stdint.h>

enum {
    TIRTC_ASSETS_OFF = 0, TIRTC_ASSETS_WAIT_STORAGE, TIRTC_ASSETS_LOADING,
    TIRTC_ASSETS_READY, TIRTC_ASSETS_ERROR, TIRTC_ASSETS_RETRY_WAIT
};
typedef struct {
    uint32_t state, loaded_bytes, total_bytes;
    int32_t error;
    uint32_t attempts, retry_delay_ms;
} tirtc_assets_snapshot_t;

/* Task context only. Starts one background loader; never initializes storage.
 * The existing storage worker owns initialization and recovery. A single
 * worker retries transient I/O/memory errors with 5..60 second backoff; no
 * retry on missing/corrupt assets. Call again after task creation failure
 * (e.g. from the main five-second loop); start never performs file I/O. */
int tirtc_assets_start(void);
bool tirtc_assets_ready(void);
int tirtc_assets_error(void);
/* Legacy whole-font getter returns NULL: full font stays on external Flash. */
const uint8_t *tirtc_assets_font(void);
/* LVGL owner only. Nonblocking, no storage calls. A miss returns NULL; process
 * consumes completed worker reads. Invalidate the screen once when true. */
const uint8_t *tirtc_assets_glyph(uint32_t offset, uint32_t bytes);
bool tirtc_assets_font_process(void);
const uint16_t *tirtc_assets_background(void);
void tirtc_assets_get_snapshot(tirtc_assets_snapshot_t *out);
/* READY follows validation of every matching UI resource, including metadata,
 * icons, digit glyphs and face tables. These additions live in fixed BSS; the
 * existing 64KiB glyph cache and background keep their boot-long allocations. */
#endif
