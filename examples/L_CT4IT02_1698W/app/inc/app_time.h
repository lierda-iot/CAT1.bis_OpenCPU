#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include "liot_rtc.h"

bool app_time_get_rtc_display(liot_rtc_time_s *tm);

#endif
