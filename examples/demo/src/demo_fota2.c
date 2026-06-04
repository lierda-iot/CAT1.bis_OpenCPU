#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liot_dev.h"
#include "liot_fota2.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_log.h"

#define LIOT_FOTA_FILE_URL "FILE:fota_test1.par"
#define LIOT_FOTA_FTP_URL "ftp://ftpuser:123456@121.89.205.240:21/5D6A3E7E/fota_test2.par"
#define LIOT_FOTA_HTTP_URL "http://xiot.oss.senthink.com/upload-http-ftp/5D6A3E7E/fota_test3.par"

static void liot_fota_cb(uint8_t progess)
{
    liot_trace("===fota process:%d===", progess);
}

void liot_fota2_demo_thread(void *arg)
{
    char version_buf[64] = {0};
    int ret = L_FOTA_UPGRADE_SUCCESS;
    Liot_FotaConfig_t fota_config = {
        .url = LIOT_FOTA_HTTP_URL,
        .enable = true,
        .timeout = 30000,
        .callback = liot_fota_cb,
    };
    liot_rtos_task_sleep_s(10);
    liot_dev_get_firmware_version(version_buf, sizeof(version_buf));
    liot_trace("current version:  %s", version_buf);
    ret = Liot_FotaUpgrade(&fota_config);
    liot_trace("get fota result is %d", ret);
    liot_rtos_task_sleep_ms(1000);
    liot_rtos_task_delete(NULL);
}