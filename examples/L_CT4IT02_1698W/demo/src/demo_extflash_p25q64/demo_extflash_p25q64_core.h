#ifndef DEMO_EXTFLASH_P25Q64_CORE_H
#define DEMO_EXTFLASH_P25Q64_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Pure pattern/tick helpers shared by demo_extflash_p25q64.c and
 * demo_extflash_p25q64_lfs.c. Header-only on purpose so the same source is
 * host-testable without the liot RTOS / flash dependency.
 */

/*
 * Raw flash demo pattern: buffer[i] = seed + i (uint8 wrap).
 * Mirrors demo_extflash_p25q64_demo_fill_pattern.
 */
static inline void demo_extflash_p25q64_core_fill_pattern(uint8_t *buffer,
                                                          uint32_t len,
                                                          uint8_t seed)
{
    uint32_t i;

    if (buffer == NULL) {
        return;
    }

    for (i = 0; i < len; ++i) {
        buffer[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

/*
 * LittleFS demo pattern: buffer[i] = seed + i*3 (uint8 wrap).
 * Mirrors demo_extflash_p25q64_lfs_demo_fill_pattern.
 */
static inline void demo_extflash_p25q64_core_fs_fill_pattern(uint8_t *buffer,
                                                             uint32_t len,
                                                             uint8_t seed)
{
    uint32_t i;

    if (buffer == NULL) {
        return;
    }

    for (i = 0; i < len; ++i) {
        buffer[i] = (uint8_t)(seed + (uint8_t)(i * 3U));
    }
}

/*
 * Ticks elapsed between now and start_tick with uint32 wraparound.
 * Mirrors demo_tick_elapsed in both demos. `now` is injected so the wrap
 * behavior is host-testable; production callers keep using demo_tick_now().
 */
static inline uint32_t demo_extflash_p25q64_core_tick_elapsed(uint32_t now,
                                                              uint32_t start_tick)
{
    return (uint32_t)(now - start_tick);
}

#endif /* DEMO_EXTFLASH_P25Q64_CORE_H */
