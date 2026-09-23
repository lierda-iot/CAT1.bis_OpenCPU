#ifndef APP_PLAYER_PORT_LIOT_H
#define APP_PLAYER_PORT_LIOT_H

#include <stdint.h>

#include "app_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t chunk_size;
    uint32_t wait_timeout_s;
} app_player_liot_config_t;

int app_player_liot_get_default_config(app_player_liot_config_t *config);
int app_player_liot_setup(const app_player_liot_config_t *config);
int app_player_liot_register(void);
const app_player_port_t *app_player_liot_port(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_PORT_LIOT_H */
