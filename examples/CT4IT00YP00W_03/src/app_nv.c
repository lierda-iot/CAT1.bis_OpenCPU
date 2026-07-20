#include <string.h>
#include "app_nv.h"
#include "liot_fs_api.h"
#include "liot_log.h"

#define APP_NV_FILE "app_nv.dat"

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

boot_mode_e app_nv_get_boot_mode(void)
{
    return g_nv.boot_mode;
}

void app_nv_set_boot_mode(boot_mode_e mode)
{
    g_nv.boot_mode = mode;
}

void app_nv_save(void)
{
    LFILE fd = liot_fopen(APP_NV_FILE, "wb+");
    if (fd < LIOT_FS_OK) {
        liot_trace("[APP_NV] save: open file fail, fd=%d", fd);
        return;
    }
    int ret = liot_fwrite(&g_nv, 1, sizeof(g_nv), fd);
    if (ret != sizeof(g_nv)) {
        liot_trace("[APP_NV] save: write fail, ret=%d", ret);
    } else {
        liot_trace("[APP_NV] save: write ok, size=%d", ret);
    }
    liot_fsync(fd);
    liot_fclose(fd);
}

void app_nv_load(void)
{
    int exist = liot_file_exist(APP_NV_FILE);
    liot_trace("[APP_NV] load: file_exist=%d", exist);
    if (exist != 0) {
        liot_trace("[APP_NV] load: file not exist, save default");
        app_nv_save();
        return;
    }
    LFILE fd = liot_fopen(APP_NV_FILE, "r");
    if (fd < LIOT_FS_OK) {
        liot_trace("[APP_NV] load: open file fail, fd=%d", fd);
        app_nv_save();
        return;
    }
    app_nv_data_t tmp;
    int ret = liot_fread(&tmp, 1, sizeof(tmp), fd);
    liot_fclose(fd);
    if (ret == sizeof(tmp) && tmp.magic == APP_NV_MAGIC) {
        memcpy(&g_nv, &tmp, sizeof(g_nv));
        liot_trace("[APP_NV] load: success");
    } else {
        liot_trace("[APP_NV] load: invalid data(ret=%d, magic=0x%X), use default", ret, tmp.magic);
        app_nv_save();
    }
}
