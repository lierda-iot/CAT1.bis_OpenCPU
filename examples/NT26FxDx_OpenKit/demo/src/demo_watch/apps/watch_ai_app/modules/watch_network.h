#ifndef WATCH_NETWORK_H
#define WATCH_NETWORK_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_network_init(void);
int watch_network_start(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_NETWORK_H */
