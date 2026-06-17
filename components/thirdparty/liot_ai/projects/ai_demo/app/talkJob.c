#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"
#include "talkPrivate.h"
#include "audioModule.h"
#include "ledModule.h"

typedef enum {
    talkStateIdle = 0,              /**< 初始状态。 */
    talkStatePlayingPrompt,         /**< 播放唤醒提示音。 */
    talkStateListening,             /**< 进入聆听状态。 */
    talkStateStandbyPrompt,         /**< 播放待机提示音。 */
} talkState_t;

typedef struct {
    talkState_t state;              /**< 当前状态。 */
} talkContext_t;

static jobDesc_t g_talkJobDesc;

static int talkJobOnStart(job_t *job, const event_t *triggerEvent)
{
    talkContext_t *context = (talkContext_t *)job->context;
    (void)triggerEvent;

    context->state = talkStatePlayingPrompt;
    frameworkSetSysState(SYS_STATE_ACTIVE);

    /* TODO 实现对话逻辑 */
    return 0;
}

static int talkJobOnEvent(job_t *job, const event_t *event)
{
    talkContext_t *context = (talkContext_t *)job->context;

    /* TODO 实现对话状态机逻辑 */
    switch (context->state) {
    case talkStatePlayingPrompt:

        break;

    case talkStateListening:

        break;

    case talkStateStandbyPrompt:

        break;

    default:
        break;
    }

    return 0;
}

static int talkJobOnStop(job_t *job, jobStopReason_E reason)
{
    (void)reason;
    ledModuleSetPattern(job->jobId, ledPatternOff);
    audioModuleStop(job->jobId);
    audioModuleCloseCapture(job->jobId);
    frameworkSetSysState(SYS_STATE_IDLE);
    return 0;
}

const jobDesc_t *getTalkJobDesc(void)
{
    static const jobOps_t g_talkJobOps = {
        .onStart = talkJobOnStart,
        .onEvent = talkJobOnEvent,
        .onStop = talkJobOnStop,
    };

    g_talkJobDesc.type = JOB_TYPE_TALK;
    g_talkJobDesc.ops = &g_talkJobOps;
    g_talkJobDesc.contextSize = sizeof(talkContext_t);
    g_talkJobDesc.jobPolicy = jobGetTalkPolicy();
    return &g_talkJobDesc;
}
