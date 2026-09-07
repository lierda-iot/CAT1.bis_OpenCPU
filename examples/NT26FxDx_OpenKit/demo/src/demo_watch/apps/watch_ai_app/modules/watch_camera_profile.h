#ifndef WATCH_CAMERA_PROFILE_H
#define WATCH_CAMERA_PROFILE_H

#include <stdint.h>

#include "app_camera_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WATCH_CAMERA_PROFILE_WIDTH 640U
#define WATCH_CAMERA_PROFILE_HEIGHT 480U
#define WATCH_CAMERA_PROFILE_TIMEOUT_MS 1000U
#define WATCH_CAMERA_PROFILE_PERIOD_MS 0U
#define WATCH_CAMERA_PROFILE_SCAN_FRAME_BYTES \
    (WATCH_CAMERA_PROFILE_WIDTH * WATCH_CAMERA_PROFILE_HEIGHT)
#define WATCH_CAMERA_PROFILE_SCAN_REQUIRED_CAPACITY \
    (WATCH_CAMERA_PROFILE_SCAN_FRAME_BYTES * 2U)

int watch_camera_profile_make_preview(app_camera_config_t *config);
int watch_camera_profile_make_scan(app_camera_config_t *config,
                                   uint32_t frame_buffer_capacity);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_CAMERA_PROFILE_H */
