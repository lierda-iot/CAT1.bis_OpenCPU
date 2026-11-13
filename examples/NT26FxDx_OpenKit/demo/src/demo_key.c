/**  @file
 *  demo_key.c
 *  @brief Key input demonstration using GPIO interrupt and wakeup pad
 *
 *  This file demonstrates how to detect key presses via:
 *  1. AON (Always-On) power domain control
 *  2. GPIO input interrupt configuration (KEY0, KEY1)
 *  3. Wakeup pad interrupt configuration (KEY2)
 *  4. Message queue for key event processing
 */
#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define KEY0_GPIO   (L_GPIO_20)
#define KEY0        (5)

#define KEY1_GPIO   (L_GPIO_22)
#define KEY1        (19)

#define KEY2        (L_WAKEUPAD_0)

/* Message queue handle for key events */
liot_queue_t g_key_queue_handle = NULL;

/* Key event types */
typedef enum
{
    KEY_EVENT_KEY0,
    KEY_EVENT_KEY1,
    KEY_EVENT_KEY2,
    KEY_EVENT_MAX
} key_event_e;

/* Key event message structure */
typedef struct
{
    key_event_e key_type;
    liot_gpiolvl_e level;
} key_event_msg_t;


/**
 * @brief GPIO input interrupt callback for KEY0
 *
 * Triggered on falling edge of KEY0_GPIO pin.
 * Sends key event to message queue.
 * @param arg Callback parameter (KEY0_GPIO passed at init)
 */
static void _demo_gpioin_int_cb0(void *arg)
{
    key_event_msg_t key_msg;
    key_msg.key_type = KEY_EVENT_KEY0;
    key_msg.level = Liot_GpioGetLevel(KEY0_GPIO);

    liot_rtos_queue_release(g_key_queue_handle, sizeof(key_event_msg_t), (uint8 *)&key_msg, 0);
}

/**
 * @brief GPIO input interrupt callback for KEY1
 *
 * Triggered on falling edge of KEY1_GPIO pin.
 * Sends key event to message queue.
 * @param arg Callback parameter (KEY1_GPIO passed at init)
 */
static void _demo_gpioin_int_cb1(void *arg)
{
    key_event_msg_t key_msg;
    key_msg.key_type = KEY_EVENT_KEY1;
    key_msg.level = Liot_GpioGetLevel(KEY1_GPIO);

    liot_rtos_queue_release(g_key_queue_handle, sizeof(key_event_msg_t), (uint8 *)&key_msg, 0);
}

/**
 * @brief Wakeup pad interrupt callback for KEY2
 *
 * Triggered on falling edge of KEY2 wakeup pad.
 * Sends key event to message queue.
 * @param arg Callback parameter (unused)
 */
static void _demo_wakeuppad_cb(void *arg)
{
    key_event_msg_t key_msg;
    key_msg.key_type = KEY_EVENT_KEY2;
    key_msg.level = Liot_WakeupPadGetLevel(KEY2);

    liot_rtos_queue_release(g_key_queue_handle, sizeof(key_event_msg_t), (uint8 *)&key_msg, 0);
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

    /* Create message queue for key events */
    liot_rtos_queue_create(&g_key_queue_handle, sizeof(key_event_msg_t), 10);

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

    liot_trace("Key demo initialized, waiting for key events...");

    /* Main message queue loop */
    key_event_msg_t key_msg;
    liot_errcode_e ret = LIOT_SUCCESS;

    while(1)
    {
        /* Wait indefinitely for key events from queue */
        ret = liot_rtos_queue_wait(g_key_queue_handle, (uint8 *)&key_msg, sizeof(key_event_msg_t), LIOT_WAIT_FOREVER);

        if (ret != LIOT_SUCCESS)
        {
            continue;
        }

        /* Process received key event */
        switch (key_msg.key_type)
        {
            case KEY_EVENT_KEY0:
                liot_trace("KEY0 interrupt triggered, gpio(%d) level: %d", KEY0_GPIO, key_msg.level);
                break;
            case KEY_EVENT_KEY1:
                liot_trace("KEY1 interrupt triggered, gpio(%d) level: %d", KEY1_GPIO, key_msg.level);
                break;
            case KEY_EVENT_KEY2:
                liot_trace("KEY2 wakeup pad triggered, level: %d", key_msg.level);
                break;
            default:
                liot_trace("Unknown key event type: %d", key_msg.key_type);
                break;
        }
    }
}

