#include "watch_network.h"

#include "app_config.h"

#include <string.h>

#include "app_event.h"
#include "app_osal.h"

#if WATCH_AI_ENABLE_NETWORK
#include "app_network_service.h"

static void watch_network_event_cb(app_network_event_t event, const char *detail, void *user)
{
    app_event_t app_event;

    (void)detail;
    (void)user;
    memset(&app_event, 0, sizeof(app_event));
    if (event == APP_NETWORK_EVENT_CONNECTED) {
        app_event.id = APP_EV_NETWORK_CONNECTED;
    } else if (event == APP_NETWORK_EVENT_DISCONNECTED) {
        app_event.id = APP_EV_NETWORK_DISCONNECTED;
    } else if (event == APP_NETWORK_EVENT_ERROR) {
        app_event.id = APP_EV_ERROR;
        app_event.data.error.message = detail;
    } else {
        return;
    }
    (void)app_event_post(&app_event);
}
#endif

int watch_network_init(void)
{
#if WATCH_AI_ENABLE_NETWORK
    (void)app_network_set_event_cb(watch_network_event_cb, NULL);
    return app_network_init();
#else
    return APP_OK;
#endif
}

int watch_network_start(void)
{
#if WATCH_AI_ENABLE_NETWORK
    return app_network_start();
#else
    return APP_ERR_NOT_SUPPORTED;
#endif
}
