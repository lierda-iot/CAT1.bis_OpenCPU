#ifndef APP_STATE_H
#define APP_STATE_H

#include "app_error.h"
#include "app_event.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_state_listener_t)(app_state_t old_state,
                                     app_state_t new_state,
                                     const app_event_t *event,
                                     void *user);

int app_state_init(void);
app_state_t app_state_get(void);
const char *app_state_name(app_state_t state);
int app_state_transition(app_state_t next, const app_event_t *event);
int app_state_handle_event(const app_event_t *event);
int app_state_add_listener(app_state_listener_t listener, void *user);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_H */
