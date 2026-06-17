#include "liot_os.h"
#include "liot_log.h"
#include "app_framework.h"
#include "hw_led.h"
#include "hw_battery.h"
#include "hw_network.h"
#include "hw_powerkey.h"
#include "hw_audio.h"
#include "hw_ldo33.h"

#define APP_TASK_STACK_SIZE  (8192U)
#define APP_TASK_PRIORITY    (5U)

static liot_task_t g_appTaskRef = NULL;

static void app_main_task(void *arg)
{
    (void)arg;

    liot_trace("[APP] power init...");
    lod33_power_init();
    liot_rtos_task_sleep_ms(2000);

    liot_trace("[APP] framework setup...");
    if (!appFrameworkSetup()) {
        liot_trace("[APP] framework setup FAILED");
    }



    liot_rtos_task_delete(NULL); // kill itself
}

void user_main(void)
{
    liot_rtos_task_create(&g_appTaskRef, APP_TASK_STACK_SIZE,
                          APP_TASK_PRIORITY, "app_main", app_main_task, NULL);
}
