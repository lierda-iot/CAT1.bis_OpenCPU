#ifndef WATCH_CAMERA_H
#define WATCH_CAMERA_H

#include <stdint.h>

#include "app_camera_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_camera_init(void);
int watch_camera_start_preview(void);
int watch_camera_stop_preview(void);
int watch_camera_shutdown(void);
int watch_camera_prepare_port(const app_camera_config_t *camera_config);
int watch_camera_present_frame(const app_camera_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_CAMERA_H */
