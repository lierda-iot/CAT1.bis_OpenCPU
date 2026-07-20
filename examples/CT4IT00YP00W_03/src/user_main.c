#include "liot_os.h"
#include "liot_log.h"
#include "app_nv.h"
#include "app_framework.h"
#include "hw_led.h"
#include "hw_battery.h"
#include "hw_network.h"
#include "hw_powerkey.h"
#include "hw_audio.h"
#include "hw_ldo33.h"

extern void app_atcmd_init(void *argv);
extern void gx8006_into_burn(void);

#define APP_TASK_STACK_SIZE  (8192U)
#define APP_TASK_PRIORITY    (5U)

static liot_task_t g_appTaskRef = NULL;

static void app_main_task(void *arg)
{
    (void)arg;

    liot_trace("[APP] nv load...");
    app_nv_load();
    app_atcmd_init(NULL);

    boot_mode_e boot_mode = app_nv_get_boot_mode();
    if(boot_mode == APP_BOOT_MODE_NORMAL)
    {
        lod33_power_init();
        liot_trace("[APP] framework setup...");
        if (!appFrameworkSetup()) {
            liot_trace("[APP] framework setup FAILED");
        }
        /* 延后 2 秒再注册电源键回调,避开开机长按残留事件导致的误关机 */
        liot_rtos_task_sleep_ms(2000);
        powerkeyInit();
    }
    else if(boot_mode == APP_BOOT_MODE_BURN)
    {
        lod33_power_init();
        liot_trace("[APP] gx8006 burn mode");
        app_nv_set_boot_mode(APP_BOOT_MODE_NORMAL);
        app_nv_save();
        gx8006_into_burn();
    }

    liot_rtos_task_delete(NULL); // kill itself
}

void user_main(void)
{
    liot_rtos_task_create(&g_appTaskRef, APP_TASK_STACK_SIZE,
                          APP_TASK_PRIORITY, "app_main", app_main_task, NULL);
}
