/**
 * @file liot_timer.h
 * @brief LIoT Platform Hardware Timer Interface
 * @details A liot-layer wrapper over the EC718 hardware timer (TIMER_* driver).
 *          Provides microsecond-level periodic/one-shot timers with callbacks,
 *          hiding low-level details such as clock config, XIC vector setup,
 *          interrupt flag clearing and match-value conversion.
 *
 * @warning [USAGE LIMIT - MUST READ]
 *          Timer expiry triggers the callback via interrupt; both callback time
 *          and interrupt entry/exit overhead count against every timer period.
 *          - PERIODIC mode: the actual period should NOT be below 400~500us.
 *            Too short a period (too frequent interrupts) or a callback that
 *            takes too long will keep the CPU occupied and may cause scheduling
 *            anomalies or even a system hang.
 *          - For shorter periods / higher frequencies (e.g. PWM waveforms), use
 *            the hardware PWM (liot_pwm / liot_apwm) which outputs directly in
 *            hardware without consuming CPU.
 *          - ONESHOT mode fires only once and has no frequency limit, but the
 *            callback must still be short.
 *
 * @version 1.0.0
 * @date 2026-07-16
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */
#ifndef LIOT_TIMER_H
#define LIOT_TIMER_H

/*===========================================================================
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "liot_api_common.h"

/*===========================================================================
 * macros
 ===========================================================================*/
/** Timer counting clock frequency (Hz); internally fixed to 26MHz source, div 1 */
#define LIOT_TIMER_CLOCK_HZ         (26000000U)
/** Counter ticks per microsecond */
#define LIOT_TIMER_TICKS_PER_US     (LIOT_TIMER_CLOCK_HZ / 1000000U)  /* 26 */
/** Max period (us): 32-bit counter / 26, about 165 seconds */
#define LIOT_TIMER_MAX_PERIOD_US    (0xFFFFFFFFU / LIOT_TIMER_TICKS_PER_US)

/*===========================================================================
 * types
 ===========================================================================*/
/**
 * @enum Liot_TimerId_e
 * @brief Hardware timer instance id.
 *
 * @warning Some instances are already used by the platform; watch for conflicts:
 *          - LIOT_TIMER_1 / LIOT_TIMER_2 : used by PWM audio, use with care
 *          - LIOT_TIMER_4               : used by SIM hot-swap jitter timer, use with care
 *          - LIOT_TIMER_5               : reserved (does not vote for sleep; counting
 *                                         stops on sleep), use with care
 *          Prefer LIOT_TIMER_0 / LIOT_TIMER_3.
 */
typedef enum
{
    LIOT_TIMER_0 = 0,   ///< free, recommended
    LIOT_TIMER_1,       ///< used by PWM audio, use with care
    LIOT_TIMER_2,       ///< used by PWM audio, use with care
    LIOT_TIMER_3,       ///< free, recommended
    LIOT_TIMER_4,       ///< used by SIM hot-swap, use with care
    LIOT_TIMER_5,       ///< reserved (no sleep vote), use with care
    LIOT_TIMER_MAX,
} Liot_TimerId_e;

/**
 * @enum Liot_TimerMode_e
 * @brief Timer running mode.
 */
typedef enum
{
    LIOT_TIMER_MODE_PERIODIC = 0,   ///< periodic: auto-reload on expiry, fires callback repeatedly
    LIOT_TIMER_MODE_ONESHOT,        ///< one-shot: fires callback once on expiry then auto-stops
} Liot_TimerMode_e;

/**
 * @typedef Liot_TimerCallback_f
 * @brief Timer expiry callback prototype.
 * @param arg user argument passed in at registration
 * @warning The callback runs in **interrupt (ISR) context**; it must be short,
 *          non-blocking, and must not call APIs that block or require task context.
 * @warning Callback time counts directly against every timer period. The longer
 *          the callback, the larger the minimum allowed period. If the callback
 *          time approaches or exceeds the period, interrupts keep occupying the
 *          CPU and may cause scheduling anomalies or a system hang. Always keep
 *          [callback time << period] with ample margin.
 */
typedef void (*Liot_TimerCallback_f)(void *arg);

/**
 * @struct Liot_TimerCfg_t
 * @brief Timer configuration parameters.
 */
typedef struct
{
    Liot_TimerId_e        id;         ///< timer instance, see Liot_TimerId_e
    Liot_TimerMode_e      mode;       ///< running mode, periodic/one-shot
    uint32_t              periodUs;   ///< period (us). Theoretical range 1 ~ LIOT_TIMER_MAX_PERIOD_US;
                                      ///< in periodic mode, due to interrupt overhead, not recommended below 400~500us (see notes above)
    Liot_TimerCallback_f  callback;   ///< expiry callback (ISR context); NULL means count only, no callback
    void                 *arg;        ///< user argument passed to the callback
} Liot_TimerCfg_t;

/*===========================================================================
 * APIs
 ===========================================================================*/
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Initialize and configure a hardware timer.
 * @details Performs clock config (26MHz/div1), driver init, period conversion,
 *          match0 interrupt enable and XIC vector binding. It does **not** start
 *          automatically; call Liot_TimerStart to begin counting.
 * @param cfg pointer to configuration, must not be NULL
 * @return LIOT_SUCCESS on success;
 *         LIOT_INVALID_PARAM_ERR on invalid params (cfg NULL / id out of range / periodUs out of range)
 */
liot_errcode_e Liot_TimerInit(const Liot_TimerCfg_t *cfg);

/**
 * @brief Start the timer counting.
 * @param id timer instance
 * @return LIOT_SUCCESS on success; LIOT_INVALID_PARAM_ERR if id out of range or not initialized
 */
liot_errcode_e Liot_TimerStart(Liot_TimerId_e id);

/**
 * @brief Stop the timer counting.
 * @param id timer instance
 * @return LIOT_SUCCESS on success; LIOT_INVALID_PARAM_ERR if id out of range or not initialized
 */
liot_errcode_e Liot_TimerStop(Liot_TimerId_e id);

/**
 * @brief Change the timer period at runtime.
 * @details Updates the match0 value. If the timer is running, the counter is
 *          **reset immediately** and re-times from now with the new period
 *          (instead of waiting for the current period to finish).
 * @param id       timer instance
 * @param periodUs new period (us), range 1 ~ LIOT_TIMER_MAX_PERIOD_US
 * @return LIOT_SUCCESS on success; LIOT_INVALID_PARAM_ERR on invalid params or not initialized
 */
liot_errcode_e Liot_TimerSetPeriod(Liot_TimerId_e id, uint32_t periodUs);

/**
 * @brief Read the timer's current elapsed time (us).
 * @note The counter increments at 26 tick/us; the return value is floored to
 *       microseconds, sub-microsecond part is dropped.
 * @param id timer instance
 * @return elapsed microseconds; 0 if id out of range or not initialized
 */
uint32_t Liot_TimerGetElapsedUs(Liot_TimerId_e id);

/**
 * @brief De-initialize the timer: stop, disable interrupt, unbind XIC vector, disable clock.
 * @param id timer instance
 * @return LIOT_SUCCESS on success; LIOT_INVALID_PARAM_ERR if id out of range
 */
liot_errcode_e Liot_TimerDeInit(Liot_TimerId_e id);

#if defined(__cplusplus)
}
#endif

#endif /* LIOT_TIMER_H */
