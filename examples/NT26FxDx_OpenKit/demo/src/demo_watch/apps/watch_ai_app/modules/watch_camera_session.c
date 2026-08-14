#include "watch_camera_session.h"

#include <string.h>

#include "app_osal.h"
#include "watch_camera.h"
#include "watch_scan.h"

#define WATCH_CAMERA_SESSION_TASK_STACK_BYTES (350U * 1024U)

#define WATCH_CAMERA_SESSION_TASK_PRIORITY 12U
#define WATCH_CAMERA_SESSION_STOP_TIMEOUT_MS 2500U
#define WATCH_CAMERA_SESSION_STOP_POLL_MS 10U
#define WATCH_CAMERA_SESSION_DELETE_SETTLE_MS 20U
#define WATCH_CAMERA_SESSION_CAPTURE_ERROR_DELAY_MS 300U


typedef struct {
    watch_camera_session_config_t config;
    app_task_t task;
    uint8_t *frame_buf;
    uint32_t frame_bytes;
    uint32_t frame_capacity;
    uint32_t frame_id;
    app_camera_config_t camera_config;
    volatile bool running;
    volatile bool task_started;
} watch_camera_session_ctx_t;

static watch_camera_session_ctx_t s_camera_session;
static uint8_t *s_camera_session_task_stack;
static void *s_camera_session_static_task;

const char *watch_camera_session_mode_name(watch_camera_session_mode_t mode)
{
    switch (mode) {
    case WATCH_CAMERA_SESSION_PREVIEW:
        return "preview";
    case WATCH_CAMERA_SESSION_SCAN_PREVIEW:
        return "scan_preview";
    default:
        return "unknown";
    }
}

bool watch_camera_session_is_running(void)
{
    return s_camera_session.task_started;
}

static bool watch_camera_session_mode_valid(watch_camera_session_mode_t mode)
{
    return mode == WATCH_CAMERA_SESSION_PREVIEW ||
           mode == WATCH_CAMERA_SESSION_SCAN_PREVIEW;
}

static bool watch_camera_session_should_log_frame(uint32_t frame_id)
{
    return frame_id <= 3U || (frame_id % 30U) == 0U;
}

static void watch_camera_session_make_frame(app_camera_frame_t *frame,
                                            uint32_t frame_id)
{
    memset(frame, 0, sizeof(*frame));
    frame->frame_id = frame_id;
    frame->data = s_camera_session.frame_buf;
    frame->len = s_camera_session.frame_bytes;
    frame->buffer_capacity = s_camera_session.frame_capacity;
    frame->width = s_camera_session.camera_config.width;
    frame->height = s_camera_session.camera_config.height;
    frame->output_format = s_camera_session.camera_config.output_format;
}

static int watch_camera_session_process_frame(const app_camera_frame_t *frame)
{
    switch (s_camera_session.config.mode) {
    case WATCH_CAMERA_SESSION_PREVIEW:
        return watch_camera_present_frame(frame);
    case WATCH_CAMERA_SESSION_SCAN_PREVIEW:
        return watch_scan_process_camera_frame(frame);
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}

static int watch_camera_session_prepare_task_stack(void)
{
    if (s_camera_session_task_stack != NULL) {
        return APP_OK;
    }

    s_camera_session_task_stack =
        (uint8_t *)app_os_malloc(WATCH_CAMERA_SESSION_TASK_STACK_BYTES);
    if (s_camera_session_task_stack == NULL) {
        app_log("watch camera session stack alloc failed: bytes=%lu",
                (unsigned long)WATCH_CAMERA_SESSION_TASK_STACK_BYTES);
        return APP_ERR_NO_MEMORY;
    }

    app_log("watch camera session stack ready: bytes=%lu ptr=%p",
            (unsigned long)WATCH_CAMERA_SESSION_TASK_STACK_BYTES,
            (void *)s_camera_session_task_stack);
    return APP_OK;
}

static void watch_camera_session_release_task_stack(void)
{
    if (s_camera_session_task_stack != NULL) {
        app_log("watch camera session stack release: ptr=%p",
                (void *)s_camera_session_task_stack);
        app_os_free(s_camera_session_task_stack);
        s_camera_session_task_stack = NULL;
    }
    s_camera_session_static_task = NULL;
}

static void watch_camera_session_task(void *arg)
{
    app_camera_frame_t frame;
    int ret;

    (void)arg;
    app_os_log_current_task("watch camera session task start");
    while (s_camera_session.running) {
        ret = app_camera_capture_frame(s_camera_session.frame_buf,
                                       s_camera_session.frame_capacity,
                                       s_camera_session.camera_config.capture_timeout_ms);
        s_camera_session.frame_id++;
        if (!s_camera_session.running) {
            break;
        }
        if (ret != APP_OK) {
            if (watch_camera_session_should_log_frame(s_camera_session.frame_id)) {
                app_log("watch camera session capture failed: mode=%s frame=%lu ret=%d",
                        watch_camera_session_mode_name(s_camera_session.config.mode),
                        (unsigned long)s_camera_session.frame_id,
                        ret);
            }
            app_os_task_delay_ms(WATCH_CAMERA_SESSION_CAPTURE_ERROR_DELAY_MS);
            continue;
        }

        watch_camera_session_make_frame(&frame, s_camera_session.frame_id);
        ret = watch_camera_session_process_frame(&frame);
        if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED &&
            watch_camera_session_should_log_frame(s_camera_session.frame_id)) {
            app_log("watch camera session frame failed: mode=%s frame=%lu ret=%d",
                    watch_camera_session_mode_name(s_camera_session.config.mode),
                    (unsigned long)s_camera_session.frame_id,
                    ret);
        }
        if (s_camera_session.camera_config.capture_period_ms != 0U) {
            app_os_task_delay_ms(s_camera_session.camera_config.capture_period_ms);
        }
    }

    s_camera_session.running = false;
    s_camera_session.task = NULL;
    s_camera_session.task_started = false;
    app_log("watch camera session task exit: mode=%s frame=%lu",
            watch_camera_session_mode_name(s_camera_session.config.mode),
            (unsigned long)s_camera_session.frame_id);
    app_os_task_delete(NULL);
    while (1) {
        app_os_task_delay_ms(1000U);
    }
}

static int watch_camera_session_wait_exit(uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;

    while (s_camera_session.task_started) {
        if (waited_ms >= timeout_ms) {
            app_log("watch camera session stop timeout: mode=%s waited=%lu frame=%lu",
                    watch_camera_session_mode_name(s_camera_session.config.mode),
                    (unsigned long)waited_ms,
                    (unsigned long)s_camera_session.frame_id);
            return APP_ERR_TIMEOUT;
        }
        app_os_task_delay_ms(WATCH_CAMERA_SESSION_STOP_POLL_MS);
        waited_ms += WATCH_CAMERA_SESSION_STOP_POLL_MS;
    }
    return APP_OK;
}

int watch_camera_session_start(const watch_camera_session_config_t *config)
{
    const app_camera_config_t *camera_config;
    int ret;

    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!watch_camera_session_mode_valid(config->mode)) {
        app_log("watch camera session start rejected: mode=%d",
                (int)config->mode);
        return APP_ERR_INVALID_ARG;
    }
    if (s_camera_session.task_started) {
        app_log("watch camera session already running: %s",
                watch_camera_session_mode_name(s_camera_session.config.mode));
        return APP_ERR_BUSY;
    }

    camera_config = app_camera_get_config();
    if (camera_config == NULL) {
        app_log("watch camera session start rejected: camera not ready");
        return APP_ERR_NOT_READY;
    }

    memset(&s_camera_session, 0, sizeof(s_camera_session));
    s_camera_session.config = *config;
    s_camera_session.camera_config = *camera_config;
    ret = app_camera_get_frame_buffer(&s_camera_session.frame_buf,
                                      &s_camera_session.frame_bytes,
                                      &s_camera_session.frame_capacity);
    if (ret != APP_OK) {
        app_log("watch camera session frame buffer failed: mode=%s ret=%d",
                watch_camera_session_mode_name(config->mode),
                ret);
        return ret;
    }
    if (s_camera_session.frame_buf == NULL ||
        s_camera_session.frame_bytes == 0U ||
        s_camera_session.frame_capacity < s_camera_session.frame_bytes) {
        app_log("watch camera session frame buffer invalid: mode=%s buf=%p bytes=%lu capacity=%lu",
                watch_camera_session_mode_name(config->mode),
                (void *)s_camera_session.frame_buf,
                (unsigned long)s_camera_session.frame_bytes,
                (unsigned long)s_camera_session.frame_capacity);
        return APP_ERR_INVALID_ARG;
    }

    s_camera_session.running = true;
    s_camera_session.task_started = true;
    ret = watch_camera_session_prepare_task_stack();
    if (ret != APP_OK) {
        s_camera_session.running = false;
        s_camera_session.task_started = false;
        s_camera_session.task = NULL;
        return ret;
    }

    ret = app_os_task_create_static(&s_camera_session.task,
                                    "camera_session",
                                    watch_camera_session_task,
                                    NULL,
                                    WATCH_CAMERA_SESSION_TASK_STACK_BYTES,
                                    WATCH_CAMERA_SESSION_TASK_PRIORITY,
                                    s_camera_session_task_stack,
                                    &s_camera_session_static_task);
    if (ret != APP_OK) {
        s_camera_session.running = false;
        s_camera_session.task_started = false;
        s_camera_session.task = NULL;
        watch_camera_session_release_task_stack();
        app_log("watch camera session task create failed: mode=%s ret=%d",
                watch_camera_session_mode_name(config->mode),
                ret);
        return ret;
    }

    app_log("watch camera session start: mode=%s %ux%u %s bytes=%lu capacity=%lu",
            watch_camera_session_mode_name(config->mode),
            (unsigned int)s_camera_session.camera_config.width,
            (unsigned int)s_camera_session.camera_config.height,
            app_camera_output_name(s_camera_session.camera_config.output_format),
            (unsigned long)s_camera_session.frame_bytes,
            (unsigned long)s_camera_session.frame_capacity);
    return APP_OK;
}

int watch_camera_session_stop(void)
{
    int ret;

    if (!s_camera_session.task_started) {
        s_camera_session.running = false;
        s_camera_session.task = NULL;
        s_camera_session.frame_buf = NULL;
        s_camera_session.frame_bytes = 0U;
        s_camera_session.frame_capacity = 0U;
        watch_camera_session_release_task_stack();
        return APP_OK;
    }

    app_log("watch camera session stop: mode=%s",
            watch_camera_session_mode_name(s_camera_session.config.mode));
    s_camera_session.running = false;
    ret = watch_camera_session_wait_exit(WATCH_CAMERA_SESSION_STOP_TIMEOUT_MS);
    if (ret != APP_OK) {
        return ret;
    }
    app_os_task_delay_ms(WATCH_CAMERA_SESSION_DELETE_SETTLE_MS);
    s_camera_session.task = NULL;
    s_camera_session.frame_buf = NULL;
    s_camera_session.frame_bytes = 0U;
    s_camera_session.frame_capacity = 0U;
    watch_camera_session_release_task_stack();
    return APP_OK;
}
