/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * WeChat voice calling for NT26F6D0/F6D_A.
 *
 * Architecture:
 *   - formal MQTT callbacks only copy bounded signaling into a fixed pool;
 *   - this worker owns HTTP, call state, WHIP and the ES8311 media loop;
 *   - the OLED UI only sends non-blocking requests and reads a snapshot;
 *   - the process-wide TiRTC runtime is initialized elsewhere exactly once.
 *
 * Network audio is G.711 A-law, 8 kHz, mono, 20 ms (160 bytes).  The board
 * codec remains 16 kHz PCM, so capture is downsampled 2:1 and playback is
 * expanded 1:2.  F6D_A initializes I2S in PLAY_RECORD mode, allowing this
 * single owner task to keep uplink capture active while TX DMA plays the
 * downlink.  Audio stream id 0 and commands 0x2000/0x2001 follow the WX VoIP
 * contract.  Acoustic echo cancellation is outside this minimal port.
 */

#include "wechat_call.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "audio_device.h"
#ifdef HWDEMO_AI_CHAT_EN
#include "ai_chat.h"
#endif
#include "device_binding.h"
#ifdef HWDEMO_DEV_CHAT_EN
#include "device_call.h"
#endif
#include "formal_mqtt.h"
#include "g711_codec.h"
#ifdef HWDEMO_LIVE_TALK_EN
#include "platform_intercom.h"
#endif
#include "tirtc_runtime.h"
#include "liot_log.h"
#include "liot_os.h"
#include "tirtc/tiRTC.h"

#define WX_CONTACT_MAX                 8U
#define WX_OPEN_ID_MAX                96U
#define WX_APP_ID_MAX                 64U
#define WX_MODEL_ID_MAX               64U
#define WX_REMARK_MAX                257U
#define WX_DISPLAY_MAX                12U
#define WX_CALL_ID_MAX                64U
#define WX_ROOM_ID_MAX               128U
#define WX_PEER_ID_MAX              1152U
#define WX_TOKEN_MAX                1152U
#define WX_SESSION_TOKEN_MAX         256U
#define WX_PAYLOAD_MAX               512U
#define WX_FROM_MAX                   64U
#define WX_REJECT_PATH                "/v1/wxvoip/reject"
#define WX_TIRTC_SERVICE_URL_CAPACITY 128U

_Static_assert(DEMO_BIND_TIRTC_ENDPOINT_MAX + sizeof(WX_REJECT_PATH) - 1U <=
                   WX_TIRTC_SERVICE_URL_CAPACITY,
               "TiRTC service request URL exceeds SDK buffer");

#define WX_MQTT_EVENT_DEPTH             3U
#define WX_AUDIO_QUEUE_DEPTH             8U
#define WX_AUDIO_PAYLOAD_MAX            640U
#define WX_PREBUFFER_PACKETS              3U
#define WX_PREBUFFER_TIMEOUT_MS          80U
#define WX_UNWANTED_QUEUE_DEPTH           4U
#define WX_WHIP_CONTEXT_COUNT             3U

#define WX_STREAM_ID                       0U
#define WX_CMD_ACCEPT                  0x2000U
#define WX_CMD_HANGUP                 0x2001U
#define WX_UPLINK_SAMPLES_8K  (DEMO_AI_AUDIO_FRAME_SAMPLES / 2U)
#define WX_PLAY_PCM_MAX_SAMPLES       (WX_AUDIO_PAYLOAD_MAX * 2U)

#define WX_CONNECT_TIMEOUT_MS          10000U
#define WX_START_TIMEOUT_MS            15000U
#define WX_OUTGOING_START_TIMEOUT_MS   30000U
#define WX_OUTGOING_TIMEOUT_MS         30000U
#define WX_INCOMING_TIMEOUT_MS         45000U
#define WX_ERROR_VISIBLE_MS             3000U
#define WX_SYNC_RETRY_MS               10000U
#define WX_IDLE_WAIT_MS                  100U
#define WX_DISCONNECT_WAIT_MS           5000U
#define WX_AUDIO_PRODUCER_TIMEOUT_MS     1000U
#define WX_IGNORED_CALL_MS              60000U
#define WX_RECENT_ROOM_MS              600000U

#define WX_CLOSE_NONE                       (-1)
#define WX_HANGUP_REASON_MANUAL                1
#define WX_HANGUP_REASON_DEVICE                4
#define WX_HANGUP_REASON_BUSY                  5
#define WX_HANGUP_REASON_TIMEOUT               6
#define WX_HANGUP_REASON_REJECT                7
#define WX_HANGUP_REASON_EXCEPTION             8

#define WX_ERR_INIT                    (-3001)
#define WX_ERR_SERVICE                 (-3002)
#define WX_ERR_CONTACTS                (-3003)
#define WX_ERR_CALL_HTTP               (-3004)
#define WX_ERR_NO_CONTACT              (-3005)
#define WX_ERR_WHIP                    (-3006)
#define WX_ERR_CONNECT_TIMEOUT         (-3007)
#define WX_ERR_START_TIMEOUT           (-3008)
#define WX_ERR_AUDIO                   (-3009)
#define WX_ERR_CONNECTION              (-3010)

typedef struct
{
    char open_id[WX_OPEN_ID_MAX];
    char app_id[WX_APP_ID_MAX];
    char model_id[WX_MODEL_ID_MAX];
    char remark[WX_REMARK_MAX];
    char display[WX_DISPLAY_MAX];
} wx_contact_t;

typedef enum
{
    WX_MQTT_INCOMING = 0,
    WX_MQTT_CANCEL,
    WX_MQTT_CONTACTS_UPDATE,
} wx_mqtt_event_type_e;

typedef struct
{
    wx_mqtt_event_type_e type;
    uint32_t received_ms;
    char peer_id[WX_PEER_ID_MAX];
    char token[WX_TOKEN_MAX];
    char room_id[WX_ROOM_ID_MAX];
    char open_id[WX_OPEN_ID_MAX];
    char app_id[WX_APP_ID_MAX];
    char model_id[WX_MODEL_ID_MAX];
    char session_token[WX_SESSION_TOKEN_MAX];
    char payload[WX_PAYLOAD_MAX];
    char call_id[WX_CALL_ID_MAX];
    char from[WX_FROM_MAX];
    char remark[WX_REMARK_MAX];
} wx_mqtt_event_t;

typedef struct
{
    uint16_t length;
    uint8_t flags;
    uint32_t generation;
    uint8_t payload[WX_AUDIO_PAYLOAD_MAX];
} wx_audio_frame_t;

typedef struct
{
    uint32_t generation;
    volatile bool pending;
} wx_whip_context_t;

static liot_sem_t s_event_sem;
static liot_queue_t s_mqtt_queue;
static liot_queue_t s_audio_queue;
static liot_queue_t s_unwanted_queue;

static wx_mqtt_event_t s_mqtt_pool[WX_MQTT_EVENT_DEPTH];
static bool s_mqtt_pool_used[WX_MQTT_EVENT_DEPTH];
static wx_audio_frame_t s_audio_pool[WX_AUDIO_QUEUE_DEPTH];
static bool s_audio_pool_used[WX_AUDIO_QUEUE_DEPTH];

static volatile demo_wechat_state_e s_state = DEMO_WECHAT_OFFLINE;
static volatile int s_error;
static volatile bool s_home_allowed;
static volatile bool s_request_call;
/* Covers the worker-owned request before DIALING becomes visible to KEY2. */
static volatile bool s_call_starting;
static volatile bool s_request_hangup;
static volatile bool s_request_refresh;
static volatile uint32_t s_request_answer_generation;
static volatile bool s_levels_dirty = true;
static volatile uint8_t s_speaker_level = DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static volatile uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;

static wx_contact_t s_contacts[WX_CONTACT_MAX];
static uint8_t s_contact_count;
static uint8_t s_selected_contact;
static uint32_t s_contacts_revision;
static char s_display_name[WX_DISPLAY_MAX] = "WX";

static wx_mqtt_event_t s_call;
static uint32_t s_incoming_generation;
static uint32_t s_session_sequence;
static uint32_t s_completed_sequence;
static uint32_t s_generation;
static uint32_t s_deadline_ms;
static uint32_t s_error_deadline_ms;
static uint32_t s_contacts_retry_at;
static bool s_profile_ready;
static bool s_audio_owned;
static demo_ai_audio_lease_t s_audio_lease = DEMO_AI_AUDIO_LEASE_INIT;
static volatile bool s_tirtc_owned;
static volatile uint32_t s_tirtc_session_generation;
static bool s_tirtc_release_error_logged;
static bool s_outgoing_session;
static volatile uint32_t s_audio_queued;
static volatile uint32_t s_audio_producers;
static volatile uint32_t s_unwanted_queued;
static volatile bool s_unwanted_overflow;
static char s_ignored_call_id[WX_CALL_ID_MAX];
static uint32_t s_ignored_call_deadline_ms;
static char s_recent_room_id[WX_ROOM_ID_MAX];
static char s_recent_app_id[WX_APP_ID_MAX];
static uint32_t s_recent_room_deadline_ms;
static uint32_t s_rx_dropped;
static uint32_t s_tx_dropped;
static uint32_t s_rx_frames;
static uint32_t s_tx_frames;
static uint32_t s_prebuffer_start_ms;
static bool s_downlink_reject_logged;
static bool s_playback_error_logged;

static volatile bool s_whip_done;
static volatile int s_whip_error;
static volatile tirtc_conn_t s_whip_conn;
static volatile bool s_start_pending;
static volatile bool s_remote_hangup_pending;
static volatile bool s_conn_error_pending;
static volatile int s_conn_error;
static volatile bool s_disconnected_pending;
static volatile tirtc_conn_t s_closing_conn;
static volatile bool s_closing_done;
static tirtc_conn_t volatile s_conn;
static wx_whip_context_t s_whip_contexts[WX_WHIP_CONTEXT_COUNT];
static wx_whip_context_t * volatile s_active_whip_context;

static char s_http_response[4096];
static __attribute__((aligned(16))) int16_t
    s_capture_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint8_t s_uplink_alaw[WX_UPLINK_SAMPLES_8K];
static __attribute__((aligned(16))) int16_t
    s_play_pcm[WX_PLAY_PCM_MAX_SAMPLES];

static bool wx_connection_matches_or_claims(tirtc_conn_t hconn);

static void wx_signal(void)
{
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_release(s_event_sem);
    }
}

static bool wx_deadline_pending(uint32_t deadline)
{
    return (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

static bool wx_state_has_session(demo_wechat_state_e state)
{
    return state == DEMO_WECHAT_RINGING ||
           state == DEMO_WECHAT_DIALING ||
           state == DEMO_WECHAT_CONNECTING ||
           state == DEMO_WECHAT_WAIT_START ||
           state == DEMO_WECHAT_IN_CALL ||
           state == DEMO_WECHAT_STOPPING;
}

/* Caller holds the RTOS critical section.  A timed-out WHIP request remains
 * busy until its real callback returns; reusing its context earlier can route
 * a late callback into a new call or another TiRTC feature. */
static bool wx_local_rtc_idle_locked(void)
{
    uint32_t i;

    if (s_conn != NULL || s_whip_conn != NULL || s_closing_conn != NULL ||
        s_active_whip_context != NULL || s_whip_done || s_audio_owned ||
        s_tirtc_owned ||
        s_unwanted_queued != 0U || s_unwanted_overflow)
    {
        return false;
    }
    for (i = 0U; i < WX_WHIP_CONTEXT_COUNT; ++i)
    {
        if (s_whip_contexts[i].pending)
        {
            return false;
        }
    }
    return true;
}

static void wx_set_state(demo_wechat_state_e state, int error)
{
    if (s_state != state || s_error != error)
    {
        s_state = state;
        s_error = error;
        liot_trace("[WX] state=%d error=%d free_heap=%u\r\n",
                   (int)state, error,
                   (unsigned int)liot_xPortGetFreeHeapSize());
    }
}

static void wx_set_display(const char *text)
{
    size_t length = text != NULL ? strlen(text) : 0U;

    if (length >= sizeof(s_display_name))
    {
        length = sizeof(s_display_name) - 1U;
    }
    liot_rtos_enter_critical();
    memset(s_display_name, 0, sizeof(s_display_name));
    if (length > 0U)
    {
        memcpy(s_display_name, text, length);
    }
    liot_rtos_exit_critical();
}

static void wx_make_open_id_tail(const char *open_id, char output[7])
{
    char reversed[6];
    size_t length;
    size_t used = 0U;
    size_t i;

    memset(output, 0, 7U);
    if (open_id == NULL)
    {
        return;
    }
    length = strlen(open_id);
    i = length;
    while (i > 0U && used < sizeof(reversed))
    {
        unsigned char ch = (unsigned char)open_id[--i];
        if (ch >= 'a' && ch <= 'z')
        {
            reversed[used++] = (char)toupper(ch);
        }
        else if ((ch >= 'A' && ch <= 'Z') ||
                 (ch >= '0' && ch <= '9'))
        {
            reversed[used++] = (char)ch;
        }
    }
    for (i = 0U; i < used; ++i)
    {
        output[i] = reversed[used - i - 1U];
    }
}

static void wx_make_display(const char *remark, const char *open_id,
                            uint8_t index,
                            char output[WX_DISPLAY_MAX])
{
    char id_tail[7];
    size_t i;
    size_t used = 0U;

    memset(output, 0, WX_DISPLAY_MAX);
    if (remark != NULL)
    {
        for (i = 0U; remark[i] != '\0' && used + 1U < WX_DISPLAY_MAX; ++i)
        {
            unsigned char ch = (unsigned char)remark[i];
            if (ch >= 'a' && ch <= 'z')
            {
                output[used++] = (char)toupper(ch);
            }
            else if ((ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' ||
                     ch == '+' || ch == ':')
            {
                output[used++] = (char)ch;
            }
        }
    }
    while (used > 0U && output[used - 1U] == ' ')
    {
        output[--used] = '\0';
    }
    if (used == 0U)
    {
        /* This OLED has an ASCII-only 5x7 font.  Preserve the real UTF-8
         * remark only in the contact cache, but use a stable masked OpenID
         * label on-screen instead of the unrelated generic CONTACTn name. */
        wx_make_open_id_tail(open_id, id_tail);
        if (id_tail[0] != '\0')
        {
            (void)snprintf(output, WX_DISPLAY_MAX, "WX-%s", id_tail);
        }
        else
        {
            (void)snprintf(output, WX_DISPLAY_MAX, "WX USER%u",
                           (unsigned int)(index + 1U));
        }
    }
}

static int wx_json_copy(const cJSON *object, const char *key,
                        char *output, size_t capacity, bool required)
{
    const cJSON *item;
    size_t length;

    if (output == NULL || capacity == 0U)
    {
        return -1;
    }
    output[0] = '\0';
    item = object != NULL ?
               cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
    if (!cJSON_IsString(item) || item->valuestring == NULL)
    {
        return required ? -1 : 0;
    }
    length = strlen(item->valuestring);
    if (length >= capacity)
    {
        return -2;
    }
    memcpy(output, item->valuestring, length + 1U);
    return 0;
}

static int wx_mqtt_slot_acquire(void)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    for (i = 0U; i < WX_MQTT_EVENT_DEPTH; ++i)
    {
        if (!s_mqtt_pool_used[i])
        {
            s_mqtt_pool_used[i] = true;
            slot = (int)i;
            break;
        }
    }
    liot_rtos_exit_critical();
    return slot;
}

static void wx_mqtt_slot_release(uint8_t slot)
{
    if (slot >= WX_MQTT_EVENT_DEPTH)
    {
        return;
    }
    memset(&s_mqtt_pool[slot], 0, sizeof(s_mqtt_pool[slot]));
    liot_rtos_enter_critical();
    s_mqtt_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static int wx_audio_slot_acquire(tirtc_conn_t hconn, uint32_t *generation)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    if (s_state == DEMO_WECHAT_IN_CALL && hconn == (tirtc_conn_t)s_conn)
    {
        for (i = 0U; i < WX_AUDIO_QUEUE_DEPTH; ++i)
        {
            if (!s_audio_pool_used[i])
            {
                s_audio_pool_used[i] = true;
                *generation = s_generation;
                ++s_audio_producers;
                slot = (int)i;
                break;
            }
        }
    }
    liot_rtos_exit_critical();
    return slot;
}

static void wx_audio_producer_done(void)
{
    bool idle = false;

    liot_rtos_enter_critical();
    if (s_audio_producers > 0U)
    {
        --s_audio_producers;
        idle = s_audio_producers == 0U;
    }
    liot_rtos_exit_critical();
    if (idle)
    {
        wx_signal();
    }
}

static bool wx_wait_audio_producers(void)
{
    uint32_t deadline = liot_rtos_get_running_time() +
                        WX_AUDIO_PRODUCER_TIMEOUT_MS;
    uint32_t producers;

    do
    {
        liot_rtos_enter_critical();
        producers = s_audio_producers;
        liot_rtos_exit_critical();
        if (producers == 0U)
        {
            return true;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, 10U);
    } while (wx_deadline_pending(deadline));

    liot_trace("[WX] audio callback quiesce timeout producers=%u\r\n",
               (unsigned int)producers);
    return false;
}

static void wx_audio_slot_release(uint8_t slot)
{
    if (slot >= WX_AUDIO_QUEUE_DEPTH)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_audio_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static void wx_audio_queued_decrement(void)
{
    liot_rtos_enter_critical();
    if (s_audio_queued != 0U)
    {
        --s_audio_queued;
    }
    liot_rtos_exit_critical();
}

static void wx_discard_audio(void)
{
    uint8_t slot;
    bool producers_idle;

    while (s_audio_queue != NULL &&
           liot_rtos_queue_wait(s_audio_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        wx_audio_queued_decrement();
        wx_audio_slot_release(slot);
    }
    liot_rtos_enter_critical();
    producers_idle = s_audio_producers == 0U;
    if (producers_idle)
    {
        /* Every reserved slot is now either drained or was released by its
         * producer.  Only this quiescent boundary may reset the pool. */
        memset(s_audio_pool_used, 0, sizeof(s_audio_pool_used));
        s_audio_queued = 0U;
    }
    liot_rtos_exit_critical();
}

static void wx_defer_disconnect(tirtc_conn_t connection)
{
    if (connection == NULL || s_unwanted_queue == NULL)
    {
        s_unwanted_overflow = true;
        ++s_rx_dropped;
        return;
    }
    liot_rtos_enter_critical();
    ++s_unwanted_queued;
    liot_rtos_exit_critical();
    if (liot_rtos_queue_release(s_unwanted_queue, sizeof(connection),
                                (uint8 *)&connection,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        liot_rtos_enter_critical();
        if (s_unwanted_queued != 0U)
        {
            --s_unwanted_queued;
        }
        s_unwanted_overflow = true;
        liot_rtos_exit_critical();
        ++s_rx_dropped;
        return;
    }
    wx_signal();
}

static void wx_drain_unwanted(void)
{
    tirtc_conn_t connection;
    uint32_t session_generation;
    bool tirtc_owned;
    int ret;

    while (s_unwanted_queue != NULL &&
           liot_rtos_queue_wait(s_unwanted_queue, (uint8 *)&connection,
                                sizeof(connection),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        liot_rtos_enter_critical();
        tirtc_owned = s_tirtc_owned;
        session_generation = s_tirtc_session_generation;
        liot_rtos_exit_critical();
        if (connection != NULL && tirtc_owned)
        {
            ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_WECHAT,
                                        session_generation);
            if (ret != 0 && ret != TIRTC_E_INVALID_HANDLE)
            {
                liot_trace("[WX] deferred managed disconnect ret=%d\r\n",
                           ret);
            }
        }
        else if (connection != NULL)
        {
            liot_rtos_enter_critical();
            s_unwanted_overflow = true;
            liot_rtos_exit_critical();
            demo_tirtc_require_restart(TIRTC_E_INVALID_HANDLE);
        }
        liot_rtos_enter_critical();
        if (s_unwanted_queued != 0U)
        {
            --s_unwanted_queued;
        }
        liot_rtos_exit_critical();
    }
}

static bool wx_release_tirtc_session(void)
{
    uint32_t session_generation;
    bool owned;
    int ret;

    liot_rtos_enter_critical();
    owned = s_tirtc_owned;
    session_generation = s_tirtc_session_generation;
    liot_rtos_exit_critical();
    if (!owned)
    {
        return true;
    }
    ret = demo_tirtc_session_release(DEMO_TIRTC_OWNER_WECHAT,
                                     session_generation);
    if (ret == 0)
    {
        liot_rtos_enter_critical();
        if (s_tirtc_session_generation == session_generation)
        {
            s_tirtc_owned = false;
            s_tirtc_session_generation = 0U;
            s_tirtc_release_error_logged = false;
        }
        liot_rtos_exit_critical();
        return true;
    }
    if (ret != TIRTC_E_BUSY && !s_tirtc_release_error_logged)
    {
        s_tirtc_release_error_logged = true;
        liot_trace("[WX] managed session release failed ret=%d generation=%u\r\n",
                   ret, (unsigned int)session_generation);
        demo_tirtc_require_restart(ret);
    }
    return false;
}

static bool wx_transport_ready(void)
{
    demo_binding_snapshot_t binding;

    demo_binding_get_snapshot(&binding);
    return binding.state == DEMO_BIND_BOUND &&
           demo_formal_mqtt_is_online() && demo_tirtc_is_ready();
}

static void wx_formal_mqtt_handler(
    demo_formal_mqtt_message_kind_e kind,
    const char *type,
    const char *channel,
    const cJSON *payload,
    void *user)
{
    wx_mqtt_event_t *event;
    uint8_t slot;
    int acquired;
    bool valid = true;

    (void)kind;
    (void)user;
    if (type == NULL || channel == NULL || strcmp(channel, "wx") != 0)
    {
        return;
    }
    if (strcmp(type, "call_incoming") != 0 &&
        strcmp(type, "call_cancel") != 0 &&
        strcmp(type, "callers_update") != 0)
    {
        return;
    }

    acquired = wx_mqtt_slot_acquire();
    if (acquired < 0)
    {
        ++s_rx_dropped;
        liot_trace("[WX-MQTT] event pool full type=%s\r\n", type);
        return;
    }
    slot = (uint8_t)acquired;
    event = &s_mqtt_pool[slot];
    memset(event, 0, sizeof(*event));
    event->received_ms = liot_rtos_get_running_time();

    if (strcmp(type, "call_incoming") == 0)
    {
        event->type = WX_MQTT_INCOMING;
        valid = payload != NULL &&
                wx_json_copy(payload, "peer_id", event->peer_id,
                             sizeof(event->peer_id), true) == 0 &&
                wx_json_copy(payload, "token", event->token,
                             sizeof(event->token), true) == 0 &&
                wx_json_copy(payload, "wx_room_id", event->room_id,
                             sizeof(event->room_id), true) == 0 &&
                wx_json_copy(payload, "wx_user_openid", event->open_id,
                             sizeof(event->open_id), false) == 0 &&
                wx_json_copy(payload, "wx_app_id", event->app_id,
                             sizeof(event->app_id), false) == 0 &&
                wx_json_copy(payload, "wx_model_id", event->model_id,
                             sizeof(event->model_id), false) == 0 &&
                wx_json_copy(payload, "wx_server_token",
                             event->session_token,
                             sizeof(event->session_token), false) == 0 &&
                wx_json_copy(payload, "wx_payload", event->payload,
                             sizeof(event->payload), false) == 0 &&
                wx_json_copy(payload, "wx_call_id", event->call_id,
                             sizeof(event->call_id), false) == 0 &&
                wx_json_copy(payload, "wx_from", event->from,
                             sizeof(event->from), false) == 0;
        if (wx_json_copy(payload, "wx_user_remark", event->remark,
                         sizeof(event->remark), false) != 0 ||
            event->remark[0] == '\0')
        {
            (void)wx_json_copy(payload, "wx_user_nickname", event->remark,
                               sizeof(event->remark), false);
        }
        if (event->remark[0] == '\0')
        {
            (void)wx_json_copy(payload, "remark", event->remark,
                               sizeof(event->remark), false);
        }
    }
    else if (strcmp(type, "call_cancel") == 0)
    {
        event->type = WX_MQTT_CANCEL;
        valid = payload != NULL &&
                wx_json_copy(payload, "wx_room_id", event->room_id,
                             sizeof(event->room_id), false) == 0 &&
                wx_json_copy(payload, "wx_call_id", event->call_id,
                             sizeof(event->call_id), false) == 0;
    }
    else
    {
        event->type = WX_MQTT_CONTACTS_UPDATE;
    }

    if (!valid || s_mqtt_queue == NULL ||
        liot_rtos_queue_release(s_mqtt_queue, sizeof(slot), &slot,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        wx_mqtt_slot_release(slot);
        ++s_rx_dropped;
        liot_trace("[WX-MQTT] invalid/overflow event type=%s\r\n", type);
        return;
    }
    liot_trace("[WX-MQTT] queued type=%s room=%u peer_len=%u token_len=%u\r\n",
               type,
               event->room_id[0] != '\0' ? 1U : 0U,
               (unsigned int)strlen(event->peer_id),
               (unsigned int)strlen(event->token));
    wx_signal();
}

static void wx_on_conn_error(tirtc_conn_t hconn, int error)
{
    if (wx_connection_matches_or_claims(hconn))
    {
        s_conn_error = error;
        s_conn_error_pending = true;
        wx_signal();
    }
}

static void wx_on_disconnected(tirtc_conn_t hconn)
{
    bool matched = false;

    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == (tirtc_conn_t)s_closing_conn)
    {
        s_closing_done = true;
        matched = true;
    }
    if (hconn != NULL &&
        (hconn == (tirtc_conn_t)s_conn ||
         hconn == (tirtc_conn_t)s_whip_conn))
    {
        s_disconnected_pending = true;
        matched = true;
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        wx_signal();
    }
}

static void wx_on_audio(tirtc_conn_t hconn,
                        const TIRTCFRAMEINFO *frame,
                        void *data)
{
    wx_audio_frame_t *message;
    uint32_t generation = 0U;
    uint8_t slot;
    int acquired;

    if (frame == NULL || data == NULL)
    {
        return;
    }
    if (frame->stream_id != WX_STREAM_ID ||
        frame->media != TIRTC_AUDIO_ALAW || frame->length == 0U ||
        frame->length > WX_AUDIO_PAYLOAD_MAX ||
        (frame->flags != TIRTC_AUDIOSAMPLE_8K16B1C &&
         frame->flags != TIRTC_AUDIOSAMPLE_16K16B1C))
    {
        ++s_rx_dropped;
        if (!s_downlink_reject_logged)
        {
            s_downlink_reject_logged = true;
            liot_trace("[WX] reject downlink stream=%u media=%u flags=%u "
                       "bytes=%u; want stream=0 ALAW 8k/16k\r\n",
                       (unsigned int)frame->stream_id,
                       (unsigned int)frame->media,
                       (unsigned int)frame->flags,
                       (unsigned int)frame->length);
        }
        return;
    }
    acquired = wx_audio_slot_acquire(hconn, &generation);
    if (acquired < 0)
    {
        ++s_rx_dropped;
        return;
    }
    slot = (uint8_t)acquired;
    message = &s_audio_pool[slot];
    message->length = (uint16_t)frame->length;
    message->flags = frame->flags;
    message->generation = generation;
    memcpy(message->payload, data, frame->length);
    liot_rtos_enter_critical();
    ++s_audio_queued;
    liot_rtos_exit_critical();
    if (s_audio_queue == NULL ||
        liot_rtos_queue_release(s_audio_queue, sizeof(slot), &slot,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        wx_audio_queued_decrement();
        wx_audio_slot_release(slot);
        wx_audio_producer_done();
        ++s_rx_dropped;
        return;
    }
    wx_audio_producer_done();
    wx_signal();
}

static bool wx_command_is(uint32_t cmdw, uint32_t expected)
{
    /* TiRTC may carry flags/sequence bits together with the 16-bit VoIP
     * command.  Accept all forms used by the official audio-only example. */
    return cmdw == expected ||
           (cmdw & 0xffffU) == expected ||
           (cmdw & 0x7fffU) == expected;
}

static bool wx_connection_matches_or_claims(tirtc_conn_t hconn)
{
    bool matched = false;

    if (hconn == NULL)
    {
        return false;
    }
    liot_rtos_enter_critical();
    if (hconn == (tirtc_conn_t)s_conn ||
        hconn == (tirtc_conn_t)s_whip_conn)
    {
        matched = true;
    }
    else if (s_state == DEMO_WECHAT_CONNECTING &&
             s_active_whip_context != NULL && s_conn == NULL &&
             s_whip_conn == NULL)
    {
        /* The data channel may announce itself before the async WHIP callback.
         * Claim exactly one handle; subsequent callbacks must match it. */
        s_whip_conn = hconn;
        matched = true;
    }
    liot_rtos_exit_critical();
    return matched;
}

static void wx_on_command(tirtc_conn_t hconn, uint32_t cmdw,
                           const void *data, uint32_t length)
{
    bool matched = wx_connection_matches_or_claims(hconn);

    (void)data;
    (void)length;
    if (!matched)
    {
        return;
    }
    if (wx_command_is(cmdw, WX_CMD_ACCEPT))
    {
        s_start_pending = true;
        wx_signal();
    }
    else if (wx_command_is(cmdw, WX_CMD_HANGUP))
    {
        s_remote_hangup_pending = true;
        wx_signal();
    }
}

static int wx_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    if (stream_id == WX_STREAM_ID &&
        wx_connection_matches_or_claims(hconn))
    {
        return 0;
    }
    return -1;
}

static void wx_on_unsubscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    (void)hconn;
    (void)stream_id;
}

static const demo_tirtc_listener_t s_tirtc_listener = {
    .on_conn_error = wx_on_conn_error,
    .on_disconnected = wx_on_disconnected,
    .on_audio = wx_on_audio,
    .on_command = wx_on_command,
    .on_subscribe_audio = wx_on_subscribe_audio,
    .on_unsubscribe_audio = wx_on_unsubscribe_audio,
};

static wx_whip_context_t *wx_whip_context_acquire(void)
{
    uint32_t i;
    wx_whip_context_t *context = NULL;

    liot_rtos_enter_critical();
    for (i = 0U; i < WX_WHIP_CONTEXT_COUNT; ++i)
    {
        if (!s_whip_contexts[i].pending)
        {
            s_whip_contexts[i].generation = s_generation;
            s_whip_contexts[i].pending = true;
            context = &s_whip_contexts[i];
            break;
        }
    }
    liot_rtos_exit_critical();
    return context;
}

static void wx_whip_connect_callback(int error, tirtc_conn_t hconn,
                                     void *user_data)
{
    wx_whip_context_t *context = (wx_whip_context_t *)user_data;
    uint32_t generation;
    bool current;

    if (context == NULL)
    {
        if (error == 0 && hconn != NULL)
        {
            wx_defer_disconnect(hconn);
        }
        return;
    }
    liot_rtos_enter_critical();
    generation = context->generation;
    current = context == s_active_whip_context &&
              generation == s_generation &&
              s_state == DEMO_WECHAT_CONNECTING;
    context->pending = false;
    if (current)
    {
        s_whip_error = error;
        s_whip_conn = hconn;
        s_whip_done = true;
    }
    liot_rtos_exit_critical();
    if (!current)
    {
        if (error == 0 && hconn != NULL)
        {
            wx_defer_disconnect(hconn);
        }
        wx_signal();
        return;
    }
    wx_signal();
}

static void wx_service_response(const char *body, void *user_data)
{
    (void)body;
    (void)user_data;
    liot_trace("[WX] reject response received\r\n");
}

static int wx_reject(const wx_mqtt_event_t *call, int reason)
{
    cJSON *root;
    char *json;
    int ret;

    if (call == NULL || call->app_id[0] == '\0' ||
        call->model_id[0] == '\0')
    {
        return -1;
    }
    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "wx_app_id", call->app_id) ||
        !cJSON_AddStringToObject(root, "wx_model_id", call->model_id) ||
        !cJSON_AddStringToObject(root, "wx_session_token",
                                call->session_token) ||
        !cJSON_AddStringToObject(root, "wx_room_id", call->room_id) ||
        !cJSON_AddStringToObject(root, "wx_payload", call->payload) ||
        !cJSON_AddNumberToObject(root, "hangup_reason", reason))
    {
        cJSON_Delete(root);
        return -1;
    }
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL)
    {
        return -1;
    }
    ret = TiRtcServiceRequest(WX_REJECT_PATH, json, NULL,
                              wx_service_response, NULL);
    cJSON_free(json);
    liot_trace("[WX] reject submit reason=%d ret=%d\r\n", reason, ret);
    return ret;
}

static int wx_business_code_ok(const char *response)
{
    cJSON *root = cJSON_Parse(response);
    const cJSON *code = root != NULL ?
                            cJSON_GetObjectItemCaseSensitive(root, "code") :
                            NULL;
    int ok = cJSON_IsNumber(code) && code->valueint == 0;

    cJSON_Delete(root);
    return ok ? 0 : -1;
}

static int wx_fetch_contacts(void)
{
    wx_contact_t contacts[WX_CONTACT_MAX];
    cJSON *root = NULL;
    const cJSON *code;
    const cJSON *data;
    const cJSON *list;
    const cJSON *item;
    uint8_t count = 0U;
    int http_status = 0;
    int ret;

    memset(contacts, 0, sizeof(contacts));
    memset(s_http_response, 0, sizeof(s_http_response));
    ret = demo_binding_service_request(
        DEMO_BIND_SERVICE_VOIP, DEMO_BIND_HTTP_GET,
        "/v1/voip/device/contacts", NULL,
        s_http_response, sizeof(s_http_response), &http_status);
    if (ret != 0 || http_status != 200)
    {
        liot_trace("[WX] contacts HTTP failed ret=%d status=%d\r\n",
                   ret, http_status);
        return WX_ERR_CONTACTS;
    }
    root = cJSON_Parse(s_http_response);
    code = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "code") : NULL;
    data = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    list = cJSON_IsObject(data) ?
               cJSON_GetObjectItemCaseSensitive(data, "contacts") : NULL;
    if (!cJSON_IsNumber(code) || code->valueint != 0 ||
        !cJSON_IsArray(list))
    {
        cJSON_Delete(root);
        return WX_ERR_CONTACTS;
    }

    cJSON_ArrayForEach(item, list)
    {
        wx_contact_t *contact;
        if (count >= WX_CONTACT_MAX)
        {
            break;
        }
        contact = &contacts[count];
        if (wx_json_copy(item, "wx_open_id", contact->open_id,
                         sizeof(contact->open_id), true) != 0 ||
            wx_json_copy(item, "wx_app_id", contact->app_id,
                         sizeof(contact->app_id), true) != 0 ||
            wx_json_copy(item, "wx_model_id", contact->model_id,
                         sizeof(contact->model_id), true) != 0)
        {
            memset(contact, 0, sizeof(*contact));
            continue;
        }
        (void)wx_json_copy(item, "remark", contact->remark,
                           sizeof(contact->remark), false);
        wx_make_display(contact->remark, contact->open_id, count,
                        contact->display);
        ++count;
    }
    cJSON_Delete(root);

    liot_rtos_enter_critical();
    memcpy(s_contacts, contacts, sizeof(contacts));
    s_contact_count = count;
    if (s_selected_contact >= count)
    {
        s_selected_contact = 0U;
    }
    ++s_contacts_revision;
    if (s_contacts_revision == 0U)
    {
        ++s_contacts_revision;
    }
    if (count > 0U)
    {
        memcpy(s_display_name, s_contacts[s_selected_contact].display,
               sizeof(s_display_name));
    }
    else
    {
        memcpy(s_display_name, "NO CONTACT", sizeof("NO CONTACT"));
    }
    liot_rtos_exit_critical();
    liot_trace("[WX] contacts ready count=%u revision=%u\r\n",
               (unsigned int)count,
               (unsigned int)s_contacts_revision);
    return 0;
}

static int wx_sync_profile(void)
{
    static const char profile[] =
        "{\"screen_width\":1,\"screen_height\":1,"
        "\"audio_rate\":8000,\"audio_channels\":1,"
        "\"up_video_mt\":\"none\",\"down_video_mt\":\"none\"," 
        "\"down_audio_mt\":\"alaw\",\"no_video\":true,"
        "\"calling_timeout_sec\":30}";
    int http_status = 0;
    int ret;

    wx_set_state(DEMO_WECHAT_SYNCING, 0);
    memset(s_http_response, 0, sizeof(s_http_response));
    ret = demo_binding_service_request(
        DEMO_BIND_SERVICE_VOIP, DEMO_BIND_HTTP_POST,
        "/v1/voip/device/profile", profile,
        s_http_response, sizeof(s_http_response), &http_status);
    if (ret != 0 || http_status != 200 ||
        wx_business_code_ok(s_http_response) != 0)
    {
        liot_trace("[WX] profile failed ret=%d status=%d\r\n",
                   ret, http_status);
        return WX_ERR_SERVICE;
    }
    liot_trace("[WX] audio-only profile ready ALAW/8k/mono/20ms video=none\r\n");
    /* Incoming calling remains available even if an empty/stale contact list
     * cannot be refreshed, so contact failure is non-fatal after profile OK. */
    (void)wx_fetch_contacts();
    s_profile_ready = true;
    wx_set_state(DEMO_WECHAT_READY, 0);
    return 0;
}

static int wx_start_whip(const wx_mqtt_event_t *event)
{
    wx_whip_context_t *context;
    uint32_t managed_generation;
    bool local_idle;
    int ret;

    if (event == NULL || event->peer_id[0] == '\0' || event->token[0] == '\0')
    {
        return WX_ERR_WHIP;
    }
    liot_rtos_enter_critical();
    local_idle = wx_local_rtc_idle_locked();
    liot_rtos_exit_critical();
    if (!local_idle)
    {
        liot_trace("[WX] WHIP deferred: previous local RTC work pending\r\n");
        /* Preserve this room so the caller's exception cleanup can close it
         * on the service instead of leaving a new orphaned notification. */
        s_call = *event;
        return WX_ERR_WHIP;
    }
    ret = demo_tirtc_session_claim(DEMO_TIRTC_OWNER_WECHAT,
                                   &managed_generation);
    if (ret != 0)
    {
        s_call = *event;
        liot_trace("[WX] managed session claim failed ret=%d\r\n", ret);
        return ret;
    }
    liot_rtos_enter_critical();
    s_tirtc_session_generation = managed_generation;
    s_tirtc_owned = true;
    s_tirtc_release_error_logged = false;
    liot_rtos_exit_critical();
    ++s_generation;
    if (s_generation == 0U)
    {
        ++s_generation;
    }
    s_call = *event;
    s_whip_done = false;
    s_whip_error = 0;
    s_whip_conn = NULL;
    s_start_pending = false;
    s_remote_hangup_pending = false;
    s_conn_error_pending = false;
    s_disconnected_pending = false;
    context = wx_whip_context_acquire();
    if (context == NULL)
    {
        return WX_ERR_WHIP;
    }
    s_active_whip_context = context;
    wx_set_state(DEMO_WECHAT_CONNECTING, 0);
    s_deadline_ms = 0U;
    liot_trace("[WX] WHIP submit peer_len=%u token_len=%u\r\n",
               (unsigned int)strlen(event->peer_id),
               (unsigned int)strlen(event->token));
    ret = demo_tirtc_whip_connect(DEMO_TIRTC_OWNER_WECHAT,
                                  s_tirtc_session_generation,
                                  event->peer_id, event->token,
                                  wx_whip_connect_callback, context);
    liot_trace("[WX] TiRtcWhipConnect submit ret=%d\r\n", ret);
    if (ret != 0)
    {
        liot_rtos_enter_critical();
        context->pending = false;
        if (s_active_whip_context == context)
        {
            s_active_whip_context = NULL;
        }
        liot_rtos_exit_critical();
        return ret;
    }
    s_deadline_ms = liot_rtos_get_running_time() + WX_CONNECT_TIMEOUT_MS;
    return 0;
}

static void wx_mark_session_complete(int result)
{
    if (s_session_sequence != 0U)
    {
        s_completed_sequence = s_session_sequence;
    }
    if (result == 0)
    {
        wx_set_state(wx_transport_ready() ? DEMO_WECHAT_READY :
                                           DEMO_WECHAT_OFFLINE,
                     0);
    }
    else
    {
        s_error_deadline_ms = liot_rtos_get_running_time() +
                              WX_ERROR_VISIBLE_MS;
        wx_set_state(DEMO_WECHAT_ERROR, result);
    }
}

/* Only a user-cancelled or timed-out outbound call is tombstoned.  Transport
 * and local HTTP failures must not suppress a legitimate new call from the
 * same contact.  A non-empty server call_id is required for exact matching.
 */
static void wx_remember_cancelled_outgoing(void)
{
    if (s_state != DEMO_WECHAT_DIALING || s_call.call_id[0] == '\0')
    {
        return;
    }
    memcpy(s_ignored_call_id, s_call.call_id, sizeof(s_ignored_call_id));
    s_ignored_call_deadline_ms = liot_rtos_get_running_time() +
                                 WX_IGNORED_CALL_MS;
}

static void wx_remember_recent_room(void)
{
    if (s_call.room_id[0] == '\0')
    {
        return;
    }
    memcpy(s_recent_room_id, s_call.room_id, sizeof(s_recent_room_id));
    memcpy(s_recent_app_id, s_call.app_id, sizeof(s_recent_app_id));
    s_recent_room_deadline_ms = liot_rtos_get_running_time() +
                                WX_RECENT_ROOM_MS;
}

/* hangup_reason applies to an established TiRTC connection.  reject_reason
 * closes a signaling room that never became a connection.  A negative value
 * means that no signal is required.  Service signaling is best effort; local
 * resources are always released even when the network is currently down. */
static void wx_cleanup_session(int result, int hangup_reason,
                                int reject_reason)
{
    tirtc_conn_t connection;
    uint32_t deadline;
    bool done = false;
    bool tirtc_owned;
    uint32_t session_generation;
    char command_body[32];
    int audio_release_ret;
    int disconnect_ret = TIRTC_E_INVALID_HANDLE;

    wx_set_state(DEMO_WECHAT_STOPPING, 0);
    if (reject_reason != WX_CLOSE_NONE && s_call.room_id[0] != '\0')
    {
        (void)wx_reject(&s_call, reject_reason);
    }
    wx_remember_recent_room();
    liot_rtos_enter_critical();
    connection = s_conn != NULL ? (tirtc_conn_t)s_conn :
                                  (tirtc_conn_t)s_whip_conn;
    tirtc_owned = s_tirtc_owned;
    session_generation = s_tirtc_session_generation;
    /* An early 0x2000 can expose the handle before the one authoritative WHIP
     * callback.  If that callback is still pending, let the late callback own
     * the single disconnect instead of disconnecting the same handle twice. */
    if (s_conn == NULL && s_active_whip_context != NULL &&
        s_active_whip_context->pending)
    {
        connection = NULL;
    }
    ++s_generation; /* invalidates a late WHIP callback */
    s_active_whip_context = NULL;
    liot_rtos_exit_critical();
    if (connection == NULL && hangup_reason != WX_CLOSE_NONE &&
        reject_reason == WX_CLOSE_NONE && s_call.room_id[0] != '\0')
    {
        (void)wx_reject(&s_call, hangup_reason);
    }
    if (s_audio_owned)
    {
        (void)demo_ai_audio_stop(&s_audio_lease);
        audio_release_ret = demo_ai_audio_release(&s_audio_lease);
        if (audio_release_ret == 0)
        {
            liot_rtos_enter_critical();
            s_audio_owned = false;
            liot_rtos_exit_critical();
        }
        else
        {
            liot_trace("[WX] audio release failed ret=%d\r\n",
                       audio_release_ret);
        }
    }
    (void)wx_wait_audio_producers();
    wx_discard_audio();

    if (connection != NULL)
    {
        if (hangup_reason != WX_CLOSE_NONE)
        {
            int written = snprintf(command_body, sizeof(command_body),
                                   "{\"reason\":%d}", hangup_reason);
            if (written > 0 && written < (int)sizeof(command_body))
            {
                (void)demo_tirtc_send_command(DEMO_TIRTC_OWNER_WECHAT,
                                              s_tirtc_session_generation,
                                              WX_CMD_HANGUP, command_body,
                                              (uint32_t)written);
            }
        }
        s_closing_conn = connection;
        s_closing_done = false;
    }
    if (tirtc_owned)
    {
        /* This also invalidates a WHIP request which has not produced a public
         * handle yet.  The adapter owns any later success and closes it. */
        disconnect_ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_WECHAT,
                                                session_generation);
        if (connection != NULL && disconnect_ret == 0)
        {
            deadline = liot_rtos_get_running_time() + WX_DISCONNECT_WAIT_MS;
            while (wx_deadline_pending(deadline))
            {
                wx_drain_unwanted();
                liot_rtos_enter_critical();
                done = s_closing_done;
                liot_rtos_exit_critical();
                if (done)
                {
                    break;
                }
                (void)liot_rtos_semaphore_wait(s_event_sem, 50U);
            }
        }
    }
    wx_drain_unwanted();
    liot_rtos_enter_critical();
    s_conn = NULL;
    s_whip_conn = NULL;
    s_closing_conn = NULL;
    s_closing_done = false;
    s_whip_done = false;
    s_start_pending = false;
    s_remote_hangup_pending = false;
    s_conn_error_pending = false;
    s_disconnected_pending = false;
    liot_rtos_exit_critical();
    s_outgoing_session = false;
    s_prebuffer_start_ms = 0U;
    s_deadline_ms = 0U;
    memset(&s_call, 0, sizeof(s_call));
    wx_set_display(s_contact_count > 0U ?
                       s_contacts[s_selected_contact].display : "WX");
    (void)wx_release_tirtc_session();
    wx_mark_session_complete(result);
    liot_trace("[WX] session closed result=%d disconnect=%d settled=%u "
               "rx_drop=%u "
               "tx_drop=%u\r\n",
               result, disconnect_ret, done ? 1U : 0U,
               (unsigned int)s_rx_dropped,
               (unsigned int)s_tx_dropped);
}

static int wx_start_audio(void)
{
    uint8_t speaker_level;
    uint8_t mic_level;
    int ret;

    liot_rtos_enter_critical();
    speaker_level = s_speaker_level;
    mic_level = s_mic_level;
    s_levels_dirty = false;
    liot_rtos_exit_critical();
    ret = demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_WECHAT,
                                &s_audio_lease);
    if (ret != 0)
    {
        liot_trace("[WX] audio acquire failed ret=%d\r\n", ret);
        return WX_ERR_AUDIO;
    }
    liot_rtos_enter_critical();
    s_audio_owned = true;
    liot_rtos_exit_critical();
    (void)demo_ai_audio_set_levels(&s_audio_lease,
                                   speaker_level, mic_level);
    ret = demo_ai_audio_init(&s_audio_lease);
    if (ret != 0)
    {
        return WX_ERR_AUDIO;
    }
    ret = demo_ai_audio_apply_levels(&s_audio_lease);
    if (ret != 0)
    {
        return WX_ERR_AUDIO;
    }
    ret = demo_tirtc_subscribe_audio(DEMO_TIRTC_OWNER_WECHAT,
                                     s_tirtc_session_generation,
                                     WX_STREAM_ID);
    liot_trace("[WX] subscribe downlink stream=%u ret=%d\r\n",
               (unsigned int)WX_STREAM_ID, ret);
    if (ret < 0)
    {
        return ret;
    }
    s_rx_frames = 0U;
    s_tx_frames = 0U;
    s_prebuffer_start_ms = 0U;
    s_downlink_reject_logged = false;
    s_playback_error_logged = false;
    wx_set_state(DEMO_WECHAT_IN_CALL, 0);
    liot_trace("[WX] media started bidirectional G711A/8k/mono "
               "(simultaneous uplink/downlink, no AEC)\r\n");
    return 0;
}

static int wx_play_one(void)
{
    wx_audio_frame_t *message;
    uint8_t slot;
    uint32_t input_samples;
    uint32_t output_samples;
    uint32_t i;
    uint32_t peak = 0U;
    uint8_t input_flags;
    int ret;

    if (liot_rtos_queue_wait(s_audio_queue, &slot, sizeof(slot),
                             LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        return 0;
    }
    wx_audio_queued_decrement();
    if (slot >= WX_AUDIO_QUEUE_DEPTH)
    {
        ++s_rx_dropped;
        return 0;
    }
    message = &s_audio_pool[slot];
    if (message->generation != s_generation)
    {
        wx_audio_slot_release(slot);
        return 1;
    }
    input_samples = message->length;
    input_flags = message->flags;
    if (input_flags == TIRTC_AUDIOSAMPLE_8K16B1C)
    {
        output_samples = input_samples * 2U;
        if (output_samples > WX_PLAY_PCM_MAX_SAMPLES)
        {
            wx_audio_slot_release(slot);
            ++s_rx_dropped;
            return 1;
        }
        for (i = 0U; i < input_samples; ++i)
        {
            int16_t sample = demo_g711_alaw_decode_sample(message->payload[i]);
            s_play_pcm[i * 2U] = sample;
            s_play_pcm[i * 2U + 1U] = sample;
        }
    }
    else
    {
        output_samples = input_samples;
        if (demo_g711_alaw_decode(message->payload, s_play_pcm,
                                  input_samples) != input_samples)
        {
            wx_audio_slot_release(slot);
            ++s_rx_dropped;
            return 1;
        }
    }
    for (i = 0U; i < output_samples; ++i)
    {
        int32_t sample = s_play_pcm[i];
        uint32_t magnitude = (uint32_t)(sample < 0 ? -sample : sample);
        if (magnitude > peak)
        {
            peak = magnitude;
        }
    }
    ret = demo_ai_audio_play(&s_audio_lease, s_play_pcm, output_samples);
    wx_audio_slot_release(slot);
    if (ret != 0)
    {
        ++s_rx_dropped;
        if (!s_playback_error_logged)
        {
            s_playback_error_logged = true;
            liot_trace("[WX] playback failed ret=%d samples=%u peak=%u\r\n",
                       ret, (unsigned int)output_samples,
                       (unsigned int)peak);
        }
        return 0;
    }
    ++s_rx_frames;
    if (s_rx_frames == 1U)
    {
        liot_trace("[WX] first downlink ALAW stream=%u flags=%u bytes=%u "
                   "pcm_samples=%u peak=%u play_ret=0\r\n",
                   (unsigned int)WX_STREAM_ID,
                   (unsigned int)input_flags,
                   (unsigned int)input_samples,
                   (unsigned int)output_samples,
                   (unsigned int)peak);
    }
    return 1;
}

static void wx_service_downlink(void)
{
    uint32_t queued;
    uint32_t now;
    uint32_t i;

    liot_rtos_enter_critical();
    queued = s_audio_queued;
    liot_rtos_exit_critical();
    if (queued == 0U)
    {
        if (demo_ai_audio_play_done(&s_audio_lease))
        {
            s_prebuffer_start_ms = 0U;
        }
        return;
    }

    now = liot_rtos_get_running_time();
    if (demo_ai_audio_play_done(&s_audio_lease))
    {
        /* A fresh or underrun start waits for roughly 60 ms of audio.  Once
         * playback is active, keep feeding Liot_AudioPlay's internal copied
         * queue before TX DMA runs dry and turns the PA off. */
        if (s_prebuffer_start_ms == 0U)
        {
            s_prebuffer_start_ms = now;
        }
        if (queued < WX_PREBUFFER_PACKETS &&
            (int32_t)(now - s_prebuffer_start_ms) <
                (int32_t)WX_PREBUFFER_TIMEOUT_MS)
        {
            return;
        }
    }
    s_prebuffer_start_ms = 0U;
    for (i = 0U; i < WX_AUDIO_QUEUE_DEPTH; ++i)
    {
        if (wx_play_one() <= 0)
        {
            break;
        }
    }
}

static int wx_send_uplink(void)
{
    TIRTCFRAMEINFO frame;
    uint32_t i;
    int ret;

    ret = demo_ai_audio_record_20ms(&s_audio_lease, s_capture_pcm);
    if (ret != 0)
    {
        return WX_ERR_AUDIO;
    }
    for (i = 0U; i < WX_UPLINK_SAMPLES_8K; ++i)
    {
        int32_t mixed = (int32_t)s_capture_pcm[i * 2U] +
                        (int32_t)s_capture_pcm[i * 2U + 1U];
        s_uplink_alaw[i] =
            demo_g711_alaw_encode_sample((int16_t)(mixed / 2));
    }
    memset(&frame, 0, sizeof(frame));
    frame.stream_id = WX_STREAM_ID;
    frame.media = TIRTC_AUDIO_ALAW;
    frame.flags = TIRTC_AUDIOSAMPLE_8K16B1C;
    frame.ts = liot_rtos_get_running_time();
    frame.length = WX_UPLINK_SAMPLES_8K;
    ret = demo_tirtc_send_audio(DEMO_TIRTC_OWNER_WECHAT,
                                s_tirtc_session_generation,
                                &frame, s_uplink_alaw);
    if (ret == TIRTC_E_BUSY)
    {
        ++s_tx_dropped;
        return 0;
    }
    if (ret < 0)
    {
        return ret;
    }
    ++s_tx_frames;
    if (s_tx_frames == 1U)
    {
        liot_trace("[WX] first uplink ALAW bytes=%u\r\n",
                   (unsigned int)WX_UPLINK_SAMPLES_8K);
    }
    return 0;
}

static void wx_new_session_sequence(void)
{
    ++s_session_sequence;
    if (s_session_sequence == 0U)
    {
        ++s_session_sequence;
    }
}

static int wx_start_outgoing_call(void)
{
    wx_contact_t contact;
    demo_binding_tirtc_identity_t identity;
    cJSON *body = NULL;
    cJSON *root = NULL;
    const cJSON *code;
    const cJSON *data;
    const cJSON *call_id;
    char *json = NULL;
    int http_status = 0;
    int ret = WX_ERR_CALL_HTTP;

    liot_rtos_enter_critical();
    if (s_request_hangup)
    {
        liot_rtos_exit_critical();
        return 0;
    }
    if (s_contact_count == 0U || s_selected_contact >= s_contact_count)
    {
        liot_rtos_exit_critical();
        return WX_ERR_NO_CONTACT;
    }
    contact = s_contacts[s_selected_contact];
    liot_rtos_exit_critical();
    memset(&identity, 0, sizeof(identity));
    if (demo_binding_get_tirtc_identity(&identity) != 0)
    {
        return WX_ERR_CALL_HTTP;
    }

    wx_new_session_sequence();
    s_outgoing_session = true;
    memset(&s_call, 0, sizeof(s_call));
    memcpy(s_call.open_id, contact.open_id, sizeof(s_call.open_id));
    memcpy(s_call.app_id, contact.app_id, sizeof(s_call.app_id));
    memcpy(s_call.model_id, contact.model_id, sizeof(s_call.model_id));
    memcpy(s_call.remark, contact.remark, sizeof(s_call.remark));
    wx_set_display(contact.display);
    wx_set_state(DEMO_WECHAT_DIALING, 0);
    /* Preserve a KEY2 received while preparing in READY, before any HTTP
     * request can make the peer ring. */
    if (s_request_hangup)
    {
        memset(&identity, 0, sizeof(identity));
        wx_cleanup_session(0, WX_CLOSE_NONE, WX_CLOSE_NONE);
        return 0;
    }
    /* Do not consume the 30-second ringing budget while HTTP is in flight. */
    s_deadline_ms = 0U;

    body = cJSON_CreateObject();
    if (body == NULL ||
        !cJSON_AddStringToObject(body, "device_id", identity.device_id) ||
        !cJSON_AddStringToObject(body, "wx_app_id", contact.app_id) ||
        !cJSON_AddStringToObject(body, "wx_user_openid", contact.open_id) ||
        !cJSON_AddStringToObject(body, "wx_model_id", contact.model_id) ||
        !cJSON_AddStringToObject(body, "wx_room_type", "voice") ||
        !cJSON_AddNumberToObject(body, "wx_version_type", 2) ||
        !cJSON_AddNumberToObject(body, "wx_caller_camera_status", 1) ||
        !cJSON_AddNumberToObject(body, "wx_listener_camera_status", 1) ||
        !cJSON_AddNumberToObject(body, "calling_timeout_sec", 30))
    {
        goto cleanup;
    }
    json = cJSON_PrintUnformatted(body);
    if (json == NULL)
    {
        goto cleanup;
    }
    memset(s_http_response, 0, sizeof(s_http_response));
    if (demo_binding_service_request(
            DEMO_BIND_SERVICE_VOIP, DEMO_BIND_HTTP_POST,
            "/v1/voip/device/call", json,
            s_http_response, sizeof(s_http_response), &http_status) != 0 ||
        http_status != 200)
    {
        goto cleanup;
    }
    root = cJSON_Parse(s_http_response);
    code = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "code") : NULL;
    data = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    call_id = cJSON_IsObject(data) ?
                  cJSON_GetObjectItemCaseSensitive(data, "call_id") : NULL;
    if (!cJSON_IsNumber(code))
    {
        goto cleanup;
    }
    if (code->valueint == 40205)
    {
        s_request_refresh = true;
        liot_trace("[WX] selected contact is stale; refresh requested\r\n");
        goto cleanup;
    }
    if (code->valueint != 0)
    {
        goto cleanup;
    }
    if (cJSON_IsString(call_id) && call_id->valuestring != NULL &&
        strlen(call_id->valuestring) < sizeof(s_call.call_id))
    {
        memcpy(s_call.call_id, call_id->valuestring,
               strlen(call_id->valuestring) + 1U);
    }
    liot_trace("[WX] outgoing accepted by signaling call_id=%s\r\n",
               s_call.call_id[0] != '\0' ? "present" : "pending");
    s_deadline_ms = liot_rtos_get_running_time() + WX_OUTGOING_TIMEOUT_MS;
    ret = 0;

cleanup:
    cJSON_Delete(root);
    cJSON_Delete(body);
    /* cJSON may be configured with RTOS allocator hooks.  Its print buffer
     * must be returned through cJSON_free(), never the C library free(). */
    cJSON_free(json);
    memset(&identity, 0, sizeof(identity));
    liot_trace("[WX] outgoing cleanup done ret=%d free_heap=%u\r\n",
               ret, (unsigned int)liot_xPortGetFreeHeapSize());
    if (ret != 0)
    {
        wx_cleanup_session(ret, WX_CLOSE_NONE, WX_CLOSE_NONE);
    }
    return ret;
}

static bool wx_incoming_matches_outgoing(const wx_mqtt_event_t *event)
{
    bool open_id_matches;
    bool call_id_matches;

    if (event == NULL)
    {
        return false;
    }
    open_id_matches = event->open_id[0] == '\0' ||
                      s_call.open_id[0] == '\0' ||
                      strcmp(event->open_id, s_call.open_id) == 0;
    /* wx_call_id is optional in the documented call_incoming payload.  The
     * official client consumes the first valid join while one outbound call
     * is pending instead of requiring the HTTP and MQTT IDs to both exist. */
    call_id_matches = event->call_id[0] == '\0' ||
                      s_call.call_id[0] == '\0' ||
                      strcmp(event->call_id, s_call.call_id) == 0;
    return open_id_matches && call_id_matches;
}

static bool wx_incoming_matches_ignored(const wx_mqtt_event_t *event)
{
    if (!wx_deadline_pending(s_ignored_call_deadline_ms))
    {
        s_ignored_call_id[0] = '\0';
        return false;
    }
    return event->call_id[0] != '\0' && s_ignored_call_id[0] != '\0' &&
           strcmp(event->call_id, s_ignored_call_id) == 0;
}

static bool wx_incoming_matches_active_room(const wx_mqtt_event_t *event)
{
    return event != NULL && event->room_id[0] != '\0' &&
           s_call.room_id[0] != '\0' &&
           strcmp(event->room_id, s_call.room_id) == 0 &&
           (event->app_id[0] == '\0' || s_call.app_id[0] == '\0' ||
            strcmp(event->app_id, s_call.app_id) == 0) &&
           wx_state_has_session(s_state);
}

static bool wx_incoming_matches_recent_room(const wx_mqtt_event_t *event)
{
    if (!wx_deadline_pending(s_recent_room_deadline_ms))
    {
        s_recent_room_id[0] = '\0';
        s_recent_app_id[0] = '\0';
        s_recent_room_deadline_ms = 0U;
        return false;
    }
    return event != NULL && event->room_id[0] != '\0' &&
           s_recent_room_id[0] != '\0' &&
           strcmp(event->room_id, s_recent_room_id) == 0 &&
           (event->app_id[0] == '\0' || s_recent_app_id[0] == '\0' ||
            strcmp(event->app_id, s_recent_app_id) == 0);
}

static bool wx_incoming_is_own_outgoing(const wx_mqtt_event_t *event)
{
    demo_binding_tirtc_identity_t identity;
    bool own_call = false;

    if (event == NULL || event->call_id[0] == '\0' || event->from[0] == '\0')
    {
        return false;
    }
    memset(&identity, 0, sizeof(identity));
    if (demo_binding_get_tirtc_identity(&identity) == 0)
    {
        own_call = strcmp(event->from, identity.device_id) == 0;
    }
    memset(&identity, 0, sizeof(identity));
    return own_call;
}

static bool wx_other_features_idle(void)
{
    bool idle = true;

#ifdef HWDEMO_AI_CHAT_EN
    idle = idle && demo_ai_chat_is_idle();
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    idle = idle && demo_dev_chat_is_idle();
#endif
#ifdef HWDEMO_LIVE_TALK_EN
    idle = idle && demo_live_talk_wait_idle(0U);
#endif
    return idle;
}

static void wx_process_incoming(const wx_mqtt_event_t *event)
{
    demo_wechat_state_e state = s_state;
    bool home_allowed = s_home_allowed;
    bool local_idle;
    char display[WX_DISPLAY_MAX];
    int ret;

    /* MQTT reconnect/QoS may redeliver call_incoming.  Rejecting a duplicate
     * of the active room would terminate our own call, so active and recently
     * closed rooms are idempotently ignored like the official implementation. */
    if (wx_incoming_matches_active_room(event))
    {
        liot_trace("[WX] duplicate current room ignored\r\n");
        return;
    }
    if (wx_incoming_matches_recent_room(event))
    {
        liot_trace("[WX] late recent room ignored\r\n");
        return;
    }
    if (wx_incoming_matches_ignored(event))
    {
        liot_trace("[WX] ignored late outgoing callback; rejecting\r\n");
        (void)wx_reject(event, WX_HANGUP_REASON_REJECT);
        return;
    }

    if (state == DEMO_WECHAT_DIALING)
    {
        if (!wx_incoming_matches_outgoing(event))
        {
            (void)wx_reject(event, WX_HANGUP_REASON_BUSY);
            return;
        }
        liot_trace("[WX] outgoing peer answered; connecting WHIP\r\n");
        ret = wx_start_whip(event);
        if (ret != 0)
        {
            wx_cleanup_session(ret, WX_CLOSE_NONE,
                               WX_HANGUP_REASON_EXCEPTION);
        }
        return;
    }

    /* If local DIALING state was lost, surface this device's own callback as
     * a normal HOME-page ring instead of silently starting audio.  The same
     * single-confirm path then performs the LIVE/AI audio handoff safely. */
    if (state == DEMO_WECHAT_READY && home_allowed &&
        wx_incoming_is_own_outgoing(event))
    {
        liot_trace("[WX] recovered own outbound callback as HOME ring\r\n");
    }

    if (state != DEMO_WECHAT_READY || !home_allowed)
    {
        liot_trace("[WX] incoming rejected busy state=%d home=%u\r\n",
                   (int)state, home_allowed ? 1U : 0U);
        (void)wx_reject(event, WX_HANGUP_REASON_BUSY);
        return;
    }
    /* Serialize only the final HOME admission transaction.  Ringing itself
     * still does not touch ES8311, preserving the proven answer handoff. */
    if (!demo_tirtc_admission_try_enter())
    {
        liot_trace("[WX] incoming rejected: admission busy\r\n");
        (void)wx_reject(event, WX_HANGUP_REASON_BUSY);
        return;
    }
    state = s_state;
    home_allowed = s_home_allowed;
    liot_rtos_enter_critical();
    local_idle = wx_local_rtc_idle_locked();
    liot_rtos_exit_critical();
    if (state != DEMO_WECHAT_READY || !home_allowed ||
        !local_idle || !wx_other_features_idle())
    {
        demo_tirtc_admission_leave();
        liot_trace("[WX] incoming rejected after owner recheck\r\n");
        (void)wx_reject(event, WX_HANGUP_REASON_BUSY);
        return;
    }
    s_outgoing_session = false;
    s_call = *event;
    ++s_incoming_generation;
    if (s_incoming_generation == 0U)
    {
        ++s_incoming_generation;
    }
    wx_new_session_sequence();
    wx_make_display(event->remark, event->open_id, 0U, display);
    wx_set_display(display);
    s_deadline_ms = (event->received_ms != 0U ? event->received_ms :
                                                   liot_rtos_get_running_time()) +
                    WX_INCOMING_TIMEOUT_MS;
    wx_set_state(DEMO_WECHAT_RINGING, 0);
    demo_tirtc_admission_leave();
    liot_trace("[WX] incoming call generation=%u; double KEY1 to answer\r\n",
               (unsigned int)s_incoming_generation);
}

static void wx_process_cancel(const wx_mqtt_event_t *event)
{
    bool matches = false;

    if (s_state == DEMO_WECHAT_DIALING &&
        event->call_id[0] != '\0' && s_call.call_id[0] != '\0' &&
        strcmp(event->call_id, s_call.call_id) == 0)
    {
        matches = true;
    }
    else if (event->room_id[0] != '\0' && s_call.room_id[0] != '\0' &&
             strcmp(event->room_id, s_call.room_id) == 0)
    {
        matches = true;
    }
    if (matches)
    {
        liot_trace("[WX] remote call_cancel matched current session\r\n");
        wx_cleanup_session(0, WX_CLOSE_NONE, WX_CLOSE_NONE);
    }
}

static void wx_process_mqtt(void)
{
    wx_mqtt_event_t *event;
    uint8_t slot;

    while (liot_rtos_queue_wait(s_mqtt_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        if (slot >= WX_MQTT_EVENT_DEPTH)
        {
            continue;
        }
        event = &s_mqtt_pool[slot];
        switch (event->type)
        {
        case WX_MQTT_INCOMING:
            wx_process_incoming(event);
            break;
        case WX_MQTT_CANCEL:
            wx_process_cancel(event);
            break;
        case WX_MQTT_CONTACTS_UPDATE:
            s_request_refresh = true;
            liot_trace("[WX] callers_update queued\r\n");
            break;
        default:
            break;
        }
        wx_mqtt_slot_release(slot);
    }
}

static void wx_process_control(void)
{
    bool call;
    bool hangup;
    bool refresh;
    uint32_t answer_generation;

    liot_rtos_enter_critical();
    call = s_request_call && s_state == DEMO_WECHAT_READY;
    s_request_call = false;
    if (call)
    {
        s_call_starting = true;
    }
    hangup = s_request_hangup;
    s_request_hangup = false;
    refresh = s_request_refresh;
    answer_generation = s_request_answer_generation;
    s_request_answer_generation = 0U;
    liot_rtos_exit_critical();

    if (hangup && wx_state_has_session(s_state))
    {
        if (s_state == DEMO_WECHAT_DIALING)
        {
            wx_remember_cancelled_outgoing();
        }
        wx_cleanup_session(
            0,
            (s_state == DEMO_WECHAT_CONNECTING ||
             s_state == DEMO_WECHAT_WAIT_START ||
             s_state == DEMO_WECHAT_IN_CALL) ?
                WX_HANGUP_REASON_MANUAL : WX_CLOSE_NONE,
            s_state == DEMO_WECHAT_RINGING ?
                WX_HANGUP_REASON_REJECT : WX_CLOSE_NONE);
        return;
    }
    if (answer_generation != 0U && s_state == DEMO_WECHAT_RINGING &&
        answer_generation == s_incoming_generation)
    {
        int ret = wx_start_whip(&s_call);
        if (ret != 0)
        {
            wx_cleanup_session(ret, WX_CLOSE_NONE,
                               WX_HANGUP_REASON_EXCEPTION);
        }
        return;
    }
    if (call)
    {
        if (s_state == DEMO_WECHAT_READY)
        {
            (void)wx_start_outgoing_call();
        }
        liot_rtos_enter_critical();
        s_call_starting = false;
        if (!wx_state_has_session(s_state))
        {
            s_request_hangup = false;
        }
        liot_rtos_exit_critical();
        return;
    }
    if (refresh && s_profile_ready && s_state == DEMO_WECHAT_READY &&
        (s_contacts_retry_at == 0U ||
         !wx_deadline_pending(s_contacts_retry_at)))
    {
        int ret;

        /* Clear immediately before I/O.  A new MQTT update arriving during
         * the request sets the flag again and therefore cannot be lost. */
        liot_rtos_enter_critical();
        s_request_refresh = false;
        liot_rtos_exit_critical();
        ret = wx_fetch_contacts();
        if (ret == 0)
        {
            s_contacts_retry_at = 0U;
        }
        else
        {
            s_contacts_retry_at = liot_rtos_get_running_time() +
                                  WX_SYNC_RETRY_MS;
            s_request_refresh = true;
        }
    }
}

static void wx_process_connection_events(void)
{
    int ret;

    if (s_whip_done && s_state == DEMO_WECHAT_CONNECTING)
    {
        s_whip_done = false;
        s_active_whip_context = NULL;
        if (s_whip_error != 0 || s_whip_conn == NULL)
        {
            ret = s_whip_error != 0 ? s_whip_error : WX_ERR_WHIP;
            wx_cleanup_session(ret, WX_CLOSE_NONE,
                               WX_HANGUP_REASON_EXCEPTION);
            return;
        }
        s_conn = (tirtc_conn_t)s_whip_conn;
        /* Both roles wait for the server's CALL_CONNECTED (0x2000) before
         * opening the codec, exactly like the current official example.  The
         * outbound server path is allowed a longer signaling window. */
        s_deadline_ms = liot_rtos_get_running_time() +
                        (s_outgoing_session ?
                             WX_OUTGOING_START_TIMEOUT_MS :
                             WX_START_TIMEOUT_MS);
        wx_set_state(DEMO_WECHAT_WAIT_START, 0);
        liot_trace("[WX] %s WHIP ready; waiting command 0x2000\r\n",
                   s_outgoing_session ? "outgoing" : "incoming");
    }
    if (s_start_pending && s_state == DEMO_WECHAT_WAIT_START)
    {
        s_start_pending = false;
        ret = wx_start_audio();
        if (ret != 0)
        {
            wx_cleanup_session(ret, WX_HANGUP_REASON_DEVICE,
                               WX_CLOSE_NONE);
            return;
        }
    }
    if (s_remote_hangup_pending || s_disconnected_pending ||
        s_conn_error_pending)
    {
        bool already_disconnected = s_disconnected_pending;

        ret = s_conn_error_pending && s_conn_error != 0 ?
                  s_conn_error : 0;
        s_remote_hangup_pending = false;
        s_disconnected_pending = false;
        s_conn_error_pending = false;
        if (already_disconnected)
        {
            /* The SDK has already retired this handle.  Do not pass it back
             * into managed disconnect, which can double-close the session. */
            s_conn = NULL;
            s_whip_conn = NULL;
            s_closing_conn = NULL;
            s_closing_done = true;
        }
        wx_cleanup_session(ret, WX_CLOSE_NONE, WX_CLOSE_NONE);
    }
}

static void wx_process_timeouts(void)
{
    if (s_state == DEMO_WECHAT_ERROR &&
        !wx_deadline_pending(s_error_deadline_ms))
    {
        wx_set_state(wx_transport_ready() && s_profile_ready ?
                         DEMO_WECHAT_READY : DEMO_WECHAT_OFFLINE,
                     0);
        return;
    }
    if (s_deadline_ms == 0U || wx_deadline_pending(s_deadline_ms))
    {
        return;
    }
    if (s_state == DEMO_WECHAT_RINGING)
    {
        liot_trace("[WX] incoming ring timeout\r\n");
        wx_cleanup_session(0, WX_CLOSE_NONE,
                           WX_HANGUP_REASON_TIMEOUT);
    }
    else if (s_state == DEMO_WECHAT_DIALING)
    {
        liot_trace("[WX] outgoing call timeout\r\n");
        wx_remember_cancelled_outgoing();
        wx_cleanup_session(WX_ERR_CONNECT_TIMEOUT, WX_CLOSE_NONE,
                           WX_CLOSE_NONE);
    }
    else if (s_state == DEMO_WECHAT_CONNECTING)
    {
        liot_trace("[WX] WHIP callback timeout\r\n");
        wx_cleanup_session(WX_ERR_CONNECT_TIMEOUT,
                           WX_HANGUP_REASON_TIMEOUT, WX_CLOSE_NONE);
    }
    else if (s_state == DEMO_WECHAT_WAIT_START)
    {
        liot_trace("[WX] command 0x2000 timeout\r\n");
        wx_cleanup_session(WX_ERR_START_TIMEOUT,
                           WX_HANGUP_REASON_TIMEOUT, WX_CLOSE_NONE);
    }
}

void demo_wechat_task(void *argv)
{
    uint32_t sync_retry_at = 0U;
    bool mqtt_registered = false;
    bool transport_was_ready = false;
    bool transport_ready;
    bool levels_dirty;
    bool release_pending;
    uint8_t speaker_level;
    uint8_t mic_level;
    int ret;

    (void)argv;
    if (liot_rtos_semaphore_create(&s_event_sem, 0U) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_mqtt_queue, sizeof(uint8_t),
                               WX_MQTT_EVENT_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_audio_queue, sizeof(uint8_t),
                               WX_AUDIO_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_unwanted_queue, sizeof(tirtc_conn_t),
                               WX_UNWANTED_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (demo_formal_mqtt_register_handler(wx_formal_mqtt_handler, NULL) != 0)
    {
        goto init_failed;
    }
    mqtt_registered = true;
    demo_tirtc_set_feature_listener(DEMO_TIRTC_FEATURE_WECHAT,
                                    &s_tirtc_listener);
    liot_trace("[WX] worker ready; waiting binding/MQTT/TiRTC "
               "free_heap=%u\r\n",
               (unsigned int)liot_xPortGetFreeHeapSize());

    while (1)
    {
        wx_drain_unwanted();
        liot_rtos_enter_critical();
        release_pending = s_tirtc_owned &&
                          !wx_state_has_session(s_state);
        liot_rtos_exit_critical();
        if (release_pending && !wx_release_tirtc_session())
        {
            (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
            continue;
        }
        transport_ready = wx_transport_ready();
        if (!transport_ready)
        {
            if (wx_state_has_session(s_state))
            {
                wx_cleanup_session(WX_ERR_CONNECTION, WX_CLOSE_NONE,
                                   WX_CLOSE_NONE);
            }
            s_profile_ready = false;
            transport_was_ready = false;
            wx_set_state(DEMO_WECHAT_OFFLINE, 0);
            (void)liot_rtos_semaphore_wait(s_event_sem, 500U);
            continue;
        }
        if (!transport_was_ready)
        {
            transport_was_ready = true;
            sync_retry_at = 0U;
        }
        if (!s_profile_ready)
        {
            if (sync_retry_at == 0U || !wx_deadline_pending(sync_retry_at))
            {
                ret = wx_sync_profile();
                if (ret != 0)
                {
                    wx_set_state(DEMO_WECHAT_ERROR, ret);
                    sync_retry_at = liot_rtos_get_running_time() +
                                    WX_SYNC_RETRY_MS;
                }
            }
            (void)liot_rtos_semaphore_wait(s_event_sem, 250U);
            continue;
        }

        wx_process_mqtt();
        wx_process_control();
        wx_process_connection_events();
        wx_process_timeouts();

        liot_rtos_enter_critical();
        levels_dirty = s_levels_dirty;
        speaker_level = s_speaker_level;
        mic_level = s_mic_level;
        if (levels_dirty)
        {
            s_levels_dirty = false;
        }
        liot_rtos_exit_critical();
        if (levels_dirty && s_state == DEMO_WECHAT_IN_CALL)
        {
            (void)demo_ai_audio_set_levels(&s_audio_lease,
                                           speaker_level, mic_level);
            (void)demo_ai_audio_apply_levels(&s_audio_lease);
        }

        if (s_state == DEMO_WECHAT_IN_CALL)
        {
            wx_service_downlink();
            /* Playback data is copied into the driver's TX queue.  The 20 ms
             * record call drives the independent RX direction and naturally
             * paces uplink. */
            ret = wx_send_uplink();
            if (ret < 0)
            {
                liot_trace("[WX] uplink/audio failed ret=%d\r\n", ret);
                wx_cleanup_session(ret, WX_HANGUP_REASON_DEVICE,
                                   WX_CLOSE_NONE);
            }
            continue;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, WX_IDLE_WAIT_MS);
    }

init_failed:
    if (mqtt_registered)
    {
        demo_formal_mqtt_unregister_handler(wx_formal_mqtt_handler, NULL);
    }
    if (s_unwanted_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_unwanted_queue);
        s_unwanted_queue = NULL;
    }
    if (s_audio_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_audio_queue);
        s_audio_queue = NULL;
    }
    if (s_mqtt_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_mqtt_queue);
        s_mqtt_queue = NULL;
    }
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_delete(s_event_sem);
        s_event_sem = NULL;
    }
    liot_trace("[WX] worker initialization failed\r\n");
    wx_set_state(DEMO_WECHAT_ERROR, WX_ERR_INIT);
    liot_rtos_task_delete(NULL);
}

void demo_wechat_mark_unavailable(void)
{
    wx_set_state(DEMO_WECHAT_ERROR, WX_ERR_INIT);
}

void demo_wechat_enter(void)
{
    liot_rtos_enter_critical();
    if (s_state == DEMO_WECHAT_READY)
    {
        s_request_refresh = true;
    }
    liot_rtos_exit_critical();
    wx_signal();
}

void demo_wechat_leave(void)
{
    liot_rtos_enter_critical();
    /* KEY2 may race the worker before a queued KEY1 call request changes the
     * state from READY to DIALING.  Cancel that request unconditionally so a
     * call can never start after the UI has already returned HOME. */
    s_request_call = false;
    if (s_state != DEMO_WECHAT_STOPPING &&
        (s_call_starting || wx_state_has_session(s_state)))
    {
        s_request_hangup = true;
    }
    liot_rtos_exit_critical();
    wx_signal();
}

void demo_wechat_select_next(void)
{
    liot_rtos_enter_critical();
    if (s_state == DEMO_WECHAT_READY && s_contact_count > 0U)
    {
        s_selected_contact =
            (uint8_t)((s_selected_contact + 1U) % s_contact_count);
        memcpy(s_display_name, s_contacts[s_selected_contact].display,
               sizeof(s_display_name));
    }
    liot_rtos_exit_critical();
}

bool demo_wechat_call_selected(void)
{
    bool accepted = false;

    liot_rtos_enter_critical();
    if (s_state == DEMO_WECHAT_READY && s_contact_count > 0U &&
        !s_request_call && !s_call_starting && wx_local_rtc_idle_locked())
    {
        s_request_call = true;
        accepted = true;
    }
    liot_rtos_exit_critical();
    if (accepted)
    {
        wx_signal();
    }
    return accepted;
}

bool demo_wechat_answer(uint32_t incoming_generation)
{
    bool accepted = false;

    liot_rtos_enter_critical();
    if (s_state == DEMO_WECHAT_RINGING &&
        incoming_generation != 0U &&
        incoming_generation == s_incoming_generation &&
        s_request_answer_generation == 0U)
    {
        s_request_answer_generation = incoming_generation;
        accepted = true;
    }
    liot_rtos_exit_critical();
    if (accepted)
    {
        wx_signal();
    }
    return accepted;
}

void demo_wechat_hangup(void)
{
    liot_rtos_enter_critical();
    s_request_call = false;
    if (s_state != DEMO_WECHAT_STOPPING &&
        (s_call_starting || wx_state_has_session(s_state)))
    {
        s_request_hangup = true;
    }
    liot_rtos_exit_critical();
    wx_signal();
}

void demo_wechat_refresh_contacts(void)
{
    s_request_refresh = true;
    wx_signal();
}

void demo_wechat_get_snapshot(demo_wechat_snapshot_t *out)
{
    if (out == NULL)
    {
        return;
    }
    liot_rtos_enter_critical();
    out->state = s_state;
    out->error = s_error;
    out->contact_count = s_contact_count;
    out->selected_contact = s_selected_contact;
    out->contacts_revision = s_contacts_revision;
    out->incoming_generation = s_incoming_generation;
    out->session_sequence = s_session_sequence;
    out->completed_sequence = s_completed_sequence;
    out->rx_dropped = s_rx_dropped;
    out->tx_dropped = s_tx_dropped;
    memcpy(out->display_name, s_display_name, sizeof(out->display_name));
    liot_rtos_exit_critical();
}

void demo_wechat_set_home_allowed(bool allowed)
{
    liot_rtos_enter_critical();
    s_home_allowed = allowed;
    liot_rtos_exit_critical();
}

bool demo_wechat_blocks_live(void)
{
    return wx_state_has_session(s_state);
}

bool demo_wechat_is_idle(void)
{
    bool idle;

    liot_rtos_enter_critical();
    idle = !wx_state_has_session(s_state) && !s_request_call &&
           !s_call_starting &&
           !s_request_hangup && s_request_answer_generation == 0U &&
           wx_local_rtc_idle_locked();
    liot_rtos_exit_critical();
    return idle;
}

void demo_wechat_set_audio_levels(uint8_t speaker_level, uint8_t mic_level)
{
    if (speaker_level < DEMO_AI_AUDIO_LEVEL_MIN)
    {
        speaker_level = DEMO_AI_AUDIO_LEVEL_MIN;
    }
    else if (speaker_level > DEMO_AI_AUDIO_LEVEL_MAX)
    {
        speaker_level = DEMO_AI_AUDIO_LEVEL_MAX;
    }
    if (mic_level < DEMO_AI_AUDIO_LEVEL_MIN)
    {
        mic_level = DEMO_AI_AUDIO_LEVEL_MIN;
    }
    else if (mic_level > DEMO_AI_AUDIO_LEVEL_MAX)
    {
        mic_level = DEMO_AI_AUDIO_LEVEL_MAX;
    }
    liot_rtos_enter_critical();
    s_speaker_level = speaker_level;
    s_mic_level = mic_level;
    s_levels_dirty = true;
    liot_rtos_exit_critical();
    wx_signal();
}
