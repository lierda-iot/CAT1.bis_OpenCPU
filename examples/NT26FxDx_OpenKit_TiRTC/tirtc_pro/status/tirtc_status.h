#ifndef TIRTC_STATUS_H
#define TIRTC_STATUS_H
#ifdef __cplusplus
extern "C" {
#endif
/* Called by the main application after the UI mailbox mutex exists.
 * publish() never queries the modem: call once before start(), then every 5 s.
 * start() creates only the independent, potentially slow identity reader. */
int tirtc_status_start(void);
void tirtc_status_publish(void);
/* Task context only. Bounded UTF-8 message, adjacent duplicate suppression,
 * eight recent entries with actual uptime; no hardware or business actions. */
void tirtc_status_event(const char *message);
#ifdef __cplusplus
}
#endif
#endif
