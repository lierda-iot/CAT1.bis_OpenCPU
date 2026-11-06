/* XiaoTai AI protocol port for Lierda F6D.
 * Protocol references: xiaotai-esp32 66a0d738 starter_runtime/starter_tirtc;
 * managed runtime/audio contracts: xiaotai-lierda f159bfd3 (MIT AND Apache-2.0).
 * One worker owns session, PCM and controls. SDK callbacks only copy into fixed
 * bounded queues. UI snapshots never contain credentials or raw dialog logs. */
#include "tirtc_ai.h"
#include "ai_call_protocol.h"
#include "../calls/tirtc_calls.h"
#include "tirtc_runtime.h"
#include "audio_device.h"
#include "opus_codec.h"
#include "device_binding.h"
#include "formal_mqtt.h"
#include "tirtc_network.h"
#include "tirtc_status.h"
#include "json_guard.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include <stdio.h>
#include <string.h>

#define AI_CMD 0x2100U
#define AI_STANDBY_SETTLE_MS 500U
#define AI_ACCESS_TTL_MS 15000U
#define AI_OWNER_WAIT_MS 12000U
/* Bound encoded bytes, complete packets and decoded duration separately. */
#define AI_RX_BYTES 15360U
#define AI_RX_PACKETS 96U
#define AI_RX_MAX_SAMPLES 30720U /* Retain the original 1.92 s capacity. */
#define AI_CMD_COUNT 3U
#define AI_CMD_BYTES 4608U
#define AI_PLAY_BATCH 24U
#define AI_PLAY_AHEAD_MS 480U
#define AI_PLAY_BUDGET_MS 20U
#define AI_PREROLL_SAMPLES 3840U /* 240 ms at 16 kHz, independent of Opus bitrate. */
#define AI_PREROLL_WAIT_MS 120U
#define AI_PLAY_POLL_MS 5U
#define AI_AUDIO_CLEANUP_RETRY_MS 1000U
#define AI_AUDIO_CLEANUP_LIMIT_MS 15000U
#define AI_SEND_HIGH_WATER_BYTES (12U * 1024U)
#define AI_TX_FAILURE_LIMIT_MS 3000U
#define AI_TX_FAILURE_LOG_MS 1000U
#if DEMO_AI_AUDIO_AEC_ENABLE
#define AI_RECORD_FAILURE_LIMIT_MS 3000U
#define AI_REMOTE_TAIL_QUIET_MS 1200U
#define AI_REMOTE_TAIL_STALL_MS 3000U
#endif
#define AI_ERR_NOT_READY (-1401)
#define AI_ERR_TOKEN (-1402)
#define AI_ERR_TIMEOUT (-1403)
#define AI_ERR_PROTOCOL (-1404)
#define AI_ERR_AUDIO (-1405)
#define AI_ERR_LOST (-1406)
#define AI_ERR_AUDIO_CLEANUP (-1407)
#define AI_ERR_UPLINK (-1408)
typedef enum { AI_IDLE, AI_TOKEN, AI_CONNECT, AI_NEGOTIATE, AI_ACTIVE, AI_STOPPING, AI_ERROR, AI_BLOCKED, AI_IDLE_FETCH, AI_WAIT_OWNER, AI_IDLE_CONNECT, AI_IDLE_READY } ai_phase_t;
typedef struct { uint8_t used; uint32_t order, epoch, received_at; uint16_t length; char data[AI_CMD_BYTES + 1U]; } ai_cmd_t;

static liot_task_t s_task;
static liot_sem_t s_sem;
static bool s_starting, s_available, s_want, s_callbacks, s_whip_done, s_lost;
static uint32_t s_request, s_running, s_generation, s_order, s_deadline;
static uint32_t s_clicked_at, s_running_clicked_at, s_connected_at, s_first_rx_at;
static bool s_first_rx_seen, s_first_rx_logged, s_first_pcm_logged, s_first_signal_logged;
static uint32_t s_requested_credential_epoch, s_credential_epoch;
static int s_whip_error, s_lost_error, s_finish_error;
static tirtc_conn_t s_connection;
static ai_phase_t s_phase;
static bool s_runtime_owned, s_audio_owned, s_audio_started, s_restart_requested;
static demo_ai_audio_lease_t s_lease = DEMO_AI_AUDIO_LEASE_INIT;
static tirtc_network_snapshot_t s_route;
static demo_binding_ai_access_t s_access;
typedef struct { uint16_t length, samples; } ai_rx_packet_t;
static uint8_t s_rx[AI_RX_BYTES], s_play_opus[DEMO_AI_OPUS_MAX_PACKET];
static ai_rx_packet_t s_rx_packets[AI_RX_PACKETS];
static uint32_t s_rx_head, s_rx_count, s_rx_samples;
static int16_t s_decode_pcm[DEMO_AI_OPUS_MAX_DECODE_SAMPLES];
static uint16_t s_decode_samples, s_decode_offset;
static uint32_t s_rx_read, s_rx_queued, s_rx_peak, s_rx_drop_bytes, s_rx_bad_frames, s_cmd_drops, s_play_errors;
static uint32_t s_media_step_at, s_media_gap_max, s_rebuffers;
static bool s_media_step_seen;
static ai_cmd_t s_commands[AI_CMD_COUNT];
static ai_call_request_t s_call_request;
static uint32_t s_call_ticket, s_call_started, s_call_epoch;
static int16_t s_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES], s_record[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint8_t s_uplink_opus[DEMO_AI_OPUS_MAX_PACKET];
static char s_device_id[DEMO_BIND_DEVICE_ID_MAX], s_role[DEMO_BIND_AI_ROLE_ID_MAX], s_rpc_id[40];
static uint32_t s_tx, s_rx_frames, s_played, s_rx_drops, s_tx_drops, s_suppressed, s_capture;
static uint32_t s_play_audio_until, s_preroll_since;
static bool s_play_continuing, s_preroll_waiting;
static uint32_t s_play_until, s_last_log, s_stopping_since, s_timestamp, s_last_ui;
static bool s_play_guard, s_controls_dirty = true;
static bool s_capture_paused;
/* UI changes only latch intent; the worker drains old SDK PCM while muted. */
static bool s_speaker_mute_pending, s_speaker_draining;
static uint8_t s_play_sample_remainder;
static uint32_t s_capture_pause_at, s_capture_pause_remainder;
static bool s_play_retry;
static uint32_t s_play_retry_since;
static bool s_suppression_hint, s_prime_wait_hint;
static uint8_t s_applied_volume, s_applied_mic_gain;
static bool s_applied_speaker, s_applied_mic;
static bool s_audio_cleanup_retry;
static uint32_t s_audio_cleanup_retry_at;
static int s_audio_cleanup_error;
static bool s_tx_failure_active, s_tx_failure_logged;
static uint32_t s_tx_failure_since, s_tx_failure_last_log;
#if DEMO_AI_AUDIO_AEC_ENABLE
static bool s_record_failure_active;
static uint32_t s_record_failure_since, s_record_failure_generation;
static bool s_remote_tail, s_remote_tail_quiet;
static uint32_t s_remote_tail_progress, s_remote_tail_quiet_since;
static uint32_t s_mic_capture_generation;
#endif
/* A background attempt has its own immutable request generation. Promotion
 * changes intent, not the generation passed through the managed callback. */
static bool s_promoted_attempt;
static bool s_standby, s_access_ready, s_start_sent, s_background_cleanup, s_resume_after_cleanup;
static bool s_home_eligible = true, s_idle_ready_seen, s_idle_retry_pending;
static uint8_t s_idle_failures;
static uint32_t s_idle_ready_since, s_idle_retry_at, s_access_at;
static uint32_t s_idle_route, s_idle_epoch;
static int s_idle_sim;
static tirtc_ui_ai_t s_ui = { .volume = DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL,
    .mic_gain = DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL, .speaker_enabled = true, .mic_enabled = true };

static uint32_t tick(void) { return liot_rtos_get_running_time(); }
static void startup_stage(const char *stage, uint32_t at, int detail)
{
    TIRTC_LOG_DEBUG("[AI21] startup request=%lu stage=%s elapsed_ms=%lu detail=%d\r\n",
        (unsigned long)s_running, stage, (unsigned long)(at-s_running_clicked_at), detail);
}
static bool pending(uint32_t deadline) { return (int32_t)(deadline - tick()) > 0; }
static void signal_worker(void) { if (s_sem) (void)liot_rtos_semaphore_release(s_sem); }
void tirtc_ai_set_audio_config(uint8_t volume, uint8_t mic_gain,
                               bool speaker_enabled, bool mic_enabled)
{
    bool changed;
    if (volume > 10U) volume = 10U;
    if (mic_gain > 10U) mic_gain = 10U;
    liot_rtos_enter_critical();
    changed = s_ui.volume != volume || s_ui.mic_gain != mic_gain ||
              s_ui.speaker_enabled != speaker_enabled || s_ui.mic_enabled != mic_enabled;
    if (s_ui.speaker_enabled && s_ui.volume && (!speaker_enabled || !volume))
        s_speaker_mute_pending = true;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if (s_ui.mic_gain != mic_gain || s_ui.mic_enabled != mic_enabled)
        ++s_mic_capture_generation;
#endif
    s_ui.volume = volume; s_ui.mic_gain = mic_gain;
    s_ui.speaker_enabled = speaker_enabled; s_ui.mic_enabled = mic_enabled;
    if (changed) s_controls_dirty = true;
    liot_rtos_exit_critical();
    if (changed) signal_worker();
}
/* Preserve complete UTF-8 code points when clipping an externally supplied string. */
static bool text_copy(char *dst, size_t cap, const char *src)
{
    size_t n = 0, width, i; bool clipped = false;
    if (!src) src = "";
    while (src[n]) {
        unsigned char c = (unsigned char)src[n];
        width = c < 0x80 ? 1U : (c >= 0xc2 && c <= 0xdf ? 2U :
                (c >= 0xe0 && c <= 0xef ? 3U : (c >= 0xf0 && c <= 0xf4 ? 4U : 0U)));
        if (!width || n + width >= cap) { clipped = true; break; }
        for (i = 1; i < width; ++i) if (!src[n+i] || ((unsigned char)src[n+i] & 0xc0) != 0x80) break;
        if (i != width || (width == 3 && ((c == 0xe0 && (unsigned char)src[n+1] < 0xa0) ||
            (c == 0xed && (unsigned char)src[n+1] >= 0xa0))) || (width == 4 &&
            ((c == 0xf0 && (unsigned char)src[n+1] < 0x90) || (c == 0xf4 && (unsigned char)src[n+1] >= 0x90)))) { clipped = true; break; }
        memcpy(dst+n, src+n, width); n += width;
    }
    dst[n] = 0; return clipped;
}
void tirtc_ai_publish(void)
{
    /* Copy belongs to this caller. The AI task publishes periodically;
     * callers on a small main/UI stack must not invoke this function. */
    tirtc_ui_ai_t copy;
    liot_rtos_enter_critical(); copy = s_ui; liot_rtos_exit_critical();
    (void)tirtc_ui_publish_ai(&copy);
}
static void show(tirtc_ui_ai_state_t state, const char *message)
{
    liot_rtos_enter_critical();
    s_ui.ai_state = state;
    s_ui.ai_active = !s_standby && s_phase != AI_IDLE && s_phase != AI_ERROR && s_phase != AI_BLOCKED;
    (void)text_copy(s_ui.ai_status, sizeof(s_ui.ai_status), message);
    liot_rtos_exit_critical();
    tirtc_ai_publish();
}
static bool ready(void)
{
    demo_binding_snapshot_t binding;
    demo_binding_get_snapshot(&binding);
    return binding.state == DEMO_BIND_BOUND && binding.credential_epoch != 0 && binding.api_ready && !binding.binding_required &&
           tirtc_network_is_ready() && tirtc_network_time_ready() &&
           demo_formal_mqtt_is_online() && demo_tirtc_is_ready();
}
static bool requested(void)
{
    bool yes;
    liot_rtos_enter_critical(); yes = s_want && s_request == s_running; liot_rtos_exit_critical();
    return yes;
}
static bool attempt_current(void)
{
    bool yes;
    liot_rtos_enter_critical();
    yes = s_request == s_running && (s_want || (s_standby && s_home_eligible && s_ui.mic_enabled && s_ui.mic_gain));
    liot_rtos_exit_critical();
    return yes;
}
static bool route_current(void)
{
    tirtc_network_snapshot_t route; demo_binding_snapshot_t binding;
    tirtc_network_get_snapshot(&route); demo_binding_get_snapshot(&binding);
    return route.ready && route.time_valid && route.generation == s_route.generation && route.sim_id == s_route.sim_id &&
           s_credential_epoch != 0 && binding.credential_epoch == s_credential_epoch;
}
static void clear_caption(void)
{
    liot_rtos_enter_critical();
    s_ui.ai_caption[0] = s_ui.ai_utterance_id[0] = s_ui.ai_emotion[0] = 0;
    s_ui.ai_caption_is_ai = s_ui.ai_caption_final = s_ui.ai_caption_truncated = false;
    s_ui.ai_revision++; s_ui.ai_paragraph++; s_ui.audio_level = 0;
    liot_rtos_exit_critical();
}
static void discard_audio_queue(void)
{
    liot_rtos_enter_critical();
    s_rx_read=s_rx_queued=s_rx_head=s_rx_count=s_rx_samples=0;
    liot_rtos_exit_critical();
    s_decode_samples=s_decode_offset=0;
    /* Worker-only: interruption/mute must not replay codec history. */
    demo_ai_opus_reset_decoder();
    s_play_retry=false; s_play_continuing=s_preroll_waiting=false;
}
static void discard_queues(void)
{
    discard_audio_queue();
    liot_rtos_enter_critical(); memset(s_commands,0,sizeof(s_commands)); liot_rtos_exit_critical();
}
static bool callback_current(tirtc_conn_t conn)
{
    return s_callbacks && s_want && s_request == s_running && conn && conn == s_connection;
}
static void connected(int error, tirtc_conn_t conn, void *arg)
{
    liot_rtos_enter_critical();
    if (s_callbacks && (s_phase == AI_CONNECT || s_phase == AI_IDLE_CONNECT) &&
        (uint32_t)(uintptr_t)arg == s_running && s_request == s_running &&
        (s_want || (s_standby && s_home_eligible))) {
        s_whip_done = true; s_whip_error = error; s_connected_at=tick();
        if (!error) s_connection = conn;
    }
    liot_rtos_exit_critical();
    /* The managed runtime owns rejected/late handles and disconnect reservations. */
    signal_worker();
}
static void connection_error(tirtc_conn_t conn, int error)
{
    liot_rtos_enter_critical();
    if (s_callbacks && s_request == s_running && (s_want || s_standby) &&
        conn && conn == s_connection) {
#if DEMO_AI_AUDIO_AEC_ENABLE
        /* A disconnect often follows an error callback. It must not turn an
         * already observed transport failure into a successful remote ending. */
        if (!s_lost || s_lost_error==TIRTC_E_CONN_REMOTECLOSE || error!=TIRTC_E_CONN_REMOTECLOSE)
#endif
            s_lost_error = error ? error : AI_ERR_LOST;
        s_lost = true;
    }
    liot_rtos_exit_critical(); signal_worker();
}
static void disconnected(tirtc_conn_t conn) {
#if DEMO_AI_AUDIO_AEC_ENABLE
    connection_error(conn, TIRTC_E_CONN_REMOTECLOSE);
#else
    connection_error(conn, 0);
#endif
}
static void audio_received(tirtc_conn_t conn, const TIRTCFRAMEINFO *frame, void *data)
{
    uint32_t write, first, tail; int samples;
    if (!frame || !data || !frame->length) return;
    samples=(frame->length<=DEMO_AI_OPUS_MAX_PACKET) ?
        demo_ai_opus_packet_samples(data,(int)frame->length) : -1;
    liot_rtos_enter_critical();
    /* ACK and first PCM can arrive before the worker consumes the ACK.
     * Cache the current connection's valid format; playback starts only after
     * successful negotiation, and finish() discards failed/cancelled attempts. */
    if (!callback_current(conn) || (s_phase!=AI_NEGOTIATE && s_phase!=AI_ACTIVE)) { liot_rtos_exit_critical(); return; }
    if (frame->stream_id!=1U || frame->media!=TIRTC_AUDIO_OPUS || frame->flags!=TIRTC_AUDIOSAMPLE_16K16B1C ||
        samples<=0 || samples>DEMO_AI_OPUS_MAX_DECODE_SAMPLES || (samples&1)) {
        s_rx_drops++; s_rx_bad_frames++; liot_rtos_exit_critical(); return;
    }
    /* Mute is intentional: do not preserve old speech for a later unmute. */
    if (!s_ui.speaker_enabled || !s_ui.volume || s_speaker_mute_pending || s_speaker_draining) { liot_rtos_exit_critical(); return; }
    if (frame->length>AI_RX_BYTES-s_rx_queued || s_rx_count>=AI_RX_PACKETS ||
        (uint32_t)samples>AI_RX_MAX_SAMPLES-s_rx_samples) {
        s_rx_drops++; s_rx_drop_bytes+=frame->length;
    } else {
        write=(s_rx_read+s_rx_queued)%AI_RX_BYTES;
        first=AI_RX_BYTES-write; if(first>frame->length)first=frame->length;
        memcpy(s_rx+write,data,first);
        if(first<frame->length)memcpy(s_rx,(const uint8_t *)data+first,frame->length-first);
        tail=(s_rx_head+s_rx_count)%AI_RX_PACKETS;
        s_rx_packets[tail].length=(uint16_t)frame->length;
        s_rx_packets[tail].samples=(uint16_t)samples;
        s_rx_count++; s_rx_samples+=(uint32_t)samples;
        s_rx_queued+=frame->length; s_rx_frames++;
        if(!s_first_rx_seen) { s_first_rx_seen=true; s_first_rx_at=tick(); }
        if(s_rx_queued>s_rx_peak)s_rx_peak=s_rx_queued;
    }
    liot_rtos_exit_critical(); signal_worker();
}
static bool ai_command(uint32_t word) { return ((word >> 1) & 0x7fffU) == AI_CMD || (word & 0xfffeU) == AI_CMD; }
static void command_received(tirtc_conn_t conn, uint32_t word, const void *data, uint32_t length)
{
    unsigned i;
    if (!ai_command(word) || !data || !length || length > AI_CMD_BYTES) return;
    liot_rtos_enter_critical();
    if (!callback_current(conn) || (s_phase != AI_NEGOTIATE && s_phase != AI_ACTIVE)) { liot_rtos_exit_critical(); return; }
    for (i = 0; i < AI_CMD_COUNT && s_commands[i].used; ++i) {}
    if (i == AI_CMD_COUNT) s_cmd_drops++;
    else {
        s_commands[i].received_at = tick(); s_commands[i].length = (uint16_t)length; s_commands[i].epoch = s_running; s_commands[i].order = ++s_order;
        memcpy(s_commands[i].data, data, length); s_commands[i].data[length] = 0; s_commands[i].used = 1;
    }
    liot_rtos_exit_critical(); signal_worker();
}
static int subscribed(tirtc_conn_t conn, uint8_t stream)
{
    bool ok; liot_rtos_enter_critical(); ok = callback_current(conn) && (s_phase == AI_NEGOTIATE || s_phase == AI_ACTIVE) && stream == 1; liot_rtos_exit_critical(); return ok ? 0 : -1;
}
static const demo_tirtc_listener_t s_listener = {
    .on_conn_error = connection_error, .on_disconnected = disconnected,
    .on_audio = audio_received, .on_command = command_received, .on_subscribe_audio = subscribed
};
static void idle_delay(bool failed)
{
    static const uint32_t delays[] = { 5000U, 15000U, 30000U, 60000U };
    uint32_t delay = 5000U;
    if (failed) {
        delay = delays[s_idle_failures < 3U ? s_idle_failures : 3U];
        if (s_idle_failures < 3U) s_idle_failures++;
    }
    s_idle_retry_at = tick() + delay; s_idle_retry_pending = true; s_idle_ready_seen = false;
}
static void finish(int error)
{
    static const char end[] = "{\"jsonrpc\":\"2.0\",\"method\":\"end_session\"}";
    if (s_phase == AI_STOPPING || s_phase == AI_BLOCKED) return;
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_remote_tail=false; s_remote_tail_quiet=false;
    s_record_failure_active=false;
#endif
    tirtc_contacts_resolve_cancel(s_call_ticket); s_call_ticket=0;
    /* A promoted idle transport can go stale just before its first ACK. Only
     * that attempt gets one cold fallback; the replacement has no retry credit. */
    s_background_cleanup = s_standby || (s_promoted_attempt && requested() &&
        s_phase!=AI_ACTIVE && error!=AI_ERR_AUDIO &&
        (s_lost || s_phase==AI_CONNECT || error==AI_ERR_PROTOCOL || error==AI_ERR_TIMEOUT));
    if(s_background_cleanup && s_promoted_attempt) s_resume_after_cleanup=true;
    s_promoted_attempt=false;
    if(s_standby && requested()) s_resume_after_cleanup=true;
    if (!s_background_cleanup) startup_stage("end",tick(),error);
    else liot_trace("[AI21] standby end ret=%d foreground=%u\r\n", error, requested()?1U:0U);
    idle_delay(error != 0);
    liot_rtos_enter_critical(); s_callbacks = false; liot_rtos_exit_critical();
    if (s_connection && s_start_sent)
        (void)demo_tirtc_send_command(DEMO_TIRTC_OWNER_AI, s_generation, AI_CMD, end, sizeof(end)-1U);
    s_finish_error = error; s_phase = AI_STOPPING; s_stopping_since = tick(); s_restart_requested = false;
    s_audio_cleanup_retry = false; s_audio_cleanup_error = 0;
    s_deadline = 0; demo_binding_clear_ai_access(&s_access); discard_queues();
    if (!s_background_cleanup) show(TIRTC_AI_RESTING, "正在结束对话");
}
static void audio_cleanup_blocked(void)
{
    /* An asynchronous SDK operation may still own DMA/buffers. Keep both
     * leases and the SDK configuration alive until a real device restart.
     * Reporting an error must not make this resource available to a new AI. */
    liot_rtos_enter_critical();
    s_want = false; s_callbacks = false;
    s_phase = AI_BLOCKED; s_finish_error = AI_ERR_AUDIO_CLEANUP;
    liot_rtos_exit_critical();
    liot_trace("[AI] audio cleanup blocked ret=%d; leases retained, device restart required\r\n", s_audio_cleanup_error);
    show(TIRTC_AI_ERROR, "音频停止失败，请重启设备");
    tirtc_status_event("音频停止失败，需重启设备");
}
static void audio_cleanup_failed(int error)
{
    if (!s_audio_cleanup_retry || error != s_audio_cleanup_error)
        liot_trace("[AI] audio cleanup pending ret=%d; retry in 1000ms\r\n", error);
    s_audio_cleanup_error = error;
    s_audio_cleanup_retry = true;
    s_audio_cleanup_retry_at = tick() + AI_AUDIO_CLEANUP_RETRY_MS;
    if ((uint32_t)(tick() - s_stopping_since) >= AI_AUDIO_CLEANUP_LIMIT_MS)
        audio_cleanup_blocked();
}
static void cleanup_step(void)
{
    int result; bool resume;
    if (s_phase == AI_BLOCKED) return;
    if (s_audio_owned) {
        if (s_audio_cleanup_retry) {
            if ((uint32_t)(tick() - s_stopping_since) >= AI_AUDIO_CLEANUP_LIMIT_MS) {
                audio_cleanup_blocked();
                return;
            }
            if (pending(s_audio_cleanup_retry_at)) return;
        }
        result = demo_ai_audio_stop(&s_lease);
        if (result != 0) { audio_cleanup_failed(result); return; }
        result = demo_ai_audio_release(&s_lease);
        if (result != 0) { audio_cleanup_failed(result); return; }
        s_audio_cleanup_retry = false;
        s_audio_owned = s_audio_started = false;
    }
    /* No codec work can follow finish() on this single worker. Return its
     * heap before releasing the RTC owner to a pending video call. */
    demo_ai_opus_deinit();
    if (s_runtime_owned) {
        (void)demo_tirtc_disconnect(DEMO_TIRTC_OWNER_AI, s_generation);
        result = demo_tirtc_session_release(DEMO_TIRTC_OWNER_AI, s_generation);
        if (result != 0) {
            if (!s_restart_requested && (uint32_t)(tick()-s_stopping_since) >= 15000U) {
                s_restart_requested = true; demo_tirtc_require_restart(AI_ERR_TIMEOUT);
            }
            return;
        }
        s_runtime_owned = false;
    }
    liot_rtos_enter_critical();
    resume = (s_background_cleanup || s_resume_after_cleanup) && s_want;
    s_connection = NULL; s_generation = 0; s_want = resume; s_running = 0;
    if(resume && !++s_request)++s_request; /* A replacement transport gets a fresh callback epoch. */
    s_whip_done = s_lost = false;
    liot_rtos_exit_critical();
    s_play_guard = s_suppression_hint = s_prime_wait_hint = false;
    s_standby = s_access_ready = s_start_sent = s_resume_after_cleanup = false;
    if (s_background_cleanup) {
        s_background_cleanup = false; s_phase = AI_IDLE;
        if(!resume && s_ui.ai_active) { clear_caption(); show(TIRTC_AI_IDLE,"已结束"); }
        return; /* Background failures and foreground fallback do not flash UI errors. */
    }
    clear_caption();
    s_phase = s_finish_error ? AI_ERROR : AI_IDLE;
    if (s_finish_error) {
        char status[80];
        if (s_finish_error == AI_ERR_UPLINK)
            (void)text_copy(status, sizeof(status), "音频发送中断，请点击表情重试");
        else (void)snprintf(status, sizeof(status), "对话未完成 (%d)，请点击表情重试", s_finish_error);
        show(TIRTC_AI_ERROR, status);
    } else show(TIRTC_AI_IDLE, "待唤醒");
    tirtc_status_event(s_finish_error ? "AI 会话结束：错误" : "AI 会话已结束");
}
static int start_command(void)
{
    cJSON *root = cJSON_CreateObject(), *params = cJSON_CreateObject(), *input = cJSON_CreateObject(), *output = cJSON_CreateObject();
    char *json; bool ok; int result = -1;
    (void)snprintf(s_rpc_id, sizeof(s_rpc_id), "%08lx-%08lx", (unsigned long)s_running, (unsigned long)tick());
    ok = root && params && input && output && cJSON_AddStringToObject(root,"jsonrpc","2.0") &&
         cJSON_AddStringToObject(root,"id",s_rpc_id) && cJSON_AddStringToObject(root,"method","start_session") &&
         cJSON_AddStringToObject(params,"device_id",s_device_id) && cJSON_AddStringToObject(params,"role_id",s_role) &&
         cJSON_AddStringToObject(input,"codec","opus") && cJSON_AddNumberToObject(input,"sample_rate",16000) && cJSON_AddNumberToObject(input,"channels",1) &&
         cJSON_AddStringToObject(output,"codec","opus") && cJSON_AddNumberToObject(output,"sample_rate",16000) && cJSON_AddNumberToObject(output,"channels",1);
    if (ok) { ok = cJSON_AddItemToObject(params,"input_audio",input); if (ok) input = NULL; }
    if (ok) { ok = cJSON_AddItemToObject(params,"output_audio",output); if (ok) output = NULL; }
    if (ok) { ok = cJSON_AddItemToObject(root,"params",params); if (ok) params = NULL; }
    json = ok ? cJSON_PrintUnformatted(root) : NULL;
    if (json) {
        startup_stage("cmd_submit",tick(),0);
        result = demo_tirtc_send_command(DEMO_TIRTC_OWNER_AI,s_generation,AI_CMD,json,(uint32_t)strlen(json));
        startup_stage("cmd_return",tick(),result);
        if(result >= 0) s_start_sent = true;
        cJSON_free(json);
    }
    cJSON_Delete(root); cJSON_Delete(params); cJSON_Delete(input); cJSON_Delete(output);
    return result;
}
static const char *json_string(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o,key); return cJSON_IsString(v) && v->valuestring ? v->valuestring : NULL;
}
static bool profile(const cJSON *o)
{
    const char *codec = json_string(o,"codec"); const cJSON *rate = cJSON_GetObjectItemCaseSensitive(o,"sample_rate"), *channels = cJSON_GetObjectItemCaseSensitive(o,"channels");
    /* Older services omit optional negotiated formats; retain our explicit
     * Opus/16-kHz/mono request, but reject an explicit incompatible profile. */
    if (o == NULL) return true;
    return codec && !strcmp(codec,"opus") && cJSON_IsNumber(rate) && rate->valuedouble == 16000 && cJSON_IsNumber(channels) && channels->valuedouble == 1;
}
static void caption(const cJSON *params)
{
    const char *text = json_string(params,"text"), *emotion = json_string(params,"emotion"), *utterance = json_string(params,"utterance_id");
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(params,"caption_type"), *mode = cJSON_GetObjectItemCaseSensitive(params,"mode"),
        *final = cJSON_GetObjectItemCaseSensitive(params,"is_final"), *uid = cJSON_GetObjectItemCaseSensitive(params,"utterance_id");
    char id[64]; size_t used; bool is_ai, append, new_group, clipped;
    if (!text || !cJSON_IsNumber(type)) return;
    s_suppression_hint = s_prime_wait_hint = false;
    id[0] = 0;
    if (utterance) (void)text_copy(id,sizeof(id),utterance);
    else if (cJSON_IsNumber(uid)) (void)snprintf(id,sizeof(id),"%.0f",uid->valuedouble);
    is_ai = type->valueint == 1; append = cJSON_IsNumber(mode) && mode->valueint == 1;
    liot_rtos_enter_critical();
    new_group = !s_ui.ai_caption[0] || s_ui.ai_caption_is_ai != is_ai || strcmp(id,s_ui.ai_utterance_id);
    if (new_group || !append) {
        if (new_group) s_ui.ai_paragraph++;
        (void)text_copy(s_ui.ai_caption,sizeof(s_ui.ai_caption),is_ai ? "AI：" : "你："); s_ui.ai_caption_truncated = false;
    }
    used = strlen(s_ui.ai_caption); clipped = text_copy(s_ui.ai_caption+used,sizeof(s_ui.ai_caption)-used,text);
    s_ui.ai_caption_truncated |= clipped; s_ui.ai_caption_is_ai = is_ai; s_ui.ai_caption_final = cJSON_IsTrue(final);
    (void)text_copy(s_ui.ai_utterance_id,sizeof(s_ui.ai_utterance_id),id);
    if (emotion) (void)text_copy(s_ui.ai_emotion,sizeof(s_ui.ai_emotion),emotion);
    s_ui.ai_revision++;
    liot_rtos_exit_critical();
    show(is_ai ? TIRTC_AI_SPEAKING : (cJSON_IsTrue(final) ? TIRTC_AI_THINKING : TIRTC_AI_LISTENING),
         is_ai ? "AI 回复中" : (cJSON_IsTrue(final) ? "AI 正在思考" : "正在聆听"));
}
static const char *call_error_message(const char *status)
{
    if (!strcmp(status,"not_found")) return "没有找到联系人，请使用完整备注或 ID";
    if (!strcmp(status,"ambiguous")) return "联系人同名，请说明微信或设备，或使用唯一 ID";
    if (!strcmp(status,"offline")) return "目标设备当前不在线";
    if (!strcmp(status,"contacts_timeout")) return "查询联系人超时，请稍后重试";
    if (!strcmp(status,"contacts_unavailable")) return "联系人查询不可用，请先检查通讯录";
    if (!strcmp(status,"busy")) return "已有通话或呼叫正在处理";
    if (!strcmp(status,"unsupported")) return "不支持此设备动作，请使用 call_contact";
    if (!strcmp(status,"unsupported_call_type")) return "通话类型应为 video 或 audio";
    return "呼叫参数无效，请提供完整联系人名称和正确类型";
}
static bool call_reply(const ai_call_request_t *request, bool ok, const char *status, bool wechat, bool video)
{
    char *json=ai_call_reply_json(request->rpc_id,ok,status,ok?"已受理呼叫，正在切换":call_error_message(status),wechat,video,request->rpc_id_numeric);
    size_t length=0; int result=-1;
    if (json) {
        length=strlen(json);
        result=demo_tirtc_send_command(DEMO_TIRTC_OWNER_AI,s_generation,AI_CMD,json,(uint32_t)length);
        cJSON_free(json);
    }
    /* SDK return includes the four command bytes. Short/failed ACK must not
     * cause an unacknowledged call. No credentials/contact names are logged. */
    liot_trace("[AI-CALL] status=%s send=%d bytes=%u\r\n",status,result,(unsigned)length);
    return length && result==(int)(length+sizeof(uint32_t));
}
static void call_received(const cJSON *root)
{
    ai_call_request_t request;
    const char *error=ai_call_parse(root,&request);
    if (!request.rpc_id[0]) { liot_trace("[AI-CALL] ignored invalid request id or duplicate fields\r\n"); return; }
    if (error) { (void)call_reply(&request,false,error,false,false); return; }
    if (s_call_ticket) {
        if (request.rpc_id_numeric!=s_call_request.rpc_id_numeric || strcmp(request.rpc_id,s_call_request.rpc_id))
            (void)call_reply(&request,false,"busy",false,false);
        return; /* One HTTP query and one eventual call per pending RPC id. */
    }
    if (tirtc_calls_has_session()) { (void)call_reply(&request,false,"busy",false,false); return; }
    liot_trace("[AI-CALL] received method=%s scope=%u video=%u\r\n",json_string(root,"method"),(unsigned)request.scope,request.video?1U:0U);
    s_call_request=request; s_call_epoch=s_running; s_call_started=tick();
    s_call_ticket=tirtc_contacts_resolve_begin(request.target,request.scope);
    if (!s_call_ticket) { (void)call_reply(&request,false,"contacts_unavailable",false,false); return; }
    show(TIRTC_AI_THINKING,"正在查找联系人");
}
static void call_lookup_step(void)
{
    tirtc_contact_t contact; tirtc_contact_result_t found;
    const char *error=NULL; int result=-1;
    if (!s_call_ticket) return;
    if (s_phase!=AI_ACTIVE || !requested() || s_call_epoch!=s_running || s_lost || !route_current()) {
        tirtc_contacts_resolve_cancel(s_call_ticket); s_call_ticket=0; return;
    }
    if ((uint32_t)(tick()-s_call_started)>=12000U) error="contacts_timeout";
    else {
        found=tirtc_contacts_resolve_poll(s_call_ticket,&contact);
        if (found==TIRTC_RESOLVE_PENDING) return;
        if (found==TIRTC_RESOLVE_NOT_FOUND) error="not_found";
        else if (found==TIRTC_RESOLVE_AMBIGUOUS) error="ambiguous";
        else if (found==TIRTC_RESOLVE_OFFLINE) error="offline";
        else if (found!=TIRTC_RESOLVE_FOUND) error="contacts_unavailable";
        else if (s_call_request.expected_id[0] && strcmp(s_call_request.expected_id,contact.id)) error="not_found";
    }
    tirtc_contacts_resolve_cancel(s_call_ticket); s_call_ticket=0;
    if (error) {
        (void)call_reply(&s_call_request,false,error,false,false);
        show(TIRTC_AI_LISTENING,call_error_message(error)); return;
    }
    if (!call_reply(&s_call_request,true,"accepted",contact.wechat,s_call_request.video)) {
        show(TIRTC_AI_LISTENING,"呼叫确认发送失败，请重试"); return;
    }
    /* Sending may yield. Fence a stop, identity change, or incoming call before
     * queuing intent. This RAM-only commit reserves the existing call worker;
     * it then waits for AI audio/RTC leases to be fully released. */
    liot_rtos_enter_critical();
    if (s_want && s_request==s_running && s_call_epoch==s_running && !s_lost)
        result=tirtc_calls_dial_contact(&contact,s_call_request.video);
    liot_rtos_exit_critical();
    if (!result) {
        liot_trace("[AI-CALL] handoff wx=%u video=%u\r\n",contact.wechat?1U:0U,s_call_request.video?1U:0U);
        finish(0);
    } else if (requested()) {
        show(TIRTC_AI_LISTENING,"呼叫未能发起，请稍后重试");
        tirtc_status_event("AI 呼叫转接取消或设备忙");
    }
}
#if DEMO_AI_AUDIO_AEC_ENABLE
static void begin_remote_tail(void)
{
    if (s_remote_tail || s_phase!=AI_ACTIVE) return;
    tirtc_contacts_resolve_cancel(s_call_ticket); s_call_ticket=0;
    s_remote_tail=true; s_remote_tail_quiet=false;
    s_record_failure_active=false;
    s_remote_tail_progress=tick();
    s_tx_failure_active=false;
    liot_rtos_enter_critical(); s_ui.audio_level=0; liot_rtos_exit_critical();
}
#endif
static bool command_notification(const cJSON *root)
{
    return !cJSON_GetObjectItemCaseSensitive(root,"id") &&
        !cJSON_GetObjectItemCaseSensitive(root,"result") &&
        !cJSON_GetObjectItemCaseSensitive(root,"error");
}
static void handle_command(ai_cmd_t *message)
{
    cJSON *root; const cJSON *result, *params, *error; const char *id, *method, *session, *version;
    if (message->epoch != s_running || !requested()) return;
    if (!ai_call_json_complete(message->data,message->length)) return;
    root = demo_json_parse_with_length_opts(message->data,(size_t)message->length+1U,NULL,1);
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return; }
    version=json_string(root,"jsonrpc");
    if (!version || strcmp(version,"2.0")) { cJSON_Delete(root); return; }
    id = json_string(root,"id"); method = json_string(root,"method"); result = cJSON_GetObjectItemCaseSensitive(root,"result");
    error=cJSON_GetObjectItemCaseSensitive(root,"error");
    if (s_phase == AI_NEGOTIATE && id && !strcmp(id,s_rpc_id)) {
        if (cJSON_GetObjectItemCaseSensitive(root,"method") ||
            ((result!=NULL)==(error!=NULL))) { cJSON_Delete(root); finish(AI_ERR_PROTOCOL); return; }
        startup_stage("ack_rx",message->received_at,0);
        startup_stage("ack_handled",tick(),0);
        session = json_string(result,"session_id");
        if (cJSON_GetObjectItemCaseSensitive(root,"error") || !session || !session[0] ||
            !profile(cJSON_GetObjectItemCaseSensitive(result,"input_audio")) || !profile(cJSON_GetObjectItemCaseSensitive(result,"output_audio"))) {
            cJSON_Delete(root); finish(AI_ERR_PROTOCOL); return;
        }
        /* Audio was prepared/primed before HTTP/WHIP. Re-preparing here would
         * reset the codec and discard the overlap that hides cold-start time. */
        if(!s_audio_owned || !s_audio_started || !requested() || !ready() || !route_current()) {
            cJSON_Delete(root); finish(requested() ? AI_ERR_NOT_READY : 0); return;
        }
        s_promoted_attempt=false;
        s_controls_dirty = true; s_phase = AI_ACTIVE; s_deadline = 0; s_timestamp = tick();
        startup_stage("session_ready",tick(),0);
        s_prime_wait_hint=s_play_guard && (pending(s_play_until) || !demo_ai_audio_play_done(&s_lease));
        show(s_prime_wait_hint ? TIRTC_AI_THINKING : TIRTC_AI_LISTENING,
             s_prime_wait_hint ? "正在准备音频" : "正在聆听");
        tirtc_status_event("AI 会话已就绪");
    } else if (s_phase == AI_ACTIVE && method) {
        params = cJSON_GetObjectItemCaseSensitive(root,"params");
#if DEMO_AI_AUDIO_AEC_ENABLE
        /* Terminal audio can still carry its final subtitle. New business
         * requests/rounds cannot restart a session already ending remotely. */
        if (s_remote_tail && strcmp(method,"caption") && strcmp(method,"interrupt")) {
            cJSON_Delete(root); return;
        }
#endif
        if (!strcmp(method,"device_action") || !strcmp(method,"call_intent") || !strcmp(method,"ai_call_intent")) {
            if (!result && !error) call_received(root);
            cJSON_Delete(root); return;
        }
        if (!command_notification(root)) { cJSON_Delete(root); return; }
        if (!strcmp(method,"caption")) caption(params);
        else if (!strcmp(method,"round_start")) { s_suppression_hint=s_prime_wait_hint=false; show(TIRTC_AI_SPEAKING,"AI 回复中"); }
        else if (!strcmp(method,"round_end")) { s_suppression_hint=s_prime_wait_hint=false; show(TIRTC_AI_LISTENING,"正在聆听"); }
        else if (!strcmp(method,"end_session")) {
            cJSON_Delete(root);
#if DEMO_AI_AUDIO_AEC_ENABLE
            begin_remote_tail();
#else
            finish(0);
#endif
            return;
        }
#if DEMO_AI_AUDIO_AEC_ENABLE
        else if (!strcmp(method,"interrupt")) {
            uint8_t speaker_level, mic_level;
            cJSON_Delete(root);
            if (s_remote_tail) { finish(0); return; }
            /* Drop the interrupted reply before the blocking Stop. Frames
             * arriving during Stop may already belong to the next reply. */
            discard_audio_queue(); s_play_guard=false; s_controls_dirty=true;
            if (demo_ai_audio_stop(&s_lease)) { finish(AI_ERR_AUDIO); return; }
            s_record_failure_active=false;
            /* Pro Stop deliberately closes its public ready gate even after
             * the AEC worker rebuilt VEM. Reopen this lease before more PCM,
             * but never revive a user-cancelled/invalidated session. */
            if(!requested() || !ready() || !route_current()) { finish(requested() ? AI_ERR_NOT_READY : 0); return; }
            liot_rtos_enter_critical();
            s_speaker_draining=false;
            speaker_level=s_ui.speaker_enabled && !s_speaker_mute_pending ? s_ui.volume : 0;
            mic_level=s_ui.mic_enabled ? s_ui.mic_gain : 0;
            liot_rtos_exit_critical();
            if(demo_ai_audio_prepare_session(&s_lease,speaker_level,mic_level)) finish(AI_ERR_AUDIO);
            return;
        }
#endif
    }
    cJSON_Delete(root);
}
static int command_slot(void)
{
    int found = -1; unsigned i;
    liot_rtos_enter_critical();
    for (i=0;i<AI_CMD_COUNT;++i) if (s_commands[i].used == 1 && (found < 0 || (int32_t)(s_commands[i].order-s_commands[found].order)<0)) found=(int)i;
    if (found >= 0) s_commands[found].used = 2;
    liot_rtos_exit_critical(); return found;
}
/* Only this worker consumes/resets packets. Keep the head reserved until all
 * decoded PCM is accepted, including when the audio queue reports BUSY. */
static uint32_t queued_audio_samples(void)
{
    uint32_t samples;
    liot_rtos_enter_critical(); samples=s_rx_samples; liot_rtos_exit_critical();
    return samples-s_decode_offset;
}
static void consume_rx_packet(void)
{
    liot_rtos_enter_critical();
    if(s_rx_count) {
        const ai_rx_packet_t *packet=&s_rx_packets[s_rx_head];
        s_rx_read=(s_rx_read+packet->length)%AI_RX_BYTES;
        s_rx_queued-=packet->length; s_rx_samples-=packet->samples;
        s_rx_head=(s_rx_head+1U)%AI_RX_PACKETS; s_rx_count--;
    }
    liot_rtos_exit_critical();
    s_decode_samples=s_decode_offset=0;
}
static uint16_t peek_audio(void)
{
    uint32_t first, samples; ai_rx_packet_t packet; int decoded;
    if(s_decode_offset==s_decode_samples) {
        liot_rtos_enter_critical();
        if(!s_rx_count) { liot_rtos_exit_critical(); return 0; }
        packet=s_rx_packets[s_rx_head];
        first=AI_RX_BYTES-s_rx_read; if(first>packet.length)first=packet.length;
        memcpy(s_play_opus,s_rx+s_rx_read,first);
        if(first<packet.length)memcpy(s_play_opus+first,s_rx,packet.length-first);
        liot_rtos_exit_critical();
        /* Decode a complete packet once, outside callbacks/critical sections.
         * A 120 ms packet can feed six 20 ms HAL blocks. */
        decoded=demo_ai_opus_decode(s_play_opus,packet.length,s_decode_pcm,
                                   DEMO_AI_OPUS_MAX_DECODE_SAMPLES);
        if(decoded!=packet.samples) {
            liot_rtos_enter_critical(); s_rx_drops++; s_rx_bad_frames++;
            s_rx_drop_bytes+=packet.length; liot_rtos_exit_critical();
            consume_rx_packet(); demo_ai_opus_reset_decoder(); return 0;
        }
        s_decode_samples=(uint16_t)decoded; s_decode_offset=0;
    }
    samples=s_decode_samples-s_decode_offset;
    if(samples>DEMO_AI_AUDIO_FRAME_SAMPLES)samples=DEMO_AI_AUDIO_FRAME_SAMPLES;
    memcpy(s_pcm,s_decode_pcm+s_decode_offset,samples*sizeof(s_pcm[0]));
    return (uint16_t)samples;
}
static void consume_audio(uint16_t samples)
{
    s_decode_offset+=samples;
    if(s_decode_offset==s_decode_samples)consume_rx_packet();
}
static bool playback_buffer_ready(bool speaker, uint8_t volume)
{
    uint32_t samples, now=tick();
    if(!speaker || !volume) {
        s_play_continuing=s_preroll_waiting=false;
        return true; /* Muted data is discarded by the normal bounded drain. */
    }
    /* FINISH can be delayed or stale. Rebuffer only after the estimated audio
     * tail has also elapsed, never on a transient notification alone. */
    if(s_play_continuing && (int32_t)(s_play_audio_until-now)<=0 &&
       demo_ai_audio_play_done(&s_lease)) {
        s_play_continuing=false; s_preroll_waiting=false;
        if(queued_audio_samples())s_rebuffers++;
    }
    if(s_play_continuing) return true;
    samples=queued_audio_samples();
    if(!samples) { s_preroll_waiting=false; return false; }
    if(!s_preroll_waiting) { s_preroll_waiting=true; s_preroll_since=now; }
    /* A short utterance/tail must not wait forever for another network packet.
     * Capture, commands, cancellation and identity checks continue every turn. */
    return samples>=AI_PREROLL_SAMPLES || (uint32_t)(now-s_preroll_since)>=AI_PREROLL_WAIT_MS;
}
static bool submit_playback(bool speaker, uint8_t volume)
{
    uint32_t began=tick(); unsigned submitted;
    if(!speaker || !volume) { discard_audio_queue(); return true; }
    if(!playback_buffer_ready(speaker,volume))return true;
    /* Bounded SDK look-ahead absorbs UI scheduling gaps. No recording blocks
     * this producer while a reply is buffered, playing or in tail protection. */
    for(submitted=0;submitted<AI_PLAY_BATCH;++submitted) {
        uint16_t samples, play_samples; bool controls_changed; int result;
        uint32_t duration, fractional, now, extra=0; int32_t ahead;
        if(!requested() || !ready() || !route_current())return false;
        liot_rtos_enter_critical(); controls_changed=s_controls_dirty; liot_rtos_exit_critical();
        if(controls_changed)return false;
        if(submitted && (uint32_t)(tick()-began)>=AI_PLAY_BUDGET_MS)break;
        samples=peek_audio(); if(!samples)break;
        /* Opus decoding can be preempted by STOP, mute or a route change.
         * Leave the decoded head for the existing cleanup/control path. */
        if(!requested() || !ready() || !route_current())return false;
        liot_rtos_enter_critical(); controls_changed=s_controls_dirty; liot_rtos_exit_critical();
        if(controls_changed)return false;
        play_samples=samples;
        fractional=play_samples+(s_play_continuing ? s_play_sample_remainder : 0U);
        duration=fractional/16U; now=tick();
        ahead=s_play_guard ? (int32_t)(s_play_audio_until-now) : 0;
        if(ahead>0 && (uint32_t)ahead+duration+(fractional%16U!=0)+
           (s_play_continuing ? 0U : DEMO_AI_AUDIO_WARMUP_MS)>AI_PLAY_AHEAD_MS)break;
        result=s_play_continuing ? demo_ai_audio_play(&s_lease,s_pcm,play_samples) :
            demo_ai_audio_play_warm(&s_lease,s_pcm,play_samples,&extra);
#if DEMO_AI_AUDIO_AEC_ENABLE
        /* The bounded AEC queue accepts the whole block or none. Backpressure
         * is normal: leave the ring head reserved and return to capture. */
        if(result==DEMO_AI_AUDIO_ERR_PLAY_BUSY) { s_play_retry=false; break; }
#endif
        if(!result) {
            now=tick();
            if(!s_play_guard || (int32_t)(s_play_audio_until-now)<=0)s_play_audio_until=now;
            s_play_audio_until+=duration+extra; s_play_sample_remainder=(uint8_t)(fractional%16U);
            if(!s_first_signal_logged) {
                uint32_t k, peak=0;
                for(k=0;k<play_samples;k++) {
                    int32_t value=s_pcm[k]; uint32_t magnitude=(uint32_t)(value<0 ? -value : value);
                    if(magnitude>peak) peak=magnitude;
                }
                if(peak>=512U) {
                    s_first_signal_logged=true;
                    startup_stage("first_signal_pcm",now,(int)peak);
                    TIRTC_LOG_DEBUG("[AI21] first signal software_peak=%lu ahead_ms=%lu extra_ms=%lu (not acoustic)\r\n",
                        (unsigned long)peak,(unsigned long)(s_play_audio_until-now-duration-extra),(unsigned long)extra);
                }
            }
            if(!s_first_pcm_logged) {
                s_first_pcm_logged=true; startup_stage("first_pcm_queued",now,(int)extra);
            }
            if(s_prime_wait_hint) { s_prime_wait_hint=false; show(TIRTC_AI_SPEAKING,"AI 回复中"); }
            s_played++; s_play_guard=true; s_play_until=s_play_audio_until+120U;
            s_play_continuing=true; s_preroll_waiting=false;
            consume_audio(samples); s_play_retry=false;
#if DEMO_AI_AUDIO_AEC_ENABLE
            if(s_remote_tail) { s_remote_tail_progress=now; s_remote_tail_quiet=false; }
#endif
        } else {
            s_play_errors++;
            /* A multi-block cold pre-roll may already own SDK/DMA buffers.
             * Its partial failure has no complete duration contract: stop the
             * session safely, never retry and duplicate the accepted silence. */
            if(result==DEMO_AI_AUDIO_ERR_WARM_PARTIAL) { finish(AI_ERR_AUDIO); return false; }
            if(!s_play_retry) { s_play_retry=true; s_play_retry_since=tick(); }
            else if((uint32_t)(tick()-s_play_retry_since)>=200U) { finish(AI_ERR_AUDIO); return false; }
            break;
        }
    }
    return true;
}
#if DEMO_AI_AUDIO_AEC_ENABLE
static void remote_tail_step(void)
{
    uint32_t now=tick(); bool empty, done=demo_ai_audio_play_done(&s_lease), complete=false;
    /* Quiet is a conservative application estimate, not a hardware DMA fence.
     * A healthy long reply has no total deadline; only lack of submissions
     * for three seconds is a stall. New media cancels the quiet window. */
    liot_rtos_enter_critical();
    /* Tail-only media skips the normal capture path, but must still release
     * its mute drain after both playback guards agree. The next turn restores
     * the requested level and can accept a late final utterance again. */
    if(s_play_guard && !pending(s_play_until) && done) s_play_guard=false;
    if(s_speaker_draining && !s_play_guard && done) s_speaker_draining=false;
    empty=s_rx_queued==0U;
    if(empty && done) {
        if(!s_remote_tail_quiet) { s_remote_tail_quiet=true; s_remote_tail_quiet_since=now; }
        else if((uint32_t)(now-s_remote_tail_quiet_since)>=AI_REMOTE_TAIL_QUIET_MS) {
            s_callbacks=false; complete=true;
        }
    } else s_remote_tail_quiet=false;
    liot_rtos_exit_critical();
    if(complete || (uint32_t)(now-s_remote_tail_progress)>=AI_REMOTE_TAIL_STALL_MS)
        finish(0);
}
#endif
static void account_capture_pause(void)
{
    uint32_t now=tick(), elapsed=now-s_capture_pause_at;
    s_timestamp+=elapsed; s_capture_pause_at=now;
    s_capture_pause_remainder+=elapsed;
    s_suppressed+=s_capture_pause_remainder/DEMO_AI_AUDIO_FRAME_MS;
    s_capture_pause_remainder%=DEMO_AI_AUDIO_FRAME_MS;
}
static bool pause_capture(void)
{
    uint32_t discarded_ms=0;
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_record_failure_active=false;
#endif
    if(!s_capture_paused) {
        if(demo_ai_audio_discard_capture_timed(&s_lease,&discarded_ms)) { finish(AI_ERR_AUDIO); return false; }
        s_timestamp+=discarded_ms;
        s_capture_paused=true; s_capture_pause_at=tick(); s_capture_pause_remainder=0;
    } else account_capture_pause();
    s_tx_failure_active=false;
    liot_rtos_enter_critical(); s_ui.audio_level=0; liot_rtos_exit_critical();
    return true;
}
static void media_step(void)
{
    int result; bool speaker, mic, dirty, suppress_capture, mute_edge, effective_speaker; uint8_t volume, mic_gain; unsigned i; uint32_t peak=0, discarded_ms=0;
#if DEMO_AI_AUDIO_AEC_ENABLE
    uint32_t mic_capture_generation;
#endif
    liot_rtos_enter_critical(); volume=s_ui.volume; mic_gain=s_ui.mic_gain;
    speaker=s_ui.speaker_enabled; mic=s_ui.mic_enabled;
    dirty=s_controls_dirty; s_controls_dirty=false;
    mute_edge=s_speaker_mute_pending; s_speaker_mute_pending=false;
    if(mute_edge)s_speaker_draining=true;
    effective_speaker=speaker && !s_speaker_draining; liot_rtos_exit_critical();
    if(mute_edge)discard_audio_queue();
    if (!mic || !mic_gain
#if !DEMO_AI_AUDIO_AEC_ENABLE
        || (speaker && volume && s_play_guard)
#endif
       ) s_tx_failure_active=false;
    if (dirty || volume!=s_applied_volume || mic_gain!=s_applied_mic_gain || effective_speaker!=s_applied_speaker || mic!=s_applied_mic) {
        if (demo_ai_audio_set_levels(&s_lease,effective_speaker ? volume : 0,mic ? mic_gain : 0)) { finish(AI_ERR_AUDIO); return; }
        /* Account for capture discarded by a level change before
         * timestamping the next fresh frame. */
        if (demo_ai_audio_discard_capture_timed(&s_lease,&discarded_ms)) { finish(AI_ERR_AUDIO); return; }
        s_timestamp += discarded_ms;
        s_applied_volume=volume; s_applied_mic_gain=mic_gain; s_applied_speaker=effective_speaker; s_applied_mic=mic;
    }
    {
        uint32_t now=tick();
        if(s_media_step_seen && (uint32_t)(now-s_media_step_at)>s_media_gap_max)s_media_gap_max=now-s_media_step_at;
        s_media_step_at=now; s_media_step_seen=true;
    }
    if(!effective_speaker || !volume)discard_audio_queue();
    if(!submit_playback(effective_speaker,volume))return;
    if(!requested())return;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(s_remote_tail) { remote_tail_step(); return; }
#endif
    if(s_play_guard && !pending(s_play_until)) {
        if(demo_ai_audio_play_done(&s_lease))s_play_guard=false;
        else if((uint32_t)(tick()-s_play_until)>=3000U) { finish(AI_ERR_AUDIO); return; }
    }
    if(s_speaker_draining) {
        /* Stop/Pause cannot safely flush this SDK. Keep codec muted until the
         * old estimated DMA tail AND FINISH agree; never resurrect old speech
         * on a quick off/on. Restore desired volume on the following turn. */
        if(!s_play_guard) {
            liot_rtos_enter_critical(); s_speaker_draining=false; liot_rtos_exit_critical();
        }
#if !DEMO_AI_AUDIO_AEC_ENABLE
        (void)pause_capture(); return;
#endif
    }
    suppress_capture=!mic || !mic_gain;
#if !DEMO_AI_AUDIO_AEC_ENABLE
    suppress_capture=suppress_capture || (speaker && volume &&
        (s_play_guard || s_preroll_waiting || queued_audio_samples()!=0));
#endif
    if(suppress_capture) { (void)pause_capture(); return; }
    if(s_capture_paused) {
#if DEMO_AI_AUDIO_AEC_ENABLE
        /* The AEC HAL opens a fresh mailbox on every capture. There is no
         * cached DMA batch requiring a discarded frame after unmute. */
        account_capture_pause(); s_capture_paused=false;
#else
        /* Resume barrier: the SDK starts a new RX DMA each time. Discard one
         * complete 20 ms frame before reopening uplink. Downlink arriving
         * during this wait is reconsidered next turn. */
        account_capture_pause();
        result=demo_ai_audio_record_20ms(&s_lease,s_record);
        if(!result)result=demo_ai_audio_discard_capture_timed(&s_lease,&discarded_ms);
        account_capture_pause();
        if(result) { finish(AI_ERR_AUDIO); return; }
        s_capture_paused=false;
        if(s_prime_wait_hint && requested() && ready() && route_current() && !s_play_guard && !queued_audio_samples()) {
            bool enabled;
            liot_rtos_enter_critical(); enabled=s_ui.mic_enabled && s_ui.mic_gain!=0; liot_rtos_exit_critical();
            if(enabled) { s_prime_wait_hint=false; show(TIRTC_AI_LISTENING,"正在聆听"); }
        }
        return;
#endif
    }
#if DEMO_AI_AUDIO_AEC_ENABLE
    liot_rtos_enter_critical(); mic_capture_generation=s_mic_capture_generation; liot_rtos_exit_critical();
#endif
    result=demo_ai_audio_record_20ms(&s_lease,s_record);
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(result==DEMO_AI_AUDIO_ERR_BUSY) { liot_rtos_task_sleep_ms(AI_PLAY_POLL_MS); return; }
    if(result==DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT) {
        uint32_t now=tick();
        liot_rtos_enter_critical();
        mic=s_ui.mic_enabled && s_ui.mic_gain && mic_capture_generation==s_mic_capture_generation;
        liot_rtos_exit_critical();
        if(!mic || !requested() || !ready() || !route_current()) {
            s_record_failure_active=false; return;
        }
        if(!s_record_failure_active || s_record_failure_generation!=mic_capture_generation) {
            s_record_failure_active=true; s_record_failure_since=now;
            s_record_failure_generation=mic_capture_generation;
        }
        if((uint32_t)(now-s_record_failure_since)>=AI_RECORD_FAILURE_LIMIT_MS)
            finish(AI_ERR_AUDIO);
        return; /* No PCM was accepted; keep serving playback while retrying. */
    }
    if(!result) s_record_failure_active=false;
#endif
    if (result) { finish(AI_ERR_AUDIO); return; }
    s_capture++;
    /* The SDK record call is synchronous; STOP can arrive during its wait. */
    if (!requested() || !ready() || !route_current()) return;
    /* A mute accepted while the synchronous capture waited applies to this frame. */
    liot_rtos_enter_critical(); mic=s_ui.mic_enabled && s_ui.mic_gain!=0; liot_rtos_exit_critical();
#if DEMO_AI_AUDIO_AEC_ENABLE
    liot_rtos_enter_critical();
    mic=mic && mic_capture_generation==s_mic_capture_generation;
    liot_rtos_exit_critical();
#endif
    if (s_play_guard && !pending(s_play_until)) {
        if (demo_ai_audio_play_done(&s_lease)) s_play_guard=false;
        else if ((uint32_t)(tick()-s_play_until)>=3000U) {
            /* FINISH is advisory and can be lost: stop explicitly, never leave
             * capture suppressed forever or assume the speaker has drained. */
            finish(AI_ERR_AUDIO); return;
        }
    }
    for(i=0;i<DEMO_AI_AUDIO_FRAME_SAMPLES;++i) { int32_t v=s_record[i]; uint32_t n=(uint32_t)(v<0 ? -v : v); if(n>peak)peak=n; }
    liot_rtos_enter_critical(); s_ui.audio_level=(uint8_t)(!mic || peak<512U ? 0U : peak<4096U ? 1U : peak<12288U ? 2U : 3U); liot_rtos_exit_critical();
    if (!mic || suppress_capture
#if !DEMO_AI_AUDIO_AEC_ENABLE
        || (speaker && volume && (s_play_guard || queued_audio_samples()!=0))
#endif
       ) {
        /* Do not upload stale captured data after protection/mute expires. */
        if(demo_ai_audio_discard_capture_timed(&s_lease,&discarded_ms)) { finish(AI_ERR_AUDIO); return; }
        if(mic && s_ui.ai_state==TIRTC_AI_LISTENING) { s_suppression_hint=true; show(TIRTC_AI_SPEAKING,"播放收尾，暂缓收音"); }
        s_tx_failure_active=false;
        s_suppressed+=(DEMO_AI_AUDIO_FRAME_MS+discarded_ms)/DEMO_AI_AUDIO_FRAME_MS;
        s_timestamp+=DEMO_AI_AUDIO_FRAME_MS+discarded_ms;
        s_capture_paused=true; s_capture_pause_at=tick(); s_capture_pause_remainder=0; return;
    }
    if(s_suppression_hint || s_prime_wait_hint) { s_suppression_hint=s_prime_wait_hint=false; show(TIRTC_AI_LISTENING,"正在聆听"); }
    {
        TIRTCFRAMEINFO frame; size_t used=0U;
        int encoded=demo_ai_opus_encode_20ms(s_record,s_uplink_opus,sizeof(s_uplink_opus));
        if(encoded<=0 || encoded>DEMO_AI_OPUS_MAX_PACKET) { finish(AI_ERR_AUDIO); return; }
        memset(&frame,0,sizeof(frame));
        frame.stream_id=1; frame.media=TIRTC_AUDIO_OPUS; frame.flags=TIRTC_AUDIOSAMPLE_16K16B1C;
#if DEMO_AI_AUDIO_AEC_ENABLE
        /* Fresh AEC capture can skip callbacks while the worker is busy. Use
         * its real completion time rather than compressing those gaps to 20ms. */
        s_timestamp=tick();
#endif
        result=demo_tirtc_get_send_buffer_used(DEMO_TIRTC_OWNER_AI,s_generation,&used);
        liot_rtos_enter_critical();
        mic=s_ui.mic_enabled && s_ui.mic_gain && s_want && s_request==s_running;
#if DEMO_AI_AUDIO_AEC_ENABLE
        mic=mic && mic_capture_generation==s_mic_capture_generation && !s_remote_tail;
#endif
        liot_rtos_exit_critical();
        if(!mic || !ready() || !route_current()) return;
        if(result==0 && used>AI_SEND_HIGH_WATER_BYTES) {
            /* Drop this fresh frame rather than queue more stale user speech.
             * Playback and capture continue; recovery sends only new PCM. */
            s_tx_drops++; s_timestamp+=DEMO_AI_AUDIO_FRAME_MS;
            s_tx_failure_active=false; return;
        }
        frame.ts=s_timestamp; frame.length=(uint32_t)encoded;
        if(result==0) result=demo_tirtc_send_audio(DEMO_TIRTC_OWNER_AI,s_generation,&frame,s_uplink_opus);
    }
    s_timestamp+=DEMO_AI_AUDIO_FRAME_MS;
    /* Native SendAudioStream returns >0 bytes queued, <0 an error. Do not
     * invent an error for a nonnegative result or compare it to PCM length. */
    if (result >= 0) {
        s_tx_failure_active=false;
        if (result > 0) s_tx++;
    } else {
        uint32_t now=tick(); bool first=!s_tx_failure_active;
        s_tx_drops++;
        if (first) { s_tx_failure_active=true; s_tx_failure_since=now; }
        if (!s_tx_failure_logged || (uint32_t)(now-s_tx_failure_last_log)>=AI_TX_FAILURE_LOG_MS) {
            s_tx_failure_logged=true;
            s_tx_failure_last_log=now;
            liot_trace("[AI] uplink send failed ret=%d consecutive_ms=%lu\r\n", result,
                       (unsigned long)(now-s_tx_failure_since));
        }
        if ((uint32_t)(now-s_tx_failure_since)>=AI_TX_FAILURE_LIMIT_MS)
            finish(AI_ERR_UPLINK);
    }
}
static void reset_attempt(uint32_t attempt, uint32_t credential, uint32_t clicked)
{
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_remote_tail=s_remote_tail_quiet=false;
    s_record_failure_active=false;
#endif
    s_tx_failure_active=s_tx_failure_logged=false; s_capture_paused=s_prime_wait_hint=false;
    s_media_step_seen=false; s_media_gap_max=s_rx_peak=s_rx_drop_bytes=s_rx_bad_frames=s_cmd_drops=s_play_errors=s_rebuffers=0;
    liot_rtos_enter_critical(); s_running=attempt; s_credential_epoch=credential;
    s_whip_done=s_lost=false; s_connection=NULL;
    s_speaker_mute_pending=s_speaker_draining=false;
    s_running_clicked_at=s_request==attempt && s_want ? s_clicked_at : clicked; s_first_rx_seen=s_first_rx_logged=s_first_pcm_logged=s_first_signal_logged=false;
    liot_rtos_exit_critical();
    s_play_guard=s_play_continuing=s_preroll_waiting=false;
    s_start_sent=s_access_ready=s_promoted_attempt=false;
    tirtc_network_get_snapshot(&s_route);
}
static bool prepare_audio(void)
{
    int result; uint32_t prewarm_ms=0; uint8_t speaker_level, mic_level;
    result=demo_ai_opus_init();
    if(result) { finish(AI_ERR_AUDIO); return false; }
    demo_ai_opus_reset();
    result=demo_ai_audio_acquire(DEMO_AI_AUDIO_OWNER_AI,&s_lease);
    if(result) { finish(AI_ERR_AUDIO); return false; } s_audio_owned=true;
    liot_rtos_enter_critical(); speaker_level=s_ui.speaker_enabled ? s_ui.volume : 0;
    mic_level=s_ui.mic_enabled ? s_ui.mic_gain : 0; liot_rtos_exit_critical();
    result=demo_ai_audio_prepare_session(&s_lease,speaker_level,mic_level);
    if(result) { finish(AI_ERR_AUDIO); return false; } s_audio_started=true;
    startup_stage("audio_ready",tick(),0);
    if(!requested() || !ready() || !route_current()) { finish(requested() ? AI_ERR_NOT_READY : 0); return false; }
    result=demo_ai_audio_prewarm_session(&s_lease,&prewarm_ms);
    if(result) { finish(AI_ERR_AUDIO); return false; }
    if(prewarm_ms) {
        s_play_audio_until=tick()+prewarm_ms; s_play_until=s_play_audio_until+120U;
        s_play_guard=true; /* Pure silence: first real frame still uses warm80. */
    }
    startup_stage("prewarm_queued",tick(),(int)prewarm_ms);
    if(!requested() || !ready() || !route_current()) { finish(requested() ? AI_ERR_NOT_READY : 0); return false; }
    return true;
}
static bool fetch_access(void)
{
    uint32_t started=tick(); int result=demo_binding_fetch_ai_access(&s_access);
    if(!attempt_current() || !ready() || !route_current()) { finish(requested() ? AI_ERR_NOT_READY : 0); return false; }
    if((uint32_t)(tick()-started)>=17000U) { finish(AI_ERR_TIMEOUT); return false; }
    if(result || !s_access.peer_id[0] || !s_access.token[0] || !s_access.device_id[0]) { finish(result ? result : AI_ERR_TOKEN); return false; }
    if(s_access.credential_epoch != s_credential_epoch) { finish(AI_ERR_NOT_READY); return false; }
    s_access_ready=true; s_access_at=tick();
    if(requested()) startup_stage("token_ready",tick(),0);
    return true;
}
static void submit_connect(void)
{
    int result;
    (void)text_copy(s_device_id,sizeof(s_device_id),s_access.device_id); (void)text_copy(s_role,sizeof(s_role),s_access.role_id);
    s_phase=s_standby ? AI_IDLE_CONNECT : AI_CONNECT; s_deadline=tick()+12000U;
    liot_rtos_enter_critical(); s_callbacks=true; liot_rtos_exit_critical();
    result=demo_tirtc_whip_connect(DEMO_TIRTC_OWNER_AI,s_generation,s_access.peer_id,s_access.token,connected,(void*)(uintptr_t)s_running);
    demo_binding_clear_ai_access(&s_access); s_access_ready=false;
    if(result) { s_resume_after_cleanup=s_standby && requested(); finish(result); }
}
static void claim_and_connect(void)
{
    int result; bool foreground=requested();
    if(s_access_ready && (uint32_t)(tick()-s_access_at)>=AI_ACCESS_TTL_MS) {
        demo_binding_clear_ai_access(&s_access); s_access_ready=false;
    }
    result=foreground ? demo_tirtc_session_claim(DEMO_TIRTC_OWNER_AI,&s_generation) : demo_tirtc_ai_standby_claim(&s_generation);
    if(result == TIRTC_E_BUSY) return; /* Preserve a user's click while another owner drains. */
    if(result) { s_resume_after_cleanup=s_standby && foreground; finish(result); return; }
    s_runtime_owned=true;
    if(foreground) {
        if(s_standby) startup_stage("begin",tick(),0);
        s_standby=false; s_phase=AI_TOKEN;
        clear_caption(); show(TIRTC_AI_THINKING,"正在连接 AI");
        if(!prepare_audio()) return;
    }
    /* Expired idle credentials are never used, nor fetched while holding an
     * idle lease. A foreground retry may fetch as in the release20 cold path. */
    if(!s_access_ready && !foreground) { finish(0); return; }
    if(!s_access_ready && !fetch_access()) return;
    if(!attempt_current() || !ready() || !route_current()) { finish(requested() ? AI_ERR_NOT_READY : 0); return; }
    if(s_standby && !demo_tirtc_ai_standby_valid(s_generation)) { s_resume_after_cleanup=requested(); finish(0); return; }
    submit_connect();
}
static void begin(void)
{
    uint32_t attempt, credential, clicked;
    liot_rtos_enter_critical();
    attempt=s_request; credential=s_requested_credential_epoch; clicked=s_clicked_at;
    s_standby=false;
    liot_rtos_exit_critical();
    reset_attempt(attempt,credential,clicked); startup_stage("begin",tick(),0);
    if(!requested()) { finish(0); return; }
    if(!ready() || !route_current()) { finish(AI_ERR_NOT_READY); return; }
    s_phase=AI_WAIT_OWNER; s_deadline=tick()+AI_OWNER_WAIT_MS;
    clear_caption(); show(TIRTC_AI_THINKING,"正在连接 AI");
    claim_and_connect();
}
static void standby_begin(void)
{
    demo_binding_snapshot_t binding; uint32_t attempt;
    demo_binding_get_snapshot(&binding);
    liot_rtos_enter_critical();
    /* A click between eligibility sampling and this critical section wins. */
    if(s_want || !s_home_eligible) { liot_rtos_exit_critical(); return; }
    if(!++s_request)++s_request;
    s_requested_credential_epoch=binding.credential_epoch; s_standby=true;
    attempt=s_running=s_request; s_phase=AI_IDLE_FETCH;
    liot_rtos_exit_critical();
    reset_attempt(attempt,binding.credential_epoch,0); s_phase=AI_IDLE_FETCH;
    if(!attempt_current() || !ready() || !route_current() || !demo_tirtc_ai_standby_allowed()) { finish(0); return; }
    TIRTC_LOG_DEBUG("[AI21] standby fetching attempt=%lu\r\n",(unsigned long)s_running);
    if(!fetch_access()) return;
    s_phase=AI_WAIT_OWNER; s_deadline=tick()+AI_OWNER_WAIT_MS;
    if(!requested() && !demo_tirtc_ai_standby_allowed()) { finish(0); return; }
    claim_and_connect();
}
static void idle_prepare_step(void)
{
    demo_binding_snapshot_t binding; tirtc_network_snapshot_t route; bool eligible;
    liot_rtos_enter_critical(); eligible=s_home_eligible && s_ui.mic_enabled && s_ui.mic_gain; liot_rtos_exit_critical();
    if(!eligible || !ready() || !demo_tirtc_ai_standby_allowed()) { s_idle_ready_seen=false; return; }
    if(s_idle_retry_pending) {
        if(pending(s_idle_retry_at)) return;
        s_idle_retry_pending=false;
    }
    demo_binding_get_snapshot(&binding); tirtc_network_get_snapshot(&route);
    if(!s_idle_ready_seen || s_idle_route!=route.generation || s_idle_epoch!=binding.credential_epoch || s_idle_sim!=route.sim_id) {
        s_idle_ready_seen=true; s_idle_ready_since=tick();
        s_idle_route=route.generation; s_idle_epoch=binding.credential_epoch; s_idle_sim=route.sim_id;
        return;
    }
    if((uint32_t)(tick()-s_idle_ready_since)>=AI_STANDBY_SETTLE_MS) standby_begin();
}
static void promote_standby(void)
{
    bool already_connected;
    int result=demo_tirtc_ai_standby_promote(s_generation);
    if(result) { s_resume_after_cleanup=requested(); finish(0); return; }
    liot_rtos_enter_critical();
    already_connected=s_whip_done && !s_whip_error && s_connection;
    s_standby=false; s_promoted_attempt=true; s_phase=already_connected ? AI_TOKEN : AI_CONNECT;
    liot_rtos_exit_critical(); s_idle_failures=0;
    s_running_clicked_at=s_clicked_at;
    startup_stage("begin",tick(),1);
    TIRTC_LOG_DEBUG("[AI21] standby hit attempt=%lu age_ms=%lu\r\n",(unsigned long)s_running,(unsigned long)(already_connected ? tick()-s_connected_at : 0U));
    clear_caption(); show(TIRTC_AI_THINKING,"正在连接 AI");
    if(!prepare_audio()) return;
    if(!already_connected) return; /* Existing WHIP callback keeps its attempt id. */
    startup_stage("connected",tick(),1);
    s_phase=AI_NEGOTIATE; s_deadline=tick()+10000U;
    if(start_command()<0) finish(AI_ERR_PROTOCOL);
}
static void diagnostics(void)
{
    uint32_t now=tick(), rx_frames, rx_drops, queued, peak, dropped_bytes, bad, cmd_drops; char text[256];
    if ((uint32_t)(now-s_last_log)<5000U) return;
    s_last_log=now;
    liot_rtos_enter_critical(); rx_frames=s_rx_frames; rx_drops=s_rx_drops;
    queued=s_rx_queued; peak=s_rx_peak; dropped_bytes=s_rx_drop_bytes; bad=s_rx_bad_frames; cmd_drops=s_cmd_drops;
    liot_rtos_exit_critical();
    (void)snprintf(text,sizeof(text),"AI 16k 单声道 Opus\n采集 %lu / 发送 %lu\n接收 %lu / 播放 %lu\n音频丢包 上行 %lu 下行 %lu\n上行抑制 %lu帧（静音/播放防回声）",
        (unsigned long)s_capture,(unsigned long)s_tx,(unsigned long)rx_frames,(unsigned long)s_played,
        (unsigned long)s_tx_drops,(unsigned long)rx_drops,(unsigned long)s_suppressed);
    liot_rtos_enter_critical(); (void)text_copy(s_ui.diagnostic_audio,sizeof(s_ui.diagnostic_audio),text); s_ui.audio_valid=true; liot_rtos_exit_critical();
    /* UI counters remain active; detailed serial output is debug-only. */
    TIRTC_LOG_DEBUG("[AI18] phase=%u cap=%lu tx=%lu rx=%lu play=%lu audio_drop=%lu/%lu mute=%lu q=%lu peak=%lu drop_bytes=%lu bad=%lu cmd_drop=%lu play_err=%lu gap_max=%lu rebuf=%lu\r\n",(unsigned)s_phase,
        (unsigned long)s_capture,(unsigned long)s_tx,(unsigned long)rx_frames,(unsigned long)s_played,
        (unsigned long)s_tx_drops,(unsigned long)rx_drops,(unsigned long)s_suppressed,
        (unsigned long)queued,(unsigned long)peak,(unsigned long)dropped_bytes,(unsigned long)bad,
        (unsigned long)cmd_drops,(unsigned long)s_play_errors,(unsigned long)s_media_gap_max,(unsigned long)s_rebuffers);
}
static void step(void)
{
    int slot; unsigned commands=0; bool want, done, lost; int error;
    liot_rtos_enter_critical(); want=s_want; done=s_whip_done; lost=s_lost; error=s_lost_error; liot_rtos_exit_critical();
    if(s_phase==AI_STOPPING) cleanup_step();
    else if(s_phase==AI_BLOCKED) { /* Diagnostics/UI only; device restart required. */ }
    else if(s_phase==AI_IDLE || s_phase==AI_ERROR) { if(want)begin(); else idle_prepare_step(); }
    else if(s_standby) {
        if(!attempt_current() || !ready() || !route_current()) finish(requested() ? AI_ERR_NOT_READY : 0);
        else if(!s_runtime_owned && !requested() && !demo_tirtc_ai_standby_allowed()) finish(0);
        else if(s_runtime_owned && !demo_tirtc_ai_standby_valid(s_generation)) { s_resume_after_cleanup=requested(); finish(0); }
        else if(lost) { s_resume_after_cleanup=requested(); finish(error); }
        else if(s_phase==AI_WAIT_OWNER) {
            if(!pending(s_deadline)) { s_resume_after_cleanup=requested(); finish(AI_ERR_TIMEOUT); }
            else claim_and_connect();
        } else if(s_phase==AI_IDLE_CONNECT) {
            if(done) {
                if(s_whip_error || !s_connection) { s_resume_after_cleanup=requested(); finish(s_whip_error ? s_whip_error : AI_ERR_LOST); }
                else {
                    s_phase=AI_IDLE_READY;
                    TIRTC_LOG_DEBUG("[AI21] standby ready attempt=%lu\r\n",(unsigned long)s_running);
                    if(requested()) promote_standby();
                }
            } else if(!pending(s_deadline)) { s_resume_after_cleanup=requested(); finish(AI_ERR_TIMEOUT); }
            else if(requested()) promote_standby();
        } else if(s_phase==AI_IDLE_READY && requested()) promote_standby();
    }
    else if(!requested()) finish(0);
    else if(!ready() || !route_current()) finish(AI_ERR_NOT_READY);
    else if(lost
#if DEMO_AI_AUDIO_AEC_ENABLE
        && !(s_phase==AI_ACTIVE && error==TIRTC_E_CONN_REMOTECLOSE)
#endif
        ) finish(error);
    else {
#if DEMO_AI_AUDIO_AEC_ENABLE
        if(s_phase==AI_ACTIVE && lost) begin_remote_tail();
        if(s_phase==AI_ACTIVE && s_remote_tail && tirtc_calls_has_session()) { finish(0); return; }
#endif
        if(s_phase==AI_WAIT_OWNER) {
            if(!pending(s_deadline)) finish(AI_ERR_TIMEOUT);
            else claim_and_connect();
        }
        if(s_phase==AI_CONNECT && done) {
            startup_stage("connected",s_connected_at,s_whip_error);
            if(s_whip_error || !s_connection) finish(s_whip_error ? s_whip_error : AI_ERR_LOST);
            else { s_phase=AI_NEGOTIATE; s_deadline=tick()+10000U; if(start_command()<0)finish(AI_ERR_PROTOCOL); }
        }
        /* A command stream must not starve capture, deadlines or cancellation. */
        while(s_phase!=AI_STOPPING && commands++<AI_CMD_COUNT && (slot=command_slot())>=0) {
            handle_command(&s_commands[slot]); liot_rtos_enter_critical(); s_commands[slot].used=0; liot_rtos_exit_critical();
        }
        if((s_phase==AI_CONNECT || s_phase==AI_NEGOTIATE) && !pending(s_deadline)) finish(AI_ERR_TIMEOUT);
        if(s_phase==AI_ACTIVE
#if DEMO_AI_AUDIO_AEC_ENABLE
            && !s_remote_tail
#endif
            ) call_lookup_step();
        if(s_phase==AI_ACTIVE) media_step();
    }
    {
        bool first; uint32_t at;
        liot_rtos_enter_critical(); first=s_first_rx_seen&&!s_first_rx_logged; at=s_first_rx_at;
        if(first)s_first_rx_logged=true;
        liot_rtos_exit_critical();
        if(first)startup_stage("first_rx",at,0);
    }
    diagnostics();
    if((uint32_t)(tick()-s_last_ui)>=200U) { s_last_ui=tick(); tirtc_ai_publish(); }
}
static void worker(void *arg)
{
    (void)arg; demo_tirtc_set_feature_listener(DEMO_TIRTC_FEATURE_AI,&s_listener);
    liot_trace("[AI-CALL47] enabled default=video explicit_audio=1 legacy=1\r\n");
    show(TIRTC_AI_IDLE,"点击开始");
    for(;;) {
        step();
        if(s_phase==AI_ACTIVE && (s_capture_paused
#if DEMO_AI_AUDIO_AEC_ENABLE
            || s_remote_tail
#else
            || queued_audio_samples()
#endif
            ))
            liot_rtos_task_sleep_ms(AI_PLAY_POLL_MS); /* Yield even with queued wake tokens. */
        else (void)liot_rtos_semaphore_wait(s_sem,s_phase==AI_ACTIVE ? LIOT_NO_WAIT : 20U);
    }
}
int tirtc_ai_start_service(void)
{
    liot_task_t task=NULL; int result;
    liot_rtos_enter_critical(); if(s_available || s_starting) { liot_rtos_exit_critical(); return 0; } s_starting=true; liot_rtos_exit_critical();
    result=s_sem ? 0 : (int)liot_rtos_semaphore_create(&s_sem,0);
    /* Match Lite's Opus worker stack: the fixed-point codec uses nested VLAs. */
    if(!result) result=(int)liot_rtos_task_create(&task,96U*1024U,13,"tirtc_ai",worker,NULL);
    liot_rtos_enter_critical(); s_starting=false; if(!result && task) { s_task=task; s_available=true; } liot_rtos_exit_critical();
    return !result && task ? 0 : -1;
}
bool tirtc_ai_has_session(void)
{
    bool busy;
    liot_rtos_enter_critical();
    busy=s_want || s_runtime_owned || s_audio_owned ||
         (s_phase!=AI_IDLE && s_phase!=AI_ERROR);
    liot_rtos_exit_critical();
    return busy;
}
int tirtc_ai_action(const tirtc_ui_action_t *a)
{
    bool stop=false;
    if(!a || !s_available) return -1;
    if(a->type==TIRTC_ACTION_RETURN_HOME) {
        liot_rtos_enter_critical(); s_home_eligible=true; liot_rtos_exit_critical();
        signal_worker(); return 0; /* Home preserves the active session. */
    }
    if(a->type==TIRTC_ACTION_ENTER_PAGE) {
        if(a->value<0 || a->value>=TIRTC_PAGE_COUNT)return -1;
        liot_rtos_enter_critical();
        s_home_eligible=a->value==TIRTC_PAGE_HOME || a->value==TIRTC_PAGE_CLOCK;
        if(!s_home_eligible) s_idle_ready_seen=false;
        stop=s_standby && !s_home_eligible;
        liot_rtos_exit_critical();
        stop=stop || (a->value!=TIRTC_PAGE_HOME && a->value!=TIRTC_PAGE_CLOCK && a->value!=TIRTC_PAGE_MENU && a->value!=TIRTC_PAGE_DIAGNOSTICS);
        if(!stop) return 0;
    } else if(a->type==TIRTC_ACTION_AI_START) {
        demo_binding_snapshot_t binding;
        demo_binding_get_snapshot(&binding);
        if(!binding.credential_epoch || !ready()) return -1;
        liot_rtos_enter_critical();
        if(s_want) { liot_rtos_exit_critical(); return 0; }
        if(s_standby && s_phase!=AI_STOPPING && s_phase!=AI_BLOCKED) {
            if(!s_ui.mic_enabled || !s_ui.mic_gain) { liot_rtos_exit_critical(); return -1; }
            if(s_request!=s_running) { if(!++s_request)++s_request; s_resume_after_cleanup=true; }
            s_clicked_at=s_running_clicked_at=tick(); s_requested_credential_epoch=binding.credential_epoch; s_want=true;
            liot_rtos_exit_critical(); signal_worker(); return 0;
        }
        if(s_phase==AI_STOPPING && s_background_cleanup) {
            if(!s_ui.mic_enabled || !s_ui.mic_gain) { liot_rtos_exit_critical(); return -1; }
            s_clicked_at=tick(); s_requested_credential_epoch=binding.credential_epoch;
            s_want=true; if(!++s_request)++s_request; s_resume_after_cleanup=true;
            liot_rtos_exit_critical(); signal_worker(); return 0;
        }
        if((s_phase!=AI_IDLE && s_phase!=AI_ERROR) || s_runtime_owned || s_audio_owned || !s_ui.mic_enabled || !s_ui.mic_gain) { liot_rtos_exit_critical(); return -1; }
        if(!++s_request)++s_request;
        s_requested_credential_epoch=binding.credential_epoch; s_clicked_at=tick();
        s_want=true; liot_rtos_exit_critical(); signal_worker(); return 0;
    } else if(a->type==TIRTC_ACTION_AI_STOP) stop=true;
    else if(a->type==TIRTC_ACTION_SET_SPEAKER_VOLUME || a->type==TIRTC_ACTION_SET_MIC_GAIN || a->type==TIRTC_ACTION_SET_SPEAKER_ENABLED || a->type==TIRTC_ACTION_SET_MIC_ENABLED) {
        bool level=a->type==TIRTC_ACTION_SET_SPEAKER_VOLUME || a->type==TIRTC_ACTION_SET_MIC_GAIN;
        if(a->value<0 || (level ? a->value>10 : a->value>1)) return -1;
        liot_rtos_enter_critical();
        if(s_ui.speaker_enabled && s_ui.volume &&
           ((a->type==TIRTC_ACTION_SET_SPEAKER_VOLUME && !a->value) ||
            (a->type==TIRTC_ACTION_SET_SPEAKER_ENABLED && !a->value)))
            s_speaker_mute_pending=true;
        if(a->type==TIRTC_ACTION_SET_SPEAKER_VOLUME)s_ui.volume=(uint8_t)a->value;
#if DEMO_AI_AUDIO_AEC_ENABLE
        if((a->type==TIRTC_ACTION_SET_MIC_GAIN && s_ui.mic_gain!=(uint8_t)a->value) ||
           (a->type==TIRTC_ACTION_SET_MIC_ENABLED && s_ui.mic_enabled!=(a->value!=0)))
            ++s_mic_capture_generation;
#endif
        if(a->type==TIRTC_ACTION_SET_MIC_GAIN)s_ui.mic_gain=(uint8_t)a->value;
        if(a->type==TIRTC_ACTION_SET_SPEAKER_ENABLED)s_ui.speaker_enabled=a->value!=0;
        if(a->type==TIRTC_ACTION_SET_MIC_ENABLED)s_ui.mic_enabled=a->value!=0;
        s_controls_dirty=true; liot_rtos_exit_critical(); signal_worker(); return 0;
    } else return -1;
    if(stop) { liot_rtos_enter_critical(); s_resume_after_cleanup=false; s_idle_retry_at=tick()+5000U; s_idle_retry_pending=true; s_idle_ready_seen=false; if(s_want || s_standby) { s_want=false; if(!++s_request)++s_request; } liot_rtos_exit_critical(); signal_worker(); }
    return 0;
}
