/**  @file
 *  demo_gpio2.c
 *  @brief GPIO2 Function Demonstration Program
 *  
 *  This file demonstrates how to use the GPIO2 API interfaces, including:
 *  1. AON (Always-On) power domain control
 *  2. GPIO output configuration and control
 *  3. GPIO input interrupt configuration
 *  4. Wakeup pad interrupt configuration
 *  5. Periodic GPIO level switching
 */
#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"

// Wakeup pad number
#define DEMO_WAKEUP_PAD (L_WAKEUPAD_5)  // Use wakeup pad 5

// GPIO output pin address
#define GPIO_OUT_ADDR   (L_GPIO_27)
#define GPIO_OUT_MDMPIN (25)

// GPIO input pin address
#define GPIO_IN_ADDR    (L_GPIO_26)
#define GPIO_IN_MDMPIN  (16)

/**
 * @brief GPIO input interrupt callback function
 * 
 * This interrupt is triggered when a rising edge is detected on the GPIO_IN_ADDR pin
 * @param arg Interrupt callback parameter, GPIO_IN_ADDR is passed here
 */
static void _demo_gpioin_int_cb(void *arg)
{
    // Get the current level state of GPIO_OUT_ADDR
    liot_gpiolvl_e getlevel = Liot_GpioGetLevel(GPIO_OUT_ADDR);
    
    // Print interrupt information and GPIO_OUT_ADDR level state
    liot_trace("gpio(%d) interrupt trigger, get gpio(%d) level: %d", 
              (int)arg, GPIO_OUT_ADDR, getlevel);
}

/**
 * @brief Wakeup pad interrupt callback function
 * 
 * This interrupt is triggered when a rising edge is detected on the DEMO_WAKEUP_PAD pin
 * @param arg Interrupt callback parameter, not used here
 */
static void _demo_wakeuppad_cb(void *arg)
{
    // Print wakeup pad interrupt information
    liot_gpiolvl_e level = Liot_WakeupPadGetLevel(DEMO_WAKEUP_PAD);
    liot_trace("wakeuppad_cb level %d", level);
}

/**
 * @brief GPIO2 demonstration thread main function
 * @param argv Thread parameter, not used here
 */
void demo_gpio2_thread(void *argv)
{
    /* 1. Enable AON (Always-On) power domain and set voltage to 3.3V */
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_AON, L_VOLT_3_30V);

    Liot_SetPinFunc(GPIO_OUT_MDMPIN, L_PIN_FUNC_0);
    Liot_SetPinFunc(GPIO_IN_MDMPIN, L_PIN_FUNC_0);
    liot_pinfunc_e outpinfnc = Liot_GetPinFunc(GPIO_OUT_MDMPIN);
    liot_pinfunc_e inpinfnc = Liot_GetPinFunc(GPIO_IN_MDMPIN);
    liot_trace("check pinfunc: out:%d(%d), in %d(%d)", GPIO_OUT_MDMPIN, outpinfnc, GPIO_IN_MDMPIN, inpinfnc);

    /* 2. Initialize GPIO output pin */
    Liot_GpioInit(GPIO_OUT_ADDR, L_IO_OUTPUT, L_IO_HIGH, NULL);

    /* 3. Initialize GPIO input interrupt */
    liot_intcb_t _intcb = {              // Interrupt callback configuration structure
        .callback = _demo_gpioin_int_cb, // Set interrupt callback function
        .arg = (void*)GPIO_IN_ADDR,      // Set callback function parameter
        .signal = L_INT_EDGE_RISE,       // Set interrupt trigger mode: rising edge trigger
    };
    Liot_GpioIntEnable();
    Liot_GpioInit(GPIO_IN_ADDR, L_IO_INPUT, L_IO_LOW, &_intcb);

    /* 4. Initialize wakeup pad interrupt */
    liot_wakeup_cfg_t wakeup_cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL
    };
    Liot_WakeupIntInit(DEMO_WAKEUP_PAD, wakeup_cfg, _demo_wakeuppad_cb, NULL);

    /* 5. Enter main loop, periodically switch GPIO output level */
    while(1)
    {
        Liot_GpioSetLevel(GPIO_OUT_ADDR, L_IO_LOW);  // Set GPIO output to low level
        liot_rtos_task_sleep_ms(1000);                // Delay 1 second

        Liot_GpioSetLevel(GPIO_OUT_ADDR, L_IO_HIGH); // Set GPIO output to high level
        liot_rtos_task_sleep_ms(2000);                // Delay 2 seconds
    }

    // Thread ends
    Liot_GpioIntDisable();
    Liot_WakeupIntDeinit(DEMO_WAKEUP_PAD);
    liot_rtos_task_delete(NULL);
}