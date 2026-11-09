#ifndef DEMO_SC7A20H_CORE_H
#define DEMO_SC7A20H_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Pure SC7A20H demo logic extracted from demo_sc7a20h.c.
 * Header-only on purpose: production call sites keep identical behavior with
 * no lvgl/liot dependency, so the same source is host-testable as-is.
 */

/* Sensitivity level (0-128) -> acceleration threshold in mg (level * 32). */
static inline int sc7a20h_demo_threshold_mg(uint8_t level)
{
    return (int)level * 32;
}

/* Shake hit is complete once the accumulated interrupt count reaches the
 * configured number of shakes. */
static inline bool sc7a20h_demo_shake_hit(int shake_count, uint8_t times)
{
    return shake_count >= (int)times;
}

/* Click hit is complete when the reported click count equals the configured
 * number of clicks. */
static inline bool sc7a20h_demo_click_hit(uint8_t click_times, uint8_t times)
{
    return click_times == times;
}

#endif /* DEMO_SC7A20H_CORE_H */
