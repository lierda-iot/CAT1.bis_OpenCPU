#ifndef WATCH_PAGE_H
#define WATCH_PAGE_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATCH_PAGE_HOME = 0,
    WATCH_PAGE_CAMERA_PREVIEW,
    WATCH_PAGE_SCAN_PREVIEW,
    WATCH_PAGE_PLAYER_LIST,
    WATCH_PAGE_PLAYER_NOW_PLAYING,
    WATCH_PAGE_RECORDER,
    WATCH_PAGE_GIF,
} watch_page_t;

int watch_page_replace(watch_page_t page);
int watch_page_push(watch_page_t page);
int watch_page_pop(void);
int watch_page_go_back(void);
watch_page_t watch_page_current(void);
const char *watch_page_name(watch_page_t page);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_PAGE_H */
