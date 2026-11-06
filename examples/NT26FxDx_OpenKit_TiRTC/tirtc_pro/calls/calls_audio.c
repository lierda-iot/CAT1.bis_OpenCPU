/* SPDX-License-Identifier: MIT
 * Shared call/LIVE audio: Lite-compatible 16 kHz local PCM with 8 kHz
 * A-law network frames and bounded 8/16 kHz A-law downlink.
 * The control task submits playback then captures; only network TX is separate. */
#include "calls_audio.h"
#include "g711_codec.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include <string.h>
#define RX_BYTES 15360U
#define RX_SEGMENTS 32U
#define PLAY_AHEAD 480U
#define PREBUFFER_BYTES 1920U
#define PREBUFFER_MS 120U
/* Match Lite05 DEV startup/rebuffering without changing WX/LIVE pacing. */
#define DEV_PREBUFFER_BYTES (200U * 8U)
#define DEV_PREBUFFER_TIMEOUT_MS 300U
#define DEV_PLAY_BATCH_MAX 8U
#define AUDIO_ERROR (-6208)
#define CALL_NETWORK_SAMPLES_8K (DEMO_AI_AUDIO_FRAME_SAMPLES / 2U)
#define CALL_TX_QUEUE_DEPTH 8U
static liot_task_t s_task;
static liot_task_t s_tx_task;
static liot_queue_t s_tx_queue;
static bool s_creating, s_running, s_owned, s_record_busy;
static bool s_tx_creating, s_tx_busy;
static demo_ai_audio_lease_t s_lease=DEMO_AI_AUDIO_LEASE_INIT;
static demo_tirtc_owner_e s_owner;
static uint32_t s_generation;
static uint8_t s_volume=DEMO_AI_AUDIO_DEFAULT_SPK_LEVEL, s_mic=DEMO_AI_AUDIO_DEFAULT_MIC_LEVEL;
static bool s_speaker=true, s_microphone=true;
static bool s_session_speaker=true, s_session_microphone=true, s_uplink_subscribed=true;
/* Adjacent equal-rate input coalesces, so normal 20-ms packets use one entry.
 * Callback metadata is bounded even if a peer alternates rate every packet. */
typedef struct { uint16_t bytes; uint8_t rate; } audio_segment_t;
static audio_segment_t s_segments[RX_SEGMENTS];
static uint32_t s_segment_head, s_segment_count;
static uint32_t s_received, s_received_bytes, s_sent_bytes;
static bool s_first_rx_pending, s_first_rx_logged, s_first_tx_logged;
static uint8_t s_first_rx_stream, s_first_rx_rate;
static uint32_t s_first_rx_length;
static uint32_t s_config_revision=1, s_applied_revision;
static bool s_mute_pending, s_draining;
static uint32_t s_drain_since;
static uint8_t s_rx[RX_BYTES], s_alaw[CALL_NETWORK_SAMPLES_8K], s_capture_alaw[CALL_NETWORK_SAMPLES_8K];
static int16_t s_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES], s_capture[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint32_t s_read, s_count, s_peak, s_drops, s_played, s_sent;
static uint32_t s_tail, s_prebuffer_at, s_partial_at;
static bool s_guard, s_continuing, s_prebuffer, s_partial;
static uint8_t s_fraction;
static uint32_t s_tx_timestamp, s_pause_at;
static bool s_pause_seen;
static int s_error;
static bool s_tx_failing, s_record_failing;
static uint32_t s_tx_failure_at, s_record_failure_at;
static uint32_t s_last_log, s_play_failure_at;
static bool s_play_failing;
/* DEV-only local counters; PCM is inspected without changing it.
 * Preserve bounded window resets without periodic serial output. */
typedef struct {
    uint32_t generation, started_at, reported_at;
    uint32_t record_try, record_ok, record_busy, record_error, record_stale;
    uint32_t record_max_ms, queue_ok, queue_fail;
    uint32_t tx_ok, tx_busy, tx_error, tx_stale, tx_max_ms;
    uint32_t pcm_samples, pcm_peak;
    uint64_t pcm_absolute;
    int record_ret, queue_ret, tx_ret;
} dev_audio_diagnostic_t;
static dev_audio_diagnostic_t s_dev_diagnostic;
enum failure_kind { FAIL_RECORD, FAIL_SEND, FAIL_PLAY, FAIL_CONTROL, FAIL_STOP, FAIL_START, FAIL_COUNT };
typedef struct {
    demo_tirtc_owner_e owner;
    uint32_t generation;
    uint32_t revision;
    TIRTCFRAMEINFO frame;
    uint8_t payload[CALL_NETWORK_SAMPLES_8K];
} call_tx_job_t;
/* Each slot has one task owner. Reset only before capture admission opens.
 * No diagnostic heap allocation and no heap query/printing under critical. */
static struct { uint32_t at; bool seen, terminal; } s_failure_log[FAIL_COUNT];
static uint32_t now(void){return liot_rtos_get_running_time();}
static bool before(uint32_t end){return (int32_t)(end-now())>0;}
static void audio_error(void){liot_rtos_enter_critical();s_error=AUDIO_ERROR;liot_rtos_exit_critical();}
static void failure_log(enum failure_kind kind,const char *reason,int result,uint32_t duration,bool terminal)
{
    uint32_t stamp=now(),generation;size_t free_bytes,max_block;
    if(s_failure_log[kind].terminal)return;
    if(!terminal&&s_failure_log[kind].seen&&(uint32_t)(stamp-s_failure_log[kind].at)<1000U)return;
    s_failure_log[kind].seen=true;s_failure_log[kind].terminal=terminal;s_failure_log[kind].at=stamp;
    liot_rtos_enter_critical();generation=s_generation;liot_rtos_exit_critical();
    free_bytes=liot_xPortGetFreeHeapSize();max_block=liot_xPortGetMaximumFreeBlockSize();
    liot_trace("[CALL24-AUDIO] failure reason=%s ret=%d duration_ms=%lu terminal=%u gen=%lu heap=%lu max_block=%lu\r\n",
        reason,result,(unsigned long)duration,terminal?1U:0U,(unsigned long)generation,
        (unsigned long)free_bytes,(unsigned long)max_block);
}
static int fatal_error(enum failure_kind kind,const char *reason,int result,uint32_t duration)
{
    failure_log(kind,reason,result,duration,true);audio_error();return AUDIO_ERROR;
}
static bool audio_session_locked(demo_tirtc_owner_e owner,uint32_t generation)
{return s_owned&&generation&&s_owner==owner&&s_generation==generation;}
static bool speaker_allowed_locked(void)
{return s_speaker&&s_volume&&s_session_speaker;}
static bool microphone_allowed_locked(void)
{return s_microphone&&s_mic&&s_session_microphone&&s_uplink_subscribed;}
/* Honour the selected level in DEV too, as Lite does. Mute admission is
 * checked separately; do not replace every nonzero setting with L7. */
static uint8_t effective_mic_level_locked(void)
{return s_mic;}
static void dev_diagnostic_record(demo_tirtc_owner_e owner,uint32_t generation,
                                  int result,uint32_t duration,bool current)
{
    uint32_t i,peak=0;uint64_t absolute=0;
    if(owner!=DEMO_TIRTC_OWNER_DEV_CHAT)return;
    if(!result)for(i=0;i<DEMO_AI_AUDIO_FRAME_SAMPLES;i++){
        int32_t sample=s_capture[i];uint32_t magnitude=(uint32_t)(sample<0?-sample:sample);
        absolute+=magnitude;if(magnitude>peak)peak=magnitude;
    }
    liot_rtos_enter_critical();
    if(s_dev_diagnostic.generation==generation){
        s_dev_diagnostic.record_ret=result;
        if(duration>s_dev_diagnostic.record_max_ms)s_dev_diagnostic.record_max_ms=duration;
        if(!result){
            s_dev_diagnostic.record_ok++;
            if(!current)s_dev_diagnostic.record_stale++;
            s_dev_diagnostic.pcm_samples+=DEMO_AI_AUDIO_FRAME_SAMPLES;
            s_dev_diagnostic.pcm_absolute+=absolute;
            if(peak>s_dev_diagnostic.pcm_peak)s_dev_diagnostic.pcm_peak=peak;
        }else if(result==DEMO_AI_AUDIO_ERR_BUSY)s_dev_diagnostic.record_busy++;
        else s_dev_diagnostic.record_error++;
    }
    liot_rtos_exit_critical();
}
/* Caller holds critical. A stale job cannot contaminate a newer session. */
static void dev_diagnostic_tx_locked(const call_tx_job_t *job,bool attempted,
                                     bool current,int result,uint32_t duration)
{
    if(job->owner!=DEMO_TIRTC_OWNER_DEV_CHAT||s_dev_diagnostic.generation!=job->generation)return;
    if(!current)s_dev_diagnostic.tx_stale++;
    if(!attempted)return;
    s_dev_diagnostic.tx_ret=result;
    if(duration>s_dev_diagnostic.tx_max_ms)s_dev_diagnostic.tx_max_ms=duration;
    if(result>0)s_dev_diagnostic.tx_ok++;
    else if(result==TIRTC_E_BUSY)s_dev_diagnostic.tx_busy++;
    else s_dev_diagnostic.tx_error++;
}
static void dev_diagnostic_report(demo_tirtc_owner_e owner,uint32_t generation,bool final)
{
    uint32_t stamp=now();
    if(owner!=DEMO_TIRTC_OWNER_DEV_CHAT)return;
    liot_rtos_enter_critical();
    if(s_dev_diagnostic.generation!=generation||
       (!final&&(uint32_t)(stamp-s_dev_diagnostic.reported_at)<5000U)){
        liot_rtos_exit_critical();return;
    }
    s_dev_diagnostic.reported_at=stamp;
    s_dev_diagnostic.pcm_samples=s_dev_diagnostic.pcm_peak=0;s_dev_diagnostic.pcm_absolute=0;
    liot_rtos_exit_critical();
}
static void revise_config_locked(void)
{if(!++s_config_revision)++s_config_revision;}
static void discard_rx(void)
{
    liot_rtos_enter_critical();s_read=s_count=0;s_segment_head=s_segment_count=0;
    liot_rtos_exit_critical();s_prebuffer=s_partial=s_continuing=false;
}
/* Caller holds the critical section. Used only after copied audio was accepted
 * by playback, or after an incomplete source sample has timed out. */
static void consume_rx_locked(uint32_t bytes)
{
    s_read=(s_read+bytes)%RX_BYTES;s_count-=bytes;
    s_segments[s_segment_head].bytes=(uint16_t)(s_segments[s_segment_head].bytes-bytes);
    if(!s_segments[s_segment_head].bytes){s_segment_head=(s_segment_head+1U)%RX_SEGMENTS;s_segment_count--;}
}
void calls_audio_set_config(uint8_t volume,uint8_t mic,bool speaker,bool microphone)
{
    bool was_speaker;
    if(volume>10)volume=10;
    if(mic>10)mic=10;
    liot_rtos_enter_critical();was_speaker=speaker_allowed_locked();
    if(s_volume!=volume||s_mic!=mic||s_speaker!=speaker||s_microphone!=microphone){
        s_volume=volume;s_mic=mic;s_speaker=speaker;s_microphone=microphone;
        revise_config_locked();
    }
    if(was_speaker&&!speaker_allowed_locked())s_mute_pending=true;
    liot_rtos_exit_critical();
}
int calls_audio_set_session_controls(demo_tirtc_owner_e owner,uint32_t generation,
                                     bool speaker,bool microphone,bool uplink_subscribed)
{
    bool was_speaker;
    liot_rtos_enter_critical();
    if(!audio_session_locked(owner,generation)){liot_rtos_exit_critical();return DEMO_AI_AUDIO_ERR_INVALID_LEASE;}
    was_speaker=speaker_allowed_locked();
    if(s_session_speaker!=speaker||s_session_microphone!=microphone||s_uplink_subscribed!=uplink_subscribed){
        s_session_speaker=speaker;s_session_microphone=microphone;s_uplink_subscribed=uplink_subscribed;
        revise_config_locked();
    }
    if(was_speaker&&!speaker_allowed_locked())s_mute_pending=true;
    liot_rtos_exit_critical();return 0;
}
bool calls_audio_busy(void){bool busy;liot_rtos_enter_critical();busy=s_owned;liot_rtos_exit_critical();return busy;}
static bool tx_job_current_locked(const call_tx_job_t *job)
{
    return s_running && audio_session_locked(job->owner,job->generation) &&
           microphone_allowed_locked() && s_config_revision==job->revision;
}
static void tx_send_job(const call_tx_job_t *job)
{
    bool current,first=false;
    int result;uint32_t started,duration;
    liot_rtos_enter_critical();
    current=tx_job_current_locked(job); s_tx_busy=current;
    if(!current)dev_diagnostic_tx_locked(job,false,false,0,0);
    liot_rtos_exit_critical();
    if(!current)return;
    started=now();result=demo_tirtc_send_audio(job->owner,job->generation,&job->frame,job->payload);duration=now()-started;
    liot_rtos_enter_critical();
    current=tx_job_current_locked(job);
    dev_diagnostic_tx_locked(job,true,current,result,duration);
    liot_rtos_exit_critical();
    /* Keep stop/release fenced until this job has finished updating its
     * session state, including a terminal error raised after logging. */
    if(!current)goto finished;
    if(result>0){
        ++s_sent; s_sent_bytes+=job->frame.length; s_tx_failing=false;
        if(!s_first_tx_logged){s_first_tx_logged=true;first=true;}
    }else if(result==TIRTC_E_BUSY){
        s_tx_failing=false;
    }else{
        if(!s_tx_failing){s_tx_failing=true;s_tx_failure_at=now();}
        if((uint32_t)(now()-s_tx_failure_at)>=3000U)
            (void)fatal_error(FAIL_SEND,"send",result,now()-s_tx_failure_at);
        else failure_log(FAIL_SEND,"send",result,now()-s_tx_failure_at,false);
    }
    if(first)TIRTC_LOG_DEBUG("[AUDIO42] first_tx owner=%u gen=%lu stream=%u rate=8000 bytes=%lu\r\n",
        (unsigned)job->owner,(unsigned long)job->generation,(unsigned)job->frame.stream_id,
        (unsigned long)job->frame.length);
finished:
    liot_rtos_enter_critical();s_tx_busy=false;liot_rtos_exit_critical();
}
static void tx_worker(void *arg)
{
    call_tx_job_t job;
    (void)arg;
    for(;;)if(liot_rtos_queue_wait(s_tx_queue,(uint8_t *)&job,sizeof(job),LIOT_WAIT_FOREVER)==LIOT_OSI_SUCCESS)tx_send_job(&job);
}
static void capture_once(void)
{
    demo_ai_audio_lease_t lease; demo_tirtc_owner_e owner=DEMO_TIRTC_OWNER_NONE; uint32_t generation=0, timestamp=0, revision=0;
    bool allowed, current; int result; call_tx_job_t job;uint32_t started,duration;
    liot_rtos_enter_critical();
    allowed=s_running&&s_owned&&!s_error&&!s_record_busy&&microphone_allowed_locked()&&s_config_revision==s_applied_revision;
    if(allowed){
        s_record_busy=true;lease=s_lease;owner=s_owner;generation=s_generation;revision=s_config_revision;
        if(owner==DEMO_TIRTC_OWNER_DEV_CHAT)s_dev_diagnostic.record_try++;
        if(s_pause_seen){s_tx_timestamp+=now()-s_pause_at;s_pause_seen=false;}
        timestamp=s_tx_timestamp;s_tx_timestamp+=20U;
    }else if(!s_pause_seen){s_pause_seen=true;s_pause_at=now();}
    liot_rtos_exit_critical();
    if(!allowed){s_tx_failing=s_record_failing=false;liot_rtos_task_sleep_ms(5);return;}
    started=now();result=demo_ai_audio_record_20ms(&lease,s_capture);duration=now()-started;
#if DEMO_AI_AUDIO_AEC_ENABLE
    /* Fresh AEC PCM follows wall time; a timeout or scheduling gap must not
     * compress the audio timeline relative to video capture timestamps. */
    if(!result)timestamp=started+duration;
#endif
    liot_rtos_enter_critical();
    current=s_running&&audio_session_locked(owner,generation)&&microphone_allowed_locked()&&s_config_revision==revision;
    liot_rtos_exit_critical();
    dev_diagnostic_record(owner,generation,result,duration,current);
    if(result && result!=DEMO_AI_AUDIO_ERR_BUSY){
        if(!s_record_failing){s_record_failing=true;s_record_failure_at=now();}
        if((uint32_t)(now()-s_record_failure_at)>=3000U)
            (void)fatal_error(FAIL_RECORD,"record",result,now()-s_record_failure_at);
        else failure_log(FAIL_RECORD,"record",result,now()-s_record_failure_at,false);
    }else if(!result){
        s_record_failing=false;
        if(current){
            {
                uint32_t sample;
                for (sample=0U; sample<CALL_NETWORK_SAMPLES_8K; ++sample) {
                    int32_t mixed=(int32_t)s_capture[sample*2U]+(int32_t)s_capture[sample*2U+1U];
                    s_capture_alaw[sample]=demo_g711_alaw_encode_sample((int16_t)(mixed/2));
                }
            }
            memset(&job,0,sizeof(job));job.owner=owner;job.generation=generation;job.revision=revision;
            memcpy(job.payload,s_capture_alaw,sizeof(s_capture_alaw));
            job.frame.stream_id=owner==DEMO_TIRTC_OWNER_WECHAT?0U:10U;job.frame.media=TIRTC_AUDIO_ALAW;
            job.frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;job.frame.ts=timestamp;job.frame.length=CALL_NETWORK_SAMPLES_8K;
            liot_rtos_enter_critical();
            current=s_running&&audio_session_locked(owner,generation)&&microphone_allowed_locked()&&s_config_revision==revision;
            liot_rtos_exit_critical();
            if(!current){s_tx_failing=false;liot_rtos_enter_critical();
                if(owner==DEMO_TIRTC_OWNER_DEV_CHAT)s_dev_diagnostic.record_stale++;
                s_record_busy=false;liot_rtos_exit_critical();return;}
            {
                int queue_result=liot_rtos_queue_release(s_tx_queue,sizeof(job),(uint8_t *)&job,LIOT_NO_WAIT);
                if(owner==DEMO_TIRTC_OWNER_DEV_CHAT){
                    liot_rtos_enter_critical();
                    s_dev_diagnostic.queue_ret=queue_result;
                    if(queue_result==LIOT_OSI_SUCCESS)s_dev_diagnostic.queue_ok++;else s_dev_diagnostic.queue_fail++;
                    liot_rtos_exit_critical();
                }
                if(queue_result!=LIOT_OSI_SUCCESS)s_tx_failing=false;
                result=queue_result==LIOT_OSI_SUCCESS ? 1 : -1;
            }
        }else s_tx_failing=false;
    }
    liot_rtos_enter_critical();s_record_busy=false;liot_rtos_exit_critical();
    if(result<=0)liot_rtos_task_sleep_ms(5);
}
int calls_audio_start_service(void)
{
    liot_task_t tx_task=NULL;int result;
    liot_rtos_enter_critical();if(s_task||s_creating||s_tx_task||s_tx_creating){liot_rtos_exit_critical();return 0;}s_creating=true;s_tx_creating=true;liot_rtos_exit_critical();
    result=(int)liot_rtos_queue_create(&s_tx_queue,sizeof(call_tx_job_t),CALL_TX_QUEUE_DEPTH);
    if(result!=LIOT_OSI_SUCCESS){liot_rtos_enter_critical();s_creating=false;s_tx_creating=false;liot_rtos_exit_critical();return -1;}
    /* Liot_AudioRecord and Liot_AudioPlay share the F6D_A I2S/codec state.
     * DEV capture therefore runs from calls_audio_step(), alongside playback,
     * just like the proven group-room path. Keep only network TX asynchronous;
     * it never touches the audio driver. */
    result=(int)liot_rtos_task_create(&tx_task,6U*1024U,11,"call_tx",tx_worker,NULL);
    if(result||!tx_task){
        /* No worker was created, so this queue has no consumer yet. Keep
         * creation fenced while releasing it, then allow a clean retry. */
        (void)liot_rtos_queue_delete(s_tx_queue);s_tx_queue=NULL;
    }
    liot_rtos_enter_critical();s_creating=false;s_tx_creating=false;if(!result&&tx_task){s_task=tx_task;s_tx_task=tx_task;}liot_rtos_exit_critical();
    return !result&&tx_task ? 0 : -1;
}
int calls_audio_start(demo_tirtc_owner_e owner,uint32_t generation)
{
    demo_ai_audio_owner_e audio_owner;uint8_t volume,mic;uint32_t revision,extra=0;int result;
    if(!generation||!s_task||s_owned)return DEMO_AI_AUDIO_ERR_BUSY;
    if(owner==DEMO_TIRTC_OWNER_WECHAT)audio_owner=DEMO_AI_AUDIO_OWNER_WECHAT;
    else if(owner==DEMO_TIRTC_OWNER_DEV_CHAT)audio_owner=DEMO_AI_AUDIO_OWNER_DEVICE;
    else if(owner==DEMO_TIRTC_OWNER_LIVE)audio_owner=DEMO_AI_AUDIO_OWNER_LIVE;
    else return DEMO_AI_AUDIO_ERR_INVALID_LEASE;
    memset(s_failure_log,0,sizeof(s_failure_log));
    result=demo_ai_audio_acquire(audio_owner,&s_lease);if(result){failure_log(FAIL_START,"acquire",result,0,true);return result;}
    liot_rtos_enter_critical();
    s_owned=true;s_owner=owner;s_generation=generation;s_running=false;s_error=0;
    memset(&s_dev_diagnostic,0,sizeof(s_dev_diagnostic));
    if(owner==DEMO_TIRTC_OWNER_DEV_CHAT){
        s_dev_diagnostic.generation=generation;
    }
    s_session_speaker=true;s_session_microphone=owner!=DEMO_TIRTC_OWNER_LIVE;
    s_uplink_subscribed=owner!=DEMO_TIRTC_OWNER_LIVE;revise_config_locked();
    volume=speaker_allowed_locked()?s_volume:0;
    mic=microphone_allowed_locked()?effective_mic_level_locked():0;
    revision=s_config_revision;
    s_received=s_received_bytes=s_sent_bytes=0;
    s_first_rx_pending=s_first_rx_logged=s_first_tx_logged=false;
    liot_rtos_exit_critical();
    result=demo_ai_audio_prepare_session(&s_lease,volume,mic);if(result){failure_log(FAIL_START,"prepare",result,0,true);return result;}
    /* Lite DEV submits speech directly; do not queue a separate silent
     * preamble while the full-duplex call is connecting. */
    if(owner!=DEMO_TIRTC_OWNER_DEV_CHAT){
        result=demo_ai_audio_prewarm_session(&s_lease,&extra);if(result){failure_log(FAIL_START,"prewarm",result,0,true);return result;}
    }
    discard_rx();s_guard=extra!=0;s_tail=now()+extra;s_fraction=0;
    s_peak=s_drops=s_played=s_sent=0;s_tx_timestamp=now();s_pause_seen=false;s_last_log=now();
    s_tx_failing=s_record_failing=s_play_failing=s_draining=false;
    liot_rtos_enter_critical();s_applied_revision=revision;s_mute_pending=false;s_running=true;
    s_dev_diagnostic.started_at=s_dev_diagnostic.reported_at=now();liot_rtos_exit_critical();
    return 0;
}
void calls_audio_receive(demo_tirtc_owner_e owner,uint32_t generation,const TIRTCFRAMEINFO *frame,const void *data)
{
    uint32_t write,first,tail;uint8_t rate;bool new_segment;
    if(!frame||!data||!frame->length||frame->length>RX_BYTES)return;
    rate=frame->flags==TIRTC_AUDIOSAMPLE_8K16B1C?8:
         frame->flags==TIRTC_AUDIOSAMPLE_16K16B1C?16:0;
    liot_rtos_enter_critical();
    if(!audio_session_locked(owner,generation)||!s_running||!speaker_allowed_locked()||s_mute_pending||s_draining){liot_rtos_exit_critical();return;}
    if(frame->media!=TIRTC_AUDIO_ALAW||!rate||
       (owner==DEMO_TIRTC_OWNER_LIVE ? (frame->stream_id!=10U&&frame->stream_id!=14U) :
        (rate!=8U||frame->stream_id!=(owner==DEMO_TIRTC_OWNER_WECHAT?0U:10U)))||frame->length>RX_BYTES-s_count){
        s_drops++;liot_rtos_exit_critical();return;
    }
    tail=(s_segment_head+s_segment_count+RX_SEGMENTS-1U)%RX_SEGMENTS;
    new_segment=!s_segment_count||s_segments[tail].rate!=rate;
    if(new_segment&&s_segment_count==RX_SEGMENTS){s_drops++;liot_rtos_exit_critical();return;}
    write=(s_read+s_count)%RX_BYTES;first=RX_BYTES-write;if(first>frame->length)first=frame->length;
    memcpy(s_rx+write,data,first);if(first<frame->length)memcpy(s_rx,(const uint8_t*)data+first,frame->length-first);
    if(new_segment){tail=(s_segment_head+s_segment_count)%RX_SEGMENTS;s_segments[tail].bytes=0;s_segments[tail].rate=rate;s_segment_count++;}
    s_segments[tail].bytes=(uint16_t)(s_segments[tail].bytes+frame->length);
    s_count+=frame->length;if(s_count>s_peak)s_peak=s_count;s_received++;s_received_bytes+=frame->length;
    if(!s_first_rx_logged&&!s_first_rx_pending){s_first_rx_pending=true;s_first_rx_stream=frame->stream_id;s_first_rx_rate=rate;s_first_rx_length=frame->length;}
    liot_rtos_exit_critical();
}
static void log_metrics(demo_tirtc_owner_e owner,uint32_t generation)
{
    dev_diagnostic_report(owner,generation,false);
    if((uint32_t)(now()-s_last_log)>=5000U){s_last_log=now();
        TIRTC_LOG_DEBUG("[AUDIO42] owner=%u gen=%lu tx=%lu tx_bytes=%lu rx=%lu rx_bytes=%lu play=%lu q=%lu peak=%lu drop=%lu capture_busy=%u\r\n",
            (unsigned)owner,(unsigned long)generation,(unsigned long)s_sent,(unsigned long)s_sent_bytes,
            (unsigned long)s_received,(unsigned long)s_received_bytes,(unsigned long)s_played,(unsigned long)s_count,
            (unsigned long)s_peak,(unsigned long)s_drops,s_record_busy?1U:0U);}
}
static int apply_controls(void)
{
    uint8_t volume,mic;uint32_t revision,discarded=0;bool busy,mute;int result;
    liot_rtos_enter_critical();revision=s_config_revision;volume=speaker_allowed_locked()?s_volume:0;
    mic=microphone_allowed_locked()?effective_mic_level_locked():0;
    busy=s_record_busy;mute=s_mute_pending;liot_rtos_exit_critical();
    if(mute&&!s_draining){
        if(busy)return DEMO_AI_AUDIO_ERR_BUSY;
        result=demo_ai_audio_set_levels(&s_lease,0,mic);if(result){failure_log(FAIL_CONTROL,"mute",result,0,result!=DEMO_AI_AUDIO_ERR_BUSY);return result;}
        discard_rx();s_draining=true;s_drain_since=now();
        liot_rtos_enter_critical();s_mute_pending=false;liot_rtos_exit_critical();
    }
    if(s_draining){
        if((s_guard&&before(s_tail+120U))||!demo_ai_audio_play_done(&s_lease)){
            if((uint32_t)(now()-s_drain_since)>=3000U)return fatal_error(FAIL_CONTROL,"mute_drain",DEMO_AI_AUDIO_ERR_BUSY,now()-s_drain_since);
            return DEMO_AI_AUDIO_ERR_BUSY;
        }
        s_draining=false;s_guard=false;s_continuing=false;
    }
    if(revision==s_applied_revision)return 0;
    if(busy)return DEMO_AI_AUDIO_ERR_BUSY;
    result=demo_ai_audio_set_levels(&s_lease,volume,mic);if(result){failure_log(FAIL_CONTROL,"levels",result,0,result!=DEMO_AI_AUDIO_ERR_BUSY);return result;}
    result=demo_ai_audio_discard_capture_timed(&s_lease,&discarded);if(result){failure_log(FAIL_CONTROL,"discard",result,0,result!=DEMO_AI_AUDIO_ERR_BUSY);return result;}
    liot_rtos_enter_critical();s_tx_timestamp+=discarded;s_applied_revision=revision;liot_rtos_exit_critical();
    return 0;
}
int calls_audio_step(demo_tirtc_owner_e owner,uint32_t generation)
{
    uint32_t started=now(),count,samples,source,first,extra,duration,fraction,i,queued;int result;bool playing,first_rx;
    bool direct_play=owner==DEMO_TIRTC_OWNER_DEV_CHAT;
    uint32_t prebuffer_bytes=direct_play?DEV_PREBUFFER_BYTES:PREBUFFER_BYTES;
    uint32_t prebuffer_ms=direct_play?DEV_PREBUFFER_TIMEOUT_MS:PREBUFFER_MS;
    uint32_t play_batch_max=direct_play?DEV_PLAY_BATCH_MAX:24U;
    uint8_t rate,first_stream,first_rate;uint32_t first_length;
    liot_rtos_enter_critical();
    if(!audio_session_locked(owner,generation)||!s_running){liot_rtos_exit_critical();return 0;}
    first_rx=s_first_rx_pending;s_first_rx_pending=false;
    first_stream=s_first_rx_stream;first_rate=s_first_rx_rate;first_length=s_first_rx_length;
    if(first_rx)s_first_rx_logged=true;
    liot_rtos_exit_critical();
    if(first_rx)TIRTC_LOG_DEBUG("[AUDIO42] first_rx owner=%u gen=%lu stream=%u rate=%u bytes=%lu\r\n",
        (unsigned)owner,(unsigned long)generation,(unsigned)first_stream,(unsigned)first_rate*1000U,(unsigned long)first_length);
    liot_rtos_enter_critical();result=s_error;liot_rtos_exit_critical();
    if(result)return result;
    log_metrics(owner,generation);
    result=apply_controls();if(result&&result!=DEMO_AI_AUDIO_ERR_BUSY)return result;
    liot_rtos_enter_critical();playing=speaker_allowed_locked()&&!s_mute_pending&&!s_draining;count=s_count;liot_rtos_exit_critical();
    if(!playing){capture_once();return 0;}
    if(s_continuing&&!before(s_tail)&&demo_ai_audio_play_done(&s_lease)){s_continuing=false;s_prebuffer=false;}
    if(!count){s_prebuffer=false;capture_once();return 0;}
    if(!s_continuing){
        if(!s_prebuffer){s_prebuffer=true;s_prebuffer_at=s_partial?s_partial_at:now();}
        if(count<prebuffer_bytes&&(uint32_t)(now()-s_prebuffer_at)<prebuffer_ms){capture_once();return 0;}
    }
    for(i=0;i<play_batch_max&&(uint32_t)(now()-started)<20U;i++){
        liot_rtos_enter_critical();count=s_count;playing=s_running&&speaker_allowed_locked()&&!s_mute_pending&&!s_draining;
        source=s_segment_count?s_segments[s_segment_head].bytes:0;
        rate=s_segment_count?s_segments[s_segment_head].rate:8;
        if(source>CALL_NETWORK_SAMPLES_8K)source=CALL_NETWORK_SAMPLES_8K;
        samples=rate==16U?source:source*2U;
        if(samples>1U&&(samples&1U))samples--;
        source=rate==16U?samples:samples/2U;
        first=RX_BYTES-s_read;if(first>source)first=source;
        memcpy(s_alaw,s_rx+s_read,first);if(source>first)memcpy(s_alaw+first,s_rx,source-first);
        liot_rtos_exit_critical();
        if(!playing||!count)break;
        if(!samples){
            /* An empty local PCM block cannot be submitted. */
            if(!s_partial){s_partial=true;s_partial_at=now();}
            if((uint32_t)(now()-s_partial_at)<PREBUFFER_MS)break;
            liot_rtos_enter_critical();consume_rx_locked(1);s_drops++;liot_rtos_exit_critical();s_partial=false;continue;
        }
        if(samples==1U){if(!s_partial){s_partial=true;s_partial_at=s_prebuffer?s_prebuffer_at:now();}
            if((uint32_t)(now()-s_partial_at)<PREBUFFER_MS)break;}
        else s_partial=false;
        queued=s_guard&&before(s_tail)?s_tail-now():0;
        duration=(samples+(samples&1U)+(s_continuing?s_fraction:0U))/16U;
        if(queued&&queued+duration+((s_continuing||direct_play)?0U:80U)+1U>PLAY_AHEAD)break;
        demo_g711_alaw_decode(s_alaw,s_pcm,source);
        if(rate==8U){
            uint32_t sample;
            for(sample=source; sample>0U; --sample){
                int16_t value=s_pcm[sample-1U];
                s_pcm[(sample-1U)*2U]=value;
                s_pcm[(sample-1U)*2U+1U]=value;
            }
        }
        if(samples&1U)s_pcm[samples]=0;
        extra=0;result=(s_continuing||direct_play)?demo_ai_audio_play(&s_lease,s_pcm,samples+(samples&1U)):
            demo_ai_audio_play_warm(&s_lease,s_pcm,samples+(samples&1U),&extra);
        if(result){
#if DEMO_AI_AUDIO_AEC_ENABLE
            /* The stream ring rejected the whole block. Keep RX at its head
             * and still service capture below; backpressure is not failure. */
            if(result==DEMO_AI_AUDIO_ERR_PLAY_BUSY){s_play_failing=false;break;}
#endif
            if(result==DEMO_AI_AUDIO_ERR_WARM_PARTIAL)return fatal_error(FAIL_PLAY,"warm_partial",result,0);
            if(!s_play_failing){s_play_failing=true;s_play_failure_at=now();}
            if((uint32_t)(now()-s_play_failure_at)>=200U)return fatal_error(FAIL_PLAY,"play",result,now()-s_play_failure_at);
            failure_log(FAIL_PLAY,"play",result,now()-s_play_failure_at,false);
            break;
        }
        fraction=samples+(samples&1U)+(s_continuing?s_fraction:0U);
        if(!s_guard||!before(s_tail))s_tail=now();
        s_tail+=duration+extra;s_fraction=(uint8_t)(fraction%16U);
        s_guard=s_continuing=true;s_prebuffer=s_partial=s_play_failing=false;s_played++;
        liot_rtos_enter_critical();consume_rx_locked(source);liot_rtos_exit_critical();
    }
    /* Capture after the playback burst in this same task. The call TX task
     * only sends already prepared A-law frames and never touches I2S. */
    capture_once();
    return 0;
}
int calls_audio_stop(demo_tirtc_owner_e owner,uint32_t generation)
{
    int result;bool busy,tx_busy;
    liot_rtos_enter_critical();
    if(!audio_session_locked(owner,generation)){liot_rtos_exit_critical();return 0;}
    s_running=false;busy=s_record_busy;tx_busy=s_tx_busy;liot_rtos_exit_critical();
    if(busy||tx_busy)return DEMO_AI_AUDIO_ERR_BUSY;
    result=demo_ai_audio_stop(&s_lease);if(result){failure_log(FAIL_STOP,"stop",result,0,false);return result;}
    result=demo_ai_audio_release(&s_lease);if(result){failure_log(FAIL_STOP,"release",result,0,false);return result;}
    if(s_tx_queue!=NULL){call_tx_job_t stale;while(liot_rtos_queue_wait(s_tx_queue,(uint8_t *)&stale,sizeof(stale),LIOT_NO_WAIT)==LIOT_OSI_SUCCESS){
        liot_rtos_enter_critical();dev_diagnostic_tx_locked(&stale,false,false,0,0);liot_rtos_exit_critical();}}
    dev_diagnostic_report(owner,generation,true);
    discard_rx();s_guard=s_continuing=false;s_owned=false;s_generation=0;return 0;
}
