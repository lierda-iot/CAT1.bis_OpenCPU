#include <stdio.h>
#include "appram.h"
#include "liot_os.h"
#include "user_main.h"
#include "liot_log.h"
#include "liot_iocfg.h"
#include "lierda_defcfg.h"
#include "mem_map.h"

#define __L_APP_HEAD_ __attribute__((section(".app_header")))
#define __L_APP_INFO_ __attribute__((section(".app_info")))

typedef struct
{
    char *appBuildDate;     // Build date (YYYYMMDD format)
    char *appBuildTime;     // Build time (HHMMSS format)
    char *appsdkVersionStr; // SDK version string
    char *appVersionStr;    // APP version string
    char *cgrmVersionStr;   // CGRM version string
    char *modemType;        // Modem type string
} app_info_t;

typedef struct _app_header_
{
    uint32_t name;
    uint32_t main_addr;
    uint32_t table1;
    uint32_t table2;
    uint32_t filesys_addr;
    uint32_t filesys_size;
    uint32_t appInfo;
} app_header_t;

extern DriveioMap hwioResource;
extern appcfg_t lsdk_cfg;

void appUserTask(void *argv)
{
    user_main();
    liot_rtos_task_delete(NULL);
}

void userTaskEnter(void)
{
    liot_task_t usetTsk = NULL;
    liot_rtos_task_create(&usetTsk, 10240, APP_PRIORITY_NORMAL, "appUserTask", &appUserTask, NULL);
}

void main(uint32_t syscallTable, uint32_t appImageLocation, uint32_t reserved2, uint32_t reserved3)
{
    copyRwDataFromBin();
    setZiDataToZero();
    userTaskEnter();
}

__L_APP_INFO_ app_info_t appInfo = {
    .appBuildDate = __DATE__,
    .appBuildTime = __TIME__,
    .appsdkVersionStr = SDK_VERSION,
    .appVersionStr = APP_VERSION,
    .cgrmVersionStr = CGRM_VERSION,
    .modemType = MODEL_TYPE,
};

__L_APP_HEAD_ app_header_t head =
{
    .name = 0x30707061, //app0
    .main_addr = (uint32_t)main,
    .table1 = (uint32_t)&hwioResource,
    .table2 = (uint32_t)&lsdk_cfg,
    .filesys_addr = PKGFLXFS_IMG_ADDR,
    .filesys_size = PKGFLXFS_IMG_SIZE,
    .appInfo = (uint32_t)&appInfo,
};