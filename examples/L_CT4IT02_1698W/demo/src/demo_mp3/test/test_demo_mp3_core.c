#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "demo_mp3_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static mp3_track_t make_track(const uint8_t *start, size_t size)
{
    mp3_track_t track;
    memset(&track, 0, sizeof(track));
    track.mp3_start = start;
    track.mp3_end = start + size;
    return track;
}

void test_frame_offset_skips_id3_junk_no_sync_returns_zero(void)
{
    static const uint8_t data[] = {
        'I', 'D', '3', 0x04, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0xFB, 0x90, 0x00
    };
    static const uint8_t junk[] = { 'x', 'y', 'z', 0xFF, 0x41 };
    mp3_track_t track = make_track(data, sizeof(data));
    mp3_track_t junk_track = make_track(junk, sizeof(junk));

    TEST_ASSERT_EQUAL_UINT32(12u, mp3_frame_offset(&track));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_frame_offset(&junk_track));
}

void test_sample_rate_and_bitrate_table_bounds(void)
{
    TEST_ASSERT_EQUAL_UINT32(44100u, mp3_sample_rate(3u, 0u));
    TEST_ASSERT_EQUAL_UINT32(24000u, mp3_sample_rate(2u, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_sample_rate(3u, 3u));
    TEST_ASSERT_EQUAL_UINT32(128u, mp3_bitrate(3u, 1u, 9u));
    TEST_ASSERT_EQUAL_UINT32(144u, mp3_bitrate(2u, 1u, 13u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_bitrate(3u, 1u, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_bitrate(3u, 1u, 16u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_bitrate(3u, 2u, 9u));
}

/*
 * MPEG1 Layer III, 44.1kHz, 128kbps: frame length = 144*128000/44100 + 0
 * = 417 (floor), so one frame should estimate 1152 samples / 44.1kHz = 26ms.
 */
void test_duration_estimate_single_frame_cbr(void)
{
    uint8_t data[417];
    mp3_track_t track;
    memset(data, 0, sizeof(data));
    data[0] = 0xFF;
    data[1] = 0xFB; /* MPEG1, Layer III, no CRC */
    data[2] = (uint8_t)((9u << 4) | (0u << 2)); /* 128kbps, 44100Hz */
    data[3] = 0x00;
    track = make_track(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(26u, mp3_estimate_duration_ms(&track, 0u));
}

void test_duration_estimate_walks_frames_skips_bad(void)
{
    uint8_t data[900];
    mp3_track_t track;
    memset(data, 0, sizeof(data));
    data[0] = 0xFF;
    data[1] = 0xFB;
    data[2] = (uint8_t)((9u << 4) | (0u << 2));
    data[3] = 0x00;
    memset(data + 417, 0xAA, 30); /* bad junk between frames */
    data[447] = 0xFF;
    data[448] = 0xFB;
    data[449] = (uint8_t)((9u << 4) | (0u << 2));
    data[450] = 0x00;
    track = make_track(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(52u, mp3_estimate_duration_ms(&track, 0u));
}

void test_current_ms_state_machine(void)
{
    uint32_t duration = 1000u;

    TEST_ASSERT_EQUAL_UINT32(0u, mp3_core_current_ms(MP3_STATE_LOADING, false,
                                                     duration, 0u, 0u, 0u, 500u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_core_current_ms(MP3_STATE_ERROR, false,
                                                     duration, 0u, 0u, 0u, 500u));
    TEST_ASSERT_EQUAL_UINT32(0u, mp3_core_current_ms(MP3_STATE_STOPPED, false,
                                                     duration, 0u, 0u, 0u, 500u));
    TEST_ASSERT_EQUAL_UINT32(duration, mp3_core_current_ms(MP3_STATE_STOPPED, true,
                                                           duration, 0u, 0u, 0u, 500u));
    TEST_ASSERT_EQUAL_UINT32(123u, mp3_core_current_ms(MP3_STATE_PAUSED, false,
                                                       duration, 123u, 0u, 0u, 500u));
    TEST_ASSERT_EQUAL_UINT32(500u, mp3_core_current_ms(MP3_STATE_PLAYING, false,
                                                       duration, 0u, 100u, 0u, 600u));
    TEST_ASSERT_EQUAL_UINT32(400u, mp3_core_current_ms(MP3_STATE_PLAYING, false,
                                                       duration, 0u, 100u, 100u, 600u));
    TEST_ASSERT_EQUAL_UINT32(duration, mp3_core_current_ms(MP3_STATE_PLAYING, false,
                                                           duration, 0u, 100u, 0u, 99999u));
}

void test_format_time_and_state_name(void)
{
    char buf[16];
    mp3_format_time(buf, sizeof(buf), 0u);
    TEST_ASSERT_EQUAL_STRING("00:00", buf);
    mp3_format_time(buf, sizeof(buf), 61000u);
    TEST_ASSERT_EQUAL_STRING("01:01", buf);
    mp3_format_time(buf, sizeof(buf), 3599999u);
    TEST_ASSERT_EQUAL_STRING("59:59", buf);
    TEST_ASSERT_EQUAL_STRING("STOPPED", mp3_state_name(MP3_STATE_STOPPED));
    TEST_ASSERT_EQUAL_STRING("PLAYING", mp3_state_name(MP3_STATE_PLAYING));
    TEST_ASSERT_EQUAL_STRING("PAUSED", mp3_state_name(MP3_STATE_PAUSED));
    TEST_ASSERT_EQUAL_STRING("LOADING", mp3_state_name(MP3_STATE_LOADING));
    TEST_ASSERT_EQUAL_STRING("ERROR", mp3_state_name(MP3_STATE_ERROR));
    TEST_ASSERT_EQUAL_STRING("?", mp3_state_name((mp3_state_t)99));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_frame_offset_skips_id3_junk_no_sync_returns_zero);
    RUN_TEST(test_sample_rate_and_bitrate_table_bounds);
    RUN_TEST(test_duration_estimate_single_frame_cbr);
    RUN_TEST(test_duration_estimate_walks_frames_skips_bad);
    RUN_TEST(test_current_ms_state_machine);
    RUN_TEST(test_format_time_and_state_name);
    return UNITY_END();
}
