#include <stdio.h>
#include "liot_type.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"

/**
 * @brief 定时器元数据。
 */
typedef struct {
    timerId_E timerId;        /**< 定时器标识。 */
} timerMetadata_t;

/** @brief 静态全局定时器数组。 */
static TimerHandle_t g_timers[TIMER_ID_MAX];
/** @brief 静态全局定时器元数据数组。 */
static timerMetadata_t g_timerMetadata[TIMER_ID_MAX];

static void frameworkTimerCallback(TimerHandle_t timer)
{
    timerMetadata_t *metadata = (timerMetadata_t *)pvTimerGetTimerID(timer);
    if (metadata == NULL) {
        return;
    }

    event_t event = {
        .eventId = EVT_TIMER_EXPIRED,
        .arg1 = (uint32_t)metadata->timerId,
        .arg2 = 0U,
        .data = NULL,
        .ownerJobId = 0U,
    };

    (void)frameworkPostEvent(&event);
}

bool frameworkTimerInitAll(void)
{
    for (uint32_t i = 1U; i < (uint32_t)TIMER_ID_MAX; i++) {
        g_timerMetadata[i].timerId = (timerId_E)i;
        g_timers[i] = xTimerCreate("fwTimer",
                                   pdMS_TO_TICKS(1000U),
                                   pdFALSE,
                                   &g_timerMetadata[i],
                                   frameworkTimerCallback);
        if (g_timers[i] == NULL) {
            return false;
        }
    }

    return true;
}

bool frameworkTimerStart(timerId_E timerId, uint32_t timeoutMs)
{
    if ((timerId <= TIMER_ID_NONE) || (timerId >= TIMER_ID_MAX) || (g_timers[timerId] == NULL)) {
        return false;
    }

    if (xTimerChangePeriod(g_timers[timerId], pdMS_TO_TICKS(timeoutMs), portMAX_DELAY) != pdPASS) {
        return false;
    }

    return xTimerStart(g_timers[timerId], portMAX_DELAY) == pdPASS;
}

bool frameworkTimerStop(timerId_E timerId)
{
    if ((timerId <= TIMER_ID_NONE) || (timerId >= TIMER_ID_MAX) || (g_timers[timerId] == NULL)) {
        return false;
    }

    return xTimerStop(g_timers[timerId], portMAX_DELAY) == pdPASS;
}

void frameworkTimerStopAll(void)
{
    for (uint32_t i = 1U; i < (uint32_t)TIMER_ID_MAX; i++) {
        if (g_timers[i] != NULL) {
            (void)xTimerStop(g_timers[i], 0U);
        }
    }
}
