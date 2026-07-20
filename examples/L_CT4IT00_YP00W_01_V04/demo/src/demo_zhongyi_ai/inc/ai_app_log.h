/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AI_APP_LOG_H
#define AI_APP_LOG_H

void ai_app_log_init(void);
void ai_app_log_printf(const char *fmt, ...);

#define AI_APP_LOG(fmt, ...) ai_app_log_printf(fmt, ##__VA_ARGS__)

#ifndef AI_APP_LOG_NO_PRINTF_ALIAS
#ifdef printf
#undef printf
#endif
#define printf(fmt, ...) ai_app_log_printf(fmt, ##__VA_ARGS__)
#endif

#ifndef AI_APP_LOG_NO_LIOT_TRACE_ALIAS
#ifdef liot_trace
#undef liot_trace
#endif
#define liot_trace(fmt, ...) ai_app_log_printf(fmt, ##__VA_ARGS__)
#endif

#endif /* AI_APP_LOG_H */
