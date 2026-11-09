#include "unity.h"

#include "sc7a20h_page_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_clamp_inside_lower_upper_bounds(void)
{
    TEST_ASSERT_EQUAL_INT(5, sc7a20h_clamp_int(5, 0, 10));
    TEST_ASSERT_EQUAL_INT(0, sc7a20h_clamp_int(-1, 0, 10));
    TEST_ASSERT_EQUAL_INT(10, sc7a20h_clamp_int(11, 0, 10));
}

void test_map_dot_axis_center_and_clamp(void)
{
    TEST_ASSERT_EQUAL_INT(180, sc7a20h_map_dot_axis(180, 0, 130, 500, 9, 351));
    TEST_ASSERT_EQUAL_INT(310, sc7a20h_map_dot_axis(180, 500, 130, 500, 9, 351));
    TEST_ASSERT_EQUAL_INT(50, sc7a20h_map_dot_axis(180, -500, 130, 500, 9, 351));
    TEST_ASSERT_EQUAL_INT(9, sc7a20h_map_dot_axis(180, -1000, 130, 500, 9, 351));
    TEST_ASSERT_EQUAL_INT(351, sc7a20h_map_dot_axis(180, 1000, 130, 500, 9, 351));
}

void test_filter_axis_weighted_average(void)
{
    TEST_ASSERT_EQUAL_INT(0, sc7a20h_filter_axis(0, 0));
    TEST_ASSERT_EQUAL_INT(2, sc7a20h_filter_axis(0, 10));
    TEST_ASSERT_EQUAL_INT(7, sc7a20h_filter_axis(10, 0));
    TEST_ASSERT_EQUAL_INT(180, sc7a20h_filter_axis(180, 180));
    TEST_ASSERT_EQUAL_INT(0, sc7a20h_filter_axis(0, 1));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_clamp_inside_lower_upper_bounds);
    RUN_TEST(test_map_dot_axis_center_and_clamp);
    RUN_TEST(test_filter_axis_weighted_average);
    return UNITY_END();
}
