/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Device-to-device audio calling for NT26F6D0/F6D_A.
 *
 * This is a small board port of the official TiRTC minimal-system call flow:
 *   call-server HTTP + formal MQTT signaling + one shared TiRTC P2P runtime.
 * The caller creates a room and waits for an inbound P2P connection.  The
 * callee obtains a one-shot token, connects to the caller and confirms the
 * room with command 0x2000.  Both directions then use G.711 A-law, 8 kHz,
 * mono, 20 ms on stream 10.  This board has no camera, so an incoming video
 * room is deliberately downgraded to its audio stream, matching the
 * official audio-only simulator policy.  SDK/MQTT callbacks only copy
 * bounded events; this worker owns HTTP, JSON, disconnect and ES8311.
 */

#include "device_call.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "audio_device.h"
#ifdef HWDEMO_AI_CHAT_EN
#include "ai_chat.h"
#endif
#include "device_binding.h"
#include "formal_mqtt.h"
#include "g711_codec.h"
#ifdef HWDEMO_LIVE_TALK_EN
#include "platform_intercom.h"
#endif
#include "tirtc_runtime.h"
#ifdef HWDEMO_WECHAT_EN
#include "wechat_call.h"
#endif
#include "liot_log.h"
#include "liot_os.h"
#include "tirtc/tiRTC.h"

#define DEV_CONTACT_MAX                    8U
#define DEV_ID_MAX                        65U
#define DEV_ROOM_MAX                     129U
/* Current F6D_A libTiRTC builds a bounded HTTP Authorization header.  Keep
 * the connect token below 601 bytes and fail visibly instead of passing an
 * oversized value into this library build. */
#define DEV_TOKEN_MAX                    601U
#define DEV_NAME_MAX                     129U
#define DEV_DISPLAY_MAX                   12U
#define DEV_HTTP_RESPONSE_MAX            4096U

#define DEV_MQTT_EVENT_DEPTH                8U
#define DEV_SIGNAL_EVENT_DEPTH              8U
#define DEV_SIGNAL_PAYLOAD_MAX             160U
#define DEV_AUDIO_QUEUE_DEPTH                8U
#define DEV_AUDIO_FRAME_BYTES              160U
/* Accept up to four coalesced 20 ms G.711 A-law packets without allocating
 * from the heap in the SDK callback. */
#define DEV_AUDIO_PAYLOAD_MAX \
    (DEV_AUDIO_FRAME_BYTES * 4U)
/* The official minimal adapter permits exactly one asynchronous connect in
 * flight.  Keep its borrowed request storage intact until the SDK callback
 * completes, even when the business-side watchdog has ended the session. */
#define DEV_CONNECT_CONTEXT_COUNT            1U
#define DEV_CONNECT_MAX_ATTEMPTS              1U
#define DEV_ROOM_ACTION_DEPTH                 8U
#define DEV_ROOM_ACTION_PATH_MAX             32U
#define DEV_ROOM_ACTION_REASON_MAX           16U
#define DEV_ROOM_ACTION_RESPONSE_MAX        256U
#define DEV_ROOM_ACTION_TASK_STACK    (12U * 1024U)
#define DEV_ROOM_ACTION_RETRIES               2U
#define DEV_ROOM_TOMBSTONE_MS              60000U

#define DEV_STREAM_ID                       10U
#define DEV_CMD_CONFIRM                 0x2000U
#define DEV_CMD_HANGUP                  0x2001U
#define DEV_UPLINK_SAMPLES_8K \
    (DEMO_AI_AUDIO_FRAME_SAMPLES / 2U)
#define DEV_PLAY_PCM_MAX_SAMPLES \
    (DEV_AUDIO_PAYLOAD_MAX * 2U)

#define DEV_HTTP_TIMEOUT_MS              10000U
#define DEV_OUTGOING_TIMEOUT_MS          30000U
#define DEV_INCOMING_TIMEOUT_MS          45000U
/* libTiRTC's internal P2P attempt also expires at about 10 seconds.  Keep
 * the application deadline later so its callback wins the boundary race. */
#define DEV_CONNECT_TIMEOUT_MS           15000U
#define DEV_CONFIRM_TIMEOUT_MS           10000U
#define DEV_AUDIO_PRODUCER_TIMEOUT_MS      1000U
#define DEV_LATE_INCOMING_GUARD_MS       10000U
#define DEV_CONTACT_RETRY_MS             10000U
#define DEV_ROOM_FIRST_CHECK_MS            2000U
#define DEV_ROOM_CHECK_INTERVAL_MS        30000U
#define DEV_EXISTING_ROOM_RETRY_MS          1000U
#define DEV_ERROR_VISIBLE_MS              3000U
#define DEV_IDLE_WAIT_MS                    50U
#define DEV_PREBUFFER_PACKETS                3U
#define DEV_PREBUFFER_TIMEOUT_MS            80U

#define DEV_ERR_INIT                     (-5001)
#define DEV_ERR_SERVICE                  (-5002)
#define DEV_ERR_CONTACTS                 (-5003)
#define DEV_ERR_NO_CONTACT               (-5004)
#define DEV_ERR_CALL                     (-5005)
#define DEV_ERR_CONNECT                  (-5006)
#define DEV_ERR_CONFIRM                  (-5007)
#define DEV_ERR_AUDIO                    (-5008)
#define DEV_ERR_TIMEOUT                  (-5009)
#define DEV_ERR_CONNECTION               (-5010)
#define DEV_ERR_ROOM                     (-5011)

typedef struct
{
    char device_id[DEV_ID_MAX];
    char display[DEV_DISPLAY_MAX];
    bool online;
} dev_contact_t;

typedef enum
{
    DEV_MQTT_INCOMING = 0,
    DEV_MQTT_ROOM_CANCEL,
    DEV_MQTT_CALL_REJECT,
    DEV_MQTT_CALLEE_ANSWERED,
    DEV_MQTT_CONTACTS_UPDATE,
} dev_mqtt_event_type_e;

typedef enum
{
    DEV_CALL_TYPE_INVALID = 0,
    DEV_CALL_TYPE_AUDIO,
    DEV_CALL_TYPE_VIDEO,
} dev_call_type_e;

typedef struct
{
    dev_mqtt_event_type_e type;
    dev_call_type_e call_type;
    char room_id[DEV_ROOM_MAX];
    char peer_id[DEV_ID_MAX];
    char peer_name[DEV_NAME_MAX];
} dev_mqtt_event_t;

typedef enum
{
    DEV_SIGNAL_INCOMING_CONNECTED = 0,
    DEV_SIGNAL_OUTGOING_CONNECTED,
    DEV_SIGNAL_CONN_ERROR,
    DEV_SIGNAL_DISCONNECTED,
    DEV_SIGNAL_COMMAND,
} dev_signal_type_e;

typedef struct
{
    dev_signal_type_e type;
    uint32_t generation;
    tirtc_conn_t connection;
    int error;
    uint8_t connect_attempt;
    uint32_t command;
    uint16_t length;
    char payload[DEV_SIGNAL_PAYLOAD_MAX];
} dev_signal_event_t;

typedef struct
{
    uint16_t length;
    uint8_t flags;
    uint32_t generation;
    uint8_t payload[DEV_AUDIO_PAYLOAD_MAX];
} dev_audio_frame_t;

typedef struct
{
    uint32_t generation;
    uint8_t attempt;
    volatile bool pending;
    /* TiRtcConnect is asynchronous.  Keep its string arguments alive until
     * the matching callback even though the current F6D_A archive consumes
     * them synchronously; this also matches the official minimal runtime. */
    char remote_id[DEV_ID_MAX];
    char token[DEV_TOKEN_MAX];
} dev_connect_context_t;

typedef enum
{
    DEV_PLATFORM_NONE = 0,
    DEV_PLATFORM_REJECT,
    DEV_PLATFORM_CANCEL,
    DEV_PLATFORM_HANGUP,
} dev_platform_action_e;

typedef struct
{
    char path[DEV_ROOM_ACTION_PATH_MAX];
    char room_id[DEV_ROOM_MAX];
    char reason[DEV_ROOM_ACTION_REASON_MAX];
} dev_room_action_job_t;

static liot_sem_t s_event_sem;
static liot_queue_t s_mqtt_queue;
static liot_queue_t s_signal_queue;
static liot_queue_t s_audio_queue;
static liot_queue_t s_room_action_queue;

static dev_audio_frame_t s_audio_pool[DEV_AUDIO_QUEUE_DEPTH];
static bool s_audio_pool_used[DEV_AUDIO_QUEUE_DEPTH];

static volatile demo_dev_chat_state_e s_state = DEMO_DEV_CHAT_OFFLINE;
static volatile int s_error;
static volatile bool s_home_allowed;
static volatile bool s_request_call;
/* Covers the worker-owned request before DIALING becomes visible to KEY2. */
static volatile bool s_call_starting;
static volatile uint32_t s_request_call_sequence;
static volatile bool s_request_hangup;
static volatile bool s_request_conn_error;
static volatile int s_request_conn_error_code;
static volatile bool s_request_refresh;
static volatile uint32_t s_request_answer_generation;
static volatile bool s_levels_dirty = true;
static volatile uint8_t s_speaker_level =
    DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static volatile uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;

static dev_contact_t s_contacts[DEV_CONTACT_MAX];
static uint8_t s_contact_count;
static uint8_t s_selected_contact;
static uint32_t s_contacts_revision;
static char s_display_name[DEV_DISPLAY_MAX] = "DEV";

static bool s_caller;
static bool s_expect_incoming;
static char s_room_id[DEV_ROOM_MAX];
static char s_peer_id[DEV_ID_MAX];
static char s_peer_name[DEV_NAME_MAX];
static char s_confirm_room[DEV_ROOM_MAX];
static uint32_t s_generation;
static volatile uint32_t s_room_cancel_generation;
static uint32_t s_incoming_generation;
static uint32_t s_session_sequence;
static uint32_t s_completed_sequence;
static uint32_t s_deadline_ms;
static uint32_t s_error_deadline_ms;
static uint32_t s_contact_retry_at;
static uint32_t s_room_check_at;
static uint32_t s_reject_incoming_until;
/* POST /call/request may return 40202 while an earlier room is still alive.
 * Keep the current DEV owner armed until GET /call/room reconciles that room;
 * otherwise a valid reverse P2P connection can be routed to LIVE or closed. */
static bool s_existing_room_recovery;
static bool s_existing_answered_caller;
static uint32_t s_existing_room_generation;
static uint32_t s_existing_room_retry_at;
static char s_existing_room_hint[DEV_ROOM_MAX];
static bool s_contacts_ready;
static volatile bool s_audio_owned;
static volatile bool s_recovery_inflight;
static volatile uint32_t s_audio_queued;
static volatile uint32_t s_audio_producers;
static volatile uint32_t s_room_action_pending;
static char s_room_action_tombstone[DEV_ROOM_MAX];
static uint32_t s_room_action_tombstone_until;
static uint32_t s_rx_frames;
static uint32_t s_tx_frames;
static uint32_t s_rx_dropped;
static uint32_t s_tx_dropped;
static uint32_t s_prebuffer_start_ms;
static bool s_downlink_reject_logged;
static bool s_playback_error_logged;

static tirtc_conn_t volatile s_conn;
static volatile bool s_tirtc_claimed;
static volatile bool s_tirtc_release_pending;
static volatile uint32_t s_tirtc_generation;
static uint32_t s_tirtc_release_at;
static dev_connect_context_t s_connect_contexts[DEV_CONNECT_CONTEXT_COUNT];
static dev_connect_context_t * volatile s_active_connect_context;
static uint8_t s_connect_attempt;

static char s_http_response[DEV_HTTP_RESPONSE_MAX];
static __attribute__((aligned(16))) int16_t
    s_capture_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint8_t s_uplink_alaw[DEV_UPLINK_SAMPLES_8K];
static __attribute__((aligned(16))) int16_t
    s_play_pcm[DEV_PLAY_PCM_MAX_SAMPLES];
static demo_ai_audio_lease_t s_audio_lease = DEMO_AI_AUDIO_LEASE_INIT;

static void dev_process_mqtt(void);

static void dev_signal(void)
{
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_release(s_event_sem);
    }
}

static bool dev_deadline_pending(uint32_t deadline)
{
    return deadline != 0U &&
           (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

static bool dev_state_has_session(demo_dev_chat_state_e state)
{
    return state == DEMO_DEV_CHAT_RINGING ||
           state == DEMO_DEV_CHAT_DIALING ||
           state == DEMO_DEV_CHAT_CONNECTING ||
           state == DEMO_DEV_CHAT_WAIT_CONFIRM ||
           state == DEMO_DEV_CHAT_IN_CALL ||
           state == DEMO_DEV_CHAT_STOPPING;
}

/* Caller holds the RTOS critical section.  This is the final single-flight
 * guard used by both UI admission and TiRtcConnect submission. */
static bool dev_connect_lifecycle_idle_locked(void)
{
    uint32_t i;

    if (s_active_connect_context != NULL || s_conn != NULL ||
        s_audio_owned || s_audio_producers != 0U ||
        s_audio_queued != 0U)
    {
        return false;
    }
    for (i = 0U; i < DEV_CONNECT_CONTEXT_COUNT; ++i)
    {
        if (s_connect_contexts[i].pending)
        {
            return false;
        }
    }
    return true;
}

static bool dev_connect_lifecycle_idle(void)
{
    bool idle;

    liot_rtos_enter_critical();
    idle = dev_connect_lifecycle_idle_locked();
    liot_rtos_exit_critical();
    return idle;
}

/* Each DEV business session owns exactly one generation in the process-wide
 * TiRTC adapter.  The adapter, not this feature, owns SDK handle destruction;
 * this prevents callbacks racing a second raw TiRtcDisconnect(). */
static int dev_tirtc_claim(void)
{
    uint32_t generation = 0U;
    int ret;

    liot_rtos_enter_critical();
    if (s_tirtc_claimed || s_tirtc_release_pending)
    {
        liot_rtos_exit_critical();
        return TIRTC_E_BUSY;
    }
    liot_rtos_exit_critical();

    ret = demo_tirtc_session_claim(DEMO_TIRTC_OWNER_DEV_CHAT,
                                   &generation);
    if (ret != 0)
    {
        return ret;
    }
    liot_rtos_enter_critical();
    s_tirtc_claimed = true;
    s_tirtc_release_pending = false;
    s_tirtc_generation = generation;
    s_tirtc_release_at = 0U;
    liot_rtos_exit_critical();
    return 0;
}

static int dev_tirtc_expect_current(void)
{
    uint32_t generation;
    bool valid;
    int ret;

    liot_rtos_enter_critical();
    valid = s_tirtc_claimed && !s_tirtc_release_pending && s_caller &&
            s_expect_incoming && s_tirtc_generation != 0U &&
            (s_state == DEMO_DEV_CHAT_DIALING ||
             s_state == DEMO_DEV_CHAT_WAIT_CONFIRM);
    generation = s_tirtc_generation;
    liot_rtos_exit_critical();
    if (!valid)
    {
        return TIRTC_E_INVALID_HANDLE;
    }
    ret = demo_tirtc_expect_incoming(DEMO_TIRTC_OWNER_DEV_CHAT,
                                     generation);
    if (ret != 0)
    {
        liot_rtos_enter_critical();
        if (s_tirtc_generation == generation)
        {
            s_expect_incoming = false;
        }
        liot_rtos_exit_critical();
    }
    return ret;
}

static void dev_tirtc_begin_release(bool keep_late_incoming_guard)
{
    uint32_t generation;
    bool claimed;

    liot_rtos_enter_critical();
    claimed = s_tirtc_claimed;
    generation = s_tirtc_generation;
    if (claimed)
    {
        s_tirtc_release_pending = true;
        s_tirtc_release_at = keep_late_incoming_guard ?
            liot_rtos_get_running_time() + DEV_LATE_INCOMING_GUARD_MS : 0U;
    }
    liot_rtos_exit_critical();
    if (!claimed || generation == 0U)
    {
        return;
    }

    (void)demo_tirtc_cancel_expected_incoming(
        DEMO_TIRTC_OWNER_DEV_CHAT, generation);
    /* This is intentionally idempotent.  It cancels a pending connect or
     * detaches an active handle; the runtime task performs SDK disconnect. */
    (void)demo_tirtc_disconnect(DEMO_TIRTC_OWNER_DEV_CHAT, generation);
    dev_signal();
}

static bool dev_tirtc_try_release(void)
{
    uint32_t generation;
    uint32_t release_at;
    bool pending;
    int ret;

    liot_rtos_enter_critical();
    pending = s_tirtc_claimed && s_tirtc_release_pending;
    generation = s_tirtc_generation;
    release_at = s_tirtc_release_at;
    liot_rtos_exit_critical();
    if (!pending || generation == 0U || dev_deadline_pending(release_at))
    {
        return !pending;
    }

    ret = demo_tirtc_session_release(DEMO_TIRTC_OWNER_DEV_CHAT,
                                     generation);
    if (ret == 0 || ret == TIRTC_E_INVALID_HANDLE)
    {
        liot_rtos_enter_critical();
        if (s_tirtc_generation == generation)
        {
            s_tirtc_claimed = false;
            s_tirtc_release_pending = false;
            s_tirtc_generation = 0U;
            s_tirtc_release_at = 0U;
        }
        liot_rtos_exit_critical();
        return true;
    }
    if (ret != TIRTC_E_BUSY)
    {
        liot_trace("[DEV] TiRTC session release deferred ret=%d\r\n", ret);
    }
    return false;
}

static void dev_set_state(demo_dev_chat_state_e state, int error)
{
    if (s_state != state || s_error != error)
    {
        s_state = state;
        s_error = error;
        liot_trace("[DEV] state=%d error=%d free_heap=%u\r\n",
                   (int)state, error,
                   (unsigned int)liot_xPortGetFreeHeapSize());
    }
}

static void dev_secure_zero(void *data, size_t size)
{
    volatile uint8_t *cursor = (volatile uint8_t *)data;

    while (cursor != NULL && size-- > 0U)
    {
        *cursor++ = 0U;
    }
}

static int dev_json_copy(const cJSON *object, const char *key,
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
    if (length == 0U || length >= capacity)
    {
        return required ? -1 : 0;
    }
    memcpy(output, item->valuestring, length + 1U);
    return 0;
}

static dev_call_type_e dev_parse_call_type(const char *value)
{
    if (value != NULL && strcmp(value, "audio") == 0)
    {
        return DEV_CALL_TYPE_AUDIO;
    }
    if (value != NULL && strcmp(value, "video") == 0)
    {
        return DEV_CALL_TYPE_VIDEO;
    }
    return DEV_CALL_TYPE_INVALID;
}

static const char *dev_call_type_name(dev_call_type_e type)
{
    return type == DEV_CALL_TYPE_VIDEO ? "video" :
           (type == DEV_CALL_TYPE_AUDIO ? "audio" : "invalid");
}

static bool dev_safe_device_id(const char *value)
{
    const unsigned char *cursor;
    size_t length;

    if (value == NULL || value[0] == '\0')
    {
        return false;
    }
    length = strlen(value);
    if (length >= DEV_ID_MAX)
    {
        return false;
    }
    for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
    {
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_' &&
            *cursor != '.' && *cursor != ':')
        {
            return false;
        }
    }
    return true;
}

static void dev_make_id_tail(const char *id, char output[7])
{
    char reverse[6];
    size_t length = id != NULL ? strlen(id) : 0U;
    size_t used = 0U;
    size_t i = length;

    memset(output, 0, 7U);
    while (i > 0U && used < sizeof(reverse))
    {
        unsigned char ch = (unsigned char)id[--i];
        if (ch >= 'a' && ch <= 'z')
        {
            reverse[used++] = (char)toupper(ch);
        }
        else if ((ch >= 'A' && ch <= 'Z') ||
                 (ch >= '0' && ch <= '9'))
        {
            reverse[used++] = (char)ch;
        }
    }
    for (i = 0U; i < used; ++i)
    {
        output[i] = reverse[used - i - 1U];
    }
}

static void dev_make_display(const char *preferred, const char *id,
                             uint8_t index,
                             char output[DEV_DISPLAY_MAX])
{
    char tail[7];
    size_t used = 0U;
    size_t i;

    memset(output, 0, DEV_DISPLAY_MAX);
    if (preferred != NULL)
    {
        for (i = 0U; preferred[i] != '\0' &&
                    used + 1U < DEV_DISPLAY_MAX; ++i)
        {
            unsigned char ch = (unsigned char)preferred[i];
            if (ch >= 'a' && ch <= 'z')
            {
                output[used++] = (char)toupper(ch);
            }
            else if ((ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9') || ch == ' ' ||
                     ch == '-' || ch == '+' || ch == ':')
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
        dev_make_id_tail(id, tail);
        if (tail[0] != '\0')
        {
            (void)snprintf(output, DEV_DISPLAY_MAX, "DEV-%s", tail);
        }
        else
        {
            (void)snprintf(output, DEV_DISPLAY_MAX, "DEVICE%u",
                           (unsigned int)(index + 1U));
        }
    }
}

static void dev_set_display(const char *name, const char *id)
{
    char display[DEV_DISPLAY_MAX];

    dev_make_display(name, id, 0U, display);
    liot_rtos_enter_critical();
    memcpy(s_display_name, display, sizeof(s_display_name));
    liot_rtos_exit_critical();
}

static bool dev_transport_ready(void)
{
    demo_binding_snapshot_t binding;

    demo_binding_get_snapshot(&binding);
    return binding.state == DEMO_BIND_BOUND &&
           demo_formal_mqtt_is_online() && demo_tirtc_is_ready();
}

static int dev_business_code(const char *response)
{
    cJSON *root = cJSON_Parse(response);
    const cJSON *code = root != NULL ?
                            cJSON_GetObjectItemCaseSensitive(root, "code") :
                            NULL;
    int value = cJSON_IsNumber(code) ? code->valueint : DEV_ERR_SERVICE;

    cJSON_Delete(root);
    return value;
}

static int dev_http(demo_binding_http_method_e method,
                    const char *path, const char *body)
{
    int status = 0;
    int ret;

    memset(s_http_response, 0, sizeof(s_http_response));
    ret = demo_binding_service_request_timeout(
        DEMO_BIND_SERVICE_CALL, method, path, body,
        s_http_response, sizeof(s_http_response), &status,
        DEV_HTTP_TIMEOUT_MS);
    if (ret != 0 || status != 200)
    {
        liot_trace("[DEV-HTTP] path=%s failed ret=%d status=%d\r\n",
                   path, ret, status);
        return ret != 0 ? ret : DEV_ERR_SERVICE;
    }
    return 0;
}

static int dev_room_action(const char *path, const char *room_id,
                           const char *reason)
{
    dev_room_action_job_t job;
    int path_length;
    int room_length;
    int reason_length = 0;

    if (path == NULL || room_id == NULL || room_id[0] == '\0' ||
        s_room_action_queue == NULL)
    {
        return -1;
    }
    memset(&job, 0, sizeof(job));
    path_length = snprintf(job.path, sizeof(job.path), "%s", path);
    room_length = snprintf(job.room_id, sizeof(job.room_id), "%s", room_id);
    if (reason != NULL)
    {
        reason_length = snprintf(job.reason, sizeof(job.reason), "%s",
                                 reason);
    }
    if (path_length <= 0 || path_length >= (int)sizeof(job.path) ||
        room_length <= 0 || room_length >= (int)sizeof(job.room_id) ||
        reason_length >= (int)sizeof(job.reason))
    {
        liot_trace("[DEV-HTTP] invalid room action path=%s\r\n", path);
        return -1;
    }
    liot_rtos_enter_critical();
    ++s_room_action_pending;
    memcpy(s_room_action_tombstone, job.room_id,
           sizeof(s_room_action_tombstone));
    s_room_action_tombstone_until = liot_rtos_get_running_time() +
                                    DEV_ROOM_TOMBSTONE_MS;
    liot_rtos_exit_critical();
    if (liot_rtos_queue_release(s_room_action_queue, sizeof(job),
                                (uint8 *)&job,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        liot_rtos_enter_critical();
        if (s_room_action_pending > 0U)
        {
            --s_room_action_pending;
        }
        liot_rtos_exit_critical();
        liot_trace("[DEV-HTTP] room action queue full path=%s\r\n", path);
        return -1;
    }
    return 0;
}

static int dev_execute_room_action(const dev_room_action_job_t *job)
{
    cJSON *root = NULL;
    char *body = NULL;
    char response[DEV_ROOM_ACTION_RESPONSE_MAX];
    int status = 0;
    int ret = DEV_ERR_SERVICE;

    if (job == NULL || job->path[0] == '\0' || job->room_id[0] == '\0')
    {
        return -1;
    }
    memset(response, 0, sizeof(response));
    root = cJSON_CreateObject();
    if (root != NULL &&
        cJSON_AddStringToObject(root, "room_id", job->room_id) != NULL &&
        (job->reason[0] == '\0' ||
         cJSON_AddStringToObject(root, "reason", job->reason) != NULL))
    {
        body = cJSON_PrintUnformatted(root);
    }
    cJSON_Delete(root);
    if (body != NULL)
    {
        ret = demo_binding_service_request_timeout(
            DEMO_BIND_SERVICE_CALL, DEMO_BIND_HTTP_POST, job->path, body,
            response, sizeof(response), &status, DEV_HTTP_TIMEOUT_MS);
        cJSON_free(body);
        if (ret == 0 && status != 200)
        {
            ret = DEV_ERR_SERVICE;
        }
        if (ret == 0)
        {
            int business = dev_business_code(response);

            if (business != 0 && business != 200)
            {
                ret = business;
            }
        }
    }
    dev_secure_zero(response, sizeof(response));
    return ret;
}

static void dev_room_action_task(void *argv)
{
    dev_room_action_job_t job;

    (void)argv;
    while (1)
    {
        if (liot_rtos_queue_wait(s_room_action_queue, (uint8 *)&job,
                                 sizeof(job),
                                 LIOT_WAIT_FOREVER) == LIOT_OSI_SUCCESS)
        {
            uint32_t attempt;
            int ret = -1;

            for (attempt = 0U; attempt < DEV_ROOM_ACTION_RETRIES; ++attempt)
            {
                ret = dev_execute_room_action(&job);
                if (ret == 0)
                {
                    break;
                }
                if (attempt + 1U < DEV_ROOM_ACTION_RETRIES)
                {
                    liot_rtos_task_sleep_ms(500U);
                }
            }

            liot_trace("[DEV-HTTP] room action path=%s ret=%d attempts=%u\r\n",
                       job.path, ret,
                       (unsigned int)(attempt < DEV_ROOM_ACTION_RETRIES ?
                                          attempt + 1U :
                                          DEV_ROOM_ACTION_RETRIES));
            liot_rtos_enter_critical();
            if (s_room_action_pending > 0U)
            {
                --s_room_action_pending;
            }
            liot_rtos_exit_critical();
            dev_secure_zero(&job, sizeof(job));
            dev_signal();
        }
    }
}

static int dev_fetch_contacts(void)
{
    dev_contact_t contacts[DEV_CONTACT_MAX];
    cJSON *root = NULL;
    const cJSON *code;
    const cJSON *data;
    const cJSON *list;
    const cJSON *item;
    uint8_t count = 0U;
    int ret;

    memset(contacts, 0, sizeof(contacts));
    dev_set_state(DEMO_DEV_CHAT_SYNCING, 0);
    ret = dev_http(DEMO_BIND_HTTP_GET,
                   "/v1/call/device/contacts", NULL);
    if (ret != 0)
    {
        return DEV_ERR_CONTACTS;
    }
    root = cJSON_Parse(s_http_response);
    code = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "code") :
                          NULL;
    data = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "data") :
                          NULL;
    list = cJSON_IsObject(data) ?
               cJSON_GetObjectItemCaseSensitive(data, "contacts") : NULL;
    if (!cJSON_IsNumber(code) ||
        (code->valueint != 0 && code->valueint != 200) ||
        !cJSON_IsArray(list))
    {
        cJSON_Delete(root);
        return DEV_ERR_CONTACTS;
    }
    cJSON_ArrayForEach(item, list)
    {
        const cJSON *type;
        const cJSON *online;
        char preferred[DEV_NAME_MAX];
        dev_contact_t *contact;

        if (count >= DEV_CONTACT_MAX || !cJSON_IsObject(item))
        {
            break;
        }
        type = cJSON_GetObjectItemCaseSensitive(item, "type");
        if (!cJSON_IsString(type) || type->valuestring == NULL ||
            strcmp(type->valuestring, "device") != 0)
        {
            continue;
        }
        contact = &contacts[count];
        if (dev_json_copy(item, "device_id", contact->device_id,
                          sizeof(contact->device_id), true) != 0 ||
            !dev_safe_device_id(contact->device_id))
        {
            continue;
        }
        memset(preferred, 0, sizeof(preferred));
        (void)dev_json_copy(item, "remark", preferred,
                            sizeof(preferred), false);
        if (preferred[0] == '\0')
        {
            (void)dev_json_copy(item, "device_name", preferred,
                                sizeof(preferred), false);
        }
        if (preferred[0] == '\0')
        {
            (void)dev_json_copy(item, "name", preferred,
                                sizeof(preferred), false);
        }
        dev_make_display(preferred, contact->device_id, count,
                         contact->display);
        online = cJSON_GetObjectItemCaseSensitive(item, "online");
        contact->online = cJSON_IsTrue(online);
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
        memcpy(s_display_name, "NO DEVICE", sizeof("NO DEVICE"));
    }
    liot_rtos_exit_critical();
    s_contacts_ready = true;
    liot_trace("[DEV] contacts ready count=%u revision=%u\r\n",
               (unsigned int)count,
               (unsigned int)s_contacts_revision);
    return 0;
}

static int dev_audio_slot_acquire(tirtc_conn_t connection,
                                  uint32_t *generation)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    if (connection == (tirtc_conn_t)s_conn &&
        s_state == DEMO_DEV_CHAT_IN_CALL)
    {
        for (i = 0U; i < DEV_AUDIO_QUEUE_DEPTH; ++i)
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

static void dev_audio_producer_done(void)
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
        dev_signal();
    }
}

static bool dev_wait_audio_producers(void)
{
    uint32_t deadline = liot_rtos_get_running_time() +
                        DEV_AUDIO_PRODUCER_TIMEOUT_MS;
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
    } while (dev_deadline_pending(deadline));

    liot_trace("[DEV] audio callback quiesce timeout producers=%u\r\n",
               (unsigned int)producers);
    return false;
}

static void dev_audio_slot_release(uint8_t slot)
{
    if (slot >= DEV_AUDIO_QUEUE_DEPTH)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_audio_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static void dev_audio_queued_decrement(void)
{
    liot_rtos_enter_critical();
    if (s_audio_queued > 0U)
    {
        --s_audio_queued;
    }
    liot_rtos_exit_critical();
}

static void dev_drain_audio(void)
{
    uint8_t slot;
    bool producers_idle;

    while (s_audio_queue != NULL &&
           liot_rtos_queue_wait(s_audio_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        dev_audio_queued_decrement();
        dev_audio_slot_release(slot);
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

static bool dev_queue_signal(const dev_signal_event_t *event)
{
    if (s_signal_queue == NULL || event == NULL ||
        liot_rtos_queue_release(s_signal_queue, sizeof(*event),
                                (uint8 *)event,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        ++s_rx_dropped;
        return false;
    }
    dev_signal();
    return true;
}

static void dev_request_connection_error(int error)
{
    liot_rtos_enter_critical();
    if (dev_state_has_session(s_state) && !s_request_conn_error)
    {
        s_request_conn_error = true;
        s_request_conn_error_code = error != 0 ? error : DEV_ERR_CONNECTION;
    }
    liot_rtos_exit_critical();
    dev_signal();
}

static void dev_formal_mqtt_handler(
    demo_formal_mqtt_message_kind_e kind,
    const char *type,
    const char *channel,
    const cJSON *payload,
    void *user)
{
    dev_mqtt_event_t event;
    char call_type[12];
    bool valid = true;

    (void)kind;
    (void)user;
    if (type == NULL || channel == NULL ||
        strcmp(channel, "device") != 0)
    {
        return;
    }
    memset(&event, 0, sizeof(event));
    if (strcmp(type, "call_incoming") == 0)
    {
        event.type = DEV_MQTT_INCOMING;
        memset(call_type, 0, sizeof(call_type));
        valid = payload != NULL &&
                dev_json_copy(payload, "room_id", event.room_id,
                              sizeof(event.room_id), true) == 0 &&
                dev_json_copy(payload, "caller_id", event.peer_id,
                              sizeof(event.peer_id), true) == 0 &&
                dev_json_copy(payload, "caller_name", event.peer_name,
                              sizeof(event.peer_name), false) == 0 &&
                dev_json_copy(payload, "call_type", call_type,
                              sizeof(call_type), true) == 0;
        event.call_type = valid ? dev_parse_call_type(call_type) :
                                  DEV_CALL_TYPE_INVALID;
        valid = valid && event.call_type != DEV_CALL_TYPE_INVALID &&
                dev_safe_device_id(event.peer_id);
        if (valid)
        {
            liot_trace("[DEV-MQTT] incoming requested=%s effective=audio\r\n",
                       dev_call_type_name(event.call_type));
        }
    }
    else if (strcmp(type, "room_cancel") == 0)
    {
        event.type = DEV_MQTT_ROOM_CANCEL;
        valid = payload != NULL &&
                dev_json_copy(payload, "room_id", event.room_id,
                              sizeof(event.room_id), true) == 0;
    }
    else if (strcmp(type, "call_reject") == 0)
    {
        event.type = DEV_MQTT_CALL_REJECT;
        valid = payload != NULL &&
                dev_json_copy(payload, "room_id", event.room_id,
                              sizeof(event.room_id), true) == 0;
    }
    else if (strcmp(type, "callee_answered") == 0)
    {
        event.type = DEV_MQTT_CALLEE_ANSWERED;
        valid = payload != NULL &&
                dev_json_copy(payload, "room_id", event.room_id,
                              sizeof(event.room_id), true) == 0;
    }
    else if (strcmp(type, "callers_update") == 0)
    {
        event.type = DEV_MQTT_CONTACTS_UPDATE;
    }
    else
    {
        return;
    }
    if (valid && event.type == DEV_MQTT_ROOM_CANCEL)
    {
        /* Invalidate a synchronous /device/info response immediately.  The
         * worker may still be blocked in HTTP and unable to drain MQTT yet. */
        liot_rtos_enter_critical();
        if (dev_state_has_session(s_state) && s_room_id[0] != '\0' &&
            strcmp(event.room_id, s_room_id) == 0)
        {
            s_room_cancel_generation = s_generation;
        }
        liot_rtos_exit_critical();
    }
    if (!valid || s_mqtt_queue == NULL ||
        liot_rtos_queue_release(s_mqtt_queue, sizeof(event),
                                (uint8 *)&event,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        ++s_rx_dropped;
        liot_trace("[DEV-MQTT] invalid/overflow type=%s\r\n", type);
        return;
    }
    liot_trace("[DEV-MQTT] queued type=%s room=%u\r\n", type,
               event.room_id[0] != '\0' ? 1U : 0U);
    dev_signal();
}

static int dev_on_conn_accepted(tirtc_conn_t hconn)
{
    dev_signal_event_t event;
    bool expected = false;

    if (hconn == NULL)
    {
        return -1;
    }
    memset(&event, 0, sizeof(event));
    liot_rtos_enter_critical();
    if (s_expect_incoming && s_caller && s_conn == NULL &&
        (s_state == DEMO_DEV_CHAT_DIALING ||
         s_state == DEMO_DEV_CHAT_WAIT_CONFIRM))
    {
        s_conn = hconn;
        s_expect_incoming = false;
        event.type = DEV_SIGNAL_INCOMING_CONNECTED;
        event.generation = s_generation;
        event.connection = hconn;
        expected = true;
    }
    liot_rtos_exit_critical();
    if (expected)
    {
        if (dev_queue_signal(&event))
        {
            return 0;
        }
        liot_rtos_enter_critical();
        if (s_conn == hconn)
        {
            s_conn = NULL;
        }
        liot_rtos_exit_critical();
        return -1;
    }
    /* Managed sessions reject non-expected handles in the runtime adapter.
     * Returning failure here is therefore sufficient and never transfers a
     * raw SDK handle to this feature for destruction. */
    return -1;
}

static void dev_on_conn_error(tirtc_conn_t hconn, int error)
{
    dev_signal_event_t event;
    dev_connect_context_t *context;
    uint32_t generation = 0U;
    bool current = false;
    bool pending = false;

    if (hconn == NULL)
    {
        return;
    }
    liot_rtos_enter_critical();
    if (hconn == (tirtc_conn_t)s_conn)
    {
        current = true;
        generation = s_generation;
    }
    else
    {
        context = (dev_connect_context_t *)s_active_connect_context;
        if (!s_caller && s_conn == NULL &&
            s_state == DEMO_DEV_CHAT_CONNECTING && context != NULL &&
            context->pending && context->generation == s_generation)
        {
            pending = true;
            generation = context->generation;
        }
    }
    liot_rtos_exit_critical();
    if (!current && !pending)
    {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.type = DEV_SIGNAL_CONN_ERROR;
    event.generation = generation;
    event.connection = hconn;
    event.error = error != 0 ? error : DEV_ERR_CONNECTION;
    if (!dev_queue_signal(&event))
    {
        dev_request_connection_error(event.error);
    }
}

static void dev_on_disconnected(tirtc_conn_t hconn)
{
    dev_signal_event_t event;
    bool current = false;

    memset(&event, 0, sizeof(event));
    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == (tirtc_conn_t)s_conn)
    {
        /* The SDK callback is the handle-release boundary.  Drop ownership
         * here, before MQTT/transport teardown can observe the old handle. */
        s_conn = NULL;
        event.generation = s_generation;
        current = true;
    }
    liot_rtos_exit_critical();
    if (!current)
    {
        /* Local teardown clears s_conn before asking the adapter to close it.
         * Wake the owner so a pending generation can now be released. */
        dev_signal();
        return;
    }
    event.type = DEV_SIGNAL_DISCONNECTED;
    event.connection = hconn;
    if (!dev_queue_signal(&event))
    {
        liot_rtos_enter_critical();
        s_request_hangup = true;
        liot_rtos_exit_critical();
        dev_signal();
    }
}

static void dev_on_audio(tirtc_conn_t hconn,
                         const TIRTCFRAMEINFO *frame,
                         void *data)
{
    dev_audio_frame_t *message;
    uint32_t generation = 0U;
    uint8_t slot;
    int acquired;

    /* Runtime callbacks are broadcast to feature listeners.  Ignore another
     * feature's frame before counting it as a malformed DEV packet. */
    liot_rtos_enter_critical();
    acquired = hconn != NULL && hconn == (tirtc_conn_t)s_conn &&
               s_state == DEMO_DEV_CHAT_IN_CALL ? 0 : -1;
    liot_rtos_exit_critical();
    if (acquired < 0)
    {
        return;
    }
    if (frame == NULL || data == NULL ||
        frame->stream_id != DEV_STREAM_ID ||
        frame->media != TIRTC_AUDIO_ALAW ||
        frame->length == 0U ||
        frame->length > DEV_AUDIO_PAYLOAD_MAX ||
        frame->flags != TIRTC_AUDIOSAMPLE_8K16B1C)
    {
        ++s_rx_dropped;
        if (frame != NULL && !s_downlink_reject_logged)
        {
            s_downlink_reject_logged = true;
            liot_trace("[DEV] reject downlink stream=%u media=%u flags=%u "
                       "bytes=%u; want stream=10 ALAW 8k <=%u\r\n",
                       (unsigned int)frame->stream_id,
                       (unsigned int)frame->media,
                       (unsigned int)frame->flags,
                       (unsigned int)frame->length,
                       (unsigned int)DEV_AUDIO_PAYLOAD_MAX);
        }
        return;
    }
    acquired = dev_audio_slot_acquire(hconn, &generation);
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
        dev_audio_queued_decrement();
        dev_audio_slot_release(slot);
        dev_audio_producer_done();
        ++s_rx_dropped;
        return;
    }
    dev_audio_producer_done();
    dev_signal();
}

static bool dev_command_is(uint32_t command, uint32_t expected)
{
    return command == expected;
}

static void dev_on_command(tirtc_conn_t hconn, uint32_t command,
                           const void *data, uint32_t length)
{
    dev_signal_event_t event;

    if (hconn == NULL || hconn != (tirtc_conn_t)s_conn ||
        (!dev_command_is(command, DEV_CMD_CONFIRM) &&
         !dev_command_is(command, DEV_CMD_HANGUP)))
    {
        return;
    }
    if (length >= DEV_SIGNAL_PAYLOAD_MAX ||
        (length > 0U && data == NULL))
    {
        ++s_rx_dropped;
        return;
    }
    memset(&event, 0, sizeof(event));
    event.type = DEV_SIGNAL_COMMAND;
    event.generation = s_generation;
    event.connection = hconn;
    event.command = command;
    event.length = (uint16_t)length;
    if (length > 0U)
    {
        memcpy(event.payload, data, length);
    }
    (void)dev_queue_signal(&event);
}

static int dev_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    if (hconn != NULL && hconn == (tirtc_conn_t)s_conn &&
        stream_id == DEV_STREAM_ID &&
        (s_state == DEMO_DEV_CHAT_DIALING ||
         s_state == DEMO_DEV_CHAT_CONNECTING ||
         s_state == DEMO_DEV_CHAT_WAIT_CONFIRM ||
         s_state == DEMO_DEV_CHAT_IN_CALL))
    {
        return 0;
    }
    return -1;
}

static void dev_on_unsubscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    (void)hconn;
    (void)stream_id;
}

static const demo_tirtc_listener_t s_tirtc_listener = {
    .on_conn_accepted = dev_on_conn_accepted,
    .on_conn_error = dev_on_conn_error,
    .on_disconnected = dev_on_disconnected,
    .on_audio = dev_on_audio,
    .on_command = dev_on_command,
    .on_subscribe_audio = dev_on_subscribe_audio,
    .on_unsubscribe_audio = dev_on_unsubscribe_audio,
};

static dev_connect_context_t *dev_connect_context_acquire(
    uint8_t attempt, const char *remote_id, const char *token)
{
    uint32_t i;
    dev_connect_context_t *context = NULL;

    liot_rtos_enter_critical();
    for (i = 0U; i < DEV_CONNECT_CONTEXT_COUNT; ++i)
    {
        if (!s_connect_contexts[i].pending)
        {
            s_connect_contexts[i].generation = s_generation;
            s_connect_contexts[i].attempt = attempt;
            s_connect_contexts[i].pending = true;
            context = &s_connect_contexts[i];
            break;
        }
    }
    liot_rtos_exit_critical();
    if (context != NULL)
    {
        dev_secure_zero(context->remote_id, sizeof(context->remote_id));
        dev_secure_zero(context->token, sizeof(context->token));
        if (remote_id != NULL)
        {
            (void)snprintf(context->remote_id,
                           sizeof(context->remote_id), "%s", remote_id);
        }
        if (token != NULL)
        {
            (void)snprintf(context->token, sizeof(context->token), "%s",
                           token);
        }
    }
    return context;
}

static void dev_connect_callback(int error, tirtc_conn_t hconn,
                                 void *user_data)
{
    dev_connect_context_t *context =
        (dev_connect_context_t *)user_data;
    dev_signal_event_t event;
    bool current = false;
    bool success = false;
    uint32_t generation;
    uint8_t attempt;

    if (context == NULL)
    {
        dev_request_connection_error(error != 0 ? error : DEV_ERR_CONNECT);
        return;
    }
    memset(&event, 0, sizeof(event));
    generation = context->generation;
    attempt = context->attempt;
    liot_rtos_enter_critical();
    current = context == s_active_connect_context &&
              generation == s_generation &&
              s_state == DEMO_DEV_CHAT_CONNECTING && s_conn == NULL;
    success = current && error == 0 && hconn != NULL;
    if (success)
    {
        s_conn = hconn;
    }
    liot_rtos_exit_critical();

    /* The managed adapter owns failed/stale handles and has copied all input
     * strings.  This callback only retires the feature's correlation slot. */
    dev_secure_zero(context->remote_id, sizeof(context->remote_id));
    dev_secure_zero(context->token, sizeof(context->token));
    liot_rtos_enter_critical();
    context->pending = false;
    if (s_active_connect_context == context)
    {
        s_active_connect_context = NULL;
    }
    liot_rtos_exit_critical();
    if (!current)
    {
        dev_signal();
        return;
    }
    event.type = DEV_SIGNAL_OUTGOING_CONNECTED;
    event.generation = generation;
    event.connection = success ? hconn : NULL;
    event.error = error;
    event.connect_attempt = attempt;
    if (!dev_queue_signal(&event))
    {
        if (success)
        {
            liot_rtos_enter_critical();
            if (s_conn == hconn)
            {
                s_conn = NULL;
            }
            liot_rtos_exit_critical();
            (void)demo_tirtc_disconnect(DEMO_TIRTC_OWNER_DEV_CHAT,
                                        s_tirtc_generation);
        }
        dev_request_connection_error(error != 0 ? error : DEV_ERR_CONNECT);
        liot_trace("[DEV] P2P callback queue full; managed cleanup armed\r\n");
    }
}

static int dev_submit_callee_connect(const char *token)
{
    dev_connect_context_t *context;
    const char *stable_token;
    uint8_t attempt;
    bool lifecycle_idle;
    int ret;

    if (s_state != DEMO_DEV_CHAT_CONNECTING || s_caller ||
        s_peer_id[0] == '\0' ||
        s_connect_attempt >= DEV_CONNECT_MAX_ATTEMPTS ||
        !s_tirtc_claimed || s_tirtc_release_pending ||
        s_tirtc_generation == 0U)
    {
        return DEV_ERR_TIMEOUT;
    }
    liot_rtos_enter_critical();
    lifecycle_idle = dev_connect_lifecycle_idle_locked();
    liot_rtos_exit_critical();
    if (!lifecycle_idle)
    {
        liot_trace("[DEV] TiRtcConnect blocked: prior lifecycle still active\r\n");
        return TIRTC_E_BUSY;
    }
    attempt = (uint8_t)(s_connect_attempt + 1U);
    context = dev_connect_context_acquire(attempt, s_peer_id, token);
    if (context == NULL)
    {
        return DEV_ERR_CONNECT;
    }
    stable_token = token != NULL ? context->token : NULL;
    liot_rtos_enter_critical();
    s_connect_attempt = attempt;
    s_active_connect_context = context;
    liot_rtos_exit_critical();

    liot_trace("[DEV] callee P2P attempt=%u/%u submit begin "
               "token_present=%u token_len=%u\r\n",
               (unsigned int)attempt,
               (unsigned int)DEV_CONNECT_MAX_ATTEMPTS,
               stable_token != NULL ? 1U : 0U,
               stable_token != NULL ? (unsigned int)strlen(stable_token) : 0U);
    ret = demo_tirtc_connect(DEMO_TIRTC_OWNER_DEV_CHAT,
                             s_tirtc_generation,
                             context->remote_id, stable_token,
                             dev_connect_callback, context);
    if (ret != 0)
    {
        liot_rtos_enter_critical();
        if (s_active_connect_context == context)
        {
            s_active_connect_context = NULL;
        }
        liot_rtos_exit_critical();
        dev_secure_zero(context->remote_id, sizeof(context->remote_id));
        dev_secure_zero(context->token, sizeof(context->token));
        liot_rtos_enter_critical();
        context->pending = false;
        liot_rtos_exit_critical();
        liot_trace("[DEV] TiRtcConnect submit failed attempt=%u ret=%d (%s)\r\n",
                   (unsigned int)attempt, ret, TiRtcGetErrorStr(ret));
        return ret;
    }

    /* Start the application watchdog after connect submission returns.  It
     * performs service discovery synchronously before its async P2P phase. */
    s_deadline_ms = liot_rtos_get_running_time() +
                    DEV_CONNECT_TIMEOUT_MS;
    liot_trace("[DEV] callee P2P connect submitted attempt=%u/%u\r\n",
               (unsigned int)attempt,
               (unsigned int)DEV_CONNECT_MAX_ATTEMPTS);
    return 0;
}

static void dev_restore_contact_display(void)
{
    liot_rtos_enter_critical();
    if (s_contact_count > 0U && s_selected_contact < s_contact_count)
    {
        memcpy(s_display_name, s_contacts[s_selected_contact].display,
               sizeof(s_display_name));
    }
    else
    {
        memcpy(s_display_name, "NO DEVICE", sizeof("NO DEVICE"));
    }
    liot_rtos_exit_critical();
}

static void dev_mark_session_complete(void)
{
    liot_rtos_enter_critical();
    s_completed_sequence = s_session_sequence;
    liot_rtos_exit_critical();
}

static void dev_flush_signal_queue(void)
{
    dev_signal_event_t event;

    while (s_signal_queue != NULL &&
           liot_rtos_queue_wait(s_signal_queue, (uint8 *)&event,
                                sizeof(event),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        /* The session owner already disconnected its current handle before
         * this drain.  Late connect callbacks are handled at callback time;
         * never enqueue this already-closed handle for a second disconnect. */
        (void)event;
    }
}

static void dev_finish_session(int result,
                               dev_platform_action_e action,
                               bool notify_peer)
{
    tirtc_conn_t connection;
    char room_id[DEV_ROOM_MAX];
    bool caller;
    bool tirtc_claimed;
    uint32_t tirtc_generation;
    bool connect_inflight = false;
    uint32_t i;
    int action_ret = 0;
    int audio_release_ret = 0;

    if (!dev_state_has_session(s_state))
    {
        return;
    }

    /* TiRtcConnect has no public cancel operation.  If the user or peer ends
     * the room while it is pending, advance the generation below and let the
     * late callback clear its context (and disconnect a late success handle).
     * demo_dev_chat_is_idle() keeps all other audio features gated until that
     * callback has completed. */
    liot_rtos_enter_critical();
    if (s_conn == NULL && s_active_connect_context != NULL)
    {
        connect_inflight = true;
    }
    for (i = 0U; !connect_inflight && i < DEV_CONNECT_CONTEXT_COUNT; ++i)
    {
        if (s_connect_contexts[i].pending)
        {
            connect_inflight = true;
        }
    }
    liot_rtos_exit_critical();
    if (connect_inflight)
    {
        liot_trace("[DEV] session ended with P2P callback pending; "
                   "late cleanup armed\r\n");
    }

    dev_set_state(DEMO_DEV_CHAT_STOPPING, 0);
    memset(room_id, 0, sizeof(room_id));
    liot_rtos_enter_critical();
    connection = (tirtc_conn_t)s_conn;
    s_conn = NULL;
    s_expect_incoming = false;
    caller = s_caller;
    tirtc_claimed = s_tirtc_claimed;
    tirtc_generation = s_tirtc_generation;
    memcpy(room_id, s_room_id, sizeof(room_id));
    s_request_call = false;
    s_request_call_sequence = 0U;
    s_request_hangup = false;
    s_request_conn_error = false;
    s_request_conn_error_code = 0;
    s_room_cancel_generation = 0U;
    s_request_answer_generation = 0U;
    s_existing_room_recovery = false;
    s_existing_answered_caller = false;
    s_existing_room_generation = 0U;
    s_existing_room_retry_at = 0U;
    s_connect_attempt = 0U;
    liot_rtos_exit_critical();

    /* Submit best-effort cloud cleanup before local teardown, on its own
     * worker, so HTTP latency can never stop the audio/session owner.  The
     * caller chooses CANCEL for user/timeout cancellation and HANGUP for P2P
     * or protocol failures, matching the official state machine. */
    if (room_id[0] != '\0')
    {
        if (action == DEV_PLATFORM_REJECT)
        {
            action_ret = dev_room_action("/v1/call/reject", room_id,
                                         "decline");
        }
        else if (action == DEV_PLATFORM_CANCEL)
        {
            action_ret = dev_room_action("/v1/call/cancel", room_id,
                                         NULL);
        }
        else if (action == DEV_PLATFORM_HANGUP)
        {
            action_ret = dev_room_action("/v1/call/hangup", room_id,
                                         result != 0 ?
                                             "p2p_error" : "hangup");
        }
    }
    if (notify_peer && connection != NULL && tirtc_claimed &&
        tirtc_generation != 0U)
    {
        static const char hangup[] = "{\"reason\":0}";
        (void)demo_tirtc_send_command(DEMO_TIRTC_OWNER_DEV_CHAT,
                                      tirtc_generation,
                                      DEV_CMD_HANGUP,
                                      hangup, sizeof(hangup) - 1U);
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
            liot_trace("[DEV] audio lease release failed ret=%d\r\n",
                       audio_release_ret);
            demo_tirtc_require_restart(DEV_ERR_AUDIO);
        }
    }
    (void)dev_wait_audio_producers();
    dev_drain_audio();
    dev_tirtc_begin_release(caller);
    (void)dev_tirtc_try_release();
    if (caller && s_state == DEMO_DEV_CHAT_STOPPING)
    {
        s_reject_incoming_until = liot_rtos_get_running_time() +
                                  DEV_LATE_INCOMING_GUARD_MS;
    }
    ++s_generation;
    if (s_generation == 0U)
    {
        ++s_generation;
    }
    dev_secure_zero(s_room_id, sizeof(s_room_id));
    dev_secure_zero(s_peer_id, sizeof(s_peer_id));
    dev_secure_zero(s_peer_name, sizeof(s_peer_name));
    dev_secure_zero(s_confirm_room, sizeof(s_confirm_room));
    dev_secure_zero(s_existing_room_hint, sizeof(s_existing_room_hint));
    s_caller = false;
    s_deadline_ms = 0U;
    dev_flush_signal_queue();
    dev_mark_session_complete();
    dev_restore_contact_display();
    if (result != 0)
    {
        dev_set_state(DEMO_DEV_CHAT_ERROR, result);
        s_error_deadline_ms = liot_rtos_get_running_time() +
                              DEV_ERROR_VISIBLE_MS;
    }
    else if (dev_transport_ready())
    {
        dev_set_state(DEMO_DEV_CHAT_READY, 0);
    }
    else
    {
        dev_set_state(DEMO_DEV_CHAT_OFFLINE, 0);
    }
    liot_trace("[DEV] session=%u closed result=%d action_ret=%d "
               "rx_drop=%u tx_drop=%u\r\n",
               (unsigned int)s_completed_sequence, result, action_ret,
               (unsigned int)s_rx_dropped,
               (unsigned int)s_tx_dropped);
}

static int dev_start_audio(void)
{
    uint8_t speaker_level;
    uint8_t mic_level;
    int ret;

    if (s_conn == NULL || !s_tirtc_claimed ||
        s_tirtc_release_pending || s_tirtc_generation == 0U)
    {
        return DEV_ERR_CONNECTION;
    }
    liot_rtos_enter_critical();
    speaker_level = s_speaker_level;
    mic_level = s_mic_level;
    s_levels_dirty = false;
    liot_rtos_exit_critical();
    ret = demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_DEVICE,
                                &s_audio_lease);
    if (ret != 0)
    {
        return DEV_ERR_AUDIO;
    }
    liot_rtos_enter_critical();
    s_audio_owned = true;
    liot_rtos_exit_critical();
    (void)demo_ai_audio_set_levels(&s_audio_lease,
                                   speaker_level, mic_level);
    ret = demo_ai_audio_init(&s_audio_lease);
    if (ret != 0)
    {
        return DEV_ERR_AUDIO;
    }
    ret = demo_ai_audio_apply_levels(&s_audio_lease);
    if (ret != 0)
    {
        return DEV_ERR_AUDIO;
    }
    ret = demo_tirtc_subscribe_audio(DEMO_TIRTC_OWNER_DEV_CHAT,
                                     s_tirtc_generation,
                                     DEV_STREAM_ID);
    liot_trace("[DEV] subscribe downlink stream=%u ret=%d\r\n",
               (unsigned int)DEV_STREAM_ID, ret);
    if (ret < 0)
    {
        return ret;
    }
    s_deadline_ms = 0U;
    s_rx_frames = 0U;
    s_tx_frames = 0U;
    s_prebuffer_start_ms = 0U;
    s_downlink_reject_logged = false;
    s_playback_error_logged = false;
    s_existing_answered_caller = false;
    dev_set_state(DEMO_DEV_CHAT_IN_CALL, 0);
    liot_trace("[DEV] media started bidirectional G711A/8k/mono/20ms "
               "stream=10 (no AEC)\r\n");
    return 0;
}

static int dev_try_activate_caller(void)
{
    if (!s_caller || s_conn == NULL || s_room_id[0] == '\0' ||
        s_confirm_room[0] == '\0')
    {
        return 0;
    }
    if (strcmp(s_room_id, s_confirm_room) != 0)
    {
        liot_trace("[DEV] room confirmation mismatch\r\n");
        return DEV_ERR_ROOM;
    }
    return dev_start_audio();
}

static int dev_play_one(void)
{
    dev_audio_frame_t *message;
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
    dev_audio_queued_decrement();
    if (slot >= DEV_AUDIO_QUEUE_DEPTH)
    {
        ++s_rx_dropped;
        return 0;
    }
    message = &s_audio_pool[slot];
    if (message->generation != s_generation)
    {
        dev_audio_slot_release(slot);
        return 1;
    }
    input_samples = message->length;
    input_flags = message->flags;
    if (input_flags == TIRTC_AUDIOSAMPLE_8K16B1C)
    {
        output_samples = input_samples * 2U;
        if (output_samples > DEV_PLAY_PCM_MAX_SAMPLES)
        {
            dev_audio_slot_release(slot);
            ++s_rx_dropped;
            return 1;
        }
        for (i = 0U; i < input_samples; ++i)
        {
            int16_t sample =
                demo_g711_alaw_decode_sample(message->payload[i]);
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
            dev_audio_slot_release(slot);
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
    dev_audio_slot_release(slot);
    if (ret != 0)
    {
        ++s_rx_dropped;
        if (!s_playback_error_logged)
        {
            s_playback_error_logged = true;
            liot_trace("[DEV] playback failed ret=%d samples=%u peak=%u\r\n",
                       ret, (unsigned int)output_samples,
                       (unsigned int)peak);
        }
        return 0;
    }
    ++s_rx_frames;
    if (s_rx_frames == 1U)
    {
        liot_trace("[DEV] first downlink ALAW bytes=%u flags=%u "
                   "pcm_samples=%u peak=%u\r\n",
                   (unsigned int)input_samples,
                   (unsigned int)input_flags,
                   (unsigned int)output_samples,
                   (unsigned int)peak);
    }
    return 1;
}

static void dev_service_downlink(void)
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
        if (s_prebuffer_start_ms == 0U)
        {
            s_prebuffer_start_ms = now;
        }
        if (queued < DEV_PREBUFFER_PACKETS &&
            (int32_t)(now - s_prebuffer_start_ms) <
                (int32_t)DEV_PREBUFFER_TIMEOUT_MS)
        {
            return;
        }
    }
    s_prebuffer_start_ms = 0U;
    for (i = 0U; i < DEV_AUDIO_QUEUE_DEPTH; ++i)
    {
        if (dev_play_one() <= 0)
        {
            break;
        }
    }
}

static int dev_send_uplink(void)
{
    TIRTCFRAMEINFO frame;
    uint32_t i;
    int ret;

    ret = demo_ai_audio_record_20ms(&s_audio_lease, s_capture_pcm);
    if (ret != 0)
    {
        return DEV_ERR_AUDIO;
    }
    for (i = 0U; i < DEV_UPLINK_SAMPLES_8K; ++i)
    {
        int32_t mixed = (int32_t)s_capture_pcm[i * 2U] +
                        (int32_t)s_capture_pcm[i * 2U + 1U];
        s_uplink_alaw[i] =
            demo_g711_alaw_encode_sample((int16_t)(mixed / 2));
    }
    memset(&frame, 0, sizeof(frame));
    frame.stream_id = DEV_STREAM_ID;
    frame.media = TIRTC_AUDIO_ALAW;
    frame.flags = TIRTC_AUDIOSAMPLE_8K16B1C;
    frame.ts = liot_rtos_get_running_time();
    frame.length = DEV_UPLINK_SAMPLES_8K;
    ret = demo_tirtc_send_audio(DEMO_TIRTC_OWNER_DEV_CHAT,
                                s_tirtc_generation,
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
        liot_trace("[DEV] first uplink ALAW bytes=%u\r\n",
                   (unsigned int)DEV_UPLINK_SAMPLES_8K);
    }
    return 0;
}

static void dev_begin_session(bool caller, const char *peer_id,
                              const char *peer_name)
{
    ++s_generation;
    if (s_generation == 0U)
    {
        ++s_generation;
    }
    if (caller && s_request_call_sequence != 0U)
    {
        s_session_sequence = s_request_call_sequence;
        s_request_call_sequence = 0U;
    }
    else
    {
        ++s_session_sequence;
        if (s_session_sequence == 0U)
        {
            ++s_session_sequence;
        }
    }
    dev_secure_zero(s_room_id, sizeof(s_room_id));
    dev_secure_zero(s_peer_id, sizeof(s_peer_id));
    dev_secure_zero(s_peer_name, sizeof(s_peer_name));
    dev_secure_zero(s_confirm_room, sizeof(s_confirm_room));
    s_caller = caller;
    s_expect_incoming = caller;
    s_conn = NULL;
    s_request_conn_error = false;
    s_request_conn_error_code = 0;
    s_room_cancel_generation = 0U;
    s_active_connect_context = NULL;
    s_connect_attempt = 0U;
    s_existing_room_recovery = false;
    s_existing_answered_caller = false;
    s_existing_room_generation = 0U;
    s_existing_room_retry_at = 0U;
    dev_secure_zero(s_existing_room_hint, sizeof(s_existing_room_hint));
    s_deadline_ms = 0U;
    if (peer_id != NULL)
    {
        (void)snprintf(s_peer_id, sizeof(s_peer_id), "%s", peer_id);
    }
    if (peer_name != NULL)
    {
        (void)snprintf(s_peer_name, sizeof(s_peer_name), "%s", peer_name);
    }
    dev_set_display(peer_name, peer_id);
}

static bool dev_other_features_idle(void)
{
    bool idle = true;

#ifdef HWDEMO_AI_CHAT_EN
    idle = idle && demo_ai_chat_is_idle();
#endif
#ifdef HWDEMO_WECHAT_EN
    idle = idle && demo_wechat_is_idle();
#endif
#ifdef HWDEMO_LIVE_TALK_EN
    idle = idle && demo_live_talk_wait_idle(0U);
#endif
    return idle;
}

/* A room GET is deliberately performed without holding a global feature lock.
 * Therefore both the preflight and the response path must validate the whole
 * local owner state.  The only difference is whether this very GET is the
 * expected recovery request in flight. */
static bool dev_recovery_owner_safe(bool recovery_inflight)
{
    uint32_t i;
    bool safe;

    liot_rtos_enter_critical();
    safe = s_state == DEMO_DEV_CHAT_READY && !s_request_call &&
           !s_existing_room_recovery &&
           !s_request_hangup && s_request_answer_generation == 0U &&
           s_conn == NULL && !s_audio_owned && !s_tirtc_claimed &&
           s_audio_producers == 0U && s_audio_queued == 0U &&
           s_room_action_pending == 0U &&
           s_recovery_inflight == recovery_inflight &&
           !dev_deadline_pending(s_reject_incoming_until);
    for (i = 0U; i < DEV_CONNECT_CONTEXT_COUNT && safe; ++i)
    {
        safe = !s_connect_contexts[i].pending;
    }
    liot_rtos_exit_critical();
    return safe && dev_other_features_idle();
}

static bool dev_recovery_safe(void)
{
    return dev_recovery_owner_safe(false);
}

/* Recover the call-server room exactly as the official minimal runtime does.
 * Old answered P2P sessions are not reusable; clear them.  An active caller
 * waits for the callee's reverse P2P connection, while an active callee rings
 * again and obtains a fresh one-shot token only after the user answers. */
static int dev_recover_room(void)
{
    cJSON *root = NULL;
    const cJSON *code;
    const cJSON *data;
    char room[DEV_ROOM_MAX];
    char status[16];
    char role[12];
    char caller[DEV_ID_MAX];
    char call_type[12];
    bool tombstoned;
    int business;
    int ret;

    memset(room, 0, sizeof(room));
    memset(status, 0, sizeof(status));
    memset(role, 0, sizeof(role));
    memset(caller, 0, sizeof(caller));
    memset(call_type, 0, sizeof(call_type));
    liot_rtos_enter_critical();
    if (s_recovery_inflight)
    {
        liot_rtos_exit_critical();
        return 0;
    }
    s_recovery_inflight = true;
    liot_rtos_exit_critical();

    ret = dev_http(DEMO_BIND_HTTP_GET, "/v1/call/room", NULL);
    if (ret != 0)
    {
        goto done;
    }
    root = cJSON_Parse(s_http_response);
    code = root != NULL ?
               cJSON_GetObjectItemCaseSensitive(root, "code") : NULL;
    data = root != NULL ?
               cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    business = cJSON_IsNumber(code) ? code->valueint : DEV_ERR_ROOM;
    if (business != 0 && business != 200)
    {
        ret = business;
        goto done;
    }
    if (data == NULL || cJSON_IsNull(data))
    {
        ret = 0;
        goto done;
    }
    /* The HTTP response can arrive after another feature acquired the shared
     * TiRTC/audio owner.  Revalidate before claiming a recovered room, like
     * the official runtime's response-time owner check. */
    if (!dev_recovery_owner_safe(true))
    {
        liot_trace("[DEV] recovery response stale; owner changed\r\n");
        ret = 0;
        goto done;
    }

    if (!cJSON_IsObject(data) ||
        dev_json_copy(data, "room_id", room, sizeof(room), true) != 0 ||
        dev_json_copy(data, "status", status, sizeof(status), true) != 0 ||
        dev_json_copy(data, "role", role, sizeof(role), true) != 0 ||
        dev_json_copy(data, "caller", caller, sizeof(caller), true) != 0 ||
        dev_json_copy(data, "call_type", call_type,
                      sizeof(call_type), true) != 0 ||
        (strcmp(status, "active") != 0 &&
         strcmp(status, "answered") != 0) ||
        (strcmp(role, "caller") != 0 && strcmp(role, "callee") != 0) ||
        !dev_safe_device_id(caller) ||
        (strcmp(call_type, "audio") != 0 &&
         strcmp(call_type, "video") != 0))
    {
        ret = DEV_ERR_ROOM;
        goto done;
    }

    /* A reject/cancel/hangup is asynchronous.  The call server may briefly
     * return the old active room while that cleanup request is settling.  Do
     * not resurrect a room that this process has just closed. */
    liot_rtos_enter_critical();
    tombstoned = s_room_action_tombstone[0] != '\0' &&
                 strcmp(room, s_room_action_tombstone) == 0 &&
                 dev_deadline_pending(s_room_action_tombstone_until);
    liot_rtos_exit_critical();
    if (tombstoned)
    {
        liot_trace("[DEV] recovery ignored recently closed room\r\n");
        ret = 0;
        goto done;
    }

    if (strcmp(status, "answered") == 0)
    {
        ret = dev_room_action("/v1/call/hangup", room, "p2p_error");
        liot_trace("[DEV] stale answered room queued for cleanup\r\n");
        goto done;
    }
    if (!s_home_allowed)
    {
        ret = strcmp(role, "caller") == 0 ?
                  dev_room_action("/v1/call/cancel", room, NULL) :
                  dev_room_action("/v1/call/reject", room, "busy");
        liot_trace("[DEV] recovered room cleared outside HOME/DEV UI\r\n");
        goto done;
    }
    if (strcmp(call_type, "video") == 0)
    {
        liot_trace("[DEV] recovered video room downgraded to audio-only\r\n");
    }
    ret = dev_tirtc_claim();
    if (ret != 0)
    {
        liot_trace("[DEV] recovery deferred: TiRTC owner busy ret=%d\r\n",
                   ret);
        ret = 0;
        goto done;
    }
    if (strcmp(role, "caller") == 0)
    {
        dev_begin_session(true, NULL, "CALLING");
        memcpy(s_room_id, room, sizeof(s_room_id));
        s_deadline_ms = liot_rtos_get_running_time() +
                        DEV_OUTGOING_TIMEOUT_MS;
        dev_set_state(DEMO_DEV_CHAT_DIALING, 0);
        ret = dev_tirtc_expect_current();
        if (ret != 0)
        {
            dev_finish_session(ret, DEV_PLATFORM_NONE, false);
            goto done;
        }
        liot_trace("[DEV] recovered active caller room; waiting P2P\r\n");
    }
    else
    {
        dev_begin_session(false, caller, caller);
        memcpy(s_room_id, room, sizeof(s_room_id));
        ++s_incoming_generation;
        if (s_incoming_generation == 0U)
        {
            ++s_incoming_generation;
        }
        s_deadline_ms = liot_rtos_get_running_time() +
                        DEV_INCOMING_TIMEOUT_MS;
        dev_set_state(DEMO_DEV_CHAT_RINGING, 0);
        liot_trace("[DEV] recovered active callee room generation=%u\r\n",
                   (unsigned int)s_incoming_generation);
    }
    ret = 0;

done:
    cJSON_Delete(root);
    liot_rtos_enter_critical();
    s_recovery_inflight = false;
    liot_rtos_exit_critical();
    if (ret != 0)
    {
        liot_trace("[DEV] room recovery check ret=%d\r\n", ret);
    }
    return ret;
}

static bool dev_existing_room_current(void)
{
    bool current;

    liot_rtos_enter_critical();
    current = s_existing_room_recovery &&
              s_existing_room_generation == s_generation && s_caller &&
              (s_state == DEMO_DEV_CHAT_DIALING ||
               s_state == DEMO_DEV_CHAT_WAIT_CONFIRM);
    liot_rtos_exit_critical();
    return current;
}

static void dev_existing_room_retry(int error)
{
    if (dev_existing_room_current())
    {
        s_existing_room_retry_at = liot_rtos_get_running_time() +
                                   DEV_EXISTING_ROOM_RETRY_MS;
        liot_trace("[DEV] existing room recovery retry ret=%d\r\n", error);
    }
}

/* Reconcile a 40202 response without releasing the DEV owner.  Unlike cold
 * boot recovery, the answering device may already be connecting while the
 * HTTP request is in flight, so generation, s_conn and queued 0x2000 must be
 * preserved until the authoritative room role/status is known. */
static int dev_recover_existing_room(void)
{
    cJSON *root = NULL;
    const cJSON *code;
    const cJSON *data;
    char room[DEV_ROOM_MAX];
    char status[16];
    char role[12];
    char caller[DEV_ID_MAX];
    char call_type[12];
    tirtc_conn_t unexpected = NULL;
    bool tombstoned;
    bool leave_requested;
    bool answered;
    bool caller_role;
    int business;
    int ret;

    if (!dev_existing_room_current())
    {
        return 0;
    }
    memset(room, 0, sizeof(room));
    memset(status, 0, sizeof(status));
    memset(role, 0, sizeof(role));
    memset(caller, 0, sizeof(caller));
    memset(call_type, 0, sizeof(call_type));

    ret = dev_http(DEMO_BIND_HTTP_GET, "/v1/call/room", NULL);
    if (ret != 0)
    {
        dev_existing_room_retry(ret);
        return ret;
    }
    root = cJSON_Parse(s_http_response);
    code = root != NULL ?
               cJSON_GetObjectItemCaseSensitive(root, "code") : NULL;
    data = root != NULL ?
               cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    business = cJSON_IsNumber(code) ? code->valueint : DEV_ERR_ROOM;
    if (business != 0 && business != 200)
    {
        cJSON_Delete(root);
        dev_existing_room_retry(business);
        return business;
    }
    if (data == NULL || cJSON_IsNull(data))
    {
        cJSON_Delete(root);
        if (dev_existing_room_current())
        {
            liot_trace("[DEV] existing room disappeared during recovery\r\n");
            dev_finish_session(0, DEV_PLATFORM_NONE, false);
        }
        return 0;
    }
    if (!cJSON_IsObject(data) ||
        dev_json_copy(data, "room_id", room, sizeof(room), true) != 0 ||
        dev_json_copy(data, "status", status, sizeof(status), true) != 0 ||
        dev_json_copy(data, "role", role, sizeof(role), true) != 0 ||
        dev_json_copy(data, "caller", caller, sizeof(caller), true) != 0 ||
        dev_json_copy(data, "call_type", call_type,
                      sizeof(call_type), true) != 0 ||
        (strcmp(status, "active") != 0 &&
         strcmp(status, "answered") != 0) ||
        (strcmp(role, "caller") != 0 && strcmp(role, "callee") != 0) ||
        !dev_safe_device_id(caller) ||
        (strcmp(call_type, "audio") != 0 &&
         strcmp(call_type, "video") != 0))
    {
        cJSON_Delete(root);
        dev_existing_room_retry(DEV_ERR_ROOM);
        return DEV_ERR_ROOM;
    }
    cJSON_Delete(root);

    if (s_existing_room_hint[0] != '\0' &&
        strcmp(s_existing_room_hint, room) != 0)
    {
        liot_trace("[DEV] 40202 room mismatch; refusing ambiguous room\r\n");
        dev_finish_session(DEV_ERR_ROOM, DEV_PLATFORM_NONE, false);
        return DEV_ERR_ROOM;
    }
    liot_rtos_enter_critical();
    tombstoned = s_room_action_tombstone[0] != '\0' &&
                 strcmp(room, s_room_action_tombstone) == 0 &&
                 dev_deadline_pending(s_room_action_tombstone_until);
    liot_rtos_exit_critical();
    if (tombstoned)
    {
        liot_trace("[DEV] 40202 recovery waiting for prior room cleanup\r\n");
        dev_existing_room_retry(0);
        return 0;
    }
    if (!dev_existing_room_current() || !dev_other_features_idle() ||
        !s_tirtc_claimed)
    {
        dev_existing_room_retry(0);
        return 0;
    }

    answered = strcmp(status, "answered") == 0;
    caller_role = strcmp(role, "caller") == 0;
    leave_requested = s_request_hangup || !s_home_allowed;

    /* An audio-only board can still join a video room using stream 10; it
     * simply does not subscribe or publish stream 11.  An answered callee
     * cannot rebuild its one-shot outbound P2P connection, however. */
    if ((answered && !caller_role) || leave_requested)
    {
        dev_platform_action_e action = answered ? DEV_PLATFORM_HANGUP :
                                       (caller_role ? DEV_PLATFORM_CANCEL :
                                                      DEV_PLATFORM_REJECT);

        memcpy(s_room_id, room, sizeof(s_room_id));
        s_caller = caller_role;
        s_expect_incoming = false;
        s_existing_room_recovery = false;
        s_existing_room_generation = 0U;
        s_existing_room_retry_at = 0U;
        dev_secure_zero(s_existing_room_hint,
                        sizeof(s_existing_room_hint));
        liot_trace("[DEV] recovered room closed status=%s role=%s\r\n",
                   status, role);
        dev_finish_session(answered ? DEV_ERR_CONNECTION : 0,
                           action, s_conn != NULL);
        return answered ? DEV_ERR_CONNECTION : 0;
    }

    if (caller_role)
    {
        if (strcmp(call_type, "video") == 0)
        {
            liot_trace("[DEV] recovered video caller as audio-only\r\n");
        }
        memcpy(s_room_id, room, sizeof(s_room_id));
        dev_secure_zero(s_peer_id, sizeof(s_peer_id));
        dev_secure_zero(s_peer_name, sizeof(s_peer_name));
        dev_set_display("CALLING", NULL);
        s_expect_incoming = s_conn == NULL;
        s_existing_room_recovery = false;
        s_existing_answered_caller = answered;
        s_existing_room_generation = 0U;
        s_existing_room_retry_at = 0U;
        dev_secure_zero(s_existing_room_hint,
                        sizeof(s_existing_room_hint));
        if (s_conn != NULL)
        {
            dev_set_state(DEMO_DEV_CHAT_WAIT_CONFIRM, 0);
            s_deadline_ms = liot_rtos_get_running_time() +
                            DEV_CONFIRM_TIMEOUT_MS;
        }
        else
        {
            dev_set_state(DEMO_DEV_CHAT_DIALING, 0);
            s_deadline_ms = liot_rtos_get_running_time() +
                            (answered ? DEV_CONFIRM_TIMEOUT_MS :
                                        DEV_OUTGOING_TIMEOUT_MS);
        }
        liot_trace("[DEV] recovered %s caller room; waiting P2P+confirm\r\n",
                   status);
        ret = dev_try_activate_caller();
        if (ret < 0)
        {
            dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
        }
        return ret;
    }

    /* The existing room belongs to this device as callee.  Convert the
     * provisional caller session in place so UI sequence ownership is not
     * completed and re-created.  Any connection captured before the role was
     * known cannot belong to an unaccepted callee and is closed separately. */
    liot_rtos_enter_critical();
    unexpected = (tirtc_conn_t)s_conn;
    s_conn = NULL;
    s_expect_incoming = false;
    ++s_generation;
    if (s_generation == 0U)
    {
        ++s_generation;
    }
    s_active_connect_context = NULL;
    s_connect_attempt = 0U;
    s_caller = false;
    memcpy(s_room_id, room, sizeof(s_room_id));
    (void)snprintf(s_peer_id, sizeof(s_peer_id), "%s", caller);
    (void)snprintf(s_peer_name, sizeof(s_peer_name), "%s", caller);
    dev_secure_zero(s_confirm_room, sizeof(s_confirm_room));
    s_existing_room_recovery = false;
    s_existing_answered_caller = false;
    s_existing_room_generation = 0U;
    s_existing_room_retry_at = 0U;
    dev_secure_zero(s_existing_room_hint, sizeof(s_existing_room_hint));
    ++s_incoming_generation;
    if (s_incoming_generation == 0U)
    {
        ++s_incoming_generation;
    }
    liot_rtos_exit_critical();
    dev_flush_signal_queue();
    dev_set_display(caller, caller);
    if (strcmp(call_type, "video") == 0)
    {
        liot_trace("[DEV] recovered video callee as audio-only\r\n");
    }
    s_deadline_ms = liot_rtos_get_running_time() + DEV_INCOMING_TIMEOUT_MS;
    dev_set_state(DEMO_DEV_CHAT_RINGING, 0);
    if (unexpected != NULL)
    {
        ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_DEV_CHAT,
                                    s_tirtc_generation);
        if ((ret != 0 && ret != TIRTC_E_INVALID_HANDLE) ||
            !demo_tirtc_wait_connections_idle(
                DEV_CONNECT_TIMEOUT_MS))
        {
            liot_trace("[DEV] role-change P2P teardown failed ret=%d\r\n",
                       ret);
            dev_finish_session(ret != 0 ? ret : DEV_ERR_TIMEOUT,
                               DEV_PLATFORM_HANGUP, false);
            return ret != 0 ? ret : DEV_ERR_TIMEOUT;
        }
    }
    liot_trace("[DEV] recovered active callee room generation=%u\r\n",
               (unsigned int)s_incoming_generation);
    return 0;
}

static int dev_start_outgoing_call(void)
{
    dev_contact_t contact;
    cJSON *root = NULL;
    cJSON *targets = NULL;
    cJSON *response_root = NULL;
    const cJSON *code;
    const cJSON *data;
    char *body = NULL;
    char room[DEV_ROOM_MAX];
    int business;
    int ret = DEV_ERR_CALL;

    memset(&contact, 0, sizeof(contact));
    memset(room, 0, sizeof(room));
    liot_rtos_enter_critical();
    if (s_request_hangup)
    {
        liot_rtos_exit_critical();
        return 0;
    }
    if (s_contact_count == 0U || s_selected_contact >= s_contact_count)
    {
        liot_rtos_exit_critical();
        return DEV_ERR_NO_CONTACT;
    }
    contact = s_contacts[s_selected_contact];
    liot_rtos_exit_critical();

    ret = dev_tirtc_claim();
    if (ret != 0)
    {
        liot_trace("[DEV] outgoing TiRTC owner unavailable ret=%d\r\n", ret);
        liot_rtos_enter_critical();
        s_request_call_sequence = 0U;
        liot_rtos_exit_critical();
        dev_mark_session_complete();
        dev_restore_contact_display();
        dev_set_state(DEMO_DEV_CHAT_ERROR, ret);
        s_error_deadline_ms = liot_rtos_get_running_time() +
                              DEV_ERROR_VISIBLE_MS;
        return ret;
    }
    dev_begin_session(true, contact.device_id, contact.display);
    dev_set_state(DEMO_DEV_CHAT_DIALING, 0);
    /* KEY2 may have arrived while the worker was still preparing in READY.
     * Settle that local cancellation before submitting a cloud call. */
    if (s_request_hangup)
    {
        dev_finish_session(0, DEV_PLATFORM_NONE, false);
        return 0;
    }
    ret = dev_tirtc_expect_current();
    if (ret != 0)
    {
        dev_finish_session(ret, DEV_PLATFORM_NONE, false);
        return ret;
    }
    s_deadline_ms = liot_rtos_get_running_time() +
                    DEV_HTTP_TIMEOUT_MS;

    root = cJSON_CreateObject();
    targets = cJSON_CreateArray();
    if (root != NULL && targets != NULL &&
        cJSON_AddItemToArray(targets,
                            cJSON_CreateString(contact.device_id)) &&
        cJSON_AddStringToObject(root, "call_type", "audio") != NULL)
    {
        /* cJSON takes ownership only when the object insertion succeeds.
         * Keep the local pointer on OOM so the common cleanup below can free
         * the array instead of leaking it and sending an incomplete request. */
        if (cJSON_AddItemToObject(root, "targets", targets))
        {
            targets = NULL;
            body = cJSON_PrintUnformatted(root);
        }
    }
    cJSON_Delete(targets);
    cJSON_Delete(root);
    if (body == NULL)
    {
        dev_finish_session(DEV_ERR_CALL, DEV_PLATFORM_NONE, false);
        return DEV_ERR_CALL;
    }
    ret = dev_http(DEMO_BIND_HTTP_POST, "/v1/call/request", body);
    cJSON_free(body);
    if (ret != 0)
    {
        dev_finish_session(DEV_ERR_CALL, DEV_PLATFORM_NONE, false);
        return DEV_ERR_CALL;
    }
    response_root = cJSON_Parse(s_http_response);
    code = response_root != NULL ?
               cJSON_GetObjectItemCaseSensitive(response_root, "code") :
               NULL;
    business = cJSON_IsNumber(code) ? code->valueint : DEV_ERR_CALL;
    data = response_root != NULL ?
               cJSON_GetObjectItemCaseSensitive(response_root, "data") :
               NULL;
    if (business == 40202)
    {
        (void)dev_json_copy(data, "room_id", room, sizeof(room), false);
        liot_rtos_enter_critical();
        s_existing_room_recovery = true;
        s_existing_answered_caller = false;
        s_existing_room_generation = s_generation;
        s_existing_room_retry_at = liot_rtos_get_running_time();
        memcpy(s_existing_room_hint, room, sizeof(s_existing_room_hint));
        s_deadline_ms = liot_rtos_get_running_time() +
                        DEV_OUTGOING_TIMEOUT_MS;
        liot_rtos_exit_critical();
        cJSON_Delete(response_root);
        /* Preserve this session's generation, accepted reverse P2P and queued
         * room confirmation until the existing server room is reconciled. */
        liot_trace("[DEV] existing room requires in-place recovery\r\n");
        (void)dev_recover_existing_room();
        return 0;
    }
    if ((business != 0 && business != 200) || !cJSON_IsObject(data) ||
        dev_json_copy(data, "room_id", room, sizeof(room), true) != 0)
    {
        cJSON_Delete(response_root);
        liot_trace("[DEV] call request rejected code=%d\r\n", business);
        dev_finish_session(business != 0 ? business : DEV_ERR_CALL,
                           DEV_PLATFORM_NONE, false);
        return DEV_ERR_CALL;
    }
    cJSON_Delete(response_root);
    memcpy(s_room_id, room, sizeof(s_room_id));
    s_deadline_ms = liot_rtos_get_running_time() +
                    DEV_OUTGOING_TIMEOUT_MS;
    liot_trace("[DEV] outgoing room ready; waiting callee P2P\r\n");
    if (s_conn != NULL)
    {
        dev_set_state(DEMO_DEV_CHAT_WAIT_CONFIRM, 0);
        s_deadline_ms = liot_rtos_get_running_time() +
                        DEV_CONFIRM_TIMEOUT_MS;
        ret = dev_try_activate_caller();
        if (ret < 0)
        {
            dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
            return ret;
        }
    }
    if (s_request_hangup)
    {
        dev_finish_session(0, DEV_PLATFORM_CANCEL, false);
    }
    return 0;
}

static bool dev_accept_context_current(uint32_t generation,
                                       const char *room_id,
                                       const char *peer_id)
{
    bool current;

    liot_rtos_enter_critical();
    current = generation == s_generation &&
              s_room_cancel_generation != generation &&
              s_state == DEMO_DEV_CHAT_CONNECTING && !s_caller &&
              !s_request_hangup && !s_request_conn_error &&
              s_tirtc_claimed && !s_tirtc_release_pending &&
              s_tirtc_generation != 0U &&
              s_conn == NULL && s_active_connect_context == NULL &&
              room_id != NULL && peer_id != NULL &&
              strcmp(room_id, s_room_id) == 0 &&
              strcmp(peer_id, s_peer_id) == 0;
    liot_rtos_exit_critical();
    return current;
}

static int dev_accept_incoming(void)
{
    cJSON *root = NULL;
    cJSON *response_root = NULL;
    const cJSON *code;
    const cJSON *data;
    const cJSON *token_item;
    char *body = NULL;
    char token[DEV_TOKEN_MAX];
    char peer_id[DEV_ID_MAX];
    char expected_peer_id[DEV_ID_MAX];
    char expected_room_id[DEV_ROOM_MAX];
    uint32_t generation;
    int business;
    int ret = DEV_ERR_CONNECT;
    size_t token_length;

    memset(token, 0, sizeof(token));
    memset(peer_id, 0, sizeof(peer_id));
    memset(expected_peer_id, 0, sizeof(expected_peer_id));
    memset(expected_room_id, 0, sizeof(expected_room_id));
    liot_rtos_enter_critical();
    generation = s_generation;
    memcpy(expected_peer_id, s_peer_id, sizeof(expected_peer_id));
    memcpy(expected_room_id, s_room_id, sizeof(expected_room_id));
    liot_rtos_exit_critical();
    dev_set_state(DEMO_DEV_CHAT_CONNECTING, 0);
    s_deadline_ms = liot_rtos_get_running_time() +
                    DEV_HTTP_TIMEOUT_MS;
    root = cJSON_CreateObject();
    if (root != NULL &&
        cJSON_AddStringToObject(root, "device_id",
                               expected_peer_id) != NULL &&
        cJSON_AddStringToObject(root, "room_id",
                               expected_room_id) != NULL &&
        cJSON_AddStringToObject(root, "purpose", "call") != NULL)
    {
        body = cJSON_PrintUnformatted(root);
    }
    cJSON_Delete(root);
    if (body == NULL)
    {
        dev_finish_session(DEV_ERR_CONNECT, DEV_PLATFORM_HANGUP, false);
        return DEV_ERR_CONNECT;
    }
    ret = dev_http(DEMO_BIND_HTTP_POST, "/v1/call/device/info", body);
    cJSON_free(body);
    /* MQTT callbacks can enqueue room_cancel while the synchronous HTTP
     * request owns this worker.  Apply those events before trusting the
     * response, then verify this is still the exact answered generation. */
    dev_process_mqtt();
    if (!dev_accept_context_current(generation, expected_room_id,
                                    expected_peer_id))
    {
        liot_trace("[DEV] stale device-info response ignored generation=%u\r\n",
                   (unsigned int)generation);
        return 0;
    }
    if (ret != 0)
    {
        dev_finish_session(DEV_ERR_CONNECT, DEV_PLATFORM_HANGUP, false);
        return DEV_ERR_CONNECT;
    }
    response_root = cJSON_Parse(s_http_response);
    code = response_root != NULL ?
               cJSON_GetObjectItemCaseSensitive(response_root, "code") :
               NULL;
    business = cJSON_IsNumber(code) ? code->valueint : DEV_ERR_CONNECT;
    data = response_root != NULL ?
               cJSON_GetObjectItemCaseSensitive(response_root, "data") :
               NULL;
    token_item = cJSON_IsObject(data) ?
                     cJSON_GetObjectItemCaseSensitive(data, "token") : NULL;
    token_length = cJSON_IsString(token_item) &&
                           token_item->valuestring != NULL ?
                       strlen(token_item->valuestring) : 0U;
    liot_trace("[DEV] device info code=%d token_len=%u max=%u\r\n",
               business, (unsigned int)token_length,
               (unsigned int)(DEV_TOKEN_MAX - 1U));
    if (token_length >= DEV_TOKEN_MAX)
    {
        liot_trace("[DEV] device info token exceeds F6D_A SDK limit\r\n");
    }
    if (business != 0 && business != 200)
    {
        cJSON_Delete(response_root);
        dev_secure_zero(token, sizeof(token));
        /* In a multi-target ring, 40210 means another device answered first.
         * This callee is not authorized to hang up the winning room. */
        liot_trace("[DEV] call accept rejected code=%d\r\n", business);
        dev_finish_session(business, DEV_PLATFORM_NONE, false);
        return DEV_ERR_CONNECT;
    }
    if (!cJSON_IsObject(data) ||
        dev_json_copy(data, "token", token, sizeof(token), true) != 0 ||
        dev_json_copy(data, "device_id", peer_id,
                      sizeof(peer_id), true) != 0)
    {
        cJSON_Delete(response_root);
        dev_secure_zero(token, sizeof(token));
        dev_finish_session(DEV_ERR_CONNECT, DEV_PLATFORM_HANGUP, false);
        return DEV_ERR_CONNECT;
    }
    cJSON_Delete(response_root);
    if (!dev_safe_device_id(peer_id) ||
        strcmp(peer_id, expected_peer_id) != 0)
    {
        dev_secure_zero(token, sizeof(token));
        dev_finish_session(DEV_ERR_ROOM, DEV_PLATFORM_HANGUP, false);
        return DEV_ERR_ROOM;
    }
    if (!dev_accept_context_current(generation, expected_room_id,
                                    expected_peer_id))
    {
        dev_secure_zero(token, sizeof(token));
        liot_trace("[DEV] device-info became stale before P2P submit "
                   "generation=%u\r\n", (unsigned int)generation);
        return 0;
    }
    ret = dev_submit_callee_connect(token);
    dev_secure_zero(token, sizeof(token));
    if (ret != 0)
    {
        dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
        return ret;
    }
    return 0;
}

/* During a 40202 recovery the authoritative room returned by the call
 * service is kept in s_existing_room_hint until GET /v1/call/room has
 * reconciled the role.  MQTT can arrive before that GET succeeds, so room
 * scoped events must match the effective room instead of s_room_id only.
 * If the 40202 response omitted room_id, only a call_incoming invitation may
 * supply the missing hint.  Delayed cancel/reject events are never allowed
 * to adopt a room and terminate an unrelated recovery. */
static bool dev_event_matches_current_room(const char *room_id,
                                           bool allow_hint_adoption)
{
    bool matches = false;

    if (room_id == NULL || room_id[0] == '\0')
    {
        return false;
    }

    liot_rtos_enter_critical();
    if (s_room_id[0] != '\0' && strcmp(room_id, s_room_id) == 0)
    {
        matches = true;
    }
    else if (s_existing_room_recovery)
    {
        if (s_existing_room_hint[0] != '\0')
        {
            matches = strcmp(room_id, s_existing_room_hint) == 0;
        }
        else if (allow_hint_adoption)
        {
            memcpy(s_existing_room_hint, room_id,
                   sizeof(s_existing_room_hint));
            s_existing_room_hint[sizeof(s_existing_room_hint) - 1U] = '\0';
            matches = true;
        }
    }
    liot_rtos_exit_critical();
    return matches;
}

static void dev_process_mqtt(void)
{
    dev_mqtt_event_t event;

    while (s_mqtt_queue != NULL &&
           liot_rtos_queue_wait(s_mqtt_queue, (uint8 *)&event,
                                sizeof(event),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        if (event.type == DEV_MQTT_CONTACTS_UPDATE)
        {
            s_request_refresh = true;
            continue;
        }
        if (event.type == DEV_MQTT_INCOMING)
        {
            bool other_idle;
            bool local_ready;
            int claim_ret;

            /* QoS1 signaling may deliver the same call more than once.  It
             * is already our active room, so do not reject the legitimate
             * first invitation as "busy". */
            if (dev_state_has_session(s_state) &&
                dev_event_matches_current_room(event.room_id, true))
            {
                liot_trace("[DEV] duplicate incoming room ignored\r\n");
                continue;
            }
            other_idle = dev_other_features_idle();
            if (!s_home_allowed || s_request_call ||
                s_state != DEMO_DEV_CHAT_READY || !other_idle ||
                !demo_tirtc_is_ready() ||
                !dev_connect_lifecycle_idle())
            {
                (void)dev_room_action("/v1/call/reject",
                                      event.room_id, "busy");
                liot_trace("[DEV] incoming busy home=%u request=%u "
                           "state=%d peers_idle=%u\r\n",
                           s_home_allowed ? 1U : 0U,
                           s_request_call ? 1U : 0U, (int)s_state,
                           other_idle ? 1U : 0U);
                continue;
            }
            claim_ret = dev_tirtc_claim();
            liot_rtos_enter_critical();
            local_ready = s_home_allowed && !s_request_call &&
                          s_state == DEMO_DEV_CHAT_READY;
            liot_rtos_exit_critical();
            if (claim_ret != 0 || !local_ready)
            {
                if (claim_ret == 0)
                {
                    dev_tirtc_begin_release(false);
                    (void)dev_tirtc_try_release();
                }
                (void)dev_room_action("/v1/call/reject",
                                      event.room_id, "busy");
                liot_trace("[DEV] incoming rejected: TiRTC owner ret=%d "
                           "local_ready=%u\r\n", claim_ret,
                           local_ready ? 1U : 0U);
                continue;
            }
            dev_begin_session(false, event.peer_id, event.peer_name);
            memcpy(s_room_id, event.room_id, sizeof(s_room_id));
            ++s_incoming_generation;
            if (s_incoming_generation == 0U)
            {
                ++s_incoming_generation;
            }
            s_deadline_ms = liot_rtos_get_running_time() +
                            DEV_INCOMING_TIMEOUT_MS;
            dev_set_state(DEMO_DEV_CHAT_RINGING, 0);
            liot_trace("[DEV] incoming %s room accepted as audio-only generation=%u\r\n",
                       dev_call_type_name(event.call_type),
                       (unsigned int)s_incoming_generation);
            continue;
        }
        if (!dev_event_matches_current_room(event.room_id, false))
        {
            continue;
        }
        if (event.type == DEV_MQTT_ROOM_CANCEL)
        {
            liot_trace("[DEV] remote room ended\r\n");
            dev_finish_session(0, DEV_PLATFORM_NONE, false);
            continue;
        }
        if (event.type == DEV_MQTT_CALL_REJECT)
        {
            /* A room may ring several target devices.  One callee rejecting
             * does not end the caller's room; the server sends room_cancel
             * only after the room itself is finished/all targets rejected. */
            liot_trace("[DEV] one callee rejected; room still pending\r\n");
            continue;
        }
        if (event.type == DEV_MQTT_CALLEE_ANSWERED)
        {
            liot_trace("[DEV] callee answered; waiting P2P+room confirm\r\n");
        }
    }
}

static bool dev_parse_confirm(const dev_signal_event_t *event)
{
    cJSON *root;
    int valid;

    if (event == NULL || event->length == 0U)
    {
        return false;
    }
    root = cJSON_Parse(event->payload);
    valid = root != NULL &&
            dev_json_copy(root, "room_id", s_confirm_room,
                          sizeof(s_confirm_room), true) == 0;
    cJSON_Delete(root);
    return valid != 0;
}

static void dev_process_signals(void)
{
    dev_signal_event_t event;
    int ret;

    while (s_signal_queue != NULL &&
           liot_rtos_queue_wait(s_signal_queue, (uint8 *)&event,
                                sizeof(event),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        if (event.generation != s_generation ||
            (event.type != DEV_SIGNAL_DISCONNECTED &&
             event.type != DEV_SIGNAL_CONN_ERROR &&
             event.connection != NULL &&
             event.connection != (tirtc_conn_t)s_conn))
        {
            /* The managed adapter owns stale/error handles and closes them
             * exactly once; this queue only carries business correlation. */
            continue;
        }
        if (event.type == DEV_SIGNAL_INCOMING_CONNECTED)
        {
            liot_trace("[DEV] caller accepted callee P2P\r\n");
            if (s_state == DEMO_DEV_CHAT_IN_CALL)
            {
                /* In-place 40202 recovery may have activated from an early
                 * confirmation before this queued event drains. */
                continue;
            }
            if (s_room_id[0] != '\0')
            {
                dev_set_state(DEMO_DEV_CHAT_WAIT_CONFIRM, 0);
                s_deadline_ms = liot_rtos_get_running_time() +
                                DEV_CONFIRM_TIMEOUT_MS;
                ret = dev_try_activate_caller();
                if (ret < 0)
                {
                    dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
                }
            }
            continue;
        }
        if (event.type == DEV_SIGNAL_OUTGOING_CONNECTED)
        {
            if (event.connect_attempt != s_connect_attempt)
            {
                liot_trace("[DEV] stale P2P callback attempt=%u current=%u ignored\r\n",
                           (unsigned int)event.connect_attempt,
                           (unsigned int)s_connect_attempt);
                if (event.connection != NULL)
                {
                    liot_rtos_enter_critical();
                    if (s_conn == event.connection)
                    {
                        s_conn = NULL;
                    }
                    liot_rtos_exit_critical();
                    (void)demo_tirtc_disconnect(
                        DEMO_TIRTC_OWNER_DEV_CHAT, s_tirtc_generation);
                }
                continue;
            }
            liot_rtos_enter_critical();
            s_active_connect_context = NULL;
            liot_rtos_exit_critical();
            liot_trace("[DEV] callee P2P callback attempt=%u/%u error=%d handle=%u\r\n",
                       (unsigned int)event.connect_attempt,
                       (unsigned int)DEV_CONNECT_MAX_ATTEMPTS,
                       event.error, event.connection != NULL ? 1U : 0U);
            if (event.error != 0 || event.connection == NULL)
            {
                ret = event.error != 0 ? event.error : DEV_ERR_CONNECT;
                liot_trace("[DEV] callee P2P terminal failure ret=%d; "
                           "no in-place retry\r\n", ret);
                dev_finish_session(ret,
                                   DEV_PLATFORM_HANGUP, false);
                continue;
            }
            {
                char confirmation[DEV_ROOM_MAX + 20U];
                int length = snprintf(confirmation, sizeof(confirmation),
                                      "{\"room_id\":\"%s\"}",
                                      s_room_id);
                ret = length > 0 && length < (int)sizeof(confirmation) ?
                          demo_tirtc_send_command(
                              DEMO_TIRTC_OWNER_DEV_CHAT,
                              s_tirtc_generation,
                              DEV_CMD_CONFIRM,
                              confirmation,
                              (uint32_t)length) :
                          DEV_ERR_CONFIRM;
            }
            if (ret < 0)
            {
                dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
                continue;
            }
            liot_trace("[DEV] room confirmation sent\r\n");
            ret = dev_start_audio();
            if (ret < 0)
            {
                dev_finish_session(ret, DEV_PLATFORM_HANGUP, true);
            }
            continue;
        }
        if (event.type == DEV_SIGNAL_COMMAND)
        {
            if (dev_command_is(event.command, DEV_CMD_HANGUP))
            {
                liot_trace("[DEV] peer hangup command\r\n");
                dev_finish_session(0, DEV_PLATFORM_NONE, false);
                continue;
            }
            if (!s_caller ||
                !dev_command_is(event.command, DEV_CMD_CONFIRM) ||
                !dev_parse_confirm(&event))
            {
                if (s_caller)
                {
                    dev_finish_session(DEV_ERR_CONFIRM,
                                       DEV_PLATFORM_HANGUP, false);
                }
                continue;
            }
            liot_trace("[DEV] room confirmation received\r\n");
            ret = dev_try_activate_caller();
            if (ret < 0)
            {
                dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
            }
            continue;
        }
        if (event.type == DEV_SIGNAL_CONN_ERROR)
        {
            liot_trace("[DEV] connection error=%d\r\n", event.error);
            dev_finish_session(event.error != 0 ? event.error :
                                                  DEV_ERR_CONNECTION,
                               DEV_PLATFORM_HANGUP, false);
            continue;
        }
        if (event.type == DEV_SIGNAL_DISCONNECTED)
        {
            liot_trace("[DEV] peer disconnected\r\n");
            /* on_disconnected is the SDK's handle-release boundary.  Clear
             * our handle before cleanup so it is never disconnected twice,
             * but still close a possibly orphaned server room. */
            liot_rtos_enter_critical();
            if (s_conn == event.connection)
            {
                s_conn = NULL;
            }
            liot_rtos_exit_critical();
            dev_finish_session(DEV_ERR_CONNECTION,
                               DEV_PLATFORM_HANGUP, false);
        }
    }
}

static void dev_process_control(void)
{
    uint32_t answer_generation = 0U;
    bool call = false;
    bool hangup = false;
    bool conn_error = false;
    bool refresh = false;
    int conn_error_code = DEV_ERR_CONNECTION;
    demo_dev_chat_state_e state;

    liot_rtos_enter_critical();
    state = s_state;
    if (s_request_call && state == DEMO_DEV_CHAT_READY)
    {
        call = true;
        s_request_call = false;
        s_call_starting = true;
    }
    if (s_request_answer_generation != 0U &&
        state == DEMO_DEV_CHAT_RINGING)
    {
        answer_generation = s_request_answer_generation;
        s_request_answer_generation = 0U;
    }
    if (s_request_hangup && dev_state_has_session(state) &&
        !s_existing_room_recovery)
    {
        hangup = true;
        s_request_hangup = false;
    }
    if (s_request_conn_error && dev_state_has_session(state))
    {
        conn_error = true;
        conn_error_code = s_request_conn_error_code != 0 ?
                              s_request_conn_error_code :
                              DEV_ERR_CONNECTION;
        s_request_conn_error = false;
        s_request_conn_error_code = 0;
    }
    if (s_request_refresh && state == DEMO_DEV_CHAT_READY)
    {
        refresh = true;
        s_request_refresh = false;
    }
    liot_rtos_exit_critical();

    if (conn_error)
    {
        dev_finish_session(conn_error_code, DEV_PLATFORM_HANGUP, false);
        return;
    }
    if (hangup)
    {
        if (state == DEMO_DEV_CHAT_RINGING)
        {
            dev_finish_session(0, DEV_PLATFORM_REJECT, false);
        }
        else if (s_caller && !s_existing_answered_caller &&
                 (state == DEMO_DEV_CHAT_DIALING ||
                  state == DEMO_DEV_CHAT_WAIT_CONFIRM))
        {
            dev_finish_session(0, DEV_PLATFORM_CANCEL,
                               s_conn != NULL);
        }
        else
        {
            dev_finish_session(0, DEV_PLATFORM_HANGUP, true);
        }
        return;
    }
    if (answer_generation != 0U)
    {
        if (answer_generation == s_incoming_generation)
        {
            (void)dev_accept_incoming();
        }
        return;
    }
    if (call)
    {
        (void)dev_start_outgoing_call();
        liot_rtos_enter_critical();
        s_call_starting = false;
        if (!dev_state_has_session(s_state))
        {
            s_request_hangup = false;
            s_request_call_sequence = 0U;
            s_completed_sequence = s_session_sequence;
        }
        liot_rtos_exit_critical();
        return;
    }
    if (refresh)
    {
        int ret = dev_fetch_contacts();
        if (ret != 0)
        {
            dev_set_state(DEMO_DEV_CHAT_ERROR, ret);
            s_error_deadline_ms = liot_rtos_get_running_time() +
                                  DEV_ERROR_VISIBLE_MS;
            s_contact_retry_at = liot_rtos_get_running_time() +
                                 DEV_CONTACT_RETRY_MS;
        }
        else
        {
            dev_set_state(DEMO_DEV_CHAT_READY, 0);
        }
    }
}

static void dev_process_timeouts(void)
{
    bool callback_ready;
    bool cleanup_pending;
    int ret;

    if (s_state == DEMO_DEV_CHAT_ERROR &&
        !dev_deadline_pending(s_error_deadline_ms))
    {
        liot_rtos_enter_critical();
        cleanup_pending = s_tirtc_claimed || s_tirtc_release_pending ||
                          !dev_connect_lifecycle_idle_locked();
        liot_rtos_exit_critical();
        if (cleanup_pending)
        {
            /* Do not advertise READY while the SDK still owns a timed-out
             * asynchronous request.  Its late callback will release it. */
            return;
        }
        dev_set_state(dev_transport_ready() ? DEMO_DEV_CHAT_READY :
                                              DEMO_DEV_CHAT_OFFLINE,
                      0);
        return;
    }
    if (!dev_state_has_session(s_state) || s_deadline_ms == 0U ||
        dev_deadline_pending(s_deadline_ms))
    {
        return;
    }
    if (s_state == DEMO_DEV_CHAT_CONNECTING)
    {
        /* A success callback may have installed s_conn just after this
         * worker drained its queue.  Give that queued event one more turn;
         * otherwise retire the expired attempt before starting the next. */
        liot_rtos_enter_critical();
        callback_ready = s_conn != NULL;
        liot_rtos_exit_critical();
        if (callback_ready)
        {
            s_deadline_ms = liot_rtos_get_running_time() + 500U;
            return;
        }
        liot_trace("[DEV] callee P2P timeout attempt=%u/%u\r\n",
                   (unsigned int)s_connect_attempt,
                   (unsigned int)DEV_CONNECT_MAX_ATTEMPTS);
        ret = DEV_ERR_TIMEOUT;
        liot_trace("[DEV] callee P2P watchdog expired; "
                   "session cleanup only\r\n");
        dev_finish_session(ret,
                           DEV_PLATFORM_HANGUP, false);
        return;
    }
    liot_trace("[DEV] session timeout state=%d\r\n", (int)s_state);
    if (s_existing_room_recovery)
    {
        /* The room role is still unknown.  End locally and let the next idle
         * recovery reconcile it instead of canceling another role's room. */
        dev_finish_session(DEV_ERR_TIMEOUT, DEV_PLATFORM_NONE, false);
        return;
    }
    if (s_state == DEMO_DEV_CHAT_RINGING)
    {
        dev_finish_session(DEV_ERR_TIMEOUT, DEV_PLATFORM_REJECT, false);
    }
    else if (s_caller && !s_existing_answered_caller &&
             (s_state == DEMO_DEV_CHAT_DIALING ||
              s_state == DEMO_DEV_CHAT_WAIT_CONFIRM))
    {
        dev_finish_session(DEV_ERR_TIMEOUT, DEV_PLATFORM_CANCEL,
                           s_conn != NULL);
    }
    else
    {
        dev_finish_session(DEV_ERR_TIMEOUT, DEV_PLATFORM_HANGUP, false);
    }
}

void demo_dev_chat_task(void *argv)
{
    liot_task_t room_action_handle = NULL;
    bool mqtt_registered = false;
    bool transport_was_ready = false;
    bool transport_ready;
    bool levels_dirty;
    uint8_t speaker_level;
    uint8_t mic_level;
    int ret;

    (void)argv;
    if (liot_rtos_semaphore_create(&s_event_sem, 0U) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_mqtt_queue, sizeof(dev_mqtt_event_t),
                               DEV_MQTT_EVENT_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_signal_queue, sizeof(dev_signal_event_t),
                               DEV_SIGNAL_EVENT_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_audio_queue, sizeof(uint8_t),
                               DEV_AUDIO_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_room_action_queue,
                               sizeof(dev_room_action_job_t),
                               DEV_ROOM_ACTION_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_task_create(&room_action_handle,
                              DEV_ROOM_ACTION_TASK_STACK,
                              LIOT_APP_TASK_PRIORITY,
                              "dev_call_http",
                              dev_room_action_task,
                              NULL) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (demo_formal_mqtt_register_handler(dev_formal_mqtt_handler,
                                          NULL) != 0)
    {
        goto init_failed;
    }
    mqtt_registered = true;
    demo_tirtc_set_feature_listener(DEMO_TIRTC_FEATURE_DEV_CHAT,
                                    &s_tirtc_listener);
    liot_trace("[DEV] worker ready; official call-server/P2P flow "
               "free_heap=%u\r\n",
               (unsigned int)liot_xPortGetFreeHeapSize());

    while (1)
    {
        (void)dev_tirtc_try_release();
        if (!dev_state_has_session(s_state))
        {
            /* Complete cleanup of a callback that crossed the bounded
             * teardown wait before admitting the next audio session. */
            dev_drain_audio();
        }
        transport_ready = dev_transport_ready();
        if (!transport_ready)
        {
            if (dev_state_has_session(s_state))
            {
                dev_platform_action_e action = DEV_PLATFORM_HANGUP;

                /* The TiRTC adapter can become unavailable before the
                 * asynchronous connect result reaches this worker.  Close
                 * the server-side room now instead of relying on the next
                 * boot's stale-room recovery. */
                if (s_state == DEMO_DEV_CHAT_RINGING)
                {
                    action = DEV_PLATFORM_REJECT;
                }
                else if (s_caller && !s_existing_answered_caller &&
                         (s_state == DEMO_DEV_CHAT_DIALING ||
                          s_state == DEMO_DEV_CHAT_WAIT_CONFIRM))
                {
                    action = DEV_PLATFORM_CANCEL;
                }
                dev_finish_session(DEV_ERR_CONNECTION,
                                   action, false);
            }
            s_contacts_ready = false;
            transport_was_ready = false;
            s_room_check_at = 0U;
            dev_set_state(DEMO_DEV_CHAT_OFFLINE, 0);
            (void)liot_rtos_semaphore_wait(s_event_sem, 500U);
            continue;
        }
        if (!transport_was_ready)
        {
            transport_was_ready = true;
            dev_set_state(DEMO_DEV_CHAT_READY, 0);
            s_request_refresh = true;
            s_contact_retry_at = 0U;
            s_room_check_at = liot_rtos_get_running_time() +
                              DEV_ROOM_FIRST_CHECK_MS;
        }

        dev_process_mqtt();
        dev_process_signals();
        dev_process_control();
        dev_process_timeouts();

        if (s_existing_room_recovery &&
            !dev_deadline_pending(s_existing_room_retry_at))
        {
            (void)dev_recover_existing_room();
        }
        else if (s_room_check_at != 0U &&
            !dev_deadline_pending(s_room_check_at) &&
            dev_recovery_safe())
        {
            (void)dev_recover_room();
            s_room_check_at = liot_rtos_get_running_time() +
                              DEV_ROOM_CHECK_INTERVAL_MS;
        }

        if (!s_contacts_ready && s_state == DEMO_DEV_CHAT_READY &&
            !s_request_refresh &&
            (s_contact_retry_at == 0U ||
             !dev_deadline_pending(s_contact_retry_at)))
        {
            s_request_refresh = true;
        }

        liot_rtos_enter_critical();
        levels_dirty = s_levels_dirty;
        speaker_level = s_speaker_level;
        mic_level = s_mic_level;
        if (levels_dirty)
        {
            s_levels_dirty = false;
        }
        liot_rtos_exit_critical();
        if (levels_dirty && s_state == DEMO_DEV_CHAT_IN_CALL)
        {
            (void)demo_ai_audio_set_levels(&s_audio_lease,
                                           speaker_level, mic_level);
            (void)demo_ai_audio_apply_levels(&s_audio_lease);
        }

        if (s_state == DEMO_DEV_CHAT_IN_CALL)
        {
            dev_service_downlink();
            ret = dev_send_uplink();
            if (ret < 0)
            {
                liot_trace("[DEV] uplink/audio failed ret=%d\r\n", ret);
                dev_finish_session(ret, DEV_PLATFORM_HANGUP, false);
            }
            continue;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, DEV_IDLE_WAIT_MS);
    }

init_failed:
    if (mqtt_registered)
    {
        demo_formal_mqtt_unregister_handler(dev_formal_mqtt_handler, NULL);
    }
    if (room_action_handle != NULL)
    {
        (void)liot_rtos_task_delete(room_action_handle);
    }
    if (s_room_action_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_room_action_queue);
        s_room_action_queue = NULL;
    }
    if (s_audio_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_audio_queue);
        s_audio_queue = NULL;
    }
    if (s_signal_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_signal_queue);
        s_signal_queue = NULL;
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
    liot_trace("[DEV] worker initialization failed\r\n");
    dev_set_state(DEMO_DEV_CHAT_ERROR, DEV_ERR_INIT);
    liot_rtos_task_delete(NULL);
}

void demo_dev_chat_mark_unavailable(void)
{
    dev_set_state(DEMO_DEV_CHAT_ERROR, DEV_ERR_INIT);
}

void demo_dev_chat_enter(void)
{
    liot_rtos_enter_critical();
    if (s_state == DEMO_DEV_CHAT_READY ||
        s_state == DEMO_DEV_CHAT_ERROR)
    {
        s_request_refresh = true;
    }
    liot_rtos_exit_critical();
    dev_signal();
}

void demo_dev_chat_leave(void)
{
    liot_rtos_enter_critical();
    s_request_call = false;
    if (!s_call_starting)
    {
        s_request_call_sequence = 0U;
    }
    if (s_state != DEMO_DEV_CHAT_STOPPING &&
        (s_call_starting || dev_state_has_session(s_state)))
    {
        s_request_hangup = true;
    }
    liot_rtos_exit_critical();
    dev_signal();
}

void demo_dev_chat_select_next(void)
{
    liot_rtos_enter_critical();
    if (s_state == DEMO_DEV_CHAT_READY && s_contact_count > 0U)
    {
        s_selected_contact =
            (uint8_t)((s_selected_contact + 1U) % s_contact_count);
        memcpy(s_display_name, s_contacts[s_selected_contact].display,
               sizeof(s_display_name));
    }
    liot_rtos_exit_critical();
}

uint32_t demo_dev_chat_call_selected(void)
{
    uint32_t sequence = 0U;

    liot_rtos_enter_critical();
    if (s_state == DEMO_DEV_CHAT_READY && s_contact_count > 0U &&
        !s_request_call && !s_call_starting && !s_tirtc_claimed &&
        demo_tirtc_is_ready() &&
        dev_connect_lifecycle_idle_locked())
    {
        ++s_session_sequence;
        if (s_session_sequence == 0U)
        {
            ++s_session_sequence;
        }
        s_request_call_sequence = s_session_sequence;
        s_request_call = true;
        sequence = s_request_call_sequence;
    }
    liot_rtos_exit_critical();
    if (sequence != 0U)
    {
        dev_signal();
    }
    return sequence;
}

bool demo_dev_chat_answer(uint32_t incoming_generation)
{
    bool accepted = false;

    liot_rtos_enter_critical();
    if (s_state == DEMO_DEV_CHAT_RINGING &&
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
        dev_signal();
    }
    return accepted;
}

void demo_dev_chat_hangup(void)
{
    liot_rtos_enter_critical();
    s_request_call = false;
    if (!s_call_starting)
    {
        s_request_call_sequence = 0U;
    }
    /* Teardown already owns STOPPING; a repeated KEY2 must not arm a hangup
     * for the next session.  READY can still contain a worker-owned call. */
    if (s_state != DEMO_DEV_CHAT_STOPPING &&
        (s_call_starting || dev_state_has_session(s_state)))
    {
        s_request_hangup = true;
    }
    liot_rtos_exit_critical();
    dev_signal();
}

void demo_dev_chat_refresh_contacts(void)
{
    s_request_refresh = true;
    dev_signal();
}

void demo_dev_chat_get_snapshot(demo_dev_chat_snapshot_t *out)
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
    out->selected_online =
        s_contact_count > 0U && s_selected_contact < s_contact_count ?
            s_contacts[s_selected_contact].online : false;
    out->contacts_revision = s_contacts_revision;
    out->incoming_generation = s_incoming_generation;
    out->session_sequence = s_session_sequence;
    out->completed_sequence = s_completed_sequence;
    out->rx_dropped = s_rx_dropped;
    out->tx_dropped = s_tx_dropped;
    memcpy(out->display_name, s_display_name, sizeof(out->display_name));
    liot_rtos_exit_critical();
}

void demo_dev_chat_set_home_allowed(bool allowed)
{
    liot_rtos_enter_critical();
    s_home_allowed = allowed;
    liot_rtos_exit_critical();
}

bool demo_dev_chat_blocks_live(void)
{
    return dev_state_has_session(s_state);
}

bool demo_dev_chat_is_idle(void)
{
    uint32_t i;
    bool idle;

    liot_rtos_enter_critical();
    idle = !dev_state_has_session(s_state) && !s_request_call &&
           !s_call_starting &&
           !s_existing_room_recovery &&
           !s_recovery_inflight &&
           s_conn == NULL && !s_audio_owned && !s_tirtc_claimed &&
           s_audio_producers == 0U && s_audio_queued == 0U &&
           !dev_deadline_pending(s_reject_incoming_until);
    for (i = 0U; i < DEV_CONNECT_CONTEXT_COUNT && idle; ++i)
    {
        idle = !s_connect_contexts[i].pending;
    }
    liot_rtos_exit_critical();
    return idle;
}

void demo_dev_chat_set_audio_levels(uint8_t speaker_level,
                                    uint8_t mic_level)
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
    dev_signal();
}
