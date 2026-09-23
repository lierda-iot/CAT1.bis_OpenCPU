#ifndef APP_OTA_SERVICE_H
#define APP_OTA_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_ota_progress_cb_t)(uint8_t progress, uint32_t bytes_per_second, void *user);

typedef struct {
    char current_version[32];
    char next_version[32];
    char download_url[256];
    bool has_update;
    bool force_update;
} app_ota_version_info_t;

typedef struct {
    int (*init)(void);
    int (*check_version)(app_ota_version_info_t *info);
    int (*upgrade)(const char *url, app_ota_progress_cb_t cb, void *user);
    int (*mark_valid)(void);
} app_ota_driver_t;

int app_ota_register_driver(const app_ota_driver_t *driver);
int app_ota_init(void);
int app_ota_check_version(app_ota_version_info_t *info);
int app_ota_upgrade(const char *url, app_ota_progress_cb_t cb, void *user);
int app_ota_mark_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_OTA_SERVICE_H */
