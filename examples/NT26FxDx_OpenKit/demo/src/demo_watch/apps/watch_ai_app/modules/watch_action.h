#ifndef WATCH_ACTION_H
#define WATCH_ACTION_H

#include "app_display_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_action_handle(app_display_action_t action, uint32_t value);
int watch_action_go_back(void);
void watch_action_display_cb(const app_display_action_event_t *event, void *user);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_ACTION_H */
