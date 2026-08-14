#include "app_main.h"

#include "app_osal.h"

#define WATCH_AI_MAIN_RETRY_DELAY_MS 1000U

void user_main(void)
{
    int ret;

    app_log("==== WATCH_AI demo ====");
    app_log("watch boot start, initializing application");
    ret = watch_ai_app_init();
    if (ret != APP_OK) {
        app_log("watch_ai_app_init failed: %d", ret);
        while (1) {
            app_os_task_delay_ms(WATCH_AI_MAIN_RETRY_DELAY_MS);
        }
    }

    app_log("watch application ready, entering event loop");
    while (1) {
        ret = watch_ai_app_run_once(APP_OS_WAIT_FOREVER);
        if (ret != APP_OK) {
            app_log("watch_ai_app_run_once failed: %d", ret);
            app_os_task_delay_ms(WATCH_AI_MAIN_RETRY_DELAY_MS);
        }
    }
}
