#ifndef __LIERDA_LOG_H__
#define __LIERDA_LOG_H__

#include "liot_log.h"

#define UNILOG_LIOT_OPEN    0

#define P_DEBUG     0
#define P_INFO      1
#define P_WARNING   2
#define P_ERROR     3

#define LIOT_PRINTF(module, level, fmt, ...) liot_trace(fmt "\r\n", ##__VA_ARGS__)

#endif
