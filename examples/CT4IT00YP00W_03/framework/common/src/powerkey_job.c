#include <stdint.h>
#include <stdbool.h>
#include "liot_type.h"
#include "liot_power.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "hw_led.h"
#include "hw_audio.h"
#include "hw_battery.h"
#include "hw_powerkey.h"
#include "liot_os.h"

static jobDesc_t g_powerkeyJobDesc;

static int powerkeyJobInit(void)
{
    /* powerkeyInit 延后到开机流程结束后由 app_main_task 调用,
     * 避免开机长按残留事件触发误关机 */
    return 0;
}

static int powerkeyJobOnStart(job_t *job, const event_t *triggerEvent)
{
    (void)job;

    switch (triggerEvent->eventId) {
    case EVT_KEY_POWER_ON:
        audioModulePlayPrompt(AUDIO_PROMPT_POWERON);
        frameworkStopCurrentJob(STOP_REASON_COMPLETED);
        break;
    case EVT_KEY_POWER_OFF:
        ledSetPattern(LED_PATTERN_SPEAKING);
        audioModulePlayPrompt(AUDIO_PROMPT_POWEROFF);
        audioModuleWaitPlayDone(LIOT_WAIT_FOREVER);
        ledSetPattern(LED_PATTERN_POWEROFF);
        liot_rtos_task_sleep_ms(100);
        liot_power_down(LIOT_POWD_NORMAL);
        break;
    case EVT_LOW_POWER:
        audioModulePlayPrompt(AUDIO_PROMPT_LOWPOWER);
        liot_power_down(LIOT_POWD_NORMAL);
        break;
    case EVT_SYSTEM_SLEEP:
        batteryModuleSleep();
        break;
    default:
        break;
    }
    return 0;
}

static int powerkeyJobOnStop(job_t *job, jobStopReason_E reason)
{
    (void)job;
    (void)reason;
    return 0;
}

const jobDesc_t *getPowerkeyJobDesc(void)
{
    static const jobOps_t ops = {
        .onInit   = powerkeyJobInit,
        .onStart  = powerkeyJobOnStart,
        .onEvent  = NULL,
        .onStop   = powerkeyJobOnStop,
    };
    g_powerkeyJobDesc.type        = JOB_TYPE_KEY;
    g_powerkeyJobDesc.ops         = &ops;
    g_powerkeyJobDesc.contextSize = 0;
    g_powerkeyJobDesc.jobPolicy   = NULL;
    return &g_powerkeyJobDesc;
}
