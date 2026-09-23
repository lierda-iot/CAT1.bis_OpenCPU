#include "watch_gif.h"

#include "app_config.h"
#include "watch_page.h"
#include "watch_session.h"

#include "app_display_service.h"
#include "app_osal.h"

#ifndef WATCH_AI_ENABLE_GIF
#define WATCH_AI_ENABLE_GIF 0
#endif

int watch_gif_init(void)
{
#if WATCH_AI_ENABLE_GIF
    app_log("watch init gif complete");
#else
    app_log("watch init gif: disabled");
#endif
    return APP_OK;
}

int watch_gif_start(void)
{
#if WATCH_AI_ENABLE_GIF && WATCH_AI_ENABLE_DISPLAY
    int ret;

    app_log("watch gif start");
    ret = watch_session_open(WATCH_SESSION_GIF, "gif", watch_gif_stop);
    if (ret != APP_OK) {
        return ret;
    }

    ret = watch_page_replace(WATCH_PAGE_GIF);
    if (ret != APP_OK) {
        (void)watch_session_close(WATCH_SESSION_GIF);
        return ret;
    }

    (void)app_display_set_status("GIF");
    app_log("watch gif started");
    return APP_OK;
#else
    app_log("watch gif start rejected: disabled");
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_gif_stop(void)
{
#if WATCH_AI_ENABLE_GIF && WATCH_AI_ENABLE_DISPLAY
    app_log("watch gif stop");
    (void)watch_page_replace(WATCH_PAGE_HOME);
    (void)app_display_set_status("IDLE");
    (void)watch_session_close(WATCH_SESSION_GIF);
    return APP_OK;
#else
    return APP_OK;
#endif
}
