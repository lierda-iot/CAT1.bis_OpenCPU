/* SDK boundaries are fake; RX FIFO, decode, playback retry and capture
 * admission below are extracted from the current production source. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calls_audio.h"
#include "g711_codec.h"
#include "liot_os.h"
#define TIRTC_LOG_DEBUG(...) ((void)0)
typedef void *liot_queue_t;
#define LIOT_NO_WAIT 0

/* PRODUCTION_DECLARATIONS */

static uint32_t clock_ms=1000;
static uint32_t record_delay_ms=20;
static uint32_t submitted_stamps[64], capture_completed[64];
static unsigned critical_depth, captures, queued_jobs, played_samples;
static int play_result, record_result;
static int16_t played[640];
void liot_rtos_enter_critical(void) {++critical_depth;}
void liot_rtos_exit_critical(void) {assert(critical_depth);--critical_depth;}
uint32_t liot_rtos_get_running_time(void) {return clock_ms;}
void liot_rtos_task_sleep_ms(uint32_t ms) {assert(!critical_depth);clock_ms+=ms;}
size_t liot_xPortGetFreeHeapSize(void) {return 100000;}
size_t liot_xPortGetMaximumFreeBlockSize(void) {return 50000;}
void liot_trace(const char *format,...) {(void)format;assert(!critical_depth);}
static void log_metrics(demo_tirtc_owner_e owner,uint32_t generation)
{(void)owner;(void)generation;}
int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *lease,uint8_t spk,uint8_t mic)
{(void)lease;(void)spk;(void)mic;assert(!critical_depth);return 0;}
int demo_ai_audio_discard_capture_timed(const demo_ai_audio_lease_t *lease,uint32_t *ms)
{(void)lease;*ms=0;return 0;}
int demo_ai_audio_record_20ms(const demo_ai_audio_lease_t *lease,int16_t pcm[320])
{(void)lease;assert(!critical_depth);++captures;clock_ms+=record_delay_ms;memset(pcm,0,640);return record_result;}
int liot_rtos_queue_release(liot_queue_t queue,uint32_t bytes,uint8_t *data,uint32_t wait)
{
    const call_tx_job_t *job=(const call_tx_job_t *)data;
    (void)queue;assert(!critical_depth && bytes==sizeof(*job) && wait==LIOT_NO_WAIT);
    assert(job->owner==s_owner && job->generation==s_generation);
    assert(job->frame.length==160 && job->frame.media==TIRTC_AUDIO_ALAW);
    assert(job->frame.flags==TIRTC_AUDIOSAMPLE_8K16B1C);
    assert(queued_jobs<64);submitted_stamps[queued_jobs]=job->frame.ts;
    capture_completed[queued_jobs]=clock_ms;++queued_jobs;return LIOT_OSI_SUCCESS;
}
bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *lease) {(void)lease;return false;}
int demo_ai_audio_play(const demo_ai_audio_lease_t *lease,const int16_t *pcm,uint32_t count)
{
    (void)lease;assert(!critical_depth && count>=2 && count<=320 && !(count&1));
    if(play_result)return play_result;
    assert(played_samples+count<=640);memcpy(played+played_samples,pcm,count*2);
    played_samples+=count;return 0;
}
int demo_ai_audio_play_warm(const demo_ai_audio_lease_t *lease,const int16_t *pcm,uint32_t count,uint32_t *extra)
{*extra=0;return demo_ai_audio_play(lease,pcm,count);}

/* PRODUCTION_FUNCTIONS */

static void timestamp_scenario(const char *scenario)
{
    unsigned i;
    uint32_t initial=clock_ms;
    if(!strcmp(scenario,"timestamp_wrap")) initial=clock_ms=UINT32_MAX-9U;
    s_tx_timestamp=initial;
    capture_once();
    assert(captures==1 && queued_jobs==1 && !s_error);
    if(!strcmp(scenario,"timestamp_continuous")) {
        for(i=0;i<9;i++)capture_once();
        assert(queued_jobs==10);
        for(i=1;i<queued_jobs;i++) {
            assert((uint32_t)(submitted_stamps[i]-submitted_stamps[i-1])==20U);
            assert((uint32_t)(capture_completed[i]-capture_completed[i-1])==20U);
        }
    } else if(!strcmp(scenario,"timestamp_timeout")) {
        /* AEC permits the next request after its 120 ms fresh-PCM timeout.
         * The caller's existing three-second error policy keeps this call up. */
        record_result=-40;record_delay_ms=120; /* HAL AEC_ERR_TIMEOUT */
        capture_once();
        assert(captures==2 && queued_jobs==1 && !s_error && s_record_failing);
        record_result=0;record_delay_ms=20;capture_once();
        assert(captures==3 && queued_jobs==2 && !s_error && !s_record_failing);
        assert((uint32_t)(capture_completed[1]-capture_completed[0])==145U);
        assert((uint32_t)(submitted_stamps[1]-submitted_stamps[0])==
               (DEMO_AI_AUDIO_AEC_ENABLE ? 145U : 40U));
    } else if(!strcmp(scenario,"timestamp_pause")) {
        s_microphone=false;capture_once();
        assert(captures==1 && queued_jobs==1 && s_pause_seen);
        clock_ms+=300U;s_microphone=true;capture_once();
        assert(captures==2 && queued_jobs==2 && !s_pause_seen);
        assert((uint32_t)(submitted_stamps[1]-submitted_stamps[0])==325U);
    } else {
        assert(!strcmp(scenario,"timestamp_wrap"));
        assert(capture_completed[0]==10U);
        /* A foreground scheduling gap discards unrequested AEC callbacks. */
        clock_ms+=73U;capture_once();
        assert(queued_jobs==2 && capture_completed[1]==103U);
        assert((uint32_t)(submitted_stamps[1]-submitted_stamps[0])==
               (DEMO_AI_AUDIO_AEC_ENABLE ? 93U : 20U));
    }
    for(i=0;i<queued_jobs;i++) {
#if DEMO_AI_AUDIO_AEC_ENABLE
        assert(submitted_stamps[i]==capture_completed[i]);
#else
        uint32_t expected=initial+i*20U;
        if(i && !strcmp(scenario,"timestamp_timeout"))expected+=20U;
        if(i && !strcmp(scenario,"timestamp_pause"))expected+=305U;
        assert(submitted_stamps[i]==expected);
#endif
    }
    assert(!critical_depth && !s_error);
}

int main(int argc,char **argv)
{
    TIRTCFRAMEINFO frame;uint8_t data[320];unsigned i;int result=0;
    assert(argc==3);
    s_owner=(demo_tirtc_owner_e)atoi(argv[1]);s_generation=7;s_running=s_owned=true;
    s_lease.owner=s_owner;s_lease.generation=7;s_applied_revision=s_config_revision;
    if(!strncmp(argv[2],"timestamp_",10)) {
        timestamp_scenario(argv[2]);
        printf("PASS call AEC=%d owner=%d %s\n",DEMO_AI_AUDIO_AEC_ENABLE,s_owner,argv[2]);
        return 0;
    }
    s_continuing=s_guard=true;s_tail=clock_ms+80;s_last_log=clock_ms;
    memset(&frame,0,sizeof(frame));frame.stream_id=s_owner==DEMO_TIRTC_OWNER_WECHAT?0:10;
    frame.media=TIRTC_AUDIO_ALAW;frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=sizeof(data);
    for(i=0;i<sizeof(data);i++)data[i]=(uint8_t)i;
    calls_audio_receive(s_owner,s_generation,&frame,data);assert(s_count==320);
    if(!strcmp(argv[2],"mute")) {
        s_microphone=false;
        assert(!calls_audio_step(s_owner,s_generation));
        assert(!captures && !queued_jobs && played_samples==640);
    } else if(!strcmp(argv[2],"record_busy")) {
        record_result=DEMO_AI_AUDIO_ERR_BUSY;
        assert(!calls_audio_step(s_owner,s_generation));
        assert(captures==1 && !queued_jobs && !s_error && !s_record_failing);
    } else {
        play_result=!strcmp(argv[2],"fatal") ? -43 : DEMO_AI_AUDIO_ERR_PLAY_BUSY;
        for(i=0;i<20 && !result;i++)result=calls_audio_step(s_owner,s_generation);
        assert(!played_samples && s_count==320 && s_read==0);
        if(!strcmp(argv[2],"fatal") || !DEMO_AI_AUDIO_AEC_ENABLE) {
            assert(result==AUDIO_ERROR && s_error==AUDIO_ERROR && queued_jobs>0);
        } else {
            assert(!result && !s_error && !s_play_failing && queued_jobs==20);
            play_result=0;
            assert(!calls_audio_step(s_owner,s_generation));
            assert(played_samples==640 && !s_count && queued_jobs==21);
            for(i=0;i<320;i++) {
                int16_t expected=demo_g711_alaw_decode_sample(data[i]);
                assert(played[i*2]==expected && played[i*2+1]==expected);
            }
            assert(!calls_audio_step(s_owner,s_generation));assert(played_samples==640);
        }
    }
    assert(!critical_depth);
    printf("PASS call AEC=%d owner=%d %s\n",DEMO_AI_AUDIO_AEC_ENABLE,s_owner,argv[2]);
    return 0;
}
