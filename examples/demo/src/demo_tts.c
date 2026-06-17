#include "liot_type.h"
#include "liot_audio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_tts.h"
#include <string.h>
#include "mem_map.h"
#include "liot_flash.h"

#ifndef CONFIG_TTS_ENABLE
#error "please set BUILD_COMP_TTS_EN = y in LSDK/rules/Makefile.defs"
#endif

const char* playbuf = "利尔达公司欢迎您";

static int ttsUserCallback(void *context, int msg, int ds, int param2, int dSize, const void *dBuffer)
{
    Liot_AudioPlay((uint8_t *)dBuffer, dSize);
    return 0;
}

/* Read resource callback function */
static bool user_read_res_cb(void *pParameter, void *pBuffer, uint32_t iPos, uint32_t nSize)
{
    liot_flash_read((uint8_t *)pBuffer, (uint32_t)((uint8_t *)pParameter + iPos), nSize);
    return true;
}

void liot_tts_demo_thread(void *argv)
{
    liot_rtos_task_sleep_ms(2000);

    Liot_AonPowerCtl(TRUE);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V );

    Liot_AudHwConfig_t cfg ={
        .i2cNum = 1,
        .i2sNum = 0,
        .paGpioNum = 8,
        .codecType = L_AUD_ES8311,
        .channel = L_AUD_MONO_RIGHT,
        .role = L_AUD_ROLE_SLAVE,
        .mode = L_AUD_MODE_I2S,
        .frameSize = L_AUD_FRAMESIZE_16_16,
        .samples = L_AUD_08K_SAMPLES,
	};

    liot_trace("Liot_AudioInit");
    Liot_AudioInit(&cfg);
    Liot_AudioSetVolume(60);
    liot_tts_set_resource((void *)PKGFLXTTS_RES_ADDR, user_read_res_cb);
    liot_tts_engine_init(ttsUserCallback);

    while(1)
    {
        liot_tts_start(playbuf, strlen(playbuf));
        liot_rtos_task_sleep_ms(5000);
    }

    liot_tts_end();
    liot_tts_exit();
    Liot_AudioDeInit();
    liot_rtos_task_delete(0); // kill itsel
}

