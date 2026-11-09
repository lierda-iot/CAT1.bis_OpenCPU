#include "salary_core_utils.h"

#include <string.h>

bool salary_is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int salary_days_in_month(int year, int month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && salary_is_leap_year(year)) return 29;
    if (month < 1 || month > 12) return 31;
    return days[month - 1];
}

void salary_time_add_hours(salary_time_fields_t *tm, int hours)
{
    tm->tm_hour += hours;
    while (tm->tm_hour >= 24) {
        tm->tm_hour -= 24;
        tm->tm_mday++;
        if (tm->tm_mday > salary_days_in_month(tm->tm_year, tm->tm_mon)) {
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
            tm->tm_mday = salary_days_in_month(tm->tm_year, tm->tm_mon);
        }
    }
}

uint32_t salary_config_checksum(const salary_config_t *cfg)
{
    return cfg->magic ^ cfg->monthly_salary ^ cfg->sound_interval ^
           ((uint32_t)cfg->start_hour << 24) ^ ((uint32_t)cfg->start_min << 16) ^
           ((uint32_t)cfg->end_hour << 8) ^ cfg->end_min;
}

bool salary_config_is_valid(const salary_config_t *cfg)
{
    return cfg != NULL &&
           cfg->magic == SALARY_NVM_MAGIC &&
           cfg->checksum == salary_config_checksum(cfg) &&
           cfg->sound_interval != 0;
}

void salary_config_init_defaults(salary_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = SALARY_NVM_MAGIC;
    cfg->monthly_salary = 10000;
    cfg->sound_interval = 1;
    cfg->start_hour = 9;
    cfg->end_hour = 18;
    cfg->checksum = salary_config_checksum(cfg);
}

void salary_config_prepare_save(salary_config_t *cfg)
{
    if (cfg->sound_interval == 0) cfg->sound_interval = 1;
    cfg->magic = SALARY_NVM_MAGIC;
    cfg->checksum = salary_config_checksum(cfg);
}

int salary_parse_digits(const char *text, int fallback)
{
    const char *s = text;
    int value = 0;
    if (s == NULL || s[0] == 0) return fallback;
    while (*s) {
        if (*s >= '0' && *s <= '9') value = value * 10 + *s - '0';
        s++;
    }
    return value > 0 ? value : fallback;
}

uint32_t salary_earned_cent(const salary_config_t *cfg, const salary_time_fields_t *tm)
{
    int start = cfg->start_hour * 3600 + cfg->start_min * 60;
    int end = cfg->end_hour * 3600 + cfg->end_min * 60;
    int now = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    int elapsed;
    uint64_t day_cent;
    if (end <= start) return 0;
    if (now <= start) elapsed = 0;
    else if (now >= end) elapsed = end - start;
    else elapsed = now - start;
    day_cent = ((uint64_t)cfg->monthly_salary * 100u) / SALARY_WORK_DAYS;
    return (uint32_t)(day_cent * (uint32_t)elapsed / (uint32_t)(end - start));
}
