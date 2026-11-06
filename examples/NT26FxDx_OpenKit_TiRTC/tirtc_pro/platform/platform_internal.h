#ifndef TIRTC_PLATFORM_INTERNAL_H
#define TIRTC_PLATFORM_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include "liot_os.h"
/* Task-only, no SDK I/O: compare attempt with the network owner's cache. */
bool tirtc_platform_attempt_current(void);
/* Shared static-pool guard; a pending temporary client blocks formal init. */
bool tirtc_platform_temp_mqtt_idle(void);
/* An idle, callback-detached semaphore only. F6D delete first takes a token. */
LiotOSStatus_t tirtc_platform_sem_delete_idle(liot_sem_t semaphore);
void tirtc_platform_binding_event(int state, int error);
/* Strict UTC RTC -> Unix seconds; failure leaves no usable timestamp. */
bool tirtc_platform_utc_seconds(uint32_t *seconds);
#endif
