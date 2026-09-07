#ifndef WATCH_RECORDER_H
#define WATCH_RECORDER_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_recorder_init(void);
int watch_recorder_start(void);
int watch_recorder_toggle(void);
int watch_recorder_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_RECORDER_H */
