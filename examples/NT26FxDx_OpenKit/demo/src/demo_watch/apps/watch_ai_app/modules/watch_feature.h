#ifndef WATCH_FEATURE_H
#define WATCH_FEATURE_H

#include "app_display_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_feature_start(app_display_action_t action);
int watch_feature_go_back(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_FEATURE_H */
