#include "watch_scan.h"

#include "app_config.h"
#include "watch_camera.h"
#include "watch_camera_profile.h"
#include "watch_camera_session.h"
#include "watch_page.h"
#include "watch_session.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_camera_service.h"
#include "app_display_service.h"
#include "app_event.h"
#include "app_osal.h"
#include "app_scan_service.h"

#if WATCH_AI_ENABLE_SCAN && WATCH_AI_ENABLE_LIOT_SCAN_PORT
#include "app_scan_port_liot.h"
#endif

#define WATCH_SCAN_RESULT_TEXT_BYTES 256U
#define WATCH_SCAN_RESULT_NOTIFY_MS 0U

static char s_scan_result_text[WATCH_SCAN_RESULT_TEXT_BYTES];
static char s_scan_flush_text[WATCH_SCAN_RESULT_TEXT_BYTES];
static uint32_t s_scan_result_frame_id;
static uint32_t s_scan_result_len;
static volatile bool s_scan_result_pending;

static bool watch_scan_ret_ok(int ret)
{
    return ret == APP_OK ||
           ret == APP_ERR_NOT_SUPPORTED ||
           ret == APP_ERR_NOT_READY;
}

static void watch_scan_post_event(app_event_id_t id)
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

static bool watch_scan_should_log_frame(uint32_t frame_id)
{
    return frame_id <= 3U || (frame_id % 30U) == 0U;
}

static void watch_scan_clear_pending_result(void)
{
    s_scan_result_text[0] = '\0';
    s_scan_result_frame_id = 0U;
    s_scan_result_len = 0U;
    s_scan_result_pending = false;
}

static void watch_scan_store_result(const app_scan_event_t *event)
{
    uint32_t copy_len;

    if (event == NULL || event->text == NULL || event->text[0] == '\0') {
        return;
    }
    if (s_scan_result_pending) {
        return;
    }

    copy_len = event->text_len;
    if (copy_len == 0U) {
        copy_len = (uint32_t)strlen(event->text);
    }
    if (copy_len >= WATCH_SCAN_RESULT_TEXT_BYTES) {
        copy_len = WATCH_SCAN_RESULT_TEXT_BYTES - 1U;
    }

    memcpy(s_scan_result_text, event->text, copy_len);
    s_scan_result_text[copy_len] = '\0';
    s_scan_result_frame_id = event->frame_id;
    s_scan_result_len = copy_len;
    s_scan_result_pending = true;
    app_log("watch scan result pending: frame=%lu len=%lu",
            (unsigned long)s_scan_result_frame_id,
            (unsigned long)s_scan_result_len);
    watch_scan_post_event(APP_EV_SCAN_RESULT);
}

static int watch_scan_make_frame(const app_camera_frame_t *camera_frame,
                                 app_scan_frame_t *scan_frame)
{
    if (camera_frame == NULL || scan_frame == NULL ||
        camera_frame->data == NULL ||
        camera_frame->output_format != APP_CAMERA_OUTPUT_GRAY ||
        camera_frame->width != WATCH_CAMERA_PROFILE_WIDTH ||
        camera_frame->height != WATCH_CAMERA_PROFILE_HEIGHT ||
        camera_frame->len < WATCH_CAMERA_PROFILE_SCAN_FRAME_BYTES ||
        camera_frame->buffer_capacity < WATCH_CAMERA_PROFILE_SCAN_REQUIRED_CAPACITY) {
        return APP_ERR_INVALID_ARG;
    }

    memset(scan_frame, 0, sizeof(*scan_frame));
    scan_frame->frame_id = camera_frame->frame_id;
    scan_frame->data = (uint8_t *)camera_frame->data;
    scan_frame->len = WATCH_CAMERA_PROFILE_SCAN_FRAME_BYTES;
    scan_frame->buffer_capacity = camera_frame->buffer_capacity;
    scan_frame->width = WATCH_CAMERA_PROFILE_WIDTH;
    scan_frame->height = WATCH_CAMERA_PROFILE_HEIGHT;
    scan_frame->format = APP_SCAN_FRAME_FORMAT_GRAY8;
    return APP_OK;
}

int watch_scan_process_camera_frame(const app_camera_frame_t *camera_frame)
{
    app_scan_frame_t scan_frame;
    app_scan_event_t scan_event;
    int preview_ret;
    int ret;

    if (camera_frame == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    preview_ret = watch_camera_present_frame(camera_frame);
    if (preview_ret != APP_OK && preview_ret != APP_ERR_NOT_SUPPORTED &&
        watch_scan_should_log_frame(camera_frame->frame_id)) {
        app_log("watch scan preview failed: frame=%lu ret=%d",
                (unsigned long)camera_frame->frame_id,
                preview_ret);
    }

    ret = watch_scan_make_frame(camera_frame, &scan_frame);
    if (ret != APP_OK) {
        if (watch_scan_should_log_frame(camera_frame->frame_id)) {
            app_log("watch scan frame rejected: frame=%lu %ux%u len=%lu cap=%lu fmt=%s ret=%d",
                    (unsigned long)camera_frame->frame_id,
                    (unsigned int)camera_frame->width,
                    (unsigned int)camera_frame->height,
                    (unsigned long)camera_frame->len,
                    (unsigned long)camera_frame->buffer_capacity,
                    app_camera_output_name(camera_frame->output_format),
                    ret);
        }
        return ret;
    }

    ret = app_scan_process_frame(&scan_frame, &scan_event);
    if (ret != APP_OK && watch_scan_should_log_frame(scan_frame.frame_id)) {
        app_log("watch scan process failed: frame=%lu ret=%d",
                (unsigned long)scan_frame.frame_id,
                ret);
    }
    if (ret == APP_OK && scan_event.id == APP_SCAN_EVENT_RESULT) {
        watch_scan_store_result(&scan_event);
    } else if (scan_event.id == APP_SCAN_EVENT_ERROR) {
        app_log("watch scan error: frame=%lu ret=%d message=%s",
                (unsigned long)scan_event.frame_id,
                scan_event.result,
                (scan_event.message != NULL) ? scan_event.message : "");
    }
    return ret;
}

void watch_scan_flush_result(void)
{
    uint32_t frame_id;
    uint32_t text_len;
#if WATCH_AI_ENABLE_DISPLAY
    int status_ret;
    int chat_ret;
    int notify_ret;
#endif

    if (!s_scan_result_pending) {
        return;
    }

    memcpy(s_scan_flush_text, s_scan_result_text, sizeof(s_scan_flush_text));
    frame_id = s_scan_result_frame_id;
    text_len = s_scan_result_len;
    s_scan_result_pending = false;
    app_log("watch scan result flush: frame=%lu len=%lu",
            (unsigned long)frame_id,
            (unsigned long)text_len);
#if WATCH_AI_ENABLE_DISPLAY
    status_ret = app_display_set_status("SCAN OK");
    chat_ret = app_display_set_chat_message(APP_DISPLAY_ROLE_SYSTEM, s_scan_flush_text);
    notify_ret = app_display_notify(s_scan_flush_text, WATCH_SCAN_RESULT_NOTIFY_MS);
    if (status_ret != APP_OK || chat_ret != APP_OK || notify_ret != APP_OK) {
        app_log("watch scan result display failed: status=%d chat=%d notify=%d",
                status_ret,
                chat_ret,
                notify_ret);
    }
#endif
}

static int watch_scan_stop_services(int *last_ret)
{
#if WATCH_AI_ENABLE_SCAN && WATCH_AI_ENABLE_CAMERA
    int ret = APP_OK;
    int deinit_ret;

    if (app_camera_get_config() != NULL) {
        ret = watch_camera_session_stop();
        if (!watch_scan_ret_ok(ret)) {
            app_log("watch scan camera session stop failed: %d", ret);
            if (last_ret != NULL) {
                *last_ret = ret;
            }
            return ret;
        }
        deinit_ret = app_camera_deinit();
        if (!watch_scan_ret_ok(deinit_ret)) {
            app_log("watch scan camera deinit failed: %d", deinit_ret);
            if (last_ret != NULL) {
                *last_ret = deinit_ret;
            }
            return deinit_ret;
        }
    }
    if (app_scan_is_initialized()) {
        deinit_ret = app_scan_deinit();
        if (!watch_scan_ret_ok(deinit_ret)) {
            app_log("watch scan decoder deinit failed: %d", deinit_ret);
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

int watch_scan_init(void)
{
#if WATCH_AI_ENABLE_SCAN
#if WATCH_AI_ENABLE_LIOT_SCAN_PORT
    app_scan_liot_config_t scan_port_config;
    int ret;

    memset(&scan_port_config, 0, sizeof(scan_port_config));
    scan_port_config.backend = APP_SCAN_LIOT_BACKEND_QY;
    ret = app_scan_liot_setup(&scan_port_config);
    if (ret != APP_OK) {
        app_log("watch init scan LIOT setup failed: %d", ret);
        return ret;
    }
    ret = app_scan_liot_register();
    if (ret != APP_OK) {
        app_log("watch init scan LIOT register failed: %d", ret);
        return ret;
    }
    app_log("watch init scan complete: service init deferred");
    return APP_OK;
#else
    app_log("watch init scan rejected: LIOT scan port disabled");
    return APP_ERR_NOT_SUPPORTED;
#endif
#else
    return APP_OK;
#endif
}

int watch_scan_start(void)
{
#if WATCH_AI_ENABLE_SCAN && WATCH_AI_ENABLE_CAMERA
    app_camera_config_t camera_config;
    app_scan_config_t scan_config;
    watch_camera_session_config_t session_config;
    uint32_t frame_capacity;
    int ret;

    app_log("watch scan start");
    if (watch_session_current() == WATCH_SESSION_SCAN) {
        app_log("watch scan already running");
        return APP_OK;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch scan start rejected: session=%s",
                watch_session_name(watch_session_current()));
        return APP_ERR_BUSY;
    }

    if (app_camera_get_config() != NULL || app_scan_is_initialized()) {
        app_log("watch scan cleanup stale services");
        ret = watch_scan_stop_services(NULL);
        if (!watch_scan_ret_ok(ret)) {
            app_log("watch scan stale cleanup failed: %d", ret);
            return ret;
        }
    }

    memset(&scan_config, 0, sizeof(scan_config));
    scan_config.width = WATCH_CAMERA_PROFILE_WIDTH;
    scan_config.height = WATCH_CAMERA_PROFILE_HEIGHT;
    scan_config.input_format = APP_SCAN_FRAME_FORMAT_GRAY8;
    ret = app_scan_get_frame_buffer_capacity(&scan_config, &frame_capacity);
    if (ret != APP_OK) {
        app_log("watch scan frame capacity query failed: %d", ret);
        return ret;
    }
    if (frame_capacity < WATCH_CAMERA_PROFILE_SCAN_REQUIRED_CAPACITY) {
        app_log("watch scan frame capacity too small: capacity=%lu required=%lu",
                (unsigned long)frame_capacity,
                (unsigned long)WATCH_CAMERA_PROFILE_SCAN_REQUIRED_CAPACITY);
        return APP_ERR_NO_MEMORY;
    }

    ret = watch_camera_profile_make_scan(&camera_config, frame_capacity);
    if (ret != APP_OK) {
        app_log("watch scan camera profile failed: %d", ret);
        return ret;
    }
#if WATCH_AI_ENABLE_LIOT_CAMERA_PORT
    ret = watch_camera_prepare_port(&camera_config);
    if (ret != APP_OK) {
        return ret;
    }
#endif

    ret = app_scan_init(&scan_config);
    if (ret != APP_OK) {
        app_log("watch scan service init failed: %d", ret);
        return ret;
    }

    ret = app_camera_init(&camera_config);
    if (ret != APP_OK) {
        (void)app_scan_deinit();
        app_log("watch scan camera service init failed: %d", ret);
        return ret;
    }
    ret = watch_session_open(WATCH_SESSION_SCAN, "scan", watch_scan_stop);
    if (ret != APP_OK) {
        (void)app_camera_deinit();
        (void)app_scan_deinit();
        app_log("watch scan session open failed: %d", ret);
        return ret;
    }

#if WATCH_AI_ENABLE_DISPLAY
    (void)watch_page_replace(WATCH_PAGE_SCAN_PREVIEW);
    (void)app_display_set_status("SCAN");
    (void)app_display_set_chat_message(APP_DISPLAY_ROLE_SYSTEM, "Scanning");
#endif

    memset(&session_config, 0, sizeof(session_config));
    session_config.mode = WATCH_CAMERA_SESSION_SCAN_PREVIEW;
    session_config.name = "scan";
    ret = watch_camera_session_start(&session_config);
    if (ret != APP_OK) {
        app_event_t event;

        memset(&event, 0, sizeof(event));
        event.id = APP_EV_CAMERA_ERROR;
        event.data.error.code = ret;
        event.data.error.message = "scan camera session start failed";
        (void)app_event_post(&event);
        (void)watch_session_close(WATCH_SESSION_SCAN);
        (void)app_camera_deinit();
        (void)app_scan_deinit();
#if WATCH_AI_ENABLE_DISPLAY
        (void)watch_page_replace(WATCH_PAGE_HOME);
        (void)app_display_set_status("SCAN ERROR");
#endif
        app_log("watch scan camera session start failed: %d", ret);
        return ret;
    }

    watch_scan_post_event(APP_EV_SCAN_START);
    app_log("watch scan started: camera=%ux%u gray frame=%lu capacity=%lu",
            (unsigned int)camera_config.width,
            (unsigned int)camera_config.height,
            (unsigned long)WATCH_CAMERA_PROFILE_SCAN_FRAME_BYTES,
            (unsigned long)camera_config.frame_buffer_capacity);
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_scan_stop(void)
{
#if WATCH_AI_ENABLE_SCAN && WATCH_AI_ENABLE_CAMERA
    int service_ret;
    int ret;

    app_log("watch scan stop");
    watch_scan_clear_pending_result();
    ret = watch_scan_stop_services(&service_ret);
    if (ret != APP_OK) {
        return ret;
    }
    watch_scan_clear_pending_result();
    (void)watch_session_close(WATCH_SESSION_SCAN);
    watch_scan_post_event(APP_EV_SCAN_STOP);
#if WATCH_AI_ENABLE_DISPLAY
    (void)watch_page_replace(WATCH_PAGE_HOME);
    (void)app_display_set_status("IDLE");
#endif
    app_log("watch scan stopped: %d", service_ret);
    return APP_OK;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}

int watch_scan_shutdown(void)
{
#if WATCH_AI_ENABLE_SCAN
    int ret = watch_scan_stop_services(NULL);

    if (watch_session_current() == WATCH_SESSION_SCAN) {
        (void)watch_session_close(WATCH_SESSION_SCAN);
    }
    return ret;
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}
