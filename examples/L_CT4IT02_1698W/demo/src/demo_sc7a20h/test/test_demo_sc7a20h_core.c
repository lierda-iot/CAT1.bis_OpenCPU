#include "unity.h"

#include "demo_sc7a20h_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_threshold_level_mg_and_boundaries(void)
{
    TEST_ASSERT_EQUAL_INT(32, sc7a20h_demo_threshold_mg(1));
    TEST_ASSERT_EQUAL_INT(1280, sc7a20h_demo_threshold_mg(40));
    TEST_ASSERT_EQUAL_INT(3456, sc7a20h_demo_threshold_mg(108));
    TEST_ASSERT_EQUAL_INT(0, sc7a20h_demo_threshold_mg(0));
    TEST_ASSERT_EQUAL_INT(4096, sc7a20h_demo_threshold_mg(128));
}

void test_shake_and_click_hit(void)
{
    TEST_ASSERT_FALSE(sc7a20h_demo_shake_hit(0, 1));
    TEST_ASSERT_TRUE(sc7a20h_demo_shake_hit(1, 1));
    TEST_ASSERT_TRUE(sc7a20h_demo_shake_hit(3, 2));
    TEST_ASSERT_TRUE(sc7a20h_demo_shake_hit(0, 0));
    TEST_ASSERT_TRUE(sc7a20h_demo_click_hit(2, 2));
    TEST_ASSERT_FALSE(sc7a20h_demo_click_hit(1, 2));
    TEST_ASSERT_FALSE(sc7a20h_demo_click_hit(0, 2));
    TEST_ASSERT_FALSE(sc7a20h_demo_click_hit(3, 2));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_threshold_level_mg_and_boundaries);
    RUN_TEST(test_shake_and_click_hit);
    return UNITY_END();
}
