/**
 * @brief Key input demo for tgai application (LSDK port)
 *
 * Ported from PLAT liot_tgai_demo/hardware/src/key.c.
 * Uses liot_gpio2 API (Liot_WakeupIntInit / Liot_WakeupPadGetLevel)
 * instead of the PLAT-only liot_gpio API.
 *
 * Button wiring: WAKEUP5, active-low, pull-up.
 * Events:
 *   Single click  -> tgai_key_single_click_cb()
 *   Double click  -> tgai_key_double_click_cb()
 *   Triple click  -> tgai_key_triple_click_cb()
 *   Long press    -> tgai_key_long_press_cb()
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_log.h"

/* ------------------------------------------------------------------ */
/* Timing parameters (ms)                                              */
/* ------------------------------------------------------------------ */
#define DEBOUNCE_TIME     20
#define CLICK_INTERVAL    300
#define LONG_PRESS_TIME   1000

/* ------------------------------------------------------------------ */
/* Key state machine                                                   */
/* ------------------------------------------------------------------ */
typedef enum {
    KEY_IDLE = 0,
    KEY_PRESSED,
    KEY_RELEASED
} key_state_t;

typedef enum {
    EVENT_NONE = 0,
    EVENT_SINGLE_CLICK,
    EVENT_DOUBLE_CLICK,
    EVENT_TRIPLE_CLICK,
    EVENT_MULTI_CLICK,
    EVENT_LONG_PRESS
} key_event_t;

static struct {
    key_state_t current_state;
    uint32_t    press_start_time;
    uint32_t    last_release_time;
    uint32_t    click_count;
    uint32_t    last_click_time;
    bool        long_press_detected;
} s_key = {
    .current_state      = KEY_IDLE,
    .press_start_time   = 0,
    .last_release_time  = 0,
    .click_count        = 0,
    .last_click_time    = 0,
    .long_press_detected = false
};

static liot_queue_t s_key_queue = NULL;
static liot_task_t        s_key_task  = NULL;

/* ------------------------------------------------------------------ */
/* Weak callbacks — override in application code                       */
/* ------------------------------------------------------------------ */
__attribute__((weak)) void tgai_key_single_click_cb(void) {}
__attribute__((weak)) void tgai_key_double_click_cb(void)  {}
__attribute__((weak)) void tgai_key_triple_click_cb(void)  {}
__attribute__((weak)) void tgai_key_long_press_cb(void)    {}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static void handle_key_event(key_event_t event)
{
    switch (event) {
        case EVENT_SINGLE_CLICK:
            liot_trace("key: single click");
            tgai_key_single_click_cb();
            break;
        case EVENT_DOUBLE_CLICK:
            liot_trace("key: double click");
            tgai_key_double_click_cb();
            break;
        case EVENT_TRIPLE_CLICK:
            liot_trace("key: triple click");
            tgai_key_triple_click_cb();
            break;
        case EVENT_LONG_PRESS:
            liot_trace("key: long press");
            tgai_key_long_press_cb();
            break;
        default:
            break;
    }
}

static bool is_key_released(void)
{
    /* Active-low: HIGH means released */
    return (Liot_WakeupPadGetLevel(L_WAKEUPAD_5) == L_IO_HIGH);
}

static uint32_t wait_for_key_release(uint32_t start_time, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        if (is_key_released()) {
            return liot_rtos_get_running_time() - start_time;
        }
        liot_rtos_task_sleep_ms(10);
        elapsed = liot_rtos_get_running_time() - start_time;

        if (elapsed >= LONG_PRESS_TIME && !s_key.long_press_detected) {
            s_key.long_press_detected = true;
            handle_key_event(EVENT_LONG_PRESS);
        }
    }
    return elapsed;
}

static void process_key_event(void)
{
    uint32_t now = liot_rtos_get_running_time();

    switch (s_key.current_state) {
        case KEY_IDLE:
            s_key.current_state      = KEY_PRESSED;
            s_key.press_start_time   = now;
            s_key.long_press_detected = false;
            break;
        case KEY_RELEASED:
            s_key.current_state      = KEY_PRESSED;
            s_key.press_start_time   = now;
            s_key.long_press_detected = false;
            break;
        default:
            break;
    }
}

static void check_click_timeout(void)
{
    if (s_key.current_state != KEY_RELEASED) return;

    uint32_t elapsed = liot_rtos_get_running_time() - s_key.last_release_time;
    if (elapsed < CLICK_INTERVAL) return;

    key_event_t event = EVENT_NONE;
    switch (s_key.click_count) {
        case 1:  event = EVENT_SINGLE_CLICK; break;
        case 2:  event = EVENT_DOUBLE_CLICK; break;
        case 3:  event = EVENT_TRIPLE_CLICK; break;
        default:
            if (s_key.click_count > 3) event = EVENT_MULTI_CLICK;
            break;
    }
    if (event != EVENT_NONE) handle_key_event(event);

    s_key.current_state = KEY_IDLE;
    s_key.click_count   = 0;
}

/* ------------------------------------------------------------------ */
/* ISR callback                                                        */
/* ------------------------------------------------------------------ */
static void wakeup5_isr_cb(void *arg)
{
    uint8_t val = 1;
    //liot_trace("key: cb");
    liot_rtos_queue_release_isr(s_key_queue, sizeof(val), &val);
}

/* ------------------------------------------------------------------ */
/* Key task                                                            */
/* ------------------------------------------------------------------ */
static void tgai_key_task(void *argv)
{
    liot_rtos_queue_create(&s_key_queue, sizeof(uint8_t), 10);
    if (s_key_queue == NULL) {
        liot_trace("key queue create failed");
        return;
    }

    liot_wakeup_cfg_t wakeup_cfg = {
        .wakeup_pull = LIOT_FORCE_PULL_UP,
        .wakeup_edge = L_INT_EDGE_FALL
    };
    Liot_WakeupIntInit(L_WAKEUPAD_5, wakeup_cfg, wakeup5_isr_cb, NULL);

    uint8_t  val       = 0;
    uint32_t wait_time = LIOT_WAIT_FOREVER;

    while (1) {
        LiotOSStatus_t ret = liot_rtos_queue_wait(s_key_queue, &val,
                                                  sizeof(val), wait_time);

        if (ret == LIOT_OSI_SUCCESS) {
            liot_rtos_task_sleep_ms(DEBOUNCE_TIME);

            if (!is_key_released()) {
                process_key_event();

                uint32_t dur = wait_for_key_release(s_key.press_start_time,
                                                    LONG_PRESS_TIME + 500);
                if (dur < LONG_PRESS_TIME && !s_key.long_press_detected) {
                    s_key.current_state      = KEY_RELEASED;
                    s_key.last_release_time  = liot_rtos_get_running_time();
                    s_key.click_count++;
                    s_key.last_click_time    = s_key.last_release_time;
                    wait_time = CLICK_INTERVAL / 2;
                } else if (s_key.long_press_detected) {
                    s_key.current_state = KEY_IDLE;
                    s_key.click_count   = 0;
                    wait_time = LIOT_WAIT_FOREVER;
                }
            }
        } else {
            /* timeout: check multi-click */
            if (wait_time != LIOT_WAIT_FOREVER) {
                check_click_timeout();
                if (s_key.current_state == KEY_IDLE) {
                    wait_time = LIOT_WAIT_FOREVER;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void demo_tgai_key_init(void)
{
    liot_rtos_task_create(&s_key_task, 1024 * 8, 14,
                          "tgai_key_task", tgai_key_task, NULL);
}

void demo_tgai_key_deinit(void)
{
    Liot_WakeupIntDeinit(L_WAKEUPAD_5);
    if (s_key_task) {
        liot_rtos_task_delete(s_key_task);
        s_key_task = NULL;
    }
}
