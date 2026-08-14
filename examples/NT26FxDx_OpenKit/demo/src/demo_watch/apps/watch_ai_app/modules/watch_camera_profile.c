#include "watch_camera_profile.h"

#include <string.h>

static void watch_camera_profile_fill_common(app_camera_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->width = WATCH_CAMERA_PROFILE_WIDTH;
    config->height = WATCH_CAMERA_PROFILE_HEIGHT;
    config->capture_timeout_ms = WATCH_CAMERA_PROFILE_TIMEOUT_MS;
    config->capture_period_ms = WATCH_CAMERA_PROFILE_PERIOD_MS;
}

int watch_camera_profile_make_preview(app_camera_config_t *config)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    watch_camera_profile_fill_common(config);
    config->bytes_per_pixel = 2U;
    config->output_format = APP_CAMERA_OUTPUT_YUYV;
    return APP_OK;
}

int watch_camera_profile_make_scan(app_camera_config_t *config,
                                   uint32_t frame_buffer_capacity)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (frame_buffer_capacity < WATCH_CAMERA_PROFILE_SCAN_REQUIRED_CAPACITY) {
        return APP_ERR_NO_MEMORY;
    }

    watch_camera_profile_fill_common(config);
    config->bytes_per_pixel = 1U;
    config->output_format = APP_CAMERA_OUTPUT_GRAY;
    config->frame_buffer_capacity = frame_buffer_capacity;
    return APP_OK;
}
