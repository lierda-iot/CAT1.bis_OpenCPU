#ifndef LOCATION_CORE_UTILS_H
#define LOCATION_CORE_UTILS_H

#include <stddef.h>
#include <stdint.h>

/*
 * Pure coordinate formatting logic extracted from demo_location.c.
 * No liot/cJSON/lvgl dependency: host-testable as-is.
 */

void location_format_coordinate(char *buffer, size_t size, int32_t value_e7,
                                char positive, char negative);
void location_format_coordinate_compact(char *buffer, size_t size, int32_t value_e7);

#endif /* LOCATION_CORE_UTILS_H */
