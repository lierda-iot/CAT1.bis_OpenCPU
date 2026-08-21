#include "watch_drawing.h"

#include "app_config.h"
#include "watch_page.h"
#include "watch_session.h"

#include "app_display_service.h"
#include "app_osal.h"

int watch_drawing_init(void)
{
    app_log("watch init drawing complete");
    return APP_OK;
}

int watch_drawing_start(void)
{
#if WATCH_AI_ENABLE_DISPLAY
    int ret;

    app_log("watch drawing start");
    ret = watch_session_open(WATCH_SESSION_DRAWING, "drawing", watch_drawing_stop);
    if (ret != APP_OK) {
        return ret;
    }

    ret = watch_page_replace(WATCH_PAGE_DRAWING);
    if (ret != APP_OK) {
        (void)watch_session_close(WATCH_SESSION_DRAWING);
        return ret;
    }

    ret = app_display_set_status("Drawing");
    if (ret != APP_OK) {
        app_log("watch drawing status set failed: %d", ret);
    }
    app_log("watch drawing started");
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_drawing_stop(void)
{
#if WATCH_AI_ENABLE_DISPLAY
    app_log("watch drawing stop");
    (void)watch_page_replace(WATCH_PAGE_HOME);
    (void)app_display_set_status("IDLE");
    (void)watch_session_close(WATCH_SESSION_DRAWING);
    return APP_OK;
#else
    return APP_OK;
#endif
}
