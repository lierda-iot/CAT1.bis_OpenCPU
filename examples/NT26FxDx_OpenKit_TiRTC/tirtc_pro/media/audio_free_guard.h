#ifndef PRO_AUDIO_FREE_GUARD_H
#define PRO_AUDIO_FREE_GUARD_H

#include <stdint.h>

typedef struct {
    uint32_t skipped;
    uintptr_t last_pdu;
    uint8_t free_pcb;
    uint32_t pending;
    uint32_t reclaimed;
    uint32_t stop_confirmations;
    uint32_t fault;
} demo_audio_free_guard_stats_t;

/* Task context only. This is diagnostic telemetry, not an ownership API. */
void demo_audio_free_guard_snapshot(demo_audio_free_guard_stats_t *out);
void demo_audio_free_guard_begin_stop(void);
/* Task context, only after both SDK stream stops and before a new capture. */
int demo_audio_free_guard_reclaim(void);

#endif
