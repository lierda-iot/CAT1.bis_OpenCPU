#include "app_storage.h"

#include <stddef.h>

static const app_storage_driver_t *s_driver;

int app_storage_register_driver(const app_storage_driver_t *driver)
{
    if (driver == NULL || driver->init == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    s_driver = driver;
    return APP_OK;
}

int app_storage_init(void)
{
    return (s_driver != NULL) ? s_driver->init() : APP_ERR_NOT_SUPPORTED;
}

int app_storage_get_string(const char *ns, const char *key, char *value, uint32_t value_size)
{
    if (s_driver == NULL || s_driver->get_string == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->get_string(ns, key, value, value_size);
}

int app_storage_set_string(const char *ns, const char *key, const char *value)
{
    if (s_driver == NULL || s_driver->set_string == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->set_string(ns, key, value);
}

int app_storage_get_i32(const char *ns, const char *key, int32_t *value)
{
    if (s_driver == NULL || s_driver->get_i32 == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->get_i32(ns, key, value);
}

int app_storage_set_i32(const char *ns, const char *key, int32_t value)
{
    if (s_driver == NULL || s_driver->set_i32 == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->set_i32(ns, key, value);
}

int app_storage_get_bool(const char *ns, const char *key, bool *value)
{
    if (s_driver == NULL || s_driver->get_bool == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->get_bool(ns, key, value);
}

int app_storage_set_bool(const char *ns, const char *key, bool value)
{
    if (s_driver == NULL || s_driver->set_bool == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->set_bool(ns, key, value);
}

int app_storage_erase_key(const char *ns, const char *key)
{
    if (s_driver == NULL || s_driver->erase_key == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return s_driver->erase_key(ns, key);
}
