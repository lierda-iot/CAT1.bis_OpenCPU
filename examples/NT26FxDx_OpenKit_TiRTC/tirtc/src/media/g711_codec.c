/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * ITU-T G.711 A-law conversion for signed 16-bit linear PCM.
 */

#include "g711_codec.h"

static uint8_t demo_g711_alaw_find_segment(uint16_t magnitude)
{
    static const uint16_t segment_end[8] = {
        0x001FU, 0x003FU, 0x007FU, 0x00FFU,
        0x01FFU, 0x03FFU, 0x07FFU, 0x0FFFU
    };
    uint8_t segment;

    for (segment = 0U; segment < 8U; ++segment)
    {
        if (magnitude <= segment_end[segment])
        {
            break;
        }
    }

    return segment;
}

uint8_t demo_g711_alaw_encode_sample(int16_t pcm_sample)
{
    uint16_t magnitude;
    uint8_t mask;
    uint8_t segment;
    uint8_t alaw;

    if (pcm_sample >= 0)
    {
        mask = 0xD5U;
        magnitude = (uint16_t)pcm_sample >> 3;
    }
    else
    {
        mask = 0x55U;
        magnitude = (uint16_t)(((uint32_t)(-(int32_t)pcm_sample) - 1U) >> 3);
    }

    segment = demo_g711_alaw_find_segment(magnitude);
    if (segment >= 8U)
    {
        return (uint8_t)(0x7FU ^ mask);
    }

    alaw = (uint8_t)(segment << 4);
    if (segment < 2U)
    {
        alaw |= (uint8_t)((magnitude >> 1) & 0x0FU);
    }
    else
    {
        alaw |= (uint8_t)((magnitude >> segment) & 0x0FU);
    }

    return (uint8_t)(alaw ^ mask);
}

int16_t demo_g711_alaw_decode_sample(uint8_t alaw_sample)
{
    uint16_t magnitude;
    uint8_t segment;

    alaw_sample ^= 0x55U;
    magnitude = (uint16_t)(alaw_sample & 0x0FU) << 4;
    segment = (uint8_t)((alaw_sample & 0x70U) >> 4);

    if (segment == 0U)
    {
        magnitude += 8U;
    }
    else
    {
        magnitude += 0x0108U;
        if (segment > 1U)
        {
            magnitude <<= (uint8_t)(segment - 1U);
        }
    }

    if ((alaw_sample & 0x80U) != 0U)
    {
        return (int16_t)magnitude;
    }

    return (int16_t)(-(int32_t)magnitude);
}

size_t demo_g711_alaw_encode(const int16_t *pcm,
                             uint8_t *alaw,
                             size_t sample_count)
{
    size_t i;

    if (sample_count != 0U && (pcm == NULL || alaw == NULL))
    {
        return 0U;
    }

    for (i = 0U; i < sample_count; ++i)
    {
        alaw[i] = demo_g711_alaw_encode_sample(pcm[i]);
    }

    return sample_count;
}

size_t demo_g711_alaw_decode(const uint8_t *alaw,
                             int16_t *pcm,
                             size_t sample_count)
{
    size_t i;

    if (sample_count != 0U && (alaw == NULL || pcm == NULL))
    {
        return 0U;
    }

    for (i = 0U; i < sample_count; ++i)
    {
        pcm[i] = demo_g711_alaw_decode_sample(alaw[i]);
    }

    return sample_count;
}
