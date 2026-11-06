#ifndef PRO_AUDIO_AEC_HOST_OS_H
#define PRO_AUDIO_AEC_HOST_OS_H
#include <stdint.h>
typedef int LiotOSStatus_t;
typedef void *liot_task_t;
typedef void *liot_mutex_t;
#define LIOT_OSI_SUCCESS 0
#define APP_PRIORITY_NORMAL 24
void liot_rtos_enter_critical(void);
void liot_rtos_exit_critical(void);
void liot_rtos_task_sleep_ms(uint32_t ms);
uint32_t liot_rtos_get_running_time(void);
LiotOSStatus_t liot_rtos_mutex_create(liot_mutex_t *mutex);
LiotOSStatus_t liot_rtos_mutex_lock(liot_mutex_t mutex, uint32_t timeout);
LiotOSStatus_t liot_rtos_mutex_unlock(liot_mutex_t mutex);
LiotOSStatus_t liot_rtos_task_create(liot_task_t *task, uint32_t stack_bytes,
    uint8_t priority, char *name, void (*entry)(void *), void *argument, ...);
#endif
