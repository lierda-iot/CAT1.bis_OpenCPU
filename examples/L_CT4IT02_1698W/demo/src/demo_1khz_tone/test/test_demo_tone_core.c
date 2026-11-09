#include "unity.h"

#include "demo_tone_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_volume_boundaries_and_set_get_roundtrip(void)
{
    demo_tone_core_volume_t state = { 100 };

    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_set(&state, 0));
    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_get(&state));

    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_set(&state, 100));
    TEST_ASSERT_EQUAL_INT(100, demo_tone_core_volume_get(&state));
}

static void test_volume_out_of_range_and_updates(void)
{
    demo_tone_core_volume_t state = { 100 };

    TEST_ASSERT_EQUAL_INT(-1, demo_tone_core_volume_set(&state, -1));
    TEST_ASSERT_EQUAL_INT(-1, demo_tone_core_volume_set(&state, 101));
    TEST_ASSERT_EQUAL_INT(-1, demo_tone_core_volume_set(&state, 1000));

    /* Rejected set leaves the previous value untouched. */
    TEST_ASSERT_EQUAL_INT(100, demo_tone_core_volume_get(&state));

    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_set(&state, 30));
    TEST_ASSERT_EQUAL_INT(30, demo_tone_core_volume_get(&state));
    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_set(&state, 55));
    TEST_ASSERT_EQUAL_INT(55, demo_tone_core_volume_get(&state));
    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_set(&state, 0));
    TEST_ASSERT_EQUAL_INT(0, demo_tone_core_volume_get(&state));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_volume_boundaries_and_set_get_roundtrip);
    RUN_TEST(test_volume_out_of_range_and_updates);
    return UNITY_END();
}
