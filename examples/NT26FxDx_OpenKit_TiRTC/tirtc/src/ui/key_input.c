/*
 * SPDX-FileCopyrightText: 2025 Lierda Technology Co., Ltd.
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file key_input.c
 * @brief Board key driver with ISR debounce and queued UI events.
 *
 * KEY0=NEXT, KEY1=CONFIRM, KEY2=BACK.  RGB control is intentionally absent.
 * Interrupt callbacks never draw, allocate, log or touch audio.
 */

#include <stdbool.h>
#include <stdint.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "ui_controller.h"

#define KEY0_GPIO              L_GPIO_20
#define KEY0_MODEM_PIN         5
#define KEY1_GPIO              L_GPIO_22
#define KEY1_MODEM_PIN         19
#define KEY2_WAKEUP_PAD        L_WAKEUPAD_0

#define KEY_COUNT              3U
#define KEY_DEBOUNCE_MS        80U

static volatile uint32_t s_key_last_tick[KEY_COUNT];

static bool key_debounce_accept(uint8_t key_index)
{
    uint32_t now;

    if (key_index >= KEY_COUNT)
    {
        return false;
    }

    now = liot_rtos_get_system_tick();
    if ((now - s_key_last_tick[key_index]) < KEY_DEBOUNCE_MS)
    {
        return false;
    }

    s_key_last_tick[key_index] = now;
    return true;
}

static void key0_isr(void *arg)
{
    (void)arg;
    if (key_debounce_accept(0U))
    {
        demo_ui_post_key_from_isr(DEMO_UI_KEY_NEXT);
    }
}

static void key1_isr(void *arg)
{
    (void)arg;
    if (key_debounce_accept(1U))
    {
        demo_ui_post_key_from_isr(DEMO_UI_KEY_CONFIRM);
    }
}

static void key2_isr(void *arg)
{
    (void)arg;
    if (key_debounce_accept(2U))
    {
        demo_ui_post_key_from_isr(DEMO_UI_KEY_BACK);
    }
}

static int key_hardware_init(void)
{
    liot_intcb_t key0_intcb = {
        .callback = key0_isr,
        .arg = NULL,
        .signal = L_INT_EDGE_FALL,
    };
    liot_intcb_t key1_intcb = {
        .callback = key1_isr,
        .arg = NULL,
        .signal = L_INT_EDGE_FALL,
    };
    liot_wakeup_cfg_t key2_cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL,
    };
    liot_gpioerr_e ret;

    ret = Liot_AonPowerCtl(true);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -1;
    }
    ret = Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -2;
    }
    ret = Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -3;
    }

    ret = Liot_SetPinFunc(KEY0_MODEM_PIN, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -4;
    }
    ret = Liot_SetPinFunc(KEY1_MODEM_PIN, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -5;
    }

    ret = Liot_GpioInit(KEY0_GPIO, L_IO_INPUT, L_IO_HIGH, &key0_intcb);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -6;
    }

    ret = Liot_GpioInit(KEY1_GPIO, L_IO_INPUT, L_IO_HIGH, &key1_intcb);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -7;
    }

    ret = Liot_GpioIntEnable();
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -8;
    }

    ret = Liot_WakeupIntInit(KEY2_WAKEUP_PAD, key2_cfg, key2_isr, NULL);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        return -9;
    }
    return 0;
}

void demo_key_task(void *argv)
{
    int ret;
    (void)argv;

    ret = key_hardware_init();
    liot_trace("[KEY] init ret=%d; KEY0=NEXT KEY1=OK KEY2=BACK debounce=%ums\r\n",
               ret, (unsigned int)KEY_DEBOUNCE_MS);

    /*
     * GPIO/wakeup callbacks remain registered after this one-shot init task
     * exits, so its stack is returned for TiRTC use.
     */
    liot_rtos_task_delete(NULL);
}
