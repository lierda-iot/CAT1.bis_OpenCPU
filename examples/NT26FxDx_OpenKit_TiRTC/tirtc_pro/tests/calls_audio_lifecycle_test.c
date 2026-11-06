/* SPDX-License-Identifier: MIT
 * Inject control-task switches at the send-result and error-log boundaries.
 * The runner inserts current production declarations/functions; SDK and RTOS
 * calls below are the only fakes. No thread timing or hardware is required. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

typedef void *liot_task_t;
typedef void *liot_queue_t;
typedef int demo_tirtc_owner_e;
typedef int demo_ai_audio_owner_e;
typedef struct { int owner; uint32_t generation; } demo_ai_audio_lease_t;
typedef struct {
    uint8_t stream_id, media, flags, reserved;
    uint32_t ts, length;
} TIRTCFRAMEINFO;
#define DEMO_AI_AUDIO_LEASE_INIT {0, 0}
#define TIRTC_LOG_DEBUG(...) fake_debug(__VA_ARGS__)
#define LIOT_WAIT_FOREVER UINT32_MAX
enum {
    DEMO_TIRTC_OWNER_NONE=0, DEMO_TIRTC_OWNER_WECHAT=2,
    DEMO_TIRTC_OWNER_DEV_CHAT=3, DEMO_TIRTC_OWNER_LIVE=4,
    DEMO_AI_AUDIO_OWNER_WECHAT=2, DEMO_AI_AUDIO_OWNER_DEVICE=3,
    DEMO_AI_AUDIO_OWNER_LIVE=4, DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL=8,
    DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL=8, DEMO_AI_AUDIO_FRAME_SAMPLES=320,
    DEMO_AI_AUDIO_ERR_BUSY=-7, DEMO_AI_AUDIO_ERR_INVALID_LEASE=-8,
    TIRTC_E_BUSY=-9, LIOT_OSI_SUCCESS=0, LIOT_NO_WAIT=0
};

/* PRODUCTION_DECLARATIONS */

enum { SWITCH_NONE, SWITCH_AFTER_RESULT, SWITCH_IN_FAILURE_LOG, SWITCH_IN_SEND,
       SWITCH_IN_FIRST_LOG, CHANGE_REVISION_IN_SEND };
static uint32_t clock_ms=100, next_generation=10;
static unsigned critical_depth, switch_kind, sends, stop_calls, release_calls;
static unsigned switch_attempts, blocked_stops, blocked_starts, cases;
static int send_result=160;
static bool send_returned, inside_trace;
static unsigned queue_creates, queue_deletes, task_creates;
static int queue_create_result, task_create_result;
static bool queue_alive, worker_exists, null_task_handle;
static void attempt_transition(void);
static void liot_rtos_enter_critical(void) { ++critical_depth; }
static void liot_rtos_exit_critical(void)
{
    assert(critical_depth);
    --critical_depth;
    if (!critical_depth && switch_kind==SWITCH_AFTER_RESULT && send_returned) {
        switch_kind=SWITCH_NONE;
        attempt_transition();
    }
}
static uint32_t liot_rtos_get_running_time(void) { return clock_ms; }
static size_t liot_xPortGetFreeHeapSize(void) { assert(!critical_depth); return 100000; }
static size_t liot_xPortGetMaximumFreeBlockSize(void) { assert(!critical_depth); return 50000; }
static void liot_trace(const char *format, ...)
{
    assert(!critical_depth);
    if (!inside_trace && switch_kind==SWITCH_IN_FAILURE_LOG && strstr(format,"[CALL24-AUDIO]")) {
        switch_kind=SWITCH_NONE;
        inside_trace=true;
        attempt_transition();
        inside_trace=false;
    }
}
static void fake_debug(const char *format, ...)
{
    assert(!critical_depth);
    if (switch_kind==SWITCH_IN_FIRST_LOG && strstr(format,"first_tx")) {
        switch_kind=SWITCH_NONE;
        attempt_transition();
    }
}
static int demo_tirtc_send_audio(int owner, uint32_t generation,
                                const TIRTCFRAMEINFO *frame, const void *data)
{
    assert(!critical_depth && s_tx_busy);
    assert(owner==s_owner && generation==s_generation && frame->length==160 && data);
    ++sends;
    if (switch_kind==SWITCH_IN_SEND) {
        switch_kind=SWITCH_NONE;
        attempt_transition();
    } else if (switch_kind==CHANGE_REVISION_IN_SEND) {
        switch_kind=SWITCH_NONE;
        ++s_config_revision;
    }
    send_returned=true;
    return send_result;
}
static int demo_ai_audio_acquire(int owner, demo_ai_audio_lease_t *lease)
{ assert(!critical_depth); *lease=(demo_ai_audio_lease_t){owner,next_generation}; return 0; }
static int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *lease, uint8_t speaker, uint8_t mic)
{ (void)lease; (void)speaker; (void)mic; assert(!critical_depth); return 0; }
static int demo_ai_audio_prewarm_session(const demo_ai_audio_lease_t *lease, uint32_t *extra)
{ (void)lease; assert(!critical_depth); *extra=1200; return 0; }
static int demo_ai_audio_stop(const demo_ai_audio_lease_t *lease)
{ (void)lease; assert(!critical_depth); ++stop_calls; return 0; }
static int demo_ai_audio_release(demo_ai_audio_lease_t *lease)
{ (void)lease; assert(!critical_depth); ++release_calls; return 0; }
static int liot_rtos_queue_wait(liot_queue_t queue, uint8_t *data, unsigned bytes, unsigned wait)
{ (void)queue; (void)data; assert(!critical_depth && bytes==sizeof(call_tx_job_t) && !wait); return -1; }
static int liot_rtos_queue_create(liot_queue_t *queue, uint32_t bytes, uint32_t count)
{
    assert(!critical_depth && bytes==sizeof(call_tx_job_t) && count==CALL_TX_QUEUE_DEPTH);
    assert(!queue_alive && !worker_exists && s_creating && s_tx_creating);
    ++queue_creates;
    if (queue_create_result) return queue_create_result;
    *queue=(void *)2; queue_alive=true; return 0;
}
static int liot_rtos_queue_delete(liot_queue_t queue)
{
    assert(!critical_depth && queue==(void *)2 && queue_alive && !worker_exists);
    assert(s_creating && s_tx_creating);
    ++queue_deletes; queue_alive=false; return 0;
}
static int liot_rtos_task_create(liot_task_t *task, uint32_t bytes, uint8_t priority,
                                 const char *name, void (*entry)(void *), void *argument)
{
    assert(!critical_depth && bytes && priority && !strcmp(name,"call_tx") && entry && !argument);
    assert(queue_alive && !worker_exists && s_creating && s_tx_creating);
    ++task_creates;
    if (task_create_result) return task_create_result;
    if (!null_task_handle) { *task=(void *)3; worker_exists=true; }
    return 0;
}

/* PRODUCTION_FUNCTIONS */

static void attempt_transition(void)
{
    unsigned stops=stop_calls, releases=release_calls;
    ++switch_attempts;
    if (calls_audio_stop(s_owner,s_generation)==DEMO_AI_AUDIO_ERR_BUSY) ++blocked_stops;
    if (calls_audio_start(DEMO_TIRTC_OWNER_WECHAT,next_generation+1)==DEMO_AI_AUDIO_ERR_BUSY) ++blocked_starts;
    /* The obsolete implementation released the old lease at this point. */
    assert(stop_calls==stops && release_calls==releases);
    assert(s_owned && s_tx_busy);
}
static void start_session(int owner)
{
    assert(!s_owned && !s_tx_busy && !critical_depth);
    s_task=s_tx_task=s_tx_queue=(void *)1;
    s_speaker=s_microphone=true; s_volume=8; s_mic=7;
    send_returned=false; switch_kind=SWITCH_NONE;
    switch_attempts=blocked_stops=blocked_starts=0;
    assert(!calls_audio_start(owner,++next_generation));
    s_session_microphone=s_uplink_subscribed=true;
    assert(!s_sent && !s_sent_bytes && !s_error);
}
static call_tx_job_t job(void)
{
    call_tx_job_t value;
    memset(&value,0,sizeof(value));
    value.owner=s_owner; value.generation=s_generation; value.revision=s_config_revision;
    value.frame.length=160;
    return value;
}
static void finish_and_check_next(int next_owner)
{
    assert(!s_tx_busy && !critical_depth);
    assert(!calls_audio_stop(s_owner,s_generation));
    assert(!s_owned);
    start_session(next_owner);
    assert(!s_error && !s_sent && !s_sent_bytes && !s_tx_failing);
    assert(!calls_audio_stop(s_owner,s_generation));
    ++cases;
}
int main(void)
{
    const int owners[]={DEMO_TIRTC_OWNER_DEV_CHAT,DEMO_TIRTC_OWNER_WECHAT,DEMO_TIRTC_OWNER_LIVE};
    queue_create_result=-1;
    assert(calls_audio_start_service()==-1);
    assert(!s_task && !s_tx_task && !s_tx_queue && !s_creating && !s_tx_creating);
    assert(queue_creates==1 && !task_creates && !queue_deletes && !queue_alive);
    queue_create_result=0; task_create_result=-1;
    assert(calls_audio_start_service()==-1);
    assert(!s_task && !s_tx_task && !s_tx_queue && !s_creating && !s_tx_creating);
    assert(queue_creates==2 && task_creates==1 && queue_deletes==1 && !queue_alive);
    task_create_result=0; null_task_handle=true;
    assert(calls_audio_start_service()==-1);
    assert(!s_task && !s_tx_task && !s_tx_queue && !s_creating && !s_tx_creating);
    assert(queue_creates==3 && task_creates==2 && queue_deletes==2 && !queue_alive);
    null_task_handle=false;
    assert(!calls_audio_start_service());
    assert(s_task==s_tx_task && s_task && s_tx_queue && !s_creating && !s_tx_creating);
    assert(queue_creates==4 && task_creates==3 && queue_deletes==2 && queue_alive && worker_exists);
    for (unsigned i=0; i<3; ++i) assert(!calls_audio_start_service());
    assert(queue_creates==4 && task_creates==3 && queue_deletes==2);
    puts("PASS 4 service lifecycle cases: queue failure, task failure, null task, clean retry/idempotent start; live-worker queue never deleted");
    for (unsigned owner=0; owner<3; ++owner) {
        /* Accepted, busy, nonterminal error and terminal error: stop/restart
         * must be blocked until every old result side effect is complete. */
        for (unsigned variant=0; variant<4; ++variant) {
            start_session(owners[owner]);
            call_tx_job_t value=job();
            send_result=variant==0 ? 160 : variant==1 ? TIRTC_E_BUSY : -100;
            clock_ms=4000;
            s_tx_failing=variant==3; s_tx_failure_at=0;
            switch_kind=SWITCH_AFTER_RESULT;
            tx_send_job(&value);
            assert(switch_attempts==1 && blocked_stops==1 && blocked_starts==1);
            assert(s_sent==(variant==0 ? 1U : 0U));
            assert(s_error==(variant==3 ? AUDIO_ERROR : 0));
            finish_and_check_next(owners[(owner+1)%3]);
        }
        for (unsigned terminal=0; terminal<2; ++terminal) {
            start_session(owners[owner]); call_tx_job_t value=job();
            send_result=-100; clock_ms=4000; s_tx_failing=terminal!=0; s_tx_failure_at=0;
            switch_kind=SWITCH_IN_FAILURE_LOG; tx_send_job(&value);
            assert(switch_attempts==1 && blocked_stops==1 && blocked_starts==1);
            assert(s_error==(terminal ? AUDIO_ERROR : 0));
            finish_and_check_next(owners[(owner+1)%3]);
        }
        start_session(owners[owner]); call_tx_job_t value=job();
        send_result=160; switch_kind=SWITCH_IN_FIRST_LOG; tx_send_job(&value);
        assert(switch_attempts==1 && blocked_stops==1 && blocked_starts==1);
        finish_and_check_next(owners[(owner+1)%3]);
        /* Close or revision change during native send makes its eventual
         * result stale, but it must still clear the in-flight fence. */
        for (unsigned variant=0; variant<2; ++variant) {
            start_session(owners[owner]); value=job(); send_result=-100;
            s_tx_failing=true; s_tx_failure_at=0; clock_ms=4000;
            switch_kind=variant ? CHANGE_REVISION_IN_SEND : SWITCH_IN_SEND;
            tx_send_job(&value); assert(!s_error && !s_sent && !s_tx_busy);
            finish_and_check_next(owners[(owner+1)%3]);
        }
        start_session(owners[owner]); value=job(); value.generation++;
        unsigned before_sends=sends; tx_send_job(&value);
        assert(sends==before_sends && !s_tx_busy && !s_error);
        finish_and_check_next(owners[(owner+1)%3]);
    }
    printf("PASS %u call TX lifecycle cases: DEV/WX/LIVE success, BUSY/error, error-log/first-log preemption, stop, stale revision/generation and clean owner handoff\n",cases);
    return 0;
}
