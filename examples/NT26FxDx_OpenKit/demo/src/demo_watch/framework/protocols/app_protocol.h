#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_PROTOCOL_EVENT_NONE = 0,
    APP_PROTOCOL_EVENT_CONNECTED,
    APP_PROTOCOL_EVENT_DISCONNECTED,
    APP_PROTOCOL_EVENT_AUDIO_OPENED,
    APP_PROTOCOL_EVENT_AUDIO_CLOSED,
    APP_PROTOCOL_EVENT_JSON,
    APP_PROTOCOL_EVENT_AUDIO_FRAME,
    APP_PROTOCOL_EVENT_ERROR,
} app_protocol_event_id_t;

typedef struct {
    app_protocol_event_id_t id;
    const void *data;
    uint32_t len;
    int code;
    const char *message;
} app_protocol_event_t;

typedef void (*app_protocol_event_cb_t)(const app_protocol_event_t *event, void *user);

typedef struct {
    int (*init)(void *ctx);
    int (*deinit)(void *ctx);
    int (*start)(void *ctx);
    int (*open_audio)(void *ctx);
    int (*close_audio)(void *ctx);
    int (*send_audio)(void *ctx, const uint8_t *data, uint32_t len, uint32_t timestamp);
    int (*send_json)(void *ctx, const char *json);
    int (*stop)(void *ctx);
} app_protocol_ops_t;

typedef struct {
    app_protocol_type_t type;
    void *ctx;
    const app_protocol_ops_t *ops;
    app_protocol_event_cb_t event_cb;
    void *event_user;
    bool initialized;
    bool started;
} app_protocol_t;

int app_protocol_bind(app_protocol_t *protocol,
                      app_protocol_type_t type,
                      const app_protocol_ops_t *ops,
                      void *ctx);
int app_protocol_init(app_protocol_t *protocol);
int app_protocol_deinit(app_protocol_t *protocol);
int app_protocol_start(app_protocol_t *protocol);
int app_protocol_stop(app_protocol_t *protocol);
int app_protocol_open_audio(app_protocol_t *protocol);
int app_protocol_close_audio(app_protocol_t *protocol);
int app_protocol_send_audio(app_protocol_t *protocol,
                            const uint8_t *data,
                            uint32_t len,
                            uint32_t timestamp);
int app_protocol_send_json(app_protocol_t *protocol, const char *json);
int app_protocol_set_event_cb(app_protocol_t *protocol, app_protocol_event_cb_t cb, void *user);
int app_protocol_emit_event(app_protocol_t *protocol, const app_protocol_event_t *event);
const char *app_protocol_type_name(app_protocol_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTOCOL_H */
