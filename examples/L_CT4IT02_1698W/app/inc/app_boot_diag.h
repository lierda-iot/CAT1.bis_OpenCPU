#ifndef APP_BOOT_DIAG_H
#define APP_BOOT_DIAG_H

#include "liot_log.h"
#include "liot_power.h"

#ifndef APP_BUILD_GIT_SHORT
#define APP_BUILD_GIT_SHORT "unknown"
#endif

#ifndef APP_BUILD_PROJECT
#define APP_BUILD_PROJECT "unknown"
#endif

#ifndef APP_BUILD_MODE
#define APP_BUILD_MODE "unknown"
#endif

#define APP_BOOT_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)

static inline const char *app_boot_powerup_reason_name(UINT8 reason)
{
    switch (reason) {
    case LIOT_PWRUP_UNKNOWN:
        return "unknown";
    case LIOT_PWRUP_PWRKEY:
        return "pwrkey";
    case LIOT_PWRUP_PIN_RESET:
        return "pin_reset";
    case LIOT_PWRUP_ALARM:
        return "alarm";
    case LIOT_PWRUP_CHARGE:
        return "charge";
    case LIOT_PWRUP_WDG:
        return "watchdog";
    case LIOT_PWRUP_PSM_WAKEUP:
        return "psm_wakeup";
    case LIOT_PWRUP_PANIC:
        return "panic";
    default:
        return "invalid";
    }
}

static inline const char *app_boot_powerup_reason_class(UINT8 reason)
{
    switch (reason) {
    case LIOT_PWRUP_WDG:
        return "wdg";
    case LIOT_PWRUP_PANIC:
        return "panic";
    case LIOT_PWRUP_PIN_RESET:
        return "pin_reset";
    case LIOT_PWRUP_PWRKEY:
        return "user_power";
    case LIOT_PWRUP_ALARM:
    case LIOT_PWRUP_CHARGE:
    case LIOT_PWRUP_PSM_WAKEUP:
        return "non_fault";
    case LIOT_PWRUP_UNKNOWN:
        return "unknown";
    default:
        return "other";
    }
}

static inline void app_boot_diag_log_reason(const char *stage,
                                            UINT8 reason,
                                            liot_power_errcode_e power_ret)
{
    APP_BOOT_MARK_TRACE("BOOT_REASON stage=%s git=%s project=%s mode=%s reason=%u name=%s class=%s ret=%d",
                        (stage != NULL) ? stage : "-",
                        APP_BUILD_GIT_SHORT,
                        APP_BUILD_PROJECT,
                        APP_BUILD_MODE,
                        (unsigned int)reason,
                        app_boot_powerup_reason_name(reason),
                        app_boot_powerup_reason_class(reason),
                        (int)power_ret);
}

static inline void app_boot_diag_log_stage(const char *stage)
{
    APP_BOOT_MARK_TRACE("BOOT_STAGE stage=%s git=%s project=%s mode=%s",
                        (stage != NULL) ? stage : "-",
                        APP_BUILD_GIT_SHORT,
                        APP_BUILD_PROJECT,
                        APP_BUILD_MODE);
}

#endif
