#include "watch_camera.h"

#include "app_config.h"
#include "watch_camera_profile.h"
#include "watch_camera_session.h"
#include "watch_page.h"
#include "watch_session.h"

#include <string.h>

#include "app_display_service.h"
#include "app_event.h"
#include "app_osal.h"

#if WATCH_AI_ENABLE_CAMERA && WATCH_AI_ENABLE_LIOT_CAMERA_PORT
#include "app_camera_port_liot.h"
#endif

#if WATCH_AI_ENABLE_SCAN
#include "app_scan_service.h"
#endif

static bool watch_camera_ret_ok(int ret)
{
    return ret == APP_OK ||
           ret == APP_ERR_NOT_SUPPORTED ||
           ret == APP_ERR_NOT_READY;
}

static void watch_camera_post_event(app_event_id_t id)
{
    app_event_t event;
    int ret;

    memset(&event, 0, sizeof(event));
    event.id = id;
    ret = app_event_post(&event);
    if (ret != APP_OK) {
        app_log("watch event post failed: %s ret=%d", app_event_name(id), ret);
    }
}

int watch_camera_prepare_port(const app_camera_config_t *camera_config)
{
#if WATCH_AI_ENABLE_CAMERA && WATCH_AI_ENABLE_LIOT_CAMERA_PORT
    app_camera_liot_config_t port_config;
    int ret;

    if (camera_config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ret = app_camera_liot_get_default_config(&port_config);
    if (ret != APP_OK) {
        return ret;
    }

    port_config.width = camera_config->width;
    port_config.height = camera_config->height;
    port_config.bytes_per_pixel = camera_config->bytes_per_pixel;
    port_config.output_format = camera_config->output_format;
    port_config.capture_timeout_ms = camera_config->capture_timeout_ms;
    port_config.capture_period_ms = camera_config->capture_period_ms;
    port_config.frame_buffer_capacity = camera_config->frame_buffer_capacity;

    ret = app_camera_liot_setup(&port_config);
    if (ret != APP_OK) {
        app_log("watch camera LIOT setup failed: %d", ret);
        return ret;
    }
    ret = app_camera_liot_register();
    if (ret != APP_OK) {
        app_log("watch camera LIOT register failed: %d", ret);
        return ret;
    }

    app_log("watch camera LIOT prepared: %ux%u %s capacity=%lu cspi=%d i2c=%d sda=%d scl=%d",
            (unsigned int)camera_config->width,
            (unsigned int)camera_config->height,
            app_camera_output_name(camera_config->output_format),
            (unsigned long)camera_config->frame_buffer_capacity,
            port_config.cspi_port,
            port_config.i2c_num,
            port_config.i2c_sda_pin,
            port_config.i2c_scl_pin);
    return APP_OK;
#else
    (void)camera_config;
    return APP_ERR_NOT_SUPPORTED;
#endif
}

#if WATCH_AI_ENABLE_CAMERA && WATCH_AI_ENABLE_DISPLAY
static int watch_camera_display_frame_format_from_camera(app_camera_output_t output,
                                                         app_display_frame_format_t *format,
                                                         uint8_t *bytes_per_pixel)
{
    if (format == NULL || bytes_per_pixel == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    switch (output) {
    case APP_CAMERA_OUTPUT_GRAY:
        *format = APP_DISPLAY_FRAME_FORMAT_GRAY;
        *bytes_per_pixel = 1U;
        return APP_OK;
    case APP_CAMERA_OUTPUT_YUYV:
        *format = APP_DISPLAY_FRAME_FORMAT_YUYV;
        *bytes_per_pixel = 2U;
        return APP_OK;
    case APP_CAMERA_OUTPUT_RGB565:
        *format = APP_DISPLAY_FRAME_FORMAT_RGB565;
        *bytes_per_pixel = 2U;
        return APP_OK;
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}
#endif

int watch_camera_present_frame(const app_camera_frame_t *camera_frame)
{
#if WATCH_AI_ENABLE_CAMERA && WATCH_AI_ENABLE_DISPLAY
    app_display_camera_frame_t display_frame;
    app_display_frame_format_t format;
    uint8_t bytes_per_pixel;
    int ret;
    static uint32_t frame_drop_count;

    if (camera_frame == NULL || camera_frame->data == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ret = watch_camera_display_frame_format_from_camera(camera_frame->output_format,
                                                        &format,
                                                        &bytes_per_pixel);
    if (ret != APP_OK) {
        return ret;
    }

    memset(&display_frame, 0, sizeof(display_frame));
    display_frame.frame_id = camera_frame->frame_id;
    display_frame.data = camera_frame->data;
    display_frame.len = camera_frame->len;
    display_frame.width = camera_frame->width;
    display_frame.height = camera_frame->height;
    display_frame.bytes_per_pixel = bytes_per_pixel;
    display_frame.format = format;
    ret = app_display_present_camera_frame(&display_frame);
    if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED) {
        frame_drop_count++;
        if (frame_drop_count <= 3U || (frame_drop_count % 30U) == 0U) {
            app_log("watch camera frame display failed: frame=%lu ret=%d",
                    (unsigned long)camera_frame->frame_id,
                    ret);
        }
    }
    return ret;
#else
    (void)camera_frame;
    return APP_ERR_NOT_SUPPORTED;
#endif
}

static int watch_camera_stop_service(int *last_ret)
{
#if WATCH_AI_ENABLE_CAMERA
    int ret = APP_OK;
    int deinit_ret;

    if (app_camera_get_config() != NULL) {
        ret = watch_camera_session_stop();
        if (!watch_camera_ret_ok(ret)) {
            app_log("watch camera session stop failed: %d", ret);
            if (last_ret != NULL) {
                *last_ret = ret;
            }
            return ret;
        }
        deinit_ret = app_camera_deinit();
        if (!watch_camera_ret_ok(deinit_ret)) {
            app_log("watch camera preview deinit failed: %d", deinit_ret);
            if (last_ret != NULL) {
                *last_ret = deinit_ret;
            }
            return deinit_ret;
        }
    }

    if (last_ret != NULL) {
        *last_ret = ret;
    }
    return APP_OK;
#else
    (void)last_ret;
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_camera_init(void)
{
#if WATCH_AI_ENABLE_CAMERA
    app_camera_config_t camera_config;
    int ret;

    ret = watch_camera_profile_make_preview(&camera_config);
    if (ret != APP_OK) {
        app_log("watch init camera profile failed: %d", ret);
        return ret;
    }

    app_log("watch init camera: default profile %ux%u %s timeout=%lu period=%lu",
            (unsigned int)camera_config.width,
            (unsigned int)camera_config.height,
            app_camera_output_name(camera_config.output_format),
            (unsigned long)camera_config.capture_timeout_ms,
            (unsigned long)camera_config.capture_period_ms);

#if WATCH_AI_ENABLE_LIOT_CAMERA_PORT
    ret = watch_camera_prepare_port(&camera_config);
    if (ret != APP_OK) {
        return ret;
    }
#endif

    app_log("watch init camera complete: service init deferred");
    return APP_OK;
#else
    app_log("watch init camera: disabled");
    return APP_OK;
#endif
}

int watch_camera_start_preview(void)
{
#if WATCH_AI_ENABLE_CAMERA
    app_camera_config_t camera_config;
    watch_camera_session_config_t session_config;
    int ret;

    app_log("watch camera preview start");
    if (watch_session_current() == WATCH_SESSION_CAMERA) {
        app_log("watch camera preview already running");
        return APP_OK;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch camera preview start rejected: session=%s",
                watch_session_name(watch_session_current()));
        return APP_ERR_BUSY;
    }
    if (app_camera_get_config() != NULL) {
        app_log("watch camera preview cleanup stale camera service");
        ret = watch_camera_stop_service(NULL);
        if (!watch_camera_ret_ok(ret)) {
            app_log("watch camera preview stale cleanup failed: %d", ret);
            return ret;
        }
    }
    ret = watch_camera_profile_make_preview(&camera_config);
    if (ret != APP_OK) {
        app_log("watch camera preview config failed: %d", ret);
        return ret;
    }
#if WATCH_AI_ENABLE_LIOT_CAMERA_PORT
    ret = watch_camera_prepare_port(&camera_config);
    if (ret != APP_OK) {
        return ret;
    }
#endif
#if WATCH_AI_ENABLE_SCAN
    if (app_scan_is_initialized()) {
        ret = app_scan_deinit();
        if (ret != APP_OK) {
            app_log("watch camera preview stale scan cleanup failed: %d", ret);
            return ret;
        }
    }
#endif
    ret = app_camera_init(&camera_config);
    if (ret != APP_OK) {
        app_log("watch camera preview service init failed: %d", ret);
        return ret;
    }

    ret = watch_session_open(WATCH_SESSION_CAMERA,
                             "camera",
                             watch_camera_stop_preview);
    if (ret != APP_OK) {
        (void)app_camera_deinit();
        app_log("watch camera preview session open failed: %d", ret);
        return ret;
    }
#if WATCH_AI_ENABLE_DISPLAY
    (void)watch_page_replace(WATCH_PAGE_CAMERA_PREVIEW);
    (void)app_display_set_status("CAMERA");
    (void)app_display_set_chat_message(APP_DISPLAY_ROLE_SYSTEM, "starting");
#endif

    memset(&session_config, 0, sizeof(session_config));
    session_config.mode = WATCH_CAMERA_SESSION_PREVIEW;
    session_config.name = "camera";
    ret = watch_camera_session_start(&session_config);
    if (ret != APP_OK) {
        app_event_t event;

        memset(&event, 0, sizeof(event));
        event.id = APP_EV_CAMERA_ERROR;
        event.data.error.code = ret;
        event.data.error.message = "camera start failed";
        (void)app_event_post(&event);
        (void)watch_session_close(WATCH_SESSION_CAMERA);
#if WATCH_AI_ENABLE_DISPLAY
        (void)watch_page_replace(WATCH_PAGE_HOME);
        (void)app_display_set_status("CAMERA ERROR");
#endif
        (void)app_camera_deinit();
        app_log("watch camera preview start failed: %d", ret);
        return ret;
    }

    watch_camera_post_event(APP_EV_CAMERA_START);
    app_log("watch camera preview started");
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_camera_stop_preview(void)
{
#if WATCH_AI_ENABLE_CAMERA
    int service_ret;
    int ret;

    app_log("watch camera preview stop");
    ret = watch_camera_stop_service(&service_ret);
    if (ret != APP_OK) {
        return ret;
    }
    (void)watch_session_close(WATCH_SESSION_CAMERA);
    watch_camera_post_event(APP_EV_CAMERA_STOP);
#if WATCH_AI_ENABLE_DISPLAY
    (void)watch_page_replace(WATCH_PAGE_HOME);
    (void)app_display_set_status("IDLE");
#endif
    app_log("watch camera preview stopped: %d", service_ret);
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_camera_shutdown(void)
{
    int ret = watch_camera_stop_service(NULL);

    if (watch_session_current() == WATCH_SESSION_CAMERA) {
        (void)watch_session_close(WATCH_SESSION_CAMERA);
    }
    return ret;
}
