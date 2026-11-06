#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdint.h>

#define __L_APP_HEAD_ __attribute__((section(".app_header")))
#define __L_APP_INFO_ __attribute__((section(".app_info")))

#define APP_HEAD_NAME(slot)    (0x30707061U + ((uint32_t)(slot) << 24))

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

void main(uint32_t syscallTable, uint32_t appImageLocation, uint32_t reserved2, uint32_t reserved3);

#endif /* __MAIN_H__ */