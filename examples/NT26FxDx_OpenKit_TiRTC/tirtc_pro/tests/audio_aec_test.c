/* Exercise the actual Pro adapter. No fake of its rings, mailbox or leases. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_device.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_os.h"

extern void demo_ai_audio_aec_service_for_test(void);
static Liot_AudRecordCb_t record_cb;
static uint32_t now_ms, capture_at, reset_at, reset_delay, stop_delay;
static unsigned depth, calls, record_stops, play_stops, stream_starts, codec_writes;
static bool mutex_locked, fail_mutex, worker_created, in_worker, freeze_worker, in_isr;
static bool startup_pending, no_startup, fail_init, fail_play, fail_record_stop;
static bool capture_pending, reset_pending, reset_enabled, zero_read, recording;
static int sw_level, codec_level, mic_gain, mic_volume;
static unsigned reset_count, codec_reads;
static int16_t capture_data[320], accepted[640 * 64];
static unsigned accepted_samples;
static uint32_t play_times[64];
static demo_ai_audio_lease_t *capture_probe;
static unsigned inflight_probes;

void liot_rtos_enter_critical(void) { assert(!in_isr); ++depth; }
void liot_rtos_exit_critical(void) { assert(depth); --depth; }
uint32_t liot_rtos_get_running_time(void) { return now_ms; }
void liot_trace(const char *format, ...) { (void)format; assert(!depth && !in_isr); }
LiotOSStatus_t liot_rtos_mutex_create(liot_mutex_t *mutex)
{ assert(!depth && !in_isr); *mutex = (void *)1; return 0; }
LiotOSStatus_t liot_rtos_mutex_lock(liot_mutex_t mutex, uint32_t timeout)
{
    (void)timeout; assert(mutex == (void *)1 && !depth && !in_isr);
    if (mutex_locked || fail_mutex) return -1;
    mutex_locked = true; return 0;
}
LiotOSStatus_t liot_rtos_mutex_unlock(liot_mutex_t mutex)
{ assert(mutex == (void *)1 && mutex_locked && !depth); mutex_locked = false; return 0; }
LiotOSStatus_t liot_rtos_task_create(liot_task_t *task, uint32_t bytes, uint8_t priority,
    char *name, void (*entry)(void *), void *arg, ...)
{
    (void)arg; assert(!depth && bytes >= 4096 && priority > 0 && name && entry);
    *task = (void *)2; worker_created = true; return 0;
}
static void emit(int16_t *pcm)
{
    assert(record_cb && !depth && !in_isr); in_isr = true;
    record_cb((uint8_t *)pcm, 640); in_isr = false;
}
static void service(void)
{
    if (worker_created && !in_worker && !freeze_worker) {
        in_worker = true; demo_ai_audio_aec_service_for_test(); in_worker = false;
    }
}
void liot_rtos_task_sleep_ms(uint32_t ms)
{
    assert(!depth && !in_isr);
    for (uint32_t i = 0; i < ms; ++i) {
        ++now_ms;
        if (reset_pending && (int32_t)(now_ms - reset_at) >= 0) {
            reset_pending = false; codec_level = 65; mic_gain = 8; mic_volume = 191; ++reset_count;
        }
        if (startup_pending) {
            int16_t silence[320] = {0}; startup_pending = false;
            codec_level = 55; emit(silence);
        }
        if (capture_probe) {
            demo_ai_audio_lease_t *lease = capture_probe;
            int16_t data[320] = {0}; uint32_t dropped = 777;
            capture_probe = NULL;
            assert(demo_ai_audio_stop(lease) == DEMO_AI_AUDIO_ERR_BUSY);
            assert(demo_ai_audio_release(lease) == DEMO_AI_AUDIO_ERR_BUSY);
            assert(demo_ai_audio_set_levels(lease, 1, 1) == DEMO_AI_AUDIO_ERR_BUSY);
            assert(demo_ai_audio_discard_capture_timed(lease, &dropped) == DEMO_AI_AUDIO_ERR_BUSY && dropped == 777);
            assert(demo_ai_audio_record_20ms(lease, data) == DEMO_AI_AUDIO_ERR_BUSY);
            /* Public playback and capture may coexist, unlike control. */
            assert(demo_ai_audio_play(lease, data, 320) == 0); ++inflight_probes;
        }
        if (capture_pending && recording && (int32_t)(now_ms - capture_at) >= 0) {
            capture_pending = false; emit(capture_data);
            memset(capture_data, 0xCD, sizeof(capture_data));
        }
        if (now_ms % 5U == 0U) service();
    }
}
liot_gpiolvl_e Liot_GpioGetLevel(liot_gpio_e gpio)
{ assert(gpio == L_GPIO_11); return L_IO_HIGH; }
Liot_AudErr_e Liot_AudioInit(Liot_AudHwConfig_t *config)
{
    assert(!depth && config->use3A == 1 && config->i2cNum == 0 && config->i2sNum == 0);
    assert(config->paGpioNum == 11 && config->samples == L_AUD_16K_SAMPLES && config->callback);
    return fail_init ? L_AUD_ERR_EXECUTE : L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioSetAec(Liot_AudAecConfig_t *c)
{ assert(!depth && c->bypass == 0 && c->delay == DEMO_AI_AUDIO_AEC_DELAY); return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetAns(Liot_AudAnsConfig_t *c)
{ assert(!depth && c->bypass == 0 && c->mode == 1); return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetAgc(Liot_AudAgcConfig_t *c)
{ assert(!depth && c->bypass == 1); return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetVolume(int v)
{ assert(!depth && !in_isr); sw_level = v; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetCodecVolume(int v)
{ assert(!depth && !in_isr && mutex_locked); codec_level = v; ++codec_writes; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioSetMicVolume(uint8_t gain, int volume)
{ assert(!depth && !in_isr && mutex_locked); mic_gain = gain; mic_volume = volume; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioGetCodecVolume(int *volume)
{
    assert(!depth && mutex_locked); ++codec_reads;
    *volume = zero_read ? 0 : ((255 * codec_level / 100) * 100) / 255;
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioGetVolume(int *volume) { *volume = sw_level; return L_AUD_ERR_SUCCESS; }
Liot_AudErr_e Liot_AudioRecordStream(Liot_AudRecordCb_t callback)
{
    assert(!depth && callback && !recording); recording = true; record_cb = callback;
    ++stream_starts; startup_pending = !no_startup; return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioRecordStreamStop(void)
{
    assert(!depth && in_worker && codec_level == 0); ++record_stops;
    if (fail_record_stop) return L_AUD_ERR_EXECUTE;
    recording = false; record_cb = NULL; return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioPlayStream(uint8_t *data, uint16_t bytes)
{
    assert(!depth && in_worker && recording && bytes == 1280 && calls < 64);
    play_times[calls++] = now_ms;
    if (fail_play) return L_AUD_ERR_EXECUTE;
    memcpy(accepted + accepted_samples, data, bytes); accepted_samples += bytes / 2;
    if (reset_enabled && calls == 3) { reset_pending = true; reset_at = now_ms + reset_delay; }
    return L_AUD_ERR_SUCCESS;
}
Liot_AudErr_e Liot_AudioPlayStreamStop(void)
{
    assert(!depth && in_worker && !recording && record_cb == NULL);
    assert(record_stops > play_stops && codec_level == 0); ++play_stops;
    if (stop_delay) liot_rtos_task_sleep_ms(stop_delay);
    return L_AUD_ERR_SUCCESS;
}
/* Accidentally entering an ordinary audio API under use3A is a test failure. */
Liot_AudErr_e Liot_AudioRecord(uint8_t *data, int bytes)
{ (void)data; (void)bytes; assert(!"ordinary record under AEC"); return L_AUD_ERR_EXECUTE; }
Liot_AudErr_e Liot_AudioPlay(uint8_t *data, int bytes)
{ (void)data; (void)bytes; assert(!"ordinary play under AEC"); return L_AUD_ERR_EXECUTE; }
Liot_AudErr_e Liot_AudioStop(void) { assert(!"ordinary stop under AEC"); return L_AUD_ERR_EXECUTE; }
Liot_AudErr_e Liot_AudioPlayPause(void) { assert(!"ordinary pause under AEC"); return L_AUD_ERR_EXECUTE; }
Liot_AudErr_e Liot_AudioPlayResume(void) { assert(!"ordinary resume under AEC"); return L_AUD_ERR_EXECUTE; }

static demo_ai_audio_lease_t open_session(demo_ai_audio_owner_e owner, unsigned speaker, unsigned mic)
{
    demo_ai_audio_lease_t lease = DEMO_AI_AUDIO_LEASE_INIT;
    assert(demo_ai_audio_acquire(owner, &lease) == 0);
    assert(demo_ai_audio_prepare_session(&lease, speaker, mic) == 0);
    assert(demo_ai_audio_is_ready(&lease)); return lease;
}
static void close_session(demo_ai_audio_lease_t *lease)
{
    assert(demo_ai_audio_release(lease) == DEMO_AI_AUDIO_ERR_BUSY);
    assert(demo_ai_audio_stop(lease) == 0);
    assert(!demo_ai_audio_is_ready(lease) && codec_level == 0);
    assert(demo_ai_audio_release(lease) == 0);
}
static void queue_capture(int16_t value)
{
    for (unsigned i = 0; i < 320; ++i) capture_data[i] = value;
    capture_at = now_ms + 20; capture_pending = true;
}
static void play_frame(demo_ai_audio_lease_t *lease, int16_t value)
{
    int16_t input[320]; for (unsigned i = 0; i < 320; ++i) input[i] = value;
    assert(demo_ai_audio_play(lease, input, 320) == 0);
    for (unsigned i = 0; i < 320; ++i) assert(input[i] == value);
}
static void capture_case(bool timeout, bool probe)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_DEVICE, 4, 7);
    int16_t output[320]; uint32_t skipped = 99;
    if (!timeout) queue_capture(1234);
    if (probe) capture_probe = &lease;
    uint32_t started = now_ms;
    int ret = demo_ai_audio_record_20ms(&lease, output);
    assert(ret == (timeout ? -40 : 0));
    assert(now_ms - started == (timeout ? 120U : 20U));
    for (unsigned i = 0; i < 320; ++i) assert(output[i] == (timeout ? 0 : 1234));
    assert(!probe || inflight_probes == 1);
    assert(demo_ai_audio_discard_capture_timed(&lease, &skipped) == 0 && skipped == 0);
    queue_capture(2222); assert(demo_ai_audio_record_20ms(&lease, output) == 0 && output[0] == 2222);
    close_session(&lease);
}
static void gain_case(bool mute)
{
    for (unsigned owner = DEMO_AI_AUDIO_OWNER_AI; owner < DEMO_AI_AUDIO_OWNER_COUNT; ++owner) {
        demo_ai_audio_lease_t lease = open_session((demo_ai_audio_owner_e)owner, mute ? 0 : 1, mute ? 0 : 1);
        bool unity = owner == DEMO_AI_AUDIO_OWNER_DEVICE;
#ifdef HWDEMO_GROUP_ROOM_EN
        unity = unity || owner == DEMO_AI_AUDIO_OWNER_GROUP_ROOM;
#endif
        assert(sw_level == (unity ? 10 : mute ? 0 : 30));
        assert(codec_level == (mute ? 0 : unity ? 38 : 30));
        assert(mic_gain == (mute ? 0 : 7) && mic_volume == (mute ? 0 : 160));
        unsigned before = accepted_samples; play_frame(&lease, 2000); play_frame(&lease, 2000);
        liot_rtos_task_sleep_ms(400);
        if (mute) {
            assert(accepted_samples == before);
            int16_t out[320]; queue_capture(1234);
            assert(demo_ai_audio_record_20ms(&lease, out) == 0);
            for (unsigned i = 0; i < 320; ++i) assert(out[i] == 0);
        } else for (unsigned i = before; i < before + 640; ++i) assert(accepted[i] == (unity ? 2000 : 6000));
        close_session(&lease);
    }
}
static void warm_case(void)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_AI, 1, 1);
    uint32_t extra = 999; int16_t input[320] = {100};
    assert(demo_ai_audio_prewarm_session(&lease, &extra) == 0 && extra == 0);
    assert(demo_ai_audio_play_done(&lease) && calls == 0);
    assert(demo_ai_audio_play_warm(&lease, input, 320, &extra) == 0 && extra == 0);
    assert(!demo_ai_audio_play_done(&lease) && calls == 0);
    liot_rtos_task_sleep_ms(400);
    assert(calls == 3 && accepted[0] == 300); /* No 1280ms leading silence. */
    assert(demo_ai_audio_play_done(&lease)); close_session(&lease);
}
static void mic_levels_case(void)
{
    static const uint8_t gains[] = {0,7,7,7,8,8,8,8,9,9,9};
    static const int volumes[] = {0,160,164,169,173,177,182,186,190,195,199};
    for (unsigned owner = DEMO_AI_AUDIO_OWNER_AI; owner < DEMO_AI_AUDIO_OWNER_COUNT; ++owner) {
        demo_ai_audio_lease_t lease = open_session((demo_ai_audio_owner_e)owner, 8, 8);
        for (uint8_t level = 0; level <= 10; ++level) {
            assert(demo_ai_audio_set_levels(&lease, 8, level) == 0);
            assert(mic_gain == gains[level] && mic_volume == volumes[level]);
        }
        close_session(&lease);
    }
}
static void backpressure_case(void)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_DEVICE, 1, 1);
    int16_t input[320]; for (unsigned i = 0; i < 320; ++i) input[i] = 100;
    for (unsigned i = 0; i < 12; ++i) assert(demo_ai_audio_play(&lease, input, 320) == 0);
    uint32_t extra = 987;
    assert(demo_ai_audio_play_warm(&lease, input, 320, &extra) == DEMO_AI_AUDIO_ERR_PLAY_BUSY && extra == 0);
    assert(calls == 0); liot_rtos_task_sleep_ms(400);
    unsigned samples_before = accepted_samples;
    assert(samples_before == 3840); /* Failed block contributed no prefix. */
    assert(demo_ai_audio_play(&lease, input, 320) == 0); liot_rtos_task_sleep_ms(250);
    unsigned nonzero = 0;
    for (unsigned i = 0; i < accepted_samples; ++i) if (accepted[i]) { assert(accepted[i] == 100); ++nonzero; }
    assert(nonzero == 4160); close_session(&lease);
}
static void pacing_case(void)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_DEVICE, 1, 1);
    for (unsigned i = 0; i < 20; ++i) { play_frame(&lease, (int16_t)(i + 1)); liot_rtos_task_sleep_ms(20); }
    liot_rtos_task_sleep_ms(300);
    unsigned cursor = 0;
    for (unsigned i = 0; i < accepted_samples; ++i) if (accepted[i]) { assert(accepted[i] == cursor / 320 + 1); ++cursor; }
    assert(cursor == 6400);
    for (unsigned i = 1; i < calls; ++i) assert(play_times[i] - play_times[i-1] == 40);
    close_session(&lease);
}
static void stop_case(const char *name)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_DEVICE, 4, 7);
    play_frame(&lease, 100); play_frame(&lease, 200); liot_rtos_task_sleep_ms(100);
    if (!strcmp(name, "slow-stop")) stop_delay = 5100;
    if (!strcmp(name, "stop-timeout")) freeze_worker = true;
    if (!strcmp(name, "record-stop-failure")) fail_record_stop = true;
    uint32_t start = now_ms; int ret = demo_ai_audio_stop(&lease);
    if (freeze_worker) {
        assert(ret == -40 && now_ms - start >= 10000 && codec_level == 0);
        assert(demo_ai_audio_release(&lease) == DEMO_AI_AUDIO_ERR_BUSY);
        assert(!demo_ai_audio_is_ready(&lease)); return;
    }
    if (fail_record_stop) { assert(ret == -41 && record_stops == 1 && play_stops == 0); return; }
    assert(ret == 0 && now_ms - start >= 200 + stop_delay && record_stops == 1 && play_stops == 1);
    assert(stream_starts == 2 && codec_level == 0);
    assert(demo_ai_audio_release(&lease) == 0);
    lease = open_session(DEMO_AI_AUDIO_OWNER_AI, 1, 1);
    int16_t out[320]; queue_capture(567); assert(demo_ai_audio_record_20ms(&lease, out) == 0 && out[0] == 567);
    play_frame(&lease, 777); liot_rtos_task_sleep_ms(300); close_session(&lease);
}
static void volume_case(unsigned delay, bool read_zero, bool contention)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_AI, 1, 1);
    reset_enabled = true; reset_delay = delay;
    play_frame(&lease, 100); play_frame(&lease, 200);
    liot_rtos_task_sleep_ms(85);
    if (contention) fail_mutex = true;
    if (read_zero) zero_read = true;
    liot_rtos_task_sleep_ms(400);
    assert(reset_count == 1 && calls == 3);
    if (contention) { assert(codec_level == 65); fail_mutex = false; liot_rtos_task_sleep_ms(100); }
    assert(codec_level == 30 && mic_gain == 7 && mic_volume == 160);
    unsigned writes = codec_writes;
    zero_read = false; liot_rtos_task_sleep_ms(300);
    assert(codec_writes == writes && codec_reads > 0); /* Quantization is not false drift. */
    close_session(&lease); writes = codec_writes; liot_rtos_task_sleep_ms(300);
    assert(codec_writes == writes && codec_level == 0); /* No worker unmute after Stop. */
}
static void stale_case(void)
{
    demo_ai_audio_lease_t lease = open_session(DEMO_AI_AUDIO_OWNER_DEVICE, 4, 7), old = lease;
    close_session(&lease); lease = open_session(DEMO_AI_AUDIO_OWNER_AI, 1, 1);
    int16_t input[320] = {0}; uint32_t ms = 99;
    assert(demo_ai_audio_stop(&old) == DEMO_AI_AUDIO_ERR_INVALID_LEASE);
    assert(demo_ai_audio_play(&old, input, 320) == DEMO_AI_AUDIO_ERR_INVALID_LEASE);
    assert(demo_ai_audio_discard_capture_timed(&old, &ms) == DEMO_AI_AUDIO_ERR_INVALID_LEASE && ms == 99);
    assert(demo_ai_audio_is_ready(&lease) && codec_level == 30); close_session(&lease);
}
static void fault_case(const char *name)
{
    demo_ai_audio_lease_t lease = DEMO_AI_AUDIO_LEASE_INIT;
    if (!strcmp(name, "init-failure")) fail_init = true;
    if (!strcmp(name, "startup-timeout")) no_startup = true;
    assert(demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_AI, &lease) == 0);
    int ret = demo_ai_audio_prepare_session(&lease, 1, 1);
    if (fail_init || no_startup) { assert(ret < 0 && !demo_ai_audio_is_ready(&lease)); return; }
    assert(ret == 0); fail_play = true; play_frame(&lease, 100); play_frame(&lease, 100);
    liot_rtos_task_sleep_ms(30);
    int16_t input[320] = {0}; assert(!demo_ai_audio_is_ready(&lease));
    assert(demo_ai_audio_play(&lease, input, 320) == -41); /* Hard faults are not retry-busy. */
    assert(codec_level == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2); const char *name = argv[1];
    if (!strcmp(name, "capture")) capture_case(false, false);
    else if (!strcmp(name, "capture-timeout")) capture_case(true, false);
    else if (!strcmp(name, "inflight")) capture_case(false, true);
    else if (!strcmp(name, "gain")) gain_case(false);
    else if (!strcmp(name, "mute")) gain_case(true);
    else if (!strcmp(name, "warm-contract")) warm_case();
    else if (!strcmp(name, "mic-levels")) mic_levels_case();
    else if (!strcmp(name, "backpressure")) backpressure_case();
    else if (!strcmp(name, "pacing")) pacing_case();
    else if (!strncmp(name, "volume-", 7)) volume_case((unsigned)atoi(name + 7), !strcmp(name, "volume-read-retry"), !strcmp(name, "volume-contention"));
    else if (!strcmp(name, "stale-owner")) stale_case();
    else if (!strcmp(name, "stream-failure") || !strcmp(name, "init-failure") || !strcmp(name, "startup-timeout")) fault_case(name);
    else stop_case(name);
    printf("PASS Pro AEC %s\n", name); return 0;
}
