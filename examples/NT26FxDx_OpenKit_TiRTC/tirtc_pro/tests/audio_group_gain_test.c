/* Compile the production audio adapter; fake only its SDK/RTOS boundary. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../media/audio_device.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"

static unsigned s_init_calls, s_sw_calls, s_codec_calls, s_mic_calls;
static unsigned s_pause_calls, s_resume_calls, s_stop_calls;
static int s_sw_volume = -1, s_codec_volume = -1;
static uint8_t s_mic_gain;
static int s_mic_volume;
static uint8_t s_expected_mic_level = 10;
static int s_critical_depth;
static int16_t s_played[DEMO_AI_AUDIO_FRAME_SAMPLES];
static unsigned s_play_calls, s_clipped;

void liot_rtos_enter_critical(void) { ++s_critical_depth; }
void liot_rtos_exit_critical(void) { assert(s_critical_depth > 0); --s_critical_depth; }
uint32_t liot_rtos_get_running_time(void) { return 0; }
void liot_trace(const char *format, ...) { (void)format; }
liot_gpiolvl_e Liot_GpioGetLevel(liot_gpio_e gpio)
{
    assert(gpio == L_GPIO_11);
    return L_IO_HIGH;
}

Liot_AudErr_e Liot_AudioInit(Liot_AudHwConfig_t *config)
{
    assert(s_critical_depth == 0);
    assert(config->channel == L_AUD_MONO_RIGHT);
    assert(config->samples == L_AUD_16K_SAMPLES);
    assert(config->frameSize == L_AUD_FRAMESIZE_16_16);
    ++s_init_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioSetVolume(int volume)
{
    assert(s_critical_depth == 0);
    s_sw_volume = volume;
    ++s_sw_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioSetCodecVolume(int volume)
{
    assert(s_critical_depth == 0);
    s_codec_volume = volume;
    ++s_codec_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioSetMicVolume(uint8_t gain, int volume)
{
    s_mic_gain = gain;
    s_mic_volume = volume;
    ++s_mic_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioGetVolume(int *volume)
{
    *volume = s_sw_volume;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioPlayPause(void)
{
    ++s_pause_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioPlayResume(void)
{
    ++s_resume_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioStop(void)
{
    ++s_stop_calls;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioPlay(uint8_t *data, int length)
{
    /* Model the checked-in F6D_D Liot_AudioApplyVolume (default mode 0),
     * including Q8 gain and signed PCM16 saturation before the codec. */
    assert(length == sizeof(s_played));
    memcpy(s_played, data, sizeof(s_played));
    ++s_play_calls;
    for (unsigned i = 0; i < DEMO_AI_AUDIO_FRAME_SAMPLES; ++i) {
        int gain = (s_sw_volume / 10) * 256 + (s_sw_volume % 10) * 26;
        int value = ((int)s_played[i] * gain) >> 8;
        if (value > 32767) { value = 32767; ++s_clipped; }
        if (value < -32768) { value = -32768; ++s_clipped; }
        s_played[i] = (int16_t)value;
    }
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioRecord(uint8_t *data, int length)
{
    (void)data;
    (void)length;
    assert(!"Gain-control tests must not record PCM");
    return L_AUD_ERR_EXECUTE;
}

static void expect_levels(int software, int codec)
{
    assert(s_sw_volume == software);
    assert(s_codec_volume == codec);
    static const uint8_t gains[] = {0,7,7,7,8,8,8,8,9,9,9};
    static const int volumes[] = {0,160,164,169,173,177,182,186,190,195,199};
    assert(s_mic_gain == gains[s_expected_mic_level]);
    assert(s_mic_volume == volumes[s_expected_mic_level]);
    assert(s_critical_depth == 0);
}

static void close_session(demo_ai_audio_lease_t *lease)
{
    assert(demo_ai_audio_release(lease) == DEMO_AI_AUDIO_ERR_BUSY);
    assert(demo_ai_audio_stop(lease) == 0);
    expect_levels(0, 0);
    assert(demo_ai_audio_release(lease) == 0);
    assert(!demo_ai_audio_lease_is_valid(lease));
}

static bool unity_owner(demo_ai_audio_owner_e owner)
{
    return owner == DEMO_AI_AUDIO_OWNER_DEVICE
#ifdef HWDEMO_GROUP_ROOM_EN
        || owner == DEMO_AI_AUDIO_OWNER_GROUP_ROOM
#endif
        ;
}

static void check_pcm(demo_ai_audio_lease_t *lease, uint8_t level)
{
    /* Include large valid A-law outputs and both full-scale PCM endpoints. */
    static const int16_t pattern[] = {8, -8, 7808, -7808, 32256, -32256, 32767, -32768};
    int16_t pcm[DEMO_AI_AUDIO_FRAME_SAMPLES];
    for (unsigned i = 0; i < DEMO_AI_AUDIO_FRAME_SAMPLES; ++i)
        pcm[i] = pattern[i % (sizeof(pattern) / sizeof(pattern[0]))];
    unsigned previous_calls = s_play_calls;
    s_clipped = 0;
    assert(demo_ai_audio_play(lease, pcm, DEMO_AI_AUDIO_FRAME_SAMPLES) == 0);
    if (!level) {
        assert(s_play_calls == previous_calls); /* True mute, no queued speech. */
    } else if (unity_owner(lease->owner)) {
        assert(s_play_calls == previous_calls + 1);
        assert(s_clipped == 0);
        assert(memcmp(pcm, s_played, sizeof(pcm)) == 0);
    }
}

static void check_owner(demo_ai_audio_owner_e owner)
{
    static const int expected_codec[] = {0, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60};
    static const int expected_dev_codec[] = {0, 38, 41, 46, 50, 53, 57, 61, 64, 69, 72};
    demo_ai_audio_lease_t lease = DEMO_AI_AUDIO_LEASE_INIT;
    unsigned previous_writes = s_sw_calls;
    assert(demo_ai_audio_acquire(owner, &lease) == 0);
    assert(demo_ai_audio_prepare_session(&lease, 8, 10) == 0);
    assert(s_sw_calls == previous_writes + 1);
    expect_levels(unity_owner(owner) ? 10 : 53,
                  unity_owner(owner) ? 64 : 53);
    assert(demo_ai_audio_is_ready(&lease));
    for (uint8_t level = 0; level <= 10; ++level) {
        assert(demo_ai_audio_set_levels(&lease, level, 10) == 0);
        expect_levels(unity_owner(owner) ? 10 : expected_codec[level],
                      unity_owner(owner) ? expected_dev_codec[level] : expected_codec[level]);
        check_pcm(&lease, level);
    }
    /* Clamping and a repeated setting keep the final level without writes. */
    previous_writes = s_sw_calls;
    assert(demo_ai_audio_set_levels(&lease, 255, 10) == 0);
    assert(demo_ai_audio_apply_levels(&lease) == 0);
    assert(s_sw_calls == previous_writes);
    expect_levels(unity_owner(owner) ? 10 : 60,
                  unity_owner(owner) ? 72 : 60);
    for (uint8_t mic = 0; mic <= 10; ++mic) {
        s_expected_mic_level = mic;
        assert(demo_ai_audio_set_levels(&lease, 10, mic) == 0);
        expect_levels(unity_owner(owner) ? 10 : 60,
                      unity_owner(owner) ? 72 : 60);
    }
    close_session(&lease);
}

static void check_handoff(demo_ai_audio_owner_e from, demo_ai_audio_owner_e to,
                          int from_sw, int to_sw, bool use_prepare)
{
    demo_ai_audio_lease_t lease = DEMO_AI_AUDIO_LEASE_INIT;
    unsigned previous_writes;
    assert(demo_ai_audio_acquire(from, &lease) == 0);
    assert(demo_ai_audio_prepare_session(&lease, 8, 10) == 0);
    expect_levels(unity_owner(from) ? 10 : from_sw,
                  unity_owner(from) ? 64 : 53);
    close_session(&lease);
    previous_writes = s_sw_calls;
    assert(demo_ai_audio_acquire(to, &lease) == 0);
    if (use_prepare) assert(demo_ai_audio_prepare_session(&lease, 8, 10) == 0);
    else assert(demo_ai_audio_init(&lease) == 0);
    assert(s_sw_calls == previous_writes + 1);
    expect_levels(unity_owner(to) ? 10 : to_sw,
                  unity_owner(to) ? 64 : 53);
    close_session(&lease);
}

int main(void)
{
    unsigned expected_sessions = 4;
#ifdef HWDEMO_GROUP_ROOM_EN
    check_owner(DEMO_AI_AUDIO_OWNER_GROUP_ROOM);
    check_handoff(DEMO_AI_AUDIO_OWNER_GROUP_ROOM, DEMO_AI_AUDIO_OWNER_WECHAT,
                  53, 53, true);
    check_handoff(DEMO_AI_AUDIO_OWNER_GROUP_ROOM, DEMO_AI_AUDIO_OWNER_AI,
                  53, 53, false);
    check_handoff(DEMO_AI_AUDIO_OWNER_AI, DEMO_AI_AUDIO_OWNER_GROUP_ROOM,
                  53, 53, true);
    expected_sessions += 7;
#else
    /* Also exercise the warm init path with Room support compiled out. */
    check_handoff(DEMO_AI_AUDIO_OWNER_WECHAT, DEMO_AI_AUDIO_OWNER_AI,
                  53, 53, false);
    expected_sessions += 2;
#endif
    check_owner(DEMO_AI_AUDIO_OWNER_AI);
    check_owner(DEMO_AI_AUDIO_OWNER_WECHAT);
    check_owner(DEMO_AI_AUDIO_OWNER_DEVICE);
    check_owner(DEMO_AI_AUDIO_OWNER_LIVE);
    assert(s_init_calls == 1);
    assert(s_pause_calls == expected_sessions - 1);
    assert(s_resume_calls == s_pause_calls);
    assert(s_stop_calls == expected_sessions);
    assert(s_sw_calls == s_codec_calls);
    assert(s_mic_calls + s_stop_calls == s_sw_calls);
    puts("PASS: audio owner gain, mute, codec levels and owner handoff");
    return 0;
}
