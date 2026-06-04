/**
 * @file liot_pwm_demo.c
 * @brief LIoT PWM (Pulse Width Modulation) Demo Application
 * @date 2025-08-26
 * @version 1.0
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 */
 /**
 * This demo application demonstrates the usage of PWM (Pulse Width Modulation) peripherals
 * on EC7xx series chips. It initializes multiple PWM channels with different configurations
 * and demonstrates dynamic duty cycle adjustment on one channel.
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_pwm.h"

#define DEMO_PWM_1_PIN      (20)   /**< PWM1 channel GPIO pin for EC718 */
#define DEMO_PWM_1_FUCN     (5)    /**< PWM1 channel function selector for EC718 */
#define DEMO_PWM_2_PIN      (106)  /**< PWM2 channel GPIO pin for EC718 */
#define DEMO_PWM_2_FUCN     (5)    /**< PWM2 channel function selector for EC718 */
#define DEMO_PWM_3_PIN      (25)   /**< PWM3 channel GPIO pin for EC718 */
#define DEMO_PWM_3_FUCN     (5)    /**< PWM3 channel function selector for EC718 */

/** @} */ // end of PWM_DEMO_HARDWARE_CONFIG

/** @defgroup PWM_DEMO_PARAMETERS PWM Demo Parameters
 *  @brief Configuration parameters for PWM demonstration
 *  @{
 */
#define PWM_UPDATE_INTERVAL_MS 500   /**< Duty cycle update interval in milliseconds */
#define PWM_MAX_DUTY_CYCLE     2000  /**< Maximum duty cycle value for PWM2 */
#define PWM_DUTY_STEP          200   /**< Duty cycle increment step */
/** @} */ // end of PWM_DEMO_PARAMETERS

/**
 * @brief PWM demonstration thread
 * 
 * This thread initializes three PWM channels with different configurations:
 * - PWM1: 10kHz frequency
 * - PWM2: 500Hz frequency with dynamic duty cycle adjustment
 * - PWM3: 100Hz frequency
 * 
 * The thread continuously adjusts the duty cycle of PWM2 from 0 to max value
 * and back to 0 in steps, demonstrating dynamic PWM control.
 * 
 * @param[in] argv Thread argument (not used in this demo)
 */
void liot_pwm_demo_thread(void *argv)
{
    liot_pwm_cfg_s ptr;               /**< PWM configuration structure */
    uint16_t duty_cycle = 0;          /**< Current duty cycle value for PWM2 */

    // Power management: Enable AON (Always-On) domain power
    Liot_AonPowerCtl(true);

    // Configure GPIO pins for PWM functionality
    Liot_SetPinFunc(DEMO_PWM_1_PIN, DEMO_PWM_1_FUCN);
    Liot_SetPinFunc(DEMO_PWM_2_PIN, DEMO_PWM_2_FUCN);
    Liot_SetPinFunc(DEMO_PWM_3_PIN, DEMO_PWM_3_FUCN);

    /** @brief PWM1 configuration: 10kHz frequency
     *  Calculation: PWM frequency = (Clock frequency) / ((period + 1) * (prescaler + 1))
     *  With RC26M clock (26MHz), period=99, prescaler=25:
     *  Frequency = 26,000,000 / ((99 + 1) * (25 + 1)) = 26,000,000 / 2600 = 10,000Hz = 10kHz
     */
    ptr.clk_sel   = LIOT_CLK_RC26M;   /**< Select 26MHz RC oscillator as clock source */
    ptr.duty      = 10;               /**< Initial duty cycle (10/100 = 10%) */
    ptr.period    = 99;               /**< PWM period value */
    ptr.prescaler = 25;               /**< Clock prescaler value */
    liot_pwm_enable(LIOT_PWM_1, &ptr);/**< Apply configuration to PWM1 */
    liot_pwm_open(LIOT_PWM_1);        /**< Enable PWM1 output */

    /** @brief PWM2 configuration: 500Hz frequency
     *  With RC26M clock (26MHz), period=1999, prescaler=25:
     *  Frequency = 26,000,000 / ((1999 + 1) * (25 + 1)) = 26,000,000 / 52,000 = 500Hz
     */
    ptr.duty      = 500;              /**< Initial duty cycle (500/2000 = 25%) */
    ptr.period    = 1999;             /**< PWM period value */
    ptr.prescaler = 25;               /**< Clock prescaler value */
    liot_pwm_enable(LIOT_PWM_2, &ptr);/**< Apply configuration to PWM2 */
    liot_pwm_open(LIOT_PWM_2);        /**< Enable PWM2 output */

    /** @brief PWM3 configuration: 100Hz frequency
     *  With RC26M clock (26MHz), period=9999, prescaler=25:
     *  Frequency = 26,000,000 / ((9999 + 1) * (25 + 1)) = 26,000,000 / 260,000 = 100Hz
     */
    ptr.duty      = 2000;             /**< Initial duty cycle (2000/10000 = 20%) */
    ptr.period    = 9999;             /**< PWM period value */
    ptr.prescaler = 25;               /**< Clock prescaler value */
    liot_pwm_enable(LIOT_PWM_3, &ptr);/**< Apply configuration to PWM3 */
    liot_pwm_open(LIOT_PWM_3);        /**< Enable PWM3 output */

    // Initial delay to ensure stable PWM output
    liot_rtos_task_sleep_ms(1000);

    // Main loop: Dynamic duty cycle adjustment for PWM2
    while (1)
    {
        liot_trace("LIOT_PWM_2 duty cycle:%d/%d\r\n", duty_cycle, PWM_MAX_DUTY_CYCLE);
        liot_pwm_set_duty_cycle(LIOT_PWM_2, duty_cycle); /**< Update PWM2 duty cycle */
        liot_rtos_task_sleep_ms(PWM_UPDATE_INTERVAL_MS); /**< Wait for next update */
        duty_cycle += PWM_DUTY_STEP;                     /**< Increment duty cycle */

        // Reset duty cycle when exceeding maximum value
        if(duty_cycle > PWM_MAX_DUTY_CYCLE)
        {
            duty_cycle = 0;  /**< Reset to minimum duty cycle */
        }
    }
    liot_pwm_close(LIOT_PWM_1);
    liot_pwm_close(LIOT_PWM_2);
    liot_pwm_close(LIOT_PWM_3);
    
    liot_pwm_disable(LIOT_PWM_1);
    liot_pwm_disable(LIOT_PWM_2);
    liot_pwm_disable(LIOT_PWM_3);

    liot_rtos_task_delete(NULL); // kill itsel
}