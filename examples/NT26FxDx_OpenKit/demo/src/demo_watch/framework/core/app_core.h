#ifndef APP_CORE_H
#define APP_CORE_H

#include <stdint.h>

#include "app_error.h"
#include "app_event.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *product_name;
    app_protocol_type_t protocol_type;
    uint32_t flags;
} app_core_config_t;

typedef void (*app_core_event_cb_t)(const app_event_t *event, void *user);

int app_core_init(const app_core_config_t *config);
int app_core_post_boot_done(void);
int app_core_process_once(uint32_t timeout_ms);
void app_core_run_forever(void);
const app_core_config_t *app_core_get_config(void);
int app_core_set_event_cb(app_core_event_cb_t cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* APP_CORE_H */
