#include "xiaozhi_core.h"

#include <string.h>

#include "liot_log.h"
#include "liot_os.h"

static void xiaozhi_ws_idle(void)
{
    liot_trace("[xiaozhi] ws idle; connect on next press\n");
}

static void xiaozhi_main_thread(void *argument)
{
    xiaozhi_config_t config;
    int ret;
    int attempt;
    (void)argument;
    memset(&config, 0, sizeof(config));

    liot_trace("[xiaozhi] demo start\n");
    for (;;) {
        ret = xiaozhi_network_start();
        if (ret == 0) break;
        liot_trace("[xiaozhi] network startup failed ret=%d; retry in 10s\n", ret);
        liot_rtos_task_sleep_s(10);
    }
    for (;;) {
        ret = xiaozhi_identity_init(&config);
        if (ret == 0) break;
        liot_trace("[xiaozhi] identity startup failed ret=%d; retry in 3s\n", ret);
        liot_rtos_task_sleep_s(3);
    }

    ret = xiaozhi_audio_probe_init();
    liot_trace("[xiaozhi] audio probe result=%d\n", ret);
    ret = xiaozhi_opus_probe_init();
    liot_trace("[xiaozhi] opus probe result=%d\n", ret);
    xiaozhi_ui_init();

    for (;;) {
        ret = xiaozhi_ota_check(&config);
        liot_trace("[xiaozhi] OTA ret=%d activation=%d code=%s ws_url_len=%u token_len=%u\n",
                   ret, config.activation_required, config.activation_code,
                   (unsigned)strlen(config.websocket_url),
                   (unsigned)strlen(config.websocket_token));
        if (ret != 0) {
            liot_rtos_task_sleep_s(10);
            continue;
        }
        if (!config.activation_required) {
            liot_trace("[xiaozhi] activated; websocket configuration ready\n");
            break;
        }
        liot_trace("[xiaozhi] activation code=%s message=%s\n",
                   config.activation_code, config.activation_message);
        do {
            ret = xiaozhi_ota_activate(&config);
            liot_trace("[xiaozhi] activate result=%d\n", ret);
            if (ret == 1) liot_rtos_task_sleep_s(3);
            else if (ret < 0) liot_rtos_task_sleep_s(10);
        } while (ret != 0);
    }

    for (;;) {
        ret = xiaozhi_ws_run(&config);
        liot_trace("[xiaozhi] websocket exited ret=%d\n", ret);
        if (ret != -21 && ret != 0) {
            liot_trace("[xiaozhi] websocket setup failed; reconnect in 5s\n");
            liot_rtos_task_sleep_s(5);
            continue;
        }
        /* Server closed the connection (idle timeout). No automatic
         * reconnect: wait for the next press to re-establish the WS. */
        xiaozhi_ws_idle();
        for (;;) {
            /* Use a finite wait as a safety backstop; press requests release
             * the semaphore from the UI/talk path to wake this loop. */
            if (xiaozhi_ws_wait_connect_request(60000) != LIOT_OSI_SUCCESS) {
                liot_trace("[xiaozhi] ws wait wake failed; retry in 1s\n");
                liot_rtos_task_sleep_s(1);
                continue;
            }
            attempt = 0;
            while (attempt < XZ_WS_RECONNECT_MAX) {
                ++attempt;
                liot_trace("[xiaozhi] press reconnect attempt=%d/%d\n",
                           attempt, XZ_WS_RECONNECT_MAX);
                ret = xiaozhi_ws_run(&config);
                liot_trace("[xiaozhi] websocket re-run ret=%d\n", ret);
                if (ret != -21 && ret != 0) {
                    if (attempt < XZ_WS_RECONNECT_MAX)
                        liot_rtos_task_sleep_ms(XZ_WS_RECONNECT_DELAY_MS);
                    continue;
                }
                if (ret == -21)
                    liot_trace("[xiaozhi] press reconnect closed again; wait for next press\n");
                break;
            }
            if (attempt >= XZ_WS_RECONNECT_MAX)
                liot_trace("[xiaozhi] press reconnect exhausted; wait for next press\n");
        }
    }
}

void liot_sound_es8375_demo_thread(void *argument)
{
    liot_task_t task = NULL;
    int ret;
    (void)argument;

    ret = liot_rtos_task_create(&task, 32 * 1024, LIOT_APP_TASK_PRIORITY,
                                "xiaozhi_main", xiaozhi_main_thread, NULL);
    liot_trace("[xiaozhi] main task create ret=%d handle=%p stack=32768\n",
               ret, task);
    liot_rtos_task_delete(NULL);
}
