/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Opus codec wrapper: 16kHz mono, 60ms frames (960 samples).
 */

#include "ai_opus.h"

#include "ai_app_log.h"
#include "opus.h"
#include "liot_os.h"

#define AI_OPUS_BITRATE      24000
#define AI_OPUS_COMPLEXITY   3

static OpusEncoder  *s_encoder;
static OpusDecoder  *s_decoder;

int ai_opus_init(void)
{
    int err = 0;

    s_encoder = opus_encoder_create(AI_OPUS_SAMPLE_RATE,
                                    AI_OPUS_CHANNELS,
                                    OPUS_APPLICATION_VOIP,
                                    &err);
    if (s_encoder == NULL || err != OPUS_OK) {
        liot_trace("ai_opus encoder create failed err=%d", err);
        return -1;
    }

    opus_encoder_ctl(s_encoder, OPUS_SET_BITRATE(AI_OPUS_BITRATE));
    opus_encoder_ctl(s_encoder, OPUS_SET_COMPLEXITY(AI_OPUS_COMPLEXITY));
    opus_encoder_ctl(s_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    s_decoder = opus_decoder_create(AI_OPUS_SAMPLE_RATE,
                                    AI_OPUS_CHANNELS,
                                    &err);
    if (s_decoder == NULL || err != OPUS_OK) {
        liot_trace("ai_opus decoder create failed err=%d", err);
        opus_encoder_destroy(s_encoder);
        s_encoder = NULL;
        return -2;
    }

    liot_trace("ai_opus init ok (16kHz mono 60ms, bitrate=%d)", AI_OPUS_BITRATE);
    return 0;
}

void ai_opus_deinit(void)
{
    if (s_encoder != NULL) {
        opus_encoder_destroy(s_encoder);
        s_encoder = NULL;
    }
    if (s_decoder != NULL) {
        opus_decoder_destroy(s_decoder);
        s_decoder = NULL;
    }
}

int ai_opus_encode(const int16_t *pcm, int frame_samples, uint8_t *out, int max_out)
{
    int ret;

    if (s_encoder == NULL || pcm == NULL || out == NULL) {
        return -1;
    }
    if (frame_samples != AI_OPUS_FRAME_SAMPLES) {
        return -2;
    }

    ret = opus_encode(s_encoder, pcm, frame_samples, out, max_out);
    if (ret < 0) {
        liot_trace("ai_opus encode err=%d", ret);
        return -3;
    }

    return ret;
}

int ai_opus_decode(const uint8_t *opus_data, int opus_len, int16_t *pcm, int max_samples)
{
    int ret;

    if (s_decoder == NULL || opus_data == NULL || pcm == NULL) {
        return -1;
    }

    ret = opus_decode(s_decoder, opus_data, opus_len, pcm, max_samples, 0);
    if (ret < 0) {
        liot_trace("ai_opus decode err=%d", ret);
        return -2;
    }

    return ret;
}
