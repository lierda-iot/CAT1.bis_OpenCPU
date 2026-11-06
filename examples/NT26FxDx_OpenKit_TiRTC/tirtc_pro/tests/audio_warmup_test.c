/* Compile the complete production HAL; fake only SDK/RTOS boundaries. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../media/audio_device.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"

enum { SDK_ITEM_BYTES = 5120, SILENT_MS = 1200, CHUNK_MS = 150 };
static unsigned s_calls, s_items, s_accepted_bytes, s_fail_item, s_largest_call;
static unsigned s_cases;
static int s_critical_depth;
static bool s_hidden_prefix;
static uint8_t s_accepted[50000];
static int16_t s_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES];

void liot_rtos_enter_critical(void) { ++s_critical_depth; }
void liot_rtos_exit_critical(void) { assert(s_critical_depth > 0); --s_critical_depth; }
uint32_t liot_rtos_get_running_time(void) { return 0; }
void liot_trace(const char *format, ...) { (void)format; }
liot_gpiolvl_e Liot_GpioGetLevel(liot_gpio_e gpio)
{ assert(gpio == L_GPIO_11); return L_IO_HIGH; }
Liot_AudErr_e Liot_AudioInit(Liot_AudHwConfig_t *config)
{
    assert(config->samples == L_AUD_16K_SAMPLES);
    assert(config->channel == L_AUD_MONO_RIGHT);
    assert(config->frameSize == L_AUD_FRAMESIZE_16_16);
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioSetVolume(int volume) { (void)volume; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioGetVolume(int *volume) { *volume = 53; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetCodecVolume(int volume) { (void)volume; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetMicVolume(uint8_t gain, int volume)
{ (void)gain; (void)volume; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioPlayPause(void) { return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioPlayResume(void) { return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioStop(void) { return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioRecord(uint8_t *data, int length)
{ (void)data; (void)length; assert(!"Warmup must not record"); return L_AUD_ERR_EXECUTE; }

/* Match the real F6D_A API's independently copied <=5120-byte queue items.
 * A failure can follow an accepted prefix inside a larger API call. We model
 * an exhausted allocation/queue retry, not timing or hardware acoustics. */
Liot_AudErr_e Liot_AudioPlay(uint8_t *data, int length)
{
    unsigned offset = 0;
    assert(data && length > 0 && s_critical_depth == 0);
    ++s_calls;
    if ((unsigned)length > s_largest_call) s_largest_call = (unsigned)length;
    while (offset < (unsigned)length) {
        unsigned bytes = (unsigned)length - offset;
        if (bytes > SDK_ITEM_BYTES) bytes = SDK_ITEM_BYTES;
        ++s_items;
        if (s_items == s_fail_item) {
            s_hidden_prefix = offset != 0;
            return L_AUD_ERR_EXECUTE;
        }
        assert(s_accepted_bytes + bytes <= sizeof(s_accepted));
        memcpy(s_accepted + s_accepted_bytes, data + offset, bytes);
        s_accepted_bytes += bytes;
        offset += bytes;
    }
    return L_AUD_ERR_SUCCESS;
}

static unsigned bytes_for_ms(unsigned ms)
{ return ms * (DEMO_AI_AUDIO_SAMPLE_RATE / 1000U) * sizeof(int16_t); }

static demo_ai_audio_lease_t begin_owner(demo_ai_audio_owner_e owner, uint8_t speaker, unsigned fail_item)
{
    demo_ai_audio_lease_t lease = DEMO_AI_AUDIO_LEASE_INIT;
    assert(demo_ai_audio_acquire(owner, &lease) == 0);
    assert(demo_ai_audio_prepare_session(&lease, speaker, 7) == 0);
    s_calls = s_items = s_accepted_bytes = s_largest_call = 0;
    s_fail_item = fail_item;
    s_hidden_prefix = false;
    memset(s_accepted, 0xcc, sizeof(s_accepted));
    return lease;
}

static demo_ai_audio_lease_t begin(uint8_t speaker, unsigned fail_item)
{ return begin_owner(DEMO_AI_AUDIO_OWNER_WECHAT, speaker, fail_item); }

static void end(demo_ai_audio_lease_t *lease)
{
    assert(!s_hidden_prefix);
    assert(s_largest_call <= SDK_ITEM_BYTES);
    assert(s_critical_depth == 0);
    assert(demo_ai_audio_release(lease) == DEMO_AI_AUDIO_ERR_BUSY);
    assert(demo_ai_audio_stop(lease) == 0);
    assert(demo_ai_audio_release(lease) == 0);
    ++s_cases;
}

static void check_prefix(unsigned silent_ms)
{
    for (unsigned i = 0; i < bytes_for_ms(silent_ms); ++i)
        assert(s_accepted[i] == 0);
    assert(memcmp(s_accepted + bytes_for_ms(silent_ms), s_pcm, sizeof(s_pcm)) == 0);
}

static void failures(bool early, unsigned failed_item)
{
    demo_ai_audio_lease_t lease = begin(8, failed_item);
    uint32_t extra = 123;
    int result = early ? demo_ai_audio_prewarm_session(&lease, &extra) :
        demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra);
    assert(result == (failed_item == 1 ? -L_AUD_ERR_EXECUTE - 10 :
                     DEMO_AI_AUDIO_ERR_WARM_PARTIAL));
    assert(extra == 0);
    assert(s_items == failed_item && s_calls == failed_item);
    assert(s_accepted_bytes == bytes_for_ms((failed_item - 1U) * CHUNK_MS));
    for (unsigned i = 0; i < s_accepted_bytes; ++i) assert(s_accepted[i] == 0);
    end(&lease);
}

static void success(bool early)
{
    demo_ai_audio_lease_t lease = begin(8, 0);
    uint32_t extra = 123;
    if (early) {
        assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0);
        assert(extra == SILENT_MS && s_calls == SILENT_MS / CHUNK_MS);
        assert(s_accepted_bytes == bytes_for_ms(SILENT_MS));
        extra = 123;
        assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0);
        assert(extra == 0 && s_calls == SILENT_MS / CHUNK_MS);
    }
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == 0);
    assert(extra == (early ? DEMO_AI_AUDIO_WARMUP_MS : DEMO_AI_AUDIO_SESSION_WARMUP_MS));
    assert(s_calls == 9 && s_items == 9);
    assert(s_accepted_bytes == bytes_for_ms(DEMO_AI_AUDIO_SESSION_WARMUP_MS) + sizeof(s_pcm));
    check_prefix(DEMO_AI_AUDIO_SESSION_WARMUP_MS);
    /* Later bursts retain only 80ms; no second session preamble is sent. */
    unsigned before = s_accepted_bytes;
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == 0);
    assert(extra == DEMO_AI_AUDIO_WARMUP_MS && s_calls == 10);
    assert(s_accepted_bytes - before == bytes_for_ms(extra) + sizeof(s_pcm));
    end(&lease);
}

static void already_prewarmed_failure(void)
{
    demo_ai_audio_lease_t lease = begin(8, 9);
    uint32_t extra;
    assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0 && extra == SILENT_MS);
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == -L_AUD_ERR_EXECUTE - 10);
    assert(extra == 0 && s_accepted_bytes == bytes_for_ms(SILENT_MS));
    /* Previously credited silence stays credited; retry only sends80ms+speech. */
    s_fail_item = 0;
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == 0);
    assert(extra == DEMO_AI_AUDIO_WARMUP_MS && s_calls == 10);
    check_prefix(DEMO_AI_AUDIO_SESSION_WARMUP_MS);
    end(&lease);
}

static void mute_and_plain_play(void)
{
    demo_ai_audio_lease_t lease = begin(0, 0);
    uint32_t extra = 123;
    assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0 && extra == 0);
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == 0 && extra == 0);
    assert(s_calls == 0 && s_accepted_bytes == 0);
    end(&lease);
    lease = begin(8, 0);
    assert(demo_ai_audio_play(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES) == 0);
    assert(s_calls == 1 && s_accepted_bytes == sizeof(s_pcm));
    assert(memcmp(s_accepted, s_pcm, sizeof(s_pcm)) == 0);
    assert(demo_ai_audio_play(&lease, s_pcm, 3) == -1);
    assert(demo_ai_audio_play(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES + 2) == -1);
    end(&lease);
}

static void level_change_credit(demo_ai_audio_owner_e owner, uint8_t initial, bool prepare)
{
    demo_ai_audio_lease_t lease = begin_owner(owner, initial, 0);
    uint32_t extra = 123;
    bool group = false;
#ifdef HWDEMO_GROUP_ROOM_EN
    group = owner == DEMO_AI_AUDIO_OWNER_GROUP_ROOM;
#endif
    assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0);
    assert(extra == ((initial || group) ? SILENT_MS : 0));
    /* Speaker mute/unmute before the first word must not introduce a hidden
     * second 1200ms for GROUP. WX retains its pre-existing level contract. */
    if (initial) assert(demo_ai_audio_set_levels(&lease, 0, 7) == 0);
    if (prepare) assert(demo_ai_audio_prepare_session(&lease, 5, 7) == 0);
    else assert(demo_ai_audio_set_levels(&lease, 5, 7) == 0);
    unsigned before = s_accepted_bytes;
    /* Discard the captured evidence bytes, not the HAL session state. */
    s_accepted_bytes = 0;
    assert(demo_ai_audio_play_warm(&lease, s_pcm, DEMO_AI_AUDIO_FRAME_SAMPLES, &extra) == 0);
    assert(extra == (group ? DEMO_AI_AUDIO_WARMUP_MS : DEMO_AI_AUDIO_SESSION_WARMUP_MS));
    assert(s_accepted_bytes == bytes_for_ms(extra) + sizeof(s_pcm));
    check_prefix(extra);
    assert(before == bytes_for_ms((initial || group) ? SILENT_MS : 0));
    end(&lease);
}

int main(void)
{
    for (unsigned i = 0; i < DEMO_AI_AUDIO_FRAME_SAMPLES; ++i) s_pcm[i] = (int16_t)(i + 100);
    /* This catches the old9600-byte call: SDK item2 failed after5120 bytes
     * accepted, but HAL reported ordinary failure with no partial credit. */
    failures(true, 2);
    for (unsigned item = 1; item <= 8; ++item) failures(true, item);
    for (unsigned item = 1; item <= 9; ++item) failures(false, item);
    success(true);
    success(false);
    already_prewarmed_failure();
    mute_and_plain_play();
    level_change_credit(DEMO_AI_AUDIO_OWNER_WECHAT, 0, false);
    level_change_credit(DEMO_AI_AUDIO_OWNER_WECHAT, 8, true);
#ifdef HWDEMO_GROUP_ROOM_EN
    level_change_credit(DEMO_AI_AUDIO_OWNER_GROUP_ROOM, 0, false);
    level_change_credit(DEMO_AI_AUDIO_OWNER_GROUP_ROOM, 8, false);
    level_change_credit(DEMO_AI_AUDIO_OWNER_GROUP_ROOM, 8, true);
#endif
    printf("PASS warmup: %u sessions; SDK item2 regression, all silence/speech failure boundaries, 1200+80ms success, PCM copy, mute/plain playback, safe cleanup\n", s_cases);
    return 0;
}
