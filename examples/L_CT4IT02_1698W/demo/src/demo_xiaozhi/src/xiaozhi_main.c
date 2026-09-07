#include "xiaozhi_core.h"

#include <string.h>

#include "liot_log.h"
#include "liot_os.h"

static void xiaozhi_main_thread(void *argument)
{
    xiaozhi_config_t config;
    int ret;
    (void)argument;
    memset(&config, 0, sizeof(config));

    liot_trace("[xiaozhi] demo start");
    for (;;) {
        ret = xiaozhi_network_start();
        if (ret == 0) break;
        liot_trace("[xiaozhi] network startup failed ret=%d; retry in 10s", ret);
        liot_rtos_task_sleep_s(10);
    }
    for (;;) {
        ret = xiaozhi_identity_init(&config);
        if (ret == 0) break;
        liot_trace("[xiaozhi] identity startup failed ret=%d; retry in 3s", ret);
        liot_rtos_task_sleep_s(3);
    }

    ret = xiaozhi_audio_probe_init();
    liot_trace("[xiaozhi] audio probe result=%d", ret);
    ret = xiaozhi_opus_probe_init();
    liot_trace("[xiaozhi] opus probe result=%d", ret);
    xiaozhi_ui_init();

    for (;;) {
        ret = xiaozhi_ota_check(&config);
        liot_trace("[xiaozhi] OTA ret=%d activation=%d code=%s ws_url_len=%u token_len=%u",
                   ret, config.activation_required, config.activation_code,
                   (unsigned)strlen(config.websocket_url),
                   (unsigned)strlen(config.websocket_token));
        if (ret != 0) {
            liot_rtos_task_sleep_s(10);
            continue;
        }
        if (!config.activation_required) {
            liot_trace("[xiaozhi] activated; websocket configuration ready");
            break;
        }
        liot_trace("[xiaozhi] activation code=%s message=%s",
                   config.activation_code, config.activation_message);
        do {
            ret = xiaozhi_ota_activate(&config);
            liot_trace("[xiaozhi] activate result=%d", ret);
            if (ret == 1) liot_rtos_task_sleep_s(3);
            else if (ret < 0) liot_rtos_task_sleep_s(10);
        } while (ret != 0);
    }

    for (;;) {
        ret = xiaozhi_ws_run(&config);
        liot_trace("[xiaozhi] websocket exited ret=%d; reconnect in 5s", ret);
        liot_rtos_task_sleep_s(5);
    }
}

void liot_sound_es8375_demo_thread(void *argument)
{
    liot_task_t task = NULL;
    int ret;
    (void)argument;

    ret = liot_rtos_task_create(&task, 32 * 1024, LIOT_APP_TASK_PRIORITY,
                                "xiaozhi_main", xiaozhi_main_thread, NULL);
    liot_trace("[xiaozhi] main task create ret=%d handle=%p stack=32768",
               ret, task);
    liot_rtos_task_delete(NULL);
}
