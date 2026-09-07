/**  @file
 *  demo_key.c
 *  @brief Key input demonstration using GPIO interrupt and wakeup pad
 *
 *  This file demonstrates how to detect key presses via:
 *  1. AON (Always-On) power domain control
 *  2. GPIO input interrupt configuration (KEY0, KEY1)
 *  3. Wakeup pad interrupt configuration (KEY2)
 */
#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define KEY0_GPIO   (L_GPIO_20)
#define KEY0        (5)

#define KEY1_GPIO   (L_GPIO_22)
#define KEY1        (19)

#define KEY2        (L_WAKEUPAD_0)

/**
 * @brief GPIO input interrupt callback for KEY0
 *
 * Triggered on falling edge of KEY0_GPIO pin.
 * @param arg Callback parameter (KEY0_GPIO passed at init)
 */
static void _demo_gpioin_int_cb0(void *arg)
{
    liot_gpiolvl_e getlevel = Liot_GpioGetLevel(KEY0_GPIO);
    liot_trace("gpio(%d) interrupt trigger, get gpio(%d) level: %d",
              (int)arg, KEY0_GPIO, getlevel);
}

/**
 * @brief GPIO input interrupt callback for KEY1
 *
 * Triggered on falling edge of KEY1_GPIO pin.
 * @param arg Callback parameter (KEY1_GPIO passed at init)
 */
static void _demo_gpioin_int_cb1(void *arg)
{
    liot_gpiolvl_e getlevel = Liot_GpioGetLevel(KEY1_GPIO);
    liot_trace("gpio(%d) interrupt trigger, get gpio(%d) level: %d",
              (int)arg, KEY1_GPIO, getlevel);
}

/**
 * @brief Wakeup pad interrupt callback for KEY2
 *
 * Triggered on falling edge of KEY2 wakeup pad.
 * @param arg Callback parameter (unused)
 */
static void _demo_wakeuppad_cb(void *arg)
{
    liot_gpiolvl_e level = Liot_WakeupPadGetLevel(KEY2);
    liot_trace("wakeuppad_cb level %d", level);
}

/**
 * @brief Key demo task entry
 * @param argv Task parameter (unused)
 */
void demo_key_task(void *argv)
{
    /* Enable AON power domain and set voltage to 3.3V */
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_AON, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);

    Liot_SetPinFunc(KEY0, L_PIN_FUNC_0);
    Liot_SetPinFunc(KEY1, L_PIN_FUNC_0);
    liot_pinfunc_e pinfnc0 = Liot_GetPinFunc(KEY0);
    liot_pinfunc_e pinfnc1 = Liot_GetPinFunc(KEY1);
    liot_trace("check pinfunc: key0:%d(%d), key1:%d(%d)", KEY0, pinfnc0, KEY1, pinfnc1);

    /* Initialize GPIO input interrupts */
    liot_intcb_t _intcb0 = {
        .callback = _demo_gpioin_int_cb0,
        .arg = (void*)KEY0_GPIO,
        .signal = L_INT_EDGE_FALL,
    };
    liot_intcb_t _intcb1 = {
        .callback = _demo_gpioin_int_cb1,
        .arg = (void*)KEY1_GPIO,
        .signal = L_INT_EDGE_FALL,
    };
    Liot_GpioInit(KEY0_GPIO, L_IO_INPUT, L_IO_HIGH, &_intcb0);
    Liot_GpioInit(KEY1_GPIO, L_IO_INPUT, L_IO_HIGH, &_intcb1);
    Liot_GpioIntEnable();

    /* Initialize wakeup pad for KEY2 */
    liot_wakeup_cfg_t wakeup_cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL
    };
    Liot_WakeupIntInit(KEY2, wakeup_cfg, _demo_wakeuppad_cb, NULL);

    while(1)
    {
        liot_trace("key task ");
        liot_rtos_task_sleep_s(1);
    }
}
