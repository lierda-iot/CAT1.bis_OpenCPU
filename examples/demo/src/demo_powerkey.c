/**
 * @file demo_powerkey.c
 * @brief Power key event and status query demo.
 */

#include "liot_log.h"
#include "liot_os.h"
#include "liot_power.h"

#define ENABLE_KEY_POWEROFF 1
#define POWERKEY_LONG_PRESS_TIME_MS 5000

static void powerkey_demo_callback(liot_pwrkey_statusmode_e status)
{
    liot_trace("power key status: %d", status);

    switch (status)
    {
        case LIOT_PWRKEY_PRESS:
            liot_trace("power key pressed");
            break;

        case LIOT_PWRKEY_RELEASE:
            liot_trace("power key released");
            break;

        case LIOT_PWRKEY_LONGPRESS:
            liot_trace("power key long pressed");
#if ENABLE_KEY_POWEROFF
            liot_power_down(LIOT_POWD_NORMAL);
#endif
            break;

        case LIOT_PWRKEY_REPEAT:
            liot_trace("power key remains pressed after long press");
            break;

        default:
            liot_trace("unknown power key status: %d", status);
            break;
    }
}

void liot_powerkey_demo_task(void *argv)
{
    liot_power_errcode_e ret;
    UINT8 pin_level = 0;

    (void)argv;

    liot_rtos_task_sleep_ms(500);
    liot_trace("==== demo powerkey start ====");

    ret = liot_pwrkey_callback_register(powerkey_demo_callback);
    if (ret != LIOT_POWER_POWD_SUCCESS)
    {
        liot_trace("register power key callback failed: %d", ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    ret = liot_pwrkey_shutdown_time_set(POWERKEY_LONG_PRESS_TIME_MS);
    if (ret != LIOT_POWER_POWD_SUCCESS)
    {
        liot_trace("set power key long press time failed: %d", ret);
        liot_pwrkey_callback_register(NULL);
        liot_rtos_task_delete(NULL);
        return;
    }

    while (1)
    {
        ret = liot_get_pwrkey_status(&pin_level);
        if (ret == LIOT_POWER_POWD_SUCCESS)
            liot_trace("power key pin value: %d", pin_level);
        else
            liot_trace("get power key status failed: %d", ret);

        liot_rtos_task_sleep_ms(1000);
    }
}
