/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define AI_APP_LOG_NO_LIOT_TRACE_ALIAS
#define AI_APP_LOG_NO_PRINTF_ALIAS
#include "ai_app_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ai_app_config.h"
#include "liot_log.h"
#include "liot_uart2.h"

#ifndef AI_APP_LOG_TRACE_ENABLE
#define AI_APP_LOG_TRACE_ENABLE 1
#endif

#ifndef AI_APP_LOG_UART_ENABLE
#define AI_APP_LOG_UART_ENABLE 1
#endif

#ifndef AI_APP_LOG_UART_PORT
/* L_USBCOM maps to the Windows "Lierda Uart Port" virtual COM port. */
#define AI_APP_LOG_UART_PORT L_USBCOM
#endif

#ifndef AI_APP_LOG_UART_BAUDRATE
#define AI_APP_LOG_UART_BAUDRATE L_UART_BR_115200
#endif

#define AI_APP_LOG_LINE_MAX 512
#define AI_APP_LOG_UART_LINE_MAX (AI_APP_LOG_LINE_MAX + 16)

#if (AI_APP_LOG_UART_ENABLE != 0)
static bool s_log_uart_checked = false;
static bool s_log_uart_ready = false;
#endif

static void ai_app_log_uart_init_once(void)
{
#if (AI_APP_LOG_UART_ENABLE != 0)
    Liot_UartConfig_t uart_cfg = {0};
    liot_uart_err_e ret = L_UART_SUCCESS;

    if (s_log_uart_checked) {
        return;
    }
    s_log_uart_checked = true;

    uart_cfg.baudrate = AI_APP_LOG_UART_BAUDRATE;
    uart_cfg.data_bit = L_UART_DATA_8;
    uart_cfg.flow_ctrl = L_UART_FC_NONE;
    uart_cfg.stop_bit = L_UART_STOP_1;
    uart_cfg.parity_bit = L_UART_PARITY_NONE;
    uart_cfg.tx_way = L_UART_TX_OPAQ;

    ret = Liot_UartInit((liot_uart_e)AI_APP_LOG_UART_PORT, &uart_cfg, NULL, NULL);
    if ((ret == L_UART_SUCCESS) || (ret == L_UART_ERR_OPEN_REPEAT)) {
        s_log_uart_ready = true;
        return;
    }

    syslogPrintf("AIAPP: uart log init failed port=%d ret=%d",
                 (int)AI_APP_LOG_UART_PORT,
                 ret);
#else
    (void)0;
#endif
}

static void ai_app_log_uart_write(const char *line)
{
#if (AI_APP_LOG_UART_ENABLE != 0)
    char uart_line[AI_APP_LOG_UART_LINE_MAX] = {0};
    int len = 0;

    if (line == NULL) {
        return;
    }

    ai_app_log_uart_init_once();
    if (!s_log_uart_ready) {
        return;
    }

    len = snprintf(uart_line, sizeof(uart_line), "AIAPP: %s\r\n", line);
    if (len <= 0) {
        return;
    }
    if ((size_t)len >= sizeof(uart_line)) {
        len = (int)strlen(uart_line);
    }

    (void)Liot_UartSend((liot_uart_e)AI_APP_LOG_UART_PORT,
                        (unsigned char *)uart_line,
                        (unsigned int)len);
#else
    (void)line;
#endif
}

void ai_app_log_init(void)
{
    ai_app_log_uart_init_once();
}

void ai_app_log_printf(const char *fmt, ...)
{
    char line[AI_APP_LOG_LINE_MAX] = {0};
    va_list ap;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

#if (AI_APP_LOG_TRACE_ENABLE != 0)
    syslogPrintf("AIAPP: %s\n", line);
#endif

    ai_app_log_uart_write(line);
}
