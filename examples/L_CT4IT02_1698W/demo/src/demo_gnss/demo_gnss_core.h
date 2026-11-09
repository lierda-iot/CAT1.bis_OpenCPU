#ifndef DEMO_GNSS_CORE_H
#define DEMO_GNSS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Pure NMEA parsing/coordinate logic extracted from demo_gnss.c.
 * Header-only on purpose: the production call site keeps identical behavior
 * with no liot/UART dependency, so the same source is host-testable as-is.
 */

/*
 * Split a comma-separated NMEA sentence into fields.
 * Returns the number of fields written (0 if line is NULL/empty).
 */
static inline int gnss_split_fields(const char *line, char *copy, size_t copy_size,
                                    char **field, size_t field_max)
{
    char *cursor;
    size_t count = 0;

    if (line == NULL || copy == NULL || copy_size == 0U ||
        field == NULL || field_max == 0U) {
        return 0;
    }

    copy[0] = '\0';
    strncpy(copy, line, copy_size - 1U);
    copy[copy_size - 1U] = '\0';
    if (copy[0] == '\0') {
        return 0;
    }

    cursor = copy;
    field[count++] = cursor;
    while (*cursor != '\0' && count < field_max) {
        if (*cursor == ',') {
            *cursor = '\0';
            field[count++] = cursor + 1;
        }
        cursor++;
    }
    return (int)count;
}

/*
 * Convert an NMEA ddmm.mmmmm coordinate plus hemisphere into e7 fixed-point.
 * Returns false on NULL/empty input, non-digit input, invalid hemisphere or
 * minutes >= 60; truncates fraction digits beyond 7 places.
 */
static inline bool gnss_coordinate_to_e7(const char *value, char hemisphere,
                                         int32_t *coordinate_e7)
{
    const char *dot;
    uint32_t whole = 0;
    uint32_t fraction = 0;
    uint32_t fraction_scale = 1;
    uint32_t degrees;
    uint32_t minutes;
    uint64_t minute_e7;
    int32_t result;
    int digits = 0;

    if (value == NULL || coordinate_e7 == NULL || value[0] == '\0') return false;
    dot = strchr(value, '.');
    while (*value != '\0' && *value != '.') {
        if (*value < '0' || *value > '9') return false;
        whole = whole * 10U + (uint32_t)(*value - '0');
        value++;
    }
    if (dot != NULL) {
        value = dot + 1;
        while (*value >= '0' && *value <= '9' && digits < 7) {
            fraction = fraction * 10U + (uint32_t)(*value - '0');
            fraction_scale *= 10U;
            value++;
            digits++;
        }
    }
    degrees = whole / 100U;
    minutes = whole % 100U;
    if (minutes >= 60U) return false;
    minute_e7 = (uint64_t)minutes * 10000000ULL;
    if (fraction_scale > 1U) {
        minute_e7 += ((uint64_t)fraction * 10000000ULL) / fraction_scale;
    }
    result = (int32_t)((uint64_t)degrees * 10000000ULL + minute_e7 / 60ULL);
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    else if (hemisphere != 'N' && hemisphere != 'E') return false;
    *coordinate_e7 = result;
    return true;
}

#endif /* DEMO_GNSS_CORE_H */
