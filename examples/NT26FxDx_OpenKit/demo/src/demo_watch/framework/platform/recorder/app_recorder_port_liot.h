#ifndef APP_RECORDER_PORT_LIOT_H
#define APP_RECORDER_PORT_LIOT_H

#include <stdint.h>

#include "app_recorder_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t chunk_bytes;
} app_recorder_liot_config_t;

int app_recorder_liot_get_default_config(app_recorder_liot_config_t *config);
int app_recorder_liot_setup(const app_recorder_liot_config_t *config);
int app_recorder_liot_register(void);
const app_recorder_port_t *app_recorder_liot_port(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RECORDER_PORT_LIOT_H */
