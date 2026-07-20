/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * AI voice dialog: sequential state machine.
 * Flow: net → token → ws_connect → idle → key → record+upload → recv TTS → play → idle
 */

#include "ai_dialog.h"

#include <stdio.h>
#include <string.h>

#include "ai_app_config.h"
#include "ai_app_log.h"
#include "ai_audio.h"
#include "ai_http_token.h"
#include "ai_key.h"
#include "ai_net.h"
#include "ai_opus.h"
#include "ai_protocol.h"
#include "ai_ws_client.h"
#include "liot_audio2.h"
#include "liot_os.h"

#define AI_DIALOG_TASK_STACK    (96 * 1024)
#define AI_DIALOG_TASK_PRIO     5
#define AI_DIALOG_RECORD_MAX_MS 15000
#define AI_DIALOG_FRAME_MS      60
#define AI_DIALOG_MAX_FRAMES    (AI_DIALOG_RECORD_MAX_MS / AI_DIALOG_FRAME_MS)
#define AI_DIALOG_RECV_BUF_SIZE 2048
#define AI_DIALOG_RECV_TIMEOUT  30000
#define AI_DIALOG_WAKE_WORD     "\xe7\x81\xb5\xe7\x8a\x80\xe7\x81\xb5\xe7\x8a\x80"

#define AI_AUDIO_SAMPLE_RATE    16000
#define AI_STREAM_PCM_FRAMES    10
#define AI_STREAM_PCM_BUF_SIZE  (AI_OPUS_FRAME_SAMPLES * 2 * AI_STREAM_PCM_FRAMES)

#define AI_DIALOG_RET_OK        0
#define AI_DIALOG_RET_INTERRUPT 1

static liot_sem_t s_key_sem;
static liot_task_t s_dialog_task;

#define AI_DIALOG_BEEP_FREQ     1000
#define AI_DIALOG_BEEP_MS       300
#define AI_DIALOG_BEEP_SAMPLES  (AI_AUDIO_SAMPLE_RATE * AI_DIALOG_BEEP_MS / 1000)
#define AI_DIALOG_BEEP_PERIOD   (AI_AUDIO_SAMPLE_RATE / AI_DIALOG_BEEP_FREQ)

static void ai_dialog_play_beep(void)
{
    static const int16_t s_sin_table[16] = {
        0, 3827, 7071, 9239, 10000, 9239, 7071, 3827,
        0, -3827, -7071, -9239, -10000, -9239, -7071, -3827
    };
    int16_t *buf = liot_rtos_malloc(AI_DIALOG_BEEP_SAMPLES * 2);
    if (buf == NULL) {
        return;
    }
    for (int i = 0; i < AI_DIALOG_BEEP_SAMPLES; i++) {
        buf[i] = s_sin_table[i % AI_DIALOG_BEEP_PERIOD];
    }
    ai_audio_play_start((const uint8_t *)buf, AI_DIALOG_BEEP_SAMPLES * 2);
    ai_audio_play_wait(AI_DIALOG_BEEP_MS + 500);
    liot_rtos_free(buf);
}

static void ai_dialog_key_cb(void)
{
    if (s_key_sem != NULL) {
        liot_rtos_semaphore_release(s_key_sem);
    }
}

static int ai_dialog_do_connect(const ai_app_config_t *cfg,
                                ai_ws_client_result_t *ws_result)
{
    ai_http_token_result_t token_result;
    const char *ws_token = NULL;
    int ret;

    memset(&token_result, 0, sizeof(token_result));

    ret = ai_net_start(cfg);
    if (ret != 0) {
        liot_trace("ai_dialog net failed ret=%d", ret);
        return -1;
    }

    ret = ai_http_fetch_token(cfg, &token_result);
    if (ret != 0) {
        liot_trace("ai_dialog token failed ret=%d", ret);
        if (cfg->allow_preset_token_fallback &&
            !ai_app_config_string_empty(cfg->preset_ai_token)) {
            liot_trace("ai_dialog using preset token");
            ws_token = cfg->preset_ai_token;
        } else {
            return -2;
        }
    } else {
        ws_token = token_result.ai_token;
    }

    ret = ai_ws_client_connect(cfg, ws_token, ws_result);
    if (ret != 0) {
        liot_trace("ai_dialog ws connect failed ret=%d", ret);
        return -3;
    }

    liot_trace("ai_dialog connected session=%s", ws_result->session_id);
    return 0;
}

static int ai_dialog_do_record_upload(const char *session_id)
{
    int16_t *pcm_frame = NULL;
    uint8_t opus_pkt[AI_OPUS_MAX_PACKET];
    char msg_buf[256];
    int msg_len;
    int ret = -1;
    int enc_len;
    int rec_bytes;
    int i;

    pcm_frame = liot_rtos_malloc(AI_OPUS_FRAME_BYTES);
    if (pcm_frame == NULL) {
        return -1;
    }

    /* Send wake */
    msg_len = snprintf(msg_buf, sizeof(msg_buf),
        "{\"type\":\"listen\",\"state\":\"wake\",\"text\":\"%s\","
        "\"session_id\":\"%s\",\"wake_reply\":false}",
        AI_DIALOG_WAKE_WORD, session_id);
    if (msg_len <= 0 || (size_t)msg_len >= sizeof(msg_buf)) {
        goto exit;
    }
    if (ai_ws_client_send_text(msg_buf) != 0) {
        liot_trace("ai_dialog send wake failed");
        goto exit;
    }
    liot_trace("ai_dialog sent wake");

    /* Send listen start */
    msg_len = snprintf(msg_buf, sizeof(msg_buf),
        "{\"type\":\"listen\",\"state\":\"start\",\"mode\":\"auto\","
        "\"session_id\":\"%s\"}", session_id);
    if (msg_len <= 0 || (size_t)msg_len >= sizeof(msg_buf)) {
        goto exit;
    }
    if (ai_ws_client_send_text(msg_buf) != 0) {
        liot_trace("ai_dialog send listen_start failed");
        goto exit;
    }
    liot_trace("ai_dialog recording (max %dms, VAD stop)", AI_DIALOG_RECORD_MAX_MS);

    /* Record frame by frame until STT end (cloud VAD) or max duration */
    for (i = 0; i < AI_DIALOG_MAX_FRAMES; i++) {
        rec_bytes = ai_audio_record((uint8_t *)pcm_frame, AI_OPUS_FRAME_BYTES, AI_DIALOG_FRAME_MS);
        if (rec_bytes != AI_OPUS_FRAME_BYTES) {
            liot_trace("ai_dialog record frame %d failed ret=%d", i, rec_bytes);
            goto exit;
        }

        enc_len = ai_opus_encode(pcm_frame, AI_OPUS_FRAME_SAMPLES, opus_pkt, AI_OPUS_MAX_PACKET);
        if (enc_len <= 0) {
            liot_trace("ai_dialog encode frame %d failed", i);
            goto exit;
        }

        if (ai_ws_client_send_binary(opus_pkt, enc_len) != 0) {
            liot_trace("ai_dialog send binary frame %d failed", i);
            goto exit;
        }

        if ((i % 10) == 0) {
            liot_trace("ai_dialog frame %d/%d enc=%d", i, AI_DIALOG_MAX_FRAMES, enc_len);
        }

        /* Non-blocking check for server STT end (cloud VAD stop) */
        if (ai_ws_client_has_data()) {
            uint8_t peek_buf[512];
            int peek_opcode = 0;
            int peek_len = ai_ws_client_recv_frame(peek_buf, sizeof(peek_buf),
                                                   &peek_opcode, 0);
            if (peek_len > 0 && peek_opcode == AI_WS_OPCODE_TEXT) {
                peek_buf[peek_len] = '\0';
                if (ai_protocol_is_stt_end((const char *)peek_buf, (size_t)peek_len)) {
                    liot_trace("ai_dialog VAD end at frame %d (%dms)", i, i * AI_DIALOG_FRAME_MS);
                    break;
                }
            }
        }
    }

    /* Send listen stop */
    msg_len = snprintf(msg_buf, sizeof(msg_buf),
        "{\"type\":\"listen\",\"state\":\"stop\",\"session_id\":\"%s\"}",
        session_id);
    if (msg_len <= 0 || (size_t)msg_len >= sizeof(msg_buf)) {
        goto exit;
    }
    if (ai_ws_client_send_text(msg_buf) != 0) {
        liot_trace("ai_dialog send listen_stop failed");
        goto exit;
    }
    liot_trace("ai_dialog listen stop sent (recorded %d frames, %dms)",
               i + 1, (i + 1) * AI_DIALOG_FRAME_MS);

    ret = 0;

exit:
    liot_rtos_free(pcm_frame);
    return ret;
}

static bool ai_dialog_key_pressed(liot_sem_t sem)
{
    return (liot_rtos_semaphore_wait(sem, 0) == LIOT_OSI_SUCCESS);
}

static void ai_dialog_play_wait_or_interrupt(liot_sem_t key_sem, int play_ms,
                                             bool *interrupted)
{
    int elapsed = 0;
    while (elapsed < play_ms) {
        if (ai_dialog_key_pressed(key_sem)) {
            *interrupted = true;
            return;
        }
        Liot_AudErr_e ret = Liot_AudioWaitPlayFinish(1);
        if (ret == L_AUD_ERR_SUCCESS) {
            return;
        }
        elapsed += 1000;
    }
}

static int ai_dialog_do_stream_tts(liot_sem_t key_sem)
{
    uint8_t *recv_buf = NULL;
    int16_t *pcm_buf[2] = {NULL, NULL};
    int pcm_samples[2] = {0, 0};
    int cur_buf = 0;
    int opcode = 0;
    int frame_len = 0;
    int dec_samples;
    int recv_timeout = AI_DIALOG_RECV_TIMEOUT;
    int total_frames = 0;
    int last_play_samples = 0;
    bool playing = false;
    bool got_tts_stop __attribute__((unused)) = false;
    bool interrupted = false;
    int ret = -1;

    recv_buf = liot_rtos_malloc(AI_DIALOG_RECV_BUF_SIZE);
    pcm_buf[0] = liot_rtos_malloc(AI_STREAM_PCM_BUF_SIZE);
    pcm_buf[1] = liot_rtos_malloc(AI_STREAM_PCM_BUF_SIZE);
    if (recv_buf == NULL || pcm_buf[0] == NULL || pcm_buf[1] == NULL) {
        liot_trace("ai_dialog stream malloc failed");
        goto exit;
    }

    liot_trace("ai_dialog waiting tts (stream)...");

    while (1) {
        /* Check for key interrupt between frames */
        if (ai_dialog_key_pressed(key_sem)) {
            interrupted = true;
            liot_trace("ai_dialog interrupted during recv");
            break;
        }

        frame_len = ai_ws_client_recv_frame(recv_buf, AI_DIALOG_RECV_BUF_SIZE,
                                            &opcode, recv_timeout);
        if (frame_len < 0) {
            liot_trace("ai_dialog stream recv end, total_frames=%d", total_frames);
            ret = (total_frames > 0) ? AI_DIALOG_RET_OK : -1;
            break;
        }

        if (opcode == AI_WS_OPCODE_TEXT) {
            recv_buf[frame_len] = '\0';
            if (ai_protocol_is_tts_start((const char *)recv_buf, (size_t)frame_len)) {
                liot_trace("ai_dialog tts_start (stream)");
            } else if (ai_protocol_is_tts_stop((const char *)recv_buf, (size_t)frame_len)) {
                got_tts_stop = true;
                recv_timeout = 5000;
                liot_trace("ai_dialog tts_stop (stream), frames so far=%d", total_frames);
            } else {
                liot_trace("ai_dialog text: %.60s", recv_buf);
            }
            continue;
        }

        if (opcode != AI_WS_OPCODE_BINARY) {
            continue;
        }

        /* Binary frame: decode opus → PCM into current buffer */
        recv_timeout = 3000;
        dec_samples = ai_opus_decode(recv_buf, frame_len,
                                     pcm_buf[cur_buf] + pcm_samples[cur_buf],
                                     AI_OPUS_FRAME_SAMPLES);
        if (dec_samples <= 0) {
            continue;
        }
        pcm_samples[cur_buf] += dec_samples;
        total_frames++;

        /* Check if current buffer is full enough to submit for playback */
        if (pcm_samples[cur_buf] >= AI_STREAM_PCM_FRAMES * AI_OPUS_FRAME_SAMPLES) {
            /* If previous play is still running, wait for it (with interrupt check) */
            if (playing) {
                int wait_ms = last_play_samples * 1000 / AI_AUDIO_SAMPLE_RATE + 50;
                ai_dialog_play_wait_or_interrupt(key_sem, wait_ms, &interrupted);
                if (interrupted) {
                    liot_trace("ai_dialog interrupted during play_wait");
                    break;
                }
            }

            /* Start playing current buffer */
            ai_audio_play_start((const uint8_t *)pcm_buf[cur_buf],
                                pcm_samples[cur_buf] * 2);
            last_play_samples = pcm_samples[cur_buf];
            playing = true;
            liot_trace("ai_dialog stream play buf%d %d samples", cur_buf, pcm_samples[cur_buf]);

            /* Switch to other buffer */
            cur_buf = 1 - cur_buf;
            pcm_samples[cur_buf] = 0;
        }
    }

    if (interrupted) {
        ai_audio_stop();
        liot_trace("ai_dialog stream interrupted, total %d frames", total_frames);
        ret = AI_DIALOG_RET_INTERRUPT;
        goto exit;
    }

    /* Play any remaining samples in current buffer */
    if (pcm_samples[cur_buf] > 0) {
        if (playing) {
            int wait_ms = last_play_samples * 1000 / AI_AUDIO_SAMPLE_RATE + 50;
            ai_dialog_play_wait_or_interrupt(key_sem, wait_ms, &interrupted);
            if (interrupted) {
                ai_audio_stop();
                ret = AI_DIALOG_RET_INTERRUPT;
                goto exit;
            }
        }
        ai_audio_play_start((const uint8_t *)pcm_buf[cur_buf],
                            pcm_samples[cur_buf] * 2);
        last_play_samples = pcm_samples[cur_buf];
        playing = true;
        liot_trace("ai_dialog stream play final buf%d %d samples", cur_buf, pcm_samples[cur_buf]);
    }

    /* Wait for last play to finish (with interrupt check) */
    if (playing) {
        int wait_ms = last_play_samples * 1000 / AI_AUDIO_SAMPLE_RATE + 50;
        ai_dialog_play_wait_or_interrupt(key_sem, wait_ms, &interrupted);
        if (interrupted) {
            ai_audio_stop();
            ret = AI_DIALOG_RET_INTERRUPT;
            goto exit;
        }
        liot_rtos_task_sleep_ms(100);
    }

    liot_trace("ai_dialog stream done, total %d frames", total_frames);
    ret = AI_DIALOG_RET_OK;

exit:
    if (recv_buf != NULL) liot_rtos_free(recv_buf);
    if (pcm_buf[0] != NULL) liot_rtos_free(pcm_buf[0]);
    if (pcm_buf[1] != NULL) liot_rtos_free(pcm_buf[1]);
    return ret;
}

static void ai_dialog_task_fn(void *arg)
{
    const ai_app_config_t *cfg = NULL;
    ai_ws_client_result_t ws_result;
    char err[128] = {0};
    int ret;
    (void)arg;

    liot_trace("ai_dialog task start");

    /* Init hardware */
    if (ai_audio_init() != 0) {
        liot_trace("ai_dialog audio init failed");
        goto exit;
    }
    if (ai_opus_init() != 0) {
        liot_trace("ai_dialog opus init failed");
        goto exit;
    }
    if (ai_key_init(ai_dialog_key_cb) != 0) {
        liot_trace("ai_dialog key init failed");
        goto exit;
    }

    cfg = ai_app_config_get();
    if (!ai_app_config_validate(cfg, err, sizeof(err))) {
        liot_trace("ai_dialog config invalid: %s", err);
        goto exit;
    }

    /* Connect */
    memset(&ws_result, 0, sizeof(ws_result));
    ret = ai_dialog_do_connect(cfg, &ws_result);
    if (ret != 0) {
        liot_trace("ai_dialog connect failed ret=%d", ret);
        goto exit;
    }

    ai_dialog_play_beep();

    /* Main loop: wait key → record → upload → stream TTS playback */
    while (1) {
        liot_trace("ai_dialog idle, press KEY_USER0");
        liot_rtos_semaphore_wait(s_key_sem, LIOT_WAIT_FOREVER);

dialog_round:
        if (!ai_ws_client_is_connected()) {
            liot_trace("ai_dialog ws disconnected, reconnecting...");
            ai_ws_client_close();
            memset(&ws_result, 0, sizeof(ws_result));
            ret = ai_dialog_do_connect(cfg, &ws_result);
            if (ret != 0) {
                liot_trace("ai_dialog reconnect failed ret=%d", ret);
                liot_rtos_task_sleep_ms(3000);
                continue;
            }
        }

        /* Record and upload */
        ret = ai_dialog_do_record_upload(ws_result.session_id);
        if (ret != 0) {
            liot_trace("ai_dialog record/upload failed ret=%d", ret);
            continue;
        }

        /* Stream TTS: receive + decode + play concurrently */
        ret = ai_dialog_do_stream_tts(s_key_sem);
        if (ret == AI_DIALOG_RET_INTERRUPT) {
            liot_trace("ai_dialog interrupted, sending abort and starting new round");
            /* Notify platform to cancel current interaction */
            {
                char abort_buf[128];
                int abort_len = snprintf(abort_buf, sizeof(abort_buf),
                    "{\"type\":\"listen\",\"state\":\"cancel\",\"session_id\":\"%s\"}",
                    ws_result.session_id);
                if (abort_len > 0 && (size_t)abort_len < sizeof(abort_buf)) {
                    ai_ws_client_send_text(abort_buf);
                }
            }
            /* Drain remaining frames from server (frame-aligned) until quiet */
            {
                uint8_t *drain_buf = liot_rtos_malloc(AI_DIALOG_RECV_BUF_SIZE);
                if (drain_buf != NULL) {
                    int drain_opcode;
                    int drain_count = 0;
                    while (drain_count < 300) {
                        int dlen = ai_ws_client_recv_frame(drain_buf,
                                       AI_DIALOG_RECV_BUF_SIZE, &drain_opcode, 500);
                        if (dlen < 0) {
                            break;
                        }
                        drain_count++;
                    }
                    liot_rtos_free(drain_buf);
                    liot_trace("ai_dialog drained %d frames", drain_count);
                }
            }
            goto dialog_round;
        }
        if (ret != 0) {
            liot_trace("ai_dialog stream tts failed ret=%d", ret);
        }
        liot_trace("ai_dialog round complete");
    }

exit:
    ai_ws_client_close();
    s_dialog_task = NULL;
    liot_trace("ai_dialog task exit");
    liot_rtos_task_delete(NULL);
}

int ai_dialog_start(void)
{
    if (s_dialog_task != NULL) {
        return 0;
    }

    liot_rtos_semaphore_create(&s_key_sem, 0);

    if (liot_rtos_task_create(&s_dialog_task,
                              AI_DIALOG_TASK_STACK,
                              AI_DIALOG_TASK_PRIO,
                              "ai_dlg",
                              ai_dialog_task_fn,
                              NULL) != LIOT_OSI_SUCCESS) {
        s_dialog_task = NULL;
        liot_trace("ai_dialog task create failed");
        return -1;
    }

    return 0;
}
