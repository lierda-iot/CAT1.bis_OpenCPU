#include <string.h>
#include "app_nv.h"

static app_nv_data_t g_nv = {
        .magic = APP_NV_MAGIC,
        .chat_mode = CHAT_MODE_DEFAULT,
        .audio_volume = AUDIO_VOLUME_DEFAULT,
        .sleep_wait_time_ms = SYSTEM_WAIT_TIME_DEFAULT_MS,
        .sleep_time_ms = SYSTEM_SLEEP_TIME_DEFAULT_MS,
        .coze_token = COZE_DEFAULT_TOKEN,
        .coze_botid = COZE_DEFAULT_BOTID,
        .coze_voiceid = COZE_DEFAULT_VOICEID,
        .led_cfg = {
            .spi_port   = PIN_WS2812B_SPI_PORT,
            .led_num    = 1,
            .brightness = 128,
        },
        .net_cfg = {
            .apn      = "",
            .username = "",
            .password = "",
        },
};

app_nv_data_t *app_nv_get(void)
{
    return &g_nv;
}
