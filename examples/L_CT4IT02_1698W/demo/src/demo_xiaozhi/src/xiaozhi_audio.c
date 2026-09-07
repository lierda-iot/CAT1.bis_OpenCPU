#include "xiaozhi_core.h"

#include <stdint.h>
#include <string.h>

#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "opus.h"

#define XZ_AUDIO_RATE              16000
#define XZ_AUDIO_FRAME_SAMPLES     960
#define XZ_AUDIO_FRAME_BYTES       (XZ_AUDIO_FRAME_SAMPLES * 2)
#define XZ_AUDIO_OPUS_MAX          512
#define XZ_AUDIO_TTS_PCM_CAPACITY  (512 * 1024)

static OpusEncoder *g_audio_encoder;
static OpusDecoder *g_audio_decoder;
static int16_t g_audio_record_pcm[XZ_AUDIO_FRAME_SAMPLES];
static int16_t g_audio_decode_pcm[XZ_AUDIO_FRAME_SAMPLES];
static uint8_t g_audio_tts_pcm[XZ_AUDIO_TTS_PCM_CAPACITY] __attribute__((aligned(16)));
static int g_audio_tts_bytes;

static void xz_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    if (event == L_AUD_EVT_ERROR)
        liot_trace("[xiaozhi] audio event error=%d", (int)event);
}

int xiaozhi_audio_init(void)
{
    Liot_AudHwConfig_t config;
    Liot_AudErr_e audio_ret;
    int opus_error;
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_rtos_task_sleep_ms(2000);
    memset(&config, 0, sizeof(config));
    config.i2cNum = 0;
    config.i2sNum = 0;
    config.paGpioNum = -1;
    config.codecType = L_AUD_ES8375;
    config.channel = L_AUD_MONO_RIGHT;
    config.role = L_AUD_ROLE_SLAVE;
    config.mode = L_AUD_MODE_I2S;
    config.frameSize = L_AUD_FRAMESIZE_16_16;
    config.samples = L_AUD_16K_SAMPLES;
    config.callback = xz_audio_callback;
    config.cbContext = NULL;
    audio_ret = Liot_AudioInit(&config);
    liot_trace("[xiaozhi] ES8375 AudioInit ret=%d", (int)audio_ret);
    if (audio_ret != L_AUD_ERR_SUCCESS) return -1;
    Liot_AudioSetVolume(60);
    Liot_AudioSetCodecVolume(60);
    Liot_AudioSetMicVolume(8, 200);
    g_audio_encoder = opus_encoder_create(XZ_AUDIO_RATE, 1, OPUS_APPLICATION_VOIP,
                                          &opus_error);
    if (g_audio_encoder == NULL || opus_error != OPUS_OK) return -2;
    opus_encoder_ctl(g_audio_encoder, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(g_audio_encoder, OPUS_SET_COMPLEXITY(3));
    opus_encoder_ctl(g_audio_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    g_audio_decoder = opus_decoder_create(XZ_AUDIO_RATE, 1, &opus_error);
    if (g_audio_decoder == NULL || opus_error != OPUS_OK) return -3;
    liot_trace("[xiaozhi] Opus ready input=16000 output=16000 frame_ms=60");
    return 0;
}

int xiaozhi_audio_capture_opus(uint8_t *opus_data, int capacity)
{
    int encoded;
    if (g_audio_encoder == NULL || opus_data == NULL || capacity < XZ_AUDIO_OPUS_MAX)
        return -1;
    if (Liot_AudioRecord((uint8_t *)g_audio_record_pcm, XZ_AUDIO_FRAME_BYTES) !=
        L_AUD_ERR_SUCCESS) return -2;
    encoded = opus_encode(g_audio_encoder, g_audio_record_pcm,
                          XZ_AUDIO_FRAME_SAMPLES, opus_data, capacity);
    return encoded > 0 ? encoded : -3;
}

void xiaozhi_audio_tts_reset(void)
{
    g_audio_tts_bytes = 0;
    if (g_audio_decoder != NULL) opus_decoder_ctl(g_audio_decoder, OPUS_RESET_STATE);
}

int xiaozhi_audio_decode_append(const uint8_t *opus_data, int length)
{
    int samples;
    int bytes;
    if (g_audio_decoder == NULL || opus_data == NULL || length <= 0) return -1;
    samples = opus_decode(g_audio_decoder, opus_data, length, g_audio_decode_pcm,
                          XZ_AUDIO_FRAME_SAMPLES, 0);
    if (samples <= 0) {
        liot_trace("[xiaozhi] Opus decode failed ret=%d bytes=%d", samples, length);
        return -2;
    }
    bytes = samples * 2;
    if (g_audio_tts_bytes + bytes > XZ_AUDIO_TTS_PCM_CAPACITY) return -3;
    memcpy(g_audio_tts_pcm + g_audio_tts_bytes, g_audio_decode_pcm, bytes);
    g_audio_tts_bytes += bytes;
    return samples;
}

int xiaozhi_audio_tts_play(void)
{
    Liot_AudErr_e ret;
    int bytes = g_audio_tts_bytes;
    if (bytes <= 0) return 0;
    liot_trace("[xiaozhi] TTS PCM play bytes=%d duration_ms=%d", bytes,
               bytes * 1000 / (XZ_AUDIO_RATE * 2));
    ret = Liot_AudioPlay(g_audio_tts_pcm, bytes);
    if (ret == L_AUD_ERR_SUCCESS) Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
    g_audio_tts_bytes = 0;
    return ret == L_AUD_ERR_SUCCESS ? 0 : -1;
}
