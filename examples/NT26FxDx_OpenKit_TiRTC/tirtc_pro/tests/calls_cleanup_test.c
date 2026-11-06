/* Host scheduler: production HTTP-result handling, cleanup, and control step.
 * No SDK callback completion or physical DMA behavior is inferred here. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui/tirtc_ui.h"

typedef void *liot_task_t;
typedef void *tirtc_conn_t;
typedef unsigned demo_tirtc_owner_e;
enum { DEMO_TIRTC_FEATURE_WECHAT, DEMO_TIRTC_FEATURE_DEV_CHAT };
typedef struct { unsigned binding_generation, credential_epoch, route_generation, sim_id; } demo_binding_service_context_t;
typedef struct { demo_binding_service_context_t context; char id[96]; bool wechat, online; } tirtc_contact_t;
typedef tirtc_ui_call_t tirtc_calls_snapshot_t;
typedef struct { char room[129], peer[1152], token[1152]; } calls_proto_event_t;
typedef struct { char room[129], call_id[65]; } calls_proto_dial_t;
typedef struct { char peer[65], token[601]; } calls_proto_access_t;
typedef struct { bool present, caller_role, answered; char room[129]; } calls_proto_room_t;
typedef enum { CALLS_PROTO_ROOM_REJECT, CALLS_PROTO_ROOM_CANCEL, CALLS_PROTO_ROOM_HANGUP } calls_proto_room_action_t;
typedef struct { uint32_t generation; int error; } tirtc_video_snapshot_t;

/* PRODUCTION_DECLARATIONS */

static uint32_t clock_ms;
static unsigned warnings, terminals, release_calls, hangup_calls, audio_stops, video_stops, listener_removals;
static bool video_stopped;
static int audio_result, release_result;
static uint32_t liot_rtos_get_running_time(void) { return clock_ms; }
static void liot_rtos_enter_critical(void) {}
static void liot_rtos_exit_critical(void) {}
#define liot_trace(...) ((void)0)
static void publish(tirtc_ui_call_state_t state, const char *message, int error)
{
    (void)message;
    s_ui.state=state;s_ui.error=error;
    if(error==TIRTC_CALL_CLEANUP_PENDING)++warnings;
    else if(state==TIRTC_CALL_ENDED||state==TIRTC_CALL_ERROR)++terminals;
}
static int demo_tirtc_send_command(unsigned owner, uint32_t generation, unsigned command, const void *data, uint32_t length)
{ (void)owner;(void)generation;(void)command;(void)data;(void)length;++hangup_calls;return 0; }
static void tirtc_video_stop(uint32_t generation) { assert(generation==7);++video_stops; }
static bool tirtc_video_is_stopped(uint32_t generation) { assert(generation==7);return video_stopped; }
static int calls_audio_stop(unsigned owner, uint32_t generation)
{ assert(owner==2&&generation==7);++audio_stops;return audio_result; }
static int demo_tirtc_cancel_expected_incoming(unsigned owner,uint32_t generation)
{ assert(owner==2&&generation==7);return 0; }
static int demo_tirtc_disconnect(unsigned owner,uint32_t generation)
{ assert(owner==2&&generation==7);return 0; }
static int demo_tirtc_session_release(unsigned owner,uint32_t generation)
{
    assert(owner==2&&generation==7);
    assert(!s_video_started&&!s_audio_started&&!s_jobs[0].used);
    ++release_calls;return release_result;
}
static void demo_tirtc_set_feature_listener(unsigned feature,void *listener)
{ (void)feature;assert(listener==NULL);++listener_removals; }
static void calls_proto_clear_event(calls_proto_event_t *event) { memset(event,0,sizeof(*event)); }
static bool demo_binding_service_context_current(const demo_binding_service_context_t *context) { (void)context;return true; }
static bool demo_tirtc_is_ready(void) { return true; }
static bool call_admission_locked(bool *entered) { *entered=false;return true; }
static void demo_tirtc_admission_leave(void) {}
static void begin_close(int error,bool remote) { (void)error;(void)remote;assert(!"unexpected fresh close"); }
static void begin_close_from(int error,bool remote,const char *source) { (void)source;begin_close(error,remote); }
static void request_dial(void) { assert(!"unexpected redial"); }
static int start_connection(const char *peer,const char *token) { (void)peer;(void)token;assert(!"unexpected connect");return 0; }
static void process_events(void) {}
static void auxiliary_step(void) {}
static bool new_call(const tirtc_contact_t *contact,bool video,const calls_proto_event_t *invite)
{ (void)contact;(void)video;(void)invite;assert(!"unexpected new call during cleanup");return false; }
static void process_confirmation(void) {}
static int demo_tirtc_session_claim(unsigned owner,uint32_t *generation) { (void)owner;(void)generation;assert(!"unexpected claim");return -1; }
static void activate(void) { assert(!"unexpected activation"); }
static int tirtc_video_set_enabled(uint32_t generation,bool enabled) { (void)generation;(void)enabled;return 0; }
static void tirtc_video_get_snapshot(tirtc_video_snapshot_t *snapshot) { memset(snapshot,0,sizeof(*snapshot)); }
static int calls_audio_step(unsigned owner,uint32_t generation) { (void)owner;(void)generation;assert(!"media active during cleanup");return 0; }

/* PRODUCTION_FUNCTIONS */

static void reset(void)
{
    memset(&s_ui,0,sizeof(s_ui));memset(s_jobs,0,sizeof(s_jobs));memset(&s_invite,0,sizeof(s_invite));
    s_phase=C_CLOSING;s_session=11;s_owner=2;s_generation=7;
    s_owned=s_listener_live=s_stop_requested=true;s_callbacks=false;
    s_pending_dial=s_hangup_sent=s_remote_end=s_end_queued=false;
    s_room_owned=s_server_answered=s_peer_answered=s_video_started=s_audio_started=false;
    s_end_pending=true;s_connection=NULL;s_final_error=s_lost=0;
    s_accept_requested=s_video_dirty=false;
    s_room[0]=s_call_id[0]=s_answer_room[0]=s_cancel_room[0]=s_cancel_call_id[0]=0;
    s_cleanup_at=s_cleanup_next=clock_ms=0;
    warnings=terminals=release_calls=hangup_calls=audio_stops=video_stops=listener_removals=0;
    video_stopped=true;audio_result=release_result=0;
    s_task=s_http_task=(void *)(uintptr_t)1;
}
static tirtc_contact_t contact(void)
{ tirtc_contact_t value={0};strcpy(value.id,"peer");value.online=true;return value; }
static void assert_busy(void)
{
    tirtc_contact_t value=contact();
    assert(tirtc_calls_has_session());
    assert(tirtc_calls_dial_contact(&value,false)==-4);
    assert(!s_pending_dial);
    assert(!callback_current((void *)(uintptr_t)1));
}
static void assert_released(void)
{
    tirtc_contact_t value=contact();
    assert(s_phase==C_IDLE&&!s_owned&&!s_generation&&!s_listener_live);
    assert(!tirtc_calls_has_session()&&terminals==1&&listener_removals==1);
    assert(tirtc_calls_dial_contact(&value,false)==0&&s_pending_dial);
}
static void at(uint32_t time) { clock_ms=time;step(); }
static unsigned cases;
static void pass(const char *name) { ++cases;printf("PASS: %s\n",name); }
int main(void)
{
    /* A running dial may create a room after its caller already cancelled. */
    reset();s_jobs[0].used=2;s_jobs[0].kind=J_DEV_DIAL;s_jobs[0].session=s_session;
    at(14000);assert(s_phase==C_CLOSING&&!warnings&&!release_calls);assert_busy();
    at(15000);assert(s_phase==C_BLOCKED&&warnings==1&&!release_calls);assert_busy();
    at(17000);assert(warnings==1&&!release_calls);
    s_jobs[0].used=3;s_jobs[0].result=0;strcpy(s_jobs[0].dial.room,"late-room");
    at(18000);
    assert(s_jobs[0].used==1&&s_jobs[0].kind==J_ROOM_END&&s_jobs[0].action==CALLS_PROTO_ROOM_CANCEL);
    assert(!strcmp(s_jobs[0].room,"late-room")&&!release_calls);assert_busy();
    s_jobs[0].used=3;s_jobs[0].result=0;at(19000);
    assert(release_calls==1&&warnings==1);assert_released();
    pass("late HTTP room is cancelled before owner reuse; timeout recovers automatically");

    reset();s_jobs[0].used=2;s_jobs[0].kind=J_DEV_DIAL;at(15000);
    s_jobs[0].used=3;s_jobs[0].result=-5604;at(18000);
    assert(warnings==1&&release_calls==1);assert_released();
    pass("late failed HTTP also restores availability without a device reboot");

    reset();s_video_started=true;video_stopped=false;s_connection=(void *)(uintptr_t)1;
    at(15000);assert_busy();assert(!release_calls&&!audio_stops&&hangup_calls==1);
    at(16000);assert(warnings==1&&hangup_calls==1&&!release_calls);
    video_stopped=true;at(17000);assert(video_stops==3&&release_calls==1);assert_released();
    pass("busy video fence retains owner and listener; later video stop recovers");

    reset();s_audio_started=true;audio_result=-40;
    at(15000);assert_busy();assert(!release_calls&&s_audio_started);
    at(16000);assert(audio_stops==2&&!release_calls&&warnings==1);
    audio_result=0;at(17000);assert_released();
    pass("audio stop failure is retried without forced lease or DMA release");

    reset();release_result=-16;at(15000);assert_busy();assert(s_owned&&s_generation==7&&!listener_removals);
    at(16000);assert(release_calls==2&&warnings==1&&!listener_removals);
    release_result=0;at(17000);assert(release_calls==3);assert_released();
    pass("pending SDK callbacks keep owner reserved until session release succeeds");

    reset();at(0);assert(!warnings&&release_calls==1);assert_released();
    pass("ordinary cancellation keeps immediate cleanup and terminal behavior");

    reset();s_ui.incoming=true;s_room_owned=true;strcpy(s_room,"incoming-room");
    at(15000);assert(s_jobs[0].kind==J_ROOM_END&&s_jobs[0].action==CALLS_PROTO_ROOM_REJECT);
    s_jobs[0].used=3;at(16000);assert_released();
    pass("unanswered incoming device call retains room rejection semantics");

    reset();s_room_owned=s_peer_answered=true;strcpy(s_room,"answered-room");
    at(15000);assert(s_jobs[0].kind==J_ROOM_END&&s_jobs[0].action==CALLS_PROTO_ROOM_HANGUP);
    s_jobs[0].used=3;at(16000);assert_released();
    pass("answered outgoing device call retains hangup semantics");

    reset();s_ui.wechat=s_ui.incoming=true;strcpy(s_invite.room,"wx-room");
    at(15000);assert(s_jobs[0].kind==J_WX_REJECT&&!strcmp(s_jobs[0].event.room,"wx-room"));
    s_jobs[0].used=3;at(16000);assert_released();
    pass("unconnected incoming WeChat call retains exact invitation rejection");

    reset();s_remote_end=true;s_end_pending=false;s_connection=(void *)(uintptr_t)1;
    at(15000);assert(!hangup_calls&&!s_jobs[0].used);assert_released();
    pass("remote hangup is not echoed during delayed teardown");

    reset();s_final_error=-1234;s_jobs[0].used=2;s_jobs[0].kind=J_INFO;
    at(15000);s_jobs[0].used=3;s_jobs[0].result=-1;at(16000);
    assert(s_ui.state==TIRTC_CALL_ERROR&&s_ui.error==-1234);assert_released();
    pass("recovery preserves the original call failure instead of latching timeout");

    reset();s_cleanup_at=UINT32_MAX-9999U;s_cleanup_next=s_cleanup_at;
    s_jobs[0].used=2;s_jobs[0].kind=J_DEV_DIAL;
    at(4999);assert(!warnings);at(5999);assert(warnings==1);assert_busy();
    s_jobs[0].used=3;s_jobs[0].result=-1;at(6999);assert_released();
    pass("deadline and retry remain valid across tick wraparound");
    printf("PASS: %u production call-cleanup scenarios\n",cases);
    return 0;
}
