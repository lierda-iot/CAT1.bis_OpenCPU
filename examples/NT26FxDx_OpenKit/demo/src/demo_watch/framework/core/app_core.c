#include "app_core.h"

#include <string.h>

#include "app_osal.h"
#include "app_state.h"

static app_core_config_t s_config;
static app_core_event_cb_t s_event_cb;
static void *s_event_cb_user;
static bool s_initialized;

int app_core_init(const app_core_config_t *config)
{
    int ret;

    if (config == NULL || config->product_name == NULL) {
        app_log("app_core init rejected: invalid config");
        return APP_ERR_INVALID_ARG;
    }

    app_log("app_core init: event queue");
    memset(&s_config, 0, sizeof(s_config));
    s_config = *config;
    s_event_cb = NULL;
    s_event_cb_user = NULL;

    ret = app_event_init();
    if (ret != APP_OK) {
        app_log("app_core event queue init failed: %d", ret);
        return ret;
    }

    app_log("app_core init: state machine");
    ret = app_state_init();
    if (ret != APP_OK) {
        app_log("app_core state init failed: %d", ret);
        return ret;
    }

    s_initialized = true;
    app_log("app_core initialized: product=%s protocol=%d",
            s_config.product_name,
            (int)s_config.protocol_type);
    return APP_OK;
}

int app_core_post_boot_done(void)
{
    app_event_t event;
    int ret;

    if (!s_initialized) {
        app_log("app_core boot event rejected: not initialized");
        return APP_ERR_NOT_READY;
    }

    memset(&event, 0, sizeof(event));
    event.id = APP_EV_BOOT_DONE;
    ret = app_event_post(&event);
    if (ret != APP_OK) {
        app_log("app_core boot event post failed: %d", ret);
        return ret;
    }
    app_log("app_core boot event posted");
    return APP_OK;
}

int app_core_process_once(uint32_t timeout_ms)
{
    app_event_t event;
    int ret;

    if (!s_initialized) {
        app_log("app_core process rejected: not initialized");
        return APP_ERR_NOT_READY;
    }

    memset(&event, 0, sizeof(event));
    ret = app_event_wait(&event, timeout_ms);
    if (ret != APP_OK) {
        return ret;
    }

    //app_log("event: %s", app_event_name(event.id));
    ret = app_state_handle_event(&event);
    if (ret != APP_OK) {
        app_log("app_core event handle failed: %s ret=%d", app_event_name(event.id), ret);
    }
    if (s_event_cb != NULL) {
        s_event_cb(&event, s_event_cb_user);
    }
    return ret;
}

void app_core_run_forever(void)
{
    while (true) {
        (void)app_core_process_once(APP_OS_WAIT_FOREVER);
    }
}

const app_core_config_t *app_core_get_config(void)
{
    return s_initialized ? &s_config : NULL;
}

int app_core_set_event_cb(app_core_event_cb_t cb, void *user)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }

    s_event_cb = cb;
    s_event_cb_user = user;
    return APP_OK;
}
