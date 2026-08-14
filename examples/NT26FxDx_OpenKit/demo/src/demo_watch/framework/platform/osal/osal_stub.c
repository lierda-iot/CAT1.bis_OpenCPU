#include "app_osal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *buffer;
    uint32_t depth;
    uint32_t item_size;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} app_stub_queue_t;

int app_os_task_create(app_task_t *task,
                       const char *name,
                       app_task_entry_t entry,
                       void *arg,
                       uint32_t stack_bytes,
                       uint8_t priority)
{
    (void)name;
    (void)entry;
    (void)arg;
    (void)stack_bytes;
    (void)priority;
    if (task == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *task = NULL;
    return APP_ERR_NOT_SUPPORTED;
}

int app_os_task_create_static(app_task_t *task,
                              const char *name,
                              app_task_entry_t entry,
                              void *arg,
                              uint32_t stack_bytes,
                              uint8_t priority,
                              void *stack_mem,
                              void *static_task)
{
    (void)name;
    (void)entry;
    (void)arg;
    (void)stack_bytes;
    (void)priority;
    (void)stack_mem;
    (void)static_task;
    if (task == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *task = NULL;
    return APP_ERR_NOT_SUPPORTED;
}

void app_os_task_delete(app_task_t task)
{
    (void)task;
}

void app_os_task_delay_ms(uint32_t ms)
{
    (void)ms;
}

int app_os_queue_create(app_queue_t *queue, uint32_t depth, uint32_t item_size)
{
    app_stub_queue_t *q;

    if (queue == NULL || depth == 0U || item_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    q = (app_stub_queue_t *)calloc(1U, sizeof(*q));
    if (q == NULL) {
        return APP_ERR_NO_MEMORY;
    }

    q->buffer = (uint8_t *)calloc(depth, item_size);
    if (q->buffer == NULL) {
        free(q);
        return APP_ERR_NO_MEMORY;
    }

    q->depth = depth;
    q->item_size = item_size;
    *queue = q;
    return APP_OK;
}

int app_os_queue_send(app_queue_t queue, const void *item, uint32_t item_size, uint32_t timeout_ms)
{
    app_stub_queue_t *q = (app_stub_queue_t *)queue;

    (void)timeout_ms;
    if (q == NULL || item == NULL || item_size != q->item_size) {
        return APP_ERR_INVALID_ARG;
    }
    if (q->count >= q->depth) {
        return APP_ERR_BUSY;
    }

    memcpy(q->buffer + (q->tail * q->item_size), item, q->item_size);
    q->tail = (q->tail + 1U) % q->depth;
    q->count++;
    return APP_OK;
}

int app_os_queue_recv(app_queue_t queue, void *item, uint32_t item_size, uint32_t timeout_ms)
{
    app_stub_queue_t *q = (app_stub_queue_t *)queue;

    (void)timeout_ms;
    if (q == NULL || item == NULL || item_size != q->item_size) {
        return APP_ERR_INVALID_ARG;
    }
    if (q->count == 0U) {
        return APP_ERR_TIMEOUT;
    }

    memcpy(item, q->buffer + (q->head * q->item_size), q->item_size);
    q->head = (q->head + 1U) % q->depth;
    q->count--;
    return APP_OK;
}

int app_os_mutex_create(app_mutex_t *mutex)
{
    if (mutex == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *mutex = (void *)1;
    return APP_OK;
}

int app_os_mutex_lock(app_mutex_t mutex, uint32_t timeout_ms)
{
    (void)timeout_ms;
    return (mutex == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

int app_os_mutex_unlock(app_mutex_t mutex)
{
    return (mutex == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

int app_os_sem_create(app_sem_t *sem, uint32_t initial_count)
{
    if (sem == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *sem = (void *)(uintptr_t)(initial_count + 1U);
    return APP_OK;
}

int app_os_sem_wait(app_sem_t sem, uint32_t timeout_ms)
{
    (void)timeout_ms;
    return (sem == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

int app_os_sem_release(app_sem_t sem)
{
    return (sem == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

int app_os_timer_create(app_timer_t *timer, app_timer_type_t type, app_timer_cb_t cb, void *arg)
{
    (void)type;
    (void)cb;
    (void)arg;
    if (timer == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    *timer = (void *)1;
    return APP_OK;
}

int app_os_timer_start(app_timer_t timer, uint32_t timeout_ms)
{
    (void)timeout_ms;
    return (timer == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

int app_os_timer_stop(app_timer_t timer)
{
    return (timer == NULL) ? APP_ERR_INVALID_ARG : APP_OK;
}

void *app_os_malloc(size_t size)
{
    return malloc(size);
}

void app_os_free(void *ptr)
{
    free(ptr);
}

void app_log(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void app_os_log_current_task(const char *tag)
{
    app_log("%s task=stub hwm=0 prio=0 handle=0",
            (tag != NULL) ? tag : "task");
}
