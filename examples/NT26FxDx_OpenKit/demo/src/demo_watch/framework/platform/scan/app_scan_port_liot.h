#ifndef APP_SCAN_PORT_LIOT_H
#define APP_SCAN_PORT_LIOT_H

#include "app_scan_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_SCAN_LIOT_BACKEND_NONE = 0,
    APP_SCAN_LIOT_BACKEND_QY,
} app_scan_liot_backend_t;

typedef struct {
    app_scan_liot_backend_t backend;
} app_scan_liot_config_t;

int app_scan_liot_setup(const app_scan_liot_config_t *config);
int app_scan_liot_register(void);
const app_scan_port_t *app_scan_liot_port(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SCAN_PORT_LIOT_H */
