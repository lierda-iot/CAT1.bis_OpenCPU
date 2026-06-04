/**
 * @file battery.c
 * @brief Battery management driver implementation
 * @details Uses periodic ADC sampling for battery voltage, built-in hysteresis state
 *          machine for level estimation, and GPIO/USB wakeup interrupt for charge detection.
 *
 *          How it works:
 *          - Creates internal task, waits on message queue with timeout (periodic sampling)
 *          - Accumulates sample_count readings then averages for current voltage
 *          - Battery level based on percentage thresholds with hysteresis
 *          - Charge status detected via USB wakeup interrupt, processed in task context
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#include "battery.h"
#include "liot_adc.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include <string.h>

/* ---------- Constants ---------- */

#define BATTERY_LEVEL_NUM     5   /* Total number of battery levels */
#define BATTERY_SAMPLE_MAX    20  /* Maximum sample count */
#define BATTERY_HYSTERESIS    3   /* Hysteresis offset (percent) */

/* Level percentage thresholds: FULL>=80%, HIGH>=60%, MID>=40%, LOW>=20%, EMPTY<20% */
static uint8_t battery_level_thresh[BATTERY_LEVEL_NUM] = {80, 60, 40, 20, 0};

/* ---------- Internal message definitions ---------- */

/** @brief Task message types */
typedef enum {
    BATTERY_MSG_SAMPLE = 0,  ///< Periodic sample timeout
    BATTERY_MSG_CHG_EVENT,   ///< USB charge status change
    BATTERY_MSG_EXIT,        ///< Task exit request
} battery_msg_e;

/* ---------- Internal data structures ---------- */

/** @brief Battery device control block */
typedef struct {
    battery_config_t     config;      ///< User config copy
    int                  voltage_mv;  ///< Current voltage (mV)
    uint8_t              percent;     ///< Current battery percentage
    battery_level_e      level;       ///< Current battery level
    battery_chg_status_e chg_status;  ///< Current charge status
    uint8_t              usb_inserted;///< USB connected flag
    liot_task_t          task_ref;    ///< Sample task handle
    liot_queue_t         msg_queue;   ///< Message queue handle
    battery_event_cb_t   event_cb;    ///< Event callback function
    uint8_t              initialized; ///< Initialization flag
} battery_dev_t;

/* Global singleton */
static battery_dev_t g_battery = {0};

/* ======================== Internal functions ======================== */

/**
 * @brief Calculate battery percentage from voltage (linear mapping)
 */
static uint8_t battery_calc_percent(battery_dev_t *dev)
{
    if (dev->voltage_mv >= dev->config.voltage_max)
        return 100;
    if (dev->voltage_mv <= dev->config.voltage_min)
        return 0;

    dev->percent = (uint8_t)((dev->voltage_mv - dev->config.voltage_min) * 100
                     / (dev->config.voltage_max - dev->config.voltage_min));
    return dev->percent;
}

/**
 * @brief Hysteresis state machine: determine new level from percent and current level
 * @note Upward transition requires exceeding previous threshold + hysteresis;
 *       downward only needs to fall below current threshold
 */
static battery_level_e battery_calc_level(uint8_t percent, battery_level_e current_level)
{
    /* First call or out-of-range: assign directly by threshold */
    if (current_level >= BATTERY_LEVEL_NUM) {
        for (int i = 0; i < BATTERY_LEVEL_NUM; i++) {
            if (percent >= battery_level_thresh[i])
                return (battery_level_e)i;
        }
        return BATTERY_LEVEL_EMPTY;
    }

    /* Upward transition: percent exceeds previous level threshold + hysteresis */
    if (current_level > BATTERY_LEVEL_FULL &&
        percent >= battery_level_thresh[current_level - 1] + BATTERY_HYSTERESIS)
        return (battery_level_e)(current_level - 1);

    /* Downward transition: percent falls below current level threshold */
    if (current_level < BATTERY_LEVEL_EMPTY &&
        percent < battery_level_thresh[current_level])
        return (battery_level_e)(current_level + 1);

    return current_level;
}

/**
 * @brief Handle charge status change (called from task context)
 */
static void battery_handle_chg_event(battery_dev_t *dev)
{
    battery_chg_status_e new_status;

    liot_gpiolvl_e usb_level = Liot_WakeupPadGetLevel((liot_wakeuppad_e)dev->config.usb_wakeup_id);
    dev->usb_inserted = (usb_level == L_IO_HIGH) ? 1 : 0;

    if (usb_level == L_IO_HIGH) {
        liot_gpiolvl_e chg_val = Liot_GpioGetLevel((liot_gpio_e)dev->config.chg_state_gpio);
        new_status = (chg_val == L_IO_LOW) ? BATTERY_CHG_CHARGING : BATTERY_CHG_DONE;
    } else {
        new_status = BATTERY_CHG_NONE;
    }

    if (new_status != dev->chg_status) {
        battery_chg_status_e old_status = dev->chg_status;
        dev->chg_status = new_status;
        if (dev->event_cb) {
            if (new_status == BATTERY_CHG_NONE)
                dev->event_cb(BATTERY_EVENT_CHG_REMOVE, dev->voltage_mv, dev->percent);
            else if (new_status == BATTERY_CHG_DONE)
                dev->event_cb(BATTERY_EVENT_CHG_DONE, dev->voltage_mv, dev->percent);
            else if (old_status == BATTERY_CHG_NONE)
                dev->event_cb(BATTERY_EVENT_CHG_INSERT, dev->voltage_mv, dev->percent);
        }
    }
}

/**
 * @brief Perform one ADC sample, return averaged voltage after sample_count readings
 * @note Returns previous voltage value if sample count not yet reached
 */
static int battery_do_sample(battery_dev_t *dev)
{
    static int sample_buf[BATTERY_SAMPLE_MAX];
    static uint32_t sample_idx = 0;
    uint8_t count = (dev->config.sample_count > BATTERY_SAMPLE_MAX)
                    ? BATTERY_SAMPLE_MAX : dev->config.sample_count;
    int vbat = 0;

    liot_adc_get_volt((liot_adc_chan_id_e)dev->config.adc_channel, &vbat);

    sample_buf[sample_idx] = vbat;
    sample_idx++;

    if (sample_idx >= count) {
        int sum = 0;
        for (uint8_t i = 0; i < count; i++)
            sum += sample_buf[i];
        sample_idx = 0;
        dev->voltage_mv = sum / count;
    }

    return dev->voltage_mv;
}

/**
 * @brief Task main loop: queue timeout triggers sampling, messages trigger event handling
 */
static void battery_sample_task(void *arg)
{
    battery_dev_t *dev = &g_battery;
    battery_msg_e msg = {0};
    dev->initialized = 1;

    while (1) {
        LiotOSStatus_t ret = liot_rtos_queue_wait(dev->msg_queue,
                                (uint8 *)&msg, sizeof(msg),
                                dev->config.sample_interval_ms);

        switch (ret == LIOT_OSI_SUCCESS ? msg : BATTERY_MSG_SAMPLE) {
            case BATTERY_MSG_EXIT:
                goto task_exit;

            case BATTERY_MSG_CHG_EVENT:
                battery_handle_chg_event(dev);
                break;

            case BATTERY_MSG_SAMPLE:
            default:
                break;
        }

        battery_do_sample(dev);
        battery_calc_percent(dev);
    
        battery_level_e new_level = battery_calc_level(dev->percent, dev->level);
        if (new_level != dev->level) {
            dev->level = new_level;
            if (dev->event_cb)
                dev->event_cb(BATTERY_EVENT_LEVEL_CHANGE, dev->voltage_mv, dev->percent);
        }
    }

task_exit:
    liot_rtos_task_delete(NULL);
}

/**
 * @brief USB wakeup interrupt callback (only posts message to task)
 */
static void battery_usb_wakeup_cb(void *arg)
{
    battery_dev_t *dev = &g_battery;
    battery_msg_e msg = BATTERY_MSG_CHG_EVENT;
    liot_rtos_queue_release_isr(dev->msg_queue, sizeof(msg), (uint8 *)&msg);
}

/* ======================== Public API ======================== */

/**
 * @brief Initialize battery management driver
 */
int battery_init(const battery_config_t *config)
{
    if (!config || config->sample_count == 0 || config->sample_count > BATTERY_SAMPLE_MAX
        || config->voltage_max <= config->voltage_min)
        return -1;

    battery_dev_t *dev = &g_battery;
    memset(dev, 0, sizeof(battery_dev_t));
    dev->config = *config;
    dev->level = BATTERY_LEVEL_EMPTY;

    /* Create message queue */
    liot_rtos_queue_create(&dev->msg_queue, sizeof(battery_msg_e), 4);

    /* Initialize charge state GPIO */
    Liot_GpioInit((liot_gpio_e)config->chg_state_gpio, L_IO_INPUT, L_IO_LOW, NULL);

    /* Register USB wakeup interrupt */
    liot_wakeup_cfg_t wk_cfg;
    wk_cfg.wakeup_edge = L_INT_EDGE_BOTH;
    wk_cfg.wakeup_pull = LIOT_FORCE_PULL_DOWN;
    Liot_WakeupIntInit((liot_wakeuppad_e)config->usb_wakeup_id, wk_cfg, battery_usb_wakeup_cb, NULL);

    /* Save callback */
    dev->event_cb = config->event_cb;

    /* Detect initial charge status */
    liot_gpiolvl_e usb_level = Liot_WakeupPadGetLevel((liot_wakeuppad_e)config->usb_wakeup_id);
    dev->usb_inserted = (usb_level == L_IO_HIGH) ? 1 : 0;
    if (usb_level == L_IO_HIGH) {
        liot_gpiolvl_e chg_val = Liot_GpioGetLevel((liot_gpio_e)config->chg_state_gpio);
        dev->chg_status = (chg_val == L_IO_LOW) ? BATTERY_CHG_CHARGING : BATTERY_CHG_DONE;
    }

    /* Create sampling task */
    liot_rtos_task_create(&dev->task_ref, 1024, APP_PRIORITY_LOW,
                          "battery_task", battery_sample_task, NULL);

    return 0;
}

void battery_deinit(void)
{
    battery_dev_t *dev = &g_battery;
    if (!dev->initialized)
        return;

    dev->initialized = 0;

    /* Send exit message to wake task and make it exit */
    battery_msg_e msg = BATTERY_MSG_EXIT;
    liot_rtos_queue_release(dev->msg_queue, sizeof(msg), (uint8 *)&msg, LIOT_NO_WAIT);

    /* Unregister USB wakeup interrupt */
    Liot_WakeupIntDeinit((liot_wakeuppad_e)dev->config.usb_wakeup_id);

    /* Delete message queue */
    liot_rtos_queue_delete(dev->msg_queue);

    dev->msg_queue = NULL;
    dev->task_ref = NULL;
    dev->event_cb = NULL;
}

int battery_get_voltage(void)
{
    if (!g_battery.initialized)
        return -1;
    return g_battery.voltage_mv;
}

uint8_t battery_get_percent(void)
{
    if (!g_battery.initialized)
        return 0;
    return g_battery.percent;
}

battery_level_e battery_get_level(void)
{
    if (!g_battery.initialized)
        return BATTERY_LEVEL_EMPTY;
    return g_battery.level;
}

battery_chg_status_e battery_get_chg_status(void)
{
    if (!g_battery.initialized)
        return BATTERY_CHG_NONE;
    return g_battery.chg_status;
}
