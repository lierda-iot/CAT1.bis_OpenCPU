#ifndef WATCH_AI_APP_MAIN_H
#define WATCH_AI_APP_MAIN_H

#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_ai_app_init(void);
int watch_ai_app_go_back(void);
int watch_ai_app_run_once(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_AI_APP_MAIN_H */
