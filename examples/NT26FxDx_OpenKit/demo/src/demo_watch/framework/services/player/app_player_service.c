#include "app_player_service.h"

#include <stddef.h>

#include "app_osal.h"

static const app_player_port_t *s_port;
static bool s_initialized;

static const app_player_port_ops_t *player_ops(void)
{
    return (s_port != NULL) ? s_port->ops : NULL;
}

static int player_require_ready(const app_player_port_ops_t **ops)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    *ops = player_ops();
    if (*ops == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return APP_OK;
}

int app_player_register_port(const app_player_port_t *port)
{
    if (port == NULL || port->ops == NULL ||
        port->ops->init == NULL ||
        port->ops->deinit == NULL ||
        port->ops->list_files == NULL ||
        port->ops->play_file == NULL ||
        port->ops->stop == NULL) {
        app_log("player port register rejected");
        return APP_ERR_INVALID_ARG;
    }

    s_port = port;
    app_log("player port registered");
    return APP_OK;
}

int app_player_init(void)
{
    const app_player_port_ops_t *ops = player_ops();
    int ret;

    if (s_initialized) {
        return APP_OK;
    }
    if (ops == NULL || ops->init == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = ops->init();
    if (ret != APP_OK) {
        app_log("player service init failed: %d", ret);
        return ret;
    }
    s_initialized = true;
    app_log("player service initialized");
    return APP_OK;
}

int app_player_deinit(void)
{
    const app_player_port_ops_t *ops;
    int ret = player_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    ret = ops->deinit();
    s_initialized = false;
    app_log("player service deinit: ret=%d", ret);
    return ret;
}

int app_player_list_files(const char *root,
                          app_player_file_t *files,
                          uint32_t capacity,
                          uint32_t *count)
{
    const app_player_port_ops_t *ops;
    int ret;

    if (files == NULL || capacity == 0U || count == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *count = 0U;
    ret = player_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return ops->list_files(root, files, capacity, count);
}

int app_player_play_file(const app_player_file_t *file,
                         volatile bool *stop_requested,
                         app_player_progress_cb_t progress_cb,
                         void *progress_user)
{
    const app_player_port_ops_t *ops;
    int ret;

    if (file == NULL || file->path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    ret = player_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return ops->play_file(file, stop_requested, progress_cb, progress_user);
}

int app_player_stop(void)
{
    const app_player_port_ops_t *ops;
    int ret = player_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return ops->stop();
}

bool app_player_is_initialized(void)
{
    return s_initialized;
}

const char *app_player_file_type_name(app_player_file_type_t type)
{
    switch (type) {
    case APP_PLAYER_FILE_MP3:
        return "mp3";
    case APP_PLAYER_FILE_WAV:
        return "wav";
    case APP_PLAYER_FILE_TTS:
        return "tts";
    default:
        return "unknown";
    }
}
