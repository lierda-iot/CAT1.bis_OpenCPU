#include "lws_adapt.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "liot_log.h"

// extern int gettimeofday(struct timeval *tv, void *tz);

// int _gettimeofday(struct timeval *tv, void *tz)
// {
//     return gettimeofday(tv, tz);
// }

void liot_lws_log(void *cx, const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    liot_trace("%s\n", buf);
}