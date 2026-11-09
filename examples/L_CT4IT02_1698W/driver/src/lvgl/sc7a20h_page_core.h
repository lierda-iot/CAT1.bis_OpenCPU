#ifndef SC7A20H_PAGE_CORE_H
#define SC7A20H_PAGE_CORE_H

#include <stdint.h>

/*
 * Pure SC7A20H attitude-page mapping/filter logic.
 * Header-only on purpose: the production call site passes its filter state
 * explicitly, keeping behavior identical without lvgl/liot dependencies.
 */

static inline int sc7a20h_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static inline int sc7a20h_map_dot_axis(int center, int16_t accel,
                                       int radius, int range,
                                       int min_pos, int max_pos)
{
    return sc7a20h_clamp_int(center + accel * radius / range, min_pos, max_pos);
}

static inline int sc7a20h_filter_axis(int prev, int target)
{
    return (prev * 3 + target) / 4;
}

#endif /* SC7A20H_PAGE_CORE_H */
