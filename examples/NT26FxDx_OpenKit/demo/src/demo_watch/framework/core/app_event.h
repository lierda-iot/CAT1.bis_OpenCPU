#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdint.h>

#include "app_error.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    app_event_id_t id;
    union {
        struct {
            int code;
            const char *message;
        } error;
        struct {
            const uint8_t *data;
            uint32_t len;
            uint32_t timestamp;
        } audio;
        struct {
            uint16_t x;
            uint16_t y;
            uint8_t action;
        } touch;
        struct {
            uint32_t action;
            uint32_t value;
        } display_action;
        struct {
            uint8_t progress;
            uint32_t bytes_per_second;
        } ota;
        uintptr_t value;
        void *ptr;
    } data;
} app_event_t;

int app_event_init(void);
int app_event_post(const app_event_t *event);
int app_event_wait(app_event_t *event, uint32_t timeout_ms);
const char *app_event_name(app_event_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_H */
