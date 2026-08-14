#ifndef WATCH_PROTOCOL_H
#define WATCH_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_protocol_init(void);
int watch_protocol_start(void);
int watch_protocol_open_audio(void);
int watch_protocol_close_audio(void);
int watch_protocol_send_audio(const uint8_t *data, uint32_t len, uint32_t timestamp);
bool watch_protocol_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_PROTOCOL_H */
