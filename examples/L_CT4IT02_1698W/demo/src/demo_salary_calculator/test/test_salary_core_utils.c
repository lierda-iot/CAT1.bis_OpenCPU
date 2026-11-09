#include "unity.h"

#include <string.h>

#include "salary_core_utils.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_leap_year_and_days_in_month(void)
{
    TEST_ASSERT_TRUE(salary_is_leap_year(2000));
    TEST_ASSERT_FALSE(salary_is_leap_year(1900));
    TEST_ASSERT_TRUE(salary_is_leap_year(2024));
    TEST_ASSERT_FALSE(salary_is_leap_year(2025));
    TEST_ASSERT_EQUAL_INT(29, salary_days_in_month(2024, 2));
    TEST_ASSERT_EQUAL_INT(28, salary_days_in_month(2025, 2));
    TEST_ASSERT_EQUAL_INT(31, salary_days_in_month(2025, 1));
    TEST_ASSERT_EQUAL_INT(30, salary_days_in_month(2025, 4));
    TEST_ASSERT_EQUAL_INT(31, salary_days_in_month(2025, 0));
    TEST_ASSERT_EQUAL_INT(31, salary_days_in_month(2025, 13));
}

void test_add_hours_across_day_month_year(void)
{
    salary_time_fields_t tm = {2026, 2, 28, 23, 30, 0};

    salary_time_add_hours(&tm, 2);
    TEST_ASSERT_EQUAL_INT(2026, tm.tm_year);
    TEST_ASSERT_EQUAL_INT(3, tm.tm_mon);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_mday);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_hour);

    tm = (salary_time_fields_t){2024, 2, 29, 22, 0, 0};
    salary_time_add_hours(&tm, 6);
    TEST_ASSERT_EQUAL_INT(2024, tm.tm_year);
    TEST_ASSERT_EQUAL_INT(3, tm.tm_mon);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_mday);
    TEST_ASSERT_EQUAL_INT(4, tm.tm_hour);

    tm = (salary_time_fields_t){2026, 12, 31, 23, 0, 0};
    salary_time_add_hours(&tm, 2);
    TEST_ASSERT_EQUAL_INT(2027, tm.tm_year);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_mon);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_mday);
    TEST_ASSERT_EQUAL_INT(1, tm.tm_hour);

    tm = (salary_time_fields_t){2026, 3, 1, 0, 0, 0};
    salary_time_add_hours(&tm, -2);
    TEST_ASSERT_EQUAL_INT(2026, tm.tm_year);
    TEST_ASSERT_EQUAL_INT(2, tm.tm_mon);
    TEST_ASSERT_EQUAL_INT(28, tm.tm_mday);
    TEST_ASSERT_EQUAL_INT(22, tm.tm_hour);

    tm = (salary_time_fields_t){2026, 1, 1, 0, 0, 0};
    salary_time_add_hours(&tm, -2);
    TEST_ASSERT_EQUAL_INT(2025, tm.tm_year);
    TEST_ASSERT_EQUAL_INT(12, tm.tm_mon);
    TEST_ASSERT_EQUAL_INT(31, tm.tm_mday);
    TEST_ASSERT_EQUAL_INT(22, tm.tm_hour);
}

void test_config_checksum_valid_defaults_prepare_save(void)
{
    salary_config_t cfg;

    salary_config_init_defaults(&cfg);
    TEST_ASSERT_TRUE(salary_config_is_valid(&cfg));
    TEST_ASSERT_EQUAL_UINT32(10000u, cfg.monthly_salary);
    TEST_ASSERT_EQUAL_UINT32(1u, cfg.sound_interval);
    TEST_ASSERT_EQUAL_UINT32(9u, cfg.start_hour);
    TEST_ASSERT_EQUAL_UINT32(18u, cfg.end_hour);

    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_FALSE(salary_config_is_valid(&cfg));

    cfg.sound_interval = 0;
    salary_config_prepare_save(&cfg);
    TEST_ASSERT_EQUAL_UINT32(1u, cfg.sound_interval);
    TEST_ASSERT_TRUE(salary_config_is_valid(&cfg));

    cfg.checksum ^= 1u;
    TEST_ASSERT_FALSE(salary_config_is_valid(&cfg));
}

void test_parse_digits_filters_mixed_text(void)
{
    TEST_ASSERT_EQUAL_INT(123, salary_parse_digits("a1b2c3", 0));
    TEST_ASSERT_EQUAL_INT(22000, salary_parse_digits("22,000元", 0));
    TEST_ASSERT_EQUAL_INT(99, salary_parse_digits("0", 99));
    TEST_ASSERT_EQUAL_INT(99, salary_parse_digits("", 99));
    TEST_ASSERT_EQUAL_INT(99, salary_parse_digits(NULL, 99));
    TEST_ASSERT_EQUAL_INT(99, salary_parse_digits("abc", 99));
}

void test_earned_cent_window_and_rate(void)
{
    salary_config_t cfg;
    salary_time_fields_t tm;

    salary_config_init_defaults(&cfg);

    tm = (salary_time_fields_t){2026, 10, 8, 8, 59, 59};
    TEST_ASSERT_EQUAL_UINT32(0u, salary_earned_cent(&cfg, &tm));
    tm = (salary_time_fields_t){2026, 10, 8, 9, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(0u, salary_earned_cent(&cfg, &tm));
    tm = (salary_time_fields_t){2026, 10, 8, 18, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(45454u, salary_earned_cent(&cfg, &tm));
    tm = (salary_time_fields_t){2026, 10, 8, 19, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(45454u, salary_earned_cent(&cfg, &tm));

    cfg.monthly_salary = 22000;
    tm = (salary_time_fields_t){2026, 10, 8, 18, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(100000u, salary_earned_cent(&cfg, &tm));
    tm = (salary_time_fields_t){2026, 10, 8, 13, 30, 0};
    TEST_ASSERT_EQUAL_UINT32(50000u, salary_earned_cent(&cfg, &tm));

    cfg.end_hour = 9;
    tm = (salary_time_fields_t){2026, 10, 8, 12, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(0u, salary_earned_cent(&cfg, &tm));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_leap_year_and_days_in_month);
    RUN_TEST(test_add_hours_across_day_month_year);
    RUN_TEST(test_config_checksum_valid_defaults_prepare_save);
    RUN_TEST(test_parse_digits_filters_mixed_text);
    RUN_TEST(test_earned_cent_window_and_rate);
    return UNITY_END();
}
