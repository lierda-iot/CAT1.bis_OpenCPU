#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"
#include "hw_led.h"
#include "hw_audio.h"
#include "app_nv.h"

static jobDesc_t g_ledSideJobDesc;

#define IDLE_POWEROFF_TIMEOUT_MS  10000

static void idleTimerReset(void)
{
    app_nv_data_t *nv = app_nv_get();
    frameworkTimerStop(TIMER_ID_IDLE_POWEROFF);
    frameworkTimerStart(TIMER_ID_IDLE_WARNING, nv->sleep_wait_time_ms);
}

static int ledSideJobInit(void)
{
    app_nv_data_t *nv = app_nv_get();
    ledInit(&nv->led_cfg);
    idleTimerReset();
    return 0;
}

static int ledSideJobOnEvent(job_t *job, const event_t *event)
{
    (void)job;
    switch (event->eventId) {
    case EVT_KEY_POWER_ON:
    case EVT_AI_CONNECTED:
    case EVT_NETWORK_READY:
        idleTimerReset();
        ledSetPattern(LED_PATTERN_STANDBY);
        break;
    case EVT_AI_RESPONSE_DONE:
        idleTimerReset();
        ledSetPattern(LED_PATTERN_STANDBY);
        break;
    case EVT_AI_RESPONSE_THINK:
        frameworkTimerStop(TIMER_ID_IDLE_POWEROFF);
        frameworkTimerStop(TIMER_ID_IDLE_WARNING);
        ledSetPattern(LED_PATTERN_THINKING);
        break;
    case EVT_AUDIO_RECORD_DONE:
    case EVT_AUDIO_WAKEUP:
        idleTimerReset();
        ledSetPattern(LED_PATTERN_THINKING);
        break;
    case EVT_AUDIO_PLAY_START:
        idleTimerReset();
        ledSetPattern(LED_PATTERN_SPEAKING);
        break;
    case EVT_AUDIO_PLAY_DONE:
        idleTimerReset();
        ledSetPattern(LED_PATTERN_STANDBY);
        break;
    case EVT_AI_CONNECT_FAIL:
    case EVT_AI_DISCONNECTED:
    case EVT_NETWORK_FAIL:
    case EVT_SIM_ERROR:
        ledSetPattern(LED_PATTERN_ERROR);
        break;
    case EVT_SYSTEM_SLEEP:
        ledSetPattern(LED_PATTERN_OFF);
        break;
    case EVT_LOW_POWER:
        ledSetPattern(LED_PATTERN_POWEROFF);
        break;
    case EVT_TIMER_EXPIRED:
        if (event->arg1 == TIMER_ID_IDLE_WARNING) {
            audioModulePlayPrompt(AUDIO_PROMPT_ASKTOHELP);
            frameworkTimerStart(TIMER_ID_IDLE_POWEROFF, IDLE_POWEROFF_TIMEOUT_MS);
        } else if (event->arg1 == TIMER_ID_IDLE_POWEROFF) {
            event_t pwrEvt = {.eventId = EVT_KEY_POWER_OFF};
            frameworkPostEvent(&pwrEvt);
        }
        break;
    default:
        break;
    }
    return 0;
}

const jobDesc_t *getLedSideJobDesc(void)
{
    static const jobOps_t ops = {
        .onInit  = ledSideJobInit,
        .onEvent = ledSideJobOnEvent,
    };
    g_ledSideJobDesc.ops         = &ops;
    g_ledSideJobDesc.contextSize = 0;
    g_ledSideJobDesc.jobPolicy   = NULL;
    return &g_ledSideJobDesc;
}
