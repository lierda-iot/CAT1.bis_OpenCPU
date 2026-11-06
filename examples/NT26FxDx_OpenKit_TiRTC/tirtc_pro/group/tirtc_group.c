/* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * Independent Room owner. Only this worker touches Room audio/state;
 * blocking HTTP/WHIP submission runs on the bounded service worker.
 */
#include "tirtc_group.h"
#include "../ui/tirtc_ui.h"
#include "../ai/tirtc_ai.h"
#include "../calls/tirtc_calls.h"
#include "../remote/tirtc_remote.h"
#include "../tirtc_log.h"
#include "audio_device.h"
#include "device_binding.h"
#include "formal_mqtt.h"
#include "g711_codec.h"
#include "../network/tirtc_network.h"
#include "tirtc_runtime.h"
#include "cJSON.h"
#include "liot_dev.h"
#include "liot_log.h"
#include "liot_os.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROOM_COMMAND               0x2200U
#define ROOM_STREAM                1U
#define ROOM_ID_MAX                65U
#define ROOM_WIRE_ID_MAX           256U
#define ROOM_SESSION_MAX           65U
#define ROOM_CONNECT_MAX           1152U
#define ROOM_HTTP_MAX              4096U
#define ROOM_HTTP_TIMEOUT          10000U
#define ROOM_RX_SLOTS              16U
#define ROOM_RX_MAX                640U
#define ROOM_RX_MAX_AGE            500U
#define ROOM_PLAY_AHEAD            240U
#define ROOM_PLAY_PREBUFFER_MS      80U
#define ROOM_PLAY_PREBUFFER_BYTES  640U
#define ROOM_PLAY_BLOCKS            12U
#define ROOM_PLAY_WORK_MS           10U
#define ROOM_COMMAND_SLOTS         2U
#define ROOM_COMMAND_MAX           16384U
#define ROOM_SYNC_INTERVAL         60000U
#define ROOM_MIC_RETRY_INTERVAL    1000U
#if DEMO_AI_AUDIO_AEC_ENABLE
#define ROOM_RECORD_FAILURE_LIMIT_MS 3000U
#endif
#define ROOM_RESOURCE_TIMEOUT      30000U
#define ROOM_CONNECT_TIMEOUT       20000U
#define ROOM_JOIN_TIMEOUT          8000U
#define ROOM_MAX_RETRIES            6U
#define ROOM_UNKNOWN_LEASE_WAIT     60000U
#define ROOM_SEND_HIGH_WATER        4096U
#define ROOM_ERR_PROTOCOL         (-6101)
#define ROOM_ERR_AUDIO            (-6102)
#define ROOM_ERR_OFFLINE          (-6103)
#define ROOM_ERR_LEASE            (-6104)
#define ROOM_ERR_INIT             (-6105)

/* Independent of verbose application logging. Aggregate only; never log PCM,
 * packet contents, timestamps, connection handles or participant identities. */
#ifndef ROOM_AUDIO_DIAGNOSTICS
#define ROOM_AUDIO_DIAGNOSTICS 1
#endif
#if ROOM_AUDIO_DIAGNOSTICS
#define ROOM_AUDIO_DIAG(...) do { __VA_ARGS__ } while (0)
#else
#define ROOM_AUDIO_DIAG(...) do {} while (0)
#endif

typedef struct {
    char room_id[ROOM_ID_MAX];
    char room_code[7];
    uint32_t version;
    bool desired;
    bool closed;
} room_assignment_t;

typedef enum {
    ROOM_JOB_ASSIGNMENT = 0, ROOM_JOB_TOKEN, ROOM_JOB_PRESENCE,
    ROOM_JOB_TERMINAL, ROOM_JOB_CONNECT, ROOM_JOB_CREATE, ROOM_JOB_JOIN, ROOM_JOB_LEAVE
} room_job_kind_e;

typedef struct {
    room_job_kind_e kind;
    uint32_t intent;
    uint32_t epoch;
    uint32_t sync_sequence;
    uint32_t version;
    uint32_t timeout_ms;
    char room_id[ROOM_ID_MAX];
    char session_id[ROOM_SESSION_MAX];
    char presence[16];
    char code[7], password[5];
    demo_binding_service_context_t context;
    char peer[ROOM_CONNECT_MAX];
    char token[ROOM_CONNECT_MAX];
} room_job_t;

typedef struct {
    int code;
    uint32_t started_ms;
    room_assignment_t assignment;
    char peer[ROOM_CONNECT_MAX];
    char token[ROOM_CONNECT_MAX];
    uint32_t heartbeat_ms;
    uint32_t lease_ms;
} room_http_result_t;

typedef struct {
    bool used;
    uint32_t epoch;
    uint32_t received_ms;
    uint16_t length;
    uint8_t bytes[ROOM_RX_MAX];
} room_audio_slot_t;

typedef struct {
    bool used;
    uint32_t epoch;
    uint16_t length;
    char bytes[ROOM_COMMAND_MAX + 1U];
} room_command_slot_t;

static liot_sem_t s_wake;
static liot_sem_t s_http_wake;
static liot_task_t s_http_task, s_room_task;
static bool s_registered;
static volatile bool s_initialized;
/* These fields are shared only in short critical sections. */
static bool s_enabled;
static bool s_foreground;
static bool s_suspend_requested;
static bool s_media_idle = true;
static bool s_accept_audio;
static bool s_accept_commands;
static bool s_mic_gate;
static bool s_mic_dirty;
static bool s_levels_dirty;
static uint8_t s_speaker = DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL;
static uint8_t s_mic_level = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;
static uint32_t s_intent;
static uint32_t s_sync_sequence;
static demo_group_call_e s_call;
static uint32_t s_call_ticket;
static uint32_t s_ticket_sequence;
static demo_group_snapshot_t s_snapshot;

/* All business fields below are owned by the Room worker unless annotated. */
static room_assignment_t s_assignment;
static uint32_t s_seen_intent;
static uint32_t s_sync_done;
static uint32_t s_epoch;
static uint32_t s_generation;
static bool s_claimed;
static bool s_joined;
static bool s_join_pending;
static bool s_stopping;
static bool s_cleanup_sent;
static bool s_stopped_audio;
static bool s_token_requested;
static bool s_manual_error;
static bool s_waiting_token;
static demo_group_state_e s_after_cleanup;
static int s_cleanup_error;
static uint32_t s_deadline;
static uint32_t s_retry_at;
static uint32_t s_sync_at;
static uint32_t s_heartbeat_at;
static uint32_t s_mic_retry_at;
static uint32_t s_lease_deadline;
static uint32_t s_heartbeat_ms;
static uint32_t s_lease_ms;
static uint32_t s_join_id;
static uint32_t s_session_started;
static unsigned int s_failures;
static bool s_conflict_wait;
static uint32_t s_conflict_deadline;
static char s_session[ROOM_SESSION_MAX];
static char s_media_session[129];
static char s_device[DEMO_BIND_DEVICE_ID_MAX];
static char s_wire_room[ROOM_WIRE_ID_MAX];
static demo_ai_audio_lease_t s_audio = DEMO_AI_AUDIO_LEASE_INIT;
/* Opaque handles are compared for callback correlation only, never called. */
static tirtc_conn_t s_wire_connection;
static bool s_connect_pending;
static bool s_connect_event;
static bool s_sdk_submit_busy;
static int s_connect_error;
static int s_callback_error;
static uint32_t s_callback_epoch;
static struct { uint32_t epoch; } s_connect_context;
static uint32_t s_producers;
static room_audio_slot_t s_rx[ROOM_RX_SLOTS];
static uint8_t s_rx_ring[ROOM_RX_SLOTS];
static uint8_t s_rx_read, s_rx_write, s_rx_count;
static uint32_t s_rx_bytes;
static room_command_slot_t s_commands[ROOM_COMMAND_SLOTS];
static uint8_t s_cmd_ring[ROOM_COMMAND_SLOTS];
static uint8_t s_cmd_read, s_cmd_write, s_cmd_count;
static bool s_command_gap;
static uint32_t s_command_oversize;
#define ROOM_NETWORK_SAMPLES_8K (DEMO_AI_AUDIO_FRAME_SAMPLES / 2U)
static __attribute__((aligned(16))) int16_t s_capture[DEMO_AI_AUDIO_FRAME_SAMPLES];
static __attribute__((aligned(16))) int16_t s_play[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint8_t s_alaw[ROOM_NETWORK_SAMPLES_8K];
/* One immutable job/result mailbox. Terminal reporting has one separate
 * saved tuple, so old HTTP can finish without retaining media ownership. */
static room_job_t s_http_job;
static room_http_result_t s_http_result;
static char s_http_response[ROOM_HTTP_MAX];
static bool s_http_busy;
static bool s_http_done;
static bool s_terminal_pending;
static room_job_t s_terminal;
static bool s_transport_was_ready;
static bool s_bound_seen;
/* UI requests contain no credentials; passwords are erased after submission. */
static tirtc_ui_room_t s_ui, s_published;
static bool s_assignment_known, s_ui_sent, s_control_pending;
static room_job_kind_e s_control_kind;
static char s_control_code[7], s_control_password[5];
static uint32_t s_control_intent, s_control_version, s_ui_generation=1U;
static demo_binding_service_context_t s_control_context;
static uint32_t s_members_due;
static char s_self_participant[64];
static bool s_microphone=true, s_speaker_enabled=true, s_capture_discard;
#if DEMO_AI_AUDIO_AEC_ENABLE
static bool s_record_failure_active;
static uint32_t s_record_failure_since;
#endif
static uint32_t s_ptt_until, s_ptt_revision, s_play_tail, s_play_offset, s_tx_stamp;
static bool s_play_guard, s_play_continuing;
static bool s_play_buffering, s_play_partial;
static uint32_t s_play_buffer_at, s_play_partial_at;
static uint8_t s_play_fraction;
static int s_control_error;
static demo_binding_service_context_t s_session_context;
static void room_publish(void);
static void room_control_tick(void);
static void room_foreground_tick(void);

#if ROOM_AUDIO_DIAGNOSTICS
typedef struct {
    uint32_t packets, bytes, length_min, length_max, bad_format, oversized;
    uint8_t observed_stream, observed_media, observed_flags;
    bool observed_bad_format;
    uint32_t peak_slots, peak_bytes, drop_slot, drop_bytes, drop_age;
    uint32_t callback_gap, timestamp_repeat, timestamp_back;
    uint32_t pcm_samples, plays, warm, warm_ms, busy, errors, tick_gap, tick_work;
} room_audio_diag_window_t;
static struct {
    bool active, rx_seen, tick_seen;
    uint32_t window_at, last_rx_at, last_timestamp, last_tick_at;
    room_audio_diag_window_t window;
} s_audio_diag;

static void room_audio_diag_begin(void) {
    uint32_t now=liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    memset(&s_audio_diag,0,sizeof(s_audio_diag));
    s_audio_diag.active=true; s_audio_diag.window_at=now;
    liot_rtos_exit_critical();
}
/* Called before the original acceptance checks, so oversized/wrong-format
 * packets from this connection are visible without changing admission. */
static void room_audio_diag_receive(tirtc_conn_t connection,const TIRTCFRAMEINFO *frame) {
    uint32_t now, gap;
    liot_rtos_enter_critical();
    if (s_audio_diag.active && connection == s_wire_connection) {
        room_audio_diag_window_t *w=&s_audio_diag.window;
        bool bad_format=frame->stream_id != ROOM_STREAM || frame->media != TIRTC_AUDIO_ALAW ||
            frame->flags != TIRTC_AUDIOSAMPLE_8K16B1C;
        /* Sample inside this lock: concurrent callbacks must not commit an
         * older sampled time after a newer one and inflate the unsigned gap. */
        now=liot_rtos_get_running_time();
        /* Preserve the first packet's metadata, replacing it only with the
         * first wrong-format tuple so a rejection can be diagnosed directly. */
        if (!w->packets || (!w->observed_bad_format && bad_format)) {
            w->observed_stream=frame->stream_id; w->observed_media=frame->media;
            w->observed_flags=frame->flags; w->observed_bad_format=bad_format;
        }
        if (!w->packets || frame->length<w->length_min) w->length_min=frame->length;
        if (frame->length>w->length_max) w->length_max=frame->length;
        ++w->packets; w->bytes+=frame->length;
        if (bad_format || frame->length == 0U) ++w->bad_format;
        if (frame->length>ROOM_RX_MAX) ++w->oversized;
        /* Continuity is per Room stream, including its rejected packets;
         * unrelated streams still contribute to the broad rejection totals. */
        if (frame->stream_id == ROOM_STREAM) {
            if (s_audio_diag.rx_seen) {
                gap=now-s_audio_diag.last_rx_at;
                if (gap>w->callback_gap) w->callback_gap=gap;
                if (frame->ts == s_audio_diag.last_timestamp) ++w->timestamp_repeat;
                else if ((int32_t)(frame->ts-s_audio_diag.last_timestamp)<0) ++w->timestamp_back;
            }
            s_audio_diag.rx_seen=true; s_audio_diag.last_rx_at=now;
            s_audio_diag.last_timestamp=frame->ts;
        }
    }
    liot_rtos_exit_critical();
}
static void room_audio_diag_tick_begin(uint32_t started) {
    liot_rtos_enter_critical();
    if (s_audio_diag.active) {
        uint32_t gap=started-s_audio_diag.last_tick_at;
        if (s_audio_diag.tick_seen && gap>s_audio_diag.window.tick_gap)
            s_audio_diag.window.tick_gap=gap;
        s_audio_diag.tick_seen=true; s_audio_diag.last_tick_at=started;
    }
    liot_rtos_exit_critical();
}
static void room_audio_diag_play(int result,uint32_t samples,bool warm,uint32_t extra) {
    liot_rtos_enter_critical();
    if (s_audio_diag.active) {
        room_audio_diag_window_t *w=&s_audio_diag.window;
        if (result == 0) {
            ++w->plays; w->pcm_samples+=samples;
            if (warm) {++w->warm;w->warm_ms+=extra;}
        } else if (result == DEMO_AI_AUDIO_ERR_BUSY
#if DEMO_AI_AUDIO_AEC_ENABLE
                   || result == DEMO_AI_AUDIO_ERR_PLAY_BUSY
#endif
                  ) ++w->busy;
        else ++w->errors;
    }
    liot_rtos_exit_critical();
}
static void room_audio_diag_tick_end(uint32_t started) {
    uint32_t elapsed=liot_rtos_get_running_time()-started;
    liot_rtos_enter_critical();
    if (s_audio_diag.active && elapsed>s_audio_diag.window.tick_work)
        s_audio_diag.window.tick_work=elapsed;
    liot_rtos_exit_critical();
}
/* One worker resets local windows under the lock without serial output.
 * Preserve gap/TS history between windows, then clear it only after all
 * producers have drained at cleanup. */
static void room_audio_diag_report(bool stopped) {
    uint32_t now=liot_rtos_get_running_time(), elapsed;
    liot_rtos_enter_critical();
    elapsed=now-s_audio_diag.window_at;
    if (!s_audio_diag.active || (!stopped && elapsed<5000U)) {
        liot_rtos_exit_critical(); return;
    }
    if (stopped) memset(&s_audio_diag,0,sizeof(s_audio_diag));
    else {
        memset(&s_audio_diag.window,0,sizeof(s_audio_diag.window));
        s_audio_diag.window_at=now;
        s_audio_diag.window.peak_slots=s_rx_count;
        s_audio_diag.window.peak_bytes=s_rx_bytes;
    }
    liot_rtos_exit_critical();
}
#endif


static void room_wake(void) {
    if (s_wake != NULL) (void)liot_rtos_semaphore_release(s_wake);
}
static uint32_t room_next(uint32_t value) {
    ++value;
    return value != 0U ? value : 1U;
}
static bool room_due(uint32_t deadline) {
    return (int32_t)(liot_rtos_get_running_time() - deadline) >= 0;
}
static void room_zero(void *data, size_t size) {
    volatile unsigned char *p = (volatile unsigned char *)data;
    while (size-- != 0U) *p++ = 0U;
}
static void room_state(demo_group_state_e state, int error) {
    bool changed;
    liot_rtos_enter_critical();
    changed = s_snapshot.state != state || s_snapshot.error != error;
    s_snapshot.state = state;
    s_snapshot.error = error;
    liot_rtos_exit_critical();
    if (changed) liot_trace("[ROOM] state=%u error=%d epoch=%u\r\n",
        (unsigned int)state, error, (unsigned int)s_epoch);
}
static bool room_allowed(void) {
    bool allowed;
    liot_rtos_enter_critical();
    allowed = s_enabled && s_foreground && s_call == DEMO_GROUP_CALL_NONE &&
        !tirtc_calls_has_session() && !tirtc_ai_has_session() && !tirtc_remote_has_session();
    liot_rtos_exit_critical();
    return allowed;
}
static const char *room_string(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring != NULL ? item->valuestring : "";
}
static bool room_copy(const cJSON *object, const char *key, char *out,
                      size_t capacity, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    size_t length;
    if (!cJSON_IsString(item) || item->valuestring == NULL) return !required && item == NULL;
    length = strlen(item->valuestring);
    if (length >= capacity || (required && length == 0U)) return false;
    memcpy(out, item->valuestring, length + 1U);
    return true;
}
static bool room_number(const cJSON *object, const char *key,
                        uint32_t *value, bool nonzero) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    double number;
    if (!cJSON_IsNumber(item)) return false;
    number = item->valuedouble;
    if (!(number >= (nonzero ? 1.0 : 0.0) && number <= 4294967295.0)) return false;
    *value = (uint32_t)number;
    return number == (double)*value;
}
static cJSON *room_parse_json(const char *text, size_t length) {
    size_t i;
    unsigned int depth=0U;
    bool quoted=false, escaped=false;
    if (text == NULL || length == 0U || memchr(text,'\0',length) != NULL) return NULL;
    /* cJSON's default nesting budget is too large for a 12 KiB worker.
     * Bound recursion before entering the parser, respecting JSON strings. */
    for (i=0U;i<length;++i) {
        char c=text[i];
        if (quoted) {
            if (escaped) escaped=false;
            else if (c=='\\') escaped=true;
            else if (c=='"') quoted=false;
        } else if (c=='"') quoted=true;
        else if (c=='{' || c=='[') { if (++depth>16U) return NULL; }
        else if (c=='}' || c==']') { if (depth==0U) return NULL; --depth; }
    }
    if (quoted || depth != 0U) return NULL;
    return cJSON_ParseWithOpts(text,NULL,1);
}
static bool room_format(const cJSON *object) {
    uint32_t rate, channels;
    return cJSON_IsObject(object) && strcmp(room_string(object,"codec"),"g711a") == 0 &&
        room_number(object,"sample_rate",&rate,true) && rate == 8000U &&
        room_number(object,"channels",&channels,true) && channels == 1U;
}
static cJSON *room_audio_descriptor(void) {
    cJSON *object = cJSON_CreateObject();
    if (object == NULL || cJSON_AddStringToObject(object,"codec","g711a") == NULL ||
        cJSON_AddNumberToObject(object,"sample_rate",8000) == NULL ||
        cJSON_AddNumberToObject(object,"channels",1) == NULL) {
        cJSON_Delete(object); return NULL;
    }
    return object;
}
static int room_send_json(cJSON *object) {
    char *json;
    int result = ROOM_ERR_PROTOCOL;
    if (object == NULL) return result;
    json = cJSON_PrintUnformatted(object);
    if (json != NULL) {
        result = demo_tirtc_send_command(DEMO_TIRTC_OWNER_GROUP_ROOM,
            s_generation, ROOM_COMMAND, json, (uint32_t)strlen(json));
        cJSON_free(json);
    }
    cJSON_Delete(object);
    return result;
}
static cJSON *room_rpc(const char *method) {
    cJSON *object = cJSON_CreateObject();
    if (object == NULL || cJSON_AddStringToObject(object,"jsonrpc","2.0") == NULL ||
        cJSON_AddStringToObject(object,"method",method) == NULL) {
        cJSON_Delete(object); return NULL;
    }
    return object;
}
static int room_send_mic(bool on) {
    cJSON *object = room_rpc("set_mic_state");
    cJSON *params = object != NULL ? cJSON_AddObjectToObject(object,"params") : NULL;
    if (params == NULL || cJSON_AddStringToObject(params,"mic_state",on ? "speaking" : "off") == NULL) {
        cJSON_Delete(object); return ROOM_ERR_PROTOCOL;
    }
    return room_send_json(object);
}
static int room_send_join(void) {
    cJSON *object = room_rpc("join_room");
    cJSON *params, *input, *output;
    if (object == NULL) return ROOM_ERR_PROTOCOL;
    params = cJSON_AddObjectToObject(object,"params");
    input = room_audio_descriptor();
    output = room_audio_descriptor();
    if (params == NULL || input == NULL || output == NULL ||
        cJSON_AddNumberToObject(object,"id",s_join_id) == NULL ||
        cJSON_AddStringToObject(params,"room_id",s_assignment.room_id) == NULL ||
        cJSON_AddStringToObject(params,"device_id",s_device) == NULL) {
        cJSON_Delete(input); cJSON_Delete(output); cJSON_Delete(object);
        return ROOM_ERR_PROTOCOL;
    }
    if (!cJSON_AddItemToObject(params,"input_audio",input)) {
        cJSON_Delete(input); cJSON_Delete(output); cJSON_Delete(object); return ROOM_ERR_PROTOCOL;
    }
    if (!cJSON_AddItemToObject(params,"output_audio",output)) {
        cJSON_Delete(output); cJSON_Delete(object); return ROOM_ERR_PROTOCOL;
    }
    return room_send_json(object);
}

static void room_connect_result(int error, tirtc_conn_t connection, void *user) {
    liot_rtos_enter_critical();
    if (user == &s_connect_context && s_connect_context.epoch == s_epoch) {
        s_connect_pending = false;
        s_connect_error = error != 0 ? error : (connection != NULL ? 0 : ROOM_ERR_PROTOCOL);
        s_connect_event = true;
        if (s_connect_error == 0 && !s_stopping && s_enabled && s_foreground &&
            s_call == DEMO_GROUP_CALL_NONE) s_wire_connection = connection;
    }
    liot_rtos_exit_critical();
    room_wake();
}
static void room_connection_error(tirtc_conn_t connection, int error) {
    liot_rtos_enter_critical();
    if (!s_media_idle && (connection == s_wire_connection || s_connect_pending)) {
        s_callback_error = error != 0 ? error : TIRTC_E_CONN_REMOTECLOSE;
        s_callback_epoch = s_epoch;
        s_mic_gate = false;
        s_accept_audio = false;
    }
    liot_rtos_exit_critical();
    room_wake();
}
static void room_disconnected(tirtc_conn_t connection) {
    room_connection_error(connection, TIRTC_E_CONN_REMOTECLOSE);
}
static int room_subscribe(tirtc_conn_t connection, uint8_t stream) {
    bool accept;
    liot_rtos_enter_critical();
    accept = !s_stopping && !s_media_idle && stream == ROOM_STREAM &&
             connection == s_wire_connection;
    liot_rtos_exit_critical();
    return accept ? 0 : TIRTC_E_INVALID_HANDLE;
}
static void room_on_audio(tirtc_conn_t connection,
                          const TIRTCFRAMEINFO *frame, void *data) {
    unsigned int slot;
    uint32_t epoch;
    if (frame != NULL && data != NULL)
        ROOM_AUDIO_DIAG(room_audio_diag_receive(connection,frame););
    if (frame == NULL || data == NULL || frame->stream_id != ROOM_STREAM ||
        frame->media != TIRTC_AUDIO_ALAW || frame->flags != TIRTC_AUDIOSAMPLE_8K16B1C ||
        frame->length == 0U || frame->length > ROOM_RX_MAX) return;
    liot_rtos_enter_critical();
    if (!s_accept_audio || connection != s_wire_connection) {
        liot_rtos_exit_critical(); return;
    }
    for (slot=0U; slot<ROOM_RX_SLOTS && s_rx[slot].used; ++slot) {}
    if (slot == ROOM_RX_SLOTS) {
        ROOM_AUDIO_DIAG(if (s_audio_diag.active) ++s_audio_diag.window.drop_slot;);
        ++s_snapshot.rx_dropped; liot_rtos_exit_critical(); return;
    }
    s_rx[slot].used = true;
    ++s_producers;
    epoch = s_epoch;
    liot_rtos_exit_critical();
    memcpy(s_rx[slot].bytes,data,frame->length);
    s_rx[slot].length = (uint16_t)frame->length;
    s_rx[slot].epoch = epoch;
    s_rx[slot].received_ms = liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    if (s_accept_audio && epoch == s_epoch && s_rx_count < ROOM_RX_SLOTS &&
        s_rx_bytes + frame->length <= 4096U) {
        s_rx_ring[s_rx_write] = (uint8_t)slot;
        s_rx_write = (uint8_t)((s_rx_write + 1U) % ROOM_RX_SLOTS);
        ++s_rx_count;
        s_rx_bytes += frame->length;
        ROOM_AUDIO_DIAG(if (s_audio_diag.active) {
            if (s_rx_count>s_audio_diag.window.peak_slots) s_audio_diag.window.peak_slots=s_rx_count;
            if (s_rx_bytes>s_audio_diag.window.peak_bytes) s_audio_diag.window.peak_bytes=s_rx_bytes;
        });
    } else {
        ROOM_AUDIO_DIAG(if (s_audio_diag.active && s_accept_audio && epoch == s_epoch) {
            if (s_rx_count>=ROOM_RX_SLOTS) ++s_audio_diag.window.drop_slot;
            else if (s_rx_bytes+frame->length>4096U) ++s_audio_diag.window.drop_bytes;
        });
        s_rx[slot].used=false; ++s_snapshot.rx_dropped;
    }
    --s_producers;
    liot_rtos_exit_critical();
    room_wake();
}
static void room_on_command(tirtc_conn_t connection, uint32_t command,
                            const void *data, uint32_t length) {
    unsigned int slot;
    uint32_t epoch;
    if (command != ROOM_COMMAND || data == NULL || length == 0U) return;
    liot_rtos_enter_critical();
    if (!s_accept_commands || connection != s_wire_connection) {
        liot_rtos_exit_critical(); return;
    }
    for (slot=0U; slot<ROOM_COMMAND_SLOTS && s_commands[slot].used; ++slot) {}
    if (slot == ROOM_COMMAND_SLOTS || length > ROOM_COMMAND_MAX) {
        ++s_snapshot.rx_dropped;
        if (length>ROOM_COMMAND_MAX) ++s_command_oversize;
        else s_command_gap=true;
        liot_rtos_exit_critical(); room_wake(); return;
    }
    s_commands[slot].used=true;
    ++s_producers;
    epoch=s_epoch;
    liot_rtos_exit_critical();
    memcpy(s_commands[slot].bytes,data,length);
    s_commands[slot].bytes[length]='\0';
    s_commands[slot].length=(uint16_t)length;
    s_commands[slot].epoch=epoch;
    liot_rtos_enter_critical();
    if (s_accept_commands && epoch == s_epoch && s_cmd_count < ROOM_COMMAND_SLOTS &&
        memchr(s_commands[slot].bytes,'\0',length) == NULL) {
        s_cmd_ring[s_cmd_write]=(uint8_t)slot;
        s_cmd_write=(uint8_t)((s_cmd_write+1U)%ROOM_COMMAND_SLOTS);
        ++s_cmd_count;
    } else { s_commands[slot].used=false; ++s_snapshot.rx_dropped; }
    --s_producers;
    liot_rtos_exit_critical();
    room_wake();
}
static const demo_tirtc_listener_t s_listener = {
    .on_conn_error=room_connection_error,
    .on_disconnected=room_disconnected,
    .on_audio=room_on_audio,
    .on_command=room_on_command,
    .on_subscribe_audio=room_subscribe
};

static void room_mqtt(demo_formal_mqtt_message_kind_e kind, const char *type,
                       const char *channel, const cJSON *payload, void *user) {
    uint32_t version;
    const cJSON *expires;
    time_t now;
    (void)channel; (void)user;
    if (kind != DEMO_FORMAL_MQTT_COMMAND || type == NULL ||
        strcmp(type,"room_assignment_changed") != 0) return;
    /* The adapter passes the full object for this top-level Room event.
     * Older adapters may supply no payload; a hint still triggers a query. */
    if (payload == NULL) {
        liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); liot_rtos_exit_critical();
        room_wake(); return;
    }
    if (!room_number(payload,"assignment_version",&version,false)) return;
    expires=cJSON_GetObjectItemCaseSensitive(payload,"expires_at");
    now=time(NULL);
    if (cJSON_IsNumber(expires) && now > 0 && expires->valuedouble <= (double)now) return;
    liot_rtos_enter_critical();
    if (version >= s_assignment.version) s_sync_sequence=room_next(s_sync_sequence);
    liot_rtos_exit_critical();
    room_wake();
}

static cJSON *room_job_body(const room_job_t *job) {
    cJSON *body=cJSON_CreateObject();
    if (job->kind == ROOM_JOB_CREATE || job->kind == ROOM_JOB_JOIN) {
        if (body == NULL || (job->kind == ROOM_JOB_JOIN &&
            cJSON_AddStringToObject(body,"room_code",job->code) == NULL) ||
            (job->password[0] != '\0' && cJSON_AddStringToObject(body,"password",job->password) == NULL)) {
            cJSON_Delete(body); return NULL;
        }
        return body;
    }
    if (job->kind == ROOM_JOB_LEAVE) {
        if (body == NULL || cJSON_AddStringToObject(body,"room_id",job->room_id) == NULL ||
            cJSON_AddNumberToObject(body,"assignment_version",job->version) == NULL) {
            cJSON_Delete(body); return NULL;
        }
        return body;
    }
    if (body == NULL || cJSON_AddStringToObject(body,"room_id",job->room_id) == NULL ||
        cJSON_AddNumberToObject(body,"assignment_version",job->version) == NULL ||
        cJSON_AddStringToObject(body,"session_id",job->session_id) == NULL ||
        (job->kind != ROOM_JOB_TOKEN &&
         cJSON_AddStringToObject(body,"state",job->presence) == NULL)) {
        cJSON_Delete(body); return NULL;
    }
    return body;
}
static bool room_parse_assignment(const cJSON *data, room_assignment_t *assignment) {
    unsigned int i;
    const char *desired=room_string(data,"desired_state");
    if (!cJSON_IsObject(data) ||
        !room_number(data,"assignment_version",&assignment->version,false) ||
        !room_copy(data,"room_id",assignment->room_id,sizeof(assignment->room_id),false) ||
        !room_copy(data,"room_code",assignment->room_code,sizeof(assignment->room_code),false)) return false;
    assignment->closed=strcmp(room_string(data,"state"),"room_closed") == 0;
    assignment->desired=strcmp(desired,"joined") == 0 && !assignment->closed;
    if (!assignment->desired) return desired[0] == '\0' || strcmp(desired,"left") == 0 || assignment->closed;
    if (assignment->version == 0U || assignment->room_id[0] == '\0' ||
        strlen(assignment->room_code) != 6U) return false;
    for (i=0U;i<6U;++i) if (assignment->room_code[i]<'0' || assignment->room_code[i]>'9') return false;
    return true;
}
static void room_execute_http(void) {
    cJSON *body=NULL, *root=NULL;
    const cJSON *data, *item;
    char *json=NULL;
    const char *path;
    int status=0, ret;
    uint32_t heartbeat, lease;
    bool submit;
    room_zero(&s_http_result,sizeof(s_http_result));
    s_http_result.code=ROOM_ERR_PROTOCOL;
    s_http_result.started_ms=liot_rtos_get_running_time();
    if (s_http_job.kind == ROOM_JOB_TOKEN) {
        liot_rtos_enter_critical();
        submit=s_claimed && s_epoch == s_http_job.epoch && !s_stopping &&
               s_enabled && s_foreground && s_call == DEMO_GROUP_CALL_NONE &&
               s_intent == s_http_job.intent &&
               demo_binding_service_context_current(&s_http_job.context);
        liot_rtos_exit_critical();
        if (!submit) { s_http_result.code=TIRTC_E_BUSY; return; }
    }
    if (s_http_job.kind == ROOM_JOB_CONNECT) {
        liot_rtos_enter_critical();
        submit=s_claimed && s_epoch == s_http_job.epoch && !s_stopping &&
               s_enabled && s_foreground && s_call == DEMO_GROUP_CALL_NONE &&
               s_intent == s_http_job.intent &&
               demo_binding_service_context_current(&s_http_job.context);
        s_sdk_submit_busy=submit;
        liot_rtos_exit_critical();
        ret=submit ? demo_tirtc_whip_connect(DEMO_TIRTC_OWNER_GROUP_ROOM,
              s_generation,s_http_job.peer,s_http_job.token,
              room_connect_result,&s_connect_context) : TIRTC_E_BUSY;
        liot_rtos_enter_critical();
        s_sdk_submit_busy=false;
        if (ret != 0 && s_epoch == s_http_job.epoch) s_connect_pending=false;
        liot_rtos_exit_critical();
        s_http_result.code=ret;
        return;
    }
    if (s_http_job.kind != ROOM_JOB_ASSIGNMENT) {
        body=room_job_body(&s_http_job);
        if (body != NULL) json=cJSON_PrintUnformatted(body);
        cJSON_Delete(body);
        if (json == NULL) return;
    }
    path=s_http_job.kind == ROOM_JOB_ASSIGNMENT ? "/v1/call/group/device/assignment" :
         s_http_job.kind == ROOM_JOB_TOKEN ? "/v1/call/group/device/connect-token" :
         s_http_job.kind == ROOM_JOB_CREATE ? "/v1/call/group/device/create" :
         s_http_job.kind == ROOM_JOB_JOIN ? "/v1/call/group/device/join" :
         s_http_job.kind == ROOM_JOB_LEAVE ? "/v1/call/group/device/leave" :
         "/v1/call/group/device/presence";
    ret=demo_binding_service_request_guarded(DEMO_BIND_SERVICE_CALL,
        s_http_job.kind == ROOM_JOB_ASSIGNMENT ? DEMO_BIND_HTTP_GET : DEMO_BIND_HTTP_POST,
        path,json,s_http_response,sizeof(s_http_response),&status,s_http_job.timeout_ms,&s_http_job.context);
    if (json != NULL) { room_zero(json,strlen(json)); cJSON_free(json); }
    if (ret != 0 || status != 200) {
        s_http_result.code=status != 0 && status != 200 ? status : ret != 0 ? ret : ROOM_ERR_PROTOCOL;
        goto done;
    }
    root=room_parse_json(s_http_response,strlen(s_http_response));
    item=cJSON_GetObjectItemCaseSensitive(root,"code");
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)item->valueint) goto done;
    s_http_result.code=item->valueint;
    if (s_http_result.code != 200) goto done;
    data=cJSON_GetObjectItemCaseSensitive(root,"data");
    if (s_http_job.kind == ROOM_JOB_ASSIGNMENT || s_http_job.kind >= ROOM_JOB_CREATE) {
        if (!room_parse_assignment(data,&s_http_result.assignment)) s_http_result.code=ROOM_ERR_PROTOCOL;
    } else if (s_http_job.kind == ROOM_JOB_TOKEN) {
        if (!cJSON_IsObject(data) ||
            !room_copy(data,"peer_id",s_http_result.peer,sizeof(s_http_result.peer),true) ||
            !room_copy(data,"token",s_http_result.token,sizeof(s_http_result.token),true) ||
            !room_number(data,"heartbeat_seconds",&heartbeat,true) ||
            !room_number(data,"lease_seconds",&lease,true) ||
            heartbeat > 3600U || lease > 86400U || lease <= 2U*heartbeat) {
            s_http_result.code=ROOM_ERR_PROTOCOL;
        } else {
            s_http_result.heartbeat_ms=heartbeat*1000U;
            s_http_result.lease_ms=lease*1000U;
            item=cJSON_GetObjectItemCaseSensitive(data,"expires_at");
            if (cJSON_IsNumber(item) && time(NULL)>0 && item->valuedouble <= (double)time(NULL))
                s_http_result.code=ROOM_ERR_LEASE;
        }
    }
done:
    cJSON_Delete(root);
    room_zero(s_http_response,sizeof(s_http_response));
}
static void room_http_task(void *unused) {
    (void)unused;
    for (;;) {
        (void)liot_rtos_semaphore_wait(s_http_wake,LIOT_WAIT_FOREVER);
        if (!s_initialized) continue;
        liot_rtos_enter_critical();
        bool run=s_http_busy && !s_http_done;
        liot_rtos_exit_critical();
        if (!run) continue;
        room_execute_http();
        liot_rtos_enter_critical();
        s_http_done=true;
        liot_rtos_exit_critical();
        room_wake();
    }
}
static bool room_http_available(void) {
    bool available;
    liot_rtos_enter_critical(); available=!s_http_busy; liot_rtos_exit_critical();
    return available;
}
static bool room_schedule(room_job_kind_e kind, const char *presence) {
    if (!room_http_available()) return false;
    room_zero(&s_http_job,sizeof(s_http_job));
    if (kind == ROOM_JOB_TOKEN || kind == ROOM_JOB_PRESENCE) {
        /* Every request for this session keeps its original identity lease. */
        s_http_job.context=s_session_context;
        if (!demo_binding_service_context_current(&s_http_job.context)) return false;
    } else if (!demo_binding_service_context(&s_http_job.context)) return false;
    s_http_job.kind=kind;
    s_http_job.intent=s_seen_intent;
    s_http_job.epoch=s_epoch;
    liot_rtos_enter_critical();
    s_http_job.sync_sequence=s_sync_sequence;
    liot_rtos_exit_critical();
    s_http_job.timeout_ms=ROOM_HTTP_TIMEOUT;
    s_http_job.version=s_assignment.version;
    memcpy(s_http_job.room_id,s_assignment.room_id,sizeof(s_http_job.room_id));
    memcpy(s_http_job.session_id,s_session,sizeof(s_http_job.session_id));
    if (presence != NULL) (void)snprintf(s_http_job.presence,sizeof(s_http_job.presence),"%s",presence);
    liot_rtos_enter_critical();
    s_http_busy=true; s_http_done=false;
    liot_rtos_exit_critical();
    (void)liot_rtos_semaphore_release(s_http_wake);
    return true;
}
static void room_close(demo_group_state_e after, int error, const char *presence) {
    if (s_stopping) return;
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_record_failure_active=false;
#endif
    liot_rtos_enter_critical();
    s_stopping=true;
    s_mic_gate=false; s_mic_dirty=false; s_ptt_until=0U; ++s_ptt_revision;
    s_ui.member_count=0U; s_ui.members_limited=false;
    s_accept_audio=false; s_accept_commands=false;
    liot_rtos_exit_critical();
    s_after_cleanup=after;
    s_mic_retry_at=0U;
    s_cleanup_error=error;
    s_cleanup_sent=false;
    s_stopped_audio=false;
    if (s_token_requested && s_session[0] != '\0' && !s_terminal_pending) {
        room_zero(&s_terminal,sizeof(s_terminal));
        s_terminal.kind=ROOM_JOB_TERMINAL;
        s_terminal.context=s_session_context;
        s_terminal.epoch=s_epoch;
        s_terminal.intent=s_seen_intent;
        s_terminal.version=s_assignment.version;
        s_terminal.timeout_ms=ROOM_HTTP_TIMEOUT;
        memcpy(s_terminal.room_id,s_assignment.room_id,sizeof(s_terminal.room_id));
        memcpy(s_terminal.session_id,s_session,sizeof(s_terminal.session_id));
        (void)snprintf(s_terminal.presence,sizeof(s_terminal.presence),"%s",presence);
        s_terminal_pending=true;
    }
    room_state(after == DEMO_GROUP_SUSPENDED ? DEMO_GROUP_SUSPENDING : DEMO_GROUP_STOPPING,error);
}
static void room_fail(int error) {
    uint32_t delay;
    bool lease_conflict=error == 40921;
    bool permanent=error == 40000 || error == 40300 || error == 40301 ||
        error == ROOM_ERR_PROTOCOL || error == ROOM_ERR_AUDIO;
    if (lease_conflict && !s_conflict_wait) {
        /* A prior device/session can retain the server lease after a lost
         * terminal report. Establish one budget, never extend it per retry. */
        s_conflict_wait=true;
        s_conflict_deadline=liot_rtos_get_running_time()+
            (s_lease_ms != 0U ? s_lease_ms : ROOM_UNKNOWN_LEASE_WAIT);
    }
    ++s_failures;
    s_manual_error=permanent || (lease_conflict ? room_due(s_conflict_deadline) :
        s_failures >= ROOM_MAX_RETRIES);
    delay=1000U << (s_failures > 5U ? 5U : (s_failures-1U));
    if (delay>30000U) delay=30000U;
    s_retry_at=liot_rtos_get_running_time()+delay+(liot_true_rand()%501U);
    if (lease_conflict && !s_manual_error &&
        (int32_t)(s_retry_at-s_conflict_deadline)>0) s_retry_at=s_conflict_deadline;
    room_close(s_manual_error ? DEMO_GROUP_ERROR : DEMO_GROUP_RECONNECTING,error,"connect_failed");
}
static void room_drain_pools(void) {
    liot_rtos_enter_critical();
    if (s_producers == 0U) {
        memset(s_rx,0,sizeof(s_rx)); memset(s_commands,0,sizeof(s_commands));
        s_rx_read=s_rx_write=s_rx_count=0U; s_rx_bytes=0U;
        s_cmd_read=s_cmd_write=s_cmd_count=0U;
    }
    liot_rtos_exit_critical();
}
static bool room_cleanup(void) {
    bool drained;
    int ret;
    if (!s_cleanup_sent) {
        if (s_joined) { (void)room_send_mic(false); (void)room_send_json(room_rpc("leave_room")); }
        s_cleanup_sent=true;
    }
    if (!s_stopped_audio) {
        if (demo_ai_audio_lease_is_valid(&s_audio)) (void)demo_ai_audio_stop(&s_audio);
        s_stopped_audio=true;
    }
    room_drain_pools();
    liot_rtos_enter_critical();
    drained=s_producers == 0U && !s_sdk_submit_busy;
    liot_rtos_exit_critical();
    if (!drained) return false;
    /* Keep the runtime reservation until the codec lease has drained. A new
     * AI/call owner must never claim RTC while ROOM still owns ES8311. */
    if (demo_ai_audio_lease_is_valid(&s_audio) && demo_ai_audio_release(&s_audio) != 0) return false;
    if (s_claimed) {
        (void)demo_tirtc_disconnect(DEMO_TIRTC_OWNER_GROUP_ROOM,s_generation);
        ret=demo_tirtc_session_release(DEMO_TIRTC_OWNER_GROUP_ROOM,s_generation);
        if (ret != 0) return false;
        s_claimed=false;
    }
    s_generation=0U;
    ROOM_AUDIO_DIAG(room_audio_diag_report(true););
    s_ui_generation=room_next(s_ui_generation);
    s_self_participant[0]=0;
    s_play_guard=s_play_continuing=false; s_play_offset=0U;
    s_play_buffering=s_play_partial=false; s_play_fraction=0U;
    s_joined=false; s_join_pending=false; s_token_requested=false; s_waiting_token=false;
    s_heartbeat_at=s_lease_deadline=0U;
    room_zero(s_session,sizeof(s_session));
    room_zero(s_media_session,sizeof(s_media_session));
    room_zero(s_wire_room,sizeof(s_wire_room));
    room_zero(s_capture,sizeof(s_capture)); room_zero(s_alaw,sizeof(s_alaw));
    liot_rtos_enter_critical();
    s_wire_connection=NULL;
    s_connect_event=false; s_callback_error=0;
    s_media_idle=true; s_stopping=false;
    liot_rtos_exit_critical();
    room_state(s_after_cleanup,s_cleanup_error);
    return true;
}

static bool room_match_wire(const char *wire) {
    const char *colon;
    if (wire == NULL || wire[0] == '\0' || strlen(wire)>=sizeof(s_wire_room)) return false;
    if (s_wire_room[0] != '\0') return strcmp(wire,s_wire_room) == 0;
    if (strcmp(wire,s_assignment.room_id) != 0) {
        colon=strchr(wire,':');
        if (colon == NULL || colon == wire || strcmp(colon+1,s_assignment.room_id) != 0) return false;
    }
    /* Pin even an unprefixed ID; later notifications cannot change scope. */
    memcpy(s_wire_room,wire,strlen(wire)+1U);
    return true;
}
static void room_members(const cJSON *params,const cJSON *members) {
    const cJSON *item,*self=cJSON_GetObjectItemCaseSensitive(params,"self");
    unsigned int count=0U,total=0U;
    const char *self_id=room_string(self,"participant_id");
    if(self_id[0]&&strlen(self_id)<sizeof(s_self_participant))
        memcpy(s_self_participant,self_id,strlen(self_id)+1U);
    memset(s_ui.members,0,sizeof(s_ui.members));
    cJSON_ArrayForEach(item,members) {
        const char *id=room_string(item,"participant_id"),*device=room_string(item,"device_id");
        const char *state=room_string(item,"state"),*name=room_string(item,"device_name");
        if(!cJSON_IsObject(item)||id[0]=='\0'||strlen(id)>=sizeof(s_ui.members[0].id))continue;
        if(state[0]&&strcmp(state,"active"))continue;
        ++total;
        if(count<TIRTC_UI_ROOM_MEMBER_MAX) {
            tirtc_ui_room_member_t *out=&s_ui.members[count++];
            memcpy(out->id,id,strlen(id)+1U);
            /* UTF-8 names are bounded at a complete codepoint boundary. */
            if(!name[0])name=device[0]?device:id;
            size_t n=strlen(name);if(n>=sizeof(out->name))n=sizeof(out->name)-1U;
            while(n&&((unsigned char)name[n]&0xc0U)==0x80U)--n;
            memcpy(out->name,name,n);out->name[n]='\0';
            out->self=(s_self_participant[0]&&!strcmp(id,s_self_participant))||
                (device[0]&&!strcmp(device,s_device));
            const char *mic=room_string(item,"mic_state");
            out->speaking=!strcmp(mic,"speaking");
        }
    }
    s_ui.member_count=(uint8_t)count;s_ui.members_limited=total>count;
    s_members_due=liot_rtos_get_running_time()+10000U;
}
static void room_signal_json(const cJSON *root) {
    const cJSON *result, *params, *error, *members;
    const char *method;
    uint32_t id;
    if (!cJSON_IsObject(root) || strcmp(room_string(root,"jsonrpc"),"2.0") != 0) return;
    if (!s_joined && !s_join_pending && room_number(root,"id",&id,true) && id == s_join_id) {
        result=cJSON_GetObjectItemCaseSensitive(root,"result");
        error=cJSON_GetObjectItemCaseSensitive(root,"error");
        if (error != NULL || !cJSON_IsObject(result) ||
            !room_copy(result,"session_id",s_media_session,sizeof(s_media_session),true) ||
            !room_format(cJSON_GetObjectItemCaseSensitive(result,"input_audio")) ||
            !room_format(cJSON_GetObjectItemCaseSensitive(result,"output_audio"))) {
            const cJSON *code=cJSON_GetObjectItemCaseSensitive(error,"code");
            room_fail(cJSON_IsNumber(code) ? code->valueint : ROOM_ERR_PROTOCOL); return;
        }
        if (room_string(result,"room_id")[0] != '\0' && !room_match_wire(room_string(result,"room_id"))) {
            room_fail(ROOM_ERR_PROTOCOL); return;
        }
        if (room_due(s_lease_deadline)) return;
        liot_rtos_enter_critical();
        if (!s_enabled || !s_foreground || s_call != DEMO_GROUP_CALL_NONE ||
            s_stopping || s_intent != s_seen_intent) {
            liot_rtos_exit_critical(); return;
        }
        s_joined=true; s_heartbeat_at=0U;
        s_mic_gate=false; s_mic_dirty=true; s_accept_audio=true;
        liot_rtos_exit_critical();
        s_mic_retry_at=0U;
        room_state(DEMO_GROUP_LISTENING,0);
        (void)room_send_json(room_rpc("get_room_snapshot"));
        s_members_due=liot_rtos_get_running_time()+10000U;
    }
    method=room_string(root,"method");
    params=cJSON_GetObjectItemCaseSensitive(root,"params");
    if (method[0] == '\0' || !cJSON_IsObject(params) || !room_match_wire(room_string(params,"room_id"))) return;
    if (strcmp(method,"room_closed") == 0) {
        s_assignment.closed=true; s_assignment.desired=false;
        room_close(DEMO_GROUP_NO_ROOM,40400,"left");
    } else if (strcmp(method,"room_snapshot") == 0) {
        members=cJSON_GetObjectItemCaseSensitive(params,"participants");
        if (!cJSON_IsArray(members)) {
            room_fail(ROOM_ERR_PROTOCOL); return;
        }
        room_members(params,members);
    } else if (strcmp(method,"participant_joined") == 0 ||
               strcmp(method,"participant_left") == 0 ||
               strcmp(method,"participant_mic_state_changed") == 0) {
        const char *id=room_string(params,"participant_id");
        if (!strcmp(method,"participant_mic_state_changed")) {
            unsigned int i;const char *mic=room_string(params,"mic_state");
            for(i=0U;i<s_ui.member_count;++i)if(!strcmp(id,s_ui.members[i].id))
                s_ui.members[i].speaking=!strcmp(mic,"speaking");
        } else s_members_due=0U;
    }
}
static void room_process_commands(void) {
    uint8_t slot;
    cJSON *root;
    unsigned int count;
    for (count=0U;count<ROOM_COMMAND_SLOTS;++count) {
        liot_rtos_enter_critical();
        if (s_cmd_count == 0U) { liot_rtos_exit_critical(); break; }
        slot=s_cmd_ring[s_cmd_read];
        s_cmd_read=(uint8_t)((s_cmd_read+1U)%ROOM_COMMAND_SLOTS); --s_cmd_count;
        liot_rtos_exit_critical();
        if (!s_stopping && s_commands[slot].epoch == s_epoch) {
            root=room_parse_json(s_commands[slot].bytes,s_commands[slot].length);
            if (root != NULL) room_signal_json(root);
            else room_fail(ROOM_ERR_PROTOCOL);
            cJSON_Delete(root);
        }
        liot_rtos_enter_critical(); s_commands[slot].used=false; liot_rtos_exit_critical();
    }
}
/* Critical section held. Network packets are fragments of one continuous
 * 8 kHz A-law stream, not independent PCM/DMA blocks. Peek without consuming:
 * a BUSY playback submission must preserve exactly the same input samples. */
static uint32_t room_play_peek_locked(uint8_t bytes[160], uint32_t now) {
    uint32_t available=0U, copied=0U;
    unsigned int i;
    while (s_rx_count != 0U) {
        room_audio_slot_t *head=&s_rx[s_rx_ring[s_rx_read]];
        if (s_accept_audio && head->epoch == s_epoch &&
            now-head->received_ms <= ROOM_RX_MAX_AGE) break;
        ROOM_AUDIO_DIAG(if (s_audio_diag.active && s_accept_audio && head->epoch == s_epoch &&
            now-head->received_ms>ROOM_RX_MAX_AGE) ++s_audio_diag.window.drop_age;);
        s_rx_bytes-=head->length; head->used=false;
        s_rx_read=(uint8_t)((s_rx_read+1U)%ROOM_RX_SLOTS); --s_rx_count;
        s_play_offset=0U; ++s_snapshot.rx_dropped;
        s_play_buffering=s_play_partial=false;
    }
    for (i=0U;i<s_rx_count;++i) {
        const room_audio_slot_t *part=&s_rx[s_rx_ring[(s_rx_read+i)%ROOM_RX_SLOTS]];
        uint32_t offset=i == 0U ? s_play_offset : 0U;
        uint32_t length=part->length-offset, take;
        if (part->epoch != s_epoch || now-part->received_ms>ROOM_RX_MAX_AGE) break;
        available+=length;
        take=160U-copied; if (take>length) take=length;
        if (take != 0U) memcpy(bytes+copied,part->bytes+offset,take);
        copied+=take;
    }
    return available;
}
/* Only the Room worker removes slots; callbacks append new slots. */
static void room_play_consume_locked(uint32_t bytes) {
    while (bytes != 0U && s_rx_count != 0U) {
        room_audio_slot_t *head=&s_rx[s_rx_ring[s_rx_read]];
        uint32_t take=head->length-s_play_offset;
        if (take>bytes) take=bytes;
        s_play_offset+=take; bytes-=take;
        if (s_play_offset == head->length) {
            s_rx_bytes-=head->length; head->used=false;
            s_rx_read=(uint8_t)((s_rx_read+1U)%ROOM_RX_SLOTS); --s_rx_count;
            s_play_offset=0U;
        }
    }
}
static void room_playback_tick(void) {
    uint8_t bytes[160];
    uint32_t started=liot_rtos_get_running_time(), now=started, block;
    ROOM_AUDIO_DIAG(room_audio_diag_tick_begin(started););
    /* FINISH is asynchronous. Require both the media-time estimate and the
     * codec completion before beginning another burst with 80 ms silence. */
    if (s_play_continuing && s_play_guard && (int32_t)(now-s_play_tail)>=0 &&
        demo_ai_audio_play_done(&s_audio)) {
        s_play_continuing=false; s_play_buffering=false; s_play_fraction=0U;
    }
    for (block=0U;block<ROOM_PLAY_BLOCKS;++block) {
        uint32_t available, source, samples, fraction, duration, queued, extra=0U, i;
        bool active;
        int ret;
        now=liot_rtos_get_running_time();
        if (now-started>=ROOM_PLAY_WORK_MS) break;
        liot_rtos_enter_critical();
        available=room_play_peek_locked(bytes,now);
        active=s_accept_audio;
        liot_rtos_exit_critical();
        if (!active || available == 0U) {
            s_play_buffering=s_play_partial=false; break;
        }
        if (!s_play_continuing) {
            if (!s_play_buffering) {s_play_buffering=true;s_play_buffer_at=now;}
            if (available<ROOM_PLAY_PREBUFFER_BYTES && now-s_play_buffer_at<ROOM_PLAY_PREBUFFER_MS) break;
        }
        source=available>160U ? 160U : available;
        /* Coalesce short network tails for at most the prebuffer wait. Each
         * A-law sample is duplicated below, including an isolated final
         * sample, so every hardware submission has an even PCM count. */
        if (source>1U) {source&=~1U;s_play_partial=false;}
        else {
            if (!s_play_partial) {
                s_play_partial=true;
                s_play_partial_at=s_play_buffering ? s_play_buffer_at : now;
            }
            if (now-s_play_partial_at<ROOM_PLAY_PREBUFFER_MS) break;
        }
        samples=2U*source;
        fraction=samples+(s_play_continuing ? s_play_fraction : 0U);
        duration=fraction/16U;
        queued=s_play_guard && (int32_t)(s_play_tail-now)>0 ? s_play_tail-now : 0U;
        if (queued+duration+(fraction%16U != 0U ? 1U : 0U)+
            (s_play_continuing ? 0U : DEMO_AI_AUDIO_WARMUP_MS)>ROOM_PLAY_AHEAD) break;
        for (i=source;i>0U;--i) {
            int16_t value=demo_g711_alaw_decode_sample(bytes[i-1U]);
            s_play[(i-1U)*2U]=value;
            s_play[(i-1U)*2U+1U]=value;
        }
        ret=s_play_continuing ? demo_ai_audio_play(&s_audio,s_play,samples) :
            demo_ai_audio_play_warm(&s_audio,s_play,samples,&extra);
        ROOM_AUDIO_DIAG(room_audio_diag_play(ret,samples,!s_play_continuing,extra););
        if (ret != 0) {
            if (ret != DEMO_AI_AUDIO_ERR_BUSY
#if DEMO_AI_AUDIO_AEC_ENABLE
                && ret != DEMO_AI_AUDIO_ERR_PLAY_BUSY
#endif
               ) room_fail(ROOM_ERR_AUDIO);
            break;
        }
        now=liot_rtos_get_running_time();
        if (!s_play_guard || (int32_t)(now-s_play_tail)>0) s_play_tail=now;
        s_play_tail+=extra+duration;
        s_play_fraction=(uint8_t)(fraction%16U);
        s_play_guard=s_play_continuing=true;
        s_play_buffering=s_play_partial=false;
        liot_rtos_enter_critical();room_play_consume_locked(source);liot_rtos_exit_critical();
    }
    ROOM_AUDIO_DIAG(room_audio_diag_tick_end(started););
}
/* Refill one 20 ms 16 kHz PCM block before capture can wait on I2S. The
 * protocol remains 8 kHz A-law, so adjacent PCM samples are averaged. */
static void room_audio_tick(void) {
    unsigned int i;
    uint32_t epoch, revision, now;
    bool on, discard;
    size_t used;
    int ret;
    TIRTCFRAMEINFO frame;
    room_playback_tick();
    if (s_stopping) return;
    now=liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    on=s_mic_gate && s_accept_audio && s_microphone && s_mic_level!=0U;
    epoch=s_epoch; revision=s_ptt_revision;
    discard=s_capture_discard; s_capture_discard=false;
    liot_rtos_exit_critical();
    if(discard) { (void)demo_ai_audio_discard_capture(&s_audio);s_tx_stamp=now-s_session_started; }
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(discard || !on) s_record_failure_active=false;
#endif
    if(!on)return;
    ret=demo_ai_audio_record_20ms(&s_audio,s_capture);
    liot_rtos_enter_critical();
    on=s_mic_gate && s_accept_audio && s_microphone && s_mic_level!=0U &&
       epoch==s_epoch && revision==s_ptt_revision && s_enabled && s_foreground &&
       s_call==DEMO_GROUP_CALL_NONE && !room_due(s_ptt_until);
    liot_rtos_exit_critical();
    if(!on) {
#if DEMO_AI_AUDIO_AEC_ENABLE
        s_record_failure_active=false;
#endif
        room_zero(s_capture,sizeof(s_capture));(void)demo_ai_audio_discard_capture(&s_audio);return;
    }
    if(ret==DEMO_AI_AUDIO_ERR_BUSY)return;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(ret==DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT) {
        now=liot_rtos_get_running_time();
        if(!s_record_failure_active) {
            s_record_failure_active=true; s_record_failure_since=now;
        }
        if((uint32_t)(now-s_record_failure_since)>=ROOM_RECORD_FAILURE_LIMIT_MS)
            room_fail(ROOM_ERR_AUDIO);
        return; /* Retry capture without ending the Room on a transient gap. */
    }
    if(!ret) s_record_failure_active=false;
#endif
    if(ret!=0) {room_fail(ROOM_ERR_AUDIO);return;}
    for(i=0U;i<ROOM_NETWORK_SAMPLES_8K;++i) {
        int32_t mixed=(int32_t)s_capture[i*2U]+(int32_t)s_capture[i*2U+1U];
        s_alaw[i]=demo_g711_alaw_encode_sample((int16_t)(mixed/2));
    }
    ret=demo_tirtc_get_send_buffer_used(DEMO_TIRTC_OWNER_GROUP_ROOM,s_generation,&used);
    if(ret==0 && used>ROOM_SEND_HIGH_WATER)ret=TIRTC_E_BUSY;
    else if(ret==0) {
        memset(&frame,0,sizeof(frame));frame.stream_id=ROOM_STREAM;frame.media=TIRTC_AUDIO_ALAW;
        frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=sizeof(s_alaw);frame.ts=s_tx_stamp;
        liot_rtos_enter_critical();on=s_mic_gate&&s_accept_audio&&epoch==s_epoch&&revision==s_ptt_revision;
        liot_rtos_exit_critical();
        if(!on)return;
        ret=demo_tirtc_send_audio(DEMO_TIRTC_OWNER_GROUP_ROOM,s_generation,&frame,s_alaw);
    }
    s_tx_stamp+=20U;
    if(ret==TIRTC_E_BUSY || ret==0) {
        liot_rtos_enter_critical();++s_snapshot.tx_dropped;liot_rtos_exit_critical();
    } else if(ret<0)room_fail(ret);
}

static void room_consume_http(void) {
    bool done, valid;
    room_job_kind_e kind;
    int code;
    liot_rtos_enter_critical(); done=s_http_busy && s_http_done; liot_rtos_exit_critical();
    if (!done) return;
    kind=s_http_job.kind; code=s_http_result.code;
    valid=demo_binding_service_context_current(&s_http_job.context) && s_http_job.intent == s_seen_intent && s_http_job.epoch == s_epoch && !s_stopping;
    TIRTC_LOG_DEBUG("[ROOM] service kind=%u code=%d epoch=%u\r\n",(unsigned int)kind,code,(unsigned int)s_http_job.epoch);
    /* Consume before clearing the mailbox; helper cannot overwrite it yet. */
    if (kind >= ROOM_JOB_CREATE) {
        if (code == 200 && demo_binding_service_context_current(&s_http_job.context)) {
            s_control_error=0;
            s_assignment_known=true;
            /* Device create/join/leave responses are authoritative assignment snapshots. */
            if (s_http_result.assignment.version >= s_assignment.version) {
                liot_rtos_enter_critical();
                s_assignment=s_http_result.assignment;
                memcpy(s_snapshot.room_code,s_assignment.room_code,sizeof(s_snapshot.room_code));
                s_ui_generation=room_next(s_ui_generation);
                liot_rtos_exit_critical();
            }
            s_manual_error=false; s_failures=0U; s_retry_at=0U; s_conflict_wait=false;
            liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); liot_rtos_exit_critical();
            s_deadline=liot_rtos_get_running_time()+ROOM_RESOURCE_TIMEOUT;
            if (kind != ROOM_JOB_LEAVE && s_http_job.intent == s_intent && room_allowed())
                (void)tirtc_ui_request_page(TIRTC_PAGE_ROOM);
        } else s_control_error=demo_binding_service_context_current(&s_http_job.context) ? code : ROOM_ERR_OFFLINE;
        /* A lost response can follow a committed server mutation. Reconcile
         * once through assignment before allowing an uncertain local retry. */
        liot_rtos_enter_critical();
        s_sync_sequence=room_next(s_sync_sequence);s_control_pending=false;
        liot_rtos_exit_critical();
        room_state(s_assignment.desired ? DEMO_GROUP_WAIT_RESOURCE : DEMO_GROUP_NO_ROOM,code == 200 ? 0 : code);
    } else if (kind == ROOM_JOB_ASSIGNMENT && demo_binding_service_context_current(&s_http_job.context) && s_http_job.intent == s_seen_intent && !s_stopping) {
        if (code == 200) {
            s_assignment_known=true;
            /* A stale response still completes this sync request. Only a new
             * hint/periodic sync should query again, never a busy retry loop. */
            s_sync_done=s_http_job.sync_sequence;
            bool changed=s_http_result.assignment.version != s_assignment.version ||
                strcmp(s_http_result.assignment.room_id,s_assignment.room_id) != 0 ||
                s_http_result.assignment.desired != s_assignment.desired;
            if (s_http_result.assignment.version >= s_assignment.version) {
                if (changed && !s_media_idle) {
                    /* Capture the old tuple before replacing assignment. */
                    room_close(DEMO_GROUP_RECONNECTING,0,"left");
                    s_retry_at=liot_rtos_get_running_time();
                }
                liot_rtos_enter_critical();
                s_assignment=s_http_result.assignment;
                memcpy(s_snapshot.room_code,s_assignment.room_code,sizeof(s_snapshot.room_code));
                liot_rtos_exit_critical();
                if (changed) { s_control_error=0; s_ui_generation=room_next(s_ui_generation); s_manual_error=false; s_failures=0U; s_conflict_wait=false; }
                s_sync_done=s_http_job.sync_sequence;
                if (s_media_idle && !s_stopping && s_enabled && !s_manual_error) room_state(
                    s_assignment.desired ? DEMO_GROUP_WAIT_RESOURCE : DEMO_GROUP_NO_ROOM,
                    s_assignment.closed ? 40400 : 0);
                s_deadline=liot_rtos_get_running_time()+ROOM_RESOURCE_TIMEOUT;
            }
        } else if (s_enabled && s_media_idle) {
            s_sync_done=s_http_job.sync_sequence;
            room_fail(code);
        } else {
            /* A background failure does not retry every 20/100 ms and cannot
             * monopolize the shared HTTP client. The next hint/online edge
             * (or enabled periodic sync) starts a fresh attempt. */
            s_sync_done=s_http_job.sync_sequence;
        }
    } else if (valid && kind == ROOM_JOB_TOKEN) {
        s_waiting_token=false;
        if (code != 200) room_fail(code);
        else if (room_allowed() && s_claimed) {
            s_conflict_wait=false;
            s_heartbeat_ms=s_http_result.heartbeat_ms;
            s_lease_ms=s_http_result.lease_ms;
            s_lease_deadline=s_http_result.started_ms+s_lease_ms;
            if (room_due(s_lease_deadline)) room_fail(ROOM_ERR_LEASE);
            else {
                /* Reuse mailbox for SDK submission only after all token
                 * fields have been copied. Keep it busy across this handoff. */
                memcpy(s_http_job.peer,s_http_result.peer,sizeof(s_http_job.peer));
                memcpy(s_http_job.token,s_http_result.token,sizeof(s_http_job.token));
                s_http_job.kind=ROOM_JOB_CONNECT;
                s_connect_context.epoch=s_epoch;
                liot_rtos_enter_critical();
                s_connect_pending=true; s_connect_event=false;
                s_http_done=false;
                liot_rtos_exit_critical();
                room_zero(&s_http_result,sizeof(s_http_result));
                s_deadline=liot_rtos_get_running_time()+ROOM_CONNECT_TIMEOUT;
                room_state(DEMO_GROUP_CONNECTING,0);
                (void)liot_rtos_semaphore_release(s_http_wake);
                return;
            }
        }
    } else if (valid && kind == ROOM_JOB_CONNECT && code != 0) room_fail(code);
    else if (valid && kind == ROOM_JOB_PRESENCE && s_joined) {
        if (code == 200) {
            s_lease_deadline=s_http_result.started_ms+s_lease_ms;
            s_heartbeat_at=s_http_result.started_ms+s_heartbeat_ms;
            s_failures=0U;
        } else if (code == 401 || code == 40300 || code == 40921 || code == 40400) room_fail(code);
        else s_heartbeat_at=liot_rtos_get_running_time()+1000U;
    }
    room_zero(&s_http_job,sizeof(s_http_job)); room_zero(&s_http_result,sizeof(s_http_result));
    liot_rtos_enter_critical(); s_http_busy=false; s_http_done=false; liot_rtos_exit_critical();
}
static void room_try_terminal(void) {
    if (!s_terminal_pending || !room_http_available()) return;
    s_http_job=s_terminal;
    room_zero(&s_terminal,sizeof(s_terminal)); s_terminal_pending=false;
    liot_rtos_enter_critical(); s_http_busy=true; s_http_done=false; liot_rtos_exit_critical();
    (void)liot_rtos_semaphore_release(s_http_wake);
}
static bool room_start_session(void) {
    demo_binding_tirtc_identity_t identity;
    uint8_t speaker, mic;
    int ret;
    if (!room_allowed() || !room_http_available() || s_terminal_pending || s_control_pending) return false;
    /* session_claim owns its own admission transaction and rejects callers
     * holding the UI admission gate. Mark local preparation busy before the
     * claim so a concurrent incoming call cannot observe a false idle gap. */
    liot_rtos_enter_critical();
    if (!s_enabled || !s_foreground || s_call != DEMO_GROUP_CALL_NONE || s_intent != s_seen_intent ||
        tirtc_calls_has_session() || tirtc_ai_has_session() || tirtc_remote_has_session()) {
        liot_rtos_exit_critical(); return false;
    }
    s_media_idle=false;
    liot_rtos_exit_critical();
    ret=demo_tirtc_session_claim(DEMO_TIRTC_OWNER_GROUP_ROOM,&s_generation);
    if (ret == 0) {
        liot_rtos_enter_critical();
        s_claimed=true;
        s_epoch=room_next(s_epoch);
        s_ui_generation=room_next(s_ui_generation);
        speaker=s_speaker; mic=s_mic_level;
        liot_rtos_exit_critical();
    }
    if (ret != 0) {
        liot_rtos_enter_critical(); s_media_idle=true; liot_rtos_exit_critical();
        return false;
    }
    ROOM_AUDIO_DIAG(room_audio_diag_begin(););
    if (!room_allowed()) { room_close(DEMO_GROUP_SUSPENDED,0,"suspended"); return true; }
    s_token_requested=false; s_joined=false; s_join_pending=false;
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_record_failure_active=false;
#endif
    if (!demo_binding_service_context(&s_session_context)) { room_fail(ROOM_ERR_OFFLINE); return true; }
    s_connect_event=false; s_callback_error=0; s_wire_room[0]='\0';
    memset(&identity,0,sizeof(identity));
    ret=demo_binding_get_tirtc_identity(&identity);
    if (ret == 0) memcpy(s_device,identity.device_id,sizeof(s_device));
    room_zero(&identity,sizeof(identity));
    if (ret != 0 || s_device[0] == '\0') { room_fail(ROOM_ERR_OFFLINE); return true; }
    ret=demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_GROUP_ROOM,&s_audio);
    if (ret == 0) ret=demo_ai_audio_prepare_session(&s_audio,speaker,mic);
    if (ret == 0) {
        uint32_t extra=0U;
        ret=demo_ai_audio_prewarm_session(&s_audio,&extra);
        s_play_tail=liot_rtos_get_running_time()+extra; s_play_guard=extra != 0U;
        s_play_continuing=false; s_play_offset=0U; s_capture_discard=true;
        s_play_buffering=s_play_partial=false; s_play_fraction=0U;
    }
    if (ret != 0) { room_fail(ROOM_ERR_AUDIO); return true; }
    if (!room_allowed()) { room_close(DEMO_GROUP_SUSPENDED,0,"suspended"); return true; }
    (void)snprintf(s_session,sizeof(s_session),"%08x%08x%08x%08x",
        (unsigned int)liot_true_rand(),(unsigned int)liot_true_rand(),
        (unsigned int)liot_true_rand(),(unsigned int)liot_true_rand());
    s_session_started=liot_rtos_get_running_time(); s_tx_stamp=0U;
    s_join_id=room_next(s_join_id);
    s_token_requested=true; s_waiting_token=true;
    room_state(DEMO_GROUP_SYNCING,0);
    if (!room_schedule(ROOM_JOB_TOKEN,NULL)) room_fail(TIRTC_E_BUSY);
    return true;
}
static void room_join_tick(void) {
    uint8_t speaker, mic;
    bool allowed, levels;
    int ret;
    if (!s_join_pending || s_stopping) return;
    liot_rtos_enter_critical();
    levels=s_levels_dirty; s_levels_dirty=false;
    speaker=s_speaker; mic=s_mic_level;
    liot_rtos_exit_critical();
    /* Settings may change while WHIP connects or the cold preamble drains.
     * Commit them before admitting speech. GROUP keeps its clock-prewarm
     * credit across speaker changes, including a muted start. */
    if (levels) {
        ret=demo_ai_audio_set_levels(&s_audio,speaker,mic);
        if (ret == DEMO_AI_AUDIO_ERR_BUSY) {
            liot_rtos_enter_critical(); s_levels_dirty=true; liot_rtos_exit_critical();
            return;
        }
        if (ret != 0) { room_fail(ROOM_ERR_AUDIO); return; }
    }
    /* Do not admit server audio behind a 1200 ms cold preamble: its 500 ms
     * receive-age limit and 240 ms playback-ahead bound would discard the
     * opening words. Wait before join, not by retaining older voice frames.
     * FINISH is advisory and may arrive early or late; use the media tail. */
    if (s_play_guard && !room_due(s_play_tail)) return;
    liot_rtos_enter_critical();
    allowed=s_enabled && s_foreground && s_call == DEMO_GROUP_CALL_NONE &&
        !s_stopping && s_intent == s_seen_intent;
    liot_rtos_exit_critical();
    if (!allowed) return;
    ret=room_send_join();
    if (ret < 0) { room_fail(ret); return; }
    s_join_pending=false;
    s_deadline=liot_rtos_get_running_time()+ROOM_JOIN_TIMEOUT;
}
/* One nonblocking owner iteration, also used by host fault-injection tests.
 * The only naturally paced media operation is one 20 ms capture. */
static uint32_t room_step(void) {
    demo_binding_snapshot_t binding;
        bool enabled, foreground, has_call, suspend_requested, ready, event, on, dirty, levels, command_gap;
        uint32_t command_oversize;
        uint32_t intent, sync;
        int error;
        uint8_t speaker, mic;
        if (!s_initialized) return 100U;
        room_foreground_tick();
        liot_rtos_enter_critical();
        enabled=s_enabled; foreground=s_foreground; has_call=s_call != DEMO_GROUP_CALL_NONE;
        suspend_requested=s_suspend_requested; s_suspend_requested=false;
        intent=s_intent; sync=s_sync_sequence;
        liot_rtos_exit_critical();
        demo_binding_get_snapshot(&binding);
        if (binding.state == DEMO_BIND_BOUND) s_bound_seen=true;
        else if (s_bound_seen && (binding.state == DEMO_BIND_RESTARTING ||
                 binding.state == DEMO_BIND_REPORTING || binding.state == DEMO_BIND_WAIT_GRANT)) {
            /* VERIFYING/ERROR/WAIT_NETWORK/DISCOVERING are normal recovery
             * states. Clear intent only after confirmed unbind/reprovision. */
            s_bound_seen=false; demo_group_intercom_exit(); enabled=false;
            if (!s_media_idle && !s_stopping) room_close(DEMO_GROUP_IDLE,0,"left");
            liot_rtos_enter_critical();
            s_intent=room_next(s_intent);
            memset(&s_assignment,0,sizeof(s_assignment));
            memset(s_snapshot.room_code,0,sizeof(s_snapshot.room_code));
            intent=s_intent;
            liot_rtos_exit_critical();
        }
        ready=binding.state == DEMO_BIND_BOUND && tirtc_network_is_ready() &&
              demo_formal_mqtt_is_online() && demo_tirtc_is_ready();
        if (intent != s_seen_intent) {
            s_seen_intent=intent;
            s_manual_error=false; s_failures=0U; s_retry_at=0U; s_conflict_wait=false;
            if (!s_media_idle && !s_stopping) room_close(DEMO_GROUP_IDLE,0,enabled ? "connect_failed" : "left");
            if (enabled) {
                liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); sync=s_sync_sequence; liot_rtos_exit_critical();
                s_deadline=liot_rtos_get_running_time()+ROOM_RESOURCE_TIMEOUT;
            }
        }
        if (ready && !s_transport_was_ready) {
            liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); sync=s_sync_sequence; liot_rtos_exit_critical();
            s_sync_at=liot_rtos_get_running_time()+ROOM_SYNC_INTERVAL;
            if (!s_manual_error) s_retry_at=0U;
        }
        s_transport_was_ready=ready;
        if ((!enabled || has_call || !foreground || suspend_requested || !ready) && !s_media_idle && !s_stopping)
            room_close(!enabled ? DEMO_GROUP_IDLE : has_call || !foreground || suspend_requested ? DEMO_GROUP_SUSPENDED : DEMO_GROUP_RECONNECTING,
                ready || !enabled || has_call || suspend_requested ? 0 : ROOM_ERR_OFFLINE,
                !enabled ? "left" : has_call || !foreground || suspend_requested ? "suspended" : "connect_failed");
        if (!s_media_idle && !s_stopping &&
            !demo_binding_service_context_current(&s_session_context))
            room_close(DEMO_GROUP_RECONNECTING,ROOM_ERR_OFFLINE,"connect_failed");
        room_consume_http();
        if (s_stopping) (void)room_cleanup();
        room_try_terminal();
        if (s_stopping) return 20U;
        room_control_tick();
        if (s_control_pending) return 20U;
        if (!enabled) {
            room_state(DEMO_GROUP_IDLE,0);
            /* Background sync is coalesced and never claims media. */
            if (ready && sync != s_sync_done && !s_terminal_pending) {
                demo_tirtc_connection_snapshot_t connection;
                demo_tirtc_get_connection_snapshot(&connection);
                /* Background notifications must not queue behind active calls
                 * and delay their next token request on the HTTP singleton. */
                if (!has_call && connection.owner == DEMO_TIRTC_OWNER_NONE &&
                    !connection.connect_pending && !connection.disconnect_pending &&
                    !connection.connect_callback_pending && !connection.expected_incoming &&
                    connection.connection_users == 0U)
                    (void)room_schedule(ROOM_JOB_ASSIGNMENT,NULL);
            }
            return 100U;
        }
        if (has_call || !foreground) { room_state(DEMO_GROUP_SUSPENDED,0); return 50U; }
        if (!ready) { room_state(DEMO_GROUP_RECONNECTING,ROOM_ERR_OFFLINE); return 100U; }
        if (room_due(s_sync_at)) {
            liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); sync=s_sync_sequence; liot_rtos_exit_critical();
            s_sync_at=liot_rtos_get_running_time()+ROOM_SYNC_INTERVAL;
        }
        liot_rtos_enter_critical();
        command_gap=s_command_gap; s_command_gap=false;
        command_oversize=s_command_oversize; s_command_oversize=0U;
        event=s_connect_event; error=s_connect_error; s_connect_event=false;
        if (s_callback_error != 0 && s_callback_epoch == s_epoch) { error=s_callback_error; event=true; s_callback_error=0; }
        liot_rtos_exit_critical();
        if (command_oversize != 0U) {
            s_ui.members_limited=true;
            liot_trace("[ROOM] oversized commands=%u; bounded snapshot omitted, resync assignment\r\n",(unsigned int)command_oversize);
            liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); sync=s_sync_sequence; liot_rtos_exit_critical();
        }
        if (command_gap && !s_media_idle) {
            /* The lost frame might be room_closed. A new authenticated
             * session is safer than renewing an uncertain media session. */
            liot_trace("[ROOM] command queue overflow; reconnecting\r\n");
            room_fail(TIRTC_E_BUSY); return 0U;
        }
        if (event && !s_media_idle) {
            if (error != 0) room_fail(error);
            else {
                liot_rtos_enter_critical();
                bool allow=s_enabled && s_foreground && s_call == DEMO_GROUP_CALL_NONE &&
                    !s_stopping && s_intent == s_seen_intent;
                s_accept_commands=allow;
                liot_rtos_exit_critical();
                if (!allow) return 0U;
                error=demo_tirtc_subscribe_audio(DEMO_TIRTC_OWNER_GROUP_ROOM,s_generation,ROOM_STREAM);
                if (error < 0) room_fail(error);
                else { s_join_pending=true; room_state(DEMO_GROUP_JOINING,0); }
            }
        }
        room_join_tick();
        room_process_commands();
        if (s_stopping) return 0U;
        if (!s_media_idle) {
            if (s_lease_deadline != 0U && room_due(s_lease_deadline)) { room_fail(ROOM_ERR_LEASE); return 0U; }
            if (!s_joined && !s_waiting_token && room_due(s_deadline)) { room_fail(TIRTC_E_TIMEOUTED); return 0U; }
            if (s_joined) {
                liot_rtos_enter_critical();
                on=s_mic_gate;
                dirty=s_mic_dirty && (s_mic_retry_at == 0U || room_due(s_mic_retry_at));
                if (dirty) s_mic_dirty=false;
                levels=s_levels_dirty; s_levels_dirty=false; speaker=s_speaker; mic=s_mic_level;
                liot_rtos_exit_critical();
                if (levels) {
                    error=demo_ai_audio_set_levels(&s_audio,speaker,mic);
                    if (error == DEMO_AI_AUDIO_ERR_BUSY) {
                        liot_rtos_enter_critical();
                        s_levels_dirty=true;
                        if (dirty) s_mic_dirty=true;
                        liot_rtos_exit_critical();
                        return 20U;
                    }
                    if (error != 0) { room_fail(ROOM_ERR_AUDIO); return 0U; }
                }
                if (dirty) {
                    /* State notification failure must not interrupt audio.
                     * Retry the latest gate, at most once per second, and
                     * preserve any key update made during the send. */
                    error=room_send_mic(on);
                    s_mic_retry_at=error < 0 ? liot_rtos_get_running_time()+ROOM_MIC_RETRY_INTERVAL : 0U;
                    if (error < 0) {
                        liot_rtos_enter_critical(); s_mic_dirty=true; liot_rtos_exit_critical();
                    }
                }
                room_state(on ? DEMO_GROUP_MIC_ON : DEMO_GROUP_LISTENING,0);
                if (s_heartbeat_at == 0U || room_due(s_heartbeat_at)) (void)room_schedule(ROOM_JOB_PRESENCE,"joined");
                if (sync != s_sync_done && room_http_available()) (void)room_schedule(ROOM_JOB_ASSIGNMENT,NULL);
                if (room_due(s_members_due)) {
                    (void)room_send_json(room_rpc("get_room_snapshot"));
                    s_members_due=liot_rtos_get_running_time()+10000U;
                }
                room_audio_tick();
                if (on) return 0U;
            }
        } else if (s_manual_error && sync != s_sync_done) {
            /* A real server relationship change can recover a terminal
             * error; the same assignment cannot silently retry bad media. */
            (void)room_schedule(ROOM_JOB_ASSIGNMENT,NULL);
        } else if (!s_manual_error && (s_retry_at == 0U || room_due(s_retry_at))) {
            if (s_retry_at != 0U) {
                s_retry_at=0U;
                liot_rtos_enter_critical(); s_sync_sequence=room_next(s_sync_sequence); sync=s_sync_sequence; liot_rtos_exit_critical();
            }
            if (sync != s_sync_done) {
                if (room_schedule(ROOM_JOB_ASSIGNMENT,NULL)) room_state(DEMO_GROUP_SYNCING,0);
            } else if (!s_assignment.desired) room_state(DEMO_GROUP_NO_ROOM,s_assignment.closed ? 40400 : 0);
            else if (!room_start_session()) {
                if (room_due(s_deadline)) { s_manual_error=true; room_state(DEMO_GROUP_ERROR,TIRTC_E_BUSY); }
                else room_state(DEMO_GROUP_WAIT_RESOURCE,0);
            }
        }
        return 20U;
}
static void room_task(void *unused) {
    (void)unused;
    for (;;) {
        uint32_t delay=room_step();
        room_publish();
        ROOM_AUDIO_DIAG(room_audio_diag_report(false););
        if (delay != 0U) (void)liot_rtos_semaphore_wait(s_wake,delay);
    }
}

int demo_group_intercom_init(void) {
    liot_task_t task=NULL;
    if (s_initialized) return 0;
    /* The app retries serially. Preserve every successfully created resource:
     * a partial start may already have a live HTTP worker waiting on its sem. */
    if (!s_wake && (liot_rtos_semaphore_create(&s_wake,0U) != LIOT_OSI_SUCCESS || !s_wake)) goto failed;
    if (!s_http_wake && (liot_rtos_semaphore_create(&s_http_wake,0U) != LIOT_OSI_SUCCESS || !s_http_wake)) goto failed;
    if (!s_registered) {
        if (demo_formal_mqtt_register_handler(room_mqtt,NULL) != 0) goto failed;
        demo_tirtc_set_feature_listener(DEMO_TIRTC_FEATURE_GROUP_ROOM,&s_listener);
        s_registered=true;
    }
    if (!s_http_task) {
        if (liot_rtos_task_create(&task,12U*1024U,LIOT_APP_TASK_PRIORITY,
                "room_http",room_http_task,NULL) != LIOT_OSI_SUCCESS || !task) goto failed;
        s_http_task=task;
    }
    if (!s_room_task) {
        task=NULL;
        if (liot_rtos_task_create(&task,24U*1024U,LIOT_APP_TASK_PRIORITY,
                "group_room",room_task,NULL) != LIOT_OSI_SUCCESS || !task) goto failed;
        s_room_task=task;
    }
    liot_rtos_enter_critical();
    s_snapshot.state=DEMO_GROUP_IDLE; s_snapshot.error=0;
    s_initialized=true;
    liot_rtos_exit_critical();
    room_wake();
    liot_trace("[ROOM] ready; media stack=24576 service stack=12288 rx=16x640 command=2x16384\r\n");
    return 0;
failed:
    /* The Room worker has not been created yet. Publish its startup failure
     * here so the page does not show an endless initial sync. */
    room_state(DEMO_GROUP_ERROR,ROOM_ERR_INIT);
    room_publish();
    liot_trace("[ROOM] init failed; other features remain available; retry pending\r\n");
    return ROOM_ERR_INIT;
}
void demo_group_intercom_enter(void) {
    if (!s_initialized) return;
    liot_rtos_enter_critical();
    if (!s_enabled) { s_enabled=true; s_intent=room_next(s_intent); }
    s_mic_gate=false;
    liot_rtos_exit_critical(); room_wake();
}
void demo_group_intercom_exit(void) {
    liot_rtos_enter_critical();
    if (s_enabled) { s_enabled=false; s_intent=room_next(s_intent); }
    s_mic_gate=false; s_accept_audio=false; s_accept_commands=false;
    liot_rtos_exit_critical(); room_wake();
}
void demo_group_intercom_retry(void) {
    if (!s_initialized) return;
    liot_rtos_enter_critical();
    if (s_enabled) s_intent=room_next(s_intent);
    s_mic_gate=false;
    liot_rtos_exit_critical(); room_wake();
}
void demo_group_intercom_toggle_mic(void) {
    uint32_t generation;
    liot_rtos_enter_critical(); generation=s_generation; liot_rtos_exit_critical();
    (void)demo_group_intercom_toggle_mic_for_generation(generation);
}
bool demo_group_intercom_toggle_mic_for_generation(uint32_t generation) {
    bool changed=false;
    liot_rtos_enter_critical();
    if (generation != 0U && generation == s_generation && s_enabled && s_foreground &&
        s_call == DEMO_GROUP_CALL_NONE && s_accept_audio && !s_stopping) {
        s_mic_gate=!s_mic_gate; s_mic_dirty=true; changed=true;
    }
    liot_rtos_exit_critical(); room_wake(); return changed;
}
void demo_group_intercom_set_foreground_allowed(bool allowed) {
    liot_rtos_enter_critical();
    if (s_foreground != allowed) {
        s_foreground=allowed;
        if (allowed) s_sync_sequence=room_next(s_sync_sequence);
        else {
            s_suspend_requested=true;
            s_mic_gate=false; s_accept_audio=false; s_accept_commands=false;
        }
    }
    liot_rtos_exit_critical(); room_wake();
}
void demo_group_intercom_set_audio_levels(uint8_t speaker, uint8_t mic) {

    if (speaker>10U) speaker=10U;

    if (mic>10U) mic=10U;
    liot_rtos_enter_critical();
    s_speaker=speaker; s_mic_level=mic; s_levels_dirty=true;
    liot_rtos_exit_critical(); room_wake();
}
bool demo_group_intercom_is_idle(void) {
    bool idle;
    liot_rtos_enter_critical(); idle=s_media_idle && s_producers == 0U && !s_sdk_submit_busy; liot_rtos_exit_critical();
    return idle;
}
bool demo_group_intercom_is_enabled(void) {
    bool enabled;
    liot_rtos_enter_critical(); enabled=s_enabled; liot_rtos_exit_critical(); return enabled;
}
void demo_group_intercom_get_snapshot(demo_group_snapshot_t *out) {
    if (out == NULL) return;
    liot_rtos_enter_critical();
    *out=s_snapshot; out->enabled=s_enabled; out->mic_on=s_mic_gate;
    out->generation=s_accept_audio ? s_generation : 0U;
    liot_rtos_exit_critical();
}
int demo_group_intercom_reserve_call(demo_group_call_e caller,uint32_t *ticket) {
    int ret;
    if (ticket == NULL || caller == DEMO_GROUP_CALL_NONE || caller>DEMO_GROUP_CALL_DEVICE) return -1;
    *ticket=0U;
    liot_rtos_enter_critical();
    if (s_call != DEMO_GROUP_CALL_NONE) ret=-1;
    else if (!s_initialized || (!s_enabled && s_media_idle)) ret=0;
    else {
        s_call=caller; s_ticket_sequence=room_next(s_ticket_sequence);
        s_call_ticket=s_ticket_sequence; *ticket=s_call_ticket;
        /* A quick cancel may release this ticket before the owner runs.
         * Keep the stop request so closed media gates cannot strand a join. */
        s_suspend_requested=true;
        s_mic_gate=false; s_accept_audio=false; s_accept_commands=false;
        ret=1;
    }
    liot_rtos_exit_critical(); room_wake(); return ret;
}
bool demo_group_intercom_call_ready(demo_group_call_e caller,uint32_t ticket) {
    bool ready;
    liot_rtos_enter_critical();
    ready=ticket != 0U && s_call == caller && s_call_ticket == ticket &&
          s_media_idle && s_producers == 0U && !s_sdk_submit_busy;
    liot_rtos_exit_critical(); return ready;
}
void demo_group_intercom_release_call(demo_group_call_e caller,uint32_t ticket) {
    liot_rtos_enter_critical();
    if (ticket != 0U && s_call == caller && s_call_ticket == ticket) {
        s_call=DEMO_GROUP_CALL_NONE; s_call_ticket=0U;
        s_sync_sequence=room_next(s_sync_sequence);
    }
    liot_rtos_exit_critical(); room_wake();
}
bool demo_group_intercom_has_call(void) {
    bool occupied;
    liot_rtos_enter_critical(); occupied=s_call != DEMO_GROUP_CALL_NONE; liot_rtos_exit_critical(); return occupied;
}

static bool room_digits(const char *text,size_t n) {
    size_t i;if(text==NULL||strlen(text)!=n)return false;
    for(i=0U;i<n;++i)if(text[i]<'0'||text[i]>'9')return false;
    return true;
}
static bool room_generation(const char *text,uint32_t *value) {
    uint32_t n=0U;size_t i,len=strlen(text);
    if(len==0U||len>10U)return false;
    for(i=0U;i<len;++i){unsigned int d=(unsigned int)(text[i]-'0');
        if (d > 9U || n > (UINT32_MAX - d) / 10U) return false;
        n = n * 10U + d;
    }
    *value=n;return n!=0U;
}
static void room_gate_off_locked(void) {
    if(s_mic_gate){s_mic_gate=false;s_mic_dirty=true;}
    s_ptt_until=0U;s_capture_discard=true;s_ptt_revision=room_next(s_ptt_revision);
}
static void room_foreground_tick(void) {
    tirtc_ui_page_t page;
    bool shown=tirtc_ui_get_rendered_page(&page) &&
        (page==TIRTC_PAGE_ROOM||page==TIRTC_PAGE_ROOM_FORM);
    bool higher=tirtc_calls_has_session()||tirtc_ai_has_session()||tirtc_remote_has_session();
    liot_rtos_enter_critical();
    /* OPEN is emitted before rendering. Retain page intent until a real room
     * refresh appears; a temporary call does not erase that intent. */
    bool allowed=s_enabled&&shown&&!higher;
    if(s_foreground!=allowed) {
        s_foreground=allowed;
        if(allowed)s_sync_sequence=room_next(s_sync_sequence);
        else {s_suspend_requested=true;room_gate_off_locked();s_accept_audio=false;s_accept_commands=false;}
    }
    if(s_mic_gate && (!s_microphone||!s_mic_level||room_due(s_ptt_until)))room_gate_off_locked();
    liot_rtos_exit_critical();
}

static const char *room_error_text(int error) {
    switch(error){
    case 40000:return "房间号或密码格式不正确";
    case 401:return "设备登录已过期，请稍后刷新";
    case 40300:case 40301:return "密码错误或无权加入房间";
    case 40320:return "房间密码错误，请重新输入";
    case 42920:return "密码尝试过多，请稍后再试";
    case 40400:return "房间不存在或已关闭";
    case 40920:return "房间已满，请稍后重试";
    case 40921:return "等待上次连接释放，请稍后重试";
    case 40922:return "房间关系已改变，请刷新";
    case 40923:return "已在其他房间，请先退出";
    case ROOM_ERR_INIT:return "对讲服务启动失败，稍后自动重试";
    case ROOM_ERR_OFFLINE:return "网络未就绪，恢复后自动连接";
    case ROOM_ERR_AUDIO:return "音频启动失败，请刷新重试";
    case ROOM_ERR_PROTOCOL:return "房间协议或音频格式不匹配";
    case ROOM_ERR_LEASE:return "房间连接租约过期，正在重连";
    default:return "连接未成功，请检查网络后刷新";
    }
}
static void room_publish(void) {
    tirtc_ui_room_t next;
    demo_group_state_e state;
    bool connected,pending,foreground,enabled;
    int error;
    memset(&next,0,sizeof(next));
    liot_rtos_enter_critical();
    next=s_ui;state=s_snapshot.state;error=s_control_error?s_control_error:s_snapshot.error;
    connected=s_joined&&s_accept_audio&&!s_stopping;
    pending=s_control_pending;foreground=s_foreground;enabled=s_enabled;
    next.mic_on=connected&&s_mic_gate;
    next.generation=s_ui_generation;
    next.known=s_assignment_known;next.assigned=s_assignment.desired;
    memcpy(next.code,s_assignment.room_code,sizeof(s_assignment.room_code));
    liot_rtos_exit_critical();
    next.connected=connected;
    /* The server broadcasts mic changes to peers. Our own row follows PTT
     * immediately, including release, without waiting for a periodic snapshot. */
    for (unsigned int i=0U; i<next.member_count; ++i)
        if (next.members[i].self) next.members[i].speaking=next.mic_on;
    next.busy=pending||s_stopping||(!s_media_idle&&!connected)||
        (state==DEMO_GROUP_SYNCING&&foreground&&enabled);
    if(!connected){next.member_count=0U;next.members_limited=false;}
    if(pending)(void)snprintf(next.message,sizeof(next.message),"正在处理房间请求…");
    else if(error)(void)snprintf(next.message,sizeof(next.message),"%s (%d)",room_error_text(error),error);
    else if(connected)(void)snprintf(next.message,sizeof(next.message),"%s",next.mic_on?"正在说话，松开停止":"正在收听，按住下方按钮说话");
    else if(!next.known)(void)snprintf(next.message,sizeof(next.message),"正在同步房间信息…");
    else if(!next.assigned)(void)snprintf(next.message,sizeof(next.message),"创建或输入房间号加入多人对讲");
    else if(!foreground||!enabled)(void)snprintf(next.message,sizeof(next.message),"房间已暂停，进入多人对讲后恢复");
    else(void)snprintf(next.message,sizeof(next.message),"正在连接房间…");
    if(!s_ui_sent||memcmp(&next,&s_published,sizeof(next))!=0){
        (void)tirtc_ui_publish_room(&next);s_published=next;s_ui_sent=true;
    }
}
static void room_control_tick(void) {
    room_job_kind_e kind;
    if(!s_control_pending||!room_http_available()||s_terminal_pending||!s_media_idle)return;
    if(!s_transport_was_ready) {s_control_error=ROOM_ERR_OFFLINE;s_control_pending=false;room_zero(s_control_password,sizeof(s_control_password));return;}
    liot_rtos_enter_critical();kind=s_control_kind;liot_rtos_exit_critical();
    if(kind==ROOM_JOB_LEAVE && s_control_version!=s_assignment.version) {
        s_control_error=40922;s_control_pending=false;return;
    }
    room_zero(&s_http_job,sizeof(s_http_job));
    /* A queued tap must not become an operation for a later binding. */
    if(!demo_binding_service_context_current(&s_control_context)) {s_control_error=ROOM_ERR_OFFLINE;s_control_pending=false;room_zero(s_control_password,sizeof(s_control_password));return;}
    s_http_job.context=s_control_context;
    s_http_job.kind=kind;s_http_job.intent=s_control_intent;s_http_job.epoch=s_epoch;
    s_http_job.timeout_ms=ROOM_HTTP_TIMEOUT;s_http_job.version=s_assignment.version;
    memcpy(s_http_job.room_id,s_assignment.room_id,sizeof(s_http_job.room_id));
    liot_rtos_enter_critical();
    memcpy(s_http_job.code,s_control_code,sizeof(s_http_job.code));
    memcpy(s_http_job.password,s_control_password,sizeof(s_http_job.password));
    room_zero(s_control_password,sizeof(s_control_password));
    s_http_busy=true;s_http_done=false;
    liot_rtos_exit_critical();
    (void)liot_rtos_semaphore_release(s_http_wake);
}
int tirtc_group_start_service(void) {return demo_group_intercom_init();}
void tirtc_group_suspend(void) {
    liot_rtos_enter_critical();
    s_foreground=false;s_suspend_requested=true;s_intent=room_next(s_intent);
    room_gate_off_locked();s_accept_audio=false;s_accept_commands=false;
    liot_rtos_exit_critical();room_wake();
}
bool tirtc_group_is_idle(void) {return demo_group_intercom_is_idle();}
bool tirtc_group_is_active(void) {
    bool active;liot_rtos_enter_critical();active=s_enabled||!s_media_idle;
    liot_rtos_exit_critical();return active;
}
void tirtc_group_set_audio_config(uint8_t volume,uint8_t gain,bool speaker,bool microphone) {
    if (volume > 10U) volume = 10U;
    if (gain > 10U) gain = 10U;
    liot_rtos_enter_critical();
    s_microphone=microphone;s_speaker_enabled=speaker;
    if(!microphone||!gain)room_gate_off_locked();
    s_speaker=speaker?volume:0U;s_mic_level=microphone?gain:0U;s_levels_dirty=true;
    liot_rtos_exit_critical();room_wake();
}
int tirtc_group_action(const tirtc_ui_action_t *action) {
    uint32_t generation=0U;
    bool same;
    if(action==NULL)return -1;
    /* Preserve navigation intent during startup retries; media and server
     * mutations still require a fully initialized service below. */
    if(action->type==TIRTC_ACTION_ROOM_OPEN||action->type==TIRTC_ACTION_ROOM_REFRESH||
       action->type==TIRTC_ACTION_ENTER_PAGE||action->type==TIRTC_ACTION_RETURN_HOME) {
        bool opening;
        if(action->type==TIRTC_ACTION_ROOM_OPEN)opening=action->value!=0;
        else if(action->type==TIRTC_ACTION_ROOM_REFRESH)opening=true;
        else opening=action->type==TIRTC_ACTION_ENTER_PAGE&&
            (action->page==TIRTC_PAGE_ROOM||action->page==TIRTC_PAGE_ROOM_FORM);
        liot_rtos_enter_critical();
        bool changed=s_enabled!=opening;
        s_enabled=opening;s_control_error=0;
        if(changed||action->type==TIRTC_ACTION_ROOM_REFRESH)s_intent=room_next(s_intent);
        room_gate_off_locked();
        if(!opening){s_foreground=false;s_suspend_requested=true;s_accept_audio=false;s_accept_commands=false;}
        liot_rtos_exit_critical();room_wake();return s_initialized?0:-1;
    }
    if(!s_initialized)return -1;
    if(action->type==TIRTC_ACTION_ROOM_PTT) {
        if(!room_generation(action->extra,&generation))return -1;
        liot_rtos_enter_critical();
        same=generation==s_ui_generation&&strcmp(action->text,s_assignment.room_code)==0;
        if(!same){liot_rtos_exit_critical();return -1;}
        if(!action->value)room_gate_off_locked();
        else if(s_enabled&&s_foreground&&s_joined&&s_accept_audio&&!s_stopping&&
                s_microphone&&s_mic_level&&!s_control_pending) {
            if(!s_mic_gate){s_capture_discard=true;s_ptt_revision=room_next(s_ptt_revision);s_mic_dirty=true;}
            s_mic_gate=true;s_ptt_until=liot_rtos_get_running_time()+750U;
        }else{liot_rtos_exit_critical();return -1;}
        liot_rtos_exit_critical();room_wake();return 0;
    }
    if(action->type!=TIRTC_ACTION_ROOM_CREATE&&action->type!=TIRTC_ACTION_ROOM_JOIN&&
       action->type!=TIRTC_ACTION_ROOM_LEAVE)return -1;
    if(action->type==TIRTC_ACTION_ROOM_JOIN&&!room_digits(action->text,6U))return -1;
    if(action->type!=TIRTC_ACTION_ROOM_LEAVE&&action->extra[0]&&!room_digits(action->extra,4U))return -1;
    if(action->type==TIRTC_ACTION_ROOM_LEAVE&&!room_generation(action->extra,&generation))return -1;
    liot_rtos_enter_critical();
    if(s_control_pending||!s_foreground||!s_enabled){liot_rtos_exit_critical();return -1;}
    if(action->type==TIRTC_ACTION_ROOM_LEAVE) {
        if(!s_assignment.desired||generation!=s_ui_generation||strcmp(action->text,s_assignment.room_code)) {
            liot_rtos_exit_critical();return -1;
        }
    }else if(s_assignment.desired){liot_rtos_exit_critical();return -1;}
    /* Capture authorization when the tap is accepted, before any HTTP wait. */
    if(!demo_binding_service_context(&s_control_context)) {liot_rtos_exit_critical();return -1;}
    s_control_kind=action->type==TIRTC_ACTION_ROOM_CREATE?ROOM_JOB_CREATE:
        action->type==TIRTC_ACTION_ROOM_JOIN?ROOM_JOB_JOIN:ROOM_JOB_LEAVE;
    s_control_intent=s_intent;s_control_version=s_assignment.version;s_control_pending=true;s_control_error=0;
    room_zero(s_control_code,sizeof(s_control_code));room_zero(s_control_password,sizeof(s_control_password));
    if(s_control_kind==ROOM_JOB_JOIN)memcpy(s_control_code,action->text,7U);
    if(s_control_kind!=ROOM_JOB_LEAVE)memcpy(s_control_password,action->extra,strlen(action->extra)+1U);
    room_gate_off_locked();
    if(!s_media_idle){s_suspend_requested=true;s_accept_audio=false;s_accept_commands=false;}
    liot_rtos_exit_critical();room_wake();return 0;
}
