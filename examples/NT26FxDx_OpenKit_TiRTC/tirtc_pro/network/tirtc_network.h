/* F6D cellular route monitor, automatic recovery, and cached time readiness. */
#ifndef TIRTC_NETWORK_H
#define TIRTC_NETWORK_H
#include "tirtc_ui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    bool ready;
    int sim_id;
    uint32_t generation;
    bool time_valid;
} tirtc_network_snapshot_t;
/* No modem or filesystem I/O. Task context; stale routes become not ready.
 * generation changes when SIM, IP, ready state, or cache freshness changes. */
void tirtc_network_get_snapshot(tirtc_network_snapshot_t *out);
bool tirtc_network_is_ready(void);
bool tirtc_network_time_ready(void);
/* Cache-only 5 s heartbeat: invalidates stale UI status even if modem I/O waits. */
void tirtc_network_publish(void);
/* Call after successful tirtc_port_start(), once its mailbox mutex exists.
 * Creates a 16 KiB task at priority 10 and an independent time worker. */
int tirtc_network_start(void);
/* LVGL callback router: only queues NETWORK_REFRESH / NETWORK_RECONNECT.
 * Returns 0 when queued, -1 when unhandled or not started. No modem I/O here. */
int tirtc_network_action(const tirtc_ui_action_t *action, void *context);
#ifdef __cplusplus
}
#endif
#endif
