#ifndef TIRTC_FONT_CACHE_H
#define TIRTC_FONT_CACHE_H
#include <stdbool.h>
#include <stdint.h>

#define TIRTC_FONT_CACHE_SLOTS 512U
#define TIRTC_FONT_CACHE_GLYPH_BYTES 128U
#define TIRTC_FONT_CACHE_BYTES (TIRTC_FONT_CACHE_SLOTS * TIRTC_FONT_CACHE_GLYPH_BYTES)
typedef struct {
    uint32_t hits, misses, loads, evictions, retries;
    uint32_t entries, pending;
    int32_t error;
} tirtc_font_cache_snapshot_t;

/* Initialize before publishing assets READY. The allocation lives until reboot.
 * Afterwards only the LVGL owner may call get/process. process is called before
 * lv_timer_handler, never from inside a glyph draw. Returned pixels are stable
 * through that entire render; the worker NEVER modifies a cache slot. */
void tirtc_font_cache_init(uint8_t *allocation, uint32_t storage_generation);
const uint8_t *tirtc_font_cache_get(uint32_t offset, uint32_t bytes);
bool tirtc_font_cache_process(void);
/* Sole resource worker, no UI locks held; all storage I/O stays here. */
bool tirtc_font_cache_worker_step(void);
void tirtc_font_cache_get_snapshot(tirtc_font_cache_snapshot_t *out);
#endif
