#ifndef	_LWS__ADAPT_H_
#define _LWS__ADAPT_H_

#include "liot_type.h"
#include "liot_os.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"

#ifndef xPortGetFreeHeapSize
extern size_t xPortGetFreeHeapSizeEc( void );
#define xPortGetFreeHeapSize()	xPortGetFreeHeapSizeEc()
#endif

struct timeval;
int _gettimeofday(struct timeval *tv, void *tz);
void liot_lws_log(void *cx, const char *fmt, ...);
#endif