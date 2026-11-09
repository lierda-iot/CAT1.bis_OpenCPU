#include "unity.h"

#include "demo_extflash_p25q64_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_fill_pattern_sequence_and_wrap(void)
{
    uint8_t buf[16];

    demo_extflash_p25q64_core_fill_pattern(buf, sizeof(buf), 0x31U);
    TEST_ASSERT_EQUAL_UINT8(0x31U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x36U, buf[5]);
    TEST_ASSERT_EQUAL_UINT8(0x40U, buf[15]);

    /* Wrap at 255 -> 0: seed 0xFE, index 3 => 0x01. */
    demo_extflash_p25q64_core_fill_pattern(buf, sizeof(buf), 0xFEU);
    TEST_ASSERT_EQUAL_UINT8(0xFEU, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00U, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, buf[3]);

    demo_extflash_p25q64_core_fill_pattern(NULL, 16u, 0x31U);

    demo_extflash_p25q64_core_fs_fill_pattern(buf, sizeof(buf), 0x41U);
    TEST_ASSERT_EQUAL_UINT8(0x41U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x47U, buf[2]);   /* 0x41 + 6 */
    TEST_ASSERT_EQUAL_UINT8(0x50U, buf[5]);   /* 0x41 + 15 */

    /* Wrap: seed 0xFE, index 3 => 0xFE + 9 = 0x107 -> 0x07. */
    demo_extflash_p25q64_core_fs_fill_pattern(buf, sizeof(buf), 0xFEU);
    TEST_ASSERT_EQUAL_UINT8(0xFEU, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x07U, buf[3]);
}

static void test_tick_elapsed_forward_and_wraparound(void)
{
    TEST_ASSERT_EQUAL_UINT32(100u,
                             demo_extflash_p25q64_core_tick_elapsed(1000u, 900u));
    TEST_ASSERT_EQUAL_UINT32(0u,
                             demo_extflash_p25q64_core_tick_elapsed(1000u, 1000u));

    /* start 0xFFFFFFF0, now 0x00000010: wraps to 32. */
    TEST_ASSERT_EQUAL_UINT32(32u,
                             demo_extflash_p25q64_core_tick_elapsed(0x10u,
                                                                   0xFFFFFFF0u));

    /* start 0xFFFFFFFF, now 0x00000000: exactly one full wrap. */
    TEST_ASSERT_EQUAL_UINT32(1u,
                             demo_extflash_p25q64_core_tick_elapsed(0u,
                                                                   0xFFFFFFFFu));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_fill_pattern_sequence_and_wrap);
    RUN_TEST(test_tick_elapsed_forward_and_wraparound);
    return UNITY_END();
}
