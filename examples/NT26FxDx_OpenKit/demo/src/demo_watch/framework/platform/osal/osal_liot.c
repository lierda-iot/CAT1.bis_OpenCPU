#include "app_osal.h"

#include <stdarg.h>
#include <stdio.h>

#include "liot_log.h"
#include "liot_os.h"

static uint32_t app_os_to_liot_timeout(uint32_t timeout_ms)
{
    if (timeout_ms == APP_OS_WAIT_FOREVER) {
        return LIOT_WAIT_FOREVER;
    }
    if (timeout_ms == APP_OS_NO_WAIT) {
        return LIOT_NO_WAIT;
    }
    return timeout_ms;
}

static int app_os_status_to_err(LiotOSStatus_t status)
{
    return (status == LIOT_OSI_SUCCESS) ? APP_OK : APP_ERR_FAIL;
}

int app_os_task_create(app_task_t *task,
                       const char *name,
                       app_task_entry_t entry,
                       void *arg,
                       uint32_t stack_bytes,
                       uint8_t priority)
{
    if (task == NULL || name == NULL || entry == NULL || stack_bytes == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_task_create((liot_task_t *)task,
                                                      stack_bytes,
                                                      priority,
                                                      (char *)name,
                                                      entry,
                                                      arg));
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
    if (task == NULL || name == NULL || entry == NULL ||
        stack_bytes == 0U || stack_mem == NULL || static_task == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_task_create_static((liot_task_t *)task,
                                                             stack_bytes,
                                                             priority,
                                                             (char *)name,
                                                             entry,
                                                             stack_mem,
                                                             static_task,
                                                             arg));
}

void app_os_task_delete(app_task_t task)
{
    (void)liot_rtos_task_delete((liot_task_t)task);
}

void app_os_task_delay_ms(uint32_t ms)
{
    liot_rtos_task_sleep_ms(ms);
}

int app_os_queue_create(app_queue_t *queue, uint32_t depth, uint32_t item_size)
{
    if (queue == NULL || depth == 0U || item_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_queue_create((liot_queue_t *)queue,
                                                       item_size,
                                                       depth));
}

int app_os_queue_send(app_queue_t queue, const void *item, uint32_t item_size, uint32_t timeout_ms)
{
    LiotOSStatus_t status;

    if (queue == NULL || item == NULL || item_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    status = liot_rtos_queue_release((liot_queue_t)queue,
                                     item_size,
                                     (uint8 *)item,
                                     app_os_to_liot_timeout(timeout_ms));
    if (status == LIOT_OSI_SUCCESS) {
        return APP_OK;
    }
    return (timeout_ms == APP_OS_NO_WAIT) ? APP_ERR_BUSY : APP_ERR_TIMEOUT;
}

int app_os_queue_recv(app_queue_t queue, void *item, uint32_t item_size, uint32_t timeout_ms)
{
    LiotOSStatus_t status;

    if (queue == NULL || item == NULL || item_size == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    status = liot_rtos_queue_wait((liot_queue_t)queue,
                                  (uint8 *)item,
                                  item_size,
                                  app_os_to_liot_timeout(timeout_ms));
    if (status == LIOT_OSI_SUCCESS) {
        return APP_OK;
    }
    return (timeout_ms == APP_OS_WAIT_FOREVER) ? APP_ERR_FAIL : APP_ERR_TIMEOUT;
}

int app_os_mutex_create(app_mutex_t *mutex)
{
    if (mutex == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_mutex_create((liot_mutex_t *)mutex));
}

int app_os_mutex_lock(app_mutex_t mutex, uint32_t timeout_ms)
{
    LiotOSStatus_t status;

    if (mutex == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    status = liot_rtos_mutex_lock((liot_mutex_t)mutex, app_os_to_liot_timeout(timeout_ms));
    if (status == LIOT_OSI_SUCCESS) {
        return APP_OK;
    }
    return (timeout_ms == APP_OS_WAIT_FOREVER) ? APP_ERR_FAIL : APP_ERR_TIMEOUT;
}

int app_os_mutex_unlock(app_mutex_t mutex)
{
    if (mutex == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_mutex_unlock((liot_mutex_t)mutex));
}

int app_os_sem_create(app_sem_t *sem, uint32_t initial_count)
{
    if (sem == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_semaphore_create((liot_sem_t *)sem, initial_count));
}

int app_os_sem_wait(app_sem_t sem, uint32_t timeout_ms)
{
    LiotOSStatus_t status;

    if (sem == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    status = liot_rtos_semaphore_wait((liot_sem_t)sem, app_os_to_liot_timeout(timeout_ms));
    if (status == LIOT_OSI_SUCCESS) {
        return APP_OK;
    }
    return (timeout_ms == APP_OS_WAIT_FOREVER) ? APP_ERR_FAIL : APP_ERR_TIMEOUT;
}

int app_os_sem_release(app_sem_t sem)
{
    if (sem == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_semaphore_release((liot_sem_t)sem));
}

int app_os_timer_create(app_timer_t *timer, app_timer_type_t type, app_timer_cb_t cb, void *arg)
{
    liot_timertype_e liot_type;

    if (timer == NULL || cb == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    liot_type = (type == APP_TIMER_PERIODIC) ? LIOT_TimerPeriodic : LIOT_TimerOnce;
    return app_os_status_to_err(liot_rtos_timer_create((liot_timer_t *)timer,
                                                       liot_type,
                                                       cb,
                                                       arg));
}

int app_os_timer_start(app_timer_t timer, uint32_t timeout_ms)
{
    if (timer == NULL || timeout_ms == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_timer_start((liot_timer_t)timer, timeout_ms));
}

int app_os_timer_stop(app_timer_t timer)
{
    if (timer == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    return app_os_status_to_err(liot_rtos_timer_stop((liot_timer_t)timer));
}

void *app_os_malloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }
    return liot_rtos_malloc(size);
}

void app_os_free(void *ptr)
{
    if (ptr != NULL) {
        liot_rtos_free(ptr);
    }
}

void app_log(const char *fmt, ...)
{
    char buf[192];
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    liot_trace("%s", buf);
}

void app_os_log_current_task(const char *tag)
{
    liot_task_status_s status = {0};
    LiotOSStatus_t ret;

    ret = liot_rtos_task_get_status(NULL, &status);
    if (ret != LIOT_OSI_SUCCESS) {
        app_log("%s task status failed: %d", (tag != NULL) ? tag : "task", (int)ret);
        return;
    }

    app_log("%s task=%s hwm=%u prio=%lu handle=%p",
            (tag != NULL) ? tag : "task",
            (status.pcTaskName != NULL) ? status.pcTaskName : "?",
            (unsigned int)status.usStackHighWaterMark,
            status.uxCurrentPriority,
            (void *)status.xHandle);
}
