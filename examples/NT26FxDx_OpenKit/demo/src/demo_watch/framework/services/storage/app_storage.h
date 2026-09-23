#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int (*init)(void);
    int (*get_string)(const char *ns, const char *key, char *value, uint32_t value_size);
    int (*set_string)(const char *ns, const char *key, const char *value);
    int (*get_i32)(const char *ns, const char *key, int32_t *value);
    int (*set_i32)(const char *ns, const char *key, int32_t value);
    int (*get_bool)(const char *ns, const char *key, bool *value);
    int (*set_bool)(const char *ns, const char *key, bool value);
    int (*erase_key)(const char *ns, const char *key);
} app_storage_driver_t;

int app_storage_register_driver(const app_storage_driver_t *driver);
int app_storage_init(void);
int app_storage_get_string(const char *ns, const char *key, char *value, uint32_t value_size);
int app_storage_set_string(const char *ns, const char *key, const char *value);
int app_storage_get_i32(const char *ns, const char *key, int32_t *value);
int app_storage_set_i32(const char *ns, const char *key, int32_t value);
int app_storage_get_bool(const char *ns, const char *key, bool *value);
int app_storage_set_bool(const char *ns, const char *key, bool value);
int app_storage_erase_key(const char *ns, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* APP_STORAGE_H */
