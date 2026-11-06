/* V04 KEY0/KEY1 volume events. No UI, persistence or audio I/O here. */
#ifndef TIRTC_KEYS_H
#define TIRTC_KEYS_H
#include <stdint.h>

/* Call once after the display has enabled the board's shared 3.3 V rail.
 * Configures only KEY0 (GPIO20/modem pin5) and KEY1 (GPIO22/modem pin19).
 * Returns 0 on success; negative on pin configuration failure. A failed
 * initialization emits no events, and may be retried. No IRQ is registered. */
int tirtc_keys_init(void);

/* Single-consumer nonblocking poll, normally every 20 ms, using the SDK's
 * millisecond running time. Returns -1 (KEY0), +1 (KEY1), or 0. Caller
 * applies/clamps the speaker level and schedules persistence separately.
 * 80 ms debounce; repeat after 600 ms then every 200 ms, max one per poll.
 * Startup-held, both-held and invalid inputs require a stable full release
 * before another press. No task, heap, waits, UI callbacks or file writes. */
int tirtc_keys_poll(uint32_t now_ms);
#endif
