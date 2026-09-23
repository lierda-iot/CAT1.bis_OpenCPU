#include "app_recorder_service.h"

#include <stddef.h>

#include "app_osal.h"

static const app_recorder_port_t *s_port;
static bool s_initialized;

static const app_recorder_port_ops_t *recorder_ops(void)
{
    return (s_port != NULL) ? s_port->ops : NULL;
}

static int recorder_require_ready(const app_recorder_port_ops_t **ops)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    *ops = recorder_ops();
    if (*ops == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return APP_OK;
}

int app_recorder_register_port(const app_recorder_port_t *port)
{
    if (port == NULL || port->ops == NULL ||
        port->ops->init == NULL ||
        port->ops->deinit == NULL ||
        port->ops->file_exists == NULL ||
        port->ops->record_file == NULL ||
        port->ops->stop == NULL) {
        app_log("recorder port register rejected");
        return APP_ERR_INVALID_ARG;
    }

    s_port = port;
    app_log("recorder port registered");
    return APP_OK;
}

int app_recorder_init(void)
{
    const app_recorder_port_ops_t *ops = recorder_ops();
    int ret;

    if (s_initialized) {
        return APP_OK;
    }
    if (ops == NULL || ops->init == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = ops->init();
    if (ret != APP_OK) {
        app_log("recorder service init failed: %d", ret);
        return ret;
    }
    s_initialized = true;
    app_log("recorder service initialized");
    return APP_OK;
}

int app_recorder_deinit(void)
{
    const app_recorder_port_ops_t *ops;
    int ret = recorder_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    ret = ops->deinit();
    s_initialized = false;
    app_log("recorder service deinit: ret=%d", ret);
    return ret;
}

int app_recorder_file_exists(const char *path, bool *exists)
{
    const app_recorder_port_ops_t *ops;
    int ret;

    if (path == NULL || path[0] == '\0' || exists == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = recorder_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return ops->file_exists(path, exists);
}

int app_recorder_record_file(const app_recorder_file_t *file,
                             volatile bool *stop_requested,
                             app_recorder_progress_cb_t progress_cb,
                             void *progress_user)
{
    const app_recorder_port_ops_t *ops;
    int ret;

    if (file == NULL || file->path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    ret = recorder_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }
    return ops->record_file(file, stop_requested, progress_cb, progress_user);
}

int app_recorder_stop(void)
{
    const app_recorder_port_ops_t *ops;
    int ret = recorder_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }
    return ops->stop();
}

bool app_recorder_is_initialized(void)
{
    return s_initialized;
}
