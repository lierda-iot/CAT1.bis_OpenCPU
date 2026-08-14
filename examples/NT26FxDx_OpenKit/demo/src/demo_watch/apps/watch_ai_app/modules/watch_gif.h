#ifndef WATCH_GIF_H
#define WATCH_GIF_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_gif_init(void);
int watch_gif_start(void);
int watch_gif_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_GIF_H */
