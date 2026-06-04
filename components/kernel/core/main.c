#include <stdio.h>
#include "appram.h"
#include "liot_os.h"
#include "user_main.h"
#include "liot_log.h"
#include "liot_iocfg.h"
#include "lierda_defcfg.h"
#include "mem_map.h"

/***************** lwip ****************/
/* sync basepack lib lwip errno to app*/
extern int errno;
void synclwiperrno(int err) { errno = err;}
void Liot_SetAppSockErrnoCB(void* cb);
/***************** end *****************/

#define __L_APP_HEAD_ __attribute__((section(".app_header")))
typedef struct _app_header_
{
    uint32_t name;
    uint32_t main_addr;
    uint32_t table1;
    uint32_t table2;
    uint32_t filesys_addr;
    uint32_t filesys_size;
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
    __Liot_SetAppSdkversion(SDK_VERSION "_" APP_VERSION);
    Liot_SetAppSockErrnoCB(synclwiperrno);
    userTaskEnter();
}

__L_APP_HEAD_ app_header_t head =
{
    .name = 0x30707061, //app0
    .main_addr = (uint32_t)main,
    .table1 = (uint32_t)&hwioResource,
    .table2 = (uint32_t)&lsdk_cfg,
    .filesys_addr = PKGFLXFS_IMG_ADDR,
    .filesys_size = PKGFLXFS_IMG_SIZE,
};