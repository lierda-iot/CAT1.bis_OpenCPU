#ifndef PRO_AUDIO_GAIN_HOST_OS_H
#define PRO_AUDIO_GAIN_HOST_OS_H
#include <stdint.h>
void liot_rtos_enter_critical(void);
void liot_rtos_exit_critical(void);
uint32_t liot_rtos_get_running_time(void);
#endif
