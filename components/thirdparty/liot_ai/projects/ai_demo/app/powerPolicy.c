#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "aiLog.h"
#include "frameworkTypes.h"
#include "keyModule.h"
#include "powerPrivate.h"


static policy_t g_powerPolicy = {0};

static bool powerPolicyMatch(const event_t *event, const job_t *currentJob, sysState_E sysState)
{
    bool iret = false;

    LOG_DEBUG("Power policy match event: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);

    switch (event->eventId) {
        default:
            break;
    }

    LOG_DEBUG("iret:%d", iret);
    return iret;
}

static policyResult_E powerPolicyDecide(const event_t *event, const job_t *currentJob, sysState_E sysState, policyAction_t *action)
{
    policyResult_E result = POLICY_IGNORE;

    LOG_DEBUG("Power policy decide event: id=%u, arg1=%u, arg2=%u", event->eventId, event->arg1, event->arg2);

    switch (event->eventId) {
        default:
            break;
    }

    return result;
}

policy_t *jobGetPowerPolicy(void)
{
    g_powerPolicy.name = "powerPolicy";
    g_powerPolicy.match = powerPolicyMatch;
    g_powerPolicy.decide =powerPolicyDecide;
    return &g_powerPolicy;
}
