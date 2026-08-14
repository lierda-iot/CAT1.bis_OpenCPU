#ifndef UI_HEADLESS_H
#define UI_HEADLESS_H

#include "app_error.h"
#include "app_event.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int ui_headless_init(void);
void ui_headless_on_state_changed(app_state_t old_state,
                                  app_state_t new_state,
                                  const app_event_t *event,
                                  void *user);

#ifdef __cplusplus
}
#endif

#endif /* UI_HEADLESS_H */
