#include "unity.h"

#include <string.h>

#include "location_core_utils.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_coordinate_positive_negative_direction(void)
{
    char buf[64];
    location_format_coordinate(buf, sizeof(buf), 302741500, 'N', 'S');
    TEST_ASSERT_EQUAL_STRING("30.2741500 N", buf);
    location_format_coordinate(buf, sizeof(buf), -302741500, 'N', 'S');
    TEST_ASSERT_EQUAL_STRING("30.2741500 S", buf);
    location_format_coordinate(buf, sizeof(buf), 1201551000, 'E', 'W');
    TEST_ASSERT_EQUAL_STRING("120.1551000 E", buf);
    location_format_coordinate(buf, sizeof(buf), -1201551000, 'E', 'W');
    TEST_ASSERT_EQUAL_STRING("120.1551000 W", buf);
}

void test_compact_format_no_space_negative(void)
{
    char buf[64];
    location_format_coordinate_compact(buf, sizeof(buf), 1201551000);
    TEST_ASSERT_EQUAL_STRING("120.1551000", buf);
    location_format_coordinate_compact(buf, sizeof(buf), -1201551000);
    TEST_ASSERT_EQUAL_STRING("-120.1551000", buf);
    location_format_coordinate_compact(buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_STRING("0.0000000", buf);
}

void test_boundary_and_truncation(void)
{
    char buf[64];
    location_format_coordinate(buf, sizeof(buf), INT32_MIN, 'N', 'S');
    TEST_ASSERT_EQUAL_STRING("214.7483648 S", buf);
    location_format_coordinate(buf, sizeof(buf), -9999999, 'N', 'S');
    TEST_ASSERT_EQUAL_STRING("0.9999999 S", buf);
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_coordinate_positive_negative_direction);
    RUN_TEST(test_compact_format_no_space_negative);
    RUN_TEST(test_boundary_and_truncation);
    return UNITY_END();
}
