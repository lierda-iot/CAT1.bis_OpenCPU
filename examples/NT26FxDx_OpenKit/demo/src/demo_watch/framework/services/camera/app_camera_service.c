#include "app_camera_service.h"

#include <stddef.h>
#include <string.h>

#include "app_osal.h"

static const app_camera_port_t *s_port;
static app_camera_config_t s_config;
static bool s_initialized;

static const app_camera_config_t s_default_config = {
    .width = 240U,
    .height = 240U,
    .bytes_per_pixel = 2U,
    .output_format = APP_CAMERA_OUTPUT_RGB565,
    .capture_timeout_ms = 1000U,
    .capture_period_ms = 120U,
};

static const app_camera_port_ops_t *camera_ops(void)
{
    return (s_port != NULL) ? s_port->ops : NULL;
}

static int camera_require_ready(const app_camera_port_ops_t **ops)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    *ops = camera_ops();
    if (*ops == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return APP_OK;
}

int app_camera_register_port(const app_camera_port_t *port)
{
    if (port == NULL || port->ops == NULL || port->ops->init == NULL) {
        app_log("camera port register rejected");
        return APP_ERR_INVALID_ARG;
    }
    s_port = port;
    app_log("camera port registered");
    return APP_OK;
}

int app_camera_init(const app_camera_config_t *config)
{
    const app_camera_port_ops_t *ops = camera_ops();
    app_camera_caps_t caps;
    int ret;

    s_config = (config != NULL) ? *config : s_default_config;
    if (s_config.width == 0U) {
        s_config.width = s_default_config.width;
    }
    if (s_config.height == 0U) {
        s_config.height = s_default_config.height;
    }
    if (s_config.bytes_per_pixel == 0U) {
        s_config.bytes_per_pixel = s_default_config.bytes_per_pixel;
    }
    if (s_config.capture_timeout_ms == 0U) {
        s_config.capture_timeout_ms = s_default_config.capture_timeout_ms;
    }
    /* capture_period_ms == 0 means no delay between successful captures. */
    s_initialized = false;

    if (ops == NULL || ops->init == NULL) {
        app_log("camera init rejected: port unavailable");
        return APP_ERR_NOT_SUPPORTED;
    }

    memset(&caps, 0, sizeof(caps));
    if (s_port != NULL) {
        caps = s_port->caps;
    }
    caps.width = s_config.width;
    caps.height = s_config.height;
    caps.bytes_per_pixel = s_config.bytes_per_pixel;
    caps.output_format = s_config.output_format;
    app_log("camera service init: %ux%u bpp=%u %s timeout=%lu period=%lu",
            (unsigned int)s_config.width,
            (unsigned int)s_config.height,
            (unsigned int)s_config.bytes_per_pixel,
            app_camera_output_name(s_config.output_format),
            (unsigned long)s_config.capture_timeout_ms,
            (unsigned long)s_config.capture_period_ms);

    ret = ops->init(&caps);
    if (ret != APP_OK) {
        app_log("camera port init failed: %d", ret);
        return ret;
    }

    s_initialized = true;
    app_log("camera initialized: %ux%u %s",
            (unsigned int)s_config.width,
            (unsigned int)s_config.height,
            app_camera_output_name(s_config.output_format));
    return APP_OK;
}

int app_camera_deinit(void)
{
    const app_camera_port_ops_t *ops;
    int ret = camera_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    if (ops->deinit == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = ops->deinit();
    if (ret == APP_OK) {
        s_initialized = false;
    }
    return ret;
}

int app_camera_capture_frame(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    const app_camera_port_ops_t *ops;
    int ret;

    if (data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    ret = camera_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->capture_frame != NULL) ?
           ops->capture_frame(data, len, timeout_ms) :
           APP_ERR_NOT_SUPPORTED;
}

int app_camera_get_frame_buffer(uint8_t **data, uint32_t *len, uint32_t *capacity)
{
    const app_camera_port_ops_t *ops;
    int ret;

    if (data == NULL || len == NULL || capacity == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = camera_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return (ops->get_frame_buffer != NULL) ?
           ops->get_frame_buffer(data, len, capacity) :
           APP_ERR_NOT_SUPPORTED;
}

const app_camera_config_t *app_camera_get_config(void)
{
    return s_initialized ? &s_config : NULL;
}

const app_camera_caps_t *app_camera_get_caps(void)
{
    return (s_port != NULL) ? &s_port->caps : NULL;
}

const char *app_camera_output_name(app_camera_output_t output)
{
    switch (output) {
    case APP_CAMERA_OUTPUT_GRAY:
        return "gray";
    case APP_CAMERA_OUTPUT_YUYV:
        return "yuyv";
    case APP_CAMERA_OUTPUT_RGB565:
        return "rgb565";
    default:
        return "unknown";
    }
}
