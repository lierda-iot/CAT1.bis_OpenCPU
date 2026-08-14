#ifndef APP_NETWORK_SERVICE_H
#define APP_NETWORK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_NETWORK_TYPE_NONE = 0,
    APP_NETWORK_TYPE_CELLULAR,
    APP_NETWORK_TYPE_WIFI,
    APP_NETWORK_TYPE_USB,
} app_network_type_t;

typedef enum {
    APP_NETWORK_EVENT_DISCONNECTED = 0,
    APP_NETWORK_EVENT_CONNECTING,
    APP_NETWORK_EVENT_CONNECTED,
    APP_NETWORK_EVENT_ERROR,
} app_network_event_t;

typedef void (*app_network_event_cb_t)(app_network_event_t event,
                                       const char *detail,
                                       void *user);

typedef struct {
    int (*init)(void);
    int (*start)(void);
    int (*stop)(void);
    bool (*is_connected)(void);
    app_network_type_t (*type)(void);
    const char *(*name)(void);
    const char *(*ip)(void);
    int (*set_event_cb)(app_network_event_cb_t cb, void *user);
} app_network_driver_t;

int app_network_register_driver(const app_network_driver_t *driver);
int app_network_init(void);
int app_network_start(void);
int app_network_stop(void);
bool app_network_is_connected(void);
app_network_type_t app_network_type(void);
const char *app_network_name(void);
const char *app_network_ip(void);
int app_network_set_event_cb(app_network_event_cb_t cb, void *user);
const char *app_network_type_name(app_network_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* APP_NETWORK_SERVICE_H */
