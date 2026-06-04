/**
 * @file liot_apwm.h
 * @brief LIoT Platform APWM Controller Interface
 * @details 
 * @version 1.0.0
 * @date 2025-12-25
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */
#ifndef LIOT_APWM_H
#define LIOT_APWM_H

/*===========================================================================
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liot_api_common.h"
#include "liot_os.h"
#include "liot_pwm.h"

/**
 * @enum Liot_ApwmIndex_e
 * @brief Enumeration defining the indices for APWM (Advanced Pulse Width Modulation) channels.
 * 
 * This enumeration lists the available APWM channel indices that can be used 
 * for configuration and control. Each index represents a unique APWM channel.
 * 
 */
typedef enum
{
    LIOT_APWM_INDEX_0,      ///< APWM channel index 0
    LIOT_APMW_INDEX_1,      ///< APWM channel index 1
    LIOT_APWM_INDEX_2,      ///< APWM channel index 2
    LIOT_APWM_INDEX_MAX,
}Liot_ApwmIndex_e;

/**
 * @enum Liot_ApwmPeriod_e
 * @brief Enumeration defining the maximum period options for APWM (Advanced Pulse Width Modulation).
 * 
 * This enumeration lists the available maximum period values that can be configured 
 * for an APWM channel. Each value represents a specific time duration in milliseconds.
 * 
 * @note The APWM clock source is 32 kHz. As the frequency increases, the precision of the PWM signal decreases.
 * 
 */
typedef enum
{
    LIOT_APWM_MAX_PERIOD_4096MS = 0,    ///< 4096ms
    LIOT_APWM_MAX_PERIOD_2048MS,        ///< 2048ms
    LIOT_APWM_MAX_PERIOD_1024MS,        ///< 1024ms
    LIOT_APWM_MAX_PERIOD_512MS,         ///< 512ms
    LIOT_APWM_MAX_PERIOD_256MS,         ///< 256ms
    LIOT_APWM_MAX_PERIOD_128MS,         ///< 128ms
    LIOT_APWM_MAX_PERIOD_64MS,          ///< 64ms
    LIOT_APWM_MAX_PERIOD_32MS,          ///< 32ms
    LIOT_APMW_MAX_PERIOD_MAX,
}Liot_ApwmPeriod_e;

/**
 * @enum Liot_ApwmAccuracy_e
 * @brief Enumeration defining the accuracy options for APWM (Advanced Pulse Width Modulation).
 * 
 * This enumeration lists the available accuracy levels for configuring APWM. Each value represents a different accuracy level, affecting the time resolution of the PWM output.
 * 
 */
typedef enum
{
    LIOT_APWM_ACCURACY_0 = 0,     ///< 0: not set accuracy
    LIOT_APWM_ACCURACY_6 = 6,     ///< 6: prd is enum fromApwmMaxPeriod_e / 2
    LIOT_APWM_ACCURACY_7 = 7,     ///< 7: prd choose from ApwmMaxPeriod_e
    LIOT_APWM_ACCURACY_8 = 8,     ///< 8: prd is enum fromApwmMaxPeriod_e * 2
}Liot_ApwmAccuracy_e;

/**
 * @struct Liot_ApwmCfg_t
 * @brief Defines the structure for APWM configuration parameters.
 * 
 * This structure contains all the necessary parameters for configuring APWM, such as channel index, 
 * period, accuracy, and the duration of high and low periods.
 * For example, when [highPeriod] is 70 and [lowPeriod] is 20, 70-20=50 20-70=50, the duty cycle is 50% high and 50% low.
 */
typedef struct
{
    Liot_ApwmIndex_e idx;           ///< APWM channel index, ranging from 0 to 2, corresponding to the [Liot_ApwmIndex_e] enumeration.   
    Liot_ApwmPeriod_e Period;       ///< APWM period, taken from the [Liot_ApwmPeriod_e] enumeration.
    Liot_ApwmAccuracy_e accuracy;   ///< APWM accuracy, taken from the [Liot_ApwmAccuracy_e]enumeration.
    uint16_t highPeriod;            ///< High period duration, ranging from 0 to 100.
    uint16_t lowPeriod;             ///< Low period duration, ranging from 0 to 100.
}Liot_ApwmCfg_t;


/**
 * @function Liot_ApwmCfg
 * @brief Configures the parameters for APWM (Advanced Pulse Width Modulation).
 * 
 * This function sets the relevant APWM parameters based on the provided configuration structure, including channel index, period, 
 * accuracy, and the duration of high and low periods.
 * 
 * @param cfg 
 *        A pointer to the [Liot_ApwmCfg_t] type, containing the complete configuration parameters for APWM.
 * 
 * @return liot_pwm_errcode_e 
 *         Returns an enumeration value indicating the result of the configuration operation. 
 *         Possible return values include success or specific error codes.
 */
liot_pwm_errcode_e Liot_ApwmCfg(Liot_ApwmCfg_t *cfg);

/**
 * @function Liot_ApwmEnable
 * @brief Enables or disables the specified APWM channel.
 * 
 * This function enables or disables the APWM output channel for the specified index.
 * 
 * @param idx 
 *        Specifies the APWM channel index to enable or disable, ranging from 0 to 2, corresponding to the [Liot_ApwmIndex_e] enumeration.
 * 
 * @param en 
 *        A boolean value indicating whether to enable the channel:
 *        - [true] : Enables the APWM channel.
 *        - [false]: Disables the APWM channel.
 */
liot_pwm_errcode_e Liot_ApwmEnable(Liot_ApwmIndex_e idx, bool en);

#endif /* LIOT_PWM_H */