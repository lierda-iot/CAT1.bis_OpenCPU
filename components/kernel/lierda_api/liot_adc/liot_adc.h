/**
 * @file liot_adc.h
 * @brief LIoT ADC (Analog-to-Digital Converter) Interface
 *
 * This header file provides the application programming interface (API) for interacting with the
 * Analog-to-Digital Converter (ADC) subsystem in LIoT platform. It includes enumerations for
 * ADC channel identification, error codes, voltage division ratios, and function prototypes
 * for reading ADC values from various channels.
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef _LIOT_ADC_H
#define _LIOT_ADC_H
/*> include header files here*/

#include <stdio.h>

#include "liot_api_common.h"
#include "liot_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/

/****************************  error code about ADC    ***************************/
typedef enum
{
    LIOT_ADC_SUCCESS             = 0,
    LIOT_ADC_INVALID_PARAM_ERR   = 10 | (LIOT_COMPONENT_BSP_ADC << 16),
    LIOT_ADC_GET_VALUE_ERROR     = 50 | (LIOT_COMPONENT_BSP_ADC << 16),
    LIOT_ADC_MEM_ADDR_NULL_ERROR = 60 | (LIOT_COMPONENT_BSP_ADC << 16),
    LIOT_ADC_TASK_ERROR,
} liot_adc_errcode_e;

typedef enum
{
    LIOT_ADC0_CHANNEL,
    LIOT_ADC1_CHANNEL,
    LIOT_ADC2_CHANNEL,
    LIOT_ADC3_CHANNEL,
    LIOT_ADC_THERMAL_CHANNEL,
    LIOT_ADC_VBAT_CHANNEL,
    LIOT_ADC_CHANNEL_MAX,
} liot_adc_chan_id_e;


#if defined (CHIP_EC718)
/**
 * @brief ADC voltage divider ratios for EC718 chip
 *
 * Specifies the available resistor divider ratios for ADC input voltage scaling
 * on EC718 series chips. These ratios determine how the input voltage is divided
 * before being sampled by the ADC.
 */
typedef enum
{
    LIOT_ADC_AIO_RESDIV_RATIO_1        = 0U,  /**< ADC AIO RESDIV select as VIN (1:1 ratio) */
    LIOT_ADC_AIO_RESDIV_RATIO_28OVER32 = 1U,  /**< ADC AIO RESDIV select as 28/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_24OVER32 = 2U,  /**< ADC AIO RESDIV select as 24/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_20OVER32 = 3U,  /**< ADC AIO RESDIV select as 20/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_16OVER32 = 4U,  /**< ADC AIO RESDIV select as 16/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_12OVER32 = 5U,  /**< ADC AIO RESDIV select as 12/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_8OVER32  = 6U,  /**< ADC AIO RESDIV select as 8/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_7OVER32  = 7U,  /**< ADC AIO RESDIV select as 7/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_6OVER32  = 8U,  /**< ADC AIO RESDIV select as 6/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_5OVER32  = 9U,  /**< ADC AIO RESDIV select as 5/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_4OVER32  = 10U, /**< ADC AIO RESDIV select as 4/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_3OVER32  = 11U, /**< ADC AIO RESDIV select as 3/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_2OVER32  = 12U, /**< ADC AIO RESDIV select as 2/32 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_1OVER32  = 13U, /**< ADC AIO RESDIV select as 1/32 VIN */
    LIOT_ADC_AIO_RESDIV_BYPASS         = 14U, /**< BYPASS the whole ADC AIO RESDIV network(direct input) */

} liot_adc_resdiv_e;
#elif defined (CHIP_EC716)
/**
 * @brief ADC voltage divider ratios for EC716 chip
 *
 * Specifies the available resistor divider ratios for ADC input voltage scaling
 * on EC716 series chips. These ratios determine how the input voltage is divided
 * before being sampled by the ADC.
 */
typedef enum
{
    LIOT_ADC_AIO_RESDIV_RATIO_1          = 0U,  /**< ADC AIO RESDIV select as VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_14OVER16   = 1U,  /**< ADC AIO RESDIV select as 14/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_12OVER16   = 2U,  /**< ADC AIO RESDIV select as 12/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_10OVER16   = 3U,  /**< ADC AIO RESDIV select as 10/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_8OVER16    = 4U,  /**< ADC AIO RESDIV select as 8/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_7OVER16    = 5U,  /**< ADC AIO RESDIV select as 7/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_6OVER16    = 6U,  /**< ADC AIO RESDIV select as 6/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_5OVER16    = 7U,  /**< ADC AIO RESDIV select as 5/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_4OVER16    = 8U,  /**< ADC AIO RESDIV select as 4/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_3OVER16    = 9U,  /**< ADC AIO RESDIV select as 3/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_2OVER16    = 10U, /**< ADC AIO RESDIV select as 2/16 VIN */
    LIOT_ADC_AIO_RESDIV_RATIO_1OVER16    = 11U, /**< ADC AIO RESDIV select as 1/16 VIN */
    LIOT_ADC_AIO_RESDIV_BYPASS           = 12U, /**< BYPASS the whole ADC AIO RESDIV network(direct input) */

} liot_adc_resdiv_e;
#endif

/*===========================================================================
 * Struct
 ===========================================================================*/

typedef struct
{
    uint32_t reqhandle;
    uint32_t request;
} liot_atadc_msg;

typedef struct
{
    uint8_t chanid;
    uint8_t hwid;
} liot_adc_func_s;

/**
 * @brief Reads the voltage value from a specified ADC channel
 *
 * This function reads the converted voltage value from the specified ADC channel.
 * The result is returned in millivolts (mV) through the output parameter.
 *
 * @param[in]  liot_adc_channel_id  ADC channel identifier (see @ref liot_adc_chan_id_e)
 * @param[out] adc_value            Pointer to store the measured voltage value in mV
 *
 * @return @ref liot_adc_errcode_e indicating success or failure
 * @retval LIOT_ADC_SUCCESS             Operation completed successfully
 * @retval LIOT_ADC_INVALID_PARAM_ERR   Invalid channel ID or NULL pointer
 * @retval LIOT_ADC_GET_VALUE_ERROR     Failed to retrieve ADC value
 */
liot_adc_errcode_e liot_adc_get_volt(liot_adc_chan_id_e liot_adc_channel_id, int *adc_value);

/**
 * @brief Reads raw ADC value with specified voltage divider ratio
 *
 * This function reads the raw ADC value using the specified resistor divider ratio
 * for input voltage scaling. The result is the raw ADC count before voltage conversion.
 *
 * @param[in]  liot_adc_channel_id  ADC channel identifier (see @ref liot_adc_chan_id_e)
 * @param[in]  liot_adc_div         Voltage divider ratio (see @ref liot_adc_resdiv_e)
 * @param[out] adc_value            Pointer to store the raw ADC value
 *
 * @return @ref liot_adc_errcode_e indicating success or failure
 * @retval LIOT_ADC_SUCCESS             Operation completed successfully
 * @retval LIOT_ADC_INVALID_PARAM_ERR   Invalid parameters or NULL pointer
 * @retval LIOT_ADC_GET_VALUE_ERROR     Failed to retrieve ADC value
 */
liot_adc_errcode_e liot_adc_get_volt_raw(liot_adc_chan_id_e liot_adc_channel_id,
                                                liot_adc_resdiv_e liot_adc_div,
                                                int *adc_value);

#ifdef __cplusplus
} /*"C" */
#endif

#endif