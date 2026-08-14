#ifndef WATCH_AUDIO_H
#define WATCH_AUDIO_H

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_audio_init(void);
int watch_audio_start_listen(void);
int watch_audio_stop_listen(void);
void watch_audio_stop_voice_path(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_AUDIO_H */
