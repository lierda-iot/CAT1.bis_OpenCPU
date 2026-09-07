#ifndef WATCH_CAMERA_SESSION_H
#define WATCH_CAMERA_SESSION_H

#include <stdbool.h>

#include "app_camera_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATCH_CAMERA_SESSION_PREVIEW = 0,
    WATCH_CAMERA_SESSION_SCAN_PREVIEW,
} watch_camera_session_mode_t;

typedef struct {
    watch_camera_session_mode_t mode;
    const char *name;
} watch_camera_session_config_t;

int watch_camera_session_start(const watch_camera_session_config_t *config);
int watch_camera_session_stop(void);
bool watch_camera_session_is_running(void);
const char *watch_camera_session_mode_name(watch_camera_session_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_CAMERA_SESSION_H */
