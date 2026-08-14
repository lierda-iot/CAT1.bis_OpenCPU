#ifndef WATCH_DISPLAY_H
#define WATCH_DISPLAY_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_display_init(void);
int watch_display_init_ui(void);
void watch_display_show_unavailable(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_DISPLAY_H */
