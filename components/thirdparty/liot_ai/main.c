#include "lierda_app_main.h"
#include "appFramework.h"
#include "liot_os.h"
#include "aiLog.h"

void liot_ai_task(void)
{
    aiLogInit(0);
    liot_rtos_task_sleep_s(3);
    LOG_DEBUG("starting AI task");

    appFrameworkSetup();

    LOG_DEBUG("AI task running");
    while (true) {
        liot_rtos_task_sleep_s(1000);
    }
    LOG_DEBUG("AI task exiting");
}
