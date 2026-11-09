#ifndef DEMO_LVGL_PHOTO_LFS_BOOT_DIAG_H
#define DEMO_LVGL_PHOTO_LFS_BOOT_DIAG_H

#include "liot_log.h"

#ifndef APP_BUILD_GIT_SHORT
#define APP_BUILD_GIT_SHORT "unknown"
#endif

#ifndef APP_BUILD_PROJECT
#define APP_BUILD_PROJECT "unknown"
#endif

#ifndef APP_BUILD_MODE
#define APP_BUILD_MODE "unknown"
#endif

#ifndef APP_BUILD_TIMESTAMP
#define APP_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

static inline void demo_lvgl_photo_lfs_boot_diag_log_config(const char *prefix)
{
    liot_trace("%s build git=%s project=%s mode=%s built=%s gif=%d jpg=%d png=%d "
               "force_reseed=%d",
               (prefix != NULL) ? prefix : "[demo_lvgl_photo_lfs]",
               APP_BUILD_GIT_SHORT,
               APP_BUILD_PROJECT,
               APP_BUILD_MODE,
               APP_BUILD_TIMESTAMP,
               DEMO_LVGL_PHOTO_LFS_ENABLE_GIF,
               DEMO_LVGL_PHOTO_LFS_ENABLE_JPEG,
               DEMO_LVGL_PHOTO_LFS_ENABLE_PNG,
               DEMO_LVGL_PHOTO_LFS_FORCE_RESEED);
}

#endif
