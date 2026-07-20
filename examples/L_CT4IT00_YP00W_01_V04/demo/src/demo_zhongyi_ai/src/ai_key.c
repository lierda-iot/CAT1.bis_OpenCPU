/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO20 (KEY_USER0) button handler with debounce.
 */

#include "ai_key.h"

#include "ai_app_log.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define AI_KEY_GPIO        L_GPIO_20
#define AI_KEY_MODEM_PIN   5
#define AI_KEY_DEBOUNCE_MS 50

static ai_key_callback_t s_on_press;
static volatile uint32_t s_last_isr_tick;

static void ai_key_isr(void *arg)
{
    uint32_t now = liot_rtos_get_system_tick();
    (void)arg;

    if ((now - s_last_isr_tick) < AI_KEY_DEBOUNCE_MS) {
        return;
    }
    s_last_isr_tick = now;

    if (s_on_press != NULL) {
        s_on_press();
    }
}

int ai_key_init(ai_key_callback_t on_press)
{
    liot_intcb_t intcb;
    liot_gpioerr_e ret;

    if (on_press == NULL) {
        return -1;
    }

    s_on_press = on_press;
    s_last_isr_tick = 0;

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_AON, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_SetPinFunc(AI_KEY_MODEM_PIN, L_PIN_FUNC_0);

    intcb.signal   = L_INT_EDGE_FALL;
    intcb.callback = ai_key_isr;
    intcb.arg      = NULL;

    ret = Liot_GpioInit(AI_KEY_GPIO, L_IO_INPUT, L_IO_HIGH, &intcb);
    if (ret != L_GPIO_ERR_SUCCESS) {
        liot_trace("ai_key gpio init failed ret=%d", (int)ret);
        return -2;
    }

    ret = Liot_GpioIntEnable();
    if (ret != L_GPIO_ERR_SUCCESS) {
        liot_trace("ai_key int enable failed ret=%d", (int)ret);
        return -3;
    }

    liot_trace("ai_key init ok GPIO20 level=%d", Liot_GpioGetLevel(AI_KEY_GPIO));
    return 0;
}
