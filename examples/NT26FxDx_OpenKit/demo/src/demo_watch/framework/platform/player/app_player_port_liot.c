#include "app_player_port_liot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_osal.h"
#include "liot_audio2.h"
#include "liot_external_flash_fs.h"
#include "liot_flash.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_tts.h"
#include "mem_map.h"

#ifndef CONFIG_TTS_ENABLE
#error "demo_watch player TTS requires BUILD_COMP_TTS_EN"
#endif

#ifndef PKGFLXTTS_RES_ADDR
#error "demo_watch player TTS requires PKGFLXTTS_RES_ADDR"
#endif

#define PLAYER_LIOT_DEFAULT_CHUNK_SIZE (4096U)
#define PLAYER_LIOT_DEFAULT_WAIT_TIMEOUT_S (120U)
#define PLAYER_LIOT_WAV_HEADER_SIZE (44U)
#define PLAYER_LIOT_TTS_PATH_PREFIX "tts:"
#define PLAYER_LIOT_TTS_PREROLL_MS (800U)
#define PLAYER_LIOT_TTS_PREROLL_CHUNK_BYTES (512U)
#define PLAYER_LIOT_TTS_PCM_BYTES_PER_SAMPLE (2U)

#define PLAYER_LIOT_AUDIO_I2C_NUM 0
#define PLAYER_LIOT_AUDIO_I2S_NUM 0
#define PLAYER_LIOT_AUDIO_PA_GPIO 11
#define PLAYER_LIOT_AUDIO_VOLUME 50
#define PLAYER_LIOT_AUDIO_CODEC_VOLUME 60

typedef struct __attribute__((packed)) {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} player_liot_wav_header_t;

typedef struct {
    app_player_liot_config_t config;
    bool configured;
    bool initialized;
    bool audio_ready;
    bool audio_sample_valid;
    Liot_AudSample_e audio_sample;
    bool playing;
    volatile bool stop_requested;
    uint32_t tts_callback_chunks;
    uint32_t tts_callback_bytes;
    uint32_t tts_callback_errors;
} player_liot_ctx_t;

static int player_liot_init(void);
static int player_liot_deinit(void);
static int player_liot_list_files(const char *root,
                                  app_player_file_t *files,
                                  uint32_t capacity,
                                  uint32_t *count);
static int player_liot_play_file(const app_player_file_t *file,
                                 volatile bool *stop_requested,
                                 app_player_progress_cb_t progress_cb,
                                 void *progress_user);
static int player_liot_stop(void);

static player_liot_ctx_t s_player_liot;

static const app_player_port_ops_t s_player_liot_ops = {
    .init = player_liot_init,
    .deinit = player_liot_deinit,
    .list_files = player_liot_list_files,
    .play_file = player_liot_play_file,
    .stop = player_liot_stop,
};

static app_player_port_t s_player_liot_port = {
    .ops = &s_player_liot_ops,
};

static uint8_t s_player_liot_tts_preroll_zero[PLAYER_LIOT_TTS_PREROLL_CHUNK_BYTES];

static const char *player_liot_sample_name(Liot_AudSample_e sample)
{
    switch (sample) {
    case L_AUD_08K_SAMPLES:
        return "8k";
    case L_AUD_16K_SAMPLES:
        return "16k";
    default:
        return "other";
    }
}

static uint32_t player_liot_sample_hz(Liot_AudSample_e sample)
{
    switch (sample) {
    case L_AUD_08K_SAMPLES:
        return 8000U;
    case L_AUD_16K_SAMPLES:
        return 16000U;
    default:
        return 0U;
    }
}

static bool player_liot_tts_read_resource(void *parameter,
                                          void *buffer,
                                          uint32_t pos,
                                          uint32_t size)
{
    uintptr_t base;

    if (parameter == NULL || buffer == NULL || size == 0U) {
        return false;
    }

    base = (uintptr_t)parameter;
    liot_flash_read((uint8_t *)buffer, (uint32_t)(base + pos), size);
    return true;
}

static int player_liot_tts_output_cb(void *context,
                                     int msg,
                                     int ds,
                                     int param2,
                                     int size,
                                     const void *buffer)
{
    Liot_AudErr_e ret;

    (void)context;
    (void)msg;
    (void)ds;
    (void)param2;
    if (s_player_liot.stop_requested) {
        return -1;
    }
    if (buffer == NULL || size <= 0) {
        return 0;
    }

    s_player_liot.tts_callback_chunks++;
    s_player_liot.tts_callback_bytes += (uint32_t)size;
    ret = Liot_AudioPlay((uint8_t *)buffer, size);
    if (ret != L_AUD_ERR_SUCCESS) {
        s_player_liot.tts_callback_errors++;
        return -1;
    }
    return 0;
}

static bool player_liot_suffix_is(const char *name, const char *suffix)
{
    size_t name_len;
    size_t suffix_len;
    size_t i;

    if (name == NULL || suffix == NULL) {
        return false;
    }
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    if (name_len < suffix_len) {
        return false;
    }

    name += name_len - suffix_len;
    for (i = 0U; i < suffix_len; i++) {
        char a = name[i];
        char b = suffix[i];

        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

static bool player_liot_file_type_from_name(const char *name,
                                            app_player_file_type_t *type)
{
    if (type == NULL) {
        return false;
    }
    if (player_liot_suffix_is(name, ".mp3")) {
        *type = APP_PLAYER_FILE_MP3;
        return true;
    }
    if (player_liot_suffix_is(name, ".wav")) {
        *type = APP_PLAYER_FILE_WAV;
        return true;
    }
    return false;
}

static void player_liot_copy_string(char *dst, uint32_t dst_size, const char *src)
{
    uint32_t i = 0U;

    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int player_liot_make_path(const char *root,
                                 const char *name,
                                 char *path,
                                 uint32_t path_size)
{
    const char *dir = (root != NULL && root[0] != '\0') ? root : "/";
    int ret;

    if (name == NULL || path == NULL || path_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (dir[0] == '/' && dir[1] == '\0') {
        ret = snprintf(path, path_size, "/%s", name);
    } else {
        size_t len = strlen(dir);

        ret = snprintf(path, path_size, "%s%s%s",
                       dir,
                       (len > 0U && dir[len - 1U] == '/') ? "" : "/",
                       name);
    }
    if (ret < 0 || (uint32_t)ret >= path_size) {
        return APP_ERR_INVALID_ARG;
    }
    return APP_OK;
}

static void player_liot_emit_progress(const app_player_file_t *file,
                                      uint32_t done,
                                      uint32_t total,
                                      bool playing,
                                      bool finished,
                                      app_player_progress_cb_t cb,
                                      void *user)
{
    app_player_progress_t progress;
    uint32_t percent = 0U;

    if (total != 0U) {
        percent = (done >= total) ? 100U : ((done * 100U) / total);
    }
    if (!finished && total != 0U && percent >= 100U) {
        percent = 99U;
    }
    if (percent > 100U) {
        percent = 100U;
    }

    memset(&progress, 0, sizeof(progress));
    progress.file = file;
    progress.bytes_done = done;
    progress.total_bytes = total;
    progress.percent = (uint8_t)percent;
    progress.playing = playing;
    progress.done = finished;
    if (cb != NULL) {
        cb(&progress, user);
    }
}

static int player_liot_audio_deinit(void)
{
    Liot_AudErr_e ret;

    if (!s_player_liot.audio_ready) {
        s_player_liot.audio_sample_valid = false;
        return APP_OK;
    }

    //(void)Liot_AudioPlayStop();
    ret = Liot_AudioDeInit();
    if (ret != L_AUD_ERR_SUCCESS) {
        app_log("player liot audio deinit failed: ret=%d", (int)ret);
        return APP_ERR_FAIL;
    }

    s_player_liot.audio_ready = false;
    s_player_liot.audio_sample_valid = false;
    app_log("player liot audio deinit ok");
    return APP_OK;
}

static int player_liot_audio_ensure_ready(Liot_AudSample_e sample)
{
    Liot_AudHwConfig_t audio_config;
    Liot_AudErr_e ret;

    if (s_player_liot.audio_ready &&
        s_player_liot.audio_sample_valid &&
        s_player_liot.audio_sample == sample) {
        return APP_OK;
    }
    if (s_player_liot.audio_ready) {
        int deinit_ret;

        if (s_player_liot.playing) {
            app_log("player liot audio switch rejected: playing old=%s new=%s",
                    player_liot_sample_name(s_player_liot.audio_sample),
                    player_liot_sample_name(sample));
            return APP_ERR_BUSY;
        }
        app_log("player liot audio switch: old=%s new=%s",
                s_player_liot.audio_sample_valid ?
                player_liot_sample_name(s_player_liot.audio_sample) : "unknown",
                player_liot_sample_name(sample));
        deinit_ret = player_liot_audio_deinit();
        if (deinit_ret != APP_OK) {
            return deinit_ret;
        }
        app_os_task_delay_ms(50U);
    }

    (void)Liot_AonPowerCtl(true);
    (void)Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.i2cNum = PLAYER_LIOT_AUDIO_I2C_NUM;
    audio_config.i2sNum = PLAYER_LIOT_AUDIO_I2S_NUM;
    audio_config.paGpioNum = PLAYER_LIOT_AUDIO_PA_GPIO;
    audio_config.codecType = L_AUD_ES8311;
    audio_config.channel = L_AUD_MONO_RIGHT;
    audio_config.role = L_AUD_ROLE_SLAVE;
    audio_config.mode = L_AUD_MODE_I2S;
    audio_config.frameSize = L_AUD_FRAMESIZE_16_16;
    audio_config.samples = sample;

    ret = Liot_AudioInit(&audio_config);
    if (ret != L_AUD_ERR_SUCCESS) {
        app_log("player liot audio init failed: sample=%s ret=%d",
                player_liot_sample_name(sample),
                (int)ret);
        return APP_ERR_FAIL;
    }
    (void)Liot_AudioSetVolume(PLAYER_LIOT_AUDIO_VOLUME);
    (void)Liot_AudioSetCodecVolume(PLAYER_LIOT_AUDIO_CODEC_VOLUME);
    s_player_liot.audio_ready = true;
    s_player_liot.audio_sample = sample;
    s_player_liot.audio_sample_valid = true;
    app_log("player liot audio ready: sample=%s",
            player_liot_sample_name(sample));
    return APP_OK;
}

static bool player_liot_stop_requested(volatile bool *external_stop)
{
    if (s_player_liot.stop_requested) {
        return true;
    }
    return external_stop != NULL && *external_stop;
}

static int player_liot_wait_finish(const char *tag)
{
    Liot_AudErr_e ret;

    ret = Liot_AudioWaitPlayFinish((int)s_player_liot.config.wait_timeout_s);
    if (ret != L_AUD_ERR_SUCCESS) {
        app_log("player liot wait failed: %s ret=%d", tag, (int)ret);
        //(void)Liot_AudioPlayStop();
        return APP_ERR_FAIL;
    }
    return APP_OK;
}

static int player_liot_play_mp3(const app_player_file_t *file,
                                LFILE_EXT fp,
                                volatile bool *stop_requested,
                                app_player_progress_cb_t progress_cb,
                                void *progress_user)
{
    uint8_t *chunk;
    uint32_t total = 0U;
    uint32_t last_percent = 255U;
    uint32_t chunk_size = s_player_liot.config.chunk_size;
    bool stream_started = false;
    int ret = APP_OK;
    Liot_AudErr_e audio_ret;

    chunk = (uint8_t *)liot_rtos_malloc(chunk_size);
    if (chunk == NULL) {
        app_log("player liot mp3 chunk alloc failed: %lu",
                (unsigned long)chunk_size);
        return APP_ERR_NO_MEMORY;
    }

    audio_ret = Liot_AudioMp3StreamStart();
    if (audio_ret != L_AUD_ERR_SUCCESS) {
        app_log("player liot mp3 stream start failed: %d", (int)audio_ret);
        liot_rtos_free(chunk);
        return APP_ERR_FAIL;
    }
    stream_started = true;
    player_liot_emit_progress(file, 0U, file->size_bytes, true, false,
                              progress_cb, progress_user);

    while (total < file->size_bytes && !player_liot_stop_requested(stop_requested)) {
        uint32_t remain = file->size_bytes - total;
        int to_read = (remain > chunk_size) ? (int)chunk_size : (int)remain;
        int read_len = liot_fread_ext(chunk, (size_t)to_read, 1, fp);

        if (read_len <= 0) {
            app_log("player liot mp3 read failed: file=%s total=%lu ret=%d",
                    file->path,
                    (unsigned long)total,
                    read_len);
            ret = APP_ERR_FAIL;
            break;
        }

        audio_ret = Liot_AudioMp3StreamPlay(chunk, read_len);
        if (audio_ret != L_AUD_ERR_SUCCESS) {
            app_log("player liot mp3 play failed: file=%s ret=%d",
                    file->path,
                    (int)audio_ret);
            ret = APP_ERR_FAIL;
            break;
        }

        total += (uint32_t)read_len;
        if (file->size_bytes != 0U) {
            uint32_t percent = (total >= file->size_bytes) ?
                               100U : ((total * 100U) / file->size_bytes);

            if (percent != last_percent) {
                last_percent = percent;
                player_liot_emit_progress(file, total, file->size_bytes,
                                          true, false, progress_cb, progress_user);
            }
        }
    }

    if (ret == APP_OK && !player_liot_stop_requested(stop_requested)) {
        ret = player_liot_wait_finish("mp3");
        if (ret == APP_OK) {
            player_liot_emit_progress(file, file->size_bytes, file->size_bytes,
                                      false, true, progress_cb, progress_user);
        }
    } else if (player_liot_stop_requested(stop_requested)) {
        //(void)Liot_AudioPlayStop();
    }

    if (stream_started) {
        (void)Liot_AudioMp3StreamStop();
    }
    liot_rtos_free(chunk);
    return ret;
}

static int player_liot_validate_wav_header(const player_liot_wav_header_t *header)
{
    if (header == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (memcmp(header->riff, "RIFF", 4) != 0 ||
        memcmp(header->wave, "WAVE", 4) != 0 ||
        memcmp(header->fmt, "fmt ", 4) != 0 ||
        memcmp(header->data, "data", 4) != 0 ||
        header->audio_format != 1U) {
        return APP_ERR_INVALID_ARG;
    }
    return APP_OK;
}

static int player_liot_play_wav(const app_player_file_t *file,
                                LFILE_EXT fp,
                                volatile bool *stop_requested,
                                app_player_progress_cb_t progress_cb,
                                void *progress_user)
{
    player_liot_wav_header_t header;
    uint8_t *chunk;
    uint32_t total = 0U;
    uint32_t remain;
    uint32_t playable;
    uint32_t last_percent = 255U;
    uint32_t chunk_size = s_player_liot.config.chunk_size;
    int ret = APP_OK;

    memset(&header, 0, sizeof(header));
    if (liot_fread_ext(&header, sizeof(header), 1, fp) != (int)sizeof(header)) {
        app_log("player liot wav header read failed: %s", file->path);
        return APP_ERR_FAIL;
    }
    ret = player_liot_validate_wav_header(&header);
    if (ret != APP_OK) {
        app_log("player liot wav header invalid: %s", file->path);
        return ret;
    }

    playable = header.data_size;
    if (file->size_bytes <= PLAYER_LIOT_WAV_HEADER_SIZE) {
        app_log("player liot wav file too small: %s size=%lu",
                file->path,
                (unsigned long)file->size_bytes);
        return APP_ERR_INVALID_ARG;
    }
    if (playable > file->size_bytes - PLAYER_LIOT_WAV_HEADER_SIZE) {
        playable = file->size_bytes - PLAYER_LIOT_WAV_HEADER_SIZE;
    }
    remain = playable;
    app_log("player liot wav info: ch=%u rate=%lu bits=%u data=%lu",
            (unsigned int)header.num_channels,
            (unsigned long)header.sample_rate,
            (unsigned int)header.bits_per_sample,
            (unsigned long)playable);

    chunk = (uint8_t *)liot_rtos_malloc(chunk_size);
    if (chunk == NULL) {
        app_log("player liot wav chunk alloc failed: %lu",
                (unsigned long)chunk_size);
        return APP_ERR_NO_MEMORY;
    }
    player_liot_emit_progress(file, 0U, playable, true, false,
                              progress_cb, progress_user);

    while (remain > 0U && !player_liot_stop_requested(stop_requested)) {
        int to_read = (remain > chunk_size) ? (int)chunk_size : (int)remain;
        int read_len = liot_fread_ext(chunk, (size_t)to_read, 1, fp);
        Liot_AudErr_e audio_ret;

        if (read_len <= 0) {
            app_log("player liot wav read failed: file=%s total=%lu ret=%d",
                    file->path,
                    (unsigned long)total,
                    read_len);
            ret = APP_ERR_FAIL;
            break;
        }

        audio_ret = Liot_AudioPlay(chunk, read_len);
        if (audio_ret != L_AUD_ERR_SUCCESS) {
            app_log("player liot wav play failed: file=%s ret=%d",
                    file->path,
                    (int)audio_ret);
            ret = APP_ERR_FAIL;
            break;
        }

        total += (uint32_t)read_len;
        remain -= (uint32_t)read_len;
        if (playable != 0U) {
            uint32_t percent = (total >= playable) ? 100U : ((total * 100U) / playable);

            if (percent != last_percent) {
                last_percent = percent;
                player_liot_emit_progress(file, total, playable,
                                          true, false, progress_cb, progress_user);
            }
        }
    }

    if (ret == APP_OK && !player_liot_stop_requested(stop_requested)) {
        ret = player_liot_wait_finish("wav");
        if (ret == APP_OK) {
            player_liot_emit_progress(file, playable, playable,
                                      false, true, progress_cb, progress_user);
        }
    } else if (player_liot_stop_requested(stop_requested)) {
        //(void)Liot_AudioPlayStop();
    }

    liot_rtos_free(chunk);
    return ret;
}

static const char *player_liot_tts_text_from_file(const app_player_file_t *file)
{
    size_t prefix_len = strlen(PLAYER_LIOT_TTS_PATH_PREFIX);

    if (file == NULL ||
        strncmp(file->path, PLAYER_LIOT_TTS_PATH_PREFIX, prefix_len) != 0 ||
        file->path[prefix_len] == '\0') {
        return NULL;
    }
    return &file->path[prefix_len];
}

static int player_liot_tts_preroll(void)
{
    uint32_t sample_hz = player_liot_sample_hz(L_AUD_08K_SAMPLES);
    uint32_t total_bytes;
    uint32_t bytes_left;

    if (!s_player_liot.audio_ready || sample_hz == 0U) {
        return APP_ERR_NOT_READY;
    }

    total_bytes = (sample_hz *
                   PLAYER_LIOT_TTS_PCM_BYTES_PER_SAMPLE *
                   PLAYER_LIOT_TTS_PREROLL_MS) / 1000U;
    bytes_left = total_bytes;
    while (bytes_left > 0U && !s_player_liot.stop_requested) {
        uint32_t chunk = (bytes_left > PLAYER_LIOT_TTS_PREROLL_CHUNK_BYTES) ?
                         PLAYER_LIOT_TTS_PREROLL_CHUNK_BYTES :
                         bytes_left;
        Liot_AudErr_e ret = Liot_AudioPlay(s_player_liot_tts_preroll_zero,
                                           (int)chunk);

        if (ret != L_AUD_ERR_SUCCESS) {
            app_log("player liot tts preroll failed: ret=%d", (int)ret);
            return APP_ERR_FAIL;
        }
        bytes_left -= chunk;
    }

    app_log("player liot tts preroll ok: ms=%lu bytes=%lu",
            (unsigned long)PLAYER_LIOT_TTS_PREROLL_MS,
            (unsigned long)total_bytes);
    return APP_OK;
}

static int player_liot_play_tts(const app_player_file_t *file,
                                volatile bool *stop_requested,
                                app_player_progress_cb_t progress_cb,
                                void *progress_user)
{
    const char *text = player_liot_tts_text_from_file(file);
    uint32_t text_len;
    bool engine_ready = false;
    int ret;
    liot_tts_errcode_e tts_ret;
    Liot_AudErr_e wait_ret;

    if (text == NULL) {
        app_log("player liot tts text invalid");
        return APP_ERR_INVALID_ARG;
    }
    text_len = (uint32_t)strlen(text);
    if (text_len == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    liot_tts_set_resource((void *)PKGFLXTTS_RES_ADDR,
                          player_liot_tts_read_resource);
    app_log("player liot tts engine init");
    tts_ret = liot_tts_engine_init(player_liot_tts_output_cb);
    if (tts_ret != LIOT_TTS_SUCCESS) {
        app_log("player liot tts engine init failed: ret=%d", (int)tts_ret);
        return APP_ERR_FAIL;
    }
    engine_ready = true;

    s_player_liot.tts_callback_chunks = 0U;
    s_player_liot.tts_callback_bytes = 0U;
    s_player_liot.tts_callback_errors = 0U;

    ret = player_liot_tts_preroll();
    if (ret != APP_OK) {
        goto cleanup;
    }
    if (player_liot_stop_requested(stop_requested)) {
        ret = APP_OK;
        goto cleanup;
    }

    player_liot_emit_progress(file, 0U, 0U, true, false,
                              progress_cb, progress_user);
    app_log("player liot tts play start: len=%lu",
            (unsigned long)text_len);

    tts_ret = liot_tts_start(text, (unsigned int)text_len);
    if (tts_ret != LIOT_TTS_SUCCESS) {
        app_log("player liot tts start failed: ret=%d chunks=%lu bytes=%lu errors=%lu",
                (int)tts_ret,
                (unsigned long)s_player_liot.tts_callback_chunks,
                (unsigned long)s_player_liot.tts_callback_bytes,
                (unsigned long)s_player_liot.tts_callback_errors);
        ret = APP_ERR_FAIL;
        goto cleanup;
    }

    if (player_liot_stop_requested(stop_requested)) {
        //(void)Liot_AudioPlayStop();
        ret = APP_OK;
        goto cleanup;
    }

    wait_ret = Liot_AudioWaitPlayFinish((int)s_player_liot.config.wait_timeout_s);
    if (wait_ret != L_AUD_ERR_SUCCESS) {
        app_log("player liot tts wait failed: ret=%d chunks=%lu bytes=%lu errors=%lu",
                (int)wait_ret,
                (unsigned long)s_player_liot.tts_callback_chunks,
                (unsigned long)s_player_liot.tts_callback_bytes,
                (unsigned long)s_player_liot.tts_callback_errors);
        //(void)Liot_AudioPlayStop();
        ret = APP_ERR_FAIL;
        goto cleanup;
    }

    ret = (s_player_liot.tts_callback_errors == 0U) ? APP_OK : APP_ERR_FAIL;
    if (ret == APP_OK) {
        player_liot_emit_progress(file, 0U, 0U, false, true,
                                  progress_cb, progress_user);
        app_log("player liot tts play done: chunks=%lu bytes=%lu",
                (unsigned long)s_player_liot.tts_callback_chunks,
                (unsigned long)s_player_liot.tts_callback_bytes);
    }

cleanup:
    if (engine_ready) {
        (void)liot_tts_end();
        (void)liot_tts_exit();
    }
    return ret;
}

static int player_liot_init(void)
{
    if (!s_player_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    if (s_player_liot.initialized) {
        return APP_OK;
    }

    s_player_liot.initialized = true;
    app_log("player liot init ok: chunk=%lu fs=board",
            (unsigned long)s_player_liot.config.chunk_size);
    return APP_OK;
}

static int player_liot_deinit(void)
{
    if (!s_player_liot.initialized) {
        return APP_OK;
    }
    s_player_liot.stop_requested = true;
    if (s_player_liot.playing || s_player_liot.audio_ready) {
        //(void)Liot_AudioPlayStop();
    }
    if (s_player_liot.audio_ready) {
        int ret = player_liot_audio_deinit();

        if (ret != APP_OK) {
            return ret;
        }
    }
    s_player_liot.initialized = false;
    s_player_liot.playing = false;
    s_player_liot.stop_requested = false;
    app_log("player liot deinit ok: fs kept mounted");
    return APP_OK;
}

static int player_liot_list_files(const char *root,
                                  app_player_file_t *files,
                                  uint32_t capacity,
                                  uint32_t *count)
{
    LDIR_EXT *dir;
    ldirent_ext *entry;
    uint32_t found = 0U;
    const char *scan_root = (root != NULL && root[0] != '\0') ? root : "/";

    if (!s_player_liot.initialized || files == NULL || capacity == 0U ||
        count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *count = 0U;

    dir = liot_opendir_ext(scan_root);
    if (dir == NULL) {
        app_log("player liot opendir failed: %s", scan_root);
        return APP_ERR_FAIL;
    }

    while ((entry = liot_readdir_ext(dir)) != NULL && found < capacity) {
        app_player_file_type_t type;
        liot_stat_ext_s st;
        char path[APP_PLAYER_FILE_PATH_MAX];

        if (entry->d_name[0] == '\0' ||
            strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !player_liot_file_type_from_name(entry->d_name, &type)) {
            continue;
        }
        if (player_liot_make_path(scan_root,
                                  entry->d_name,
                                  path,
                                  sizeof(path)) != APP_OK) {
            app_log("player liot path too long: %s", entry->d_name);
            continue;
        }
        if (liot_stat_ext(path, &st) != LIOT_EXTFLASH_OK ||
            st.type != LIOT_EXTFLASH_TYPE_FILE) {
            continue;
        }

        memset(&files[found], 0, sizeof(files[found]));
        player_liot_copy_string(files[found].name,
                                sizeof(files[found].name),
                                entry->d_name);
        player_liot_copy_string(files[found].path,
                                sizeof(files[found].path),
                                path);
        files[found].size_bytes = st.size;
        files[found].type = type;
        app_log("player liot file: index=%lu name=%s size=%lu type=%s",
                (unsigned long)found,
                files[found].name,
                (unsigned long)files[found].size_bytes,
                app_player_file_type_name(files[found].type));
        found++;
    }

    (void)liot_closedir_ext(dir);
    *count = found;
    app_log("player liot list done: root=%s count=%lu capacity=%lu",
            scan_root,
            (unsigned long)found,
            (unsigned long)capacity);
    return APP_OK;
}

static int player_liot_play_file(const app_player_file_t *file,
                                 volatile bool *stop_requested,
                                 app_player_progress_cb_t progress_cb,
                                 void *progress_user)
{
    LFILE_EXT fp;
    int ret;

    if (!s_player_liot.initialized || file == NULL || file->path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (s_player_liot.playing) {
        return APP_ERR_BUSY;
    }

    if (file->type == APP_PLAYER_FILE_TTS) {
        ret = player_liot_audio_ensure_ready(L_AUD_08K_SAMPLES);
        if (ret != APP_OK) {
            return ret;
        }

        s_player_liot.stop_requested = false;
        s_player_liot.playing = true;
        app_log("player liot play start: %s type=%s size=%lu",
                file->path,
                app_player_file_type_name(file->type),
                (unsigned long)file->size_bytes);
        ret = player_liot_play_tts(file, stop_requested, progress_cb, progress_user);
        s_player_liot.playing = false;
        if (player_liot_stop_requested(stop_requested)) {
            app_log("player liot play stopped: %s", file->path);
        } else {
            app_log("player liot play exit: %s ret=%d", file->path, ret);
        }
        s_player_liot.stop_requested = false;
        return ret;
    }

    if (file->type != APP_PLAYER_FILE_MP3 && file->type != APP_PLAYER_FILE_WAV) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = player_liot_audio_ensure_ready(L_AUD_16K_SAMPLES);
    if (ret != APP_OK) {
        return ret;
    }

    fp = liot_fopen_ext(file->path, "r");
    if (fp <= 0) {
        app_log("player liot open failed: %s fd=%d", file->path, (int)fp);
        return APP_ERR_FAIL;
    }

    s_player_liot.stop_requested = false;
    s_player_liot.playing = true;
    app_log("player liot play start: %s type=%s size=%lu",
            file->path,
            app_player_file_type_name(file->type),
            (unsigned long)file->size_bytes);

    if (file->type == APP_PLAYER_FILE_MP3) {
        ret = player_liot_play_mp3(file, fp, stop_requested, progress_cb, progress_user);
    } else {
        ret = player_liot_play_wav(file, fp, stop_requested, progress_cb, progress_user);
    }

    (void)liot_fclose_ext(fp);
    s_player_liot.playing = false;
    if (player_liot_stop_requested(stop_requested)) {
        app_log("player liot play stopped: %s", file->path);
    } else {
        app_log("player liot play exit: %s ret=%d", file->path, ret);
    }
    s_player_liot.stop_requested = false;
    return ret;
}

static int player_liot_stop(void)
{
    s_player_liot.stop_requested = true;
    //(void)Liot_AudioPlayStop();
    app_log("player liot stop");
    return APP_OK;
}

int app_player_liot_get_default_config(app_player_liot_config_t *config)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    memset(config, 0, sizeof(*config));
    config->chunk_size = PLAYER_LIOT_DEFAULT_CHUNK_SIZE;
    config->wait_timeout_s = PLAYER_LIOT_DEFAULT_WAIT_TIMEOUT_S;
    return APP_OK;
}

int app_player_liot_setup(const app_player_liot_config_t *config)
{
    app_player_liot_config_t effective_config;

    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    effective_config = *config;
    if (effective_config.chunk_size == 0U) {
        effective_config.chunk_size = PLAYER_LIOT_DEFAULT_CHUNK_SIZE;
    }
    if (effective_config.wait_timeout_s == 0U) {
        effective_config.wait_timeout_s = PLAYER_LIOT_DEFAULT_WAIT_TIMEOUT_S;
    }

    memset(&s_player_liot, 0, sizeof(s_player_liot));
    s_player_liot.config = effective_config;
    s_player_liot.configured = true;
    app_log("player liot setup: chunk=%lu wait=%lu fs=board",
            (unsigned long)s_player_liot.config.chunk_size,
            (unsigned long)s_player_liot.config.wait_timeout_s);
    return APP_OK;
}

int app_player_liot_register(void)
{
    int ret;

    if (!s_player_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    ret = app_player_register_port(&s_player_liot_port);
    if (ret != APP_OK) {
        app_log("player liot register failed: %d", ret);
        return ret;
    }
    app_log("player liot port registered");
    return APP_OK;
}

const app_player_port_t *app_player_liot_port(void)
{
    return &s_player_liot_port;
}
