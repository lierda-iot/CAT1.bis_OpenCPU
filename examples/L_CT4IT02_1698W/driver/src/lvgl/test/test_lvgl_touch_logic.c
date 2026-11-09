#include "unity.h"

#include "lvgl_touch_logic.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_touch_latest_sample_overwrites_old_move(void)
{
    lvgl_touch_read_state_t state = {0};
    lvgl_touch_sample_t first = {
        .touch_cnt = 1,
        .event = LVGL_TOUCH_EVENT_MOVE,
        .x = 10,
        .y = 20,
    };
    lvgl_touch_sample_t second = {
        .touch_cnt = 1,
        .event = LVGL_TOUCH_EVENT_MOVE,
        .x = 30,
        .y = 40,
    };

    lvgl_touch_read_state_init(&state);
    lvgl_touch_read_state_feed(&state, &first);
    lvgl_touch_read_state_feed(&state, &second);

    TEST_ASSERT_EQUAL_INT16(30, state.last_x);
    TEST_ASSERT_EQUAL_INT16(40, state.last_y);
    TEST_ASSERT_FALSE(state.release_pending);
}

void test_touch_release_deferred_once_after_final_sample(void)
{
    lvgl_touch_read_state_t state = {0};
    lvgl_touch_sample_t move = {
        .touch_cnt = 1,
        .event = LVGL_TOUCH_EVENT_MOVE,
        .x = 12,
        .y = 34,
    };
    lvgl_touch_sample_t release = {
        .touch_cnt = 0,
        .event = LVGL_TOUCH_EVENT_UP,
        .x = 0,
        .y = 0,
    };

    lvgl_touch_read_state_init(&state);
    lvgl_touch_read_state_feed(&state, &move);
    lvgl_touch_read_state_feed(&state, &release);

    TEST_ASSERT_TRUE(state.release_pending);
    TEST_ASSERT_TRUE(lvgl_touch_read_state_prepare_release(&state, true));
    TEST_ASSERT_TRUE(state.release_deferred);
    lvgl_touch_read_state_consume_release(&state);
    TEST_ASSERT_FALSE(state.release_deferred);
    TEST_ASSERT_FALSE(lvgl_touch_read_state_prepare_release(&state, false));
}

void test_touch_transform_scale_swap_clamp(void)
{
    lvgl_touch_transform_cfg_t cfg = {
        .swap_xy = true,
        .mirror_x = true,
        .mirror_y = false,
        .max_x = 359,
        .max_y = 359,
    };
    int32_t x = -1;
    int32_t y = -1;

    lvgl_touch_transform_point(&cfg, 20, 380, &x, &y);

    TEST_ASSERT_EQUAL_INT32(0, x);
    TEST_ASSERT_EQUAL_INT32(20, y);
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_touch_latest_sample_overwrites_old_move);
    RUN_TEST(test_touch_release_deferred_once_after_final_sample);
    RUN_TEST(test_touch_transform_scale_swap_clamp);
    return UNITY_END();
}
