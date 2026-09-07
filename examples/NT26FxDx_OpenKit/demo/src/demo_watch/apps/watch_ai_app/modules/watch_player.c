#include "watch_player.h"

#include "app_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_display_service.h"
#include "app_osal.h"
#include "app_player_port_liot.h"
#include "app_player_service.h"
#include "watch_page.h"
#include "watch_session.h"

#define WATCH_PLAYER_TASK_STACK_BYTES (32U * 1024U)
#define WATCH_PLAYER_TASK_PRIORITY 12U
#define WATCH_PLAYER_STOP_TIMEOUT_MS 3000U
#define WATCH_PLAYER_STOP_POLL_MS 10U
#define WATCH_PLAYER_PROGRESS_STEP 5U
#define WATCH_PLAYER_TTS_NAME "TTS"
#define WATCH_PLAYER_TTS_PATH_PREFIX "tts:"
#define WATCH_PLAYER_TTS_TEXT "利尔达公司欢迎您"

typedef struct {
    bool configured;
    bool boot_tts_started;
    bool task_started;
    bool playing;
    volatile bool stop_requested;
    uint32_t file_count;
    uint32_t selected_index;
    uint32_t last_report_percent;
    app_task_t task;
    app_player_file_t *files;
    app_player_file_t current_file;
} watch_player_ctx_t;

static watch_player_ctx_t s_player;

static const char *watch_player_type_name(app_player_file_type_t type)
{
    return app_player_file_type_name(type);
}

static app_display_player_file_type_t watch_player_display_type(app_player_file_type_t type)
{
    switch (type) {
    case APP_PLAYER_FILE_WAV:
        return APP_DISPLAY_PLAYER_FILE_WAV;
    case APP_PLAYER_FILE_TTS:
        return APP_DISPLAY_PLAYER_FILE_TTS;
    case APP_PLAYER_FILE_MP3:
    default:
        return APP_DISPLAY_PLAYER_FILE_MP3;
    }
}

static void watch_player_copy_display_file(app_display_player_file_t *dst,
                                           const app_player_file_t *src)
{
    if (dst == NULL || src == NULL) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    (void)snprintf(dst->name, sizeof(dst->name), "%s", src->name);
    dst->size_bytes = src->size_bytes;
    dst->type = watch_player_display_type(src->type);
}

static void watch_player_copy_status(app_display_player_status_t *dst,
                                     const app_player_file_t *file,
                                     uint32_t bytes_done,
                                     uint32_t size_bytes,
                                     bool playing,
                                     bool done)
{
    if (dst == NULL || file == NULL) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    (void)snprintf(dst->name, sizeof(dst->name), "%s", file->name);
    if (file->type == APP_PLAYER_FILE_TTS) {
        dst->size_bytes = 0U;
        dst->bytes_done = 0U;
        dst->percent = done ? 100U : (playing ? 1U : 0U);
    } else {
        dst->size_bytes = size_bytes;
        dst->bytes_done = bytes_done;
        dst->percent = (size_bytes == 0U) ? 0U :
                       ((bytes_done >= size_bytes) ? 100U :
                        (uint8_t)((bytes_done * 100U) / size_bytes));
    }
    dst->playing = playing;
    dst->done = done;
    dst->type = watch_player_display_type(file->type);
}

static bool watch_player_should_log_percent(uint32_t percent, bool done)
{
    if (done || percent == 0U || percent == 100U) {
        return true;
    }
    return (percent % 10U) == 0U;
}

static void watch_player_publish_status(uint32_t bytes_done,
                                        uint32_t size_bytes,
                                        bool playing,
                                        bool done)
{
    app_display_player_status_t status;
    bool is_tts = s_player.current_file.type == APP_PLAYER_FILE_TTS;

    watch_player_copy_status(&status,
                             &s_player.current_file,
                             bytes_done,
                             size_bytes,
                             playing,
                             done);
    if (!is_tts) {
        if (status.percent < s_player.last_report_percent &&
            !done &&
            status.percent != 0U) {
            return;
        }
        if (!watch_player_should_log_percent(status.percent, done)) {
            if (status.percent <= s_player.last_report_percent) {
                return;
            }
            if ((status.percent - s_player.last_report_percent) < WATCH_PLAYER_PROGRESS_STEP) {
                return;
            }
        }
    } else if (!done && status.percent == s_player.last_report_percent) {
        return;
    }

    s_player.last_report_percent = status.percent;
    app_log("watch player progress: file=%s type=%s %u%% bytes=%lu/%lu playing=%d done=%d",
            s_player.current_file.name,
            watch_player_type_name(s_player.current_file.type),
            (unsigned int)status.percent,
            (unsigned long)bytes_done,
            (unsigned long)size_bytes,
            playing ? 1 : 0,
            done ? 1 : 0);
#if WATCH_AI_ENABLE_DISPLAY
    (void)app_display_set_player_status(&status);
#else
    (void)status;
#endif
}

static void watch_player_progress_cb(const app_player_progress_t *progress, void *user)
{
    (void)user;

    if (progress == NULL || progress->file == NULL) {
        return;
    }
    if (s_player.stop_requested) {
        return;
    }

    watch_player_publish_status(progress->bytes_done,
                                progress->total_bytes,
                                progress->playing,
                                progress->done);
}

static int watch_player_wait_exit(uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;

    while (s_player.task_started) {
        if (waited_ms >= timeout_ms) {
            app_log("watch player stop timeout: waited=%lu file=%s",
                    (unsigned long)waited_ms,
                    s_player.current_file.name);
            return APP_ERR_TIMEOUT;
        }
        app_os_task_delay_ms(WATCH_PLAYER_STOP_POLL_MS);
        waited_ms += WATCH_PLAYER_STOP_POLL_MS;
    }
    return APP_OK;
}

static void watch_player_release_files(void)
{
    if (s_player.files != NULL) {
        app_log("watch player files release: count=%lu ptr=%p",
                (unsigned long)s_player.file_count,
                (void *)s_player.files);
        app_os_free(s_player.files);
        s_player.files = NULL;
    }
    s_player.file_count = 0U;
    s_player.selected_index = 0U;
    s_player.last_report_percent = 0U;
    memset(&s_player.current_file, 0, sizeof(s_player.current_file));
}

static void watch_player_clear_display(void)
{
#if WATCH_AI_ENABLE_DISPLAY
    app_display_player_status_t status;

    memset(&status, 0, sizeof(status));
    (void)app_display_set_player_files(NULL, 0U);
    (void)app_display_set_player_status(&status);
#endif
}

static int watch_player_prepare_tts_file(app_player_file_t *file,
                                         const char *name,
                                         const char *text)
{
    int ret;

    if (file == NULL || name == NULL || text == NULL || text[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    memset(file, 0, sizeof(*file));
    (void)snprintf(file->name, sizeof(file->name), "%s", name);
    ret = snprintf(file->path,
                   sizeof(file->path),
                   "%s%s",
                   WATCH_PLAYER_TTS_PATH_PREFIX,
                   text);
    if (ret < 0 || (uint32_t)ret >= sizeof(file->path)) {
        return APP_ERR_INVALID_ARG;
    }
    file->type = APP_PLAYER_FILE_TTS;
    file->size_bytes = 0U;
    return APP_OK;
}

static int watch_player_prepend_tts_item(void)
{
    app_player_file_t *file;
    int ret;

    if (s_player.files == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_player.file_count >= APP_DISPLAY_PLAYER_FILE_MAX) {
        app_log("watch player tts item skipped: list full");
        return APP_ERR_NO_MEMORY;
    }

    if (s_player.file_count > 0U) {
        memmove(&s_player.files[1],
                &s_player.files[0],
                s_player.file_count * sizeof(s_player.files[0]));
    }

    file = &s_player.files[0];
    ret = watch_player_prepare_tts_file(file,
                                        WATCH_PLAYER_TTS_NAME,
                                        WATCH_PLAYER_TTS_TEXT);
    if (ret != APP_OK) {
        return ret;
    }
    s_player.file_count++;
    app_log("watch player tts item prepended: index=0");
    return APP_OK;
}

static uint32_t watch_player_find_tts_index(void)
{
    uint32_t i;

    for (i = 0U; i < s_player.file_count; i++) {
        if (s_player.files[i].type == APP_PLAYER_FILE_TTS) {
            return i;
        }
    }
    return APP_DISPLAY_PLAYER_FILE_MAX;
}

static void watch_player_reset_runtime(void)
{
    s_player.task = NULL;
    s_player.task_started = false;
    s_player.playing = false;
    s_player.stop_requested = false;
    s_player.last_report_percent = 0U;
}

static void watch_player_boot_tts_task(void *arg)
{
    int ret;

    (void)arg;
    s_player.playing = true;
    app_os_log_current_task("watch boot tts task start");
    app_log("watch boot tts task play: text=%s", WATCH_PLAYER_TTS_TEXT);

    ret = app_player_play_file(&s_player.current_file,
                               &s_player.stop_requested,
                               NULL,
                               NULL);
    if (ret == APP_OK && !s_player.stop_requested) {
        app_log("watch boot tts done");
    } else if (s_player.stop_requested) {
        app_log("watch boot tts stopped: ret=%d", ret);
    } else {
        app_log("watch boot tts failed: ret=%d", ret);
    }

    s_player.playing = false;
    if (app_player_is_initialized()) {
        int deinit_ret = app_player_deinit();

        if (deinit_ret != APP_OK) {
            app_log("watch boot tts player deinit failed: %d", deinit_ret);
        }
    }

#if WATCH_AI_ENABLE_DISPLAY
    if (watch_session_current() == WATCH_SESSION_NONE) {
        (void)app_display_set_status("IDLE");
    }
#endif
    memset(&s_player.current_file, 0, sizeof(s_player.current_file));
    s_player.stop_requested = false;
    s_player.task_started = false;
    s_player.task = NULL;
    app_os_task_delete(NULL);
    while (1) {
        app_os_task_delay_ms(1000U);
    }
}

static void watch_player_task(void *arg)
{
    int ret;

    (void)arg;
    s_player.playing = true;
    app_os_log_current_task("watch player task start");
    app_log("watch player task play: file=%s type=%s size=%lu",
            s_player.current_file.name,
            watch_player_type_name(s_player.current_file.type),
            (unsigned long)s_player.current_file.size_bytes);

    ret = app_player_play_file(&s_player.current_file,
                               &s_player.stop_requested,
                               watch_player_progress_cb,
                               NULL);
    if (ret == APP_OK && !s_player.stop_requested) {
#if WATCH_AI_ENABLE_DISPLAY
        (void)app_display_set_status("PLAYER");
#endif
        app_log("watch player task done: file=%s", s_player.current_file.name);
    } else if (s_player.stop_requested) {
        app_log("watch player task stopped: file=%s ret=%d",
                s_player.current_file.name,
                ret);
    } else {
#if WATCH_AI_ENABLE_DISPLAY
        (void)app_display_set_status("PLAYER ERR");
#endif
        app_log("watch player task failed: file=%s ret=%d",
                s_player.current_file.name,
                ret);
    }

    s_player.playing = false;
    s_player.task_started = false;
    s_player.task = NULL;
    app_os_task_delete(NULL);
    while (1) {
        app_os_task_delay_ms(1000U);
    }
}

int watch_player_init(void)
{
    app_player_liot_config_t config;
    int ret;

    if (s_player.configured) {
        app_log("watch init player already configured");
        return APP_OK;
    }

    app_log("watch init player");
    ret = app_player_liot_get_default_config(&config);
    if (ret != APP_OK) {
        app_log("watch init player default config failed: %d", ret);
        return ret;
    }
    ret = app_player_liot_setup(&config);
    if (ret != APP_OK) {
        app_log("watch init player setup failed: %d", ret);
        return ret;
    }
    ret = app_player_liot_register();
    if (ret != APP_OK) {
        app_log("watch init player register failed: %d", ret);
        return ret;
    }

    s_player.configured = true;
    app_log("watch init player complete");
    return APP_OK;
}

int watch_player_start_boot_tts(void)
{
    int ret;

    app_log("watch boot tts start");
    if (!s_player.configured) {
        return APP_ERR_NOT_READY;
    }
    if (s_player.boot_tts_started) {
        app_log("watch boot tts skipped: already started");
        return APP_OK;
    }
    if (s_player.task_started) {
        app_log("watch boot tts skipped: player task busy");
        return APP_OK;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch boot tts skipped: session=%s",
                watch_session_name(watch_session_current()));
        return APP_OK;
    }

    ret = app_player_init();
    if (ret != APP_OK) {
        app_log("watch boot tts player init failed: %d", ret);
        return ret;
    }
    ret = watch_player_prepare_tts_file(&s_player.current_file,
                                        WATCH_PLAYER_TTS_NAME,
                                        WATCH_PLAYER_TTS_TEXT);
    if (ret != APP_OK) {
        (void)app_player_deinit();
        return ret;
    }

    s_player.stop_requested = false;
    s_player.last_report_percent = 0U;
    s_player.task_started = true;
    ret = app_os_task_create(&s_player.task,
                             "watch_boot_tts",
                             watch_player_boot_tts_task,
                             NULL,
                             WATCH_PLAYER_TASK_STACK_BYTES,
                             WATCH_PLAYER_TASK_PRIORITY);
    if (ret != APP_OK) {
        s_player.task_started = false;
        s_player.task = NULL;
        memset(&s_player.current_file, 0, sizeof(s_player.current_file));
        (void)app_player_deinit();
        app_log("watch boot tts task create failed: %d", ret);
        return ret;
    }

    s_player.boot_tts_started = true;
#if WATCH_AI_ENABLE_DISPLAY
    (void)app_display_set_status("TTS");
#endif
    app_log("watch boot tts task created");
    return APP_OK;
}

int watch_player_start(void)
{
    app_display_player_file_t display_files[APP_DISPLAY_PLAYER_FILE_MAX];
    int ret;

    app_log("watch player start");
    if (!s_player.configured) {
        return APP_ERR_NOT_READY;
    }
    if (watch_session_current() == WATCH_SESSION_PLAYER) {
        app_log("watch player already running");
        return APP_OK;
    }
    if (s_player.task_started) {
        app_log("watch player start rejected: task busy");
        return APP_ERR_BUSY;
    }
    if (watch_session_current() != WATCH_SESSION_NONE) {
        app_log("watch player start rejected: session=%s",
                watch_session_name(watch_session_current()));
        return APP_ERR_BUSY;
    }

    ret = watch_session_open(WATCH_SESSION_PLAYER, "player", watch_player_stop);
    if (ret != APP_OK) {
        app_log("watch player session open failed: %d", ret);
        return ret;
    }

    ret = app_player_init();
    if (ret != APP_OK) {
        app_log("watch player service init failed: %d", ret);
        (void)watch_session_close(WATCH_SESSION_PLAYER);
        return ret;
    }

    s_player.files = (app_player_file_t *)app_os_malloc(
        APP_DISPLAY_PLAYER_FILE_MAX * sizeof(app_player_file_t));
    if (s_player.files == NULL) {
        app_log("watch player file list alloc failed");
        (void)app_player_deinit();
        (void)watch_session_close(WATCH_SESSION_PLAYER);
        return APP_ERR_NO_MEMORY;
    }
    app_log("watch player files alloc: count=%u bytes=%lu ptr=%p",
            (unsigned int)APP_DISPLAY_PLAYER_FILE_MAX,
            (unsigned long)(APP_DISPLAY_PLAYER_FILE_MAX * sizeof(app_player_file_t)),
            (void *)s_player.files);
    memset(s_player.files, 0, APP_DISPLAY_PLAYER_FILE_MAX * sizeof(app_player_file_t));

    ret = app_player_list_files("/",
                                s_player.files,
                                APP_DISPLAY_PLAYER_FILE_MAX - 1U,
                                &s_player.file_count);
    if (ret != APP_OK) {
        app_log("watch player list files failed: %d", ret);
        s_player.file_count = 0U;
    }

    ret = watch_player_prepend_tts_item();
    if (ret != APP_OK && s_player.file_count == 0U) {
        app_log("watch player tts item prepend failed: %d", ret);
        watch_player_release_files();
        (void)app_player_deinit();
        (void)watch_session_close(WATCH_SESSION_PLAYER);
        return ret;
    }

    memset(display_files, 0, sizeof(display_files));
    for (uint32_t i = 0U; i < s_player.file_count; i++) {
        watch_player_copy_display_file(&display_files[i], &s_player.files[i]);
    }

    ret = app_display_set_player_files(display_files, s_player.file_count);
    if (ret != APP_OK) {
        app_log("watch player display files failed: %d", ret);
        watch_player_release_files();
        (void)app_player_deinit();
        (void)watch_session_close(WATCH_SESSION_PLAYER);
        return ret;
    }

    ret = watch_page_replace(WATCH_PAGE_PLAYER_LIST);
    if (ret != APP_OK) {
        app_log("watch player page switch to list failed: %d", ret);
        watch_player_release_files();
        (void)app_player_deinit();
        (void)watch_session_close(WATCH_SESSION_PLAYER);
        return ret;
    }

#if WATCH_AI_ENABLE_DISPLAY
    (void)app_display_set_status("PLAYER");
#endif
    app_log("watch player started: files=%lu",
            (unsigned long)s_player.file_count);
    return APP_OK;
}

int watch_player_start_tts(void)
{
    uint32_t index;
    int ret;

    app_log("watch player tts start");
    if (watch_session_current() == WATCH_SESSION_PLAYER &&
        watch_page_current() == WATCH_PAGE_PLAYER_LIST) {
        index = watch_player_find_tts_index();
        if (index >= APP_DISPLAY_PLAYER_FILE_MAX) {
            app_log("watch player tts select failed: no item");
            return APP_ERR_NOT_READY;
        }
        return watch_player_select(index);
    }

    ret = watch_player_start();
    if (ret != APP_OK) {
        return ret;
    }
    index = watch_player_find_tts_index();
    if (index >= APP_DISPLAY_PLAYER_FILE_MAX) {
        app_log("watch player tts select failed: no item");
        (void)watch_player_stop();
        return APP_ERR_NOT_READY;
    }
    ret = watch_player_select(index);
    if (ret != APP_OK) {
        (void)watch_player_stop();
    }
    return ret;
}

int watch_player_select(uint32_t index)
{
    int ret;

    if (watch_session_current() != WATCH_SESSION_PLAYER) {
        return APP_ERR_NOT_READY;
    }
    if (watch_page_current() != WATCH_PAGE_PLAYER_LIST) {
        app_log("watch player select rejected: page=%s",
                watch_page_name(watch_page_current()));
        return APP_ERR_BUSY;
    }
    if (s_player.files == NULL || index >= s_player.file_count) {
        app_log("watch player select invalid: index=%lu count=%lu",
                (unsigned long)index,
                (unsigned long)s_player.file_count);
        return APP_ERR_INVALID_ARG;
    }
    if (s_player.task_started) {
        app_log("watch player select rejected: busy");
        return APP_ERR_BUSY;
    }

    s_player.selected_index = index;
    s_player.current_file = s_player.files[index];
    s_player.stop_requested = false;
    s_player.last_report_percent = 0U;
    s_player.task_started = true;
    ret = app_os_task_create(&s_player.task,
                             "watch_player",
                             watch_player_task,
                             NULL,
                             WATCH_PLAYER_TASK_STACK_BYTES,
                             WATCH_PLAYER_TASK_PRIORITY);
    if (ret != APP_OK) {
        s_player.task_started = false;
        s_player.task = NULL;
        app_log("watch player task create failed: %d", ret);
        return ret;
    }

    ret = watch_page_replace(WATCH_PAGE_PLAYER_NOW_PLAYING);
    if (ret != APP_OK) {
        app_log("watch player page switch to now playing failed: %d", ret);
        s_player.stop_requested = true;
        (void)app_player_stop();
        (void)watch_player_wait_exit(WATCH_PLAYER_STOP_TIMEOUT_MS);
        watch_player_reset_runtime();
        return ret;
    }

#if WATCH_AI_ENABLE_DISPLAY
    (void)app_display_set_status("PLAY");
#endif

    app_log("watch player selected: index=%lu file=%s type=%s size=%lu",
            (unsigned long)index,
            s_player.current_file.name,
            watch_player_type_name(s_player.current_file.type),
            (unsigned long)s_player.current_file.size_bytes);
    return APP_OK;
}

int watch_player_stop(void)
{
    watch_page_t current_page = watch_page_current();
    int ret = APP_OK;

    app_log("watch player stop: page=%s task=%d playing=%d",
            watch_page_name(current_page),
            s_player.task_started ? 1 : 0,
            s_player.playing ? 1 : 0);

    if (current_page == WATCH_PAGE_PLAYER_NOW_PLAYING || s_player.task_started) {
        if (s_player.task_started) {
            s_player.stop_requested = true;
            (void)app_player_stop();
            ret = watch_player_wait_exit(WATCH_PLAYER_STOP_TIMEOUT_MS);
            if (ret != APP_OK) {
                app_log("watch player stop waiting failed: %d", ret);
                return ret;
            }
        }

        if (current_page == WATCH_PAGE_PLAYER_NOW_PLAYING) {
            (void)watch_page_replace(WATCH_PAGE_PLAYER_LIST);
#if WATCH_AI_ENABLE_DISPLAY
            (void)app_display_set_status("PLAYER");
#endif
            app_log("watch player back: now_playing -> list");
            return APP_OK;
        }
    }

    watch_player_clear_display();
    watch_player_release_files();
    if (app_player_is_initialized()) {
        ret = app_player_deinit();
        if (ret != APP_OK) {
            app_log("watch player deinit failed: %d", ret);
        }
    }
    (void)watch_session_close(WATCH_SESSION_PLAYER);
    (void)watch_page_replace(WATCH_PAGE_HOME);
#if WATCH_AI_ENABLE_DISPLAY
    (void)app_display_set_status("IDLE");
#endif
    watch_player_reset_runtime();
    app_log("watch player back: list -> home");
    return ret;
}
