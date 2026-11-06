#ifndef PRO_ROOM_HOST_OS_H
#define PRO_ROOM_HOST_OS_H
#include <stdint.h>
typedef void *liot_task_t;
typedef void *liot_sem_t;
#define LIOT_APP_TASK_PRIORITY 10
#define LIOT_OSI_SUCCESS 0
#define LIOT_WAIT_FOREVER UINT32_MAX
void liot_rtos_enter_critical(void);
void liot_rtos_exit_critical(void);
uint32_t liot_rtos_get_running_time(void);
int liot_rtos_semaphore_create(liot_sem_t *, uint32_t);
int liot_rtos_semaphore_wait(liot_sem_t, uint32_t);
int liot_rtos_semaphore_release(liot_sem_t);
int liot_rtos_task_create(liot_task_t *, uint32_t, uint32_t,
                          const char *, void (*)(void *), void *);
#endif
