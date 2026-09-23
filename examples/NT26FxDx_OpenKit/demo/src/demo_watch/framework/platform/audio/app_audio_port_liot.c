#include "app_audio_port_liot.h"

#include <stddef.h>
#include <string.h>

#include "app_media_service.h"
#include "app_osal.h"

#define APP_AUDIO_LIOT_DEFAULT_SAMPLE_RATE_HZ 16000U
#define APP_AUDIO_LIOT_DEFAULT_FRAME_MS 20U
#define APP_AUDIO_LIOT_DEFAULT_FRAME_SAMPLES 320U
#define APP_AUDIO_LIOT_DEFAULT_MAX_ENCODED_BYTES 256U
#define APP_AUDIO_LIOT_DEFAULT_UART_BAUDRATE 115200U
#define APP_AUDIO_LIOT_DEFAULT_VOLUME 70U
#define APP_AUDIO_LIOT_DEFAULT_VAD_TIMEOUT_S 5U
#define APP_AUDIO_LIOT_GX8006_MAX_OPUS_BYTES 249U
#define APP_AUDIO_LIOT_DEFAULT_BACKEND APP_AUDIO_LIOT_BACKEND_GX8006
#define APP_AUDIO_LIOT_DEFAULT_POWER_GPIO 25
#define APP_AUDIO_LIOT_DEFAULT_POWER_PIN 106
#define APP_AUDIO_LIOT_DEFAULT_POWER_PIN_FUNC 0
#define APP_AUDIO_LIOT_DEFAULT_POWER_ON_DELAY_MS 500U
#define APP_AUDIO_LIOT_DEFAULT_RESET_GPIO 28
#define APP_AUDIO_LIOT_DEFAULT_RESET_PIN 78
#define APP_AUDIO_LIOT_DEFAULT_RESET_PIN_FUNC 0
#define APP_AUDIO_LIOT_DEFAULT_BOOT_GPIO 4
#define APP_AUDIO_LIOT_DEFAULT_BOOT_PIN 80
#define APP_AUDIO_LIOT_DEFAULT_BOOT_PIN_FUNC 0
#define APP_AUDIO_LIOT_DEFAULT_PA_GPIO 17
#define APP_AUDIO_LIOT_DEFAULT_PA_PIN 100
#define APP_AUDIO_LIOT_DEFAULT_PA_PIN_FUNC 4
#define APP_AUDIO_LIOT_DEFAULT_UART_PORT 1
#define APP_AUDIO_LIOT_DEFAULT_UART_TX_PIN 18
#define APP_AUDIO_LIOT_DEFAULT_UART_TX_FUNC 1
#define APP_AUDIO_LIOT_DEFAULT_UART_RX_PIN 17
#define APP_AUDIO_LIOT_DEFAULT_UART_RX_FUNC 1

#ifndef APP_AUDIO_LIOT_ENABLE_GX8006
#define APP_AUDIO_LIOT_ENABLE_GX8006 0
#endif

#ifndef APP_AUDIO_LIOT_ENABLE_I2S_CODEC
#define APP_AUDIO_LIOT_ENABLE_I2S_CODEC 0
#endif

#if APP_AUDIO_LIOT_ENABLE_GX8006
#include "gx8006.h"
#include "liot_gpio2.h"
#endif

typedef struct {
    app_audio_liot_config_t config;
    app_audio_event_cb_t event_cb;
    void *event_user;
    bool configured;
    bool initialized;
    bool running;
    bool recording;
    bool playing;
    bool media_ready;
} app_audio_liot_ctx_t;

static int audio_liot_init(const app_audio_caps_t *caps);
static int audio_liot_deinit(void);
static int audio_liot_start(void);
static int audio_liot_stop(void);
static int audio_liot_start_record(void);
static int audio_liot_stop_record(void);
static int audio_liot_read_pcm(int16_t *pcm, uint32_t sample_capacity, uint32_t *sample_count);
static int audio_liot_play_pcm(const int16_t *pcm, uint32_t samples);
static int audio_liot_play_opus(const uint8_t *data, uint32_t len);
static int audio_liot_encode_opus(const int16_t *pcm,
                                  uint32_t samples,
                                  uint8_t *out,
                                  uint32_t out_size,
                                  uint32_t *out_len);
static int audio_liot_decode_opus(const uint8_t *data,
                                  uint32_t len,
                                  int16_t *pcm,
                                  uint32_t sample_capacity,
                                  uint32_t *sample_count);
static int audio_liot_play_prompt(const char *name);
static int audio_liot_set_event_cb(app_audio_event_cb_t cb, void *user);

static app_audio_liot_ctx_t s_audio_liot;

static const app_audio_port_ops_t s_audio_liot_ops = {
    .init = audio_liot_init,
    .deinit = audio_liot_deinit,
    .start = audio_liot_start,
    .stop = audio_liot_stop,
    .start_record = audio_liot_start_record,
    .stop_record = audio_liot_stop_record,
    .read_pcm = audio_liot_read_pcm,
    .play_pcm = audio_liot_play_pcm,
    .play_opus = audio_liot_play_opus,
    .encode_opus = audio_liot_encode_opus,
    .decode_opus = audio_liot_decode_opus,
    .play_prompt = audio_liot_play_prompt,
    .set_event_cb = audio_liot_set_event_cb,
};

static app_audio_port_t s_audio_liot_port = {
    .ops = &s_audio_liot_ops,
};

static void audio_liot_emit_event(app_audio_event_id_t id, int code, const char *message)
{
    app_audio_event_t event;

    if (s_audio_liot.event_cb == NULL) {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.id = id;
    event.code = code;
    event.message = message;
    s_audio_liot.event_cb(&event, s_audio_liot.event_user);
}

static app_audio_format_t audio_liot_default_format(void)
{
    app_audio_format_t format;

    memset(&format, 0, sizeof(format));
    format.sample_rate_hz = APP_AUDIO_LIOT_DEFAULT_SAMPLE_RATE_HZ;
    format.channels = 1U;
    format.bits_per_sample = 16U;
    format.frame_ms = APP_AUDIO_LIOT_DEFAULT_FRAME_MS;
    return format;
}

static bool audio_liot_format_valid(const app_audio_format_t *format)
{
    if (format == NULL) {
        return false;
    }
    if (format->sample_rate_hz == 0U || format->channels == 0U ||
        format->bits_per_sample == 0U || format->frame_ms == 0U) {
        return false;
    }
    return true;
}

static bool audio_liot_backend_available(app_audio_liot_backend_t backend)
{
    switch (backend) {
    case APP_AUDIO_LIOT_BACKEND_NONE:
        return true;
    case APP_AUDIO_LIOT_BACKEND_GX8006:
        return APP_AUDIO_LIOT_ENABLE_GX8006 != 0;
    case APP_AUDIO_LIOT_BACKEND_I2S_CODEC:
        return APP_AUDIO_LIOT_ENABLE_I2S_CODEC != 0;
    default:
        return false;
    }
}

static bool audio_liot_backend_configurable(app_audio_liot_backend_t backend)
{
    switch (backend) {
    case APP_AUDIO_LIOT_BACKEND_NONE:
    case APP_AUDIO_LIOT_BACKEND_GX8006:
    case APP_AUDIO_LIOT_BACKEND_I2S_CODEC:
        return true;
    default:
        return false;
    }
}

static void audio_liot_apply_default_config(app_audio_liot_config_t *config)
{
    app_audio_format_t default_format = audio_liot_default_format();

    if (config == NULL) {
        return;
    }
    if (!audio_liot_format_valid(&config->record_format)) {
        config->record_format = default_format;
    }
    if (!audio_liot_format_valid(&config->playback_format)) {
        config->playback_format = default_format;
    }
    if (config->frame_samples == 0U) {
        config->frame_samples = APP_AUDIO_LIOT_DEFAULT_FRAME_SAMPLES;
    }
    if (config->max_encoded_bytes == 0U) {
        config->max_encoded_bytes = APP_AUDIO_LIOT_DEFAULT_MAX_ENCODED_BYTES;
    }
    if (config->uart_baudrate == 0U) {
        config->uart_baudrate = APP_AUDIO_LIOT_DEFAULT_UART_BAUDRATE;
    }
    if (config->default_volume == 0U) {
        config->default_volume = APP_AUDIO_LIOT_DEFAULT_VOLUME;
    }
    if (config->vad_timeout_s == 0U) {
        config->vad_timeout_s = APP_AUDIO_LIOT_DEFAULT_VAD_TIMEOUT_S;
    }
}

int app_audio_liot_get_default_config(app_audio_liot_config_t *config)
{
    app_audio_format_t default_format;

    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    default_format = audio_liot_default_format();
    memset(config, 0, sizeof(*config));
    config->backend = APP_AUDIO_LIOT_DEFAULT_BACKEND;
    config->record_format = default_format;
    config->playback_format = default_format;
    config->frame_samples = APP_AUDIO_LIOT_DEFAULT_FRAME_SAMPLES;
    config->max_encoded_bytes = APP_AUDIO_LIOT_DEFAULT_MAX_ENCODED_BYTES;
    config->enable_record = true;
    config->enable_playback = true;
    config->enable_opus_encode = false;
    config->enable_opus_decode = true;
    config->enable_vad = true;
    config->power_gpio = APP_AUDIO_LIOT_DEFAULT_POWER_GPIO;
    config->power_pin = APP_AUDIO_LIOT_DEFAULT_POWER_PIN;
    config->power_pin_func = APP_AUDIO_LIOT_DEFAULT_POWER_PIN_FUNC;
    config->power_on_delay_ms = APP_AUDIO_LIOT_DEFAULT_POWER_ON_DELAY_MS;
    config->reset_gpio = APP_AUDIO_LIOT_DEFAULT_RESET_GPIO;
    config->reset_pin = APP_AUDIO_LIOT_DEFAULT_RESET_PIN;
    config->reset_pin_func = APP_AUDIO_LIOT_DEFAULT_RESET_PIN_FUNC;
    config->boot_gpio = APP_AUDIO_LIOT_DEFAULT_BOOT_GPIO;
    config->boot_pin = APP_AUDIO_LIOT_DEFAULT_BOOT_PIN;
    config->boot_pin_func = APP_AUDIO_LIOT_DEFAULT_BOOT_PIN_FUNC;
    config->pa_gpio = APP_AUDIO_LIOT_DEFAULT_PA_GPIO;
    config->pa_pin = APP_AUDIO_LIOT_DEFAULT_PA_PIN;
    config->pa_pin_func = APP_AUDIO_LIOT_DEFAULT_PA_PIN_FUNC;
    config->uart_port = APP_AUDIO_LIOT_DEFAULT_UART_PORT;
    config->uart_tx_pin = APP_AUDIO_LIOT_DEFAULT_UART_TX_PIN;
    config->uart_tx_func = APP_AUDIO_LIOT_DEFAULT_UART_TX_FUNC;
    config->uart_rx_pin = APP_AUDIO_LIOT_DEFAULT_UART_RX_PIN;
    config->uart_rx_func = APP_AUDIO_LIOT_DEFAULT_UART_RX_FUNC;
    config->uart_baudrate = APP_AUDIO_LIOT_DEFAULT_UART_BAUDRATE;
    config->default_volume = APP_AUDIO_LIOT_DEFAULT_VOLUME;
    config->vad_timeout_s = APP_AUDIO_LIOT_DEFAULT_VAD_TIMEOUT_S;
    return APP_OK;
}

static void audio_liot_update_caps(const app_audio_liot_config_t *config)
{
    memset(&s_audio_liot_port.caps, 0, sizeof(s_audio_liot_port.caps));
    if (config == NULL || config->backend == APP_AUDIO_LIOT_BACKEND_NONE) {
        return;
    }

    s_audio_liot_port.caps.has_record = config->enable_record;
    s_audio_liot_port.caps.has_playback = config->enable_playback;
    s_audio_liot_port.caps.has_opus_encode = config->enable_opus_encode;
    s_audio_liot_port.caps.has_opus_decode = config->enable_opus_decode;
    s_audio_liot_port.caps.has_vad = config->enable_vad;
    if (config->backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        s_audio_liot_port.caps.has_opus_encode = false;
        s_audio_liot_port.caps.has_opus_decode = config->enable_playback;
    }
    s_audio_liot_port.caps.input_format = config->record_format;
    s_audio_liot_port.caps.output_format = config->playback_format;
    s_audio_liot_port.caps.frame_samples = config->frame_samples;
    s_audio_liot_port.caps.max_encoded_bytes = config->max_encoded_bytes;
}

static int audio_liot_require_ready(void)
{
    if (!s_audio_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    if (!s_audio_liot.initialized) {
        return APP_ERR_NOT_READY;
    }
    return APP_OK;
}

static int audio_liot_backend_not_supported(void)
{
    return APP_ERR_NOT_SUPPORTED;
}

#if APP_AUDIO_LIOT_ENABLE_GX8006
static int audio_liot_gx8006_set_pin_func(int pin, int func)
{
    if (pin < 0 || func < 0) {
        return APP_OK;
    }
    return (Liot_SetPinFunc((int)pin, (liot_pinfunc_e)func) == L_GPIO_ERR_SUCCESS) ?
           APP_OK : APP_ERR_FAIL;
}

static int audio_liot_gx8006_power_on(void)
{
    int ret;

    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.power_pin,
                                         s_audio_liot.config.power_pin_func);
    if (ret != APP_OK) {
        return ret;
    }
    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.reset_pin,
                                         s_audio_liot.config.reset_pin_func);
    if (ret != APP_OK) {
        return ret;
    }
    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.boot_pin,
                                         s_audio_liot.config.boot_pin_func);
    if (ret != APP_OK) {
        return ret;
    }
    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.pa_pin,
                                         s_audio_liot.config.pa_pin_func);
    if (ret != APP_OK) {
        return ret;
    }
    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.uart_tx_pin,
                                         s_audio_liot.config.uart_tx_func);
    if (ret != APP_OK) {
        return ret;
    }
    ret = audio_liot_gx8006_set_pin_func(s_audio_liot.config.uart_rx_pin,
                                         s_audio_liot.config.uart_rx_func);
    if (ret != APP_OK) {
        return ret;
    }

    if (s_audio_liot.config.power_gpio >= 0) {
        if (Liot_GpioInit((liot_gpio_e)s_audio_liot.config.power_gpio,
                          L_IO_OUTPUT,
                          L_IO_LOW,
                          NULL) != L_GPIO_ERR_SUCCESS) {
            return APP_ERR_FAIL;
        }
        app_os_task_delay_ms(100U);
        if (Liot_GpioSetLevel((liot_gpio_e)s_audio_liot.config.power_gpio,
                              L_IO_HIGH) != L_GPIO_ERR_SUCCESS) {
            return APP_ERR_FAIL;
        }
        if (s_audio_liot.config.power_on_delay_ms != 0U) {
            app_os_task_delay_ms(s_audio_liot.config.power_on_delay_ms);
        }
    }
    return APP_OK;
}

static void audio_liot_gx8006_event_cb(gx8006_evt_e evt, const uint8_t *data, uint32_t len)
{
    switch (evt) {
    case GX_EVT_MIC_VAD_START:
        s_audio_liot.recording = true;
        audio_liot_emit_event(APP_AUDIO_EVENT_VAD_START, APP_OK, NULL);
        break;
    case GX_EVT_MIC_VAD_DATA:
        if (s_audio_liot.media_ready && data != NULL && len != 0U) {
            (void)app_media_emit_audio_frame(data, len, 0U);
        }
        break;
    case GX_EVT_MIC_VAD_END:
        s_audio_liot.recording = false;
        audio_liot_emit_event(APP_AUDIO_EVENT_VAD_END, APP_OK, NULL);
        break;
    case GX_EVT_AWAKEN:
        audio_liot_emit_event(APP_AUDIO_EVENT_RECORD_STARTED, APP_OK, "gx8006_awaken");
        break;
    case GX_EVT_AWAKEN_TIMEOUT:
        audio_liot_emit_event(APP_AUDIO_EVENT_RECORD_STOPPED, APP_OK, "gx8006_awaken_timeout");
        break;
    default:
        break;
    }
}

static int audio_liot_gx8006_init_backend(void)
{
    gx8006_config_t cfg;
    int ret;

    if (s_audio_liot.config.uart_port < 0) {
        return APP_ERR_INVALID_ARG;
    }

    ret = audio_liot_gx8006_power_on();
    if (ret != APP_OK) {
        return ret;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.rst_gpio = s_audio_liot.config.reset_gpio;
    cfg.boot_gpio = s_audio_liot.config.boot_gpio;
    cfg.pa_mode_gpio = s_audio_liot.config.pa_gpio;
    cfg.uart_port = s_audio_liot.config.uart_port;
    cfg.uart_baudrate = s_audio_liot.config.uart_baudrate;
    cfg.default_chat_mode = GX_Q_AND_A_CHAT_MODE;
    cfg.default_volume = s_audio_liot.config.default_volume;
    cfg.vad_timeout_time = s_audio_liot.config.vad_timeout_s;
    cfg.evt_cb = audio_liot_gx8006_event_cb;

    gx8006_init(&cfg);
    gx8006_set_vad_awaken_enable(1U);
    return APP_OK;
}

static int audio_liot_gx8006_deinit_backend(void)
{
    gx8006_deinit();
    return APP_OK;
}

static int audio_liot_gx8006_start_record(void)
{
    gx8006_mic_open();
    s_audio_liot.recording = true;
    audio_liot_emit_event(APP_AUDIO_EVENT_RECORD_STARTED, APP_OK, NULL);
    return APP_OK;
}

static int audio_liot_gx8006_stop_record(void)
{
    gx8006_mic_close();
    s_audio_liot.recording = false;
    audio_liot_emit_event(APP_AUDIO_EVENT_RECORD_STOPPED, APP_OK, NULL);
    return APP_OK;
}

static int audio_liot_gx8006_play_opus(const uint8_t *data, uint32_t len)
{
    int ret;

    if (len > APP_AUDIO_LIOT_GX8006_MAX_OPUS_BYTES) {
        return APP_ERR_INVALID_ARG;
    }
    if (!s_audio_liot.playing) {
        ret = gx8006_spk_stream_start();
        if (ret != 0) {
            return APP_ERR_FAIL;
        }
        s_audio_liot.playing = true;
    }

    ret = gx8006_spk_stream_write(data, len);
    return (ret == 0) ? APP_OK : APP_ERR_FAIL;
}

static int audio_liot_gx8006_stop_playback(void)
{
    int ret;

    if (!s_audio_liot.playing) {
        return APP_OK;
    }
    ret = gx8006_spk_stream_stop();
    s_audio_liot.playing = false;
    audio_liot_emit_event(APP_AUDIO_EVENT_PLAYBACK_DONE, (ret == 0) ? APP_OK : APP_ERR_FAIL, NULL);
    return (ret == 0) ? APP_OK : APP_ERR_FAIL;
}
#endif

static int audio_liot_init(const app_audio_caps_t *caps)
{
    (void)caps;
    if (!s_audio_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    if (!audio_liot_backend_available(s_audio_liot.config.backend)) {
        return APP_ERR_NOT_SUPPORTED;
    }

    s_audio_liot.media_ready = true;
#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        int ret = audio_liot_gx8006_init_backend();

        if (ret != APP_OK) {
            s_audio_liot.media_ready = false;
            return ret;
        }
    }
#endif

    s_audio_liot.initialized = true;
    app_log("audio liot initialized: backend=%d record=%d playback=%d opus_dec=%d",
            (int)s_audio_liot.config.backend,
            s_audio_liot_port.caps.has_record ? 1 : 0,
            s_audio_liot_port.caps.has_playback ? 1 : 0,
            s_audio_liot_port.caps.has_opus_decode ? 1 : 0);
    return APP_OK;
}

static int audio_liot_deinit(void)
{
    int ret = audio_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }

#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        ret = audio_liot_gx8006_stop_playback();
        if (ret != APP_OK) {
            return ret;
        }
        ret = audio_liot_gx8006_deinit_backend();
        if (ret != APP_OK) {
            return ret;
        }
    }
#endif

    s_audio_liot.running = false;
    s_audio_liot.recording = false;
    s_audio_liot.playing = false;
    s_audio_liot.media_ready = false;
    s_audio_liot.initialized = false;
    return APP_OK;
}

static int audio_liot_start(void)
{
    int ret = audio_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    s_audio_liot.running = true;
    return APP_OK;
}

static int audio_liot_stop(void)
{
    int ret = audio_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        ret = audio_liot_gx8006_stop_playback();
        if (ret != APP_OK) {
            return ret;
        }
        if (s_audio_liot.recording) {
            ret = audio_liot_gx8006_stop_record();
            if (ret != APP_OK) {
                return ret;
            }
        }
    }
#endif
    s_audio_liot.running = false;
    s_audio_liot.recording = false;
    s_audio_liot.playing = false;
    return APP_OK;
}

static int audio_liot_start_record(void)
{
    int ret = audio_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_record) {
        return APP_ERR_NOT_SUPPORTED;
    }
#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        return audio_liot_gx8006_start_record();
    }
#endif
    return audio_liot_backend_not_supported();
}

static int audio_liot_stop_record(void)
{
    int ret = audio_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_record) {
        return APP_ERR_NOT_SUPPORTED;
    }
#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        return audio_liot_gx8006_stop_record();
    }
#endif
    return audio_liot_backend_not_supported();
}

static int audio_liot_read_pcm(int16_t *pcm, uint32_t sample_capacity, uint32_t *sample_count)
{
    int ret;

    if (pcm == NULL || sample_capacity == 0U || sample_count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *sample_count = 0U;
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_record) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return audio_liot_backend_not_supported();
}

static int audio_liot_play_pcm(const int16_t *pcm, uint32_t samples)
{
    int ret;

    if (pcm == NULL || samples == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_playback) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return audio_liot_backend_not_supported();
}

static int audio_liot_play_opus(const uint8_t *data, uint32_t len)
{
    int ret;

    if (data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_opus_decode) {
        return APP_ERR_NOT_SUPPORTED;
    }
#if APP_AUDIO_LIOT_ENABLE_GX8006
    if (s_audio_liot.config.backend == APP_AUDIO_LIOT_BACKEND_GX8006) {
        return audio_liot_gx8006_play_opus(data, len);
    }
#endif
    return audio_liot_backend_not_supported();
}

static int audio_liot_encode_opus(const int16_t *pcm,
                                  uint32_t samples,
                                  uint8_t *out,
                                  uint32_t out_size,
                                  uint32_t *out_len)
{
    int ret;

    if (pcm == NULL || samples == 0U || out == NULL || out_size == 0U || out_len == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *out_len = 0U;
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_opus_encode) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return audio_liot_backend_not_supported();
}

static int audio_liot_decode_opus(const uint8_t *data,
                                  uint32_t len,
                                  int16_t *pcm,
                                  uint32_t sample_capacity,
                                  uint32_t *sample_count)
{
    int ret;

    if (data == NULL || len == 0U || pcm == NULL || sample_capacity == 0U || sample_count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *sample_count = 0U;
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    if (!s_audio_liot_port.caps.has_opus_decode) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return audio_liot_backend_not_supported();
}

static int audio_liot_play_prompt(const char *name)
{
    int ret;

    if (name == NULL || name[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    ret = audio_liot_require_ready();
    if (ret != APP_OK) {
        return ret;
    }
    return audio_liot_backend_not_supported();
}

static int audio_liot_set_event_cb(app_audio_event_cb_t cb, void *user)
{
    s_audio_liot.event_cb = cb;
    s_audio_liot.event_user = user;
    return APP_OK;
}

int app_audio_liot_setup(const app_audio_liot_config_t *config)
{
    app_audio_liot_config_t effective_config;

    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!audio_liot_backend_configurable(config->backend)) {
        return APP_ERR_NOT_SUPPORTED;
    }

    effective_config = *config;
    audio_liot_apply_default_config(&effective_config);

    memset(&s_audio_liot, 0, sizeof(s_audio_liot));
    s_audio_liot.config = effective_config;
    s_audio_liot.configured = true;
    audio_liot_update_caps(&effective_config);
    return APP_OK;
}

int app_audio_liot_register(void)
{
    if (!s_audio_liot.configured) {
        return APP_ERR_NOT_READY;
    }
    return app_media_register_port(&s_audio_liot_port);
}

const app_audio_port_t *app_audio_liot_port(void)
{
    return &s_audio_liot_port;
}
