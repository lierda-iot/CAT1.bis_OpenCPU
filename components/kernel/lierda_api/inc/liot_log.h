#ifndef __LIOT_LOG_H__
#define __LIOT_LOG_H__

int syslogPrintf(const char *fmt, ...);

#define liot_trace(fmt, ...) syslogPrintf(fmt, ##__VA_ARGS__)
#define printf(fmt, ...) syslogPrintf(fmt, ##__VA_ARGS__)

#endif