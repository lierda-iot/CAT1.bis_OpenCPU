/**
 * @file liot_router_reset.c
 * @brief 复位按键 - 长按5秒清NV重启
 */

#include "liot_router_user.h"
#include "lierda_app_main.h"
#include "liot_fs_api.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_power.h"

static liot_timer_t gResetTimer = NULL;

static void reset_timer_cb(void *arg)
{
    (void)arg;
    if (Liot_WakeupPadGetLevel((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD) == L_IO_LOW) {
        liot_trace("reset: long press, clear NV & reboot");
        liot_remove(LIOT_ROUTER_NV_FILE);
        liot_power_reset(LIOT_RESET_NORMAL);
    }
}

static void reset_wakeup_handler(void)
{
    if (Liot_WakeupPadGetLevel((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD) == L_IO_LOW) {
        liot_rtos_timer_start(gResetTimer, LIOT_ROUTER_RESET_LONG_PRESS_MS);
    } else {
        liot_rtos_timer_stop(gResetTimer);
    }
}

void Liot_RouterResetInit(void)
{
    liot_wakeup_cfg_t cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_BOTH,
    };
    Liot_WakeupIntInit((liot_wakeuppad_e)LIOT_ROUTER_RESET_WAKEUP_PAD, cfg, reset_wakeup_handler, NULL);
    liot_rtos_timer_create(&gResetTimer, LIOT_TimerOnce, reset_timer_cb, NULL);
    liot_trace("reset init done, pad=%d", LIOT_ROUTER_RESET_WAKEUP_PAD);
}
