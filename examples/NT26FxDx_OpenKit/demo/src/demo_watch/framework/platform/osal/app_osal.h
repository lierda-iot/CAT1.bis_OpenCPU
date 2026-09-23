#ifndef APP_OSAL_H
#define APP_OSAL_H

#include <stddef.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_OS_NO_WAIT      0U
#define APP_OS_WAIT_FOREVER 0xffffffffU

typedef void *app_task_t;
typedef void *app_queue_t;
typedef void *app_mutex_t;
typedef void *app_sem_t;
typedef void *app_timer_t;

typedef enum {
    APP_TIMER_ONCE = 0,
    APP_TIMER_PERIODIC,
} app_timer_type_t;

typedef void (*app_task_entry_t)(void *arg);
typedef void (*app_timer_cb_t)(void *arg);

int app_os_task_create(app_task_t *task,
                       const char *name,
                       app_task_entry_t entry,
                       void *arg,
                       uint32_t stack_bytes,
                       uint8_t priority);
int app_os_task_create_static(app_task_t *task,
                              const char *name,
                              app_task_entry_t entry,
                              void *arg,
                              uint32_t stack_bytes,
                              uint8_t priority,
                              void *stack_mem,
                              void *static_task);
void app_os_task_delete(app_task_t task);
void app_os_task_delay_ms(uint32_t ms);

int app_os_queue_create(app_queue_t *queue, uint32_t depth, uint32_t item_size);
int app_os_queue_send(app_queue_t queue, const void *item, uint32_t item_size, uint32_t timeout_ms);
int app_os_queue_recv(app_queue_t queue, void *item, uint32_t item_size, uint32_t timeout_ms);

int app_os_mutex_create(app_mutex_t *mutex);
int app_os_mutex_lock(app_mutex_t mutex, uint32_t timeout_ms);
int app_os_mutex_unlock(app_mutex_t mutex);

int app_os_sem_create(app_sem_t *sem, uint32_t initial_count);
int app_os_sem_wait(app_sem_t sem, uint32_t timeout_ms);
int app_os_sem_release(app_sem_t sem);

int app_os_timer_create(app_timer_t *timer, app_timer_type_t type, app_timer_cb_t cb, void *arg);
int app_os_timer_start(app_timer_t timer, uint32_t timeout_ms);
int app_os_timer_stop(app_timer_t timer);

void *app_os_malloc(size_t size);
void app_os_free(void *ptr);
void app_log(const char *fmt, ...);
void app_os_log_current_task(const char *tag);

#ifdef __cplusplus
}
#endif

#endif /* APP_OSAL_H */
