#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "hw_audio.h"
#include "hw_network.h"
#include "app_nv.h"

static jobDesc_t g_networkJobDesc;

static int networkJobInit(void)
{
    app_nv_data_t *nv = app_nv_get();
    networkModuleInit(&nv->net_cfg);
    return 0;
}

static int networkJobOnStart(job_t *job, const event_t *triggerEvent)
{
    (void)job;

    switch (triggerEvent->eventId) {
    case EVT_NETWORK_READY:
        audioModulePlayPrompt(AUDIO_PROMPT_CONNECTED);
        {
            event_t evt = {.eventId = EVT_AI_CONNECT_REQ};
            frameworkPostEvent(&evt);
        }
        break;
    case EVT_NETWORK_FAIL:
        audioModulePlayPrompt(AUDIO_PROMPT_CONNECTFAIL);
        break;
    case EVT_SIM_ERROR:
        audioModulePlayPrompt(AUDIO_PROMPT_SIM_ERROR);
        break;
    default:
        break;
    }

    return 0;
}

static int networkJobOnStop(job_t *job, jobStopReason_E reason)
{
    (void)job;
    (void)reason;
    return 0;
}

const jobDesc_t *getNetworkJobDesc(void)
{
    static const jobOps_t ops = {
        .onInit   = networkJobInit,
        .onStart  = networkJobOnStart,
        .onEvent  = NULL,
        .onStop   = networkJobOnStop,
    };
    g_networkJobDesc.type        = JOB_TYPE_NETWORK;
    g_networkJobDesc.ops         = &ops;
    g_networkJobDesc.contextSize = 0;
    g_networkJobDesc.jobPolicy   = NULL;
    return &g_networkJobDesc;
}
