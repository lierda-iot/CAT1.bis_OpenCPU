#ifndef FRAMEWORK_CORE_H
#define FRAMEWORK_CORE_H

bool frameworkInit(void);
bool frameworkStart(void);

bool frameworkRegisterPolicy(policy_t *policy);
bool frameworkRegisterJob(const jobDesc_t *jobDesc);
bool frameworkUnregisterJob(jobType_E jobType);
bool frameworkRegisterSideJob(const jobDesc_t *jobDesc);
bool frameworkUnregisterSideJob(const jobDesc_t *jobDesc);
bool frameworkPostEvent(const event_t *event);
sysState_E frameworkGetSysState(void);
const job_t *frameworkGetCurrentJob(void);
bool frameworkStartJob(jobType_E jobType, const event_t *triggerEvent);
void frameworkStopCurrentJob(jobStopReason_E reason);
void frameworkSetSysState(sysState_E sysState);

#endif /* FRAMEWORK_CORE_H */
