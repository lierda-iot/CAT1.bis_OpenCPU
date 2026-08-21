#include "watch_page.h"

#include "app_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_display_service.h"
#include "app_osal.h"

#define WATCH_PAGE_STACK_DEPTH 8U

static watch_page_t s_page_stack[WATCH_PAGE_STACK_DEPTH] = {WATCH_PAGE_HOME};
static uint8_t s_page_depth = 1U;

const char *watch_page_name(watch_page_t page)
{
    switch (page) {
    case WATCH_PAGE_HOME:
        return "home";
    case WATCH_PAGE_CAMERA_PREVIEW:
        return "camera_preview";
    case WATCH_PAGE_SCAN_PREVIEW:
        return "scan_preview";
    case WATCH_PAGE_PLAYER_LIST:
        return "player_list";
    case WATCH_PAGE_PLAYER_NOW_PLAYING:
        return "player_now_playing";
    case WATCH_PAGE_RECORDER:
        return "recorder";
    case WATCH_PAGE_GIF:
        return "gif";
    case WATCH_PAGE_DRAWING:
        return "drawing";
    default:
        return "unknown";
    }
}

watch_page_t watch_page_current(void)
{
    return s_page_stack[s_page_depth - 1U];
}

static bool watch_page_valid(watch_page_t page)
{
    switch (page) {
    case WATCH_PAGE_HOME:
    case WATCH_PAGE_CAMERA_PREVIEW:
    case WATCH_PAGE_SCAN_PREVIEW:
    case WATCH_PAGE_PLAYER_LIST:
    case WATCH_PAGE_PLAYER_NOW_PLAYING:
    case WATCH_PAGE_RECORDER:
    case WATCH_PAGE_GIF:
    case WATCH_PAGE_DRAWING:
        return true;
    default:
        return false;
    }
}

static bool watch_page_is_feature_root(watch_page_t page)
{
    return page == WATCH_PAGE_HOME ||
           page == WATCH_PAGE_CAMERA_PREVIEW ||
           page == WATCH_PAGE_SCAN_PREVIEW ||
           page == WATCH_PAGE_PLAYER_LIST ||
           page == WATCH_PAGE_PLAYER_NOW_PLAYING ||
           page == WATCH_PAGE_RECORDER ||
           page == WATCH_PAGE_GIF ||
           page == WATCH_PAGE_DRAWING;
}

static int watch_page_to_screen(watch_page_t page, app_display_screen_t *screen)
{
#if WATCH_AI_ENABLE_DISPLAY
    if (screen == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    switch (page) {
    case WATCH_PAGE_HOME:
        *screen = APP_DISPLAY_SCREEN_HOME;
        return APP_OK;
    case WATCH_PAGE_CAMERA_PREVIEW:
    case WATCH_PAGE_SCAN_PREVIEW:
        *screen = APP_DISPLAY_SCREEN_CAMERA;
        return APP_OK;
    case WATCH_PAGE_PLAYER_LIST:
        *screen = APP_DISPLAY_SCREEN_PLAYER_LIST;
        return APP_OK;
    case WATCH_PAGE_PLAYER_NOW_PLAYING:
        *screen = APP_DISPLAY_SCREEN_PLAYER_NOW_PLAYING;
        return APP_OK;
    case WATCH_PAGE_RECORDER:
        *screen = APP_DISPLAY_SCREEN_RECORDER;
        return APP_OK;
    case WATCH_PAGE_GIF:
        *screen = APP_DISPLAY_SCREEN_GIF;
        return APP_OK;
    case WATCH_PAGE_DRAWING:
        *screen = APP_DISPLAY_SCREEN_DRAWING;
        return APP_OK;
    default:
        return APP_ERR_INVALID_ARG;
    }
#else
    (void)page;
    (void)screen;
    return APP_OK;
#endif
}

static int watch_page_show(watch_page_t page)
{
#if WATCH_AI_ENABLE_DISPLAY
    app_display_screen_t screen;
    int ret = watch_page_to_screen(page, &screen);

    if (ret != APP_OK) {
        return ret;
    }
    ret = app_display_show_screen(screen);
    if (ret != APP_OK) {
        app_log("watch page show failed: %s ret=%d",
                watch_page_name(page), ret);
    }
    return ret;
#else
    (void)page;
    return APP_OK;
#endif
}

int watch_page_replace(watch_page_t page)
{
    watch_page_t old_page = watch_page_current();
    int ret;

    if (!watch_page_valid(page)) {
        return APP_ERR_INVALID_ARG;
    }

    ret = watch_page_show(page);
    if (ret != APP_OK) {
        app_log("watch page replace failed: %s -> %s ret=%d",
                watch_page_name(old_page),
                watch_page_name(page),
                ret);
        return ret;
    }

    s_page_stack[0] = page;
    s_page_depth = 1U;
    app_log("watch page replace: %s -> %s",
            watch_page_name(old_page),
            watch_page_name(page));
    return APP_OK;
}

int watch_page_push(watch_page_t page)
{
    watch_page_t old_page = watch_page_current();
    int ret;

    if (!watch_page_valid(page)) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_page_depth >= WATCH_PAGE_STACK_DEPTH) {
        app_log("watch page push failed: stack full current=%s request=%s",
                watch_page_name(old_page),
                watch_page_name(page));
        return APP_ERR_NO_MEMORY;
    }

    ret = watch_page_show(page);
    if (ret != APP_OK) {
        app_log("watch page push failed: %s -> %s ret=%d",
                watch_page_name(old_page),
                watch_page_name(page),
                ret);
        return ret;
    }

    s_page_stack[s_page_depth] = page;
    s_page_depth++;
    app_log("watch page push: %s -> %s depth=%u",
            watch_page_name(old_page),
            watch_page_name(page),
            (unsigned int)s_page_depth);
    return APP_OK;
}

int watch_page_pop(void)
{
    watch_page_t old_page;
    watch_page_t new_page;
    int ret;

    if (s_page_depth <= 1U) {
        app_log("watch page pop ignored: root=%s",
                watch_page_name(watch_page_current()));
        return APP_ERR_NOT_SUPPORTED;
    }

    old_page = watch_page_current();
    new_page = s_page_stack[s_page_depth - 2U];
    ret = watch_page_show(new_page);
    if (ret != APP_OK) {
        app_log("watch page pop failed: %s -> %s ret=%d",
                watch_page_name(old_page),
                watch_page_name(new_page),
                ret);
        return ret;
    }

    s_page_depth--;
    app_log("watch page pop: %s -> %s depth=%u",
            watch_page_name(old_page),
            watch_page_name(new_page),
            (unsigned int)s_page_depth);
    return APP_OK;
}

int watch_page_go_back(void)
{
    watch_page_t page = watch_page_current();

    if (s_page_depth > 1U) {
        return watch_page_pop();
    }
    if (watch_page_is_feature_root(page)) {
        app_log("watch page back ignored: root=%s",
                watch_page_name(page));
        return APP_ERR_NOT_SUPPORTED;
    }

    app_log("watch page back unsupported: %s",
            watch_page_name(page));
    return APP_ERR_NOT_SUPPORTED;
}
