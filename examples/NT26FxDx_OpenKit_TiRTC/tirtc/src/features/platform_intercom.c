/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Platform H5 LIVE talkback for NT26F6D0/F6D_A.
 *
 * Platform contract:
 *   device -> platform: stream 10, G.711 A-law, 8 kHz mono
 *   platform -> device: stream 14 (current H5 talkback) or stream 10
 *                       (device/call profile),
 *                       G.711 A-law, 8/16 kHz mono
 *   device video: stream 11 is acknowledged for VIEW compatibility; this
 *                 audio-only product intentionally sends no video frames.
 *
 * TiRTC callbacks only update bounded state or copy data into a fixed pool.
 * Audio, codec, send and disconnect operations stay in this owner task.
 */

#include "platform_intercom.h"

#include <stdint.h>
#include <string.h>

#include "audio_device.h"
#ifdef HWDEMO_AI_CHAT_EN
#include "ai_chat.h"
#endif
#ifdef HWDEMO_DEV_CHAT_EN
#include "device_call.h"
#endif
#include "g711_codec.h"
#include "tirtc_runtime.h"
#ifdef HWDEMO_WECHAT_EN
#include "wechat_call.h"
#endif
#include "liot_log.h"
#include "liot_os.h"
#include "tirtc/tiRTC.h"

#define LIVE_UPLINK_STREAM_ID          10U
#define LIVE_DOWNLINK_GITHUB_STREAM_ID 10U
#define LIVE_DOWNLINK_STREAM_ID        14U
#define LIVE_VIDEO_STREAM_ID           11U
#define LIVE_CMD_HANGUP                 0x1104U
#define LIVE_CMD_REQ_AUDIO              0x1106U
#define LIVE_CMD_SET_SEND_AUDIO         0x1108U
#define LIVE_CMD_RESPONSE_BIT           0x8000U
#define LIVE_RX_QUEUE_DEPTH            12U
#define LIVE_RX_PAYLOAD_MAX            640U
#define LIVE_PLAY_PCM_MAX_SAMPLES      (LIVE_RX_PAYLOAD_MAX * 2U)
#define LIVE_UPLINK_8K_SAMPLES         (DEMO_AI_AUDIO_FRAME_SAMPLES / 2U)
#define LIVE_PREBUFFER_PACKETS          3U
#define LIVE_PREBUFFER_TIMEOUT_MS      80U
#define LIVE_DISCONNECT_WAIT_MS       5000U
#define LIVE_RX_PRODUCER_TIMEOUT_MS    1000U
#define LIVE_IDLE_WAIT_MS              500U
#define LIVE_ACTIVE_WAIT_MS            10U

typedef enum
{
    LIVE_STOP_NONE = 0,
    LIVE_STOP_HOME_LEFT,
    LIVE_STOP_REMOTE_DISCONNECTED,
    LIVE_STOP_CONNECTION_ERROR,
    LIVE_STOP_AUDIO_ERROR,
    LIVE_STOP_SEND_ERROR,
} live_stop_reason_e;

typedef struct
{
    uint32_t generation;
    uint32_t timestamp_ms;
    uint16_t length;
    uint8_t flags;
    uint8_t payload[LIVE_RX_PAYLOAD_MAX];
} live_rx_frame_t;

static liot_sem_t s_event_sem;
static liot_queue_t s_rx_queue;

static live_rx_frame_t s_rx_pool[LIVE_RX_QUEUE_DEPTH];
static bool s_rx_pool_used[LIVE_RX_QUEUE_DEPTH];
static __attribute__((aligned(16))) int16_t
    s_capture_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint8_t s_uplink_alaw[LIVE_UPLINK_8K_SAMPLES];
static __attribute__((aligned(16))) int16_t
    s_play_pcm[LIVE_PLAY_PCM_MAX_SAMPLES];

static volatile bool s_home_allowed;
static volatile bool s_active;
static volatile bool s_busy;
static volatile bool s_stop_requested;
static volatile bool s_remote_disconnected;
static volatile bool s_uplink_subscribed;
static volatile bool s_remote_audio_enabled;
static volatile bool s_bad_rx_logged;
static volatile bool s_manager_ready;
static volatile bool s_home_policy_logged;
static volatile live_stop_reason_e s_stop_reason;
static volatile int s_connection_error;
static volatile tirtc_conn_t s_claimed_conn;
static volatile tirtc_conn_t s_conn;
static volatile tirtc_conn_t s_closing_conn;
static volatile bool s_disconnect_complete;
static volatile uint32_t s_generation;
static volatile uint32_t s_rx_queued;
static volatile uint32_t s_rx_producers;
static volatile uint32_t s_rx_dropped;
static volatile uint32_t s_tx_dropped;
static volatile uint32_t s_rx_frames;
static volatile uint32_t s_tx_frames;
static uint32_t s_prebuffer_start_ms;
static volatile uint8_t s_speaker_level =
    DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static volatile uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;
static volatile bool s_levels_changed = true;
static demo_ai_audio_lease_t s_audio_lease = DEMO_AI_AUDIO_LEASE_INIT;
static volatile bool s_audio_owned;
static volatile bool s_tirtc_owned;
static volatile uint32_t s_tirtc_session_generation;
static bool s_tirtc_release_error_logged;

static void live_signal(void)
{
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_release(s_event_sem);
    }
}

static bool live_matches_connection_locked(tirtc_conn_t hconn)
{
    return hconn != NULL &&
           (hconn == (tirtc_conn_t)s_conn ||
            hconn == (tirtc_conn_t)s_claimed_conn);
}

static int live_rx_slot_acquire(tirtc_conn_t hconn, uint32_t *generation)
{
    uint32_t i;
    int slot = -1;

    liot_rtos_enter_critical();
    if (live_matches_connection_locked(hconn))
    {
        for (i = 0U; i < LIVE_RX_QUEUE_DEPTH; ++i)
        {
            if (!s_rx_pool_used[i])
            {
                s_rx_pool_used[i] = true;
                *generation = s_generation;
                ++s_rx_producers;
                slot = (int)i;
                break;
            }
        }
    }
    liot_rtos_exit_critical();
    return slot;
}

static void live_rx_producer_done(void)
{
    bool idle = false;

    liot_rtos_enter_critical();
    if (s_rx_producers > 0U)
    {
        --s_rx_producers;
        idle = s_rx_producers == 0U;
    }
    liot_rtos_exit_critical();
    if (idle)
    {
        live_signal();
    }
}

static bool live_wait_rx_producers(void)
{
    uint32_t deadline = liot_rtos_get_running_time() +
                        LIVE_RX_PRODUCER_TIMEOUT_MS;
    uint32_t producers;

    do
    {
        liot_rtos_enter_critical();
        producers = s_rx_producers;
        liot_rtos_exit_critical();
        if (producers == 0U)
        {
            return true;
        }
        (void)liot_rtos_semaphore_wait(s_event_sem, 10U);
    } while ((int32_t)(deadline - liot_rtos_get_running_time()) > 0);

    liot_trace("[LIVE] audio callback quiesce timeout producers=%u\r\n",
               (unsigned int)producers);
    return false;
}

static void live_rx_slot_release(uint8_t slot)
{
    if (slot >= LIVE_RX_QUEUE_DEPTH)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_rx_pool_used[slot] = false;
    liot_rtos_exit_critical();
}

static void live_rx_queued_decrement(void)
{
    liot_rtos_enter_critical();
    if (s_rx_queued != 0U)
    {
        --s_rx_queued;
    }
    liot_rtos_exit_critical();
}

static void live_note_rx_drop(void)
{
    liot_rtos_enter_critical();
    ++s_rx_dropped;
    liot_rtos_exit_critical();
}

static void live_discard_rx_queue(void)
{
    uint8_t slot;
    bool producers_idle;

    while (s_rx_queue != NULL &&
           liot_rtos_queue_wait(s_rx_queue, &slot, sizeof(slot),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        live_rx_queued_decrement();
        live_rx_slot_release(slot);
    }
    liot_rtos_enter_critical();
    producers_idle = s_rx_producers == 0U;
    if (producers_idle)
    {
        memset(s_rx_pool_used, 0, sizeof(s_rx_pool_used));
        s_rx_queued = 0U;
    }
    liot_rtos_exit_critical();
}

static bool live_release_tirtc_session(void)
{
    uint32_t session_generation;
    bool owned;
    bool wake = false;
    int ret;

    liot_rtos_enter_critical();
    owned = s_tirtc_owned;
    session_generation = s_tirtc_session_generation;
    liot_rtos_exit_critical();
    if (!owned)
    {
        return true;
    }
    ret = demo_tirtc_session_release(DEMO_TIRTC_OWNER_LIVE,
                                     session_generation);
    if (ret == 0)
    {
        liot_rtos_enter_critical();
        if (s_tirtc_session_generation == session_generation)
        {
            s_tirtc_owned = false;
            s_tirtc_session_generation = 0U;
            s_tirtc_release_error_logged = false;
            if (!s_audio_owned && !s_active && s_conn == NULL &&
                s_claimed_conn == NULL && s_closing_conn == NULL)
            {
                s_busy = false;
                wake = true;
            }
        }
        liot_rtos_exit_critical();
        if (wake)
        {
            live_signal();
        }
        return true;
    }
    if (ret != TIRTC_E_BUSY && !s_tirtc_release_error_logged)
    {
        s_tirtc_release_error_logged = true;
        liot_trace("[LIVE] managed session release failed ret=%d generation=%u\r\n",
                   ret, (unsigned int)session_generation);
        demo_tirtc_require_restart(ret);
    }
    return false;
}

static bool live_other_features_idle(void)
{
    bool idle = true;

#ifdef HWDEMO_AI_CHAT_EN
    idle = idle && demo_ai_chat_is_idle();
#endif
#ifdef HWDEMO_WECHAT_EN
    idle = idle && demo_wechat_is_idle();
#endif
#ifdef HWDEMO_DEV_CHAT_EN
    idle = idle && demo_dev_chat_is_idle();
#endif
    return idle;
}

static int live_on_conn_accepted(tirtc_conn_t hconn)
{
    int accepted = -1;
    int adopt_ret = TIRTC_E_BUSY;
    uint32_t managed_generation = 0U;
    bool home_allowed;
    bool busy;
    bool claimed;
    bool connected;
    bool stopping;
    bool eligible;

    if (hconn == NULL)
    {
        return -1;
    }
    if (!demo_tirtc_admission_try_enter())
    {
        liot_trace("[LIVE] incoming rejected: admission busy\r\n");
        return -1;
    }
    if (!live_other_features_idle())
    {
        demo_tirtc_admission_leave();
        liot_trace("[LIVE] incoming rejected: feature owner busy\r\n");
        return -1;
    }

    liot_rtos_enter_critical();
    home_allowed = s_home_allowed;
    busy = s_busy;
    claimed = s_claimed_conn != NULL;
    connected = s_conn != NULL;
    stopping = s_stop_requested;
    eligible = s_home_allowed && !s_busy && s_claimed_conn == NULL &&
               s_conn == NULL && !s_stop_requested;
    liot_rtos_exit_critical();

    if (eligible)
    {
        /* Adopt the SDK handle before publishing it to the LIVE worker.  From
         * this point every callback and operation is generation-routed by the
         * single managed adapter. */
        adopt_ret = demo_tirtc_adopt_incoming(DEMO_TIRTC_OWNER_LIVE, hconn,
                                              &managed_generation);
    }
    if (adopt_ret == 0)
    {
        liot_rtos_enter_critical();
        s_tirtc_owned = true;
        s_tirtc_session_generation = managed_generation;
        s_tirtc_release_error_logged = false;
        ++s_generation;
        if (s_generation == 0U)
        {
            ++s_generation;
        }
        s_claimed_conn = hconn;
        s_busy = true;
        s_remote_disconnected = false;
        s_uplink_subscribed = false;
        s_remote_audio_enabled = true;
        s_bad_rx_logged = false;
        s_stop_reason = LIVE_STOP_NONE;
        s_connection_error = 0;
        if (!s_home_allowed || s_stop_requested)
        {
            s_stop_reason = LIVE_STOP_HOME_LEFT;
            s_stop_requested = true;
        }
        accepted = 0;
        liot_rtos_exit_critical();
    }
    demo_tirtc_admission_leave();

    liot_trace("[LIVE] incoming handle=%p %s home=%u busy=%u "
               "claimed=%u connected=%u stopping=%u\r\n",
               hconn, accepted == 0 ? "ACCEPT" : "REJECT",
               home_allowed ? 1U : 0U, busy ? 1U : 0U,
               claimed ? 1U : 0U, connected ? 1U : 0U,
                stopping ? 1U : 0U);
    if (eligible && adopt_ret != 0)
    {
        liot_trace("[LIVE] incoming adopt failed ret=%d\r\n", adopt_ret);
    }

    if (accepted == 0)
    {
        live_signal();
    }
    return accepted;
}

static void live_on_conn_error(tirtc_conn_t hconn, int error)
{
    bool matched = false;

    liot_rtos_enter_critical();
    if (live_matches_connection_locked(hconn))
    {
        s_connection_error = error;
        s_stop_reason = LIVE_STOP_CONNECTION_ERROR;
        s_stop_requested = true;
        matched = true;
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        liot_trace("[LIVE] connection error handle=%p error=%d (%s)\r\n",
                   hconn, error, TiRtcGetErrorStr(error));
        live_signal();
    }
}

static void live_on_disconnected(tirtc_conn_t hconn)
{
    bool matched = false;

    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == (tirtc_conn_t)s_closing_conn)
    {
        /* Optional SDK-side release notification used for diagnostics. */
        s_disconnect_complete = true;
        matched = true;
    }
    else if (live_matches_connection_locked(hconn))
    {
        s_remote_disconnected = true;
        s_disconnect_complete = true;
        s_stop_reason = LIVE_STOP_REMOTE_DISCONNECTED;
        s_stop_requested = true;
        matched = true;
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        liot_trace("[LIVE] disconnected handle=%p\r\n", hconn);
        live_signal();
    }
}

static void live_on_audio(tirtc_conn_t hconn,
                          const TIRTCFRAMEINFO *frame,
                          void *data)
{
    live_rx_frame_t *message;
    uint32_t generation = 0U;
    uint8_t slot;
    int acquired;
    bool matched;
    bool log_bad = false;

    liot_rtos_enter_critical();
    matched = live_matches_connection_locked(hconn);
    liot_rtos_exit_critical();
    if (!matched || frame == NULL || data == NULL)
    {
        return;
    }
    if ((frame->stream_id != LIVE_DOWNLINK_GITHUB_STREAM_ID &&
         frame->stream_id != LIVE_DOWNLINK_STREAM_ID) ||
        frame->media != TIRTC_AUDIO_ALAW || frame->length == 0U ||
        frame->length > LIVE_RX_PAYLOAD_MAX ||
        (frame->flags != TIRTC_AUDIOSAMPLE_8K16B1C &&
         frame->flags != TIRTC_AUDIOSAMPLE_16K16B1C))
    {
        liot_rtos_enter_critical();
        if (!s_bad_rx_logged)
        {
            s_bad_rx_logged = true;
            log_bad = true;
        }
        liot_rtos_exit_critical();
        if (log_bad)
        {
            liot_trace("[LIVE] unsupported downlink stream=%u media=%u "
                       "flags=%u len=%u (want stream=10/14 ALAW 8k/16k)\r\n",
                       (unsigned int)frame->stream_id,
                       (unsigned int)frame->media,
                       (unsigned int)frame->flags,
                       (unsigned int)frame->length);
        }
        return;
    }

    acquired = live_rx_slot_acquire(hconn, &generation);
    if (acquired < 0)
    {
        live_note_rx_drop();
        return;
    }
    slot = (uint8_t)acquired;
    message = &s_rx_pool[slot];
    message->generation = generation;
    message->timestamp_ms = frame->ts;
    message->length = (uint16_t)frame->length;
    message->flags = frame->flags;
    memcpy(message->payload, data, frame->length);

    /* Count before publishing the queue item.  The consumer can run as soon
     * as queue_release returns, so incrementing afterwards can leave a false
     * non-zero count when producer and consumer overlap. */
    liot_rtos_enter_critical();
    ++s_rx_queued;
    liot_rtos_exit_critical();

    if (s_rx_queue == NULL ||
        liot_rtos_queue_release(s_rx_queue, sizeof(slot), &slot,
                                LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        live_rx_queued_decrement();
        live_rx_slot_release(slot);
        live_rx_producer_done();
        live_note_rx_drop();
        return;
    }
    live_rx_producer_done();
    live_signal();
}

static int live_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    int accepted = -1;

    if (stream_id != LIVE_UPLINK_STREAM_ID)
    {
        liot_trace("[LIVE] reject device-audio subscription stream=%u "
                   "handle=%p (want 10)\r\n",
                   (unsigned int)stream_id, hconn);
        return -1;
    }
    liot_rtos_enter_critical();
    if (live_matches_connection_locked(hconn))
    {
        s_uplink_subscribed = true;
        s_remote_audio_enabled = true;
        accepted = 0;
    }
    liot_rtos_exit_critical();
    if (accepted == 0)
    {
        liot_trace("[LIVE] platform subscribed device audio stream=%u\r\n",
                   (unsigned int)stream_id);
        live_signal();
    }
    else
    {
        liot_trace("[LIVE] reject device-audio subscription stream=%u "
                   "handle=%p\r\n", (unsigned int)stream_id, hconn);
    }
    return accepted;
}

static void live_on_unsubscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    bool matched = false;

    if (stream_id != LIVE_UPLINK_STREAM_ID)
    {
        return;
    }
    liot_rtos_enter_critical();
    if (live_matches_connection_locked(hconn))
    {
        s_uplink_subscribed = false;
        s_remote_audio_enabled = false;
        matched = true;
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        liot_trace("[LIVE] platform unsubscribed device audio stream=%u\r\n",
                   (unsigned int)stream_id);
        live_signal();
    }
}

static int live_on_subscribe_video(tirtc_conn_t hconn, uint8_t stream_id)
{
    bool matched;
    int accepted = -1;

    liot_rtos_enter_critical();
    matched = live_matches_connection_locked(hconn);
    if (matched && stream_id == LIVE_VIDEO_STREAM_ID)
    {
        /* The platform VIEW page always asks for video stream 11.  This board
         * is intentionally audio-only, so accept the advertised stream to
         * keep the VIEW session alive but publish no video frames. */
        accepted = 0;
    }
    liot_rtos_exit_critical();
    liot_trace("[LIVE] platform video subscription stream=%u %s "
               "(audio-only; no video frames)\r\n",
               (unsigned int)stream_id,
               accepted == 0 ? "accepted" : "rejected");
    return accepted;
}

static void live_on_unsubscribe_video(tirtc_conn_t hconn, uint8_t stream_id)
{
    bool matched;

    liot_rtos_enter_critical();
    matched = live_matches_connection_locked(hconn);
    liot_rtos_exit_critical();
    if (matched)
    {
        liot_trace("[LIVE] platform unsubscribed video stream=%u\r\n",
                   (unsigned int)stream_id);
    }
}

static void live_on_command(tirtc_conn_t hconn, uint32_t cmdw,
                            const void *data, uint32_t length)
{
    uint16_t command = (uint16_t)(cmdw & 0x7fffU);
    bool enabled = true;
    bool matched;
    bool stop = false;

    if (data != NULL && length >= 1U)
    {
        enabled = ((const uint8_t *)data)[0] != 0U;
    }
    liot_rtos_enter_critical();
    matched = live_matches_connection_locked(hconn);
    if (matched && (cmdw & LIVE_CMD_RESPONSE_BIT) == 0U)
    {
        if (command == LIVE_CMD_REQ_AUDIO ||
            command == LIVE_CMD_SET_SEND_AUDIO)
        {
            s_remote_audio_enabled = enabled;
        }
        else if (command == LIVE_CMD_HANGUP)
        {
            s_stop_reason = LIVE_STOP_REMOTE_DISCONNECTED;
            s_stop_requested = true;
            stop = true;
        }
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        liot_trace("[LIVE] command cmd=0x%04x response=%u enabled=%u len=%u\r\n",
                   (unsigned int)command,
                   (cmdw & LIVE_CMD_RESPONSE_BIT) != 0U ? 1U : 0U,
                   enabled ? 1U : 0U, (unsigned int)length);
        if (stop)
        {
            live_signal();
        }
    }
}

static const demo_tirtc_incoming_listener_t s_incoming_listener = {
    .on_conn_accepted = live_on_conn_accepted,
    .on_conn_error = live_on_conn_error,
    .on_disconnected = live_on_disconnected,
    .on_audio = live_on_audio,
    .on_command = live_on_command,
    .on_subscribe_video = live_on_subscribe_video,
    .on_unsubscribe_video = live_on_unsubscribe_video,
    .on_subscribe_audio = live_on_subscribe_audio,
    .on_unsubscribe_audio = live_on_unsubscribe_audio,
};

static bool live_begin_claimed_session(void)
{
    tirtc_conn_t connection;
    uint32_t generation;
    uint8_t speaker_level;
    uint8_t mic_level;
    int subscribe_github_ret;
    int subscribe_legacy_ret;
    int request_audio_ret;
    int audio_ret;
    uint8_t request_audio = 1U;
    uint32_t request_cmdw;

    liot_rtos_enter_critical();
    connection = (tirtc_conn_t)s_claimed_conn;
    generation = s_generation;
    speaker_level = s_speaker_level;
    mic_level = s_mic_level;
    if (connection != NULL && s_home_allowed && !s_stop_requested)
    {
        s_conn = connection;
        s_claimed_conn = NULL;
    }
    else
    {
        connection = NULL;
    }
    liot_rtos_exit_critical();

    if (connection == NULL)
    {
        return false;
    }

    live_discard_rx_queue();
    s_prebuffer_start_ms = 0U;
    liot_trace("[LIVE] incoming claimed handle=%p generation=%u\r\n",
               connection, (unsigned int)generation);
    audio_ret = demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_LIVE,
                                      &s_audio_lease);
    if (audio_ret == 0)
    {
        liot_rtos_enter_critical();
        s_audio_owned = true;
        liot_rtos_exit_critical();
        audio_ret = demo_ai_audio_init(&s_audio_lease);
    }
    if (audio_ret != 0)
    {
        liot_rtos_enter_critical();
        s_stop_reason = LIVE_STOP_AUDIO_ERROR;
        s_stop_requested = true;
        liot_rtos_exit_critical();
        liot_trace("[LIVE] audio acquire/init failed ret=%d\r\n", audio_ret);
        return false;
    }

    /* The native/device VIEW profile maps audio to stream 10.  The current
     * browser H5 publishes microphone talkback on stream 14.  Subscribe to
     * both and accept the session when either path is available. */
    subscribe_github_ret = demo_tirtc_subscribe_audio(
        DEMO_TIRTC_OWNER_LIVE, s_tirtc_session_generation,
        LIVE_DOWNLINK_GITHUB_STREAM_ID);
    subscribe_legacy_ret = demo_tirtc_subscribe_audio(
        DEMO_TIRTC_OWNER_LIVE, s_tirtc_session_generation,
        LIVE_DOWNLINK_STREAM_ID);
    liot_trace("[LIVE] subscribe platform downlink stream=10/14 ret=%d/%d\r\n",
               subscribe_github_ret, subscribe_legacy_ret);
    if (subscribe_github_ret < 0 && subscribe_legacy_ret < 0)
    {
        liot_rtos_enter_critical();
        s_connection_error = subscribe_github_ret;
        s_stop_reason = LIVE_STOP_CONNECTION_ERROR;
        s_stop_requested = true;
        liot_rtos_exit_critical();
        return false;
    }

    /* The complete official VIEW implementation pairs SubscribeAudio with
     * REQ_AUDIO.  Current H5 publishes talkback on stream 14 while older and
     * device-to-device paths use 10, so keep both subscriptions and request
     * audio explicitly.  A peer that does not implement this command simply
     * ignores it; the session remains usable. */
    request_cmdw = ((generation & 0xffffU) << 16) | LIVE_CMD_REQ_AUDIO;
    request_audio_ret = demo_tirtc_send_command(
        DEMO_TIRTC_OWNER_LIVE, s_tirtc_session_generation,
        request_cmdw, &request_audio, sizeof(request_audio));
    liot_trace("[LIVE] request platform audio cmd=0x%08x ret=%d\r\n",
               (unsigned int)request_cmdw, request_audio_ret);

    (void)demo_ai_audio_set_levels(&s_audio_lease,
                                   speaker_level, mic_level);
    liot_rtos_enter_critical();
    if (s_speaker_level == speaker_level && s_mic_level == mic_level)
    {
        s_levels_changed = false;
    }
    liot_rtos_exit_critical();
    liot_rtos_enter_critical();
    if (s_conn == connection && !s_stop_requested)
    {
        s_active = true;
        s_rx_dropped = 0U;
        s_tx_dropped = 0U;
        s_rx_frames = 0U;
        s_tx_frames = 0U;
    }
    liot_rtos_exit_critical();
    if (!s_active)
    {
        return false;
    }

    liot_trace("[LIVE] active bidirectional: uplink=10 ALAW/8k, "
               "downlink=10-or-14 ALAW/8k-or-16k, "
               "video=11 accepted/no-data, no AEC\r\n");
    return true;
}

static int live_play_one_downlink(uint32_t generation)
{
    live_rx_frame_t *message;
    uint8_t slot;
    uint32_t input_samples;
    uint32_t output_samples;
    uint32_t i;
    uint32_t peak = 0U;
    uint8_t source_flags;
    int ret;

    if (s_rx_queue == NULL ||
        liot_rtos_queue_wait(s_rx_queue, &slot, sizeof(slot),
                             LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        return 0;
    }
    live_rx_queued_decrement();
    if (slot >= LIVE_RX_QUEUE_DEPTH)
    {
        live_note_rx_drop();
        return 0;
    }

    message = &s_rx_pool[slot];
    if (message->generation != generation)
    {
        live_rx_slot_release(slot);
        return 1;
    }

    input_samples = message->length;
    source_flags = message->flags;
    if (source_flags == TIRTC_AUDIOSAMPLE_8K16B1C)
    {
        output_samples = input_samples * 2U;
        if (output_samples > LIVE_PLAY_PCM_MAX_SAMPLES)
        {
            live_rx_slot_release(slot);
            live_note_rx_drop();
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
            live_rx_slot_release(slot);
            live_note_rx_drop();
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
    live_rx_slot_release(slot);
    if (ret != 0)
    {
        live_note_rx_drop();
        liot_trace("[LIVE] playback rejected ret=%d samples=%u\r\n",
                   ret, (unsigned int)output_samples);
        return 1;
    }
    ++s_rx_frames;
    if (s_rx_frames == 1U)
    {
        liot_trace("[LIVE] first downlink ALAW frame bytes=%u "
                   "source=%s pcm_samples=%u peak=%u play_ret=0\r\n",
                   (unsigned int)input_samples,
                   source_flags == TIRTC_AUDIOSAMPLE_8K16B1C ?
                       "8k" : "16k",
                   (unsigned int)output_samples,
                   (unsigned int)peak);
    }
    return 1;
}

static void live_service_downlink(uint32_t generation)
{
    uint32_t queued;
    uint32_t now;
    uint32_t i;

    liot_rtos_enter_critical();
    queued = s_rx_queued;
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
        /* Match WX/DEV playback: start with a small jitter cushion, then keep
         * Liot_AudioPlay's copied queue filled so I2S and PA stay enabled. */
        if (s_prebuffer_start_ms == 0U)
        {
            s_prebuffer_start_ms = now;
        }
        if (queued < LIVE_PREBUFFER_PACKETS &&
            (int32_t)(now - s_prebuffer_start_ms) <
                (int32_t)LIVE_PREBUFFER_TIMEOUT_MS)
        {
            return;
        }
    }
    s_prebuffer_start_ms = 0U;
    for (i = 0U; i < LIVE_RX_QUEUE_DEPTH; ++i)
    {
        if (live_play_one_downlink(generation) <= 0)
        {
            break;
        }
    }
}

static int live_send_uplink_20ms(tirtc_conn_t connection)
{
    TIRTCFRAMEINFO frame;
    uint32_t i;
    int ret;

    ret = demo_ai_audio_record_20ms(&s_audio_lease, s_capture_pcm);
    if (ret != 0)
    {
        liot_trace("[LIVE] capture failed ret=%d\r\n", ret);
        return ret;
    }
    /* ES8311 remains at the AI-compatible 16 kHz hardware rate.  Average
     * adjacent samples to form the platform's fixed 8 kHz LIVE stream. */
    for (i = 0U; i < LIVE_UPLINK_8K_SAMPLES; ++i)
    {
        int32_t mixed = (int32_t)s_capture_pcm[i * 2U] +
                        (int32_t)s_capture_pcm[i * 2U + 1U];
        s_uplink_alaw[i] =
            demo_g711_alaw_encode_sample((int16_t)(mixed / 2));
    }

    memset(&frame, 0, sizeof(frame));
    frame.stream_id = LIVE_UPLINK_STREAM_ID;
    frame.media = TIRTC_AUDIO_ALAW;
    frame.flags = TIRTC_AUDIOSAMPLE_8K16B1C;
    frame.ts = liot_rtos_get_running_time();
    frame.length = LIVE_UPLINK_8K_SAMPLES;
    ret = demo_tirtc_send_audio(DEMO_TIRTC_OWNER_LIVE,
                                s_tirtc_session_generation,
                                &frame, s_uplink_alaw);
    if (ret == TIRTC_E_BUSY)
    {
        ++s_tx_dropped;
        return 0;
    }
    if (ret < 0)
    {
        liot_trace("[LIVE] uplink failed ret=%d (%s)\r\n",
                   ret, TiRtcGetErrorStr(ret));
        return ret;
    }
    ++s_tx_frames;
    if (s_tx_frames == 1U)
    {
        liot_trace("[LIVE] first uplink ALAW frame bytes=%u 8kHz\r\n",
                   (unsigned int)LIVE_UPLINK_8K_SAMPLES);
    }
    return 0;
}

static void live_cleanup_session(void)
{
    tirtc_conn_t connection;
    bool remote_disconnected;
    live_stop_reason_e reason;
    int connection_error;
    bool audio_owned;
    bool tirtc_owned;
    bool tirtc_released = true;
    uint32_t session_generation;
    int audio_ret = 0;
    int audio_release_ret = 0;
    int disconnect_ret = 0;
    uint32_t disconnect_deadline;
    bool disconnect_complete = false;

    liot_rtos_enter_critical();
    connection = s_conn != NULL ? (tirtc_conn_t)s_conn :
                                  (tirtc_conn_t)s_claimed_conn;
    remote_disconnected = s_remote_disconnected;
    reason = s_stop_reason;
    connection_error = s_connection_error;
    audio_owned = s_audio_owned;
    tirtc_owned = s_tirtc_owned;
    session_generation = s_tirtc_session_generation;
    ++s_generation;
    if (s_generation == 0U)
    {
        ++s_generation;
    }
    s_conn = NULL;
    s_claimed_conn = NULL;
    s_closing_conn = connection;
    s_active = false;
    s_uplink_subscribed = false;
    s_remote_audio_enabled = false;
    s_disconnect_complete = remote_disconnected;
    liot_rtos_exit_critical();

    if (audio_owned)
    {
        audio_ret = demo_ai_audio_stop(&s_audio_lease);
    }
    (void)live_wait_rx_producers();
    live_discard_rx_queue();
    s_prebuffer_start_ms = 0U;
    if (connection != NULL && !remote_disconnected)
    {
        disconnect_ret = demo_tirtc_disconnect(
            DEMO_TIRTC_OWNER_LIVE, session_generation);
        if (disconnect_ret == 0)
        {
            /* Bound the feature-task wait.  The runtime still keeps its
             * closing reservation until the SDK callback confirms release. */
            disconnect_deadline = liot_rtos_get_running_time() +
                                  LIVE_DISCONNECT_WAIT_MS;
            while ((int32_t)(disconnect_deadline -
                             liot_rtos_get_running_time()) > 0)
            {
                liot_rtos_enter_critical();
                disconnect_complete = s_disconnect_complete;
                liot_rtos_exit_critical();
                if (disconnect_complete)
                {
                    break;
                }
                (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
            }
            if (!disconnect_complete)
            {
                liot_trace("[LIVE] disconnect callback not observed in %u ms; "
                           "managed session remains reserved handle=%p\r\n",
                           (unsigned int)LIVE_DISCONNECT_WAIT_MS, connection);
            }
        }
        else if (disconnect_ret == TIRTC_E_INVALID_HANDLE)
        {
            disconnect_complete = true;
        }
    }
    else
    {
        /* on_disconnected runs on an SDK thread.  Yield once so that callback
         * can return before HOME admission is reopened for a new handle. */
        liot_rtos_task_sleep_ms(20U);
        disconnect_complete = true;
    }
    if (audio_owned)
    {
        audio_release_ret = demo_ai_audio_release(&s_audio_lease);
    }
    if (tirtc_owned)
    {
        tirtc_released = live_release_tirtc_session();
    }
    liot_rtos_enter_critical();
    if (audio_release_ret == 0)
    {
        s_audio_owned = false;
    }
    s_closing_conn = NULL;
    s_disconnect_complete = false;
    s_stop_requested = false;
    s_remote_disconnected = false;
    s_stop_reason = LIVE_STOP_NONE;
    s_connection_error = 0;
    /* A failed lease release is an invariant violation.  Fail closed instead
     * of admitting a second LIVE session onto an ambiguously owned codec. */
    s_busy = s_audio_owned || s_tirtc_owned;
    liot_rtos_exit_critical();
    live_signal();
    liot_trace("[LIVE] closed reason=%d error=%d audio=%d release=%d "
               "rtc_release=%u "
               "disconnect=%d "
               "settled=%u "
               "rx_drop=%u tx_drop=%u\r\n",
                (int)reason, connection_error, audio_ret, audio_release_ret,
                tirtc_released ? 1U : 0U,
                disconnect_ret,
               disconnect_complete ? 1U : 0U,
               (unsigned int)s_rx_dropped,
               (unsigned int)s_tx_dropped);
}

void demo_live_talk_set_home_allowed(bool allowed)
{
    bool wake = false;
    bool changed;
    bool busy;

    liot_rtos_enter_critical();
    changed = !s_home_policy_logged || s_home_allowed != allowed;
    s_home_policy_logged = true;
    s_home_allowed = allowed;
    busy = s_busy;
    if (!allowed && (s_conn != NULL || s_claimed_conn != NULL))
    {
        s_stop_reason = LIVE_STOP_HOME_LEFT;
        s_stop_requested = true;
        wake = true;
    }
    liot_rtos_exit_critical();
    if (changed)
    {
        liot_trace("[LIVE] HOME admission %s busy=%u\r\n",
                   allowed ? "ENABLED" : "DISABLED", busy ? 1U : 0U);
    }
    if (wake)
    {
        live_signal();
    }
}

void demo_live_talk_set_audio_levels(uint8_t speaker_level,
                                     uint8_t mic_level)
{
    liot_rtos_enter_critical();
    s_speaker_level = speaker_level;
    s_mic_level = mic_level;
    s_levels_changed = true;
    liot_rtos_exit_critical();
    live_signal();
}

bool demo_live_talk_is_active(void)
{
    bool active;

    liot_rtos_enter_critical();
    active = s_active;
    liot_rtos_exit_critical();
    return active;
}

bool demo_live_talk_wait_idle(uint32_t timeout_ms)
{
    uint32_t deadline = liot_rtos_get_running_time() + timeout_ms;
    bool busy;

    while (1)
    {
        liot_rtos_enter_critical();
        busy = s_busy;
        liot_rtos_exit_critical();
        if (!busy)
        {
            return true;
        }
        if (timeout_ms == 0U ||
            (int32_t)(deadline - liot_rtos_get_running_time()) <= 0)
        {
            return false;
        }
        /* Do not consume the owner's semaphore; polling keeps the stop wakeup
         * reserved for demo_live_talk_task and bounds handoff latency. */
        liot_rtos_task_sleep_ms(10U);
    }
}

bool demo_live_talk_wait_ready(uint32_t timeout_ms)
{
    uint32_t deadline = liot_rtos_get_running_time() + timeout_ms;

    while (!s_manager_ready)
    {
        if (timeout_ms == 0U ||
            (int32_t)(deadline - liot_rtos_get_running_time()) <= 0)
        {
            return false;
        }
        liot_rtos_task_sleep_ms(10U);
    }
    return true;
}

void demo_live_talk_task(void *argv)
{
    tirtc_conn_t connection;
    uint32_t generation;
    uint8_t speaker_level;
    uint8_t mic_level;
    bool active;
    bool claim_pending;
    bool stop_requested;
    bool remote_audio_enabled;
    bool levels_changed;
    bool release_pending;
    int ret;

    (void)argv;
    if (liot_rtos_semaphore_create(&s_event_sem, 0U) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_rx_queue, sizeof(uint8_t),
                               LIVE_RX_QUEUE_DEPTH) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }

    demo_tirtc_set_incoming_listener(&s_incoming_listener);
    s_manager_ready = true;
    liot_trace("[LIVE] manager ready; waiting platform connection at HOME\r\n");
    while (1)
    {
        liot_rtos_enter_critical();
        release_pending = s_tirtc_owned && !s_active &&
                          s_conn == NULL && s_claimed_conn == NULL &&
                          s_closing_conn == NULL && !s_stop_requested;
        active = s_active;
        claim_pending = s_claimed_conn != NULL;
        stop_requested = s_stop_requested;
        connection = (tirtc_conn_t)s_conn;
        generation = s_generation;
        liot_rtos_exit_critical();

        if (release_pending && !live_release_tirtc_session())
        {
            (void)liot_rtos_semaphore_wait(s_event_sem, 100U);
            continue;
        }

        if (!active && !stop_requested && claim_pending)
        {
            (void)live_begin_claimed_session();
            continue;
        }
        if (stop_requested)
        {
            live_cleanup_session();
            continue;
        }
        if (!active || connection == NULL)
        {
            (void)liot_rtos_semaphore_wait(s_event_sem, LIVE_IDLE_WAIT_MS);
            continue;
        }

        liot_rtos_enter_critical();
        speaker_level = s_speaker_level;
        mic_level = s_mic_level;
        levels_changed = s_levels_changed;
        if (levels_changed)
        {
            s_levels_changed = false;
        }
        liot_rtos_exit_critical();
        if (levels_changed)
        {
            (void)demo_ai_audio_set_levels(&s_audio_lease,
                                           speaker_level, mic_level);
        }

        live_service_downlink(generation);
        liot_rtos_enter_critical();
        stop_requested = s_stop_requested;
        remote_audio_enabled = s_remote_audio_enabled;
        liot_rtos_exit_critical();
        if (stop_requested)
        {
            continue;
        }

        if (!remote_audio_enabled)
        {
            (void)liot_rtos_semaphore_wait(s_event_sem,
                                           LIVE_ACTIVE_WAIT_MS);
            continue;
        }

        /* Playback is asynchronous TX DMA; 20 ms capture runs on the
         * independent RX path and naturally paces the device uplink. */
        ret = live_send_uplink_20ms(connection);
        if (ret < 0)
        {
            liot_rtos_enter_critical();
            if (s_conn == connection)
            {
                s_connection_error = ret;
                s_stop_reason = LIVE_STOP_SEND_ERROR;
                s_stop_requested = true;
            }
            liot_rtos_exit_critical();
        }
    }

init_failed:
    if (s_rx_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_rx_queue);
        s_rx_queue = NULL;
    }
    if (s_event_sem != NULL)
    {
        (void)liot_rtos_semaphore_delete(s_event_sem);
        s_event_sem = NULL;
    }
    liot_trace("[LIVE] queue/semaphore init failed\r\n");
    liot_rtos_task_delete(NULL);
}
