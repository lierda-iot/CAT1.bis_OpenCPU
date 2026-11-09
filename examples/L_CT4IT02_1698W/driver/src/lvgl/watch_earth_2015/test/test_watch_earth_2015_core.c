#include "unity.h"

#include "watch_earth_2015_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_hour_angle_12_hour_wrap_and_steps(void)
{
    TEST_ASSERT_EQUAL_INT16(0, watch_earth_2015_hour_angle(0, 0, 0));
    TEST_ASSERT_EQUAL_INT16(0, watch_earth_2015_hour_angle(12, 0, 0));
    TEST_ASSERT_EQUAL_INT16(300, watch_earth_2015_hour_angle(1, 0, 0));
    TEST_ASSERT_EQUAL_INT16(3300, watch_earth_2015_hour_angle(11, 0, 0));
    TEST_ASSERT_EQUAL_INT16(150, watch_earth_2015_hour_angle(12, 30, 0));
    TEST_ASSERT_EQUAL_INT16(304, watch_earth_2015_hour_angle(1, 0, 48));
    TEST_ASSERT_EQUAL_INT16(2850, watch_earth_2015_hour_angle(9, 30, 0));
}

void test_minute_angle_second_contribution(void)
{
    TEST_ASSERT_EQUAL_INT16(0, watch_earth_2015_minute_angle(0, 0));
    TEST_ASSERT_EQUAL_INT16(60, watch_earth_2015_minute_angle(1, 0));
    TEST_ASSERT_EQUAL_INT16(3599, watch_earth_2015_minute_angle(59, 59));
}

void test_second_angle_60_steps(void)
{
    TEST_ASSERT_EQUAL_INT16(0, watch_earth_2015_second_angle(0));
    TEST_ASSERT_EQUAL_INT16(60, watch_earth_2015_second_angle(1));
    TEST_ASSERT_EQUAL_INT16(3540, watch_earth_2015_second_angle(59));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_hour_angle_12_hour_wrap_and_steps);
    RUN_TEST(test_minute_angle_second_contribution);
    RUN_TEST(test_second_angle_60_steps);
    return UNITY_END();
}
