#include "unity.h"

#include "demo_mp3_standalone_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_volume_boundaries_and_out_of_range(void)
{
    TEST_ASSERT_EQUAL_INT(0, demo_mp3_standalone_hw_volume(0));
    TEST_ASSERT_EQUAL_INT(DEMO_MP3_STANDALONE_HW_VOLUME_MAX,
                          demo_mp3_standalone_hw_volume(100));
    TEST_ASSERT_EQUAL_INT(-1, demo_mp3_standalone_hw_volume(-1));
    TEST_ASSERT_EQUAL_INT(-1, demo_mp3_standalone_hw_volume(101));
    TEST_ASSERT_EQUAL_INT(-1, demo_mp3_standalone_hw_volume(1000));
}

static void test_volume_rounding_scale_and_safe_cap(void)
{
    TEST_ASSERT_EQUAL_INT(1, demo_mp3_standalone_hw_volume(1));
    TEST_ASSERT_EQUAL_INT(35, demo_mp3_standalone_hw_volume(50));
    TEST_ASSERT_EQUAL_INT(14, demo_mp3_standalone_hw_volume(20));
    TEST_ASSERT_EQUAL_INT(63, demo_mp3_standalone_hw_volume(90));
    TEST_ASSERT_EQUAL_INT(DEMO_MP3_STANDALONE_HW_VOLUME_MAX - 1,
                          demo_mp3_standalone_hw_volume(99));
    TEST_ASSERT_TRUE(demo_mp3_standalone_hw_volume(100) <=
                     DEMO_MP3_STANDALONE_HW_VOLUME_MAX);
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_volume_boundaries_and_out_of_range);
    RUN_TEST(test_volume_rounding_scale_and_safe_cap);
    return UNITY_END();
}
