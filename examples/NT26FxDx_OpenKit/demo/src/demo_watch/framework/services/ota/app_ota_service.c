#include "app_ota_service.h"

#include <stddef.h>
#include <string.h>

static const app_ota_driver_t *s_driver;

int app_ota_register_driver(const app_ota_driver_t *driver)
{
    if (driver == NULL || driver->init == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    s_driver = driver;
    return APP_OK;
}

int app_ota_init(void)
{
    return (s_driver != NULL) ? s_driver->init() : APP_ERR_NOT_SUPPORTED;
}

int app_ota_check_version(app_ota_version_info_t *info)
{
    if (info == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    memset(info, 0, sizeof(*info));
    if (s_driver == NULL || s_driver->check_version == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->check_version(info);
}

int app_ota_upgrade(const char *url, app_ota_progress_cb_t cb, void *user)
{
    if (url == NULL || url[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (s_driver == NULL || s_driver->upgrade == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->upgrade(url, cb, user);
}

int app_ota_mark_valid(void)
{
    if (s_driver == NULL || s_driver->mark_valid == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->mark_valid();
}
