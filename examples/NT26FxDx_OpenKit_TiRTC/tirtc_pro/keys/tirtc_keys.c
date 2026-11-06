/*
 * Pin mapping matches official V04 demo/src/demo_key.c and the reference
 * xiaotai-lierda tirtc/src/ui/key_input.c: KEY0 GPIO20/pin5, KEY1 GPIO22/pin19,
 * both function 0 and active low. This LCD port already enables the AON rail.
 * Do not reconfigure GPIO25/shared power, KEY2/wakeup, PWRKEY or global IRQs.
 */
#include "tirtc_keys.h"
#include <stdbool.h>
#include "liot_gpio2.h"
#include "liot_log.h"

#define KEY_DEBOUNCE_MS 80U
#define KEY_REPEAT_START_MS 600U
#define KEY_REPEAT_MS 200U
#define KEY_INVALID 255U

static bool s_initialized, s_armed;
static uint8_t s_candidate = KEY_INVALID, s_stable = KEY_INVALID, s_active;
static uint32_t s_candidate_since, s_repeat_due;

static void reset_input(void)
{
    s_armed = false;
    s_candidate = s_stable = KEY_INVALID;
    s_active = 0;
    s_candidate_since = s_repeat_due = 0;
}

int tirtc_keys_init(void)
{
    liot_gpioerr_e result;
    int stage = 1;
    if (s_initialized) return 0;
    reset_input();
    result = Liot_SetPinFunc(5, L_PIN_FUNC_0);
    if (result) goto fail;
    stage = 2;
    result = Liot_SetPinFunc(19, L_PIN_FUNC_0);
    if (result) goto fail;
    stage = 3;
    result = Liot_GpioInit(L_GPIO_20, L_IO_INPUT, L_IO_HIGH, NULL);
    if (result) goto fail;
    stage = 4;
    result = Liot_GpioInit(L_GPIO_22, L_IO_INPUT, L_IO_HIGH, NULL);
    if (result) goto fail;
    s_initialized = true;
    liot_trace("[KEY] ready KEY0=volume- GPIO20/pin5 KEY1=volume+ GPIO22/pin19 debounce=80 repeat=600/200ms\r\n");
    return 0;
fail:
    liot_trace("[KEY] init failed stage=%d ret=%d\r\n", stage, result);
    return -stage;
}

int tirtc_keys_poll(uint32_t now_ms)
{
    liot_gpiolvl_e key0, key1;
    uint8_t raw;
    if (!s_initialized) return 0;
    key0 = Liot_GpioGetLevel(L_GPIO_20);
    key1 = Liot_GpioGetLevel(L_GPIO_22);
    if ((key0 != L_IO_LOW && key0 != L_IO_HIGH) ||
        (key1 != L_IO_LOW && key1 != L_IO_HIGH)) {
        reset_input();
        return 0;
    }
    raw = (uint8_t)((key0 == L_IO_LOW ? 1U : 0U) |
                    (key1 == L_IO_LOW ? 2U : 0U));
    /* Immediately stop repeat on a chord/rollover. Rearm only after both
     * keys have been released for a complete debounce interval. */
    if (raw == 3U || (s_active && raw && raw != s_active)) {
        s_armed = false;
        s_active = 0;
    }
    if (raw != s_candidate) {
        s_candidate = raw;
        s_candidate_since = now_ms;
        return 0;
    }
    if ((uint32_t)(now_ms - s_candidate_since) < KEY_DEBOUNCE_MS) return 0;
    if (s_stable != raw) {
        s_stable = raw;
        if (!raw) {
            s_armed = true;
            s_active = 0;
        } else if (s_armed && raw != 3U) {
            s_active = raw;
            s_repeat_due = now_ms + KEY_REPEAT_START_MS;
            return raw == 1U ? -1 : 1;
        }
        return 0;
    }
    if (s_armed && s_active && raw == s_active &&
        (int32_t)(now_ms - s_repeat_due) >= 0) {
        /* A late caller never gets a burst of missed repeats. */
        s_repeat_due = now_ms + KEY_REPEAT_MS;
        return raw == 1U ? -1 : 1;
    }
    return 0;
}
