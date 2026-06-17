#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "talkPrivate.h"
#include "audioModule.h"

static policy_t g_talkPolicy;

static bool talkPolicyMatch(const event_t *event, const job_t *currentJob, sysState_E sysState)
{
    (void)currentJob;
    (void)sysState;
    return false;
}

static policyResult_E talkPolicyDecide(const event_t *event, const job_t *currentJob, sysState_E sysState, policyAction_t *action)
{
    (void)event;
    (void)sysState;

    if (action == NULL) {
        return POLICY_IGNORE;
    }

    action->nextJobType = JOB_TYPE_TALK;
    return (currentJob != NULL) ? POLICY_ABORT_CURR_AND_START_NEW : POLICY_START_JOB;
}

policy_t *jobGetTalkPolicy(void)
{
    g_talkPolicy.name = "talkPolicy";
    g_talkPolicy.match = talkPolicyMatch;
    g_talkPolicy.decide =talkPolicyDecide;
    return &g_talkPolicy;
}
