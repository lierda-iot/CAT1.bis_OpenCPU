#ifndef UI_AI_BASIC_H
#define UI_AI_BASIC_H

#include "app_error.h"
#include "app_event.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *idle_text;
    const char *listening_text;
    const char *thinking_text;
    const char *speaking_text;
    const char *error_text;
} ui_ai_basic_config_t;

int ui_ai_basic_init(const ui_ai_basic_config_t *config);
int ui_ai_basic_show_state(app_state_t state);
void ui_ai_basic_on_state_changed(app_state_t old_state,
                                  app_state_t new_state,
                                  const app_event_t *event,
                                  void *user);

#ifdef __cplusplus
}
#endif

#endif /* UI_AI_BASIC_H */
