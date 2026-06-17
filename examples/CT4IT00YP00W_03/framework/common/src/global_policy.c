#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"
#include "app_nv.h"
#include "app_framework.h"

static policy_t g_globalPolicy;

static bool globalPolicyMatch(const event_t *event, const job_t *currentJob, sysState_E sysState)
{
    (void)currentJob;
    (void)sysState;
    switch (event->eventId) {
    case EVT_KEY_WAKEUP:
    case EVT_AUDIO_WAKEUP:
    case EVT_KEY_POWER_ON:
    case EVT_KEY_POWER_OFF:
    case EVT_LOW_POWER:
    case EVT_SYSTEM_SLEEP:
    case EVT_NETWORK_READY:
    case EVT_NETWORK_FAIL:
    case EVT_SIM_ERROR:
    case EVT_TIMER_EXPIRED:
    case EVT_AUDIO_RECORD_DONE:
    case EVT_AUDIO_PLAY_START:
    case EVT_AUDIO_PLAY_DONE:
    case EVT_AI_RESPONSE_DONE:
    case EVT_AI_CONNECT_REQ:
    case EVT_AI_CONNECTED:
    case EVT_AI_CONNECT_FAIL:
    case EVT_AI_DISCONNECTED:
        return true;
    default:
        return false;
    }
}

static policyResult_E globalPolicyDecide(const event_t *event, const job_t *currentJob,
                                         sysState_E sysState, policyAction_t *action)
{
    (void)sysState;

    switch (event->eventId) {
    case EVT_KEY_WAKEUP:
    case EVT_AI_CONNECT_REQ:
        if (currentJob != NULL && currentJob->type == JOB_TYPE_TALK)
            return POLICY_POST_CURR_JOB;
        action->nextJobType = JOB_TYPE_TALK;
        return POLICY_ABORT_CURR_AND_START_NEW;

    case EVT_KEY_POWER_ON:
    case EVT_KEY_POWER_OFF:
    case EVT_LOW_POWER:
    case EVT_SYSTEM_SLEEP:
        action->nextJobType = JOB_TYPE_KEY;
        return POLICY_ABORT_CURR_AND_START_NEW;

    case EVT_NETWORK_READY:
    case EVT_NETWORK_FAIL:
    case EVT_SIM_ERROR:
        if (currentJob != NULL &&
            (currentJob->type == JOB_TYPE_NETWORK || currentJob->type == JOB_TYPE_TALK))
            return POLICY_IGNORE;
        action->nextJobType = JOB_TYPE_NETWORK;
        return POLICY_ABORT_CURR_AND_START_NEW;

    case EVT_AUDIO_WAKEUP:
    case EVT_TIMER_EXPIRED:
    case EVT_AUDIO_RECORD_DONE:
    case EVT_AUDIO_PLAY_START:
    case EVT_AUDIO_PLAY_DONE:
    case EVT_AI_RESPONSE_DONE:
    case EVT_AI_CONNECTED:
    case EVT_AI_CONNECT_FAIL:
    case EVT_AI_DISCONNECTED:
        return POLICY_POST_CURR_JOB;

    default:
        return POLICY_IGNORE;
    }
}

policy_t *globalPolicy(void)
{
    g_globalPolicy.name = "globalPolicy";
    g_globalPolicy.match = globalPolicyMatch;
    g_globalPolicy.decide = globalPolicyDecide;
    return &g_globalPolicy;
}
