/**
 * @file liot_pwm.h
 * @brief LIoT Platform PWM Controller Interface
 * @details This header file provides the application programming interface (API) for configuring
 *          and controlling Pulse Width Modulation (PWM) peripherals on LIoT-compatible devices.
 *          It includes definitions for PWM channels, configuration structures, error codes,
 *          and function prototypes for PWM initialization, control, and parameter adjustment.
 * @version 1.0.0
 * @date 2025-8-26
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 */
#ifndef LIOT_PWM_H
#define LIOT_PWM_H

/*===========================================================================
 * include files
 ===========================================================================*/
#include "liot_api_common.h"
#include "liot_type.h"

/**
 * @defgroup pwm_constants PWM Constants
 * @brief PWM controller constant definitions
 * @{
 */

/**
 * @brief Base value for PWM error codes
 * @details Error codes for PWM functions will range from LIOT_PWM_ERRCODE_BASE to
 *          (LIOT_PWM_ERRCODE_BASE + 0x0FFF)
 */
#define LIOT_PWM_ERRCODE_BASE (LIOT_COMPONENT_BSP_PWM << 16)

/** @} */ // end of pwm_constants

/**
 * @defgroup pwm_enums PWM Enumerations
 * @brief Enumerated types for PWM configuration and control
 * @{
 */

/**
 * @brief PWM operation error codes
 * @details These error codes are returned by PWM API functions to indicate success or specific failures
 */
typedef enum
{
    LIOT_PWM_SUCCESS = LIOT_SUCCESS,               /**< Operation completed successfully */
    LIOT_PWM_EXECUTE_ERR = 1 | LIOT_PWM_ERRCODE_BASE, /**< Generic execution error */
    LIOT_PWM_INVALID_PARAM_ERR,                     /**< Invalid input parameter */
    LIOT_PWM_FUNC_SET_ERR,                          /**< Failed to set PWM function */
    LIOT_PWM_ACQUIRE_ERR,                           /**< Failed to acquire PWM resource */
    LIOT_PWM_START_ERR,                             /**< Failed to start PWM output */
    LIOT_PWM_STOP_ERR,                              /**< Failed to stop PWM output */
    LIOT_PWM_REPEAT_OPEN_ERR,                       /**< Attempted to open an already open PWM channel */
    LIOT_PWM_REPEAT_CLOSE_ERR                       /**< Attempted to close an already closed PWM channel */
} liot_pwm_errcode_e;

/**
 * @brief PWM channel selection
 * @details Identifies the available PWM output channels on the device
 * @note Channel availability may vary between different hardware platforms
 */
typedef enum
{
    LIOT_PWM_0,          /**< PWM channel 0 */
    LIOT_PWM_1,          /**< PWM channel 1 */
    LIOT_PWM_2,          /**< PWM channel 2 */
    LIOT_PWM_3,          /**< PWM channel 3 */
    LIOT_PWM_4,          /**< PWM channel 4 */
    LIOT_PWM_5,          /**< PWM channel 5 */
    LIOT_PWM_MAX         /**< Total number of PWM channels (not a valid channel) */
} liot_pwm_sel_e;

/**
 * @brief PWM clock source selection
 * @details Specifies the available clock sources for PWM generation
 */
typedef enum
{
    LIOT_CLK_RC26M,      /**< 26MHz RC oscillator clock source */
    LIOT_CLK_RTC_40K     /**< 40KHz RTC clock source */
} liot_pwm_clk_e;

/** @} */ // end of pwm_enums

/**
 * @defgroup pwm_structs PWM Structures
 * @brief Data structures for PWM configuration
 * @{
 */

/**
 * @brief PWM configuration parameters
 * @details Contains all necessary parameters to configure PWM output characteristics
 */
typedef struct
{
    liot_pwm_clk_e clk_sel;    /**< Clock source selection */
    uint16_t prescaler;        /**< Clock prescaler value (1-255) */
    uint16_t period;           /**< PWM period value (counter reload value) */
    uint16_t duty;             /**< PWM duty cycle value (compare value) */
} liot_pwm_cfg_s;

/** @} */ // end of pwm_structs

/**
 * @defgroup pwm_functions PWM Functions
 * @brief PWM controller API functions
 * @{
 */

/**
 * @brief Opens and initializes a PWM channel
 * @details Configures the specified PWM channel and prepares it for operation
 * @param[in] pwm_sel PWM channel to open (0 to LIOT_PWM_MAX-1)
 * @return Operation status
 * @retval LIOT_PWM_SUCCESS Success
 * @retval LIOT_PWM_INVALID_PARAM_ERR Invalid channel selection
 * @retval LIOT_PWM_REPEAT_OPEN_ERR Channel already open
 * @note This function must be called before any other PWM functions for a given channel
 */
liot_pwm_errcode_e liot_pwm_open(liot_pwm_sel_e pwm_sel);

/**
 * @brief Closes a PWM channel
 * @details Disables the specified PWM channel and releases associated resources
 * @param[in] pwm_sel PWM channel to close (0 to LIOT_PWM_MAX-1)
 * @return Operation status
 * @retval LIOT_PWM_SUCCESS Success
 * @retval LIOT_PWM_INVALID_PARAM_ERR Invalid channel selection
 * @retval LIOT_PWM_REPEAT_CLOSE_ERR Channel already closed
 */
liot_pwm_errcode_e liot_pwm_close(liot_pwm_sel_e pwm_sel);

/**
 * @brief Enables PWM output with specified configuration
 * @details Starts PWM generation on the specified channel with the given parameters
 * @param[in] pwm_sel PWM channel to configure (0 to LIOT_PWM_MAX-1)
 * @param[in] pwm_cfg Pointer to PWM configuration structure
 * @return Operation status
 * @retval LIOT_PWM_SUCCESS Success
 * @retval LIOT_PWM_INVALID_PARAM_ERR Invalid parameter (NULL pointer or invalid values)
 * @retval LIOT_PWM_START_ERR Failed to start PWM output
 * @note The PWM channel must be opened with liot_pwm_open() before calling this function
 * @note prescaler range: 1-255, period and duty range: 0-65535
 */
liot_pwm_errcode_e liot_pwm_enable(liot_pwm_sel_e pwm_sel, liot_pwm_cfg_s *pwm_cfg);

/**
 * @brief Disables PWM output
 * @details Stops PWM generation on the specified channel while maintaining configuration
 * @param[in] pwm_sel PWM channel to disable (0 to LIOT_PWM_MAX-1)
 * @return Operation status
 * @retval LIOT_PWM_SUCCESS Success
 * @retval LIOT_PWM_INVALID_PARAM_ERR Invalid channel selection
 * @retval LIOT_PWM_STOP_ERR Failed to stop PWM output
 */
liot_pwm_errcode_e liot_pwm_disable(liot_pwm_sel_e pwm_sel);

/**
 * @brief Sets PWM duty cycle
 * @details Updates the duty cycle of an already enabled PWM channel
 * @param[in] pwm_sel PWM channel to update (0 to LIOT_PWM_MAX-1)
 * @param[in] duty_cycle New duty cycle value (compare value)
 * @return Operation status
 * @retval LIOT_PWM_SUCCESS Success
 * @retval LIOT_PWM_INVALID_PARAM_ERR Invalid channel or duty cycle value
 * @retval LIOT_PWM_FUNC_SET_ERR Failed to update duty cycle
 * @note The PWM channel must be enabled with liot_pwm_enable() before calling this function
 * @note duty_cycle must be less than the configured period value
 */
liot_pwm_errcode_e liot_pwm_set_duty_cycle(liot_pwm_sel_e pwm_sel, uint32_t duty_cycle);

/** @} */ // end of pwm_functions

#endif /* LIOT_PWM_H */