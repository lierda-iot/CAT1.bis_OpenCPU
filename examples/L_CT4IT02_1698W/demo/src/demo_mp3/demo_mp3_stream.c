/*
 * demo_mp3_stream.c - MP3 player (streaming implementation)
 *
 * Drop-in replacement for demo_mp3.c. Provides the same 5 public symbols
 * declared in demo_mp3.h but decodes through the SDK's persistent streaming
 * decoder (Liot_AudioMp3StreamStart/Play/Stop) instead of the blocking
 * whole-file Liot_AudioPlayMp3.
 *
 * Why streaming:
 *   - The blocking Liot_AudioPlayMp3 runs a local mad_decoder to completion in
 *     the caller's task. AudioStop() only flips the playback-FIFO state and
 *     cannot interrupt an in-flight mad_decoder_run, so Stop->PlayMp3 can leave
 *     the decoder half-reset (README "next track ends early" limitation).
 *   - Streaming keeps one persistent decoder; pause/resume/stop are handled by
 *     simply stopping the feed + flipping the FIFO state, so the decoder is
 *     never interrupted.
 *   - Memory drops from one whole-file malloc (~469KB) to a 4KB chunk buffer.
 *   - Progress is bytes_fed / total_bytes, independent of an assumed bitrate.
 *
 * This file does NOT modify demo_mp3.c; the build selects between the two via
 * MP3_IMPL in examples/L_CT4IT02_1698W/Makefile.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lierda_app_main.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "lvgl.h"
#include "gui_guider.h"

#include "demo_mp3.h"

#define MP3_LOG_PREFIX              "[demo_mp3_stream]"
#define MP3_BOOT_DELAY_MS           (8000U)
#define MP3_AUDIO_READY_DELAY_MS    (2000U)

#define MP3_COVER_W                 (360U)
#define MP3_COVER_H                 (360U)
#define MP3_UI_UPDATE_MS            (250U)
#define MP3_SAFE_HW_VOLUME_MAX      (70U)
#define MP3_AUDIO_VOLUME            (MP3_SAFE_HW_VOLUME_MAX)
#define MP3_CODEC_VOLUME            (MP3_SAFE_HW_VOLUME_MAX)

/* Streaming feed parameters. */
#define MP3_STREAM_CHUNK_SIZE       (4096U)
#define MP3_WAIT_FINISH_TIMEOUT_S   (120)

/* Autokick for the ES8375 cold-start first-play silence workaround. */
#define MP3_AUTOKICK_DELAY_MS       (500U)
#define MP3_AUTOKICK_PAUSE_MS       (80U)

/* Page exit / long-press gesture (kept identical to demo_mp3.c). */
#define MP3_EXIT_HOLD_MS            (1200U)
#define MP3_EXIT_MOVE_LIMIT         (12)
#define MP3_RESTART_DELAY_MS        (100U)
#define MP3_REENTER_BLOCK_MS        (2000U)
#define MP3_EXIT_DELAY_MS           (300U)
#define MP3_TOGGLE_MIN_INTERVAL_MS  (100U)

extern const uint8_t g_demo_mp3_because_mp3_start[];
extern const uint8_t g_demo_mp3_because_mp3_end[];
extern const uint8_t g_demo_mp3_because_cover_start[];
extern const uint8_t g_demo_mp3_because_cover_end[];
extern void lvgl_init(void);

typedef struct {
    const char *title;
    const uint8_t *mp3_start;
    const uint8_t *mp3_end;
    const uint8_t *cover_start;
    const uint8_t *cover_end;
} mp3_track_t;

static const mp3_track_t g_mp3_tracks[] = {
    {
        "Because Of You",
        g_demo_mp3_because_mp3_start,
        g_demo_mp3_because_mp3_end,
        g_demo_mp3_because_cover_start,
        g_demo_mp3_because_cover_end,
    },
};

#define MP3_TRACK_COUNT             (sizeof(g_mp3_tracks) / sizeof(g_mp3_tracks[0]))

typedef enum {
    MP3_STATE_STOPPED = 0,
    MP3_STATE_PLAYING,
    MP3_STATE_PAUSED,
    MP3_STATE_LOADING,
    MP3_STATE_ERROR,
} mp3_state_t;

/*
 * All fields shared between the LVGL task (UI timer / touch callbacks) and the
 * playback task are volatile to guarantee cross-task visibility.
 */
typedef struct {
    volatile bool audio_ready;
    volatile bool stop_req;
    volatile bool restart_req;
    volatile bool play_finished;
    volatile bool page_active;
    volatile bool page_exiting;
    volatile bool resume_req;
    volatile bool stream_started;
    volatile bool load_done;
    volatile bool load_ok;
    volatile bool ui_ready;
    volatile mp3_state_t state;

    volatile uint32_t current_track;
    volatile uint32_t total_bytes;
    volatile uint32_t fed_bytes;
    volatile uint32_t duration_ms;
    volatile uint32_t started_tick;
    volatile uint32_t paused_total_ms;
    volatile uint32_t paused_tick;
    volatile uint32_t pause_started_ms;
    volatile uint32_t last_exit_tick;
    volatile uint32_t exit_started_tick;
    volatile uint32_t last_toggle_tick;

    uint32_t exit_press_tick;
    lv_point_t exit_press_point;
    bool exit_press_candidate;
    bool page_del;

    liot_task_t play_task;
    liot_task_t load_task;

    lv_obj_t *screen;
    lv_obj_t *cover_img;
    lv_obj_t *title_label;
    lv_obj_t *state_label;
    lv_obj_t *time_label;
    lv_obj_t *play_btn_label;
    lv_obj_t *bar;
    lv_timer_t *ui_timer;
    lv_timer_t *exit_timer;

    lv_img_dsc_t cover_dsc;
} demo_mp3_ctx_t;

static demo_mp3_ctx_t g_mp3;

static const char *mp3_state_name(mp3_state_t s);
static void mp3_set_state_from(mp3_state_t state, const char *reason);
#define mp3_set_state(s) mp3_set_state_from((s), __func__)
static void mp3_update_ui(void);
static void mp3_refresh_track_ui(void);
static void mp3_start_play(void);
static void mp3_start_load_task(void);
static void demo_mp3_handle_load_done(void);
static void demo_mp3_page_exit_timer_cb(lv_timer_t *timer);
static void demo_mp3_page_exit_request(void);

static void mp3_at_log(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len > 0) {
        liot_trace("%s\n", buf);
    }
}

#define MP3_LOG(fmt, ...) mp3_at_log(fmt, ##__VA_ARGS__)

static uint32_t mp3_seed_size(const uint8_t *start, const uint8_t *end)
{
    return (uint32_t)(end - start);
}

/*
 * Locate the first MPEG audio frame sync word and skip a leading ID3v2 tag
 * (and any junk before it). The -12dB source starts with "ID3..." at offset 0,
 * so feeding from offset 0 would make libmad resync mid-tag. Skip to the first
 * 0xFF 0xEx sync.
 */
static uint32_t mp3_frame_offset(const mp3_track_t *track)
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
static uint32_t mp3_sample_rate(uint32_t version, uint32_t idx)
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
static uint32_t mp3_bitrate(uint32_t version, uint32_t layer, uint32_t idx)
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
static uint32_t mp3_estimate_duration_ms(const mp3_track_t *track, uint32_t base)
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

static const mp3_track_t *mp3_current_track(void)
{
    if (g_mp3.current_track >= MP3_TRACK_COUNT) {
        g_mp3.current_track = 0;
    }
    return &g_mp3_tracks[g_mp3.current_track];
}

static bool mp3_set_embedded_cover_desc(void)
{
    const mp3_track_t *track = mp3_current_track();
    uint32_t cover_size = mp3_seed_size(track->cover_start, track->cover_end);

    if ((track->cover_start == NULL) ||
        (cover_size != (MP3_COVER_W * MP3_COVER_H * 2U))) {
        return false;
    }

    memset(&g_mp3.cover_dsc, 0, sizeof(g_mp3.cover_dsc));
    g_mp3.cover_dsc.header.always_zero = 0;
    g_mp3.cover_dsc.header.w = MP3_COVER_W;
    g_mp3.cover_dsc.header.h = MP3_COVER_H;
    g_mp3.cover_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    g_mp3.cover_dsc.data_size = cover_size;
    g_mp3.cover_dsc.data = track->cover_start;
    return true;
}

static uint32_t mp3_now_ms(void)
{
    return liot_rtos_get_system_tick();
}

static uint32_t mp3_elapsed_ms(uint32_t start)
{
    return (uint32_t)(mp3_now_ms() - start);
}

/* Current playback position in ms, frozen while paused. */
static uint32_t mp3_current_ms(void)
{
    uint32_t elapsed;

    if (g_mp3.state == MP3_STATE_LOADING ||
        g_mp3.state == MP3_STATE_ERROR) {
        return 0U;
    }
    if (g_mp3.state == MP3_STATE_STOPPED) {
        return g_mp3.play_finished ? g_mp3.duration_ms : 0U;
    }
    if (g_mp3.state == MP3_STATE_PAUSED) {
        return g_mp3.pause_started_ms;
    }

    elapsed = mp3_elapsed_ms(g_mp3.started_tick);
    if (elapsed > g_mp3.paused_total_ms) {
        elapsed -= g_mp3.paused_total_ms;
    } else {
        elapsed = 0U;
    }
    if (g_mp3.duration_ms > 0U && elapsed > g_mp3.duration_ms) {
        elapsed = g_mp3.duration_ms;
    }
    return elapsed;
}

static void mp3_format_time(char *buf, size_t len, uint32_t ms)
{
    uint32_t sec = ms / 1000U;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(sec / 60U), (unsigned long)(sec % 60U));
}

static void mp3_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;

    switch (event) {
    case L_AUD_EVT_START:
        mp3_at_log("%s EVT_START state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        break;
    case L_AUD_EVT_FINISH:
        mp3_at_log("%s EVT_FINISH state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        /*
         * Deferred-finish semantics: do not require state==PLAYING. If we are
         * paused while the FIFO drains, remember the finish and let the resume
         * path observe it. Otherwise this is a natural completion.
         */
        if (!g_mp3.page_exiting) {
            g_mp3.play_finished = true;
        }
        break;
    case L_AUD_EVT_ERROR:
        mp3_at_log("%s EVT_ERROR state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        if (!g_mp3.page_exiting) {
            mp3_set_state(MP3_STATE_ERROR);
            g_mp3.play_finished = true;
        }
        break;
    case L_AUD_EVT_CLOSE:
        mp3_at_log("%s EVT_CLOSE state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        if (!g_mp3.page_exiting) {
            g_mp3.play_finished = true;
        }
        break;
    default:
        break;
    }
}

static void mp3_audio_init_once(void)
{
    Liot_AudHwConfig_t cfg;
    Liot_AudErr_e ret;

    if (g_mp3.audio_ready) return;

    liot_rtos_task_sleep_ms(MP3_AUDIO_READY_DELAY_MS);
    Liot_AonPowerCtl(TRUE);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    memset(&cfg, 0, sizeof(cfg));
    cfg.i2cNum = 0;
    cfg.i2sNum = 0;
    cfg.paGpioNum = -1;
    cfg.codecType = L_AUD_ES8375;
    cfg.channel = L_AUD_MONO_RIGHT;
    cfg.role = L_AUD_ROLE_SLAVE;
    cfg.mode = L_AUD_MODE_I2S;
    cfg.frameSize = L_AUD_FRAMESIZE_16_16;
    cfg.samples = L_AUD_44K_SAMPLES;
    cfg.callback = mp3_audio_callback;

    ret = Liot_AudioInit(&cfg);
    liot_trace("%s AudioInit ret=%d", MP3_LOG_PREFIX, (int)ret);
    if (ret == L_AUD_ERR_SUCCESS) {
        Liot_AudioSetVolume(MP3_AUDIO_VOLUME);
        Liot_AudioSetCodecVolume(MP3_CODEC_VOLUME);
        Liot_AudioSetMicVolume(8, 200);
        g_mp3.audio_ready = true;
    }
}

void demo_mp3_audio_preinit(void)
{
    mp3_audio_init_once();
}

bool demo_mp3_audio_is_ready(void)
{
    return g_mp3.audio_ready;
}

static const char *mp3_state_name(mp3_state_t s)
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

static void mp3_set_state_from(mp3_state_t state, const char *reason)
{
    mp3_at_log("%s STATE %s -> %s (%s)", MP3_LOG_PREFIX,
               mp3_state_name(g_mp3.state), mp3_state_name(state), reason);
    g_mp3.state = state;
}

/*
 * Read one chunk from the embedded MP3 resource. The seed already points at a
 * frame-aligned offset (the -12dB source has no ID3 in front; the decoder's
 * input callback tolerates a short scan window anyway).
 */
static uint32_t mp3_read_chunk(const mp3_track_t *track, uint32_t base,
                               uint32_t offset,
                               uint8_t *buf, uint32_t buf_size)
{
    uint32_t total = mp3_seed_size(track->mp3_start, track->mp3_end) - base;
    uint32_t n;

    if (offset >= total) {
        return 0;
    }
    n = total - offset;
    if (n > buf_size) {
        n = buf_size;
    }
    memcpy(buf, track->mp3_start + base + offset, n);
    return n;
}

/*
 * Streaming playback task.
 *
 *   Mp3StreamStart()  once -> persistent decoder
 *   loop: Mp3StreamPlay(chunk) -> mad_decoder_run -> Liot_AudioPlay(pcm)
 *   pause: stop feeding + Liot_AudioPlayPause()
 *   resume: Liot_AudioPlayResume() + continue feeding from fed_bytes
 *   end: feed all bytes, then Liot_AudioWaitPlayFinish(timeout)
 *   stop: Mp3StreamStop() clean release
 */
static void mp3_playback_task(void *argv)
{
    const mp3_track_t *track;
    uint8_t *chunk = NULL;
    uint32_t total;
    uint32_t base;
    uint32_t fed;
    uint32_t chunk_size = MP3_STREAM_CHUNK_SIZE;
    Liot_AudErr_e ret;
    bool restart_req;
    bool autokicked = false;
    bool resume_failed = false;
    uint32_t watchdog_budget;

    (void)argv;
    g_mp3.stop_req = false;
    g_mp3.restart_req = false;
    g_mp3.resume_req = false;
    g_mp3.play_finished = false;
    g_mp3.stream_started = false;
    g_mp3.paused_total_ms = 0;
    g_mp3.pause_started_ms = 0;

    mp3_audio_init_once();
    if (!g_mp3.audio_ready) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }

    track = mp3_current_track();
    total = mp3_seed_size(track->mp3_start, track->mp3_end);
    if ((track->mp3_start == NULL) || (total == 0U)) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }
    base = mp3_frame_offset(track);
    total -= base;
    if (total == 0U) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }

    chunk = (uint8_t *)malloc(chunk_size);
    if (chunk == NULL) {
        mp3_at_log("%s malloc chunk failed size=%lu", MP3_LOG_PREFIX, (unsigned long)chunk_size);
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }

    ret = Liot_AudioMp3StreamStart();
    mp3_at_log("%s StreamStart ret=%d", MP3_LOG_PREFIX, (int)ret);
    if (ret != L_AUD_ERR_SUCCESS) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }
    g_mp3.stream_started = true;

    g_mp3.total_bytes = total;
    g_mp3.fed_bytes = 0;
    g_mp3.duration_ms = mp3_estimate_duration_ms(track, base);
    if (g_mp3.duration_ms == 0U) {
        mp3_at_log("%s duration estimate failed, fallback to measured",
                   MP3_LOG_PREFIX);
    }
    g_mp3.started_tick = mp3_now_ms();
    mp3_set_state(MP3_STATE_PLAYING);
    watchdog_budget = (uint32_t)MP3_WAIT_FINISH_TIMEOUT_S * 1000U;

    fed = 0;
    while (fed < total && !g_mp3.stop_req) {
        uint32_t n;

        if (g_mp3.state == MP3_STATE_PAUSED) {
            if (g_mp3.resume_req) {
                ret = Liot_AudioPlayResume();
                mp3_at_log("%s Resume ret=%d", MP3_LOG_PREFIX, (int)ret);
                g_mp3.resume_req = false;
                if (ret == L_AUD_ERR_SUCCESS) {
                    mp3_set_state(MP3_STATE_PLAYING);
                    g_mp3.pause_started_ms = 0;
                } else {
                    /* Resume failed: fall back to a clean restart of the track. */
                    resume_failed = true;
                    break;
                }
            }
            liot_rtos_task_sleep_ms(20);
            continue;
        }

        n = mp3_read_chunk(track, base, fed, chunk, chunk_size);
        if (n == 0U) {
            mp3_at_log("%s read_chunk empty offset=%lu", MP3_LOG_PREFIX, (unsigned long)fed);
            mp3_set_state(MP3_STATE_ERROR);
            break;
        }

        ret = Liot_AudioMp3StreamPlay(chunk, (int)n);
        if (ret != L_AUD_ERR_SUCCESS) {
            mp3_at_log("%s StreamPlay failed ret=%d fed=%lu", MP3_LOG_PREFIX, (int)ret, (unsigned long)fed);
            mp3_set_state(MP3_STATE_ERROR);
            break;
        }
        fed += n;
        g_mp3.fed_bytes = fed;

        /*
         * ES8375 cold-start silence workaround: kick pause/resume once right
         * after the first chunk has primed the FIFO (not after the whole file).
         */
        if (!autokicked) {
            autokicked = true;
            liot_rtos_task_sleep_ms(MP3_AUTOKICK_DELAY_MS);
            if (!g_mp3.stop_req) {
                (void)Liot_AudioPlayPause();
                liot_rtos_task_sleep_ms(MP3_AUTOKICK_PAUSE_MS);
                (void)Liot_AudioPlayResume();
            }
        }
    }

    if (resume_failed) {
        /* Resume failed under rapid toggling: request a clean restart. */
        mp3_at_log("%s resume failed, restart track", MP3_LOG_PREFIX);
        mp3_set_state(MP3_STATE_LOADING);
        g_mp3.restart_req = true;
    } else if (!g_mp3.stop_req && g_mp3.state != MP3_STATE_ERROR &&
               fed >= total) {
        /*
         * Only treat as natural completion once every byte has been fed AND
         * the FIFO has actually drained. EVT_FINISH may fire early during the
         * feed loop because decoding is far faster than real-time playback, so
         * gate completion on fed >= total and then wait for a real drain.
         */
        uint32_t waited_ms = 0U;
        bool drained = false;

        /* Drop any premature EVT_FINISH accumulated while feeding. */
        g_mp3.play_finished = false;

        while (!g_mp3.stop_req && g_mp3.state != MP3_STATE_ERROR) {
            if (g_mp3.state == MP3_STATE_PAUSED) {
                liot_rtos_task_sleep_ms(20);
                continue; /* pause: do not advance the watchdog */
            }
            ret = Liot_AudioWaitPlayFinish(1); /* one second slice */
            if (ret == L_AUD_ERR_SUCCESS) {
                drained = true;
                break;
            }
            waited_ms += 1000U;
            if (waited_ms >= watchdog_budget) {
                mp3_at_log("%s drain watchdog timeout waited=%lu",
                           MP3_LOG_PREFIX, (unsigned long)waited_ms);
                mp3_set_state(MP3_STATE_ERROR);
                break;
            }
        }

        if (drained && !g_mp3.stop_req && g_mp3.state != MP3_STATE_ERROR) {
            uint32_t elapsed = mp3_elapsed_ms(g_mp3.started_tick);

            if (elapsed > g_mp3.paused_total_ms) {
                elapsed -= g_mp3.paused_total_ms;
            } else {
                elapsed = 0U;
            }
            if (g_mp3.duration_ms == 0U && elapsed > 0U) {
                g_mp3.duration_ms = elapsed;
            }
            g_mp3.play_finished = true;
            mp3_set_state(MP3_STATE_STOPPED);
        }
    }

    if (g_mp3.stop_req) {
        (void)Liot_AudioStop();
    }

done:
    if (g_mp3.stream_started) {
        (void)Liot_AudioMp3StreamStop();
        g_mp3.stream_started = false;
    }
    if (chunk != NULL) {
        free(chunk);
        chunk = NULL;
    }

    restart_req = g_mp3.restart_req;
    g_mp3.restart_req = false;
    g_mp3.play_task = NULL;

    if (restart_req && g_mp3.page_active && !g_mp3.page_exiting) {
        mp3_set_state(MP3_STATE_STOPPED);
        liot_rtos_task_sleep_ms(MP3_RESTART_DELAY_MS);
        mp3_start_play();
    }
    liot_rtos_task_delete(NULL);
}

static void mp3_start_play(void)
{
    if (g_mp3.page_exiting) return;

    if (g_mp3.state == MP3_STATE_PAUSED) {
        g_mp3.paused_total_ms += mp3_elapsed_ms(g_mp3.paused_tick);
        g_mp3.resume_req = true;
        return;
    }

    if ((g_mp3.state == MP3_STATE_PLAYING) || (g_mp3.state == MP3_STATE_LOADING)) return;
    if (g_mp3.play_task != NULL) {
        g_mp3.restart_req = true;
        return;
    }

    g_mp3.stop_req = false;
    liot_rtos_task_create(&g_mp3.play_task, 10 * 1024, LIOT_APP_TASK_PRIORITY,
                          "mp3_playback", mp3_playback_task, NULL);
}

static void mp3_pause_play(void)
{
    Liot_AudErr_e ret;

    if (g_mp3.state != MP3_STATE_PLAYING) return;
    g_mp3.pause_started_ms = mp3_current_ms();
    g_mp3.paused_tick = mp3_now_ms();
    ret = Liot_AudioPlayPause();
    mp3_at_log("%s Pause ret=%d", MP3_LOG_PREFIX, (int)ret);
    if (ret == L_AUD_ERR_SUCCESS) {
        mp3_set_state(MP3_STATE_PAUSED);
    }
    /* On failure keep PLAYING: app state stays consistent with hardware. */
}

static void mp3_request_restart_play(void)
{
    if (g_mp3.play_task != NULL) {
        g_mp3.stop_req = true;
        g_mp3.restart_req = true;
        mp3_set_state(MP3_STATE_LOADING);
        return;
    }

    mp3_start_play();
}

static void mp3_update_ui(void)
{
    char cur[12];
    char total[12];
    char text[32];
    uint32_t current_ms;
    uint32_t total_ms = g_mp3.duration_ms;
    const char *state_text = "Stopped";

    if (!g_mp3.ui_ready && g_mp3.screen == NULL) return;

    switch (g_mp3.state) {
    case MP3_STATE_PLAYING: state_text = "Playing"; break;
    case MP3_STATE_PAUSED: state_text = "Paused"; break;
    case MP3_STATE_LOADING: state_text = "Loading"; break;
    case MP3_STATE_ERROR: state_text = "Error"; break;
    default: break;
    }

    current_ms = mp3_current_ms();

    mp3_format_time(cur, sizeof(cur), current_ms);
    if (total_ms != 0U) {
        mp3_format_time(total, sizeof(total), total_ms);
    } else {
        snprintf(total, sizeof(total), "--:--");
    }
    snprintf(text, sizeof(text), "%s / %s", cur, total);

    if (g_mp3.time_label != NULL) lv_label_set_text(g_mp3.time_label, text);
    if (g_mp3.state_label != NULL) lv_label_set_text(g_mp3.state_label, state_text);
    if (g_mp3.play_btn_label != NULL) {
        lv_label_set_text(g_mp3.play_btn_label,
                          (g_mp3.state == MP3_STATE_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
    if (g_mp3.bar != NULL && total_ms > 0U) {
        int value = (int)((current_ms * 100U) / total_ms);
        if (value > 100) value = 100;
        lv_bar_set_value(g_mp3.bar, value, LV_ANIM_OFF);
    }
}

static void mp3_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_mp3.load_done) {
        demo_mp3_handle_load_done();
    }
    mp3_update_ui();
}

static void mp3_switch_track(int delta)
{
    uint32_t next_track;

    if (delta < 0) {
        next_track = (g_mp3.current_track + MP3_TRACK_COUNT - 1U) % MP3_TRACK_COUNT;
    } else {
        next_track = (g_mp3.current_track + 1U) % MP3_TRACK_COUNT;
    }
    g_mp3.current_track = next_track;
    mp3_refresh_track_ui();
    mp3_request_restart_play();
}

static void mp3_prev_event_cb(lv_event_t *e)
{
    (void)e;
    mp3_switch_track(-1);
}

static void mp3_next_event_cb(lv_event_t *e)
{
    (void)e;
    mp3_switch_track(1);
}

static void mp3_play_event_cb(lv_event_t *e)
{
    (void)e;

    /* Debounce rapid play/pause toggling that can desync the FIFO state. */
    if (g_mp3.last_toggle_tick != 0U &&
        lv_tick_elaps(g_mp3.last_toggle_tick) < MP3_TOGGLE_MIN_INTERVAL_MS) {
        return;
    }
    g_mp3.last_toggle_tick = lv_tick_get();

    if (g_mp3.state == MP3_STATE_PLAYING) {
        mp3_pause_play();
    } else {
        mp3_start_play();
    }
    mp3_update_ui();
}

static void mp3_delete_timer(lv_timer_t **timer)
{
    if ((timer != NULL) && (*timer != NULL)) {
        lv_timer_del(*timer);
        *timer = NULL;
    }
}

static void demo_mp3_page_exit(void)
{
    lv_obj_t *active_scr = lv_scr_act();

    if (!g_mp3.page_active && (g_mp3.screen == NULL)) {
        mp3_at_log("%s exit ignored inactive active=%p", MP3_LOG_PREFIX, active_scr);
        return;
    }

    g_mp3.stop_req = true;
    g_mp3.restart_req = false;
    g_mp3.load_done = false;
    g_mp3.load_ok = false;
    (void)Liot_AudioStop();

    /*
     * Wait for the playback task to self-terminate instead of force-deleting
     * it (avoids double-delete / dangling handle races). If it does not exit,
     * fall through to unload the UI; the task keeps a NULL handle.
     */
    if (g_mp3.play_task != NULL) {
        mp3_at_log("%s exit wait play_task=%p state=%s",
                   MP3_LOG_PREFIX, g_mp3.play_task, mp3_state_name(g_mp3.state));
        if (g_mp3.exit_timer == NULL) {
            g_mp3.exit_timer = lv_timer_create(demo_mp3_page_exit_timer_cb, 200, NULL);
            lv_timer_set_repeat_count(g_mp3.exit_timer, 1);
        }
        return;
    }

    mp3_at_log("%s exit begin active=%p screen=%p menu=%p",
               MP3_LOG_PREFIX, active_scr, g_mp3.screen, guider_ui.mp3);

    g_mp3.page_active = false;
    g_mp3.page_exiting = false;
    g_mp3.exit_started_tick = 0U;
    g_mp3.last_exit_tick = lv_tick_get();
    g_mp3.ui_ready = false;
    mp3_delete_timer(&g_mp3.ui_timer);
    mp3_delete_timer(&g_mp3.exit_timer);
    mp3_set_state(MP3_STATE_STOPPED);

    ui_load_scr_animation(&guider_ui,
                          &guider_ui.mp3,
                          guider_ui.mp3_del,
                          &g_mp3.page_del,
                          setup_scr_mp3,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          0,
                          false,
                          false);

    g_mp3.screen = NULL;
    g_mp3.cover_img = NULL;
    g_mp3.title_label = NULL;
    g_mp3.state_label = NULL;
    g_mp3.time_label = NULL;
    g_mp3.play_btn_label = NULL;
    g_mp3.bar = NULL;
    g_mp3.page_del = true;
    mp3_at_log("%s exit end menu=%p", MP3_LOG_PREFIX, guider_ui.mp3);
}

static void demo_mp3_page_exit_timer_cb(lv_timer_t *timer)
{
    if (g_mp3.exit_timer == timer) {
        g_mp3.exit_timer = NULL;
    }
    lv_timer_del(timer);
    demo_mp3_page_exit();
}

static void demo_mp3_page_exit_request(void)
{
    lv_indev_t *indev;

    if (g_mp3.page_exiting) {
        mp3_at_log("%s exit request ignored", MP3_LOG_PREFIX);
        return;
    }

    mp3_at_log("%s exit request active=%p screen=%p", MP3_LOG_PREFIX, lv_scr_act(), g_mp3.screen);
    g_mp3.page_exiting = true;
    g_mp3.exit_started_tick = mp3_now_ms();
    g_mp3.stop_req = true;
    g_mp3.restart_req = false;
    (void)Liot_AudioStop();

    indev = lv_indev_get_act();
    if (indev != NULL) {
        lv_indev_wait_release(indev);
    }

    if (g_mp3.exit_timer == NULL) {
        g_mp3.exit_timer = lv_timer_create(demo_mp3_page_exit_timer_cb, MP3_EXIT_DELAY_MS, NULL);
        lv_timer_set_repeat_count(g_mp3.exit_timer, 1);
    }
}

static void mp3_page_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev;
    lv_point_t point;
    int32_t dx;
    int32_t dy;

    if (code == LV_EVENT_PRESSED) {
        g_mp3.exit_press_candidate =
            (lv_event_get_target(event) == lv_event_get_current_target(event));
        if (!g_mp3.exit_press_candidate) return;
        indev = lv_indev_get_act();
        if (indev == NULL) {
            g_mp3.exit_press_candidate = false;
            return;
        }
        lv_indev_get_point(indev, &g_mp3.exit_press_point);
        g_mp3.exit_press_tick = lv_tick_get();
        return;
    }

    if (code == LV_EVENT_PRESSING && g_mp3.exit_press_candidate) {
        indev = lv_indev_get_act();
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        dx = point.x - g_mp3.exit_press_point.x;
        dy = point.y - g_mp3.exit_press_point.y;
        if (dx > MP3_EXIT_MOVE_LIMIT || dx < -MP3_EXIT_MOVE_LIMIT ||
            dy > MP3_EXIT_MOVE_LIMIT || dy < -MP3_EXIT_MOVE_LIMIT) {
            g_mp3.exit_press_candidate = false;
            return;
        }
        if (lv_tick_elaps(g_mp3.exit_press_tick) >= MP3_EXIT_HOLD_MS) {
            g_mp3.exit_press_candidate = false;
            lv_event_stop_bubbling(event);
            lv_event_stop_processing(event);
            demo_mp3_page_exit_request();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_mp3.exit_press_candidate = false;
    }
}

static lv_obj_t *mp3_create_btn(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 64, 44);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_center(label);
    return label;
}

static void mp3_refresh_track_ui(void)
{
    if (!g_mp3.ui_ready) return;

    lv_label_set_text(g_mp3.title_label, mp3_current_track()->title);
    if (mp3_set_embedded_cover_desc() && (g_mp3.cover_img != NULL)) {
        lv_img_set_src(g_mp3.cover_img, &g_mp3.cover_dsc);
    }
    g_mp3.total_bytes = mp3_seed_size(mp3_current_track()->mp3_start, mp3_current_track()->mp3_end);
    g_mp3.fed_bytes = 0;
    g_mp3.duration_ms = 0;
    g_mp3.paused_total_ms = 0;
    g_mp3.pause_started_ms = 0;
    lv_bar_set_value(g_mp3.bar, 0, LV_ANIM_OFF);
    mp3_update_ui();
}

static void mp3_create_page(void)
{
    lv_obj_t *cover;
    lv_obj_t *cover_title;
    bool cover_ok = mp3_set_embedded_cover_desc();

    g_mp3.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_mp3.screen, lv_color_hex(0x10141F), 0);
    lv_obj_set_style_bg_opa(g_mp3.screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_mp3.screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_mp3.screen, mp3_page_event_cb, LV_EVENT_ALL, NULL);

    g_mp3.cover_img = lv_img_create(g_mp3.screen);
    lv_obj_set_pos(g_mp3.cover_img, 0, 0);
    if (cover_ok) {
        lv_img_set_src(g_mp3.cover_img, &g_mp3.cover_dsc);
    }

    g_mp3.title_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.title_label, mp3_current_track()->title);
    lv_obj_set_style_text_color(g_mp3.title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_mp3.title_label, &lv_font_DAIMENG_28, 0);
    lv_obj_align(g_mp3.title_label, LV_ALIGN_TOP_MID, 0, 14);

    if (!cover_ok) {
        cover = lv_obj_create(g_mp3.screen);
        lv_obj_set_size(cover, 220, 170);
        lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 52);
        lv_obj_set_style_radius(cover, 18, 0);
        lv_obj_set_style_bg_color(cover, lv_color_hex(0xF7B267), 0);
        lv_obj_set_style_bg_grad_color(cover, lv_color_hex(0x522B5B), 0);
        lv_obj_set_style_bg_grad_dir(cover, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(cover, 0, 0);

        cover_title = lv_label_create(cover);
        lv_label_set_text_fmt(cover_title, "MP3\n%s", mp3_current_track()->title);
        lv_obj_set_style_text_align(cover_title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(cover_title, lv_color_white(), 0);
        lv_obj_set_style_text_font(cover_title, &lv_font_montserratMedium_23, 0);
        lv_obj_center(cover_title);
    }

    g_mp3.state_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.state_label, "Loading");
    lv_obj_set_style_text_color(g_mp3.state_label, lv_color_hex(0xD8DEE9), 0);
    lv_obj_align(g_mp3.state_label, LV_ALIGN_TOP_MID, 0, 202);

    g_mp3.bar = lv_bar_create(g_mp3.screen);
    lv_obj_set_size(g_mp3.bar, 240, 12);
    lv_obj_align(g_mp3.bar, LV_ALIGN_TOP_MID, 0, 230);
    lv_bar_set_range(g_mp3.bar, 0, 100);
    lv_bar_set_value(g_mp3.bar, 0, LV_ANIM_OFF);

    g_mp3.time_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.time_label, "00:00 / --:--");
    lv_obj_set_style_text_color(g_mp3.time_label, lv_color_hex(0xD8DEE9), 0);
    lv_obj_set_style_text_font(g_mp3.time_label, &lv_font_montserratMedium_18, 0);
    lv_obj_align(g_mp3.time_label, LV_ALIGN_TOP_MID, 0, 248);

    mp3_create_btn(g_mp3.screen, LV_SYMBOL_PREV, 68, 272, mp3_prev_event_cb);
    g_mp3.play_btn_label = mp3_create_btn(g_mp3.screen, LV_SYMBOL_PLAY, 148, 272, mp3_play_event_cb);
    mp3_create_btn(g_mp3.screen, LV_SYMBOL_NEXT, 228, 272, mp3_next_event_cb);

    lv_scr_load_anim(g_mp3.screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    guider_ui.mp3_del = true;
    g_mp3.page_del = false;
    if (g_mp3.ui_timer != NULL) lv_timer_del(g_mp3.ui_timer);
    g_mp3.ui_timer = lv_timer_create(mp3_ui_timer_cb, MP3_UI_UPDATE_MS, NULL);
    g_mp3.ui_ready = true;
    mp3_update_ui();
}

static void demo_mp3_handle_load_done(void)
{
    bool ok = g_mp3.load_ok;

    g_mp3.load_done = false;
    if (!g_mp3.page_active || g_mp3.page_exiting) {
        mp3_at_log("%s load done ignored active=%d exiting=%d",
                   MP3_LOG_PREFIX, g_mp3.page_active ? 1 : 0, g_mp3.page_exiting ? 1 : 0);
        return;
    }

    mp3_at_log("%s load done ok=%d", MP3_LOG_PREFIX, ok ? 1 : 0);
    if (ok) {
        mp3_set_state(MP3_STATE_STOPPED);
        mp3_update_ui();
    } else {
        mp3_set_state(MP3_STATE_ERROR);
        mp3_update_ui();
    }
}

static void mp3_load_task(void *argv)
{
    bool ok = false;

    (void)argv;
    mp3_at_log("%s load task begin", MP3_LOG_PREFIX);
    mp3_audio_init_once();
    ok = g_mp3.audio_ready;
    mp3_at_log("%s load step audio ok=%d", MP3_LOG_PREFIX, ok ? 1 : 0);

    g_mp3.load_ok = ok;
    g_mp3.load_done = true;
    g_mp3.load_task = NULL;
    liot_rtos_task_delete(NULL);
}

static void mp3_start_load_task(void)
{
    LiotOSStatus_t ret;

    if (g_mp3.load_task != NULL) {
        mp3_at_log("%s load task already running", MP3_LOG_PREFIX);
        return;
    }

    ret = liot_rtos_task_create(&g_mp3.load_task, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                                "mp3_load", mp3_load_task, NULL);
    mp3_at_log("%s create load task ret=%d handle=%p", MP3_LOG_PREFIX, (int)ret, g_mp3.load_task);
    if (ret != LIOT_OSI_SUCCESS) {
        g_mp3.load_task = NULL;
        mp3_set_state(MP3_STATE_ERROR);
        mp3_update_ui();
    }
}

void demo_mp3_page_enter(void)
{
    if (g_mp3.page_active) {
        mp3_at_log("%s enter ignored active", MP3_LOG_PREFIX);
        return;
    }
    if ((g_mp3.last_exit_tick != 0U) && (lv_tick_elaps(g_mp3.last_exit_tick) < MP3_REENTER_BLOCK_MS)) {
        mp3_at_log("%s enter ignored guard", MP3_LOG_PREFIX);
        return;
    }

    mp3_at_log("%s enter begin", MP3_LOG_PREFIX);
    g_mp3.page_active = true;
    g_mp3.page_exiting = false;
    g_mp3.stop_req = false;
    g_mp3.restart_req = false;
    g_mp3.play_finished = false;
    g_mp3.load_done = false;
    g_mp3.load_ok = false;
    mp3_delete_timer(&g_mp3.ui_timer);
    mp3_delete_timer(&g_mp3.exit_timer);

    mp3_create_page();
    mp3_set_state(MP3_STATE_LOADING);
    mp3_update_ui();
    mp3_start_load_task();
    mp3_at_log("%s enter end", MP3_LOG_PREFIX);
}

static void demo_mp3_page_enter_cb(void *arg)
{
    (void)arg;
    demo_mp3_page_enter();
}

void demo_mp3_page_enter_async(void)
{
    lv_async_call(demo_mp3_page_enter_cb, NULL);
}

void liot_mp3_demo_thread(void *argv)
{
    (void)argv;
    liot_trace("%s demo start", MP3_LOG_PREFIX);
    liot_rtos_task_sleep_ms(MP3_BOOT_DELAY_MS);
    liot_trace("[demo_mp3_stream] === MP3 DEMO START ===");
    liot_rtos_task_sleep_ms(1000);
    liot_trace("[demo_mp3_stream] === MP3 DEMO START 2 ===");
    liot_rtos_task_sleep_ms(1000);
    liot_trace("[demo_mp3_stream] === MP3 DEMO START 3 ===");
    lvgl_init();
    demo_mp3_page_enter_async();
    while (1) {
        liot_rtos_task_sleep_s(10);
    }
}
