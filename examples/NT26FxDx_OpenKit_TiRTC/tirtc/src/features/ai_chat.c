/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * TiRTC AI Chat MVP for NT26F6D0/F6D_A.
 *
 * The ES8311 side is raw signed PCM S16LE, 16 kHz, mono.  The network side
 * is explicitly requested and validated as Opus, 16 kHz, mono.  Every uplink
 * packet represents one 20 ms PCM frame.  SDK callbacks only copy into small
 * bounded queues; HTTP, JSON, codec and audio-driver work stays in this task.
 *
 * This first version is intentionally half duplex because the current board
 * path has no validated acoustic echo canceller.  Recording pauses while the
 * AI audio is playing, preventing the speaker from feeding back into ASR.
 */

#include "ai_chat.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "audio_device.h"
#include "opus_codec.h"
#include "device_binding.h"
#include "tirtc_runtime.h"
#include "liot_log.h"
#include "liot_os.h"
#include "tirtc/tiRTC.h"


#define AI_CMD_WORD                    0x2100U
#define AI_STREAM_ID                   1U
#define AI_SEND_HIGH_WATER_BYTES       (12U * 1024U)
#define AI_SDK_BEARER_SAFE_MAX         600U
#define AI_CONNECT_TIMEOUT_MS          30000U
#define AI_ACCESS_PREFETCH_TTL_MS      15000U
#define AI_START_RESPONSE_TIMEOUT_MS   15000U
#define AI_END_FLUSH_TIMEOUT_MS         100U
#define AI_DISCONNECT_SETTLE_MS        5000U
#define AI_CALLBACK_QUIESCE_MS         1000U
#define AI_KCP_SETTLE_MS               300U
#define AI_ACTIVE_POLL_MS              10U
#define AI_SPEECH_IDLE_MS              800U
#ifndef TIRTC_AI_PREBUFFER_PACKETS
#define TIRTC_AI_PREBUFFER_PACKETS       1U
#endif
#define AI_PREBUFFER_PACKETS TIRTC_AI_PREBUFFER_PACKETS
#define AI_PREBUFFER_TIMEOUT_MS         80U
#define AI_AUDIO_QUEUE_DEPTH           16U
#define AI_COMMAND_QUEUE_DEPTH         8U
#define AI_UNWANTED_QUEUE_DEPTH        4U
#define AI_WHIP_CONTEXT_COUNT          4U
#define AI_COMMAND_MAX_BYTES           1024U
#define AI_DECODE_MAX_SAMPLES          1920U
#define AI_PLAY_BATCH_SAMPLES          2560U

#define AI_ERR_CANCELLED               (-1000)
#define AI_ERR_GET_TOKEN               (-1001)
#define AI_ERR_AUDIO_INIT              (-1002)
#define AI_ERR_OPUS_INIT               (-1003)
#define AI_ERR_WHIP_SUBMIT             (-1008)
#define AI_ERR_WHIP_TIMEOUT            (-1009)
#define AI_ERR_START_SEND              (-1010)
#define AI_ERR_START_TIMEOUT           (-1011)
#define AI_ERR_START_REJECTED          (-1012)
#define AI_ERR_AUDIO_FORMAT            (-1013)
#define AI_ERR_CONNECTION              (-1014)
#define AI_ERR_QUEUE_INIT              (-1015)
#define AI_ERR_TASK_CREATE             (-1016)
#define AI_ERR_TOKEN_TOO_LONG          (-1017)
#define AI_ERR_DOWNLINK_SUBSCRIBE      (-1018)

typedef struct
{
    uint16_t length;
    uint8_t media;
    uint8_t flags;
    uint32_t ts;
    uint8_t payload[DEMO_AI_OPUS_MAX_PACKET];
} ai_audio_message_t;

typedef struct
{
    uint16_t length;
    uint32_t cmdw;
    char payload[AI_COMMAND_MAX_BYTES + 1U];
} ai_command_message_t;

typedef struct
{
    uint32_t generation;
    volatile bool pending;
} ai_whip_context_t;

static liot_sem_t s_event_sem;
static liot_queue_t s_audio_queue;
static liot_queue_t s_command_queue;
static liot_queue_t s_unwanted_queue;

/*
 * TiRTC invokes media and command callbacks from SDK-owned threads whose
 * stack size is intentionally small on EC718.  Never put a 1 KiB packet on
 * that callback stack and then ask the RTOS queue to copy it again.  The
 * fixed pools own packet storage; queues carry only a one-byte slot index.
 */
static ai_audio_message_t s_audio_pool[AI_AUDIO_QUEUE_DEPTH];
static bool s_audio_pool_used[AI_AUDIO_QUEUE_DEPTH];
static ai_command_message_t s_command_pool[AI_COMMAND_QUEUE_DEPTH];
static bool s_command_pool_used[AI_COMMAND_QUEUE_DEPTH];
static volatile uint32_t s_callback_producers;

static volatile demo_ai_chat_state_e s_state = DEMO_AI_CHAT_IDLE;
static volatile int s_error;
static volatile bool s_want_active;
static volatile uint32_t s_control_sequence;
static volatile uint32_t s_running_request_id;
static volatile uint32_t s_idle_ack_sequence;
static volatile uint32_t s_completed_request_id;
static volatile int s_completed_result;
static volatile bool s_prepare_requested;
static volatile bool s_levels_dirty = true;
static volatile uint8_t s_speaker_level = 8U;
static volatile uint8_t s_mic_level = 10U;
static volatile uint32_t s_rx_dropped;
static volatile uint32_t s_tx_dropped;
static volatile uint32_t s_request_started_ms;
static volatile bool s_first_play_reported;
static uint32_t s_rx_frames;
static uint32_t s_tx_frames;

static volatile bool s_whip_done;
static volatile int s_whip_error;
static volatile tirtc_conn_t s_whip_conn;
static volatile bool s_conn_error_pending;
static volatile int s_conn_error;
static volatile bool s_disconnected_pending;
static volatile tirtc_conn_t s_cleanup_conn;
static volatile bool s_cleanup_disconnect_done;
static volatile uint32_t s_unwanted_pending;
static volatile bool s_unwanted_overflow;

static tirtc_conn_t volatile s_conn;
static uint32_t s_generation;
static ai_whip_context_t s_whip_contexts[AI_WHIP_CONTEXT_COUNT];
static ai_whip_context_t * volatile s_active_whip_context;
static demo_binding_ai_access_t s_access;
/* Only demo_ai_chat_task writes the cache metadata and credential object. */
static bool s_access_prepared;
static uint32_t s_access_prepared_ms;
static char s_start_request_id[32];
static bool s_start_submitted;
static bool s_start_response_done;
static int s_start_response_error;
static bool s_server_speaking;
static bool s_remote_ended;
static volatile uint32_t s_last_audio_ms;
static uint32_t s_play_pending_samples;
static uint32_t s_prebuffer_start_ms;
static demo_ai_audio_lease_t s_audio_lease = DEMO_AI_AUDIO_LEASE_INIT;
static volatile bool s_audio_owned;
static volatile bool s_tirtc_owned;
static volatile uint32_t s_tirtc_session_generation;
static bool s_tirtc_release_error_logged;

static __attribute__((aligned(16))) int16_t
    s_capture_pcm[DEMO_AI_PCM_SAMPLES_20MS];
static uint8_t s_uplink_opus[DEMO_AI_OPUS_MAX_PACKET];
static __attribute__((aligned(16))) int16_t
    s_decode_pcm[AI_DECODE_MAX_SAMPLES];
static __attribute__((aligned(16))) int16_t
    s_play_pcm[AI_PLAY_BATCH_SAMPLES];

static void ai_signal(void)
{
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_release(s_event_sem);
    }
}

static uint32_t ai_request_elapsed_ms(void)
{
    uint32_t started_ms = s_request_started_ms;

    return started_ms == 0U ? 0U :
        (uint32_t)(liot_rtos_get_running_time() - started_ms);
}

static bool ai_binding_is_ready(void)
{
    demo_binding_snapshot_t binding;

    demo_binding_get_snapshot(&binding);
    return binding.state == DEMO_BIND_BOUND;
}

static void ai_access_clear(void)
{
    demo_binding_clear_ai_access(&s_access);
    s_access_prepared = false;
    s_access_prepared_ms = 0U;
}

static bool ai_access_prepared_is_valid(uint32_t now_ms)
{
    if (!s_access_prepared)
    {
        return false;
    }
    if (!ai_binding_is_ready() ||
        (uint32_t)(now_ms - s_access_prepared_ms) >=
            AI_ACCESS_PREFETCH_TTL_MS)
    {
        liot_trace("[AI-PREFETCH] discarded expired/unbound credentials\r\n");
        ai_access_clear();
        return false;
    }
    return true;
}

static int ai_access_fetch(void)
{
    int ret;

    ai_access_clear();
    ret = demo_binding_fetch_ai_access(&s_access);
    if (ret != 0)
    {
        ai_access_clear();
        return ret;
    }
    if (strlen(s_access.token) > AI_SDK_BEARER_SAFE_MAX)
    {
        liot_trace("[AI] token too long for TiRTC WHIP header len=%u max=%u\r\n",
                   (unsigned int)strlen(s_access.token),
                   (unsigned int)AI_SDK_BEARER_SAFE_MAX);
        ai_access_clear();
        return AI_ERR_TOKEN_TOO_LONG;
    }
    return 0;
}

/* Runs only in demo_ai_chat_task.  It performs one opportunistic fetch and
 * never retries or refreshes periodically.  A failed prefetch is deliberately
 * invisible to the UI because demo_ai_chat_start keeps the synchronous path. */
static void ai_access_prepare(void)
{
    uint32_t now_ms = liot_rtos_get_running_time();
    int opus_ret;
    int ret;

    /* Opus objects are process-lifetime singletons.  Build them in the same
     * AI worker before the user enters the page; session init remains as an
     * idempotent fallback if this best-effort warm-up cannot allocate. */
    opus_ret = demo_ai_opus_init();
    if (opus_ret == 0)
    {
        demo_ai_opus_reset();
    }
    else
    {
        liot_trace("[AI-PREFETCH] Opus warm-up failed ret=%d\r\n",
                   opus_ret);
    }

    if (ai_access_prepared_is_valid(now_ms))
    {
        return;
    }
    if (!ai_binding_is_ready())
    {
        ai_access_clear();
        liot_trace("[AI-PREFETCH] skipped: binding not ready\r\n");
        return;
    }

    liot_trace("[AI-PREFETCH] fetch begin\r\n");
    ret = ai_access_fetch();
    if (ret != 0)
    {
        liot_trace("[AI-PREFETCH] fetch failed ret=%d; synchronous fallback kept\r\n",
                   ret);
        return;
    }
    if (!ai_binding_is_ready())
    {
        ai_access_clear();
        liot_trace("[AI-PREFETCH] discarded: binding changed during fetch\r\n");
        return;
    }

    s_access_prepared_ms = liot_rtos_get_running_time();
    s_access_prepared = true;
    liot_trace("[AI-PREFETCH] ready ttl=%u ms peer_len=%u role_len=%u\r\n",
               (unsigned int)AI_ACCESS_PREFETCH_TTL_MS,
               (unsigned int)strlen(s_access.peer_id),
               (unsigned int)strlen(s_access.role_id));
}

static bool ai_access_take_prepared(void)
{
    uint32_t now_ms = liot_rtos_get_running_time();
    uint32_t age_ms;

    if (!ai_access_prepared_is_valid(now_ms))
    {
        return false;
    }

    age_ms = (uint32_t)(now_ms - s_access_prepared_ms);
    /* Consume before any session operation.  Even a failed WHIP submission
     * can therefore never return these credentials to the cache. */
    s_access_prepared = false;
    s_access_prepared_ms = 0U;
    liot_trace("[AI-PREFETCH] consumed age=%u ms\r\n",
               (unsigned int)age_ms);
    return true;
}

static int ai_audio_slot_acquire(tirtc_conn_t hconn)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    for (i = 0U; hconn == (tirtc_conn_t)s_conn &&
                  i < AI_AUDIO_QUEUE_DEPTH; i++)
    {
        if (!s_audio_pool_used[i])
        {
            s_audio_pool_used[i] = true;
            ++s_callback_producers;
            slot = (int)i;
            break;
        }
    }
    liot_rtos_exit_critical();
    return slot;
}

static void ai_audio_slot_release(uint8_t slot)
{
    if (slot >= AI_AUDIO_QUEUE_DEPTH)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_audio_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static int ai_command_slot_acquire(tirtc_conn_t hconn)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    for (i = 0U; hconn == (tirtc_conn_t)s_conn &&
                  i < AI_COMMAND_QUEUE_DEPTH; i++)
    {
        if (!s_command_pool_used[i])
        {
            s_command_pool_used[i] = true;
            ++s_callback_producers;
            slot = (int)i;
            break;
        }
    }
    liot_rtos_exit_critical();
    return slot;
}

static void ai_command_slot_release(uint8_t slot)
{
    if (slot >= AI_COMMAND_QUEUE_DEPTH)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_command_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static void ai_callback_producer_done(void)
{
    bool idle = false;

    liot_rtos_enter_critical();
    if (s_callback_producers > 0U)
    {
        --s_callback_producers;
        idle = s_callback_producers == 0U;
    }
    liot_rtos_exit_critical();
    if (idle)
    {
        ai_signal();
    }
}

static bool ai_wait_callback_producers(void)
{
    uint32_t deadline = liot_rtos_get_running_time() +
                        AI_CALLBACK_QUIESCE_MS;
    uint32_t producers;

    do
    {
        liot_rtos_enter_critical();
        producers = s_callback_producers;
        liot_rtos_exit_critical();
        if (producers == 0U)
        {
            return true;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, 10U);
    } while ((int32_t)(deadline - liot_rtos_get_running_time()) > 0);

    liot_trace("[AI] callback quiesce timeout producers=%u\r\n",
               (unsigned int)producers);
    return false;
}

static void ai_discard_audio_queue(void)
{
    uint8_t slot;
    bool producers_idle;

    while (s_audio_queue != NULL &&
           liot_rtos_queue_wait(s_audio_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        ai_audio_slot_release(slot);
    }
    liot_rtos_enter_critical();
    producers_idle = s_callback_producers == 0U;
    if (producers_idle)
    {
        memset(s_audio_pool_used, 0, sizeof(s_audio_pool_used));
    }
    liot_rtos_exit_critical();
}

static void ai_discard_command_queue(void)
{
    uint8_t slot;
    bool producers_idle;

    while (s_command_queue != NULL &&
           liot_rtos_queue_wait(s_command_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        ai_command_slot_release(slot);
    }
    liot_rtos_enter_critical();
    producers_idle = s_callback_producers == 0U;
    if (producers_idle)
    {
        memset(s_command_pool_used, 0, sizeof(s_command_pool_used));
    }
    liot_rtos_exit_critical();
}

/* The caller has already incremented s_unwanted_pending.  Keep that count
 * reserved until managed disconnect submission returns so is_idle cannot expose an SDK
 * handle that is still queued or being destroyed. */
static void ai_defer_disconnect_reserved(tirtc_conn_t hconn)
{
    if (s_unwanted_queue == NULL ||
        liot_rtos_queue_release(s_unwanted_queue, sizeof(hconn),
                                (uint8 *)&hconn,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        /* Callbacks must not call TiRtcDisconnect.  With MAX_CONNECTIONS=1
         * this queue should never overflow; count it if the invariant breaks. */
        s_rx_dropped++;
        liot_rtos_enter_critical();
        if (s_unwanted_pending > 0U)
        {
            s_unwanted_pending--;
        }
        s_unwanted_overflow = true;
        liot_rtos_exit_critical();
        liot_trace("[AI] deferred disconnect queue overflow; "
                   "idle remains blocked handle=%p\r\n", hconn);
        return;
    }
    ai_signal();
}

static void ai_defer_disconnect(tirtc_conn_t hconn)
{
    if (hconn == NULL)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_unwanted_pending++;
    liot_rtos_exit_critical();
    ai_defer_disconnect_reserved(hconn);
}

static void ai_disconnect_deferred(void)
{
    tirtc_conn_t hconn;
    uint32_t session_generation;
    bool tirtc_owned;
    int ret;

    while (s_unwanted_queue != NULL &&
           liot_rtos_queue_wait(s_unwanted_queue, (uint8 *)&hconn,
                                sizeof(hconn),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        liot_rtos_enter_critical();
        tirtc_owned = s_tirtc_owned;
        session_generation = s_tirtc_session_generation;
        liot_rtos_exit_critical();
        if (hconn != NULL && tirtc_owned)
        {
            /* Managed WHIP owns the opaque handle.  A stale callback can only
             * request teardown of the current generation, never disconnect a
             * raw address that the SDK may already have recycled. */
            ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_AI,
                                        session_generation);
            if (ret != 0 && ret != TIRTC_E_INVALID_HANDLE)
            {
                liot_trace("[AI] deferred managed disconnect ret=%d\r\n",
                           ret);
            }
        }
        else if (hconn != NULL)
        {
            liot_rtos_enter_critical();
            s_unwanted_overflow = true;
            liot_rtos_exit_critical();
            demo_tirtc_require_restart(TIRTC_E_INVALID_HANDLE);
        }
        liot_rtos_enter_critical();
        if (s_unwanted_pending > 0U)
        {
            s_unwanted_pending--;
        }
        liot_rtos_exit_critical();
    }
}

static bool ai_release_tirtc_session(void)
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

    ret = demo_tirtc_session_release(DEMO_TIRTC_OWNER_AI,
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
        liot_trace("[AI] managed session release failed ret=%d generation=%u\r\n",
                   ret, (unsigned int)session_generation);
        demo_tirtc_require_restart(ret);
    }
    return false;
}

static bool ai_deadline_pending(uint32_t deadline)
{
    return (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

/* TIRTC_E_CONN_REMOTECLOSE can arrive either through a callback or directly
 * from a send/connect API.  It always means that this conversation is over,
 * not that the UI should remain on an error page. */
static int ai_normalize_session_result(int result)
{
    if (result == TIRTC_E_CONN_REMOTECLOSE)
    {
        s_remote_ended = true;
        return AI_ERR_CANCELLED;
    }
    return result;
}

/* A peer-initiated close is the normal end of an AI conversation.  Preserve
 * every other SDK error so genuine network/server failures remain visible. */
static int ai_connection_end_result(void)
{
    if (s_conn_error_pending)
    {
        if (s_conn_error != 0)
        {
            return ai_normalize_session_result(s_conn_error);
        }
    }
    if (s_disconnected_pending)
    {
        s_remote_ended = true;
        return AI_ERR_CANCELLED;
    }
    return AI_ERR_CONNECTION;
}

/* A bool alone cannot distinguish an old request from a rapid BACK/re-enter.
 * Bind every session stage to the request id that started it. */
static bool ai_request_is_active(void)
{
    bool active;

    liot_rtos_enter_critical();
    active = s_want_active && s_running_request_id != 0U &&
             s_control_sequence == s_running_request_id;
    liot_rtos_exit_critical();
    return active;
}

static void ai_set_state(demo_ai_chat_state_e state, int error)
{
    if (s_state != state || s_error != error)
    {
        s_state = state;
        s_error = error;
        liot_trace("[AI] state=%d error=%d free_heap=%u\r\n",
                   (int)state, error,
                   (unsigned int)liot_xPortGetFreeHeapSize());
    }
}

static void ai_on_conn_error(tirtc_conn_t hconn, int error)
{
    if (hconn == s_conn || hconn == s_whip_conn)
    {
        s_conn_error = error;
        s_conn_error_pending = true;
        ai_signal();
    }
}

static void ai_on_disconnected(tirtc_conn_t hconn)
{
    bool matched = false;

    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == (tirtc_conn_t)s_cleanup_conn)
    {
        s_cleanup_disconnect_done = true;
        matched = true;
    }
    if (hconn == s_conn || hconn == s_whip_conn)
    {
        s_disconnected_pending = true;
        matched = true;
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        ai_signal();
    }
}

static void ai_on_audio(tirtc_conn_t hconn,
                        const TIRTCFRAMEINFO *frame,
                        void *data)
{
    ai_audio_message_t *message;
    uint8_t slot;
    int acquired;

    if (hconn != s_conn || frame == NULL || data == NULL ||
        frame->stream_id != AI_STREAM_ID ||
        frame->length == 0U ||
        frame->length > sizeof(s_audio_pool[0].payload))
    {
        return;
    }

    acquired = ai_audio_slot_acquire(hconn);
    if (acquired < 0)
    {
        s_rx_dropped++;
        return;
    }
    slot = (uint8_t)acquired;
    message = &s_audio_pool[slot];
    message->length = (uint16_t)frame->length;
    message->media = frame->media;
    message->flags = frame->flags;
    message->ts = frame->ts;
    memcpy(message->payload, data, frame->length);
    if (s_audio_queue == NULL ||
        liot_rtos_queue_release(s_audio_queue, sizeof(slot), &slot,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        ai_audio_slot_release(slot);
        ai_callback_producer_done();
        s_rx_dropped++;
        return;
    }
    ai_callback_producer_done();
    s_last_audio_ms = liot_rtos_get_running_time();
    ai_signal();
}

static void ai_on_command(tirtc_conn_t hconn, uint32_t cmdw,
                          const void *data, uint32_t length)
{
    ai_command_message_t *message;
    uint8_t slot;
    int acquired;

    if (hconn != s_conn || cmdw != AI_CMD_WORD || data == NULL ||
        length == 0U || length > AI_COMMAND_MAX_BYTES)
    {
        return;
    }

    acquired = ai_command_slot_acquire(hconn);
    if (acquired < 0)
    {
        s_rx_dropped++;
        return;
    }
    slot = (uint8_t)acquired;
    message = &s_command_pool[slot];
    message->length = (uint16_t)length;
    message->cmdw = cmdw;
    memcpy(message->payload, data, length);
    message->payload[length] = '\0';
    if (s_command_queue == NULL ||
        liot_rtos_queue_release(s_command_queue, sizeof(slot), &slot,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        ai_command_slot_release(slot);
        ai_callback_producer_done();
        s_rx_dropped++;
        return;
    }
    ai_callback_producer_done();
    ai_signal();
}

static int ai_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    return (hconn == s_conn && stream_id == AI_STREAM_ID) ? 0 : -1;
}

static void ai_on_unsubscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    (void)hconn;
    (void)stream_id;
}

static const demo_tirtc_listener_t s_tirtc_listener = {
    .on_conn_error = ai_on_conn_error,
    .on_disconnected = ai_on_disconnected,
    .on_audio = ai_on_audio,
    .on_command = ai_on_command,
    .on_subscribe_audio = ai_on_subscribe_audio,
    .on_unsubscribe_audio = ai_on_unsubscribe_audio,
};

static void ai_whip_connect_callback(int error, tirtc_conn_t hconn,
                                     void *user_data)
{
    ai_whip_context_t *context = (ai_whip_context_t *)user_data;
    uint32_t generation;
    bool current;
    bool disconnect_reserved = false;

    if (context == NULL)
    {
        if (error == 0 && hconn != NULL)
        {
            ai_defer_disconnect(hconn);
        }
        return;
    }

    liot_rtos_enter_critical();
    generation = context->generation;
    current = context == s_active_whip_context &&
              generation == s_generation &&
              s_state == DEMO_AI_CHAT_CONNECTING;
    if (!current && error == 0 && hconn != NULL)
    {
        /* Reserve the deferred-disconnect count before clearing pending.
         * This closes the otherwise observable false-idle gap. */
        s_unwanted_pending++;
        disconnect_reserved = true;
    }
    context->pending = false;
    if (current)
    {
        s_whip_error = error;
        s_whip_conn = hconn;
        s_whip_done = true;
    }
    liot_rtos_exit_critical();

    if (disconnect_reserved)
    {
        ai_defer_disconnect_reserved(hconn);
    }
    if (current)
    {
        ai_signal();
    }
}

static ai_whip_context_t *ai_whip_context_acquire(void)
{
    uint32_t i;
    ai_whip_context_t *context = NULL;

    liot_rtos_enter_critical();
    for (i = 0U; i < AI_WHIP_CONTEXT_COUNT; i++)
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

static void ai_whip_submit_failed(ai_whip_context_t *context, int error)
{
    uint32_t generation;

    if (context == NULL)
    {
        return;
    }
    liot_rtos_enter_critical();
    generation = context->generation;
    context->pending = false;
    if (context == s_active_whip_context && generation == s_generation &&
        s_state == DEMO_AI_CHAT_CONNECTING)
    {
        s_whip_error = error;
        s_whip_conn = NULL;
        s_whip_done = true;
    }
    liot_rtos_exit_critical();
    ai_signal();
}

static int ai_whip_submit(ai_whip_context_t *context)
{
    int ret;

    if (context == NULL)
    {
        return AI_ERR_WHIP_SUBMIT;
    }
    liot_trace("[AI-WHIP] submit begin peer_len=%u token_len=%u "
               "free_heap=%u\r\n",
               (unsigned int)strlen(s_access.peer_id),
               (unsigned int)strlen(s_access.token),
               (unsigned int)liot_xPortGetFreeHeapSize());
    ret = demo_tirtc_whip_connect(DEMO_TIRTC_OWNER_AI,
                                  s_tirtc_session_generation,
                                  s_access.peer_id, s_access.token,
                                  ai_whip_connect_callback, context);
    liot_trace("[AI] TiRtcWhipConnect submit ret=%d\r\n", ret);
    if (ret != 0)
    {
        ret = ai_normalize_session_result(ret);
        ai_whip_submit_failed(context, ret);
    }
    return ret;
}


/* Return 0 when the optional field is absent, 1 when it matches the format
 * requested by this firmware, and -1 when the server explicitly selected an
 * unsupported format. */
static int ai_json_audio_validate_optional(const cJSON *result,
                                           const char *field)
{
    const cJSON *audio = cJSON_GetObjectItemCaseSensitive(result, field);
    const cJSON *codec;
    const cJSON *sample_rate;
    const cJSON *channels;

    if (audio == NULL)
    {
        return 0;
    }
    if (!cJSON_IsObject(audio))
    {
        return -1;
    }
    codec = cJSON_GetObjectItemCaseSensitive(audio, "codec");
    sample_rate = cJSON_GetObjectItemCaseSensitive(audio, "sample_rate");
    channels = cJSON_GetObjectItemCaseSensitive(audio, "channels");
    return cJSON_IsString(codec) && codec->valuestring != NULL &&
           strcmp(codec->valuestring, "opus") == 0 &&
           cJSON_IsNumber(sample_rate) && sample_rate->valueint == 16000 &&
           cJSON_IsNumber(channels) && channels->valueint == 1 ? 1 : -1;
}

static void ai_handle_command(const ai_command_message_t *message)
{
    cJSON *root;
    cJSON *method;
    cJSON *id;
    cJSON *result;
    cJSON *error;
    const cJSON *session_id;
    int input_audio_status;
    int output_audio_status;
    const char *method_text = NULL;

    if (message == NULL || message->cmdw != AI_CMD_WORD)
    {
        return;
    }
    root = cJSON_ParseWithLength(message->payload, message->length);
    if (root == NULL)
    {
        liot_trace("[AI] command JSON parse failed len=%u\r\n",
                   (unsigned int)message->length);
        return;
    }

    method = cJSON_GetObjectItemCaseSensitive(root, "method");
    id = cJSON_GetObjectItemCaseSensitive(root, "id");
    result = cJSON_GetObjectItemCaseSensitive(root, "result");
    error = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (cJSON_IsString(method) && method->valuestring != NULL)
    {
        method_text = method->valuestring;
    }

    if (method_text == NULL && cJSON_IsString(id) &&
        id->valuestring != NULL &&
        strcmp(id->valuestring, s_start_request_id) == 0)
    {
        s_start_response_done = true;
        if (error != NULL || !cJSON_IsObject(result))
        {
            const cJSON *message_item = cJSON_IsObject(error)
                ? cJSON_GetObjectItemCaseSensitive(error, "message") : NULL;
            liot_trace("[AI] start_session rejected%s%s\r\n",
                       cJSON_IsString(message_item) ? ": " : "",
                       cJSON_IsString(message_item) ? message_item->valuestring : "");
            s_start_response_error = AI_ERR_START_REJECTED;
        }
        else
        {
            session_id = cJSON_GetObjectItemCaseSensitive(result,
                                                           "session_id");
            input_audio_status = ai_json_audio_validate_optional(
                result, "input_audio");
            output_audio_status = ai_json_audio_validate_optional(
                result, "output_audio");
            if (!cJSON_IsString(session_id) ||
                session_id->valuestring == NULL ||
                session_id->valuestring[0] == '\0')
            {
                liot_trace("[AI] start_session response missing session_id\r\n");
                s_start_response_error = AI_ERR_START_REJECTED;
            }
            else if (input_audio_status < 0 || output_audio_status < 0)
            {
                liot_trace("[AI] server selected unsupported audio format\r\n");
                s_start_response_error = AI_ERR_AUDIO_FORMAT;
            }
            else
            {
                s_start_response_error = 0;
                liot_trace("[AI] start_session OK: Opus/16kHz/mono/20ms "
                           "format=%s\r\n",
                           (input_audio_status > 0 &&
                            output_audio_status > 0)
                               ? "server-confirmed" : "request-retained");
            }
        }
    }
    else if (method_text != NULL)
    {
        if (strcmp(method_text, "round_start") == 0)
        {
            s_server_speaking = true;
            s_last_audio_ms = liot_rtos_get_running_time();
        }
        else if (strcmp(method_text, "round_end") == 0)
        {
            /* Playback completion/idle timeout remains the final authority. */
            s_server_speaking = false;
        }
        else if (strcmp(method_text, "interrupt") == 0)
        {
            (void)demo_ai_audio_stop(&s_audio_lease);
            ai_discard_audio_queue();
            s_play_pending_samples = 0U;
            s_prebuffer_start_ms = 0U;
            demo_ai_opus_reset();
            s_server_speaking = false;
            liot_trace("[AI] server interrupt -> playback queue cleared\r\n");
        }
        else if (strcmp(method_text, "end_session") == 0)
        {
            s_remote_ended = true;
            liot_trace("[AI] server ended session\r\n");
        }
    }
    cJSON_Delete(root);
}

static void ai_process_commands(void)
{
    ai_command_message_t *message;
    uint8_t slot;

    while (s_command_queue != NULL &&
           liot_rtos_queue_wait(s_command_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        if (slot < AI_COMMAND_QUEUE_DEPTH)
        {
            message = &s_command_pool[slot];
            ai_handle_command(message);
            ai_command_slot_release(slot);
        }
    }
}

static int ai_send_start_session(void)
{
    char json[640];
    uint32_t deadline;
    int length;
    int ret;

    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }
    snprintf(s_start_request_id, sizeof(s_start_request_id),
             "nt26-ai-%u", (unsigned int)s_generation);
    length = snprintf(
        json, sizeof(json),
        "{\"jsonrpc\":\"2.0\",\"id\":\"%s\","
        "\"method\":\"start_session\",\"params\":{"
        "\"device_id\":\"%s\",\"role_id\":\"%s\","
        "\"input_audio\":{\"codec\":\"opus\",\"sample_rate\":16000,"
        "\"channels\":1},\"output_audio\":{\"codec\":\"opus\","
        "\"sample_rate\":16000,\"channels\":1}}}",
        s_start_request_id, s_access.device_id, s_access.role_id);
    if (length <= 0 || (size_t)length >= sizeof(json))
    {
        return AI_ERR_START_SEND;
    }

    s_start_response_done = false;
    s_start_response_error = 0;
    ret = demo_tirtc_send_command(DEMO_TIRTC_OWNER_AI,
                                  s_tirtc_session_generation,
                                  AI_CMD_WORD, json, (uint32_t)length);
    memset(json, 0, sizeof(json));
    if (ret < 0)
    {
        liot_trace("[AI] start_session send failed ret=%d (%s)\r\n",
                   ret, TiRtcGetErrorStr(ret));
        ret = ai_normalize_session_result(ret);
        if (ret == AI_ERR_CANCELLED)
        {
            return ret;
        }
        return AI_ERR_START_SEND;
    }
    s_start_submitted = true;

    liot_trace("[AI] start_session sent; waiting for negotiated format\r\n");
    deadline = liot_rtos_get_running_time() + AI_START_RESPONSE_TIMEOUT_MS;
    while (!s_start_response_done && ai_deadline_pending(deadline))
    {
        if (!ai_request_is_active())
        {
            return AI_ERR_CANCELLED;
        }
        if (s_remote_ended)
        {
            return AI_ERR_CANCELLED;
        }
        if (s_conn_error_pending || s_disconnected_pending)
        {
            ret = ai_connection_end_result();
            return (ret == 0) ? AI_ERR_CANCELLED : ret;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
        ai_process_commands();
    }
    if (!s_start_response_done)
    {
        return AI_ERR_START_TIMEOUT;
    }
    return s_start_response_error;
}

static int ai_play_downlink(void)
{
    ai_audio_message_t *message;
    uint8_t slot;
    uint32_t sample_count = s_play_pending_samples;
    int decoded;
    int ret;

    /* Do not overwrite s_play_pcm until the driver reports FINISH.  If the
     * driver applies backpressure, keep this decoded batch and retry it. */
    while (sample_count + AI_DECODE_MAX_SAMPLES <=
           AI_PLAY_BATCH_SAMPLES)
    {
        if (liot_rtos_queue_wait(s_audio_queue, &slot, sizeof(slot),
                                 LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
        {
            break;
        }
        if (slot >= AI_AUDIO_QUEUE_DEPTH)
        {
            s_rx_dropped++;
            continue;
        }
        message = &s_audio_pool[slot];
        if (message->media != TIRTC_AUDIO_OPUS ||
            message->flags != TIRTC_AUDIOSAMPLE_16K16B1C)
        {
            ai_audio_slot_release(slot);
            s_rx_dropped++;
            continue;
        }
        decoded = demo_ai_opus_decode(message->payload, message->length,
                                      s_decode_pcm,
                                      AI_DECODE_MAX_SAMPLES);
        if (decoded <= 0 ||
            sample_count + (uint32_t)decoded > AI_PLAY_BATCH_SAMPLES)
        {
            ai_audio_slot_release(slot);
            s_rx_dropped++;
            continue;
        }
        memcpy(&s_play_pcm[sample_count], s_decode_pcm,
               (size_t)decoded * sizeof(int16_t));
        sample_count += (uint32_t)decoded;
        s_rx_frames++;
        if (s_rx_frames == 1U)
        {
            liot_trace("[AI] first downlink Opus frame bytes=%u samples=%d\r\n",
                       (unsigned int)message->length, decoded);
        }
        ai_audio_slot_release(slot);
    }

    s_play_pending_samples = sample_count;
    if (sample_count == 0U)
    {
        return 0;
    }
    ret = demo_ai_audio_play(&s_audio_lease, s_play_pcm, sample_count);
    if (ret != 0)
    {
        liot_trace("[AI] playback backpressure ret=%d samples=%u\r\n",
                   ret, (unsigned int)sample_count);
        return 0;
    }
    s_play_pending_samples = 0U;
    s_last_audio_ms = liot_rtos_get_running_time();
    s_server_speaking = true;
    ai_set_state(DEMO_AI_CHAT_SPEAKING, 0);
    if (!s_first_play_reported)
    {
        s_first_play_reported = true;
        liot_trace("[AI-LATENCY] first_audio_submit=%u ms\r\n",
                   (unsigned int)ai_request_elapsed_ms());
    }
    return 1;
}

static int ai_send_uplink_20ms(void)
{
    TIRTCFRAMEINFO frame;
    size_t send_buffer_used;
    int encoded;
    int ret;

    /* Half duplex on this board: never start an I2S capture transaction while
     * the ES8311 playback path is active, otherwise speaker audio can feed
     * back into ASR and some codec revisions reject the overlapping request. */
    if (s_server_speaking || !demo_ai_audio_play_done(&s_audio_lease) ||
        !ai_request_is_active())
    {
        liot_rtos_task_sleep_ms(DEMO_AI_AUDIO_FRAME_MS);
        return 0;
    }
    ret = demo_tirtc_get_send_buffer_used(DEMO_TIRTC_OWNER_AI,
                                          s_tirtc_session_generation,
                                          &send_buffer_used);
    if (ret != 0)
    {
        return ai_normalize_session_result(ret);
    }
    if (send_buffer_used > AI_SEND_HIGH_WATER_BYTES)
    {
        s_tx_dropped++;
        liot_rtos_task_sleep_ms(DEMO_AI_AUDIO_FRAME_MS);
        return 0;
    }
    ret = demo_ai_audio_record_20ms(&s_audio_lease, s_capture_pcm);
    if (ret != 0)
    {
        liot_trace("[AI] capture failed ret=%d\r\n", ret);
        return ret;
    }
    encoded = demo_ai_opus_encode_20ms(s_capture_pcm, s_uplink_opus,
                                       sizeof(s_uplink_opus));
    if (encoded <= 0)
    {
        liot_trace("[AI] Opus encode failed ret=%d\r\n", encoded);
        return encoded;
    }

    memset(&frame, 0, sizeof(frame));
    frame.stream_id = AI_STREAM_ID;
    frame.media = TIRTC_AUDIO_OPUS;
    frame.flags = TIRTC_AUDIOSAMPLE_16K16B1C;
    /* Host monotonic milliseconds preserve real gaps when frames are dropped. */
    frame.ts = liot_rtos_get_running_time();
    frame.length = (uint32_t)encoded;
    ret = demo_tirtc_send_audio(DEMO_TIRTC_OWNER_AI,
                                s_tirtc_session_generation,
                                &frame, s_uplink_opus);
    if (ret == TIRTC_E_BUSY)
    {
        s_tx_dropped++;
        return 0;
    }
    if (ret < 0)
    {
        liot_trace("[AI] uplink send failed ret=%d (%s)\r\n",
                   ret, TiRtcGetErrorStr(ret));
        return ai_normalize_session_result(ret);
    }
    s_tx_frames++;
    if (s_tx_frames == 1U)
    {
        liot_trace("[AI] first uplink Opus frame bytes=%d\r\n", encoded);
    }
    return 0;
}

static int ai_active_loop(void)
{
    uint32_t audio_count = 0U;
    uint32_t now;
    int ret;

    ai_set_state(DEMO_AI_CHAT_LISTENING, 0);
    while (!s_remote_ended)
    {
        if (!ai_request_is_active())
        {
            return AI_ERR_CANCELLED;
        }
        ai_process_commands();
        if (s_remote_ended)
        {
            return AI_ERR_CANCELLED;
        }
        ai_disconnect_deferred();
        if (s_conn_error_pending || s_disconnected_pending)
        {
            liot_trace("[AI] connection ended error=%d disconnected=%d\r\n",
                       s_conn_error, s_disconnected_pending ? 1 : 0);
            return ai_connection_end_result();
        }
        if (s_levels_dirty)
        {
            (void)demo_ai_audio_set_levels(&s_audio_lease,
                                           s_speaker_level, s_mic_level);
            s_levels_dirty = false;
        }

        (void)liot_rtos_queue_get_cnt(s_audio_queue, &audio_count);
        now = liot_rtos_get_running_time();
        if (s_play_pending_samples > 0U)
        {
            (void)ai_play_downlink();
            (void)liot_rtos_semaphore_wait(s_event_sem, AI_ACTIVE_POLL_MS);
            continue;
        }
        if (audio_count > 0U)
        {
            /* Liot_AudioPlay copies each block into its internal queue.  Feed
             * that queue while it is active for gap-free streaming.  On a
             * fresh/underrun start, collect roughly 60 ms first (or wait at
             * most 80 ms) to absorb normal 4G packet jitter. */
            if (demo_ai_audio_play_done(&s_audio_lease))
            {
                if (s_prebuffer_start_ms == 0U)
                {
                    s_prebuffer_start_ms = now;
                }
                if (audio_count < AI_PREBUFFER_PACKETS &&
                    (int32_t)(now - s_prebuffer_start_ms) <
                        (int32_t)AI_PREBUFFER_TIMEOUT_MS)
                {
                    ai_set_state(DEMO_AI_CHAT_SPEAKING, 0);
                    (void)liot_rtos_semaphore_wait(s_event_sem,
                                                   AI_ACTIVE_POLL_MS);
                    continue;
                }
            }
            s_prebuffer_start_ms = 0U;
            (void)ai_play_downlink();
            continue;
        }
        s_prebuffer_start_ms = 0U;

        if (s_server_speaking || !demo_ai_audio_play_done(&s_audio_lease))
        {
            ai_set_state(DEMO_AI_CHAT_SPEAKING, 0);
            if (demo_ai_audio_play_done(&s_audio_lease) &&
                (int32_t)(now - s_last_audio_ms) >=
                (int32_t)AI_SPEECH_IDLE_MS)
            {
                s_server_speaking = false;
                ai_set_state(DEMO_AI_CHAT_LISTENING, 0);
            }
            else
            {
                (void)liot_rtos_semaphore_wait(s_event_sem,
                                               AI_ACTIVE_POLL_MS);
                continue;
            }
        }

        ret = ai_send_uplink_20ms();
        if (ret < 0)
        {
            return ret;
        }
    }
    return 0;
}

static int ai_send_end_session(void)
{
    static const char end_json[] =
        "{\"jsonrpc\":\"2.0\",\"method\":\"end_session\"}";
    int ret;

    if (s_conn != NULL && s_start_submitted && !s_remote_ended &&
        !s_conn_error_pending && !s_disconnected_pending)
    {
        ret = demo_tirtc_send_command(DEMO_TIRTC_OWNER_AI,
                                      s_tirtc_session_generation,
                                      AI_CMD_WORD, end_json,
                                      sizeof(end_json) - 1U);
        liot_trace("[AI-CLEANUP] end_session ret=%d\r\n", ret);
        if (ret >= 0)
        {
            /* Best-effort grace period: Disconnect may otherwise discard the
             * asynchronous JSON-RPC notification immediately after enqueue. */
            liot_rtos_task_sleep_ms(AI_END_FLUSH_TIMEOUT_MS);
        }
        return ret;
    }
    return 0;
}

static bool ai_disconnect_and_wait(tirtc_conn_t connection,
                                   bool already_disconnected)
{
    uint32_t settle_deadline;
    bool complete = false;
    int ret;

    if (connection == NULL || already_disconnected)
    {
        return true;
    }

    liot_rtos_enter_critical();
    s_cleanup_conn = connection;
    s_cleanup_disconnect_done = false;
    liot_rtos_exit_critical();

    ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_AI,
                                s_tirtc_session_generation);
    liot_trace("[AI-CLEANUP] disconnect ret=%d\r\n", ret);
    if (ret == 0)
    {
        /* tiRTC.h says ordinary applications must regard hconn as invalid
         * once TiRtcDisconnect returns and need not receive on_disconnected.
         * Wait briefly for diagnostics, but never hold HOME/LIVE disabled
         * forever when that optional callback is absent. */
        settle_deadline = liot_rtos_get_running_time() +
                          AI_DISCONNECT_SETTLE_MS;
        while (ai_deadline_pending(settle_deadline))
        {
            liot_rtos_enter_critical();
            complete = s_cleanup_disconnect_done;
            liot_rtos_exit_critical();
            if (complete)
            {
                break;
            }
            (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
        }
        if (!complete)
        {
            liot_trace("[AI-CLEANUP] disconnect callback not observed in %u "
                       "ms; releasing app session per SDK contract\r\n",
                       (unsigned int)AI_DISCONNECT_SETTLE_MS);
        }
    }
    else
    {
        complete = (ret == TIRTC_E_INVALID_HANDLE);
    }

    liot_rtos_enter_critical();
    s_cleanup_conn = NULL;
    s_cleanup_disconnect_done = false;
    liot_rtos_exit_critical();
    liot_trace("[AI-CLEANUP] disconnect settled=%u\r\n",
               complete ? 1U : 0U);
    return complete;
}

static void ai_session_cleanup(void)
{
    tirtc_conn_t connection;
    tirtc_conn_t whip_connection;
    bool already_disconnected;
    bool audio_owned;
    bool tirtc_owned;
    int audio_ret = 0;
    int disconnect_ret;
    int release_ret = 0;

    ai_set_state(DEMO_AI_CHAT_STOPPING, 0);
    /* A delayed WHIP callback now belongs to an obsolete attempt. */
    liot_rtos_enter_critical();
    s_active_whip_context = NULL;
    connection = s_conn;
    whip_connection = (tirtc_conn_t)s_whip_conn;
    already_disconnected = s_disconnected_pending;
    audio_owned = s_audio_owned;
    tirtc_owned = s_tirtc_owned;
    liot_rtos_exit_critical();
    if (audio_owned)
    {
        audio_ret = demo_ai_audio_stop(&s_audio_lease);
    }
    liot_trace("[AI-CLEANUP] audio stop ret=%d\r\n", audio_ret);
    (void)ai_send_end_session();
    liot_rtos_enter_critical();
    s_conn = NULL;
    s_whip_conn = NULL;
    liot_rtos_exit_critical();
    (void)ai_wait_callback_producers();
    ai_discard_audio_queue();
    ai_discard_command_queue();
    if (connection != NULL)
    {
        (void)ai_disconnect_and_wait(connection, already_disconnected);
    }
    if (whip_connection != NULL && whip_connection != connection)
    {
        (void)ai_disconnect_and_wait(whip_connection, false);
    }
    if (connection == NULL && whip_connection == NULL && tirtc_owned)
    {
        /* There may still be an asynchronous WHIP request without a public
         * handle.  Invalidate it in the adapter; any late success is then
         * retained and closed by the runtime before this session can release. */
        disconnect_ret = demo_tirtc_disconnect(DEMO_TIRTC_OWNER_AI,
                                                s_tirtc_session_generation);
        liot_trace("[AI-CLEANUP] cancel pending connect ret=%d\r\n",
                   disconnect_ret);
    }
    ai_disconnect_deferred();
    if (audio_owned)
    {
        release_ret = demo_ai_audio_release(&s_audio_lease);
        if (release_ret == 0)
        {
            liot_rtos_enter_critical();
            s_audio_owned = false;
            liot_rtos_exit_critical();
        }
        else
        {
            liot_trace("[AI-CLEANUP] audio release failed ret=%d\r\n",
                       release_ret);
        }
    }
    demo_ai_opus_reset();
    ai_access_clear();
    s_server_speaking = false;
    s_remote_ended = false;
    s_play_pending_samples = 0U;
    s_prebuffer_start_ms = 0U;
    s_whip_done = false;
    s_conn_error_pending = false;
    s_conn_error = 0;
    s_disconnected_pending = false;
    s_start_submitted = false;
    s_start_response_done = false;
    s_start_response_error = 0;
    (void)ai_release_tirtc_session();
}

static int ai_run_session(void)
{
    ai_whip_context_t *whip_context;
    uint32_t deadline;
    uint32_t settle_deadline;
    uint32_t managed_generation;
    int32_t settle_remaining_ms;
    bool used_prefetch;
    int ret;

    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }
    liot_rtos_enter_critical();
    s_generation++;
    if (s_generation == 0U)
    {
        s_generation++;
    }
    liot_rtos_exit_critical();
    s_rx_dropped = 0U;
    s_tx_dropped = 0U;
    s_rx_frames = 0U;
    s_tx_frames = 0U;
    s_first_play_reported = false;
    s_whip_done = false;
    s_whip_error = 0;
    s_whip_conn = NULL;
    s_conn_error_pending = false;
    s_conn_error = 0;
    s_disconnected_pending = false;
    s_remote_ended = false;
    s_start_submitted = false;
    s_server_speaking = false;
    s_play_pending_samples = 0U;
    s_prebuffer_start_ms = 0U;
    ai_discard_audio_queue();
    ai_discard_command_queue();

    /* The system runtime is device-authenticated once after provisioning.
     * Entering AI only waits for that runtime and creates one short-lived
     * WHIP session, matching the official minimal adapter lifecycle. */
    ai_set_state(DEMO_AI_CHAT_START_SDK, 0);
    ret = demo_tirtc_wait_ready(AI_CONNECT_TIMEOUT_MS);
    if (ret != 0)
    {
        liot_trace("[AI] TiRTC runtime unavailable ret=%d\r\n", ret);
        return ret;
    }
    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }

    ai_set_state(DEMO_AI_CHAT_GET_TOKEN, 0);
    used_prefetch = ai_access_take_prepared();
    if (!used_prefetch)
    {
        ret = ai_access_fetch();
        if (ret != 0)
        {
            if (ret != AI_ERR_TOKEN_TOO_LONG)
            {
                liot_trace("[AI] GET /v1/ai/token failed ret=%d\r\n", ret);
            }
            return ret == AI_ERR_TOKEN_TOO_LONG ? ret : AI_ERR_GET_TOKEN;
        }
    }
    liot_trace("[AI] short-lived credentials ready peer_len=%u role_len=%u\r\n",
               (unsigned int)strlen(s_access.peer_id),
               (unsigned int)strlen(s_access.role_id));
    liot_trace("[AI-LATENCY] token_ready=%u ms source=%s\r\n",
               (unsigned int)ai_request_elapsed_ms(),
               used_prefetch ? "prefetch" : "network");
    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }
    ret = demo_tirtc_session_claim(DEMO_TIRTC_OWNER_AI,
                                   &managed_generation);
    if (ret != 0)
    {
        liot_trace("[AI] managed session claim failed ret=%d\r\n", ret);
        return ai_normalize_session_result(ret);
    }
    liot_rtos_enter_critical();
    s_tirtc_session_generation = managed_generation;
    s_tirtc_owned = true;
    s_tirtc_release_error_logged = false;
    liot_rtos_exit_critical();
    ai_set_state(DEMO_AI_CHAT_CONNECTING, 0);
    whip_context = ai_whip_context_acquire();
    if (whip_context == NULL)
    {
        liot_trace("[AI] no free WHIP attempt context; previous callback pending\r\n");
        return AI_ERR_WHIP_SUBMIT;
    }
    liot_rtos_enter_critical();
    s_active_whip_context = whip_context;
    liot_rtos_exit_critical();
    ret = ai_whip_submit(whip_context);
    if (ret != 0)
    {
        liot_rtos_enter_critical();
        whip_context->pending = false;
        s_active_whip_context = NULL;
        liot_rtos_exit_critical();
        liot_trace("[AI] WHIP worker submit failed ret=%d\r\n", ret);
        /* Preserve the SDK error on the OLED and serial log.  -1008 is only
         * meaningful when the application cannot submit the attempt itself;
         * replacing an SDK error (for example -40012) hides the real fault. */
        return ret;
    }

    deadline = liot_rtos_get_running_time() + AI_CONNECT_TIMEOUT_MS;
    while (!s_whip_done && ai_deadline_pending(deadline))
    {
        if (!ai_request_is_active())
        {
            return AI_ERR_CANCELLED;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
    }
    if (!s_whip_done)
    {
        return AI_ERR_WHIP_TIMEOUT;
    }
    if (s_whip_error != 0 || s_whip_conn == NULL)
    {
        liot_trace("[AI] WHIP connect failed ret=%d (%s)\r\n",
                   s_whip_error, TiRtcGetErrorStr(s_whip_error));
        return (s_whip_error != 0) ?
            ai_normalize_session_result(s_whip_error) : AI_ERR_CONNECTION;
    }
    s_conn = (tirtc_conn_t)s_whip_conn;
    liot_trace("[AI-LATENCY] whip_ready=%u ms\r\n",
               (unsigned int)ai_request_elapsed_ms());
    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }
    /* Official minimal examples keep 300 ms between WHIP success and
     * start_session so the KCP data channel can settle.  Prepare ES8311 and
     * Opus inside that same window, turning two serial delays into one. */
    settle_deadline = liot_rtos_get_running_time() + AI_KCP_SETTLE_MS;
    liot_trace("[AI] WHIP connected; preparing media during KCP settle\r\n");
    ret = demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_AI, &s_audio_lease);
    if (ret != 0)
    {
        liot_trace("[AI] audio acquire failed ret=%d\r\n", ret);
        return AI_ERR_AUDIO_INIT;
    }
    liot_rtos_enter_critical();
    s_audio_owned = true;
    liot_rtos_exit_critical();
    if (demo_ai_audio_prepare_session(&s_audio_lease,
                                      s_speaker_level,
                                      s_mic_level) != 0)
    {
        return AI_ERR_AUDIO_INIT;
    }
    if (demo_ai_opus_init() != 0)
    {
        return AI_ERR_OPUS_INIT;
    }
    s_levels_dirty = false;
    demo_ai_opus_reset();

    settle_remaining_ms = (int32_t)(settle_deadline -
                                    liot_rtos_get_running_time());
    if (settle_remaining_ms > 0)
    {
        liot_rtos_task_sleep_ms((uint32_t)settle_remaining_ms);
    }
    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }

    ai_set_state(DEMO_AI_CHAT_NEGOTIATING, 0);
    ret = ai_send_start_session();
    if (ret != 0)
    {
        return ret;
    }
    liot_trace("[AI-LATENCY] session_ready=%u ms\r\n",
               (unsigned int)ai_request_elapsed_ms());
    if (!ai_request_is_active())
    {
        return AI_ERR_CANCELLED;
    }

    /* Official minimal-system AI flow subscribes only after the matching
     * start_session response has accepted the negotiated format. */
    ret = demo_tirtc_subscribe_audio(DEMO_TIRTC_OWNER_AI,
                                     s_tirtc_session_generation,
                                     AI_STREAM_ID);
    liot_trace("[AI] subscribe downlink stream=%u ret=%d\r\n",
               (unsigned int)AI_STREAM_ID, ret);
    if (ret < 0)
    {
        ret = ai_normalize_session_result(ret);
        if (ret == AI_ERR_CANCELLED)
        {
            return ret;
        }
        return AI_ERR_DOWNLINK_SUBSCRIBE;
    }
    /* WHIP input pointers are no longer needed after connect and signaling. */
    memset(s_access.peer_id, 0, sizeof(s_access.peer_id));
    memset(s_access.token, 0, sizeof(s_access.token));
    ret = ai_active_loop();
    liot_trace("[AI] session leave ret=%d rx_drop=%u tx_drop=%u\r\n",
               ret, (unsigned int)s_rx_dropped,
               (unsigned int)s_tx_dropped);
    return ret;
}

uint32_t demo_ai_chat_start(void)
{
    uint32_t request_id;
    uint32_t started_ms = liot_rtos_get_running_time();

    liot_rtos_enter_critical();
    s_request_started_ms = started_ms;
    s_first_play_reported = false;
    s_want_active = true;
    s_control_sequence++;
    if (s_control_sequence == 0U)
    {
        s_control_sequence++;
    }
    request_id = s_control_sequence;
    liot_rtos_exit_critical();
    liot_trace("[AI-LATENCY] request=%u start=0 ms\r\n",
               (unsigned int)request_id);
    ai_signal();
    return request_id;
}

void demo_ai_chat_prepare(void)
{
    /* The UI only posts intent.  Fetching and cache ownership remain in the
     * existing AI worker, so no HTTP call or credential write occurs here. */
    liot_rtos_enter_critical();
    s_prepare_requested = true;
    liot_rtos_exit_critical();
    ai_signal();
}

void demo_ai_chat_mark_unavailable(void)
{
    ai_set_state(DEMO_AI_CHAT_ERROR, AI_ERR_TASK_CREATE);
}

void demo_ai_chat_stop(void)
{
    liot_rtos_enter_critical();
    s_want_active = false;
    s_control_sequence++;
    if (s_control_sequence == 0U)
    {
        s_control_sequence++;
    }
    liot_rtos_exit_critical();
    ai_signal();
}

bool demo_ai_chat_is_idle(void)
{
    bool idle;
    uint32_t i;

    liot_rtos_enter_critical();
    idle = !s_want_active && s_running_request_id == 0U &&
           s_idle_ack_sequence == s_control_sequence &&
           s_active_whip_context == NULL && s_conn == NULL &&
           s_whip_conn == NULL && s_cleanup_conn == NULL &&
           s_unwanted_pending == 0U && !s_unwanted_overflow &&
           !s_audio_owned && !s_tirtc_owned;
    for (i = 0U; idle && i < AI_WHIP_CONTEXT_COUNT; i++)
    {
        if (s_whip_contexts[i].pending)
        {
            idle = false;
        }
    }
    liot_rtos_exit_critical();
    return idle;
}

void demo_ai_chat_get_snapshot(demo_ai_chat_snapshot_t *out)
{
    if (out != NULL)
    {
        liot_rtos_enter_critical();
        out->state = s_state;
        out->error = s_error;
        out->rx_dropped = s_rx_dropped;
        out->tx_dropped = s_tx_dropped;
        out->completed_request_id = s_completed_request_id;
        out->completed_result = s_completed_result;
        liot_rtos_exit_critical();
    }
}

void demo_ai_chat_set_audio_levels(uint8_t speaker_level, uint8_t mic_level)
{
    if (speaker_level < 1U)
    {
        speaker_level = 1U;
    }
    else if (speaker_level > 10U)
    {
        speaker_level = 10U;
    }
    if (mic_level < 1U)
    {
        mic_level = 1U;
    }
    else if (mic_level > 10U)
    {
        mic_level = 10U;
    }
    s_speaker_level = speaker_level;
    s_mic_level = mic_level;
    s_levels_dirty = true;
    ai_signal();
}

void demo_ai_chat_task(void *argv)
{
    uint32_t handled_sequence = 0U;
    uint32_t requested_sequence;
    bool want_active;
    bool release_pending;
    bool prepare_requested;
    int ret;

    (void)argv;
    if (liot_rtos_semaphore_create(&s_event_sem, 0U) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_audio_queue, sizeof(uint8_t),
                               AI_AUDIO_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_command_queue, sizeof(uint8_t),
                               AI_COMMAND_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_unwanted_queue, sizeof(tirtc_conn_t),
                               AI_UNWANTED_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }

    demo_tirtc_set_listener(&s_tirtc_listener);
    liot_trace("[AI] manager ready: shared device TiRTC runtime; "
               "PCM S16LE/16k/mono -> Opus 20ms/24kbps (half duplex) "
               "free_heap=%u\r\n",
               (unsigned int)liot_xPortGetFreeHeapSize());
    ai_set_state(DEMO_AI_CHAT_IDLE, 0);
    while (1)
    {
        ai_disconnect_deferred();

        liot_rtos_enter_critical();
        release_pending = s_tirtc_owned && s_running_request_id == 0U;
        requested_sequence = s_control_sequence;
        want_active = s_want_active;
        prepare_requested = s_prepare_requested;
        liot_rtos_exit_critical();
        if (release_pending && !ai_release_tirtc_session())
        {
            (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
            continue;
        }
        if (prepare_requested)
        {
            liot_rtos_enter_critical();
            s_prepare_requested = false;
            liot_rtos_exit_critical();
        }
        if (!want_active || requested_sequence == handled_sequence)
        {
            if (!want_active)
            {
                /* Acknowledge the exact control generation only after any
                 * active session has returned through ai_session_cleanup().
                 * The HOME LIVE policy uses this to avoid sharing ES8311
                 * with a still-stopping AI session. */
                liot_rtos_enter_critical();
                if (!s_want_active && s_running_request_id == 0U)
                {
                    s_idle_ack_sequence = s_control_sequence;
                }
                liot_rtos_exit_critical();
                if (prepare_requested)
                {
                    ai_access_prepare();
                }
                else if (s_access_prepared)
                {
                    /* Expiry only clears secrets; it never refreshes them. */
                    (void)ai_access_prepared_is_valid(
                        liot_rtos_get_running_time());
                }
            }
            if (!want_active && s_state != DEMO_AI_CHAT_IDLE &&
                s_state != DEMO_AI_CHAT_ERROR)
            {
                ai_set_state(DEMO_AI_CHAT_IDLE, 0);
            }
            (void)liot_rtos_semaphore_wait(s_event_sem, 500U);
            continue;
        }

        handled_sequence = requested_sequence;
        liot_rtos_enter_critical();
        s_running_request_id = handled_sequence;
        liot_rtos_exit_critical();
        ret = ai_run_session();
        ret = ai_normalize_session_result(ret);
        ai_session_cleanup();
        if (ret == AI_ERR_CANCELLED)
        {
            ret = 0;
        }
        if (ret == 0)
        {
            ai_set_state(DEMO_AI_CHAT_IDLE, 0);
        }
        else
        {
            ai_set_state(DEMO_AI_CHAT_ERROR, ret);
        }
        liot_rtos_enter_critical();
        if (s_running_request_id == handled_sequence)
        {
            s_running_request_id = 0U;
        }
        s_completed_request_id = handled_sequence;
        s_completed_result = ret;
        if (s_control_sequence == handled_sequence)
        {
            s_want_active = false;
        }
        liot_rtos_exit_critical();
        liot_trace("[AI] request=%u complete result=%d resources released\r\n",
                   (unsigned int)handled_sequence, ret);
    }

init_failed:
    if (s_unwanted_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_unwanted_queue);
        s_unwanted_queue = NULL;
    }
    if (s_command_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_command_queue);
        s_command_queue = NULL;
    }
    if (s_audio_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_audio_queue);
        s_audio_queue = NULL;
    }
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_delete(s_event_sem);
        s_event_sem = NULL;
    }
    ai_set_state(DEMO_AI_CHAT_ERROR, AI_ERR_QUEUE_INIT);
    liot_trace("[AI] queue/semaphore init failed\r\n");
    liot_rtos_task_delete(NULL);
}
