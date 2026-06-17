#ifndef FRAMEWORK_TIMER_H
#define FRAMEWORK_TIMER_H

bool frameworkTimerInitAll(void);
bool frameworkTimerStart(timerId_E timerId, uint32_t timeoutMs);
bool frameworkTimerStop(timerId_E timerId);
void frameworkTimerStopAll(void);

#endif /* FRAMEWORK_TIMER_H */
