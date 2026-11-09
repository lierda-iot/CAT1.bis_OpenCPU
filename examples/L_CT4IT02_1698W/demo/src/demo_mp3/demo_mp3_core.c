#include "demo_mp3_core.h"

#include <stdio.h>

uint32_t mp3_seed_size(const uint8_t *start, const uint8_t *end)
{
    return (uint32_t)(end - start);
}

/*
 * Locate the first MPEG audio frame sync word and skip a leading ID3v2 tag
 * (and any junk before it). The -12dB source starts with "ID3..." at offset 0,
 * so feeding from offset 0 would make libmad resync mid-tag. Skip to the first
 * 0xFF 0xEx sync.
 */
uint32_t mp3_frame_offset(const mp3_track_t *track)
{
    const uint8_t *p = track->mp3_start;
    uint32_t size = mp3_seed_size(track->mp3_start, track->mp3_end);
    uint32_t i;

    for (i = 0; i + 1U < size; i++) {
        if (p[i] == 0xFFU && (p[i + 1U] & 0xE0U) == 0xE0U) {
            return i;
        }
    }
    return 0;
}

/* MPEG audio sample-rate lookup: [MPEG1 / MPEG2] x sampling_freq_index. */
uint32_t mp3_sample_rate(uint32_t version, uint32_t idx)
{
    static const uint32_t table[2][4] = {
        { 44100U, 48000U, 32000U, 0U }, /* MPEG1 */
        { 22050U, 24000U, 16000U, 0U }, /* MPEG2 / MPEG2.5 */
    };

    if (version > 3U || idx > 3U) {
        return 0U;
    }
    return table[(version == 3U) ? 0U : 1U][idx];
}

/*
 * MPEG Layer III bit-rate lookup (kbps).
 * version: 3=MPEG1, 2=MPEG2, 0=MPEG2.5. layer: 1=L3, 2=L2, 3=L1.
 * Index 0 and 15 are reserved ("free"/invalid).
 */
uint32_t mp3_bitrate(uint32_t version, uint32_t layer, uint32_t idx)
{
    static const uint32_t mpeg1_l3[16] = {
        0U, 32U, 40U, 48U, 56U, 64U, 80U, 96U,
        112U, 128U, 160U, 192U, 224U, 256U, 320U, 0U
    };
    static const uint32_t mpeg2_l3[16] = {
        0U, 8U, 16U, 24U, 32U, 40U, 48U, 56U,
        64U, 80U, 96U, 112U, 128U, 144U, 160U, 0U
    };

    if (idx > 15U) {
        return 0U;
    }
    if (layer != 1U) {
        return 0U; /* this player only feeds MP3 (Layer III) */
    }
    return (version == 3U) ? mpeg1_l3[idx] : mpeg2_l3[idx];
}

/*
 * Estimate total duration in milliseconds by walking MPEG audio frame headers.
 *
 * The streaming feed decodes far faster than the real-time 44.1kHz playback,
 * so byte position cannot represent playback position. This scan gives a
 * precise, VBR-friendly duration before playback starts.
 */
uint32_t mp3_estimate_duration_ms(const mp3_track_t *track, uint32_t base)
{
    const uint8_t *p = track->mp3_start + base;
    uint32_t size = mp3_seed_size(track->mp3_start, track->mp3_end) - base;
    uint64_t total_samples = 0;
    uint32_t i = 0;

    while (i + 4U <= size) {
        uint32_t version;
        uint32_t layer;
        uint32_t sf_idx;
        uint32_t br_idx;
        uint32_t pad;
        uint32_t samples;
        uint32_t srate;
        uint32_t bitrate;
        uint32_t framelen;

        if (p[i] != 0xFFU || (p[i + 1U] & 0xE0U) != 0xE0U) {
            i++;
            continue;
        }

        version = (p[i + 1U] >> 3) & 0x03U;
        layer   = (p[i + 1U] >> 1) & 0x03U;
        sf_idx  = (p[i + 2U] >> 2) & 0x03U;
        br_idx  = (p[i + 2U] >> 4) & 0x0FU;
        pad     = (p[i + 2U] >> 1) & 0x01U;

        /* Reject reserved / invalid combinations (anti false-sync). */
        if (version == 1U || layer == 0U) {
            i++;
            continue;
        }
        if (br_idx == 0U || br_idx == 15U) {
            i++;
            continue;
        }

        srate   = mp3_sample_rate(version, sf_idx);
        bitrate = mp3_bitrate(version, layer, br_idx);
        if (srate == 0U || bitrate == 0U) {
            i++;
            continue;
        }

        /* Samples per frame: Layer III = 1152 (MPEG1) / 576 (MPEG2/2.5). */
        samples = 1152U;
        if (version != 3U) {
            samples = 576U;
        }

        framelen = (144U * bitrate * 1000U) / srate + pad;
        if (framelen < 4U || i + framelen > size) {
            break;
        }

        total_samples += samples;
        i += framelen;
    }

    if (total_samples == 0U) {
        return 0U;
    }
    return (uint32_t)((total_samples * 1000ULL) / 44100ULL);
}

/* Current playback position in ms, frozen while paused. */
uint32_t mp3_core_current_ms(mp3_state_t state, bool play_finished,
                             uint32_t duration_ms, uint32_t pause_started_ms,
                             uint32_t started_tick, uint32_t paused_total_ms,
                             uint32_t now_ms)
{
    uint32_t elapsed;

    if (state == MP3_STATE_LOADING ||
        state == MP3_STATE_ERROR) {
        return 0U;
    }
    if (state == MP3_STATE_STOPPED) {
        return play_finished ? duration_ms : 0U;
    }
    if (state == MP3_STATE_PAUSED) {
        return pause_started_ms;
    }

    elapsed = now_ms - started_tick;
    if (elapsed > paused_total_ms) {
        elapsed -= paused_total_ms;
    } else {
        elapsed = 0U;
    }
    if (duration_ms > 0U && elapsed > duration_ms) {
        elapsed = duration_ms;
    }
    return elapsed;
}

void mp3_format_time(char *buf, size_t len, uint32_t ms)
{
    uint32_t sec = ms / 1000U;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(sec / 60U), (unsigned long)(sec % 60U));
}

const char *mp3_state_name(mp3_state_t s)
{
    switch (s) {
    case MP3_STATE_STOPPED: return "STOPPED";
    case MP3_STATE_PLAYING: return "PLAYING";
    case MP3_STATE_PAUSED:  return "PAUSED";
    case MP3_STATE_LOADING: return "LOADING";
    case MP3_STATE_ERROR:   return "ERROR";
    default: return "?";
    }
}
