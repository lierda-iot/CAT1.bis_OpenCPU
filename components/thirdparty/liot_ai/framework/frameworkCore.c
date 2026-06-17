#include <string.h>

#include "liot_type.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "aiLog.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"


#define FRAMEWORK_MAX_POLICIES         (8U)
#define FRAMEWORK_MAX_JOB_TYPES        (8U)
#define FRAMEWORK_MAX_SIDE_JOBS        (8U)
#define FRAMEWORK_EVENT_QUEUE_LENGTH   (16U)
#define FRAMEWORK_DELAYED_QUEUE_LENGTH (8U)
#define FRAMEWORK_TASK_STACK_SIZE      (4096U)
#define FRAMEWORK_TASK_PRIORITY        (6U)
#define FRAMEWORK_JOB_STORAGE_SIZE     (256U)

/**
 * @brief 框架全局上下文。
 */
typedef struct {
    QueueHandle_t eventQueue;                                  /**< 主事件队列。 */
    QueueHandle_t delayedQueue;                                /**< 延迟事件队列。 */
    TaskHandle_t frameworkTaskHandle;                          /**< 框架任务句柄。 */
    policy_t *globalPolicies[FRAMEWORK_MAX_POLICIES];          /**< 全局 policy 注册表。 */
    uint32_t globalPolicyCount;                                /**< 全局 policy 数量。 */
    const jobDesc_t *jobTable[FRAMEWORK_MAX_JOB_TYPES];        /**< job 注册表。 */
    const jobDesc_t *sideJobTable[FRAMEWORK_MAX_SIDE_JOBS];    /**< side job 注册表。 */
    job_t currentJob;                                          /**< 当前 job。 */
    uint8_t currentJobStorage[FRAMEWORK_JOB_STORAGE_SIZE];     /**< 当前 job 的上下文缓冲区。 */
    bool hasCurrentJob;                                        /**< 是否存在当前 job。 */
    sysState_E sysState;                                       /**< 当前系统状态。 */
    uint32_t nextJobId;                                        /**< 下一个 job 标识。 */
} frameworkContext_t;

/** @brief 静态全局框架上下文。 */
static frameworkContext_t g_framework;

static void frameworkTask(void *argument);
static void frameworkHandleEvent(const event_t *event);
static bool frameworkPostDelayedEvent(const event_t *event);
static void frameworkFlushDelayedEvents(void);
static const jobDesc_t *frameworkFindJobDesc(jobType_E jobType);
static bool frameworkIsEventOwnedByCurrentJob(const event_t *event);
static bool frameworkTryHandleCurrentJobPolicy(const event_t *event);
static bool frameworkTryHandleGlobalPolicies(const event_t *event);
static void frameworkHandlePolicyResult(policyResult_E result,
                                        const policyAction_t *action,
                                        const event_t *event,
                                        const job_t *currentJob);

bool frameworkInit(void)
{
    (void)memset(&g_framework, 0, sizeof(g_framework));
    g_framework.sysState = SYS_STATE_IDLE;
    g_framework.nextJobId = 1U;

    g_framework.eventQueue = xQueueCreate(FRAMEWORK_EVENT_QUEUE_LENGTH, sizeof(event_t));
    g_framework.delayedQueue = xQueueCreate(FRAMEWORK_DELAYED_QUEUE_LENGTH, sizeof(event_t));
    if ((g_framework.eventQueue == NULL) || (g_framework.delayedQueue == NULL)) {
        return false;
    }

    return frameworkTimerInitAll();
}

bool frameworkStart(void)
{
    return xTaskCreate(frameworkTask,
                       "frameworkTask",
                       FRAMEWORK_TASK_STACK_SIZE,
                       NULL,
                       FRAMEWORK_TASK_PRIORITY,
                       &g_framework.frameworkTaskHandle) == pdPASS;
}

bool frameworkRegisterPolicy(policy_t *policy)
{
    if ((policy == NULL) || (policy->match == NULL) || (policy->decide == NULL)) {
        return false;
    }

    if (g_framework.globalPolicyCount >= FRAMEWORK_MAX_POLICIES) {
        return false;
    }

    g_framework.globalPolicies[g_framework.globalPolicyCount++] = policy;
    return true;
}

bool frameworkRegisterSideJob(const jobDesc_t *jobDesc)
{
    if ((jobDesc == NULL) || (jobDesc->ops == NULL)) {
        return false;
    }

    for (uint32_t i = 0U; i < FRAMEWORK_MAX_SIDE_JOBS; i++) {
        if (g_framework.sideJobTable[i] == NULL) {
            g_framework.sideJobTable[i] = jobDesc;
            if (jobDesc->ops->onInit != NULL) {
                (void)jobDesc->ops->onInit();
            }
            return true;
        }
    }

    return false;
}

bool frameworkUnregisterSideJob(const jobDesc_t *jobDesc)
{
    if (jobDesc == NULL) {
        return false;
    }

    for (uint32_t i = 0U; i < FRAMEWORK_MAX_SIDE_JOBS; i++) {
        if (g_framework.sideJobTable[i] == jobDesc) {
            if (jobDesc->ops != NULL && jobDesc->ops->onDeInit != NULL) {
                (void)jobDesc->ops->onDeInit();
            }
            g_framework.sideJobTable[i] = NULL;
            return true;
        }
    }

    return false;
}

bool frameworkRegisterJob(const jobDesc_t *jobDesc)
{
    if ((jobDesc == NULL) || (jobDesc->type <= JOB_TYPE_NONE) || (jobDesc->type >= FRAMEWORK_MAX_JOB_TYPES)) {
        return false;
    }

    if (jobDesc->ops == NULL) {
        return false;
    }

    g_framework.jobTable[jobDesc->type] = jobDesc;

    if (jobDesc->ops->onInit != NULL) {
        (void)jobDesc->ops->onInit();
    }

    return true;
}

bool frameworkUnregisterJob(jobType_E jobType)
{
    const jobDesc_t *jobDesc = frameworkFindJobDesc(jobType);
    if (jobDesc == NULL) {
        return false;
    }

    if (jobDesc->ops->onDeInit != NULL) {
        (void)jobDesc->ops->onDeInit();
    }

    g_framework.jobTable[jobType] = NULL;
    return true;
}

bool frameworkPostEvent(const event_t *event)
{
    if ((event == NULL) || (g_framework.eventQueue == NULL)) {
        return false;
    }

    if (xPortIsInsideInterrupt()) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        BaseType_t result = xQueueSendFromISR(g_framework.eventQueue, event, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
        return result == pdPASS;
    }

    return xQueueSend(g_framework.eventQueue, event, 0U) == pdPASS;
}

sysState_E frameworkGetSysState(void)
{
    return g_framework.sysState;
}

const job_t *frameworkGetCurrentJob(void)
{
    return g_framework.hasCurrentJob ? &g_framework.currentJob : NULL;
}

void frameworkSetSysState(sysState_E sysState)
{
    g_framework.sysState = sysState;
}

static const jobDesc_t *frameworkFindJobDesc(jobType_E jobType)
{
    uint32_t i = 0;
    if ((jobType <= JOB_TYPE_NONE) || (jobType >= FRAMEWORK_MAX_JOB_TYPES)) {
        return NULL;
    }

    for(i=0; i<FRAMEWORK_MAX_JOB_TYPES; i++) {
        if (g_framework.jobTable[i] != NULL && g_framework.jobTable[i]->type == jobType) {
            return g_framework.jobTable[i];
        }
    }

    return NULL;
}

bool frameworkStartJob(jobType_E jobType, const event_t *triggerEvent)
{
    const jobDesc_t *jobDesc = frameworkFindJobDesc(jobType);
    if (jobDesc == NULL) {
        LOG_ERROR("Failed to find job description for type: %d", jobType);
        return false;
    }

    (void)memset(&g_framework.currentJob, 0, sizeof(g_framework.currentJob));
    (void)memset(g_framework.currentJobStorage, 0, sizeof(g_framework.currentJobStorage));

    if (jobDesc->contextSize > sizeof(g_framework.currentJobStorage)) {
        return false;
    }

    g_framework.currentJob.jobId = g_framework.nextJobId++;
    g_framework.currentJob.type = jobType;
    g_framework.currentJob.ops = jobDesc->ops;
    g_framework.currentJob.context = g_framework.currentJobStorage;
    g_framework.hasCurrentJob = true;
    g_framework.sysState = SYS_STATE_ACTIVE;

    if (g_framework.currentJob.ops->onStart(&g_framework.currentJob, triggerEvent) != 0) {
        (void)memset(&g_framework.currentJob, 0, sizeof(g_framework.currentJob));
        g_framework.hasCurrentJob = false;
        g_framework.sysState = SYS_STATE_IDLE;
        return false;
    }

    return true;
}

void frameworkStopCurrentJob(jobStopReason_E reason)
{
    if (!g_framework.hasCurrentJob) {
        return;
    }

    g_framework.currentJob.ops->onStop(&g_framework.currentJob, reason);
    frameworkTimerStopAll();

    (void)memset(&g_framework.currentJob, 0, sizeof(g_framework.currentJob));
    (void)memset(g_framework.currentJobStorage, 0, sizeof(g_framework.currentJobStorage));
    g_framework.hasCurrentJob = false;
    g_framework.sysState = SYS_STATE_IDLE;
}

static bool frameworkPostDelayedEvent(const event_t *event)
{
    return xQueueSend(g_framework.delayedQueue, event, 0U) == pdPASS;
}

static void frameworkFlushDelayedEvents(void)
{
    event_t delayedEvent;

    while (xQueueReceive(g_framework.delayedQueue, &delayedEvent, 0U) == pdPASS) {
        frameworkHandleEvent(&delayedEvent);
        if (g_framework.hasCurrentJob) {
            break;
        }
    }
}

static bool frameworkIsEventOwnedByCurrentJob(const event_t *event)
{
    if ((event == NULL) || !g_framework.hasCurrentJob) {
        return false;
    }

    if (event->ownerJobId == 0U) {
        return true;
    }

    return event->ownerJobId == g_framework.currentJob.jobId;
}

static void frameworkHandlePolicyResult(policyResult_E result, const policyAction_t *action, const event_t *event, const job_t *currentJob)
{
    LOG_DEBUG("Handling policy result: %d for event: id=%u, arg1=%u, arg2=%u", result, event->eventId, event->arg1, event->arg2);
    switch (result) {
    case POLICY_IGNORE:
        break;

    case POLICY_START_JOB:
        if (action != NULL) {
            (void)frameworkStartJob(action->nextJobType, event);
        }
        break;
    case POLICY_STOP_JOB:
        if (g_framework.hasCurrentJob) {
            frameworkStopCurrentJob(STOP_REASON_COMPLETED);
        }
        break;

    case POLICY_POST_CURR_JOB:
        if (g_framework.hasCurrentJob && frameworkIsEventOwnedByCurrentJob(event)) {
            (void)g_framework.currentJob.ops->onEvent(&g_framework.currentJob, event);
        }
        break;

    case POLICY_ABORT_CURR_AND_START_NEW:
        if (g_framework.hasCurrentJob) {
            frameworkStopCurrentJob(STOP_REASON_REPLACED);
        }
        if (action != NULL) {
            (void)frameworkStartJob(action->nextJobType, event);
        }
        break;

    case POLICY_DELAY:
        (void)frameworkPostDelayedEvent(event);
        break;

    case POLICY_DIRECT_HANDLE:
        if ((action != NULL) && (action->directHandler != NULL)) {
            (void)action->directHandler(event, currentJob);
        }
        break;

    default:
        break;
    }
}

static bool frameworkTryHandleCurrentJobPolicy(const event_t *event)
{
    const jobDesc_t *jobDesc;
    policy_t *jobPolicy;
    policyAction_t action;
    policyResult_E result;

    if (!g_framework.hasCurrentJob) {
        LOG_INFO("No current job to handle event: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);
        return false;
    }

    if (!frameworkIsEventOwnedByCurrentJob(event)) {
        LOG_INFO("Event not owned by current job, ignoring: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);
        return false;
    }

    jobDesc = frameworkFindJobDesc(g_framework.currentJob.type);
    if ((jobDesc == NULL) || (jobDesc->jobPolicy == NULL)) {
        LOG_WARN("Current job has no policy to handle event: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);
        return false;
    }

    jobPolicy = jobDesc->jobPolicy;
    if (!jobPolicy->match(event, &g_framework.currentJob, g_framework.sysState)) {
        LOG_INFO("Current job policy does not match event, ignoring: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);
        return false;
    }

    (void)memset(&action, 0, sizeof(action));
    result = jobPolicy->decide(event, &g_framework.currentJob, g_framework.sysState, &action);
    frameworkHandlePolicyResult(result, &action, event, &g_framework.currentJob);

    return true;
}

static bool frameworkTryHandleGlobalPolicies(const event_t *event)
{
    policyResult_E result;
    policyAction_t action;

    const job_t *currentJob = g_framework.hasCurrentJob ? &g_framework.currentJob : NULL;

    for (uint32_t i = 0U; i < g_framework.globalPolicyCount; i++) {
        policy_t *policy = g_framework.globalPolicies[i];
        LOG_DEBUG("Trying global policy: %s for event: id=%u, arg1=%u, arg2=%u", policy->name, event->eventId, event->arg1, event->arg2);
        if ((policy == NULL) || !policy->match(event, currentJob, g_framework.sysState)) {
            continue;
        }

        (void)memset(&action, 0, sizeof(action));
        result = policy->decide(event, currentJob, g_framework.sysState, &action);
        frameworkHandlePolicyResult(result, &action, event, currentJob);

        return true;
    }

    return false;
}

static void frameworkHandleEvent(const event_t *event)
{
    if (event == NULL) {
        return;
    }

    if (frameworkTryHandleCurrentJobPolicy(event)) {
        return;
    }

    if (frameworkTryHandleGlobalPolicies(event)) {
        return;
    }

    LOG_WARN("Event not handled: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);
}

static void frameworkDispatchSideJobs(const event_t *event)
{
    const job_t *currentJob = g_framework.hasCurrentJob ? &g_framework.currentJob : NULL;

    for (uint32_t i = 0U; i < FRAMEWORK_MAX_SIDE_JOBS; i++) {
        const jobDesc_t *desc = g_framework.sideJobTable[i];
        if (desc == NULL || desc->ops == NULL || desc->ops->onEvent == NULL) {
            continue;
        }
        if (desc->jobPolicy != NULL && desc->jobPolicy->match != NULL) {
            if (!desc->jobPolicy->match(event, currentJob, g_framework.sysState)) {
                continue;
            }
        }
        (void)desc->ops->onEvent(NULL, event);
    }
}

static void frameworkTask(void *argument)
{
    event_t event;
    (void)argument;

    while (1) {
        if (xQueueReceive(g_framework.eventQueue, &event, portMAX_DELAY) == pdPASS) {
            frameworkHandleEvent(&event);
            frameworkDispatchSideJobs(&event);

            if (!g_framework.hasCurrentJob) {
                frameworkFlushDelayedEvents();
            }
        }
    }
}
