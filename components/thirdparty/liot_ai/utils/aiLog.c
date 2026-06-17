#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "aiLog.h"
#include "liot_log.h"

static int g_logDefLevel = 0;

void aiLogInit(int level)
{
    g_logDefLevel = level;
}

void aiLogRelease(void)
{
}

void aiLog(int level, const char *fmt, ...)
{
    va_list args;
    char buf[256];

    if (level < g_logDefLevel) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    liot_trace("%s\n", buf);
}
