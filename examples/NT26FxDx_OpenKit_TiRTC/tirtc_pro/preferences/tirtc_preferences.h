#ifndef TIRTC_PREFERENCES_H
#define TIRTC_PREFERENCES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t volume;
    uint8_t mic_gain;
    bool speaker_enabled;
    bool mic_enabled;
} tirtc_preferences_t;

typedef struct {
    bool initialized;
    bool dirty;
    bool saving;
    int32_t save_error;
    uint32_t sequence;
    bool load_pending;
    int32_t load_error;
    uint32_t value_revision;
    uint32_t save_revision; /* Incremented only after a verified write, not a load. */
    tirtc_preferences_t value;
} tirtc_preferences_snapshot_t;

typedef enum {
    TIRTC_PREFERENCES_VOLUME,
    TIRTC_PREFERENCES_MIC_GAIN,
    TIRTC_PREFERENCES_SPEAKER_ENABLED,
    TIRTC_PREFERENCES_MIC_ENABLED
} tirtc_preferences_field_t;

/* Call once before starting the UI, from a task allowed to do synchronous
 * NVM I/O. Returns 0 for a valid saved record, 1 for defaults, -4 if another
 * init is in progress, or a negative load error. Unknown slots are retried
 * by poll and block saves until their sequence is known. Defaults are
 * 8/10/true/true and are not auto-written. No loop/wait for retry in init. */
int tirtc_preferences_init(void);

/* Task-context, bounded RAM-only access. A NULL output is ignored. */
void tirtc_preferences_get(tirtc_preferences_t *out);
void tirtc_preferences_get_snapshot(tirtc_preferences_snapshot_t *out);

/* UI-safe: validate, copy to RAM and mark dirty. now_ms is monotonic SDK
 * milliseconds. 0 includes no-op, -2 invalid input, -3 not initialized. */
int tirtc_preferences_set(const tirtc_preferences_t *value, uint32_t now_ms);

/* Atomic field updates for UI/key callers; avoid a separate get/set pair.
 * update validates 0..10 levels and 0/1 switches. adjust clamps volume at
 * 0..10 even for an extreme delta. Optional out returns the committed RAM
 * snapshot under the same lock. Neither function performs I/O or logging. */
int tirtc_preferences_update(tirtc_preferences_field_t field, int value,
                              uint32_t now_ms, tirtc_preferences_t *out);
int tirtc_preferences_adjust_volume(int delta, uint32_t now_ms,
                                     tirtc_preferences_t *out);

/* One background/main task calls every 500-1000 ms. At least 1000 ms after
 * the last real change, write the alternate 32-byte NVM slot and read it
 * back. Failure retries after 5000 ms. Unreadable boot slots are re-probed
 * read-only on the same cadence; user-edited fields win over a later load.
 * Never call from the UI or an ISR. SDK filesystem calls are synchronous;
 * use a dedicated persistence task if its daemon stalls must be isolated. */
void tirtc_preferences_poll(uint32_t now_ms);

#endif
