#include <stdint.h>

#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

static volatile int g_tone_volume = 100;
static volatile int g_tone_audio_ready;

int demo_tone_set_volume(int volume)
{
    if (volume < 0 || volume > 100) return -1;
    g_tone_volume = volume;
    if (g_tone_audio_ready) {
        Liot_AudioSetVolume(volume);
        Liot_AudioSetCodecVolume(volume);
    }
    return 0;
}

int demo_tone_get_volume(void)
{
    return g_tone_volume;
}

void demo_tone_atcmd_init(void);

extern const uint8_t g_tone_1khz_start[];
extern const uint8_t g_tone_1khz_end[];

__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".global g_tone_1khz_start\n"
    ".global g_tone_1khz_end\n"
    "g_tone_1khz_start:\n"
    ".incbin \"F:/cat1.bis_opencpu_internal/examples/L_CT4IT02_1698W/demo/src/demo_mp3/sine_1khz_44k_mono_s16le.pcm\"\n"
    "g_tone_1khz_end:\n"
    ".balign 16\n"
);

static void tone_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    liot_trace("[tone-1khz] audio event=%d", (int)event);
}

void liot_sound_es8375_demo_thread(void *argument)
{
    Liot_AudHwConfig_t config;
    Liot_AudErr_e ret;
    int tone_length = (int)(g_tone_1khz_end - g_tone_1khz_start);
    uint32_t loop_count = 0;

    (void)argument;
    liot_trace("[tone-1khz] standalone demo start");
    demo_tone_atcmd_init();
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_rtos_task_sleep_ms(2000);

    config.i2cNum = 0;
    config.i2sNum = 0;
    config.paGpioNum = -1;
    config.codecType = L_AUD_ES8375;
    config.channel = L_AUD_MONO_RIGHT;
    config.role = L_AUD_ROLE_SLAVE;
    config.mode = L_AUD_MODE_I2S;
    config.frameSize = L_AUD_FRAMESIZE_16_16;
    config.samples = L_AUD_44K_SAMPLES;
    config.callback = tone_audio_callback;
    config.cbContext = NULL;

    ret = Liot_AudioInit(&config);
    liot_trace("[tone-1khz] AudioInit ret=%d len=%d", (int)ret, tone_length);
    if (ret != L_AUD_ERR_SUCCESS) goto keep_alive;
    g_tone_audio_ready = 1;
    liot_trace("[tone-1khz] volume=%d sw ret=%d codec ret=%d",
               g_tone_volume,
               (int)Liot_AudioSetVolume(g_tone_volume),
               (int)Liot_AudioSetCodecVolume(g_tone_volume));

    for (;;) {
        ret = Liot_AudioPlay((uint8_t *)g_tone_1khz_start, tone_length);
        liot_trace("[tone-1khz] loop=%lu play ret=%d",
                   (unsigned long)loop_count++, (int)ret);
        if (ret == L_AUD_ERR_SUCCESS) {
            Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
        } else {
            liot_rtos_task_sleep_ms(1000);
        }
    }

keep_alive:
    for (;;) liot_rtos_task_sleep_ms(1000);
}
