#include "app_time.h"

static int app_time_days_in_month(int year, int month)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && ((year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0)))) return 29;
    if (month < 1 || month > 12) return 31;
    return days[month - 1];
}

static void app_time_add_hours(liot_rtc_time_s *tm, int hours)
{
    tm->tm_hour += hours;
    while (tm->tm_hour >= 24) {
        tm->tm_hour -= 24;
        tm->tm_mday++;
        if (tm->tm_mday > app_time_days_in_month(tm->tm_year, tm->tm_mon)) {
            tm->tm_mday = 1;
            tm->tm_mon++;
            if (tm->tm_mon > 12) {
                tm->tm_mon = 1;
                tm->tm_year++;
            }
        }
    }
    while (tm->tm_hour < 0) {
        tm->tm_hour += 24;
        tm->tm_mday--;
        if (tm->tm_mday < 1) {
            tm->tm_mon--;
            if (tm->tm_mon < 1) {
                tm->tm_mon = 12;
                tm->tm_year--;
            }
            tm->tm_mday = app_time_days_in_month(tm->tm_year, tm->tm_mon);
        }
    }
}

bool app_time_get_rtc_display(liot_rtc_time_s *tm)
{
    if (liot_rtc_get_time(tm) != LIOT_RTC_SUCCESS) return false;
    app_time_add_hours(tm, 8);
    return true;
}
