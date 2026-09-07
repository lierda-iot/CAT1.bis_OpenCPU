#include <stdint.h>

#include "liot_audio2.h"
#include "liot_log.h"
#include "liot_os.h"

extern const uint8_t g_demo_mp3_pcm_test_start[];
extern const uint8_t g_demo_mp3_pcm_test_end[];

__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".global g_demo_mp3_pcm_test_start\n"
    ".global g_demo_mp3_pcm_test_end\n"
    "g_demo_mp3_pcm_test_start:\n"
    ".incbin \"../examples/L_CT4IT02_1698W/demo/src/demo_mp3/sine_1khz_16k_mono_s16le.pcm\"\n"
    "g_demo_mp3_pcm_test_end:\n"
    ".balign 16\n"
);

Liot_AudErr_e demo_mp3_play_pcm_test(void)
{
    int length = (int)(g_demo_mp3_pcm_test_end - g_demo_mp3_pcm_test_start);
    Liot_AudErr_e ret;

    liot_trace("[tone-1khz] loop start 16k mono s16le -12dBFS len=%d", length);
    for (;;) {
        ret = Liot_AudioPlay((uint8_t *)g_demo_mp3_pcm_test_start, length);
        liot_trace("[tone-1khz] play ret=%d", (int)ret);
        if (ret != L_AUD_ERR_SUCCESS) return ret;
        Liot_AudioWaitPlayFinish(LIOT_WAIT_FOREVER);
    }
}
