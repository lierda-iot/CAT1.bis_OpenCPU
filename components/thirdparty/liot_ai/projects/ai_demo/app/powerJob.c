#include <stdint.h>
#include <stdbool.h>
#include "aiLog.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"
#include "audioModule.h"
#include "ledModule.h"
#include "powerPrivate.h"

static jobDesc_t g_powerJobDesc = {0};

static int powerJobOnStart(job_t *job, const event_t *triggerEvent)
{
    LOG_DEBUG("Power on job started, event:%u", triggerEvent->eventId);

/*
    event_t localEvent = {0};
    localEvent.eventId = EVT_LED_SET_PATTERN;
    localEvent.arg1 = ledPatternInteractionBlink;
    localEvent.ownerJobId = job->jobId;
    frameworkPostEvent(&localEvent);

    localEvent.eventId = EVT_AUDIO_PLAY;
    localEvent.arg1 = AUDIO_ID_POWER_ON;
    localEvent.ownerJobId = job->jobId;
    frameworkPostEvent(&localEvent);
*/

    return 0;
}

static int powerJobOnEvent(job_t *job, const event_t *event)
{
    LOG_DEBUG("Power on job received event: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);

    /* 等待事件执行结果，触发关机业务 */

    /* 所有事情处理结束后，发送stop_job消息 */
    event_t localEvent = {0};
    localEvent.eventId = EVT_STOP_CURRENT_JOB;
    localEvent.ownerJobId = job->jobId;
    frameworkPostEvent(&localEvent);

    return 0;
}

static int powerJobOnStop(job_t *job, jobStopReason_E reason)
{
    LOG_DEBUG("Power on job stopped, reason=%d", reason);

    return 0;
}

const jobDesc_t *getPowerJobDesc(void)
{
    static const jobOps_t g_poweronJobOps = {
        .onStart = powerJobOnStart,
        .onEvent = powerJobOnEvent,
        .onStop = powerJobOnStop,
    };

    g_powerJobDesc.type = JOB_TYPE_POWER;
    g_powerJobDesc.ops = &g_poweronJobOps;
    g_powerJobDesc.contextSize = 0;
    g_powerJobDesc.jobPolicy = jobGetPowerPolicy();
    return &g_powerJobDesc;
}
