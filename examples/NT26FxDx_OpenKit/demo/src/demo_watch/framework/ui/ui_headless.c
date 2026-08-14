#include "ui_headless.h"

#include "app_osal.h"
#include "app_state.h"

int ui_headless_init(void)
{
    app_log("ui headless initialized");
    return APP_OK;
}

void ui_headless_on_state_changed(app_state_t old_state,
                                  app_state_t new_state,
                                  const app_event_t *event,
                                  void *user)
{
    (void)event;
    (void)user;
    app_log("ui headless state: %s -> %s",
            app_state_name(old_state),
            app_state_name(new_state));
}
