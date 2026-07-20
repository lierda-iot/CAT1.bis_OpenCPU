#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hw_audio.h"
#include "audio_prompts.h"
#include "gx8006.h"
#include "liot_os.h"
#include "liot_log.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "ai_client.h"
#include "FreeRTOS.h"
#include "queue.h"

extern uint32_t gx8006_get_tx_pending(void);

#define PLAYBACK_TASK_STACK     (4096)
#define PLAYBACK_TASK_PRIO      (7)
#define PLAYBACK_QUEUE_LEN      (100)
#define PLAYBACK_IDLE_TIMEOUT   (2000)

/* ========== types & state ========== */

typedef struct {
    uint8_t *data;
    uint32_t len;
    uint8_t needfree;
} audio_frame_t;

typedef enum {
    PLAYBACK_IDLE = 0,
    PLAYBACK_PLAYING,
    PLAYBACK_STOP,
} playback_state_e;

static volatile playback_state_e g_playback_state = PLAYBACK_IDLE;
static liot_task_t     g_playback_task  = NULL;
static QueueHandle_t   g_playback_queue = NULL;

/* ========== prompt table ========== */

typedef struct {
    const uint8_t *data;
    uint32_t len;
} prompt_entry_t;

static const prompt_entry_t g_prompt_table[] = {
    [AUDIO_PROMPT_POWERON]         = {qystart_opus,        sizeof(qystart_opus)},
    [AUDIO_PROMPT_POWEROFF]        = {poweroff_opus,       sizeof(poweroff_opus)},
    [AUDIO_PROMPT_CONNECTED]       = {connected_opus,      sizeof(connected_opus)},
    [AUDIO_PROMPT_CLOUD_CONNECTED] = {cloudconnected_opus, sizeof(cloudconnected_opus)},
    [AUDIO_PROMPT_CONNECTFAIL]     = {connectfail_opus,    sizeof(connectfail_opus)},
    [AUDIO_PROMPT_SIM_ERROR]       = {simerror_opus,       sizeof(simerror_opus)},
    [AUDIO_PROMPT_AWAKE]           = {awake_opus,          sizeof(awake_opus)},
    [AUDIO_PROMPT_SEND]            = {dingdong_opus,       sizeof(dingdong_opus)},
    [AUDIO_PROMPT_ASKTOHELP]       = {system_wait_opus,    sizeof(system_wait_opus)},
};

/* ========== playback internals ========== */

static void playback_enqueue(uint8_t *data, uint32_t len, audio_prompt_e prompt)
{
    audio_frame_t frame = {0};

    if (prompt == AUDIO_PROMPT_NONE) {
        if (!data || len == 0)
            return;
        uint8_t *buf = liot_rtos_malloc(len);
        if (!buf)
            return;
        memcpy(buf, data, len);
        frame.data = buf;
        frame.len = len;
        frame.needfree = 1;
        if (xQueueSend(g_playback_queue, &frame, portMAX_DELAY) != pdPASS)
            liot_rtos_free(buf);
    } else {
        const prompt_entry_t *entry = &g_prompt_table[prompt];
        if (entry->data == NULL || entry->len == 0)
            return;
        uint32_t offset = 44 + GX8006_OPUS_FRAME_SIZE;
        if (offset >= entry->len)
            return;
        while (offset < entry->len) {
            uint32_t chunk = entry->len - offset;
            if (chunk > GX8006_OPUS_FRAME_SIZE)
                chunk = GX8006_OPUS_FRAME_SIZE;
            frame.data = (uint8_t *)&entry->data[offset];
            frame.len = chunk;
            frame.needfree = 0;
            xQueueSend(g_playback_queue, &frame, portMAX_DELAY);
            offset += GX8006_OPUS_FRAME_SIZE;
        }
    }
}

static void playback_task(void *arg)
{
    (void)arg;
    audio_frame_t frame;
    TickType_t wait = portMAX_DELAY;
    while (1) {
        if (xQueueReceive(g_playback_queue, &frame, wait) == pdPASS) {
            wait = pdMS_TO_TICKS(PLAYBACK_IDLE_TIMEOUT);
            if (g_playback_state == PLAYBACK_IDLE) {
                g_playback_state = PLAYBACK_PLAYING;
                gx8006_spk_stream_start();
                liot_trace("[AUDIO] stream_start (cached %d)",
                           (int)uxQueueMessagesWaiting(g_playback_queue) + 1);
            }

            if (g_playback_state == PLAYBACK_PLAYING) {
                gx8006_spk_stream_write(frame.data, frame.len);
            }

            if (frame.needfree)
                liot_rtos_free(frame.data);

        } else {
            if (g_playback_state == PLAYBACK_PLAYING) {
                if (gx8006_get_tx_pending() > 0) {
                    wait = pdMS_TO_TICKS(500);
                    continue;
                }
                liot_trace("[AUDIO] stream_stop");
                gx8006_spk_stream_stop();
            }
            wait = portMAX_DELAY;
            g_playback_state = PLAYBACK_IDLE;
        }
    }
}

/* ========== gx8006 callback ========== */

static void gx8006_event_handler(gx8006_evt_e evt, const uint8_t *data, uint32_t len)
{
    event_t fw_evt = {0};

    switch (evt) {
    case GX_EVT_MIC_VAD_START:
        break;
    case GX_EVT_MIC_VAD_DATA:
        if (data && len > 0 && ai_client_is_connected()) {
            ai_client_audio_t audio = {.data = (uint8_t *)data, .size = len};
            ai_client_send_audio(&audio);
        }
        break;
    case GX_EVT_MIC_VAD_END:
        fw_evt.eventId = EVT_AUDIO_RECORD_DONE;
        frameworkPostEvent(&fw_evt);
        break;
    case GX_EVT_AWAKEN:
        fw_evt.eventId = EVT_AUDIO_WAKEUP;
        frameworkPostEvent(&fw_evt);
        break;
    default:
        break;
    }
}

/* ========== public API ========== */

void audioModuleInit(const audio_config_t *cfg)
{
    gx8006_config_t gx_cfg = {
        .rst_gpio          = cfg->rst_gpio,
        .boot_gpio         = cfg->boot_gpio,
        .pa_mode_gpio      = cfg->pa_mode_gpio,
        .uart_port         = cfg->uart_port,
        .uart_baudrate     = cfg->uart_baudrate,
        .default_chat_mode = (gx8006_chat_mode_e)cfg->chat_mode,
        .default_volume    = cfg->volume,
        .vad_timeout_time  = cfg->vad_timeout_time,
        .evt_cb            = cfg->evt_cb ? (gx8006_evt_cb_t)cfg->evt_cb : gx8006_event_handler,
    };

    gx8006_init(&gx_cfg);
    liot_rtos_task_sleep_ms(500);
    g_playback_state = PLAYBACK_IDLE;
    
    if (g_playback_queue == NULL)
        g_playback_queue = xQueueCreate(PLAYBACK_QUEUE_LEN, sizeof(audio_frame_t));
    if (g_playback_task == NULL)
        liot_rtos_task_create(&g_playback_task, PLAYBACK_TASK_STACK,
                              PLAYBACK_TASK_PRIO, "audio_pb", playback_task, NULL);
}

void audioModuleDeinit(void)
{
    gx8006_deinit();
}

void audioModulePlayPrompt(audio_prompt_e prompt)
{
    if (prompt < 0 || prompt >= AUDIO_PROMPT_MAX)
            return;
    playback_enqueue(NULL, 0, prompt);
}

void audioModuleWritePlayback(uint8_t *data, uint32_t len)
{
    if (!data || len == 0 || !g_playback_queue)
        return;
    playback_enqueue(data, len, AUDIO_PROMPT_NONE);
}

void audioModuleStop(void)
{
    if (g_playback_state == PLAYBACK_PLAYING)
        g_playback_state = PLAYBACK_STOP;
}

bool audioModuleWaitPlayDone(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (1) {
        if (g_playback_queue && uxQueueMessagesWaiting(g_playback_queue) == 0
            && gx8006_get_tx_pending() == 0)
            return true;
        if (timeout_ms != 0xffffffff && elapsed >= timeout_ms)
            break;
        liot_rtos_task_sleep_ms(10);
        elapsed += 10;
    }
    return (g_playback_queue && uxQueueMessagesWaiting(g_playback_queue) == 0
            && gx8006_get_tx_pending() == 0);
}

void audioModuleStartRecord(void)
{
    gx8006_mic_open();
    liot_rtos_task_sleep_ms(10);
    gx8006_set_vad_awaken_enable(1);
}

void audioModuleStopRecord(void)
{
    gx8006_set_vad_awaken_enable(0);
    gx8006_mic_close();
}

int audioModuleReadRecord(uint8_t *buf, uint32_t *len)
{
    (void)buf;
    (void)len;
    return -1;
}

void audioModuleVolumeUp(void)
{
    uint32_t vol = gx8006_spk_get_volume();
    if (vol < 100) {
        gx8006_spk_set_volume(vol + 10);
    }
}

void audioModuleVolumeDown(void)
{
    uint32_t vol = gx8006_spk_get_volume();
    if (vol > 10) {
        gx8006_spk_set_volume(vol - 10);
    }
}

void audioModuleVolumeSet(uint32_t vol)
{
    if (vol > 100) vol = 100;
    gx8006_spk_set_volume(vol);
}

uint32_t audioModuleVolumeGet(void)
{
    return gx8006_spk_get_volume();
}
