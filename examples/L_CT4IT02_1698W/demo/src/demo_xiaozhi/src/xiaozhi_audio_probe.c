#include "xiaozhi_core.h"

#include <string.h>

#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

static void xz_audio_probe_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    liot_trace("[xiaozhi] audio probe event=%d", (int)event);
}

static Liot_AudHwConfig_t g_audio_config;
static uint8_t g_audio_warmup_silence[3200] __attribute__((aligned(16)));

int xiaozhi_audio_probe_init(void)
{
    Liot_AudErr_e ret;

    liot_trace("[xiaozhi] audio probe power begin");
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_rtos_task_sleep_ms(2000);

    memset(&g_audio_config, 0, sizeof(g_audio_config));
    g_audio_config.i2cNum = 0;
    g_audio_config.i2sNum = 0;
    g_audio_config.paGpioNum = -1;
    g_audio_config.codecType = L_AUD_ES8375;
    g_audio_config.channel = L_AUD_MONO_RIGHT;
    g_audio_config.role = L_AUD_ROLE_SLAVE;
    g_audio_config.mode = L_AUD_MODE_I2S;
    g_audio_config.frameSize = L_AUD_FRAMESIZE_16_16;
    g_audio_config.samples = L_AUD_16K_SAMPLES;
    g_audio_config.callback = xz_audio_probe_callback;
    g_audio_config.cbContext = NULL;
    liot_trace("[xiaozhi] audio probe AudioInit begin");
    ret = Liot_AudioInit(&g_audio_config);
    liot_trace("[xiaozhi] audio probe AudioInit ret=%d", (int)ret);
    if (ret != L_AUD_ERR_SUCCESS) return -1;
    liot_trace("[xiaozhi] audio probe volume ret=%d codec=%d mic=%d",
               (int)Liot_AudioSetVolume(60),
               (int)Liot_AudioSetCodecVolume(60),
               (int)Liot_AudioSetMicVolume(8, 200));
    ret = Liot_AudioPlay(g_audio_warmup_silence,
                         sizeof(g_audio_warmup_silence));
    if (ret == L_AUD_ERR_SUCCESS)
        ret = Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
    liot_trace("[xiaozhi] audio output warmup ret=%d", (int)ret);
    return 0;
}
