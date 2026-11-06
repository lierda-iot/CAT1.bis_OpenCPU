/* Production LIVE controller with deterministic late release boundaries.
 * No actual SDK callback, audio driver or camera DMA completion is inferred. */
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
typedef struct { unsigned generation; } demo_binding_service_context_t;
typedef struct { uint32_t generation; int error; } tirtc_video_snapshot_t;
enum { DEMO_TIRTC_OWNER_LIVE=4, TIRTC_E_INVALID_HANDLE=-2 };

/* PRODUCTION_DECLARATIONS */

static uint32_t clock_ms;
static unsigned warnings, terminals, closing_notices, hangups, disconnects;
static unsigned video_stops, audio_stops, release_calls, critical_depth, cases;
static int audio_result, release_result, disconnect_result;
static bool video_stopped;
static tirtc_ui_state_t ui_state;

/* PRODUCTION_UI_BUSY_FUNCTION */
static uint32_t liot_rtos_get_running_time(void) { return clock_ms; }
static void liot_rtos_enter_critical(void) { ++critical_depth; }
static void liot_rtos_exit_critical(void) { assert(critical_depth); --critical_depth; }
static void publish(tirtc_ui_remote_state_t state,const char *message)
{
    assert(!critical_depth); (void)message;
    s_ui.state=state; s_ui.error=s_error; ui_state.remote=s_ui;
    if (s_phase==R_BLOCKED && s_connection) ++warnings;
    else if (state==TIRTC_REMOTE_CLOSING) ++closing_notices;
    else if (state==TIRTC_REMOTE_ERROR || state==TIRTC_REMOTE_ENDED) ++terminals;
}
static int demo_tirtc_send_command(unsigned owner,uint32_t generation,
                                  uint32_t command,const void *data,uint32_t length)
{
    assert(!critical_depth && owner==OWNER && generation==7);
    assert((command & 0xffffU)==LIVE_HANGUP && !data && !length);
    ++hangups; return 0;
}
static int demo_tirtc_disconnect(unsigned owner,uint32_t generation)
{
    assert(!critical_depth && owner==OWNER && generation==7);
    ++disconnects; return disconnect_result;
}
static void tirtc_video_stop(uint32_t generation)
{ assert(!critical_depth && generation==7); ++video_stops; }
static bool tirtc_video_is_stopped(uint32_t generation)
{ assert(!critical_depth && generation==7); return video_stopped; }
static int calls_audio_stop(unsigned owner,uint32_t generation)
{ assert(!critical_depth && owner==OWNER && generation==7); ++audio_stops; return audio_result; }
static int demo_tirtc_session_release(unsigned owner,uint32_t generation)
{
    assert(!critical_depth && owner==OWNER && generation==7);
    assert(!s_video_started && !s_audio_started);
    ++release_calls; return release_result;
}
/* Closing/blocked cleanup must run even if the route/SDK is no longer ready. */
static bool demo_tirtc_is_ready(void) { return false; }
static bool demo_binding_service_context_current(const demo_binding_service_context_t *context)
{ (void)context; return false; }
static void start_media(void) { assert(!"media restart during cleanup"); }
static int apply_controls_locked(void) { assert(!"controls during cleanup"); return 0; }
static int tirtc_video_start_live(uint32_t generation,bool enabled)
{ (void)generation; (void)enabled; assert(!"video restart during cleanup"); return 0; }
static void tirtc_video_get_snapshot(tirtc_video_snapshot_t *snapshot)
{ memset(snapshot,0,sizeof(*snapshot)); }
static int calls_audio_step(unsigned owner,uint32_t generation)
{ (void)owner; (void)generation; assert(!"audio capture during cleanup"); return 0; }

/* PRODUCTION_FUNCTIONS */

static void reset(void)
{
    memset(&s_ui,0,sizeof(s_ui));
    s_ui.state=TIRTC_REMOTE_CLOSING; ui_state.remote=s_ui;
    s_phase=R_CLOSING; s_owned=true; s_generation=7;
    s_adopting=false; s_stop=true; s_remote_end=false;
    s_audio_started=s_video_started=true;
    s_disconnect_sent=s_hangup_sent=false;
    s_audio_subscribed=s_video_subscribed=false;
    s_connection=(void *)(uintptr_t)1; s_rx_seen=true;
    s_error=0; s_close_at=clock_ms=0;
    warnings=terminals=closing_notices=hangups=disconnects=0;
    video_stops=audio_stops=release_calls=critical_depth=0;
    audio_result=release_result=disconnect_result=0; video_stopped=true;
}
static void at(uint32_t time) { clock_ms=time; step(); assert(!critical_depth); }
static void assert_busy(void)
{
    assert(tirtc_remote_has_session() && s_owned && s_generation==7);
    assert(s_connection && !terminals && remote_session_active());
    assert(s_ui.state==TIRTC_REMOTE_CLOSING);
    if(s_phase==R_BLOCKED) assert(s_ui.error==REMOTE_CLEANUP);
}
static void assert_released(void)
{
    assert(s_phase==R_IDLE && !s_owned && !s_connection && !s_rx_seen);
    assert(!s_audio_started && !s_video_started && !tirtc_remote_has_session());
    assert(terminals==1 && !remote_session_active());
}
static void pass(const char *name) { ++cases; printf("PASS: %s\n",name); }

int main(void)
{
    reset(); release_result=-16;
    at(14999); assert(s_phase==R_CLOSING && !warnings && release_calls==1); assert_busy();
    at(15000); assert(s_phase==R_BLOCKED && warnings==1 && release_calls==2); assert_busy();
    for (unsigned i=0;i<20;i++) at(15005+i*5U);
    assert(warnings==1 && release_calls==22 && hangups==1 && disconnects==1); assert_busy();
    release_result=0; at(20000);
    assert(release_calls==23 && warnings==1); assert_released();
    pass("late SDK release keeps UI busy after timeout until real release, without repeated warning/hangup/disconnect");

    reset(); video_stopped=false;
    at(15000); at(16000);
    assert(s_phase==R_BLOCKED && warnings==1 && s_video_started && !audio_stops && !release_calls); assert_busy();
    video_stopped=true; at(17000);
    assert(video_stops==3 && audio_stops==1 && release_calls==1); assert_released();
    pass("late camera/DMA completion preserves media ordering and then recovers");

    reset(); audio_result=-40;
    at(15000); at(16000);
    assert(s_phase==R_BLOCKED && warnings==1 && s_audio_started && !release_calls); assert_busy();
    audio_result=0; at(17000);
    assert(audio_stops==3 && release_calls==1); assert_released();
    pass("late audio stop recovers without prematurely releasing the session");

    reset(); video_stopped=false; audio_result=release_result=-16;
    at(15000); video_stopped=true; at(16000);
    assert(!release_calls); assert_busy();
    audio_result=0; at(17000);
    assert(release_calls==1); assert_busy();
    release_result=0; at(18000);
    assert(warnings==1); assert_released();
    pass("successive camera/audio/SDK gates each remain required after the timeout");

    reset(); s_phase=R_ACTIVE; s_stop=false;
    begin_close(0); assert(closing_notices==1 && s_phase==R_CLOSING);
    at(0); assert(!warnings && hangups==1 && disconnects==1);
    assert(s_ui.state==TIRTC_REMOTE_ENDED && !s_ui.error); assert_released();
    at(20000); assert(terminals==1 && release_calls==1);
    pass("ordinary local close remains immediate and idle polling does not repeat cleanup");

    reset(); s_phase=R_ACTIVE; s_stop=false;
    begin_close(-1234); at(0);
    assert(!warnings && s_ui.state==TIRTC_REMOTE_ERROR && s_ui.error==-1234); assert_released();
    pass("ordinary error close retains its original error result");

    reset(); s_remote_end=true; release_result=-16;
    at(15000); at(16000); release_result=0; at(17000);
    assert(!hangups && disconnects==1 && warnings==1); assert_released();
    pass("remote hangup is not echoed while delayed cleanup continues");

    reset(); disconnect_result=release_result=-16;
    at(15000); at(16000);
    assert(disconnects==2 && !s_disconnect_sent && warnings==1); assert_busy();
    disconnect_result=TIRTC_E_INVALID_HANDLE; release_result=0; at(17000);
    assert(disconnects==3 && s_disconnect_sent); assert_released();
    pass("failed disconnect continues retrying through the existing SDK release gate");

    reset(); release_result=-16; s_close_at=UINT32_MAX-9999U;
    at(4999); assert(!warnings && s_phase==R_CLOSING);
    at(5000); assert(warnings==1 && s_phase==R_BLOCKED);
    release_result=0; at(6000); assert_released();
    pass("cleanup deadline and recovery survive the millisecond clock wrap");

    reset(); s_adopting=true; at(20000);
    assert(!video_stops && !audio_stops && !release_calls && !warnings); assert_busy();
    s_adopting=false; at(20001); assert_released();
    pass("adoption still fences cleanup and is never bypassed by the deadline");

    printf("PASS: %u production LIVE cleanup scenarios\n",cases);
    return 0;
}
