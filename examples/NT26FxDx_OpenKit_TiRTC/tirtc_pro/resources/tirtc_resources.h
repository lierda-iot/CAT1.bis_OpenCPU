/* Resource snapshots for the NT26F6D0 / F6D_A application. */
#ifndef TIRTC_RESOURCES_H
#define TIRTC_RESOURCES_H

#include "tirtc_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fast periodic snapshot: layout, RAM, uptime and cached storage. Call once
 * immediately after the UI starts, then from one owner every five seconds.
 * Performs no filesystem operations and never waits for a storage I/O lock. */
void tirtc_resources_publish(void);

/* Start independent internal/external filesystem workers after the shared
 * peripheral power rail is ready. No device I/O occurs in this start call.
 * A worker may fail without preventing the fast snapshot or the other worker. */
int tirtc_resources_start(void);

/* Copy worker snapshots only. Called by the fast collector; host tests stub
 * this boundary. Preserves layout/heap/time fields already filled by caller. */
void tirtc_resources_read_storage(tirtc_ui_resources_t *out);

/* Implemented by the firmware layout module (host tests supply a stub).
 * Fill only flash/static-RAM fields and validity flags, plus filesystem_total.
 * The output has been zeroed. Values are bytes and describe this APP layout,
 * not entire-chip capacity. This callback must not invoke UI or blocking I/O. */
void tirtc_resources_read_layout(tirtc_ui_resources_t *out);

#ifdef __cplusplus
}
#endif
#endif
