/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT
 */

#ifndef TIRTC_APP_JSON_GUARD_H
#define TIRTC_APP_JSON_GUARD_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cJSON.h"

/* The linked parser uses 104 bytes per recursive value frame.  Keep external
 * messages within the same nesting budget as the GROUP service worker. */
#define DEMO_JSON_MAX_DEPTH 16U

/* Return the opening bracket which exceeds the budget, or NULL.  This is
 * only a recursion guard: cJSON still owns syntax and trailing-data checks.
 * Stop after the first container so prefix-accepting Parse variants retain
 * their behavior even when ignored trailing data contains more brackets. */
static inline const char *demo_json_depth_error(const char *value,
                                                size_t length)
{
    size_t i = 0U;
    unsigned int depth = 0U;
    bool quoted = false;
    bool escaped = false;

    if (value == NULL)
    {
        return NULL;
    }
    /* Match cJSON 1.7.16's BOM and leading-whitespace handling. */
    if (length > 4U && (unsigned char)value[0] == 0xEFU &&
        (unsigned char)value[1] == 0xBBU &&
        (unsigned char)value[2] == 0xBFU)
    {
        i = 3U;
    }
    while (i < length && (unsigned char)value[i] <= 32U)
    {
        ++i;
    }
    if (i == length || (value[i] != '[' && value[i] != '{'))
    {
        /* Strings, numbers and literals cannot recurse. */
        return NULL;
    }
    for (; i < length; ++i)
    {
        const char c = value[i];

        if (quoted)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                quoted = false;
            }
        }
        else if (c == '"')
        {
            quoted = true;
        }
        else if (c == '[' || c == '{')
        {
            if (++depth > DEMO_JSON_MAX_DEPTH)
            {
                return value + i;
            }
        }
        else if (c == ']' || c == '}')
        {
            if (--depth == 0U)
            {
                return NULL;
            }
        }
    }
    return NULL;
}

static inline cJSON *demo_json_parse(const char *value)
{
    if (value != NULL &&
        demo_json_depth_error(value, strlen(value) + 1U) != NULL)
    {
        return NULL;
    }
    return cJSON_Parse(value);
}

static inline cJSON *demo_json_parse_with_length(const char *value,
                                               size_t length)
{
    if (demo_json_depth_error(value, length) != NULL)
    {
        return NULL;
    }
    return cJSON_ParseWithLength(value, length);
}

static inline cJSON *demo_json_parse_with_length_opts(
    const char *value, size_t length, const char **return_parse_end,
    cJSON_bool require_null_terminated)
{
    const char *error = demo_json_depth_error(value, length);

    if (error != NULL)
    {
        if (return_parse_end != NULL)
        {
            *return_parse_end = error;
        }
        return NULL;
    }
    return cJSON_ParseWithLengthOpts(value, length, return_parse_end,
                                     require_null_terminated);
}

#endif /* TIRTC_APP_JSON_GUARD_H */
