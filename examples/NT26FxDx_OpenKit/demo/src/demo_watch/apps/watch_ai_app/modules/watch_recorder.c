#include "watch_recorder.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_display_service.h"
#include "app_event.h"
#include "app_osal.h"
#include "app_recorder_port_liot.h"
#include "app_recorder_service.h"
#include "watch_page.h"
#include "watch_session.h"

#define WATCH_RECORDER_TASK_STACK_BYTES (24U * 1024U)
#define WATCH_RECORDER_TASK_PRIORITY 10U
#define WATCH_RECORDER_STOP_TIMEOUT_MS 10000U
#define WATCH_RECORDER_STOP_POLL_MS 10U
#define WATCH_RECORDER_DELETE_SETTLE_MS 20U
#define WATCH_RECORDER_UI_UPDATE_MS 50U
#define WATCH_RECORDER_FILE_SCAN_MAX 9999U
#define WATCH_RECORDER_SAMPLE_RATE_HZ 16000U
#define WATCH_RECORDER_CHANNELS 1U
#define WATCH_RECORDER_BITS_PER_SAMPLE 16U

typedef struct {
    bool configured;
    bool task_started;
    bool recording;
    bool saving;
    volatile bool stop_requested;
    bool exit_requested;
    app_task_t task;
    app_recorder_liot_config_t config;
    app_recorder_file_t current_file;
    uint32_t last_bytes_done;
    uint32_t last_duration_ms;
    uint8_t last_level;
    app_recorder_stop_reason_t last_stop_reason;
    uint32_t last_log_second;
    uint32_t last_ui_duration_ms;
} watch_recorder_ctx_t;

static watch_recorder_ctx_t s_recorder;
static uint8_t *s_recorder_task_stack;
static void *s_recorder_static_task;

static int watch_recorder_stop_recording(void);
static int watch_recorder_close_page(void);

static const char *watch_recorder_stop_reason_name(app_recorder_stop_reason_t reason)
{
    switch (reason) {
    case APP_RECORDER_STOP_REASON_NORMAL:
        return "normal";
    case APP_RECORDER_STOP_REASON_NO_SPACE:
        return "no_space";
    case APP_RECORDER_STOP_REASON_ERROR:
        return "error";
    case APP_RECORDER_STOP_REASON_NONE:
    default:
        return "none";
    }
}

static void watch_recorder_post_event(app_event_id_t id)
{
    app_event_t event;
    int ret;

    memset(&event, 0, sizeof(event));
    event.id = id;
    ret = app_event_post(&event);
    if (ret != APP_OK) {
        app_log("watch recorder event post failed: %s ret=%d",
                app_event_name(id),
                ret);
    }
}

static void watch_recorder_reset_runtime(void)
{
    s_recorder.task = NULL;
    s_recorder.task_started = false;
    s_recorder.recording = false;
    s_recorder.saving = false;
    s_recorder.stop_requested = false;
    s_recorder.exit_requested = false;
    s_recorder.last_bytes_done = 0U;
    s_recorder.last_duration_ms = 0U;
    s_recorder.last_level = 0U;
    s_recorder.last_stop_reason = APP_RECORDER_STOP_REASON_NONE;
    s_recorder.last_log_second = 0U;
    s_recorder.last_ui_duration_ms = 0U;
    memset(&s_recorder.current_file, 0, sizeof(s_recorder.current_file));
}

static void watch_recorder_copy_display_status(app_display_recorder_status_t *dst,
                                               const app_recorder_progress_t *progress)
{
    if (dst == NULL || progress == NULL || progress->file == NULL) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    (void)snprintf(dst->name, sizeof(dst->name), "%s", progress->file->name);
    dst->bytes_done = progress->bytes_done;
    dst->duration_ms = progress->duration_ms;
    dst->level = progress->level;
    dst->recording = progress->recording;
    dst->saving = s_recorder.saving && !progress->done;
    dst->done = progress->done;
    dst->stop_reason = progress->stop_reason;
    dst->sample_rate_hz = progress->file->sample_rate_hz;
    dst->channels = progress->file->channels;
    dst->bits_per_sample = progress->file->bits_per_sample;
}

static void watch_recorder_set_page_status(const char *name,
                                           uint32_t bytes_done,
                                           uint32_t duration_ms,
                                           uint8_t level,
                                           bool recording,
                                           bool saving,
                                           bool done,
                                           app_recorder_stop_reason_t stop_reason)
{
#if WATCH_AI_ENABLE_DISPLAY
    app_display_recorder_status_t status;
    const char *display_name = name;

    memset(&status, 0, sizeof(status));
    if ((display_name == NULL || display_name[0] == '\0') &&
        s_recorder.current_file.name[0] != '\0') {
        display_name = s_recorder.current_file.name;
    }
    if (display_name != NULL) {
        (void)snprintf(status.name, sizeof(status.name), "%s", display_name);
    }
    status.bytes_done = bytes_done;
    status.duration_ms = duration_ms;
    status.level = level;
    status.recording = recording;
    status.saving = saving;
    status.done = done;
    status.stop_reason = stop_reason;
    status.sample_rate_hz = (s_recorder.current_file.sample_rate_hz != 0U) ?
                            s_recorder.current_file.sample_rate_hz :
                            WATCH_RECORDER_SAMPLE_RATE_HZ;
    status.channels = (s_recorder.current_file.channels != 0U) ?
                      s_recorder.current_file.channels :
                      WATCH_RECORDER_CHANNELS;
    status.bits_per_sample = (s_recorder.current_file.bits_per_sample != 0U) ?
                             s_recorder.current_file.bits_per_sample :
                             WATCH_RECORDER_BITS_PER_SAMPLE;
    (void)app_display_set_recorder_status(&status);
#else
    (void)name;
    (void)bytes_done;
    (void)duration_ms;
    (void)level;
    (void)recording;
    (void)saving;
    (void)done;
    (void)stop_reason;
#endif
}

static void watch_recorder_publish_status(const app_recorder_progress_t *progress)
{
    app_display_recorder_status_t status;
    uint32_t second;
    bool should_log;
    bool should_update_ui;

    if (progress == NULL || progress->file == NULL) {
        return;
    }
    if (progress->done) {
        s_recorder.saving = false;
    }

    watch_recorder_copy_display_status(&status, progress);
    s_recorder.last_bytes_done = progress->bytes_done;
    s_recorder.last_duration_ms = progress->duration_ms;
    s_recorder.last_level = progress->level;
    if (progress->stop_reason != APP_RECORDER_STOP_REASON_NONE) {
        s_recorder.last_stop_reason = progress->stop_reason;
    }
    second = progress->duration_ms / 1000U;
    should_log = progress->bytes_done == 0U ||
                 progress->done ||
                 progress->stop_reason != APP_RECORDER_STOP_REASON_NONE ||
                 second != s_recorder.last_log_second;
    s_recorder.last_log_second = second;
    should_update_ui = progress->done ||
                       progress->stop_reason != APP_RECORDER_STOP_REASON_NONE ||
                       progress->bytes_done == 0U ||
                       s_recorder.last_ui_duration_ms == 0U ||
                       progress->duration_ms < s_recorder.last_ui_duration_ms ||
                       (progress->duration_ms - s_recorder.last_ui_duration_ms) >=
                       WATCH_RECORDER_UI_UPDATE_MS;

    if (should_log) {
        app_log("watch recorder progress: file=%s bytes=%lu duration=%lu level=%u rec=%d done=%d reason=%s",
                progress->file->name,
                (unsigned long)progress->bytes_done,
                (unsigned long)progress->duration_ms,
                (unsigned int)progress->level,
                progress->recording ? 1 : 0,
                progress->done ? 1 : 0,
                watch_recorder_stop_reason_name(progress->stop_reason));
    }
#if WATCH_AI_ENABLE_DISPLAY
    if (should_update_ui) {
        int ret = app_display_set_recorder_status(&status);

        if (ret == APP_OK) {
            s_recorder.last_ui_duration_ms = progress->duration_ms;
        } else if (should_log) {
            app_log("watch recorder display update failed: ret=%d duration=%lu",
                    ret,
                    (unsigned long)progress->duration_ms);
        }
    }
#else
    (void)status;
#endif
}

static void watch_recorder_progress_cb(const app_recorder_progress_t *progress, void *user)
{
    (void)user;

    if (progress == NULL || progress->file == NULL) {
        return;
    }
    if (s_recorder.stop_requested && !progress->done) {
        return;
    }

    watch_recorder_publish_status(progress);
}

static int watch_recorder_wait_exit(uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;

    while (s_recorder.task_started) {
        if (waited_ms >= timeout_ms) {
            app_log("watch recorder save timeout: waited=%lu file=%s",
                    (unsigned long)waited_ms,
                    s_recorder.current_file.name);
            return APP_ERR_TIMEOUT;
        }
        app_os_task_delay_ms(WATCH_RECORDER_STOP_POLL_MS);
        waited_ms += WATCH_RECORDER_STOP_POLL_MS;
    }
    return APP_OK;
}

static void watch_recorder_release_task_stack(void)
{
    if (s_recorder_task_stack != NULL) {
        app_log("watch recorder stack release: ptr=%p",
                (void *)s_recorder_task_stack);
        app_os_free(s_recorder_task_stack);
        s_recorder_task_stack = NULL;
    }
    s_recorder_static_task = NULL;
}

static int watch_recorder_prepare_task_stack(void)
{
    if (s_recorder_task_stack != NULL) {
        return APP_OK;
    }

    s_recorder_task_stack = (uint8_t *)app_os_malloc(WATCH_RECORDER_TASK_STACK_BYTES);
    if (s_recorder_task_stack == NULL) {
        app_log("watch recorder stack alloc failed: bytes=%lu",
                (unsigned long)WATCH_RECORDER_TASK_STACK_BYTES);
        return APP_ERR_NO_MEMORY;
    }

    app_log("watch recorder stack ready: bytes=%lu ptr=%p",
            (unsigned long)WATCH_RECORDER_TASK_STACK_BYTES,
            (void *)s_recorder_task_stack);
    return APP_OK;
}

static int watch_recorder_path_exists(const char *path, bool *exists)
{
    int ret;

    if (path == NULL || exists == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ret = app_recorder_file_exists(path, exists);
    if (ret != APP_OK) {
        app_log("watch recorder file check failed: %s ret=%d", path, ret);
    }
    return ret;
}

static int watch_recorder_make_file(app_recorder_file_t *file)
{
    uint32_t index;
    char path[APP_RECORDER_FILE_PATH_MAX];

    if (file == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    for (index = 1U; index <= WATCH_RECORDER_FILE_SCAN_MAX; index++) {
        int ret = snprintf(path, sizeof(path), "/rec_%04lu.wav", (unsigned long)index);
        bool exists = false;

        if (ret < 0 || (uint32_t)ret >= sizeof(path)) {
            return APP_ERR_INVALID_ARG;
        }
        ret = watch_recorder_path_exists(path, &exists);
        if (ret != APP_OK) {
            return ret;
        }
        if (exists) {
            continue;
        }
        memset(file, 0, sizeof(*file));
        (void)snprintf(file->name, sizeof(file->name), "rec_%04lu.wav", (unsigned long)index);
        (void)snprintf(file->path, sizeof(file->path), "%s", path);
        file->sample_rate_hz = WATCH_RECORDER_SAMPLE_RATE_HZ;
        file->channels = WATCH_RECORDER_CHANNELS;
        file->bits_per_sample = WATCH_RECORDER_BITS_PER_SAMPLE;
        file->chunk_bytes = s_recorder.config.chunk_bytes;
        app_log("watch recorder file ready: %s", file->path);
        return APP_OK;
    }

    app_log("watch recorder file name exhausted");
    return APP_ERR_NO_MEMORY;
}

static void watch_recorder_task(void *arg)
{
    int ret;

    (void)arg;
    app_os_log_current_task("watch recorder task start");
    app_log("watch recorder task record: file=%s path=%s",
            s_recorder.current_file.name,
            s_recorder.current_file.path);

    s_recorder.recording = true;
    ret = app_recorder_record_file(&s_recorder.current_file,
                                   &s_recorder.stop_requested,
                                   watch_recorder_progress_cb,
                                   NULL);
    s_recorder.recording = false;
    s_recorder.saving = false;
    if (ret != APP_OK &&
        s_recorder.last_stop_reason == APP_RECORDER_STOP_REASON_NONE) {
        s_recorder.last_stop_reason = APP_RECORDER_STOP_REASON_ERROR;
    }
    if (ret == APP_OK && s_recorder.stop_requested) {
        app_log("watch recorder task stopped: file=%s reason=%s",
                s_recorder.current_file.name,
                watch_recorder_stop_reason_name(s_recorder.last_stop_reason));
    } else if (ret == APP_OK) {
        app_log("watch recorder task done: file=%s bytes=%lu reason=%s",
                s_recorder.current_file.name,
                (unsigned long)s_recorder.last_bytes_done,
                watch_recorder_stop_reason_name(s_recorder.last_stop_reason));
    } else {
        app_log("watch recorder task failed: file=%s ret=%d bytes=%lu reason=%s",
                s_recorder.current_file.name,
                ret,
                (unsigned long)s_recorder.last_bytes_done,
                watch_recorder_stop_reason_name(s_recorder.last_stop_reason));
    }

    s_recorder.task_started = false;
    s_recorder.task = NULL;
    watch_recorder_post_event(APP_EV_RECORDER_DONE);
    app_os_task_delete(NULL);
    while (1) {
        app_os_task_delay_ms(1000U);
    }
}

static int watch_recorder_begin_recording(void)
{
    app_display_recorder_status_t status;
    int ret;

    if (s_recorder.task_started || s_recorder.recording || s_recorder.saving) {
        app_log("watch recorder begin ignored: task=%d rec=%d saving=%d",
                s_recorder.task_started ? 1 : 0,
                s_recorder.recording ? 1 : 0,
                s_recorder.saving ? 1 : 0);
        return APP_ERR_BUSY;
    }

    ret = app_recorder_init();
    if (ret != APP_OK) {
        app_log("watch recorder service init failed: %d", ret);
        return ret;
    }

    ret = watch_recorder_make_file(&s_recorder.current_file);
    if (ret != APP_OK) {
        app_log("watch recorder file alloc failed: %d", ret);
        (void)app_recorder_deinit();
        return ret;
    }

    memset(&status, 0, sizeof(status));
    (void)snprintf(status.name, sizeof(status.name), "%s", s_recorder.current_file.name);
    status.sample_rate_hz = s_recorder.current_file.sample_rate_hz;
    status.channels = s_recorder.current_file.channels;
    status.bits_per_sample = s_recorder.current_file.bits_per_sample;
    status.recording = true;
    (void)app_display_set_status("REC");
    (void)app_display_set_recorder_status(&status);

    ret = watch_recorder_prepare_task_stack();
    if (ret != APP_OK) {
        (void)app_recorder_deinit();
        return ret;
    }

    s_recorder.stop_requested = false;
    s_recorder.saving = false;
    s_recorder.task_started = true;
    ret = app_os_task_create_static(&s_recorder.task,
                                    "watch_recorder",
                                    watch_recorder_task,
                                    NULL,
                                    WATCH_RECORDER_TASK_STACK_BYTES,
                                    WATCH_RECORDER_TASK_PRIORITY,
                                    s_recorder_task_stack,
                                    &s_recorder_static_task);
    if (ret != APP_OK) {
        s_recorder.task_started = false;
        s_recorder.task = NULL;
        watch_recorder_release_task_stack();
        (void)app_recorder_deinit();
        app_log("watch recorder task create failed: %d", ret);
        return ret;
    }

    app_log("watch recorder task create ok: file=%s", s_recorder.current_file.path);
    return APP_OK;
}

int watch_recorder_init(void)
{
    int ret;

    if (s_recorder.configured) {
        app_log("watch init recorder already configured");
        return APP_OK;
    }

    app_log("watch init recorder");
    ret = app_recorder_liot_get_default_config(&s_recorder.config);
    if (ret != APP_OK) {
        app_log("watch init recorder default config failed: %d", ret);
        return ret;
    }
    ret = app_recorder_liot_setup(&s_recorder.config);
    if (ret != APP_OK) {
        app_log("watch init recorder setup failed: %d", ret);
        return ret;
    }
    ret = app_recorder_liot_register();
    if (ret != APP_OK) {
        app_log("watch init recorder register failed: %d", ret);
        return ret;
    }

    s_recorder.configured = true;
    app_log("watch init recorder complete");
    return APP_OK;
}

int watch_recorder_start(void)
{
    int ret;

    app_log("watch recorder page enter");
    if (!s_recorder.configured) {
        return APP_ERR_NOT_READY;
    }
    if (watch_session_current() == WATCH_SESSION_RECORDER) {
        app_log("watch recorder already open");
        return APP_OK;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch recorder start rejected: session=%s",
                watch_session_name(watch_session_current()));
        return APP_ERR_BUSY;
    }

    ret = watch_session_open(WATCH_SESSION_RECORDER, "recorder", watch_recorder_stop);
    if (ret != APP_OK) {
        app_log("watch recorder session open failed: %d", ret);
        return ret;
    }

    watch_recorder_reset_runtime();
    watch_recorder_set_page_status("",
                                   0U,
                                   0U,
                                   0U,
                                   false,
                                   false,
                                   false,
                                   APP_RECORDER_STOP_REASON_NONE);
    (void)app_display_set_status("REC");

    ret = watch_page_replace(WATCH_PAGE_RECORDER);
    if (ret != APP_OK) {
        app_log("watch recorder page switch failed: %d", ret);
        (void)watch_session_close(WATCH_SESSION_RECORDER);
        watch_recorder_reset_runtime();
        (void)app_display_set_status("IDLE");
        return ret;
    }

    app_log("watch recorder page ready");
    return APP_OK;
}

int watch_recorder_toggle(void)
{
    watch_page_t current_page = watch_page_current();
    int ret;

    app_log("watch recorder toggle: page=%s task=%d recording=%d saving=%d",
            watch_page_name(current_page),
            s_recorder.task_started ? 1 : 0,
            s_recorder.recording ? 1 : 0,
            s_recorder.saving ? 1 : 0);

    if (current_page != WATCH_PAGE_RECORDER ||
        watch_session_current() != WATCH_SESSION_RECORDER) {
        app_log("watch recorder toggle rejected: page=%s session=%s",
                watch_page_name(current_page),
                watch_session_name(watch_session_current()));
        return APP_ERR_NOT_READY;
    }
    if (s_recorder.saving) {
        app_log("watch recorder toggle ignored: saving");
        return APP_OK;
    }
    if (s_recorder.task_started || s_recorder.recording) {
        app_log("watch recorder stop pressed");
        return watch_recorder_stop_recording();
    }

    app_log("watch recorder start pressed");
    ret = watch_recorder_begin_recording();
    if (ret != APP_OK) {
        watch_recorder_set_page_status("",
                                       0U,
                                       0U,
                                       0U,
                                       false,
                                       false,
                                       false,
                                       APP_RECORDER_STOP_REASON_NONE);
    }
    return ret;
}

static int watch_recorder_stop_recording(void)
{
    int ret = APP_OK;

    if (s_recorder.saving) {
        app_log("watch recorder stop recording ignored: saving");
        return APP_OK;
    }
    if (!s_recorder.task_started && !s_recorder.recording) {
        app_log("watch recorder stop recording ignored: no task");
        return APP_OK;
    }

    s_recorder.saving = true;
    app_log("watch recorder saving: file=%s bytes=%lu timeout=%lu",
            s_recorder.current_file.name,
            (unsigned long)s_recorder.last_bytes_done,
            (unsigned long)WATCH_RECORDER_STOP_TIMEOUT_MS);
    watch_recorder_set_page_status(s_recorder.current_file.name,
                                   s_recorder.last_bytes_done,
                                   s_recorder.last_duration_ms,
                                   s_recorder.last_level,
                                   false,
                                   true,
                                   false,
                                   APP_RECORDER_STOP_REASON_NONE);
    s_recorder.stop_requested = true;
    (void)app_recorder_stop();
    ret = watch_recorder_wait_exit(WATCH_RECORDER_STOP_TIMEOUT_MS);
    if (ret != APP_OK) {
        app_log("watch recorder save timeout: ret=%d", ret);
        return ret;
    }
    app_os_task_delay_ms(WATCH_RECORDER_DELETE_SETTLE_MS);

    if (app_recorder_is_initialized()) {
        ret = app_recorder_deinit();
        if (ret != APP_OK) {
            app_log("watch recorder deinit failed: %d", ret);
        }
    }
    watch_recorder_release_task_stack();

    app_log("watch recorder save finished");
    return APP_OK;
}

static int watch_recorder_close_page(void)
{
    int ret = APP_OK;

    if (app_recorder_is_initialized()) {
        ret = app_recorder_deinit();
        if (ret != APP_OK) {
            app_log("watch recorder deinit failed: %d", ret);
        }
    }

    (void)watch_session_close(WATCH_SESSION_RECORDER);
    (void)watch_page_replace(WATCH_PAGE_HOME);
    (void)app_display_set_status("IDLE");
    if (s_recorder.last_bytes_done > 0U) {
        if (s_recorder.last_stop_reason == APP_RECORDER_STOP_REASON_NO_SPACE) {
            (void)app_display_notify("Storage full", 1800U);
        } else if (s_recorder.last_stop_reason == APP_RECORDER_STOP_REASON_ERROR) {
            (void)app_display_notify("Save failed", 1800U);
        } else {
            (void)app_display_notify(s_recorder.current_file.name, 1800U);
        }
    }
    watch_recorder_release_task_stack();
    watch_recorder_reset_runtime();
    app_log("watch recorder back to home");
    return ret;
}

int watch_recorder_stop(void)
{
    watch_page_t current_page = watch_page_current();
    bool session_active = watch_session_current() == WATCH_SESSION_RECORDER;
    bool should_finalize = current_page == WATCH_PAGE_RECORDER ||
                           s_recorder.task_started ||
                           s_recorder.recording ||
                           s_recorder.saving ||
                           app_recorder_is_initialized() ||
                           session_active;
    int ret = APP_OK;

    app_log("watch recorder stop: page=%s task=%d recording=%d saving=%d",
            watch_page_name(current_page),
            s_recorder.task_started ? 1 : 0,
            s_recorder.recording ? 1 : 0,
            s_recorder.saving ? 1 : 0);

    if (!should_finalize) {
        app_log("watch recorder stop ignored: idle");
        return APP_OK;
    }

    if (current_page == WATCH_PAGE_RECORDER || s_recorder.task_started ||
        s_recorder.recording || s_recorder.saving || app_recorder_is_initialized()) {
        if (s_recorder.exit_requested) {
            app_log("watch recorder stop ignored: exit pending");
            return APP_OK;
        }
        s_recorder.exit_requested = true;
        ret = watch_recorder_stop_recording();
        if (ret != APP_OK) {
            s_recorder.exit_requested = false;
            return ret;
        }
        ret = watch_recorder_close_page();
        s_recorder.exit_requested = false;
        return ret;
    }

    app_log("watch recorder stop ignored: idle");
    return APP_OK;
}
