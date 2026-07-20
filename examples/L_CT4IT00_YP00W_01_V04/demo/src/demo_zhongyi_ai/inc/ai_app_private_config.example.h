/*
 * Copy this file to ai_app_private_config.h for local board testing.
 * ai_app_private_config.h is ignored by git. Do not commit real secrets.
 */

#ifndef AI_APP_PRIVATE_CONFIG_H
#define AI_APP_PRIVATE_CONFIG_H

#define AI_APP_DEVICE_TYPE ""
#define AI_APP_DEVICE_ID ""
#define AI_APP_SECRET_KEY ""

/* Optional: omit this define to use the SDK_VERSION build macro. */
/* #define AI_APP_AI_SDK_VER "" */

/* Optional: only used if AI_APP_ALLOW_PRESET_TOKEN_FALLBACK is set to 1. */
#define AI_APP_PRESET_AI_TOKEN ""
#define AI_APP_ALLOW_PRESET_TOKEN_FALLBACK 0

/* Leave APN empty to use module/SIM default APN when available. */
#define AI_APP_APN ""
#define AI_APP_APN_USER ""
#define AI_APP_APN_PASSWORD ""
#define AI_APP_APN_AUTH_TYPE 0

/*
 * Application log output.
 * By default app logs are plain text on the Windows "Lierda Uart Port",
 * which is L_USBCOM in the SDK UART API:
 *   AIAPP: ...
 * Set AI_APP_LOG_TRACE_ENABLE to 1 only if you also want syslog/EPAT output.
 */
#define AI_APP_LOG_TRACE_ENABLE 0
#define AI_APP_LOG_UART_ENABLE 1
#define AI_APP_LOG_UART_PORT L_USBCOM
#define AI_APP_LOG_UART_BAUDRATE L_UART_BR_115200

#endif /* AI_APP_PRIVATE_CONFIG_H */
