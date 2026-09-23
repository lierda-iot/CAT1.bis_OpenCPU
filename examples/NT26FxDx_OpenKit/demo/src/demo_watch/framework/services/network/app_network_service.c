#include "app_network_service.h"

#include <stddef.h>

static const app_network_driver_t *s_driver;

int app_network_register_driver(const app_network_driver_t *driver)
{
    if (driver == NULL || driver->init == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    s_driver = driver;
    return APP_OK;
}

int app_network_init(void)
{
    return (s_driver != NULL) ? s_driver->init() : APP_ERR_NOT_SUPPORTED;
}

int app_network_start(void)
{
    return (s_driver != NULL && s_driver->start != NULL) ? s_driver->start() : APP_ERR_NOT_SUPPORTED;
}

int app_network_stop(void)
{
    return (s_driver != NULL && s_driver->stop != NULL) ? s_driver->stop() : APP_ERR_NOT_SUPPORTED;
}

bool app_network_is_connected(void)
{
    return (s_driver != NULL && s_driver->is_connected != NULL) ? s_driver->is_connected() : false;
}

app_network_type_t app_network_type(void)
{
    return (s_driver != NULL && s_driver->type != NULL) ? s_driver->type() : APP_NETWORK_TYPE_NONE;
}

const char *app_network_name(void)
{
    return (s_driver != NULL && s_driver->name != NULL) ? s_driver->name() : "";
}

const char *app_network_ip(void)
{
    return (s_driver != NULL && s_driver->ip != NULL) ? s_driver->ip() : "";
}

int app_network_set_event_cb(app_network_event_cb_t cb, void *user)
{
    if (s_driver == NULL || s_driver->set_event_cb == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->set_event_cb(cb, user);
}

const char *app_network_type_name(app_network_type_t type)
{
    switch (type) {
    case APP_NETWORK_TYPE_NONE:
        return "none";
    case APP_NETWORK_TYPE_CELLULAR:
        return "cellular";
    case APP_NETWORK_TYPE_WIFI:
        return "wifi";
    case APP_NETWORK_TYPE_USB:
        return "usb";
    default:
        return "unknown";
    }
}
