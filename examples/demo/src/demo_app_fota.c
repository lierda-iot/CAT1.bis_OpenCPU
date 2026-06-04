#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_fota.h"
#include "liot_os.h"

#define LIOT_APP_TEST_FILENAME "upgrade.bin"

void liot_fota_demo_thread(void *arg)
{
    liot_rtos_task_sleep_s(10);    
    bool is_reboot = TRUE;
    bool is_check = FALSE;

    while(1)
    {
        if(!is_check)
        {
            liot_fota_errcode_e result = Liot_FotaAppUpgradeCheck(LIOT_APP_TEST_FILENAME, is_reboot);
            if (result == LIOT_FOTA_UPGRADE_SUCCESS) {
                liot_trace("Upgrade check passed successfully.\n");
                is_check = TRUE;
            } else {
                liot_trace("Upgrade check failed with error code: %d\n", result);
            }
        }
        
        liot_rtos_task_sleep_ms(5*1000);
    }
}