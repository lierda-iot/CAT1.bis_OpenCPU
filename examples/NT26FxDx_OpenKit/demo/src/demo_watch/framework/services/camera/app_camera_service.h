#ifndef APP_CAMERA_SERVICE_H
#define APP_CAMERA_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_CAMERA_OUTPUT_GRAY = 0,
    APP_CAMERA_OUTPUT_YUYV,
    APP_CAMERA_OUTPUT_RGB565,
} app_camera_output_t;

typedef struct {
    bool has_capture;
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
    app_camera_output_t output_format;
} app_camera_caps_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
    app_camera_output_t output_format;
    uint32_t capture_timeout_ms;
    uint32_t capture_period_ms;
    uint32_t frame_buffer_capacity;
} app_camera_config_t;

typedef struct {
    uint32_t frame_id;
    const uint8_t *data;
    uint32_t len;
    uint32_t buffer_capacity;
    uint16_t width;
    uint16_t height;
    app_camera_output_t output_format;
} app_camera_frame_t;

typedef struct {
    int (*init)(const app_camera_caps_t *caps);
    int (*deinit)(void);
    int (*capture_frame)(uint8_t *data, uint32_t len, uint32_t timeout_ms);
    int (*get_frame_buffer)(uint8_t **data, uint32_t *len, uint32_t *capacity);
} app_camera_port_ops_t;

typedef struct {
    app_camera_caps_t caps;
    const app_camera_port_ops_t *ops;
} app_camera_port_t;

int app_camera_register_port(const app_camera_port_t *port);
int app_camera_init(const app_camera_config_t *config);
int app_camera_deinit(void);
int app_camera_capture_frame(uint8_t *data, uint32_t len, uint32_t timeout_ms);
int app_camera_get_frame_buffer(uint8_t **data, uint32_t *len, uint32_t *capacity);
const app_camera_config_t *app_camera_get_config(void);
const app_camera_caps_t *app_camera_get_caps(void);
const char *app_camera_output_name(app_camera_output_t output);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAMERA_SERVICE_H */
