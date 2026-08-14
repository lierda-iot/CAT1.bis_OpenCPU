#include "app_recorder_port_liot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_osal.h"
#include "liot_audio2.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define RECORDER_LIOT_DEFAULT_CHUNK_BYTES 1280U
#define RECORDER_LIOT_WAV_HEADER_SIZE 44U
#define RECORDER_LIOT_PCM_BYTES_PER_SAMPLE 2U
#define RECORDER_LIOT_AUDIO_I2C_NUM 0
#define RECORDER_LIOT_AUDIO_I2S_NUM 0
#define RECORDER_LIOT_AUDIO_PA_GPIO 11
#define RECORDER_LIOT_AUDIO_VOLUME 50
#define RECORDER_LIOT_AUDIO_CODEC_VOLUME 50
#define RECORDER_LIOT_CHANNELS 1U
#define RECORDER_LIOT_BITS_PER_SAMPLE 16U
#define RECORDER_LIOT_FREE_RESERVE_BYTES (128U * 1024U)
#define RECORDER_LIOT_FREE_CHECK_INTERVAL_BYTES (32U * 1024U)

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
} recorder_liot_wav_header_t;

typedef struct {
    app_recorder_liot_config_t config;
    bool configured;
    bool initialized;
    bool audio_ready;
    bool recording;
    volatile bool stop_requested;
    Liot_AudSample_e audio_sample;
    bool audio_sample_valid;
} recorder_liot_ctx_t;

static int recorder_liot_init(void);
static int recorder_liot_deinit(void);
static int recorder_liot_file_exists(const char *path, bool *exists);
static int recorder_liot_record_file(const app_recorder_file_t *file,
                                     volatile bool *stop_requested,
                                     app_recorder_progress_cb_t progress_cb,
                                     void *progress_user);
static int recorder_liot_stop(void);

static recorder_liot_ctx_t s_recorder_liot;

static const app_recorder_port_ops_t s_recorder_liot_ops = {
    .init = recorder_liot_init,
    .deinit = recorder_liot_deinit,
    .file_exists = recorder_liot_file_exists,
    .record_file = recorder_liot_record_file,
    .stop = recorder_liot_stop,
};

static app_recorder_port_t s_recorder_liot_port = {
    .ops = &s_recorder_liot_ops,
};

static const char *recorder_liot_sample_name(Liot_AudSample_e sample)
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

static uint8_t recorder_liot_level_from_pcm(const int16_t *pcm, uint32_t samples)
{
    int32_t peak = 0;
    uint32_t i;

    if (pcm == NULL || samples == 0U) {
        return 0U;
    }

    for (i = 0U; i < samples; i++) {
        int32_t value = pcm[i];

        if (value < 0) {
            value = -value;
        }
        if (value > peak) {
            peak = value;
        }
    }
    if (peak > 32767) {
        peak = 32767;
    }
    return (uint8_t)((peak * 100) / 32767);
}

static uint32_t recorder_liot_duration_ms(const app_recorder_file_t *file,
                                          uint32_t bytes_done)
{
    uint64_t bytes_per_sec;

    if (file == NULL || file->sample_rate_hz == 0U ||
        file->channels == 0U || file->bits_per_sample == 0U) {
        return 0U;
    }

    bytes_per_sec = (uint64_t)file->sample_rate_hz *
                    (uint64_t)file->channels *
                    (uint64_t)(file->bits_per_sample / 8U);
    if (bytes_per_sec == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)bytes_done * 1000U) / bytes_per_sec);
}

static void recorder_liot_emit_progress(const app_recorder_file_t *file,
                                        uint32_t bytes_done,
                                        uint8_t level,
                                        bool recording,
                                        bool done,
                                        app_recorder_stop_reason_t stop_reason,
                                        app_recorder_progress_cb_t progress_cb,
                                        void *progress_user)
{
    app_recorder_progress_t progress;

    if (file == NULL) {
        return;
    }

    memset(&progress, 0, sizeof(progress));
    progress.file = file;
    progress.bytes_done = bytes_done;
    progress.duration_ms = recorder_liot_duration_ms(file, bytes_done);
    progress.level = level;
    progress.recording = recording;
    progress.done = done;
    progress.stop_reason = stop_reason;
    if (progress_cb != NULL) {
        progress_cb(&progress, progress_user);
    }
}

static bool recorder_liot_stop_requested(volatile bool *external_stop)
{
    if (s_recorder_liot.stop_requested) {
        return true;
    }
    return external_stop != NULL && *external_stop;
}

static int recorder_liot_get_free_size(uint32_t *free_bytes)
{
    int ret;

    if (free_bytes == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ret = liot_exflash_free_size_get();
    if (ret < 0) {
        app_log("recorder liot free size failed: ret=%d", ret);
        return APP_ERR_FAIL;
    }

    *free_bytes = (uint32_t)ret;
    return APP_OK;
}

static bool recorder_liot_has_record_space(uint32_t free_bytes,
                                           uint32_t chunk_bytes)
{
    uint32_t required = RECORDER_LIOT_FREE_RESERVE_BYTES +
                        RECORDER_LIOT_WAV_HEADER_SIZE +
                        chunk_bytes;

    return free_bytes > required;
}

static int recorder_liot_wav_write_header(LFILE_EXT fp,
                                          const app_recorder_file_t *file,
                                          uint32_t data_bytes)
{
    recorder_liot_wav_header_t header;

    if (fp <= 0 || file == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(&header, 0, sizeof(header));
    memcpy(header.riff, "RIFF", 4);
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    memcpy(header.data, "data", 4);
    header.file_size = data_bytes + RECORDER_LIOT_WAV_HEADER_SIZE - 8U;
    header.fmt_size = 16U;
    header.audio_format = 1U;
    header.num_channels = file->channels;
    header.sample_rate = file->sample_rate_hz;
    header.byte_rate = file->sample_rate_hz *
                       file->channels *
                       (file->bits_per_sample / 8U);
    header.block_align = (uint16_t)(file->channels * (file->bits_per_sample / 8U));
    header.bits_per_sample = file->bits_per_sample;
    header.data_size = data_bytes;

    app_log("recorder liot header step: seek begin file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);
    if (liot_fseek_ext(fp, 0, LIOT_EXTFLASH_SEEK_SET) < 0) {
        app_log("recorder liot seek header failed: %s", file->path);
        return APP_ERR_FAIL;
    }
    app_log("recorder liot header step: seek end file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);

    app_log("recorder liot header step: write begin file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);
    if (liot_fwrite_ext(&header, sizeof(header), 1, fp) != (int)sizeof(header)) {
        app_log("recorder liot header write failed: %s", file->path);
        return APP_ERR_FAIL;
    }
    app_log("recorder liot header step: write end file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);

    app_log("recorder liot header step: sync begin file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);
    // if (liot_fsync_ext(fp) != LIOT_EXTFLASH_OK) {
    //     app_log("recorder liot header sync failed: %s", file->path);
    //     return APP_ERR_FAIL;
    // }
    app_log("recorder liot header step: sync end file=%s bytes=%lu",
            file->path,
            (unsigned long)data_bytes);
    return APP_OK;
}

static int recorder_liot_audio_deinit(void)
{
    Liot_AudErr_e ret;

    if (!s_recorder_liot.audio_ready) {
        s_recorder_liot.audio_sample_valid = false;
        return APP_OK;
    }

    app_log("recorder liot audio deinit begin");
    //(void)Liot_AudioRecordStop();
    ret = Liot_AudioDeInit();
    if (ret != L_AUD_ERR_SUCCESS) {
        app_log("recorder liot audio deinit failed: ret=%d", (int)ret);
        return APP_ERR_FAIL;
    }

    s_recorder_liot.audio_ready = false;
    s_recorder_liot.audio_sample_valid = false;
    app_log("recorder liot audio deinit ok");
    return APP_OK;
}

static int recorder_liot_audio_ensure_ready(const app_recorder_file_t *file)
{
    Liot_AudHwConfig_t audio_config;
    Liot_AudSample_e sample = L_AUD_16K_SAMPLES;
    Liot_AudErr_e ret;

    if (file == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (file->sample_rate_hz == 8000U) {
        sample = L_AUD_08K_SAMPLES;
    } else if (file->sample_rate_hz == 16000U) {
        sample = L_AUD_16K_SAMPLES;
    } else {
        return APP_ERR_NOT_SUPPORTED;
    }
    if (file->channels != RECORDER_LIOT_CHANNELS ||
        file->bits_per_sample != RECORDER_LIOT_BITS_PER_SAMPLE) {
        return APP_ERR_NOT_SUPPORTED;
    }
    if (s_recorder_liot.audio_ready &&
        s_recorder_liot.audio_sample_valid &&
        s_recorder_liot.audio_sample == sample) {
        return APP_OK;
    }
    if (s_recorder_liot.audio_ready) {
        int deinit_ret = recorder_liot_audio_deinit();

        if (deinit_ret != APP_OK) {
            return deinit_ret;
        }
    }

    (void)Liot_AonPowerCtl(true);
    (void)Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.i2cNum = RECORDER_LIOT_AUDIO_I2C_NUM;
    audio_config.i2sNum = RECORDER_LIOT_AUDIO_I2S_NUM;
    audio_config.paGpioNum = RECORDER_LIOT_AUDIO_PA_GPIO;
    audio_config.codecType = L_AUD_ES8311;
    audio_config.channel = L_AUD_MONO_RIGHT;
    audio_config.role = L_AUD_ROLE_SLAVE;
    audio_config.mode = L_AUD_MODE_I2S;
    audio_config.frameSize = L_AUD_FRAMESIZE_16_16;
    audio_config.samples = sample;

    ret = Liot_AudioInit(&audio_config);
    if (ret != L_AUD_ERR_SUCCESS) {
        app_log("recorder liot audio init failed: sample=%s ret=%d",
                recorder_liot_sample_name(sample),
                (int)ret);
        return APP_ERR_FAIL;
    }
    //(void)Liot_AudioSetVolume(RECORDER_LIOT_AUDIO_VOLUME);
    //(void)Liot_AudioSetCodecVolume(RECORDER_LIOT_AUDIO_CODEC_VOLUME);
    (void)Liot_AudioSetMicVolume(8U, 200);
    s_recorder_liot.audio_ready = true;
    s_recorder_liot.audio_sample = sample;
    s_recorder_liot.audio_sample_valid = true;
    app_log("recorder liot audio ready: sample=%s",
            recorder_liot_sample_name(sample));
    return APP_OK;
}

static bool recorder_liot_file_supported(const app_recorder_file_t *file)
{
    if (file == NULL) {
        return false;
    }
    if ((file->sample_rate_hz != 8000U && file->sample_rate_hz != 16000U) ||
        file->channels != RECORDER_LIOT_CHANNELS ||
        file->bits_per_sample != RECORDER_LIOT_BITS_PER_SAMPLE) {
        return false;
    }
    return true;
}

static int recorder_liot_file_exists(const char *path, bool *exists)
{
    int ret;

    if (path == NULL || path[0] == '\0' || exists == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ret = liot_file_exist_ext(path);
    if (ret == LIOT_EXTFLASH_OK) {
        *exists = true;
        return APP_OK;
    }
    if (ret == LIOT_EXTFLASH_NOT_EXIST) {
        *exists = false;
        return APP_OK;
    }

    app_log("recorder liot file exists failed: %s ret=%d", path, ret);
    return APP_ERR_FAIL;
}

static int recorder_liot_record_file(const app_recorder_file_t *file,
                                     volatile bool *stop_requested,
                                     app_recorder_progress_cb_t progress_cb,
                                     void *progress_user)
{
    LFILE_EXT fp = 0;
    uint8_t *chunk = NULL;
    uint32_t bytes_done = 0U;
    uint32_t chunk_bytes;
    uint32_t next_free_check_bytes = 0U;
    uint32_t free_bytes = 0U;
    bool audio_ready_here = false;
    bool stopped_no_space = false;
    int ret = APP_OK;

    if (!s_recorder_liot.initialized || file == NULL || file->path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (s_recorder_liot.recording) {
        return APP_ERR_BUSY;
    }
    if (!recorder_liot_file_supported(file)) {
        app_log("recorder liot unsupported format: %s %lu Hz %u ch %u bit",
                file->path,
                (unsigned long)file->sample_rate_hz,
                (unsigned int)file->channels,
                (unsigned int)file->bits_per_sample);
        return APP_ERR_NOT_SUPPORTED;
    }

    chunk_bytes = (file->chunk_bytes != 0U) ? file->chunk_bytes : s_recorder_liot.config.chunk_bytes;
    if (chunk_bytes == 0U) {
        chunk_bytes = RECORDER_LIOT_DEFAULT_CHUNK_BYTES;
    }
    if ((chunk_bytes & 1U) != 0U) {
        chunk_bytes++;
    }

    ret = recorder_liot_get_free_size(&free_bytes);
    if (ret != APP_OK) {
        return ret;
    }
    if (!recorder_liot_has_record_space(free_bytes, chunk_bytes)) {
        app_log("recorder liot no space before start: file=%s free=%lu reserve=%lu chunk=%lu",
                file->path,
                (unsigned long)free_bytes,
                (unsigned long)RECORDER_LIOT_FREE_RESERVE_BYTES,
                (unsigned long)chunk_bytes);
        return APP_ERR_FAIL;
    }

    ret = recorder_liot_audio_ensure_ready(file);
    if (ret != APP_OK) {
        return ret;
    }
    audio_ready_here = true;

    fp = liot_fopen_ext(file->path, "w+");
    if (fp <= 0) {
        app_log("recorder liot open failed: %s fd=%d", file->path, (int)fp);
        ret = APP_ERR_FAIL;
        goto cleanup;
    }

    if (recorder_liot_wav_write_header(fp, file, 0U) != APP_OK) {
        ret = APP_ERR_FAIL;
        goto cleanup;
    }

    chunk = (uint8_t *)app_os_malloc(chunk_bytes);
    if (chunk == NULL) {
        app_log("recorder liot chunk alloc failed: %lu", (unsigned long)chunk_bytes);
        ret = APP_ERR_NO_MEMORY;
        goto cleanup;
    }

    memset(chunk, 0, chunk_bytes);
    s_recorder_liot.stop_requested = false;
    s_recorder_liot.recording = true;
    recorder_liot_emit_progress(file,
                                0U,
                                0U,
                                true,
                                false,
                                APP_RECORDER_STOP_REASON_NONE,
                                progress_cb,
                                progress_user);
    app_log("recorder liot record start: file=%s chunk=%lu free=%lu reserve=%lu sample=%s",
            file->path,
            (unsigned long)chunk_bytes,
            (unsigned long)free_bytes,
            (unsigned long)RECORDER_LIOT_FREE_RESERVE_BYTES,
            recorder_liot_sample_name(s_recorder_liot.audio_sample));

    while (!recorder_liot_stop_requested(stop_requested)) {
        Liot_AudErr_e audio_ret;
        int write_ret;
        uint8_t level;

        if (bytes_done >= next_free_check_bytes) {
            ret = recorder_liot_get_free_size(&free_bytes);
            if (ret != APP_OK) {
                break;
            }
            if (!recorder_liot_has_record_space(free_bytes, chunk_bytes)) {
                stopped_no_space = true;
                ret = APP_OK;
                app_log("recorder liot stop: no space file=%s free=%lu reserve=%lu bytes=%lu",
                        file->path,
                        (unsigned long)free_bytes,
                        (unsigned long)RECORDER_LIOT_FREE_RESERVE_BYTES,
                        (unsigned long)bytes_done);
                break;
            }
            next_free_check_bytes = bytes_done + RECORDER_LIOT_FREE_CHECK_INTERVAL_BYTES;
        }

        memset(chunk, 0, chunk_bytes);
        audio_ret = Liot_AudioRecord(chunk, (int)chunk_bytes);
        if (audio_ret != L_AUD_ERR_SUCCESS) {
            if (recorder_liot_stop_requested(stop_requested)) {
                ret = APP_OK;
                break;
            }
            app_log("recorder liot record failed: file=%s ret=%d bytes=%lu",
                    file->path,
                    (int)audio_ret,
                    (unsigned long)bytes_done);
            ret = APP_ERR_FAIL;
            break;
        }

        write_ret = liot_fwrite_ext(chunk, chunk_bytes, 1, fp);
        if (write_ret != (int)chunk_bytes) {
            uint32_t free_after_fail = 0U;

            app_log("recorder liot write failed: file=%s ret=%d bytes=%lu",
                    file->path,
                    write_ret,
                    (unsigned long)bytes_done);
            if (recorder_liot_get_free_size(&free_after_fail) == APP_OK &&
                !recorder_liot_has_record_space(free_after_fail, chunk_bytes)) {
                stopped_no_space = true;
                ret = APP_OK;
                app_log("recorder liot write stopped: no space file=%s free=%lu bytes=%lu",
                        file->path,
                        (unsigned long)free_after_fail,
                        (unsigned long)bytes_done);
            } else {
                ret = APP_ERR_FAIL;
            }
            break;
        }

        bytes_done += chunk_bytes;
        level = recorder_liot_level_from_pcm((const int16_t *)chunk,
                                             chunk_bytes / RECORDER_LIOT_PCM_BYTES_PER_SAMPLE);
        recorder_liot_emit_progress(file,
                                    bytes_done,
                                    level,
                                    true,
                                    false,
                                    APP_RECORDER_STOP_REASON_NONE,
                                    progress_cb, progress_user);
    }

    if (s_recorder_liot.recording) {
        //(void)Liot_AudioRecordStop();
    }

    if (bytes_done > 0U) {
        app_log("recorder liot finalize step: truncate begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        int truncate_ret = liot_ftruncate_ext(fp,
                                              RECORDER_LIOT_WAV_HEADER_SIZE + bytes_done);

        if (truncate_ret != LIOT_EXTFLASH_OK) {
            app_log("recorder liot truncate failed: file=%s ret=%d bytes=%lu",
                    file->path,
                    truncate_ret,
                    (unsigned long)bytes_done);
        }
        app_log("recorder liot finalize step: truncate end file=%s bytes=%lu ret=%d",
                file->path,
                (unsigned long)bytes_done,
                truncate_ret);

        app_log("recorder liot finalize step: header write begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        if (recorder_liot_wav_write_header(fp, file, bytes_done) != APP_OK) {
            ret = APP_ERR_FAIL;
        }
        app_log("recorder liot finalize step: header write end file=%s bytes=%lu ret=%d",
                file->path,
                (unsigned long)bytes_done,
                ret);
    }

    if (fp > 0) {
        app_log("recorder liot finalize step: fsync begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        //(void)liot_fsync_ext(fp);
        app_log("recorder liot finalize step: fsync end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        app_log("recorder liot finalize step: fclose begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        (void)liot_fclose_ext(fp);
        app_log("recorder liot finalize step: fclose end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        fp = 0;
    }
    if (bytes_done == 0U) {
        (void)liot_remove_ext(file->path);
    }
    if (chunk != NULL) {
        app_os_free(chunk);
        chunk = NULL;
    }
    if (audio_ready_here) {
        app_log("recorder liot finalize step: audio deinit begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        (void)recorder_liot_audio_deinit();
        app_log("recorder liot finalize step: audio deinit end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        audio_ready_here = false;
    }

    s_recorder_liot.recording = false;
    s_recorder_liot.stop_requested = false;

    recorder_liot_emit_progress(file,
                                bytes_done,
                                0U,
                                false,
                                ret == APP_OK,
                                (ret != APP_OK) ? APP_RECORDER_STOP_REASON_ERROR :
                                (stopped_no_space ? APP_RECORDER_STOP_REASON_NO_SPACE :
                                 APP_RECORDER_STOP_REASON_NORMAL),
                                progress_cb,
                                progress_user);

    if (ret == APP_OK) {
        app_log("recorder liot record done: file=%s bytes=%lu reason=%s",
                file->path,
                (unsigned long)bytes_done,
                stopped_no_space ? "no_space" : "normal");
    }
    return ret;

cleanup:
    if (s_recorder_liot.recording) {
        //(void)Liot_AudioRecordStop();
    }
    if (fp > 0) {
        app_log("recorder liot cleanup step: header rewrite begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        if (bytes_done > 0U) {
            (void)recorder_liot_wav_write_header(fp, file, bytes_done);
        }
        app_log("recorder liot cleanup step: header rewrite end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        app_log("recorder liot cleanup step: fclose begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        (void)liot_fclose_ext(fp);
        app_log("recorder liot cleanup step: fclose end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        fp = 0;
    }
    if (bytes_done == 0U) {
        (void)liot_remove_ext(file->path);
    }
    if (chunk != NULL) {
        app_os_free(chunk);
    }
    if (audio_ready_here) {
        app_log("recorder liot cleanup step: audio deinit begin file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
        (void)recorder_liot_audio_deinit();
        app_log("recorder liot cleanup step: audio deinit end file=%s bytes=%lu",
                file->path,
                (unsigned long)bytes_done);
    }
    s_recorder_liot.recording = false;
    s_recorder_liot.stop_requested = false;
    return ret;
}

static int recorder_liot_init(void)
{
    if (!s_recorder_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    if (s_recorder_liot.initialized) {
        return APP_OK;
    }

    s_recorder_liot.initialized = true;
    app_log("recorder liot init ok: chunk=%lu",
            (unsigned long)s_recorder_liot.config.chunk_bytes);
    return APP_OK;
}

static int recorder_liot_deinit(void)
{
    if (!s_recorder_liot.initialized) {
        return APP_OK;
    }

    s_recorder_liot.stop_requested = true;
    if (s_recorder_liot.audio_ready || s_recorder_liot.recording) {
        //(void)Liot_AudioRecordStop();
    }
    if (s_recorder_liot.audio_ready) {
        int ret = recorder_liot_audio_deinit();

        if (ret != APP_OK) {
            return ret;
        }
    }
    s_recorder_liot.initialized = false;
    s_recorder_liot.recording = false;
    s_recorder_liot.stop_requested = false;
    app_log("recorder liot deinit ok");
    return APP_OK;
}

static int recorder_liot_stop(void)
{
    s_recorder_liot.stop_requested = true;
    //(void)Liot_AudioRecordStop();
    app_log("recorder liot stop");
    return APP_OK;
}

int app_recorder_liot_get_default_config(app_recorder_liot_config_t *config)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    config->chunk_bytes = RECORDER_LIOT_DEFAULT_CHUNK_BYTES;
    return APP_OK;
}

int app_recorder_liot_setup(const app_recorder_liot_config_t *config)
{
    app_recorder_liot_config_t effective_config;

    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    effective_config = *config;
    if (effective_config.chunk_bytes == 0U) {
        effective_config.chunk_bytes = RECORDER_LIOT_DEFAULT_CHUNK_BYTES;
    }
    if ((effective_config.chunk_bytes & 1U) != 0U) {
        effective_config.chunk_bytes++;
    }

    memset(&s_recorder_liot, 0, sizeof(s_recorder_liot));
    s_recorder_liot.config = effective_config;
    s_recorder_liot.configured = true;
    app_log("recorder liot setup: chunk=%lu",
            (unsigned long)s_recorder_liot.config.chunk_bytes);
    return APP_OK;
}

int app_recorder_liot_register(void)
{
    int ret;

    if (!s_recorder_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    ret = app_recorder_register_port(&s_recorder_liot_port);
    if (ret != APP_OK) {
        app_log("recorder liot register failed: %d", ret);
        return ret;
    }
    app_log("recorder liot port registered");
    return APP_OK;
}

const app_recorder_port_t *app_recorder_liot_port(void)
{
    return &s_recorder_liot_port;
}
