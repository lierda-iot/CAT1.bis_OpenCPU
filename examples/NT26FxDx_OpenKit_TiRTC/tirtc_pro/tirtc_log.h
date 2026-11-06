#ifndef TIRTC_APP_LOG_H
#define TIRTC_APP_LOG_H

#include "liot_log.h"

/* Normal firmware retains startup/error logs. Detailed periodic traces are
 * compiled out; UI snapshots and recovery/state machines continue to run. */
#ifndef TIRTC_ENABLE_DEBUG_LOG
#define TIRTC_ENABLE_DEBUG_LOG 0
#endif

#define TIRTC_LOG_DEBUG(...) do { \
    if (TIRTC_ENABLE_DEBUG_LOG) liot_trace(__VA_ARGS__); \
} while (0)

#endif
