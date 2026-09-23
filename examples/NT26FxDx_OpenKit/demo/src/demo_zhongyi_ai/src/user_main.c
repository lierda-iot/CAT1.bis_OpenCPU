/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Main entry: AI voice dialog (Phase 2).
 * Define AI_PHASE_A_LOOPBACK to revert to Phase A loopback test.
 */

#include "ai_app_log.h"
#include "liot_os.h"

/* #define AI_PHASE_A_LOOPBACK */

#ifdef AI_PHASE_A_LOOPBACK
#include "ai_audio.h"
#include "ai_key.h"
#include "ai_opus.h"
#include "liot_gpio2.h"
#include <string.h>

#define AI_RECORD_DURATION_MS 2000
#define AI_PCM_BUF_SIZE       (16000 * 2 * 2)
#define AI_OPUS_PKT_BUF_SIZE  (AI_OPUS_MAX_PACKET * 40)
#define AI_LOOPBACK_TASK_STACK (64 * 1024)
#define AI_LOOPBACK_TASK_PRIO  5

static liot_sem_t s_key_sem;

static void ai_key_pressed(void)
{
    if (s_key_sem != NULL) {
        liot_rtos_semaphore_release(s_key_sem);
    }
}

static void ai_loopback_task(void *arg)
{
    uint8_t *pcm_buf = NULL;
    uint8_t *opus_buf = NULL;
    int16_t *decode_buf = NULL;
    int recorded_bytes = 0;
    int total_frames = 0;
    int opus_offset = 0;
    int pcm_offset = 0;
    int i;
    (void)arg;

    liot_trace("ai_loopback task start");

    if (ai_audio_init() != 0) {
        liot_trace("ai_loopback audio init failed");
        goto exit;
    }
    if (ai_opus_init() != 0) {
        liot_trace("ai_loopback opus init failed");
        goto exit;
    }
    if (ai_key_init(ai_key_pressed) != 0) {
        liot_trace("ai_loopback key init failed");
        goto exit;
    }

    pcm_buf = liot_rtos_malloc(AI_PCM_BUF_SIZE);
    opus_buf = liot_rtos_malloc(AI_OPUS_PKT_BUF_SIZE);
    decode_buf = liot_rtos_malloc(AI_PCM_BUF_SIZE);
    if (pcm_buf == NULL || opus_buf == NULL || decode_buf == NULL) {
        liot_trace("ai_loopback malloc failed");
        goto exit;
    }

    liot_trace("ai_loopback ready, press KEY_USER0 to record");

    while (1) {
        liot_rtos_semaphore_wait(s_key_sem, LIOT_WAIT_FOREVER);
        liot_trace("ai_loopback key pressed, recording %d ms", AI_RECORD_DURATION_MS);

        recorded_bytes = ai_audio_record(pcm_buf, AI_PCM_BUF_SIZE, AI_RECORD_DURATION_MS);
        if (recorded_bytes <= 0) {
            liot_trace("ai_loopback record failed ret=%d", recorded_bytes);
            continue;
        }

        total_frames = recorded_bytes / AI_OPUS_FRAME_BYTES;
        liot_trace("ai_loopback recorded %d bytes, %d opus frames", recorded_bytes, total_frames);

        opus_offset = 0;
        typedef struct { int offset; int len; } pkt_info_t;
        pkt_info_t *pkts = liot_rtos_malloc(total_frames * sizeof(pkt_info_t));
        if (pkts == NULL) {
            continue;
        }

        for (i = 0; i < total_frames; i++) {
            int enc_len = ai_opus_encode(
                (const int16_t *)(pcm_buf + i * AI_OPUS_FRAME_BYTES),
                AI_OPUS_FRAME_SAMPLES,
                opus_buf + opus_offset,
                AI_OPUS_MAX_PACKET);
            if (enc_len <= 0) {
                break;
            }
            pkts[i].offset = opus_offset;
            pkts[i].len = enc_len;
            opus_offset += enc_len;
        }
        if (i < total_frames) {
            liot_rtos_free(pkts);
            continue;
        }
        liot_trace("ai_loopback encoded %d frames, total opus %d bytes", total_frames, opus_offset);

        pcm_offset = 0;
        for (i = 0; i < total_frames; i++) {
            int dec_samples = ai_opus_decode(
                opus_buf + pkts[i].offset, pkts[i].len,
                decode_buf + pcm_offset / 2, AI_OPUS_FRAME_SAMPLES);
            if (dec_samples <= 0) {
                break;
            }
            pcm_offset += dec_samples * 2;
        }
        liot_rtos_free(pkts);
        if (i < total_frames) {
            continue;
        }
        liot_trace("ai_loopback decoded %d bytes, playing", pcm_offset);
        ai_audio_play((const uint8_t *)decode_buf, pcm_offset, 5000);
        liot_trace("ai_loopback play done");
    }

exit:
    if (pcm_buf != NULL) liot_rtos_free(pcm_buf);
    if (opus_buf != NULL) liot_rtos_free(opus_buf);
    if (decode_buf != NULL) liot_rtos_free(decode_buf);
    liot_rtos_task_delete(NULL);
}

void user_main(void)
{
    liot_task_t task = NULL;
    ai_app_log_init();
    liot_trace("==== PHASE_A loopback ====");
    liot_rtos_semaphore_create(&s_key_sem, 0);
    liot_rtos_task_create(&task, AI_LOOPBACK_TASK_STACK, AI_LOOPBACK_TASK_PRIO,
                          "ai_loop", ai_loopback_task, NULL);
    while (1) {
        liot_rtos_task_sleep_s(10);
    }
}

#else /* Phase 2: AI Dialog */

#include "ai_dialog.h"

void user_main(void)
{
    ai_app_log_init();
    liot_trace("==== AI_DIALOG v1 ====");
    ai_dialog_start();

    while (1) {
        liot_rtos_task_sleep_s(10);
    }
}

#endif
