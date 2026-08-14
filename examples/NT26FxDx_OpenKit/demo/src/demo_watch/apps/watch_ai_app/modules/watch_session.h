#ifndef WATCH_SESSION_H
#define WATCH_SESSION_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATCH_SESSION_NONE = 0,
    WATCH_SESSION_CAMERA,
    WATCH_SESSION_SCAN,
    WATCH_SESSION_PLAYER,
    WATCH_SESSION_RECORDER,
    WATCH_SESSION_GIF,
} watch_session_t;

typedef int (*watch_session_stop_cb_t)(void);

int watch_session_open(watch_session_t session,
                       const char *name,
                       watch_session_stop_cb_t stop);
int watch_session_close(watch_session_t session);
int watch_session_go_back(void);
watch_session_t watch_session_current(void);
const char *watch_session_name(watch_session_t session);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_SESSION_H */
