#ifndef WATCH_EARTH_2015_CORE_H
#define WATCH_EARTH_2015_CORE_H

#include <stdint.h>

/*
 * Pure watchface hand-angle conversion (1/10 degree), extracted from
 * watch_earth_2015.c so the arithmetic can be host-tested without lvgl.
 */

static inline int16_t watch_earth_2015_hour_angle(int hour, int minute, int second)
{
    return (int16_t)(((hour % 12) * 300) + (minute * 5) + (second / 12));
}

static inline int16_t watch_earth_2015_minute_angle(int minute, int second)
{
    return (int16_t)((minute * 60) + second);
}

static inline int16_t watch_earth_2015_second_angle(int second)
{
    return (int16_t)(second * 60);
}

#endif /* WATCH_EARTH_2015_CORE_H */
