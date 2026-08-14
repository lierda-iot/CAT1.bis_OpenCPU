#ifndef WATCH_PLAYER_H
#define WATCH_PLAYER_H

#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_player_init(void);
int watch_player_start_boot_tts(void);
int watch_player_start(void);
int watch_player_start_tts(void);
int watch_player_select(uint32_t index);
int watch_player_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_PLAYER_H */
