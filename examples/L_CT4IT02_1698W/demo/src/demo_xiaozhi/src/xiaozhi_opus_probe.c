#include "xiaozhi_core.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "liot_audio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "opus.h"

static OpusEncoder *g_probe_encoder;
static OpusDecoder *g_probe_decoder;
static struct {
    uint32_t guard_before[4];
    int16_t pcm[2560];
    uint32_t guard_after[4];
} __attribute__((aligned(16))) g_probe_record_buffer;
static unsigned int g_capture_session;
static unsigned int g_capture_frame;
static unsigned int g_pcm_pending;
static __attribute__((aligned(16))) int16_t g_pcm_pending_buffer[4096];
static __attribute__((aligned(16))) int16_t g_tts_decode_buffer[960];
static __attribute__((aligned(16))) uint8_t g_capture_pcm_block[5120];

#define XZ_TTS_SLOT_COUNT 3
#define XZ_TTS_SLOT_BYTES (16 * 1024)
#define XZ_TTS_WARMUP_BYTES 6400

typedef enum {
    XZ_TTS_SLOT_FREE = 0,
    XZ_TTS_SLOT_FILLING,
    XZ_TTS_SLOT_READY,
    XZ_TTS_SLOT_PLAYING
} xz_tts_slot_state_t;

typedef struct {
    __attribute__((aligned(16))) uint8_t pcm[XZ_TTS_SLOT_BYTES];
    volatile unsigned int bytes;
    volatile xz_tts_slot_state_t state;
} xz_tts_slot_t;

static xz_tts_slot_t g_tts_slots[XZ_TTS_SLOT_COUNT];
static int g_tts_fill_slot = -1;
static liot_sem_t g_tts_ready_sem;
static liot_task_t g_tts_play_task;
static unsigned int g_tts_dropped_frames;
static volatile bool g_tts_warmup_pending;
static volatile bool g_tts_aborted;

#define XZ_PCM_GUARD_BEFORE 0x13579BDFu
#define XZ_PCM_GUARD_AFTER  0x2468ACE0u

static int xz_tts_find_free_slot(void)
{
    int i;
    for (i = 0; i < XZ_TTS_SLOT_COUNT; ++i) {
        if (g_tts_slots[i].state == XZ_TTS_SLOT_FREE) {
            g_tts_slots[i].bytes = 0;
            g_tts_slots[i].state = XZ_TTS_SLOT_FILLING;
            return i;
        }
    }
    return -1;
}

static void xz_tts_submit_fill_slot(void)
{
    int slot = g_tts_fill_slot;
    if (slot < 0 || g_tts_slots[slot].bytes == 0) return;
    g_tts_slots[slot].state = XZ_TTS_SLOT_READY;
    g_tts_fill_slot = -1;
    if (g_tts_ready_sem != NULL)
        liot_rtos_semaphore_release(g_tts_ready_sem);
}

static void xz_tts_play_thread(void *argument)
{
    int i;
    Liot_AudErr_e ret;
    (void)argument;
    for (;;) {
        liot_rtos_semaphore_wait(g_tts_ready_sem, LIOT_WAIT_FOREVER);
        for (i = 0; i < XZ_TTS_SLOT_COUNT; ++i) {
            if (g_tts_slots[i].state != XZ_TTS_SLOT_READY) continue;
            g_tts_slots[i].state = XZ_TTS_SLOT_PLAYING;
            liot_trace("[xiaozhi] TTS stream play slot=%d bytes=%u",
                       i, g_tts_slots[i].bytes);
            ret = Liot_AudioPlay(g_tts_slots[i].pcm, g_tts_slots[i].bytes);
            if (ret == L_AUD_ERR_SUCCESS)
                ret = Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
            liot_trace("[xiaozhi] TTS stream finish slot=%d ret=%d",
                       i, (int)ret);
            g_tts_slots[i].bytes = 0;
            g_tts_slots[i].state = XZ_TTS_SLOT_FREE;
        }
    }
}

static Liot_AudErr_e xz_audio_record_pcm(void)
{
    Liot_AudErr_e ret;
    unsigned int i;

    for (i = 0; i < 4; ++i) {
        g_probe_record_buffer.guard_before[i] = XZ_PCM_GUARD_BEFORE;
        g_probe_record_buffer.guard_after[i] = XZ_PCM_GUARD_AFTER;
    }
    ret = Liot_AudioRecord((uint8_t *)g_probe_record_buffer.pcm,
                           sizeof(g_probe_record_buffer.pcm));
    for (i = 0; i < 4; ++i) {
        if (g_probe_record_buffer.guard_before[i] != XZ_PCM_GUARD_BEFORE ||
            g_probe_record_buffer.guard_after[i] != XZ_PCM_GUARD_AFTER) {
            liot_trace("[xiaozhi] FATAL audio record buffer overflow index=%u before=%08x after=%08x",
                       i, g_probe_record_buffer.guard_before[i],
                       g_probe_record_buffer.guard_after[i]);
            return L_AUD_ERR_EXECUTE;
        }
    }
    return ret;
}

void xiaozhi_audio_capture_session_begin(void)
{
    ++g_capture_session;
    g_capture_frame = 0;
    g_pcm_pending = 0;
    if (g_probe_encoder != NULL)
        opus_encoder_ctl(g_probe_encoder, OPUS_RESET_STATE);
    xiaozhi_capture_begin();
    liot_trace("[xiaozhi] capture session=%u begin", g_capture_session);
}

void xiaozhi_audio_capture_session_end(void)
{
    xiaozhi_capture_end();
}

int xiaozhi_opus_probe_init(void)
{
    int error = OPUS_OK;
    int ret;

    liot_trace("[xiaozhi] opus probe encoder create begin");
    g_probe_encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &error);
    liot_trace("[xiaozhi] opus probe encoder=%p error=%d",
               g_probe_encoder, error);
    if (g_probe_encoder == NULL || error != OPUS_OK) return -1;
    opus_encoder_ctl(g_probe_encoder, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(g_probe_encoder, OPUS_SET_COMPLEXITY(3));
    opus_encoder_ctl(g_probe_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    liot_trace("[xiaozhi] opus probe decoder create begin");
    g_probe_decoder = opus_decoder_create(16000, 1, &error);
    liot_trace("[xiaozhi] opus probe decoder=%p error=%d",
               g_probe_decoder, error);
    if (g_probe_decoder == NULL || error != OPUS_OK) return -2;
    ret = xiaozhi_capture_init();
    if (ret != 0) return -3;
    ret = liot_rtos_semaphore_create_ex(&g_tts_ready_sem, 0,
                                        XZ_TTS_SLOT_COUNT);
    if (ret != LIOT_OSI_SUCCESS) return -4;
    ret = liot_rtos_task_create(&g_tts_play_task, 16 * 1024,
                                LIOT_APP_TASK_PRIORITY,
                                "xiaozhi_tts", xz_tts_play_thread, NULL);
    liot_trace("[xiaozhi] TTS stream task create ret=%d handle=%p",
               ret, g_tts_play_task);
    if (ret != LIOT_OSI_SUCCESS) return -5;
    liot_trace("[xiaozhi] opus probe ready rate=16000 channels=1 frame_ms=60");
    return 0;
}

int xiaozhi_audio_capture_opus(uint8_t *opus_data, int capacity)
{
    int encoded;
    if (g_probe_encoder == NULL || opus_data == NULL || capacity <= 0) return -1;
    ++g_capture_frame;
    if (g_capture_frame <= 3)
        liot_trace("[xiaozhi] capture s=%u frame=%u record begin",
                   g_capture_session, g_capture_frame);
    while (g_pcm_pending < 960) {
        if (xiaozhi_capture_read(g_capture_pcm_block,
                                 sizeof(g_capture_pcm_block),
                                 LIOT_WAIT_FOREVER) != 0) return -2;
        memcpy(g_pcm_pending_buffer + g_pcm_pending,
               g_capture_pcm_block,
               sizeof(g_probe_record_buffer.pcm));
        g_pcm_pending += 2560;
        if (g_capture_frame <= 3)
            liot_trace("[xiaozhi] capture s=%u frame=%u record ret=%d block=5120 pending=%u",
                       g_capture_session, g_capture_frame, 0,
                       g_pcm_pending);
    }
    encoded = opus_encode(g_probe_encoder, g_pcm_pending_buffer, 960,
                          opus_data, capacity);
    memmove(g_pcm_pending_buffer,
            g_pcm_pending_buffer + 960,
            (g_pcm_pending - 960) * sizeof(int16_t));
    g_pcm_pending -= 960;
    if (g_capture_frame <= 3)
        liot_trace("[xiaozhi] capture s=%u frame=%u opus ret=%d",
                   g_capture_session, g_capture_frame, encoded);
    return encoded > 0 ? encoded : -3;
}

int xiaozhi_audio_capture_flush_opus(uint8_t *opus_data, int capacity)
{
    int encoded;
    if (g_probe_encoder == NULL || opus_data == NULL || capacity <= 0) return -1;
    while (g_pcm_pending < 960) {
        if (xiaozhi_capture_read_nowait(g_capture_pcm_block,
                                        sizeof(g_capture_pcm_block)) != 0)
            return 0;
        memcpy(g_pcm_pending_buffer + g_pcm_pending, g_capture_pcm_block,
               sizeof(g_probe_record_buffer.pcm));
        g_pcm_pending += 2560;
    }
    encoded = opus_encode(g_probe_encoder, g_pcm_pending_buffer, 960,
                          opus_data, capacity);
    memmove(g_pcm_pending_buffer, g_pcm_pending_buffer + 960,
            (g_pcm_pending - 960) * sizeof(int16_t));
    g_pcm_pending -= 960;
    return encoded > 0 ? encoded : -3;
}

int xiaozhi_audio_capture_discard(void)
{
    Liot_AudErr_e record_ret;

    record_ret = xz_audio_record_pcm();
    return record_ret == L_AUD_ERR_SUCCESS ? 0 : -1;
}

int xiaozhi_audio_decode_append(const uint8_t *opus_data, int length)
{
    int samples;
    unsigned int bytes;
    xz_tts_slot_t *slot;

    if (g_tts_aborted) return 0;
    if (g_probe_decoder == NULL || opus_data == NULL || length <= 0) return -1;
    samples = opus_decode(g_probe_decoder, opus_data, length,
                          g_tts_decode_buffer, 960, 0);
    if (samples <= 0) return -2;
    bytes = (unsigned int)samples * sizeof(int16_t);
    if (g_tts_fill_slot < 0) g_tts_fill_slot = xz_tts_find_free_slot();
    if (g_tts_fill_slot < 0) {
        ++g_tts_dropped_frames;
        return samples;
    }
    slot = &g_tts_slots[g_tts_fill_slot];
    if (g_tts_warmup_pending && slot->bytes == 0) {
        memset(slot->pcm, 0, XZ_TTS_WARMUP_BYTES);
        slot->bytes = XZ_TTS_WARMUP_BYTES;
        g_tts_warmup_pending = false;
        liot_trace("[xiaozhi] TTS first-block warmup bytes=%u",
                   (unsigned int)XZ_TTS_WARMUP_BYTES);
    }
    if (slot->bytes + bytes > XZ_TTS_SLOT_BYTES) {
        xz_tts_submit_fill_slot();
        g_tts_fill_slot = xz_tts_find_free_slot();
        if (g_tts_fill_slot < 0) {
            ++g_tts_dropped_frames;
            return samples;
        }
        slot = &g_tts_slots[g_tts_fill_slot];
    }
    memcpy(slot->pcm + slot->bytes, g_tts_decode_buffer, bytes);
    slot->bytes += bytes;
    return samples;
}

void xiaozhi_audio_tts_reset(void)
{
    g_tts_aborted = false;
    g_tts_dropped_frames = 0;
    g_tts_warmup_pending = true;
    if (g_probe_decoder != NULL)
        opus_decoder_ctl(g_probe_decoder, OPUS_RESET_STATE);
}

void xiaozhi_audio_tts_abort(void)
{
    int i;
    g_tts_aborted = true;
    g_tts_fill_slot = -1;
    (void)Liot_AudioStop();
    for (i = 0; i < XZ_TTS_SLOT_COUNT; ++i) {
        g_tts_slots[i].bytes = 0;
        g_tts_slots[i].state = XZ_TTS_SLOT_FREE;
    }
    if (g_probe_decoder != NULL)
        opus_decoder_ctl(g_probe_decoder, OPUS_RESET_STATE);
    liot_trace("[xiaozhi] TTS aborted and queue cleared");
}

int xiaozhi_audio_tts_play(void)
{
    xz_tts_submit_fill_slot();
    liot_trace("[xiaozhi] TTS stream flush dropped_frames=%u",
               g_tts_dropped_frames);
    return 0;
}
