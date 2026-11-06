/* SPDX-License-Identifier: MIT
 * Authenticated platform VIEW: camera 11, microphone 10, talkback 10/14.
 * SDK callbacks only latch bounded state or copy into the audio mailbox.
 * One control worker owns media start/step/stop; the existing camera and
 * audio workers retain their hardware fences and generation-scoped leases.
 */
#include "tirtc_remote.h"
#include "../runtime/tirtc_runtime.h"
#include "../calls/tirtc_calls.h"
#include "../calls/calls_audio.h"
#include "../ai/tirtc_ai.h"
#include "../video/tirtc_video.h"
#include "../preferences/tirtc_preferences.h"
#include "../status/tirtc_status.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include <stdio.h>
#include <string.h>

#define REMOTE_ERROR (-6401)
#define REMOTE_STALE (-6402)
#define REMOTE_CLEANUP (-6403)
#define REMOTE_CLEANUP_MS 15000U
#define REMOTE_IDLE_MS 30000U
#define LIVE_HANGUP 0x1104U
#define LIVE_REQ_AUDIO 0x1106U
#define LIVE_SEND_AUDIO 0x1108U
#define LIVE_RESPONSE 0x8000U
#define OWNER DEMO_TIRTC_OWNER_LIVE

typedef enum { R_IDLE, R_STARTING, R_ACTIVE, R_CLOSING, R_BLOCKED } remote_phase_t;
static liot_task_t s_task;
static bool s_creating, s_available, s_adopting, s_owned;
static remote_phase_t s_phase;
static tirtc_conn_t s_connection;
static uint32_t s_generation, s_started, s_close_at, s_last_publish, s_rx_at;
static bool s_stop, s_remote_end, s_audio_started, s_video_started, s_disconnect_sent;
static bool s_microphone=true, s_speaker=true, s_camera=true;
static bool s_audio_subscribed, s_video_subscribed, s_remote_audio=true, s_rx_seen;
static bool s_dirty, s_hangup_sent;
static int s_error;
static tirtc_ui_page_t s_page=TIRTC_PAGE_HOME;
static demo_binding_service_context_t s_context;
static tirtc_ui_remote_t s_ui;

static uint32_t now(void) { return liot_rtos_get_running_time(); }
static bool page_allowed(tirtc_ui_page_t page)
{ return page==TIRTC_PAGE_HOME || page==TIRTC_PAGE_CLOCK; }
static bool matches(tirtc_conn_t connection)
{
    /* Called under the same short critical section as stop/adoption. The
     * STARTING reservation also latches terminal callbacks during adoption. */
    return connection && connection==s_connection &&
           (s_phase==R_STARTING || s_phase==R_ACTIVE) && !s_stop;
}
/* RAM-only setters, safe inside the controller critical section. Updating
 * the audio revision here also invalidates a Record already in progress;
 * a later worker tick must never reopen a newer mute/unsubscribe decision. */
static int apply_controls_locked(void)
{
    int result=0;
    if (s_audio_started)
        result=calls_audio_set_session_controls(OWNER,s_generation,
                s_speaker && !s_stop,s_microphone && s_remote_audio && !s_stop,
                s_audio_subscribed && !s_stop);
    if (s_video_started)
        (void)tirtc_video_set_enabled(s_generation,s_camera && s_video_subscribed && !s_stop);
    return result;
}
bool tirtc_remote_has_session(void)
{
    bool busy;
    liot_rtos_enter_critical(); busy=s_phase!=R_IDLE; liot_rtos_exit_critical();
    return busy;
}
void tirtc_remote_get_snapshot(tirtc_ui_remote_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical(); *out=s_ui; liot_rtos_exit_critical();
}
static void publish(tirtc_ui_remote_state_t state, const char *message)
{
    tirtc_ui_remote_t ui;
    tirtc_preferences_t preferences;
    bool changed;
    uint32_t stamp=now();
    tirtc_preferences_get(&preferences);
    liot_rtos_enter_critical();
    changed=s_ui.state!=state || s_ui.error!=s_error;
    s_ui.state=state; s_ui.generation=s_generation; s_ui.error=s_error;
    s_ui.seconds=s_started ? (uint32_t)(stamp-s_started)/1000U : 0;
    s_ui.mic_enabled=s_microphone && preferences.mic_enabled && preferences.mic_gain;
    s_ui.speaker_enabled=s_speaker && preferences.speaker_enabled && preferences.volume;
    s_ui.camera_enabled=s_camera;
    s_ui.audio_subscribed=s_audio_subscribed; s_ui.video_subscribed=s_video_subscribed;
    s_ui.talkback_active=s_rx_seen && (uint32_t)(stamp-s_rx_at)<1500U;
    if (++s_ui.revision==0U) ++s_ui.revision;
    (void)snprintf(s_ui.peer_name,sizeof(s_ui.peer_name),"远程客户端");
    if (state==TIRTC_REMOTE_ACTIVE &&
        (!preferences.mic_enabled || !preferences.mic_gain ||
         !preferences.speaker_enabled || !preferences.volume))
        message="静音限制：请在设备设置中开启音频";
    (void)snprintf(s_ui.message,sizeof(s_ui.message),"%s",message);
    ui=s_ui; s_dirty=false; s_last_publish=stamp;
    liot_rtos_exit_critical();
    (void)tirtc_ui_publish_remote(&ui);
    if (changed) {
        liot_trace("[LIVE42] state=%u gen=%lu error=%d\r\n",
                   (unsigned)state,(unsigned long)ui.generation,ui.error);
        tirtc_status_event(message);
    }
}
static int accepted(tirtc_conn_t connection)
{
    demo_binding_service_context_t context;
    tirtc_ui_page_t rendered_page=TIRTC_PAGE_COUNT, policy_page;
    uint32_t generation=0;
    bool eligible, rendered, call_busy, ai_busy, bound, available;
    remote_phase_t phase;
    int result=-1;
    if (!connection || !demo_tirtc_admission_try_enter()) {
        liot_trace("[LIVE44] admission unavailable handle=%p\r\n",connection);
        return -1;
    }
    rendered=tirtc_ui_get_rendered_page(&rendered_page);
    call_busy=tirtc_calls_has_session(); ai_busy=tirtc_ai_has_session();
    bound=demo_binding_service_context(&context);
    eligible=rendered && page_allowed(rendered_page) && !call_busy && !ai_busy && bound;
    liot_rtos_enter_critical();
    policy_page=s_page; phase=s_phase; available=s_available;
    eligible=eligible && s_available && s_phase==R_IDLE && page_allowed(s_page);
    if (eligible) {
        s_phase=R_STARTING; s_adopting=true; s_connection=connection;
        s_stop=s_remote_end=s_rx_seen=false; s_error=0; s_generation=0;
        s_microphone=s_speaker=s_camera=s_remote_audio=true;
        s_audio_subscribed=s_video_subscribed=false;
        s_audio_started=s_video_started=s_disconnect_sent=s_hangup_sent=false;
        s_started=0; s_context=context; s_dirty=true;
    }
    liot_rtos_exit_critical();
    if (eligible) {
        result=demo_tirtc_adopt_incoming(OWNER,connection,&generation);
        liot_rtos_enter_critical();
        if (!result) { s_owned=true; s_generation=generation; }
        else { s_phase=R_IDLE; s_connection=NULL; s_stop=false; }
        s_adopting=false;
        liot_rtos_exit_critical();
    }
    demo_tirtc_admission_leave();
    liot_trace("[LIVE42] incoming %s gen=%lu result=%d\r\n",
               result ? "rejected" : "accepted",(unsigned long)generation,result);
    if (result)
        TIRTC_LOG_DEBUG("[LIVE44] admission detail rendered=%u page=%u policy=%u phase=%u available=%u ai=%u call=%u bound=%u\r\n",
                   rendered?1U:0U,(unsigned)rendered_page,(unsigned)policy_page,
                   (unsigned)phase,available?1U:0U,ai_busy?1U:0U,call_busy?1U:0U,bound?1U:0U);
    return result;
}
static void connection_error(tirtc_conn_t connection,int error)
{
    liot_rtos_enter_critical();
    if (matches(connection)) {
        s_error=error ? error : REMOTE_ERROR; s_remote_end=true; s_stop=true;
        (void)apply_controls_locked();
    }
    liot_rtos_exit_critical();
}
static void disconnected(tirtc_conn_t connection)
{
    liot_rtos_enter_critical();
    if (matches(connection)) { s_remote_end=true; s_stop=true; (void)apply_controls_locked(); }
    liot_rtos_exit_critical();
}
static void audio_received(tirtc_conn_t connection,const TIRTCFRAMEINFO *frame,void *data)
{
    uint32_t generation=0;
    if (!frame || !data || !frame->length || frame->length>640U ||
        frame->media!=TIRTC_AUDIO_ALAW || (frame->stream_id!=10U && frame->stream_id!=14U) ||
        (frame->flags!=TIRTC_AUDIOSAMPLE_8K16B1C && frame->flags!=TIRTC_AUDIOSAMPLE_16K16B1C)) return;
    liot_rtos_enter_critical();
    if (matches(connection) && s_phase==R_ACTIVE && s_owned) {
        generation=s_generation; s_rx_seen=true; s_rx_at=now();
    }
    liot_rtos_exit_critical();
    if (generation) calls_audio_receive(OWNER,generation,frame,data);
}
static int subscribe_audio(tirtc_conn_t connection,uint8_t stream)
{
    bool ok;
    liot_rtos_enter_critical(); ok=matches(connection) && stream==10U;
    if (ok) { s_audio_subscribed=true; s_remote_audio=true; s_dirty=true; (void)apply_controls_locked(); }
    liot_rtos_exit_critical();
    return ok ? 0 : -1;
}
static void unsubscribe_audio(tirtc_conn_t connection,uint8_t stream)
{
    liot_rtos_enter_critical();
    if (matches(connection) && stream==10U) { s_audio_subscribed=false; s_dirty=true; (void)apply_controls_locked(); }
    liot_rtos_exit_critical();
}
static int subscribe_video(tirtc_conn_t connection,uint8_t stream)
{
    bool ok;
    uint32_t generation;
    liot_rtos_enter_critical(); ok=matches(connection) && stream==11U;
    generation=s_generation;
    if (ok) { s_video_subscribed=true; s_dirty=true; (void)apply_controls_locked(); }
    liot_rtos_exit_critical();
    liot_trace("[LIVE44] video subscribe gen=%lu stream=%u accepted=%u\r\n",
               (unsigned long)generation,(unsigned)stream,ok?1U:0U);
    return ok ? 0 : -1;
}
static void unsubscribe_video(tirtc_conn_t connection,uint8_t stream)
{
    liot_rtos_enter_critical();
    if (matches(connection) && stream==11U) { s_video_subscribed=false; s_dirty=true; (void)apply_controls_locked(); }
    liot_rtos_exit_critical();
}
static void command_received(tirtc_conn_t connection,uint32_t word,const void *data,uint32_t length)
{
    uint16_t command=(uint16_t)(word & 0x7fffU);
    if (word & LIVE_RESPONSE) return;
    if ((command==LIVE_REQ_AUDIO || command==LIVE_SEND_AUDIO) &&
        (length>1U || (length && (!data || *(const uint8_t *)data>1U)))) return;
    liot_rtos_enter_critical();
    if (matches(connection)) {
        if (command==LIVE_HANGUP) { s_remote_end=true; s_stop=true; }
        else if (command==LIVE_REQ_AUDIO || command==LIVE_SEND_AUDIO) {
            s_remote_audio=!length || *(const uint8_t *)data!=0; s_dirty=true;
        }
        (void)apply_controls_locked();
    }
    liot_rtos_exit_critical();
}
static const demo_tirtc_incoming_listener_t s_listener={
    .on_conn_accepted=accepted,.on_conn_error=connection_error,.on_disconnected=disconnected,
    .on_audio=audio_received,.on_command=command_received,
    .on_subscribe_audio=subscribe_audio,.on_unsubscribe_audio=unsubscribe_audio,
    .on_subscribe_video=subscribe_video,.on_unsubscribe_video=unsubscribe_video
};
static void begin_close(int error)
{
    liot_rtos_enter_critical();
    if (s_phase==R_CLOSING || s_phase==R_BLOCKED) { liot_rtos_exit_critical(); return; }
    s_phase=R_CLOSING; s_stop=true; if (!s_error) s_error=error; s_close_at=now();
    s_audio_subscribed=s_video_subscribed=false; s_dirty=true;
    liot_rtos_exit_critical();
    if (s_video_started) tirtc_video_stop(s_generation);
    /* Scoped even on partially failed start: never stop a different owner. */
    (void)calls_audio_stop(OWNER,s_generation);
    publish(TIRTC_REMOTE_CLOSING,"正在结束远程监控");
}
static void cleanup_step(void)
{
    int result;
    if (!s_hangup_sent) {
        s_hangup_sent=true;
        if (!s_remote_end)
            (void)demo_tirtc_send_command(OWNER,s_generation,
                    ((s_generation & 0xffffU)<<16)|LIVE_HANGUP,NULL,0);
    }
    /* Stop transport promptly; retain the session until capture/DMA and
     * SDK callbacks are quiescent. Releasing a timed-out lease is unsafe. */
    if (!s_disconnect_sent) {
        result=demo_tirtc_disconnect(OWNER,s_generation);
        if (!result || result==TIRTC_E_INVALID_HANDLE) s_disconnect_sent=true;
    }
    if (s_video_started) {
        tirtc_video_stop(s_generation);
        if (!tirtc_video_is_stopped(s_generation)) goto waiting;
        s_video_started=false;
    }
    if (calls_audio_stop(OWNER,s_generation)) goto waiting;
    s_audio_started=false;
    if (s_owned) {
        result=demo_tirtc_session_release(OWNER,s_generation);
        if (result) goto waiting;
        s_owned=false;
    }
    liot_rtos_enter_critical(); s_connection=NULL; s_rx_seen=false; liot_rtos_exit_critical();
    publish(s_error ? TIRTC_REMOTE_ERROR : TIRTC_REMOTE_ENDED,
            s_error ? "远程连接中断" : "远程监控已结束");
    liot_rtos_enter_critical(); s_phase=R_IDLE; liot_rtos_exit_critical();
    return;
waiting:
    /* Warn once, but keep the same media/SDK release gates alive for late
     * completion. A timeout never authorizes reusing their resources. */
    if (s_phase!=R_BLOCKED && (uint32_t)(now()-s_close_at)>=REMOTE_CLEANUP_MS) {
        liot_rtos_enter_critical(); s_phase=R_BLOCKED; s_error=REMOTE_CLEANUP; liot_rtos_exit_critical();
        publish(TIRTC_REMOTE_CLOSING,"正在后台结束远程查看，请稍后重试");
    }
}
static void start_media(void)
{
    int result, first, second;
    bool stopped;
    uint8_t request=1;
    publish(TIRTC_REMOTE_CONNECTING,"远程连接中");
    result=calls_audio_start(OWNER,s_generation);
    if (result) { begin_close(result); return; }
    liot_rtos_enter_critical();
    s_audio_started=true; stopped=s_stop;
    /* Keep capture closed until the controller is ACTIVE. */
    liot_rtos_exit_critical();
    if (stopped) { begin_close(0); return; }
    /* The platform supports the current H5 talkback14 and native client10. */
    first=demo_tirtc_subscribe_audio(OWNER,s_generation,10U);
    second=demo_tirtc_subscribe_audio(OWNER,s_generation,14U);
    liot_trace("[LIVE42] downlink subscribe10/14=%d/%d\r\n",first,second);
    if (first<0 && second<0) { begin_close(first); return; }
    result=demo_tirtc_send_command(OWNER,s_generation,
            ((s_generation & 0xffffU)<<16)|LIVE_REQ_AUDIO,&request,1U);
    liot_trace("[LIVE42] request talkback=%d; up=10 ALAW8k + 11 MJPEG; down=10/14 ALAW8k/16k\r\n",result);
    liot_rtos_enter_critical();
    if (!s_stop) { s_phase=R_ACTIVE; s_started=now(); s_dirty=true; }
    liot_rtos_exit_critical();
}
static void step(void)
{
    remote_phase_t phase;
    bool stop, adopting, video, dirty;
    int error,result;
    tirtc_video_snapshot_t status;
    liot_rtos_enter_critical();
    phase=s_phase; stop=s_stop; adopting=s_adopting; error=s_error;
    video=s_video_subscribed; dirty=s_dirty;
    liot_rtos_exit_critical();
    if (adopting || phase==R_IDLE) return;
    if (phase==R_CLOSING || phase==R_BLOCKED) { cleanup_step(); return; }
    if (!demo_tirtc_is_ready() || !demo_binding_service_context_current(&s_context)) {
        begin_close(REMOTE_STALE); return;
    }
    if (stop || error) { begin_close(error); return; }
    if (phase==R_STARTING) { start_media(); return; }
    liot_rtos_enter_critical(); result=apply_controls_locked(); liot_rtos_exit_critical();
    if (result) { begin_close(result); return; }
    if (video && !s_video_started) {
        result=tirtc_video_start_live(s_generation,false);
        if (result) { begin_close(result); return; }
        liot_rtos_enter_critical(); s_video_started=true;
        (void)apply_controls_locked(); liot_rtos_exit_critical();
    }
    if (s_video_started) {
        tirtc_video_get_snapshot(&status);
        if (status.generation==s_generation && status.error) { begin_close(status.error); return; }
    }
    result=calls_audio_step(OWNER,s_generation);
    if (result) { begin_close(result); return; }
    /* Bound a connected client that never subscribes or sends audio. */
    liot_rtos_enter_critical();
    if (!s_audio_subscribed && !s_video_subscribed && !s_rx_seen &&
        (uint32_t)(now()-s_started)>=REMOTE_IDLE_MS && !s_stop) {
        s_stop=true; s_error=REMOTE_ERROR;
        (void)apply_controls_locked();
    }
    stop=s_stop; error=s_error;
    liot_rtos_exit_critical();
    if (stop) { begin_close(error); return; }
    if (dirty || (uint32_t)(now()-s_last_publish)>=1000U)
        publish(TIRTC_REMOTE_ACTIVE,"远程查看中");
}
static void worker(void *context)
{
    (void)context;
    for (;;) {
        uint32_t started=now(),elapsed,period;
        step();
        /* Shared audio capture paces subscribed uplink itself. Only wait for
         * the remainder of the control/idle period after nonblocking work. */
        period=tirtc_remote_has_session() ? 5U : 100U;
        elapsed=(uint32_t)(now()-started);
        if (elapsed<period) liot_rtos_task_sleep_ms(period-elapsed);
    }
}
int tirtc_remote_start_service(void)
{
    liot_task_t task=NULL;
    int result;
    liot_rtos_enter_critical();
    if (s_available || s_creating) { liot_rtos_exit_critical(); return 0; }
    s_creating=true;
    liot_rtos_exit_critical();
    result=calls_audio_start_service();
    if (!result) result=(int)liot_rtos_task_create(&task,12U*1024U,13,"remote_ctrl",worker,NULL);
    liot_rtos_enter_critical();
    s_creating=false; if (!result && task) { s_task=task; s_available=true; }
    liot_rtos_exit_critical();
    if (result || !task) return -1;
    /* Automatic idle-HOME admission deliberately disables the optional AI
     * standby connection. Foreground AI/calls keep exclusive ownership. */
    publish(TIRTC_REMOTE_IDLE,"等待远程连接");
    demo_tirtc_set_incoming_listener(&s_listener);
    liot_trace("[LIVE42] service ready; HOME/CLOCK admission; camera subscription11, audio10\r\n");
    return 0;
}
static bool parse_generation(const char *text,size_t capacity,uint32_t *out)
{
    uint32_t value=0;
    size_t i;
    for (i=0;i<capacity && text[i];++i) {
        unsigned digit=(unsigned char)text[i]-(unsigned)'0';
        if (digit>9U || value>(UINT32_MAX-digit)/10U) return false;
        value=value*10U+digit;
    }
    if (!i || i==capacity || !value) return false;
    *out=value; return true;
}
int tirtc_remote_action(const tirtc_ui_action_t *action)
{
    uint32_t generation;
    int result=-1;
    if (!action) return -1;
    if (action->type==TIRTC_ACTION_ENTER_PAGE || action->type==TIRTC_ACTION_RETURN_HOME) {
        tirtc_ui_page_t page=action->type==TIRTC_ACTION_RETURN_HOME ? TIRTC_PAGE_HOME : (tirtc_ui_page_t)action->value;
        if ((unsigned)page>=TIRTC_PAGE_COUNT) return -1;
        liot_rtos_enter_critical(); s_page=page;
        if (!page_allowed(page) && s_phase!=R_IDLE) s_stop=true;
        (void)apply_controls_locked();
        liot_rtos_exit_critical(); return 0;
    }
    if (action->type!=TIRTC_ACTION_REMOTE_END && action->type!=TIRTC_ACTION_REMOTE_SET_MIC &&
        action->type!=TIRTC_ACTION_REMOTE_SET_SPEAKER && action->type!=TIRTC_ACTION_REMOTE_SET_CAMERA) return -1;
    if (!parse_generation(action->extra,sizeof(action->extra),&generation)) return -1;
    if (action->type!=TIRTC_ACTION_REMOTE_END && action->value!=0 && action->value!=1) return -1;
    liot_rtos_enter_critical();
    if (s_available && generation==s_generation && (s_phase==R_STARTING || s_phase==R_ACTIVE) && !s_stop) {
        switch (action->type) {
        case TIRTC_ACTION_REMOTE_END:s_stop=true;break;
        case TIRTC_ACTION_REMOTE_SET_MIC:s_microphone=action->value!=0;break;
        case TIRTC_ACTION_REMOTE_SET_SPEAKER:s_speaker=action->value!=0;break;
        case TIRTC_ACTION_REMOTE_SET_CAMERA:s_camera=action->value!=0;break;
        default:break;
        }
        s_dirty=true; result=0;
        (void)apply_controls_locked();
    }
    liot_rtos_exit_critical();
    if (!result && action->type==TIRTC_ACTION_REMOTE_END)
        liot_trace("[LIVE44] local end requested gen=%lu\r\n",(unsigned long)generation);
    return result;
}
