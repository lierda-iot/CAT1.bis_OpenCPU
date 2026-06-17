#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "aiLog.h"
#include "frameworkTypes.h"
#include "audioModule.h"
#include "keyModule.h"

static policy_t g_globalPolicy = {0};

static bool globalPolicyMatch(const event_t *event, const job_t *currentJob, sysState_E sysState)
{
    bool iret = false;

    switch (event->eventId) {
        case EVT_KEY_POWER_ON:
        case EVT_KEY_POWER_OFF:
            iret = true;
            break;
        default:
            break;
    }
    LOG_DEBUG("iret:%d", iret);
    return iret;
}

static policyResult_E globalPolicyDecide(const event_t *event, const job_t *currentJob, sysState_E sysState, policyAction_t *action)
{
    policyResult_E iret = POLICY_IGNORE;

    if (action == NULL) {
        return POLICY_IGNORE;
    }

    switch (event->eventId) {
        case EVT_KEY_POWER_ON:
        case EVT_KEY_POWER_OFF:
            action->nextJobType = JOB_TYPE_POWER;
            iret = POLICY_START_JOB;
            break;

        default:
            break;
    }
    LOG_DEBUG("iret:%d", iret);
    return iret;
}

policy_t *globalPolicy(void)
{
    g_globalPolicy.name = "globalPolicy";
    g_globalPolicy.match = globalPolicyMatch;
    g_globalPolicy.decide = globalPolicyDecide;
    return &g_globalPolicy;
}
