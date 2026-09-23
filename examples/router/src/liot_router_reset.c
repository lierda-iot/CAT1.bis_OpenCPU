/**
 * @file liot_router_reset.c
 * @brief Reset button: short press(>=200ms) reboot, long press(5s) clear NV & reboot
 */

#include "liot_router_user.h"
#include "lierda_app_main.h"
#include "liot_fs_api.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_power.h"

#define RESET_SHORT_PRESS_MS  500
#define RESET_LONG_PRESS_MS  (LIOT_ROUTER_RESET_LONG_PRESS_MS)

static liot_timer_t s_resetTimer = NULL;


static void reset_timer_cb(void *arg)
{
    (void)arg;
    static uint8_t reset_key_state = 0;
    liot_rtos_timer_stop(s_resetTimer);
    if (reset_key_state == 0)
    {
        if (Liot_WakeupPadGetLevel((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD) == L_IO_LOW)
        {
            liot_trace("reset: long press");
            reset_key_state = 1;
            liot_rtos_timer_start(s_resetTimer, RESET_LONG_PRESS_MS);
        }
        else
        {
            liot_trace("reset: short press, reboot");
            liot_power_reset(LIOT_RESET_NORMAL);
        }
    }
    else
    {
        if (Liot_WakeupPadGetLevel((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD) == L_IO_LOW)
        {
            reset_key_state = 0;
            liot_trace("reset: long press, clear NV & reboot");
            liot_remove(LIOT_ROUTER_NV_FILE);
            liot_power_reset(LIOT_RESET_NORMAL);
        }
    }
}

static void reset_wakeup_handler(void)
{
    if (Liot_WakeupPadGetLevel((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD) == L_IO_LOW)
    {
        liot_trace("reset key pressed");
        liot_rtos_timer_start(s_resetTimer, RESET_SHORT_PRESS_MS);
    }
}

void Liot_RouterResetInit(void)
{
    liot_rtos_timer_create(&s_resetTimer, LIOT_TimerOnce, reset_timer_cb, NULL);
    liot_wakeup_cfg_t cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL,
    };
    Liot_WakeupIntInit((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD, cfg, reset_wakeup_handler, NULL);
    liot_trace("reset init done, pad=%d", LIOT_ROUTER_RESET_WAKEUP_PAD);
}
