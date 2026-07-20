#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "frameworkTimer.h"
#include "hw_audio.h"
#include "ai_client.h"
#include "aiLog.h"
#include "app_nv.h"
#include "liot_gpio2.h"
#include "liot_dev.h"
#define TALK_RESPONSE_TIMEOUT_MS    3000

static jobDesc_t g_talkJobDesc;

static int talkJobInit(void)
{
    app_nv_data_t *nv = app_nv_get();
    audio_config_t cfg = {
        .rst_gpio        = PIN_GX8006_RST,
        .boot_gpio       = PIN_GX8006_BOOT,
        .pa_mode_gpio    = PIN_GX8006_PA_SD,
        .uart_port       = GX8006_UART_PORT,
        .uart_baudrate   = 1000000,
        .chat_mode       = nv->chat_mode,
        .volume          = nv->audio_volume,
        .vad_timeout_time = GX8006_VAD_TIMEOUT,
        .evt_cb          = NULL,
    };
    audioModuleInit(&cfg);
    audioModulePlayPrompt(AUDIO_PROMPT_POWERON);
    return 0;
}
static int talkJobDeInit(void)
{
    audioModuleDeinit();
    return 0;
}

static int talkJobOnStart(job_t *job, const event_t *triggerEvent)
{
    (void)job;
    if (triggerEvent->eventId == EVT_AI_CONNECT_REQ) {
        LOG_INFO("[AL CLOUDE] start → AI connecting");
        app_nv_data_t *nv = app_nv_get();
        ai_client_config_t cfg = {0};
        strncpy(cfg.token, nv->coze_token, sizeof(cfg.token) - 1);
        strncpy(cfg.botid, nv->coze_botid, sizeof(cfg.botid) - 1);
        strncpy(cfg.voiceid, nv->coze_voiceid, sizeof(cfg.voiceid) - 1);
        liot_dev_get_imei(cfg.imei, sizeof(cfg.imei), 0);
        int ret = ai_client_init(&cfg);
        LOG_WARN("[AL CLOUDE] init ret=%d", ret);
    }
    return 0;
}

static int talkJobOnEvent(job_t *job, const event_t *event)
{
    (void)job;
    switch (event->eventId) {
    case EVT_TIMER_EXPIRED:
        if (event->arg1 == TIMER_ID_TALK_LISTEN_TIMEOUT) {
            LOG_INFO("[AL CLOUDE] listen timeout → THINKING");
        } else if (event->arg1 == TIMER_ID_TALK_RESPONSE_TIMEOUT) {
            LOG_WARN("[AL CLOUDE] response timeout, stop");
            event_t doneEvt = { .eventId = EVT_AI_RESPONSE_DONE };
            frameworkPostEvent(&doneEvt);
        }
        break;

    case EVT_AUDIO_WAKEUP:
        LOG_INFO("[AL CLOUDE] wakeup");
        ai_client_cancel();
        audioModulePlayPrompt(AUDIO_PROMPT_AWAKE);
        break;

    case EVT_AUDIO_RECORD_DONE:
        LOG_INFO("[AL CLOUDE] VAD end");
        ai_client_send_audio_complete();
        frameworkTimerStart(TIMER_ID_TALK_RESPONSE_TIMEOUT, TALK_RESPONSE_TIMEOUT_MS);
        break;

    case EVT_AUDIO_PLAY_START:
        LOG_INFO("[AL CLOUDE] AI audio play start");
        break;

    case EVT_AUDIO_PLAY_DONE:
        LOG_INFO("[AL CLOUDE] AI audio play done");
        break;
    case EVT_AI_RESPONSE_THINK:
        LOG_INFO("[AL CLOUDE] AI response thinking");
        frameworkTimerStop(TIMER_ID_TALK_RESPONSE_TIMEOUT);
        break;

    case EVT_AI_RESPONSE_DONE:
        LOG_INFO("[AL CLOUDE] AI response done");
        frameworkTimerStop(TIMER_ID_TALK_RESPONSE_TIMEOUT);
        break;

    case EVT_AI_CONNECTED:
        LOG_INFO("[AL CLOUDE] AI connected");
        audioModulePlayPrompt(AUDIO_PROMPT_CLOUD_CONNECTED);
        audioModuleStartRecord();
        break;

    case EVT_AI_CONNECT_FAIL:
        LOG_WARN("[AL CLOUDE] AI connect failed");
        audioModulePlayPrompt(AUDIO_PROMPT_CONNECTFAIL);
        break;

    case EVT_AI_DISCONNECTED:
        LOG_WARN("[AL CLOUDE] AI disconnected, stop");
        frameworkStopCurrentJob(STOP_REASON_ERROR);
        break;

    default:
        break;
    }
    return 0;
}

static int talkJobOnStop(job_t *job, jobStopReason_E reason)
{
    LOG_INFO("[AL CLOUDE] stop, reason=%d", reason);
    (void)reason;
    return 0;
}

const jobDesc_t *getTalkJobDesc(void)
{
    static const jobOps_t ops = {
        .onInit  = talkJobInit,
        .onDeInit = talkJobDeInit,
        .onStart = talkJobOnStart,
        .onEvent = talkJobOnEvent,
        .onStop  = talkJobOnStop,
    };
    g_talkJobDesc.type        = JOB_TYPE_TALK;
    g_talkJobDesc.ops         = &ops;
    g_talkJobDesc.contextSize = 0;
    g_talkJobDesc.jobPolicy   = NULL;
    return &g_talkJobDesc;
}
