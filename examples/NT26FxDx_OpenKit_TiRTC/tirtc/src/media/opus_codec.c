/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Opus 16 kHz mono/20 ms adapter for low-bandwidth TiRTC AI audio.
 */

#include "opus_codec.h"

#include <stddef.h>

#include "liot_log.h"
#include "opus.h"

#define DEMO_AI_OPUS_COMPLEXITY  3

static OpusEncoder *s_encoder;
static OpusDecoder *s_decoder;

int demo_ai_opus_init(void)
{
    int error;
    int ret;

    if (s_encoder != NULL && s_decoder != NULL)
    {
        return 0;
    }

    demo_ai_opus_deinit();

    error = OPUS_OK;
    s_encoder = opus_encoder_create(DEMO_AI_OPUS_SAMPLE_RATE,
                                    DEMO_AI_OPUS_CHANNELS,
                                    OPUS_APPLICATION_VOIP,
                                    &error);
    if (s_encoder == NULL || error != OPUS_OK)
    {
        s_encoder = NULL;
        liot_trace("[AI-OPUS] encoder create failed ret=%d\r\n", error);
        return (error < 0) ? error : OPUS_ALLOC_FAIL;
    }

    ret = opus_encoder_ctl(s_encoder,
                           OPUS_SET_BITRATE(DEMO_AI_OPUS_BITRATE));
    if (ret == OPUS_OK)
    {
        ret = opus_encoder_ctl(s_encoder,
                               OPUS_SET_COMPLEXITY(DEMO_AI_OPUS_COMPLEXITY));
    }
    if (ret == OPUS_OK)
    {
        ret = opus_encoder_ctl(s_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    }
    if (ret != OPUS_OK)
    {
        liot_trace("[AI-OPUS] encoder config failed ret=%d\r\n", ret);
        demo_ai_opus_deinit();
        return ret;
    }

    error = OPUS_OK;
    s_decoder = opus_decoder_create(DEMO_AI_OPUS_SAMPLE_RATE,
                                    DEMO_AI_OPUS_CHANNELS,
                                    &error);
    if (s_decoder == NULL || error != OPUS_OK)
    {
        s_decoder = NULL;
        liot_trace("[AI-OPUS] decoder create failed ret=%d\r\n", error);
        demo_ai_opus_deinit();
        return (error < 0) ? error : OPUS_ALLOC_FAIL;
    }

    liot_trace("[AI-OPUS] ready 16kHz/mono/20ms 24kbps complexity=%d\r\n",
               DEMO_AI_OPUS_COMPLEXITY);
    return 0;
}

void demo_ai_opus_reset(void)
{
    if (s_encoder != NULL)
    {
        (void)opus_encoder_ctl(s_encoder, OPUS_RESET_STATE);
    }
    if (s_decoder != NULL)
    {
        (void)opus_decoder_ctl(s_decoder, OPUS_RESET_STATE);
    }
}

void demo_ai_opus_deinit(void)
{
    if (s_encoder != NULL)
    {
        opus_encoder_destroy(s_encoder);
        s_encoder = NULL;
    }
    if (s_decoder != NULL)
    {
        opus_decoder_destroy(s_decoder);
        s_decoder = NULL;
    }
}

int demo_ai_opus_encode_20ms(
    const int16_t pcm[DEMO_AI_OPUS_FRAME_SAMPLES],
    uint8_t *packet,
    int packet_capacity)
{
    if (s_encoder == NULL || pcm == NULL || packet == NULL ||
        packet_capacity <= 0)
    {
        return OPUS_BAD_ARG;
    }

    return opus_encode(s_encoder,
                       pcm,
                       DEMO_AI_OPUS_FRAME_SAMPLES,
                       packet,
                       packet_capacity);
}

int demo_ai_opus_decode(const uint8_t *packet,
                        int packet_size,
                        int16_t *pcm,
                        int pcm_capacity_samples)
{
    if (s_decoder == NULL || packet == NULL || packet_size <= 0 ||
        pcm == NULL || pcm_capacity_samples <= 0)
    {
        return OPUS_BAD_ARG;
    }

    return opus_decode(s_decoder,
                       packet,
                       packet_size,
                       pcm,
                       pcm_capacity_samples,
                       0);
}
