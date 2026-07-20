#include <stdarg.h>
#include <stdio.h>
#include "liot_log.h"

void tgt_log_printf(const char *content)
{
    liot_trace("%s", content);
}

