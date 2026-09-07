#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "liot_at_cmd.h"
#include "liot_audio2.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

#define MP3_ID3_SIZE 44
#define MP3_SAFE_HW_VOLUME_MAX 70

extern const uint8_t g_standalone_mp3_start[];
extern const uint8_t g_standalone_mp3_end[];

static volatile int g_mp3_volume = 100;
static volatile int g_mp3_audio_ready;

static int standalone_mp3_set_volume(int volume)
{
    int hw_volume;
    Liot_AudErr_e sw_ret;
    Liot_AudErr_e codec_ret;

    if (volume < 0 || volume > 100) return -1;
    g_mp3_volume = volume;
    if (!g_mp3_audio_ready) return 0;
    hw_volume = (volume * MP3_SAFE_HW_VOLUME_MAX + 50) / 100;
    sw_ret = Liot_AudioSetVolume(hw_volume);
    codec_ret = Liot_AudioSetCodecVolume(hw_volume);
    liot_trace("[mp3-standalone] user_volume=%d hw_volume=%d sw_ret=%d codec_ret=%d",
               volume, hw_volume, (int)sw_ret, (int)codec_ret);
    return sw_ret == L_AUD_ERR_SUCCESS && codec_ret == L_AUD_ERR_SUCCESS ? 0 : -1;
}

static liot_at_result_enum_type standalone_mp3_volume_atcmd(
    const liot_atCommand_Input *input)
{
    char response[40];

    if (input == NULL) return LIOT_AT_ERROR;
    switch (input->op) {
    case LIOT_AT_CMD_SET: {
        char *end = NULL;
        long volume;

        if (input->args_count != 1 || input->arg[0] == '\0')
            return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
        volume = strtol(input->arg, &end, 10);
        if (end == input->arg || *end != '\0' ||
            standalone_mp3_set_volume((int)volume) != 0)
            return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
        snprintf(response, sizeof(response), "+MP3VOL: %d", g_mp3_volume);
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK, response);
    }
    case LIOT_AT_CMD_READ:
        snprintf(response, sizeof(response), "+MP3VOL: %d", g_mp3_volume);
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK, response);
    case LIOT_AT_CMD_TEST:
        return liot_atcmd_reply(input->atHandle, LIOT_AT_OK,
                                "+MP3VOL: (0-100)");
    default:
        return liot_atcmd_reply(input->atHandle, LIOT_AT_ERROR, NULL);
    }
}

static const liot_atCommand g_mp3_atcmd_table[] = {
    LIOT_ATCMD("+MP3VOL", standalone_mp3_volume_atcmd)
};

static void standalone_mp3_atcmd_init(void)
{
    liot_atcmd_register((liot_atCommandP)g_mp3_atcmd_table,
                        sizeof(g_mp3_atcmd_table) /
                            sizeof(g_mp3_atcmd_table[0]));
    liot_open_atcmd_init();
    liot_trace("[mp3-standalone] AT ready: AT+MP3VOL=<0-100>, AT+MP3VOL?");
}

__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".global g_standalone_mp3_start\n"
    ".global g_standalone_mp3_end\n"
    "g_standalone_mp3_start:\n"
    ".incbin \"F:/cat1.bis_opencpu_internal/examples/L_CT4IT02_1698W/demo/src/demo_mp3/because of you_-12dB.mp3\"\n"
    "g_standalone_mp3_end:\n"
    ".balign 16\n"
);

static void standalone_mp3_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;
    liot_trace("[mp3-standalone] audio event=%d", (int)event);
}

void liot_sound_es8375_demo_thread(void *argument)
{
    Liot_AudHwConfig_t config;
    Liot_AudErr_e ret;
    uint8_t *mp3_data = (uint8_t *)g_standalone_mp3_start + MP3_ID3_SIZE;
    int mp3_length = (int)(g_standalone_mp3_end - g_standalone_mp3_start) - MP3_ID3_SIZE;

    (void)argument;
    liot_trace("[mp3-standalone] start size=%d offset=%d",
               mp3_length, MP3_ID3_SIZE);
    standalone_mp3_atcmd_init();

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
    config.callback = standalone_mp3_callback;
    config.cbContext = NULL;

    ret = Liot_AudioInit(&config);
    liot_trace("[mp3-standalone] AudioInit ret=%d", (int)ret);
    if (ret != L_AUD_ERR_SUCCESS) goto keep_alive;

    g_mp3_audio_ready = 1;

    standalone_mp3_set_volume(g_mp3_volume);

    for (;;) {
        ret = Liot_AudioPlayMp3(mp3_data, mp3_length);
        liot_trace("[mp3-standalone] play ret=%d len=%d", (int)ret,
                   mp3_length);
        if (ret == L_AUD_ERR_SUCCESS) {
            ret = Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
            liot_trace("[mp3-standalone] playback finished wait_ret=%d; loop",
                       (int)ret);
        }
        liot_rtos_task_sleep_ms(ret == L_AUD_ERR_SUCCESS ? 50 : 1000);
    }

keep_alive:
    for (;;) liot_rtos_task_sleep_ms(1000);
}
