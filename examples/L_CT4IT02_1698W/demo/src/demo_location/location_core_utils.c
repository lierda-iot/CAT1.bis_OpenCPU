#include "location_core_utils.h"

#include <stdio.h>

void location_format_coordinate(char *buffer, size_t size, int32_t value_e7,
                                char positive, char negative)
{
    int64_t value = value_e7;
    char direction = positive;
    if (value < 0) {
        value = -value;
        direction = negative;
    }
    snprintf(buffer, size, "%ld.%07ld %c",
             (long)(value / 10000000),
             (long)(value % 10000000), direction);
}

void location_format_coordinate_compact(char *buffer, size_t size, int32_t value_e7)
{
    int64_t value = value_e7;
    uint64_t abs_value = (uint64_t)(value < 0 ? -value : value);
    snprintf(buffer, size, "%s%ld.%07ld",
             value < 0 ? "-" : "",
             (long)(abs_value / 10000000U),
             (long)(abs_value % 10000000U));
}
