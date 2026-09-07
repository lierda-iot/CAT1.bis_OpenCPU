/**
 * @file user_main.c
 * @brief Application entry point for L_CT4IT02_1698W
 */

#include "liot_log.h"
#include "liot_os.h"
#include "liot_power.h"
#include "app_boot_diag.h"
#include "app_menu.h"
#include "lcd.h"

void user_main(void)
{
    UINT8 powerup_reason = LIOT_PWRUP_UNKNOWN;
    liot_power_errcode_e power_ret = liot_get_powerup_reason(&powerup_reason);

    liot_trace("L_CT4IT02_1698W app start");
    liot_trace("[boot] powerup reason=%u ret=%d (wdg=%u panic=%u)",
               (unsigned int)powerup_reason, (int)power_ret,
               (unsigned int)LIOT_PWRUP_WDG, (unsigned int)LIOT_PWRUP_PANIC);
    app_boot_diag_log_reason("boot", powerup_reason, power_ret);
    app_boot_diag_log_stage("user_main_enter");

    app_menu_init();
    app_boot_diag_log_stage("app_menu_ready");
    app_boot_diag_log_stage("lvgl_init_wait");
    lvgl_init();
    app_boot_diag_log_stage("lvgl_ready");
    liot_rtos_task_delete(NULL);
    while (1)
    {
        liot_rtos_task_sleep_s(1);
    }
}
