/**
 * @file liot_rtc.h
 * @brief Real-Time Clock (RTC) API for Lierda modules
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#ifndef _LIOT_RTC_H_
#define _LIOT_RTC_H_

#include "liot_api_common.h"
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Macro Definition
 *===========================================================================*/
#define LIOT_RTC_ERRCODE_BASE (LIOT_COMPONENT_BSP_RTC << 16)

/*========================================================================
 *  Enumeration Definition
 *========================================================================*/
/**
 * rtc errcode
 */
typedef enum
{
    LIOT_RTC_SUCCESS = LIOT_SUCCESS, ///< Operation successful

    LIOT_RTC_INVALID_PARAM_ERR = 1 | LIOT_RTC_ERRCODE_BASE, ///< Invalid parameter error
    LIOT_RTC_SET_TIME_ERROR,                                ///< Set time error
    LIOT_RTC_GET_TIME_ERROR,                                ///< Get time error
    LIOT_RTC_SET_CB_ERR,                                    ///< Set callback error
    LIOT_RTC_ENABLE_ALARM_ERR,                              ///< Enable alarm error
    LIOT_RTC_REMOVE_ALARM_ERR,                              ///< Remove alarm error
    LIOT_RTC_GET_FUNC_ERR,                                  ///< Get function error
} liot_errcode_rtc_e;

/*========================================================================
 *  Type Definition
 *========================================================================*/
// time[2000-01-01 00:00:00-----2100-01-01 00:00:00]
typedef struct liot_rtc_time_struct
{
    int tm_sec;  // seconds [0,59]
    int tm_min;  // minutes [0,59]
    int tm_hour; // hour [0,23]
    int tm_mday; // day of month [1,31]
    int tm_mon;  // month of year [1,12]
    int tm_year; // year [2000-2100]
    int tm_wday; // wday [0-6],sunday = 0,this value has no effect when setting the time
} liot_rtc_time_s;

/*===========================================================================
 * Functions declaration
 ===========================================================================*/
/**
 * @brief RTC callback function type for alarm notifications
 *
 * This is the function pointer type for RTC alarm callbacks.
 * The callback function is called when an RTC alarm is triggered.
 */
typedef void (*liot_rtc_cb)(void);

/**
 * @brief Set RTC time
 *
 * This function sets the RTC time to the specified date and time.
 *
 * @param tm Time structure containing the date and time to set
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Time set successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_set_time(liot_rtc_time_s *tm);

/**
 * @brief Get RTC time
 *
 * This function retrieves the current RTC time.
 *
 * @param tm Pointer to time structure to store the retrieved time
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Time retrieved successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_get_time(liot_rtc_time_s *tm);

/**
 * @brief Get RTC time converted to seconds
 *
 * This function retrieves the current RTC time and converts it to seconds
 * since January 1, 1970, 00:00:00.
 *
 * @return Converted seconds since 1970-01-01 00:00:00
 */
INT32 liot_rtc_get_time_s(void);

/**
 * @brief Get RTC time converted to millisecond
 *
 * This function retrieves the current RTC time and converts it to millisecond
 * since January 1, 1970, 00:00:00.
 *
 * @return Converted millisecond since 1970-01-01 00:00:00
 */
INT32 Liot_GetTimestamp(uint64_t *timestamp);

/**
 * @brief Get local RTC time
 *
 * This function retrieves the current local RTC time, adjusted for the
 * configured timezone.
 *
 * @param tm Pointer to time structure to store the retrieved local time
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Local time retrieved successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_get_localtime(liot_rtc_time_s *tm);

/**
 * @brief Set timezone in 15-minute units
 *
 * This function sets the timezone offset in 15-minute units.
 *
 * @param timezone Timezone value in 15-minute units, range: [-96, +96]
 *                 (representing -24 to +24 hours)
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Timezone set successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_set_timezone(int timezone);

/**
 * @brief Get timezone in 15-minute units
 *
 * This function retrieves the currently configured timezone offset in 15-minute units.
 *
 * @param timezone Pointer to store the timezone value in 15-minute units
 *                 Range: [-48, +56] (representing -12 to +14 hours)
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Timezone retrieved successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_get_timezone(int *timezone);

/**
 * @brief Print RTC time
 *
 * This function prints the specified RTC time to the trace output.
 *
 * @param tm Time structure containing the time to print
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Time printed successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_print_time(liot_rtc_time_s tm);

/**
 * @brief Set RTC alarm time
 *
 * This function sets the RTC alarm time. The alarm will trigger at the
 * specified date and time.
 *
 * @param tm Pointer to time structure containing the alarm time
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Alarm time set successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_set_alarm(liot_rtc_time_s *tm);

/**
 * @brief Get RTC alarm time
 *
 * This function retrieves the currently configured RTC alarm time.
 *
 * @param tm Pointer to time structure to store the retrieved alarm time
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Alarm time retrieved successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_get_alarm(liot_rtc_time_s *tm);

/**
 * @brief Enable and disable RTC alarm
 *
 * This function enables or disables the RTC alarm functionality.
 *
 * @param on_off Switch value: 1 means enable, 0 means disable
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Alarm enabled/disabled successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_enable_alarm(unsigned char on_off);

/**
 * @brief Register RTC alarm callback function
 *
 * This function registers a callback function that will be called when
 * the RTC alarm is triggered.
 *
 * @attention
 * 1. It is forbidden to block the interrupt;\n
 * 2. It is forbidden to call Audio start/stop/close, file write/read, CFW (related to RPC) in interrupt;\n
 * 3. It is forbidden to enter critical in interrupt;\n
 * 4. It is suggested for users to perform simple operations, or send event (no timeout) to inform your task in interrupt.
 *
 * @param cb Alarm callback function to register
 * @return Error code indicating success or failure
 *         - LIOT_RTC_SUCCESS: Callback registered successfully
 *         - Other: Error code
 */
liot_errcode_rtc_e liot_rtc_register_cb(liot_rtc_cb cb);

#ifdef __cplusplus
} /*"C" */
#endif

#endif /* _LIOT_RTC_H_ */
