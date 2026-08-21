#ifndef WATCH_DRAWING_H
#define WATCH_DRAWING_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_drawing_init(void);
int watch_drawing_start(void);
int watch_drawing_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_DRAWING_H */
