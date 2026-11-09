#ifndef SALARY_CORE_UTILS_H
#define SALARY_CORE_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pure salary calculator logic extracted from demo_salary_calculator.c.
 * No liot/lvgl dependency; RTC fields are carried in salary_time_fields_t so
 * the production file only needs a thin struct<->liot_rtc_time_s adapter.
 */

#define SALARY_NVM_MAGIC 0x53414C31u
#define SALARY_WORK_DAYS 22u

typedef struct {
    uint32_t magic;
    uint32_t monthly_salary;
    uint32_t sound_interval;
    uint8_t start_hour;
    uint8_t start_min;
    uint8_t end_hour;
    uint8_t end_min;
    uint32_t checksum;
} salary_config_t;

typedef struct {
    int tm_year;
    int tm_mon;
    int tm_mday;
    int tm_hour;
    int tm_min;
    int tm_sec;
} salary_time_fields_t;

bool salary_is_leap_year(int year);
int salary_days_in_month(int year, int month);
void salary_time_add_hours(salary_time_fields_t *tm, int hours);
uint32_t salary_config_checksum(const salary_config_t *cfg);
bool salary_config_is_valid(const salary_config_t *cfg);
void salary_config_init_defaults(salary_config_t *cfg);
void salary_config_prepare_save(salary_config_t *cfg);
int salary_parse_digits(const char *text, int fallback);
uint32_t salary_earned_cent(const salary_config_t *cfg, const salary_time_fields_t *tm);

#endif /* SALARY_CORE_UTILS_H */
