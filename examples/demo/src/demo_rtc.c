/**
 * @file liot_rtc_demo.c
 * @brief RTC (Real-Time Clock) demo implementation
 *
 * This demo demonstrates how to use the RTC functionality on the CAT1 module.
 * It includes time management operations such as setting/getting time, timezone handling,
 * time conversion between seconds and time structure, and alarm functionality.
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.0
 * @date 2025-08-12
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_nw.h"
#include "liot_rtc.h"
#include "liot_type.h"

/*========================================================================
 *  Global Variable
 *========================================================================*/
static liot_sem_t nw_semp;
/* Store the number of days in each month for non-leap years */
static int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/* Determine if it is a leap year */
#define leapyear(year)   ((year) % 4 == 0)
#define days_in_year(a)  (leapyear(a) ? 366 : 365)
#define days_in_month(a) (month_days[(a)-1])

#define FEBRUARY    2
#define STARTOFTIME 1970
#define SECDAY      86400L
#define SECYR       (SECDAY * 365)

#if !defined(rtc_demo_err_exit)
#define rtc_demo_err_exit(x, action, str) \
    do                                    \
    {                                     \
        if (x)                            \
        {                                 \
            liot_trace(str);             \
            {                             \
                goto action;              \
            }                             \
        }                                 \
    } while (1 == 0)
#endif

/*========================================================================
 *  function Definition
 *========================================================================*/

/**
 * @brief Calculate the day of the week corresponding to the given time structure
 *
 * This function calculates the day of the week (0-6, Sunday-Saturday) for a given
 * date in the RTC time structure.
 *
 * @param rtc_time Time structure pointer containing date information
 * @return Returns 0 on success; returns -1 on failure (year < 1753)
 */
static int liot_rtc_fix_weekday(liot_rtc_time_s *rtc_time)
{
    int leapsToDate;
    int lastYear;
    int day;
    int MonthOffset[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    if (rtc_time->tm_year < 1753)
        return -1;
    lastYear = rtc_time->tm_year - 1;

    // Number of leap corrections to apply up to end of last year
    leapsToDate = lastYear / 4 - lastYear / 100 + lastYear / 400;

    // This year is a leap year if it is divisible by 4 except when it is divisible by 100 unless it is divisible by 400
    // e.g. 1904 was a leap year, 1900 was not, 1996 is, and 2000 will be
    if ((rtc_time->tm_year % 4 == 0) && ((rtc_time->tm_year % 100 != 0) || (rtc_time->tm_year % 400 == 0)) &&
        (rtc_time->tm_mon > 2))
    {
        // We are past Feb. 29 in a leap year
        day = 1;
    }
    else
    {
        day = 0;
    }

    day += lastYear * 365 + leapsToDate + MonthOffset[rtc_time->tm_mon - 1] + rtc_time->tm_mday;

    rtc_time->tm_wday = day % 7;

    return 0;
}

/**
 * @brief Convert seconds to RTC time structure
 *
 * This function converts a Unix timestamp (seconds since 1970-01-01 00:00:00)
 * to an RTC time structure containing year, month, day, hour, minute, second, and weekday.
 *
 * @param time_t Input timestamp (seconds)
 * @param rtc_time Output RTC time structure pointer
 * @return Returns 0 on success; returns -1 on failure
 */
static int liot_sec_conv_rtc_time(int32_t *time_t, liot_rtc_time_s *rtc_time)
{
    int i;
    long hms, day;

    day = *time_t / SECDAY;
    hms = *time_t % SECDAY;

    // Hours, minutes, seconds are easy
    rtc_time->tm_hour = hms / 3600;
    rtc_time->tm_min  = (hms % 3600) / 60;
    rtc_time->tm_sec  = (hms % 3600) % 60;

    // Number of years in days
    for (i = STARTOFTIME; day >= days_in_year(i); i++)
    {
        day -= days_in_year(i);
    }
    rtc_time->tm_year = i;

    // Number of months in days left
    if (leapyear(rtc_time->tm_year))
    {
        days_in_month(FEBRUARY) = 29;
    }
    for (i = 1; day >= days_in_month(i); i++)
    {
        day -= days_in_month(i);
    }
    days_in_month(FEBRUARY) = 28;
    rtc_time->tm_mon        = i;

    // Days are what is left over (+1) from all that.
    rtc_time->tm_mday = day + 1;

    // Determine the day of week
    return liot_rtc_fix_weekday(rtc_time);
}

/**
 * @brief Convert RTC time structure to seconds
 *
 * This function converts an RTC time structure to a Unix timestamp
 * (seconds since 1970-01-01 00:00:00).
 *
 * @param time_t Output timestamp (seconds)
 * @param rtc_time Input RTC time structure pointer
 * @return Returns 0 on success
 */
static int liot_rtc_conv_sec_time(int32_t *time_t, liot_rtc_time_s *rtc_time)
{
    int mon  = rtc_time->tm_mon;
    int year = rtc_time->tm_year;
    int32_t days, hours;

    mon -= 2;
    if (0 >= (int)mon)
    {
        // 1..12 -> 11,12,1..10
        mon += 12;

        // Puts Feb last since it has leap day
        year -= 1;
    }
    days =
        (unsigned long)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + rtc_time->tm_mday) + year * 365 - 719499;

    hours = (days * 24) + rtc_time->tm_hour;

    *time_t = (hours * 60 + rtc_time->tm_min) * 60 + rtc_time->tm_sec;

    return 0;
}

/**
 * @brief RTC alarm callback function
 *
 * This callback function is called when the RTC alarm triggers.
 * It logs a message and prints the alarm time.
 */
void liot_rtc_test_callback(void)
{
    liot_trace("liot rtc test callback come in!");

    liot_rtc_time_s test_tm = {0};

    // disable RTC alarm
    // liot_rtc_enable_alarm(0);

    // get alarm time
    liot_trace("=========callback print alarm time===========\r\n");
    liot_rtc_get_alarm(&test_tm);
    liot_rtc_print_time(test_tm);
}

/**
 * @brief Network event callback function
 *
 * This callback function handles various network events including signal quality,
 * data registration status, and NITZ time updates.
 *
 * @param nSim      SIM card index
 * @param ind_type  Event type
 * @param ctx       Event context parameter (type depends on ind_type)
 */
static void liot_nw_ind_callback(uint8_t nSim, unsigned int ind_type, void *ctx)
{
    char csq = 99;
    liot_nw_common_reg_status_info_s *liot_nw_msg = NULL;
    liot_nw_nitz_time_info_s *liot_nw_nitz_time_info = NULL;
    liot_trace("nSim=%d, ind_type=%x", nSim, ind_type);
    switch (ind_type)
    {
    case LIOT_NW_SIGNAL_QUALITY_IND:
        csq = *((char *)ctx);
        liot_trace("csq=%d", csq);
        break;
    case LIOT_NW_DATA_REG_STATUS_IND:
        liot_nw_msg = (liot_nw_common_reg_status_info_s *)ctx;
        liot_trace("regState=%d, lac=0x%X, cid=0x%X, act=%d",
                   liot_nw_msg->state,
                   liot_nw_msg->lac,
                   liot_nw_msg->cid,
                   liot_nw_msg->act);
        if (liot_nw_msg->state == LIOT_NW_REG_STATE_HOME_NETWORK)
        {
            liot_rtos_semaphore_release(nw_semp);
        }

        break;
    case LIOT_NW_NITZ_TIME_UPDATE_IND:
        liot_nw_nitz_time_info = (liot_nw_nitz_time_info_s *)ctx;
        liot_trace(
            "nitz_time=%s, abs_time=%ld", liot_nw_nitz_time_info->nitz_time, liot_nw_nitz_time_info->abs_time);
        break;
    }
}

/**
 * @brief RTC demo main function
 *
 * This is the main function for the RTC demonstration. It performs the following operations:
 * 1. Initializes network semaphore and waits for network registration
 * 2. Sets timezone to +32
 * 3. Gets and prints current time
 * 4. Converts time structure to seconds and vice versa
 * 5. Sets a specific time (2022-11-10 16:50:30)
 * 6. Sets and enables an alarm that triggers 10 seconds later
 * 7. Registers alarm callback function
 * 8. Enters a loop to print current time and local time with different timezones
 *
 * @param arvg Thread argument (unused)
 */
void liot_rtc_demo_thread(void *arvg)
{
    liot_rtc_time_s tm;
    int32_t time_t;
    LiotOSStatus_t err = LIOT_OSI_SUCCESS;
    liot_errcode_rtc_e ret;
    int timezone_value = 32; // time zone +32
    /* Initialize network semaphore (initial state is unavailable, used to wait for network registration) */
    liot_rtos_semaphore_create(&nw_semp, 0);
    /* Register network status callback */
    liot_nw_register_cb(liot_nw_ind_callback);
    /* Wait until network registration is complete */
    liot_rtos_semaphore_wait(nw_semp, LIOT_WAIT_FOREVER);

    /* Set timezone to +32 */
    liot_rtc_set_timezone(timezone_value);

    /* Get current time */
    ret = liot_rtc_get_localtime(&tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "get time err");

    /* Print current time */
    ret = liot_rtc_print_time(tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "print time err");

    /* Convert time structure to seconds, 15 minutes per timezone interval */
    liot_rtc_conv_sec_time(&time_t, &tm);
    liot_trace("getting time convert %ld sec.\r\n", time_t);
    liot_trace("local seconds from 1970.1.1 is %d", (liot_rtc_get_time_s() + timezone_value * 15 * 60));

    /* Convert seconds to time structure */
    liot_sec_conv_rtc_time(&time_t, &tm);
    ret = liot_rtc_print_time(tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "print time err");

    // 2022-11-10 16:50:30 [TUS]
    tm.tm_year = 2022;
    tm.tm_mon  = 11;
    tm.tm_mday = 10;
    tm.tm_wday = 4;
    tm.tm_hour = 16;
    tm.tm_min  = 50;
    tm.tm_sec  = 30;
    ret        = liot_rtc_print_time(tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "print time err");

    /* Reset time and print it */
    ret = liot_rtc_set_time(&tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "set time err");
    ret = liot_rtc_get_localtime(&tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "get time err");
    ret = liot_rtc_print_time(tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "print time err");

    /* Set and enable alarm */
    liot_rtc_get_time(&tm);    
    tm.tm_sec += 10;
    ret = liot_rtc_set_alarm(&tm);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "set alarm err");
    ret = liot_rtc_register_cb(liot_rtc_test_callback);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "reg cb err");
    ret = liot_rtc_enable_alarm(1);
    rtc_demo_err_exit(ret != LIOT_RTC_SUCCESS, LIOT_RTC_DEMO_EXIT, "enable alarm err");
    liot_rtc_get_timezone(&timezone_value);
    liot_trace("timezone_value_trace: %d\r\n", timezone_value);

    while (1)
    {
        liot_rtc_get_time(&tm);
        liot_trace("=========print current time===========\r\n");
        liot_rtc_print_time(tm);

        liot_rtc_set_timezone(-20); // time zone -20
        liot_rtc_get_localtime(&tm);
        liot_trace("=========print local time===========\r\n");
        liot_rtc_print_time(tm);

        liot_rtc_get_timezone(&timezone_value);
        liot_trace("timezone_value_trace: %d\r\n", timezone_value);
        liot_rtos_task_sleep_s(5);
    }
LIOT_RTC_DEMO_EXIT:
    err = liot_rtos_task_delete(NULL);
    if (err != LIOT_OSI_SUCCESS)
    {
        liot_trace("task deleted failed");
    }
}
