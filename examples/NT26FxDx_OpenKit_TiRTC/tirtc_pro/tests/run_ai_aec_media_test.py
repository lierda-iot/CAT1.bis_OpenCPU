#!/usr/bin/env python3
"""Exercise Pro's production AI media/protocol functions with real cJSON.

No hardware or network is used. The HAL fake preserves whole-block admission,
bounded playback capacity, capture timing and cancellation boundaries. Opus
packet duration is parsed by the production adapter; encode/decode are fakes
for queue/lifecycle checks, not codec fidelity or acoustic performance tests.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

CJSON_SHA256 = "fdfd427d82fadb395076567edf470c80cebee319e38fd417198508fe11ae56e7"


def function(source, name):
    match = re.search(r"(?m)^(?:static\s+)?[^;{}\n]+\b" + name + r"\s*\([^;{}]*\)\s*\{", source)
    if not match:
        raise AssertionError("Missing production function: " + name)
    start = source.index("{", match.start())
    depth = 1
    quoted = None
    escape = False
    i = start + 1
    while depth:
        c = source[i]
        if quoted:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == quoted:
                quoted = None
        elif source.startswith("/*", i):
            i = source.index("*/", i + 2) + 1
        elif source.startswith("//", i):
            i = source.index("\n", i)
        elif c in "\"'":
            quoted = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return source[match.start():i]


PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_device.h"
#include "opus_codec.h"
#include "opus.h"
#include "json_guard.h"
#define TIRTC_ENABLE_DEBUG_LOG 0
#define TIRTC_LOG_DEBUG(...) ((void)0)
#define liot_trace(...) ((void)0)
#define TIRTC_AUDIO_ALAW 7U
#define TIRTC_AUDIOSAMPLE_8K16B1C 8U
#define TIRTC_AUDIO_OPUS 9U
#define TIRTC_AUDIOSAMPLE_16K16B1C 16U
#define DEMO_TIRTC_OWNER_AI 1U
#define TIRTC_E_CONN_REMOTECLOSE (-101)
enum { TIRTC_AI_THINKING, TIRTC_AI_LISTENING, TIRTC_AI_SPEAKING };
typedef void *tirtc_conn_t;
typedef struct { uint8_t stream_id, media, flags; uint32_t ts, length; } TIRTCFRAMEINFO;
typedef enum { AI_IDLE, AI_TOKEN, AI_CONNECT, AI_NEGOTIATE, AI_ACTIVE, AI_STOPPING, AI_ERROR, AI_BLOCKED, AI_IDLE_FETCH, AI_WAIT_OWNER, AI_IDLE_CONNECT, AI_IDLE_READY } ai_phase_t;
typedef struct { uint8_t used; uint32_t order, epoch, received_at; uint16_t length; char data[AI_CMD_BYTES + 1U]; } ai_cmd_t;
static ai_phase_t s_phase;
static struct { uint8_t volume, mic_gain, audio_level; bool speaker_enabled, mic_enabled; int ai_state; } s_ui;
static bool s_want, s_callbacks, s_lost, s_audio_owned, s_audio_started, s_promoted_attempt;
static bool s_whip_done, s_standby, s_runtime_owned, s_resume_after_cleanup;
static int s_lost_error, s_whip_error;
static uint32_t s_connected_at, s_last_ui;
static uint32_t s_request, s_running, s_generation, s_deadline, s_order;
static tirtc_conn_t s_connection;
static demo_ai_audio_lease_t s_lease;
typedef struct { uint16_t length, samples; } ai_rx_packet_t;
static uint8_t s_rx[AI_RX_BYTES], s_play_opus[DEMO_AI_OPUS_MAX_PACKET], s_uplink_opus[DEMO_AI_OPUS_MAX_PACKET];
static ai_rx_packet_t s_rx_packets[AI_RX_PACKETS];
static uint32_t s_rx_head, s_rx_count, s_rx_samples;
static int16_t s_decode_pcm[DEMO_AI_OPUS_MAX_DECODE_SAMPLES];
static uint16_t s_decode_samples, s_decode_offset;
static int16_t s_pcm[DEMO_AI_AUDIO_FRAME_SAMPLES], s_record[DEMO_AI_AUDIO_FRAME_SAMPLES];
static uint32_t s_rx_read, s_rx_queued, s_rx_peak, s_rx_drop_bytes, s_rx_bad_frames, s_cmd_drops, s_play_errors;
static uint32_t s_tx, s_rx_frames, s_played, s_rx_drops, s_tx_drops, s_suppressed, s_capture;
static uint32_t s_play_audio_until, s_preroll_since, s_play_until, s_timestamp;
static bool s_play_continuing, s_preroll_waiting, s_play_guard, s_controls_dirty;
static bool s_capture_paused, s_speaker_mute_pending, s_speaker_draining;
static uint8_t s_play_sample_remainder;
static uint32_t s_capture_pause_at, s_capture_pause_remainder;
static bool s_play_retry; static uint32_t s_play_retry_since;
static bool s_suppression_hint, s_prime_wait_hint;
static uint8_t s_applied_volume, s_applied_mic_gain;
static bool s_applied_speaker, s_applied_mic;
static bool s_tx_failure_active, s_tx_failure_logged;
static uint32_t s_tx_failure_since, s_tx_failure_last_log;
static bool s_media_step_seen; static uint32_t s_media_step_at, s_media_gap_max, s_rebuffers;
static bool s_first_signal_logged, s_first_pcm_logged, s_first_rx_seen;
static uint32_t s_first_rx_at, s_call_ticket;
static char s_rpc_id[40];
static ai_cmd_t s_commands[AI_CMD_COUNT];
static bool s_first_rx_logged;
#if DEMO_AI_AUDIO_AEC_ENABLE
static bool s_record_failure_active;
static uint32_t s_record_failure_since, s_record_failure_generation;
static bool s_remote_tail, s_remote_tail_quiet;
static uint32_t s_remote_tail_progress, s_remote_tail_quiet_since, s_mic_capture_generation;
#endif
static uint32_t now_ms, drain_until, record_delay;
static bool network_ready, route_valid, sdk_busy, done_stuck, bounded_queue;
static bool hal_stopped, cancel_on_stop, audio_on_stop;
static uint32_t queued_at_stop;
static int record_error, prepare_error, record_marker;
static int query_error; static size_t query_used;
static unsigned query_calls, query_hook;
static unsigned prepare_calls;
static uint32_t discard_debt;
static int playback_error, finish_error, finish_count;
static unsigned captures, sends, level_calls, discards, stop_calls, warm_calls, captions, actions, cancelled;
static unsigned allocations, cases, played_samples;
static int16_t played_pcm[200000];
static uint32_t sent_ts[2000];
static uint8_t sent_first[2000];
static unsigned capture_hook, decode_hook;
static unsigned encode_calls, decode_calls, decoder_reset_calls, codec_reset_calls;
static int encode_error, decode_error, codec_init_error, decode_samples_override;
static int last_decode_bytes;
static uint8_t last_decode_marker;
static int16_t last_encoded_pcm[DEMO_AI_OPUS_FRAME_SAMPLES];
static bool calls_busy;
static unsigned lookup_steps;
static uint32_t tick(void) { return now_ms; }
static void liot_rtos_task_sleep_ms(uint32_t ms) { now_ms+=ms; }
static void liot_rtos_enter_critical(void) {}
static void liot_rtos_exit_critical(void) {}
static bool requested(void) { return s_want && s_request==s_running; }
static bool ready(void) { return network_ready; }
static bool route_current(void) { return route_valid; }
static void signal_worker(void) {}
static void cleanup_step(void) {}
static void begin(void) {}
static void idle_prepare_step(void) {}
static bool attempt_current(void) { return requested(); }
static bool demo_tirtc_ai_standby_allowed(void) { return true; }
static bool demo_tirtc_ai_standby_valid(uint32_t g) { (void)g; return true; }
static void claim_and_connect(void) {}
static void promote_standby(void) {}
static int start_command(void) { return 0; }
static void call_lookup_step(void) { ++lookup_steps; }
static bool tirtc_calls_has_session(void) { return calls_busy; }
static void diagnostics(void) {}
static void tirtc_ai_publish(void) {}
static void finish(int error) { ++finish_count; finish_error=error; s_phase=AI_STOPPING; s_callbacks=false; }
static void startup_stage(const char *a,uint32_t b,int c) { (void)a;(void)b;(void)c; }
static void show(int state,const char *text) { (void)text; s_ui.ai_state=state; }
static void tirtc_status_event(const char *text) { (void)text; }
static void tirtc_contacts_resolve_cancel(uint32_t ticket) { if(ticket) ++cancelled; }
void tirtc_ai_set_audio_config(uint8_t volume,uint8_t gain,bool speaker,bool mic);
static void caption(const cJSON *params) { (void)params; ++captions; }
static void call_received(const cJSON *root) { (void)root; ++actions; }
int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *l,uint8_t a,uint8_t b) {
    (void)l;(void)a;(void)b; ++level_calls; return 0;
}
int demo_ai_audio_discard_capture_timed(const demo_ai_audio_lease_t *l,uint32_t *ms) {
    (void)l; ++discards; *ms=discard_debt; discard_debt=0; return 0;
}
int demo_ai_audio_record_20ms(const demo_ai_audio_lease_t *l,int16_t *pcm) {
    unsigned i; (void)l;
    if(hal_stopped) return -1;
    /* BUSY is immediate; timeout/error waits still allow concurrent UI intent. */
    if(record_error!=DEMO_AI_AUDIO_ERR_BUSY) now_ms+=record_delay;
    if(!record_error)++captures;
    for(i=0;i<DEMO_AI_AUDIO_FRAME_SAMPLES;++i) pcm[i]=(int16_t)(i+100+record_marker);
    if(capture_hook==1U) s_ui.mic_enabled=false;
    if(capture_hook==2U) ++s_request;
    if(capture_hook==4U) network_ready=false;
    if(capture_hook==5U) route_valid=false;
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(capture_hook==3U) {
        tirtc_ai_set_audio_config(s_ui.volume,s_ui.mic_gain,true,false);
        tirtc_ai_set_audio_config(s_ui.volume,s_ui.mic_gain,true,true);
    }
#endif
    return record_error;
}
int demo_ai_audio_play(const demo_ai_audio_lease_t *l,const int16_t *pcm,uint32_t samples) {
    (void)l;
    if(hal_stopped) return -1;
    assert(samples>=2U && samples<=320U && !(samples&1U));
    if(sdk_busy || (bounded_queue && (int32_t)(drain_until-now_ms)>236)) return -42;
    if(playback_error) return playback_error;
    assert(played_samples+samples<sizeof(played_pcm)/sizeof(played_pcm[0]));
    memcpy(played_pcm+played_samples,pcm,samples*sizeof(*pcm)); played_samples+=samples;
    if((int32_t)(drain_until-now_ms)<0) drain_until=now_ms;
    drain_until+=(samples+15U)/16U;
    return 0;
}
int demo_ai_audio_play_warm(const demo_ai_audio_lease_t *l,const int16_t *pcm,uint32_t n,uint32_t *ms) {
    int result; ++warm_calls; *ms=0; result=demo_ai_audio_play(l,pcm,n);
#if !DEMO_AI_AUDIO_AEC_ENABLE
    if(!result) { *ms=80U; drain_until+=80U; }
#endif
    return result;
}
bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *l) {
    (void)l; return !done_stuck && (int32_t)(now_ms-drain_until)>=0;
}
static void put(unsigned length,uint8_t value);
int demo_ai_audio_stop(const demo_ai_audio_lease_t *l) {
    (void)l; ++stop_calls; hal_stopped=true; queued_at_stop=s_rx_queued;
    /* The real stream Stop yields while muted DMA drains. The network
     * callback can deliver a later reply before this call returns. */
    if(audio_on_stop) { now_ms+=100U; put(160,99); now_ms+=100U; }
    if(cancel_on_stop)++s_request;
    return 0;
}
int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *l,uint8_t a,uint8_t b) {
    (void)l;(void)a;(void)b; ++prepare_calls;
    if(prepare_error) return prepare_error;
    hal_stopped=false; return 0;
}
int demo_ai_audio_acquire(demo_ai_audio_owner_e owner,demo_ai_audio_lease_t *lease) {
    (void)owner;(void)lease; return 0;
}
int demo_ai_audio_prewarm_session(const demo_ai_audio_lease_t *lease,uint32_t *extra) {
    (void)lease;
#if DEMO_AI_AUDIO_AEC_ENABLE
    *extra=0;
#else
    *extra=DEMO_AI_AUDIO_SESSION_WARMUP_MS-DEMO_AI_AUDIO_WARMUP_MS;
#endif
    return 0;
}
int demo_ai_opus_init(void) { return codec_init_error; }
void demo_ai_opus_reset(void) { ++codec_reset_calls; }
void demo_ai_opus_reset_decoder(void) { ++decoder_reset_calls; }
void demo_ai_opus_deinit(void) {}
int demo_ai_opus_encode_20ms(const int16_t *pcm,uint8_t *packet,int capacity) {
    ++encode_calls; assert(capacity>=60); memcpy(last_encoded_pcm,pcm,sizeof(last_encoded_pcm));
    if(encode_error)return encode_error;
    memset(packet,(uint8_t)pcm[0],60);packet[0]=0x08U;return 60;
}
int demo_ai_opus_decode(const uint8_t *packet,int length,int16_t *pcm,int capacity) {
    int i,n; ++decode_calls;last_decode_bytes=length;
    if(decode_error)return decode_error;
    n=demo_ai_opus_packet_samples(packet,length);
    assert(n>0 && n<=capacity && length>=2);
    last_decode_marker=packet[1];
    for(i=1;i<length;++i)assert(packet[i]==last_decode_marker);
    for(i=0;i<n;++i)pcm[i]=(int16_t)last_decode_marker;
    if(decode_hook==1U)s_want=false;
    if(decode_hook==2U)++s_request;
    if(decode_hook==3U)network_ready=false;
    if(decode_hook==4U)route_valid=false;
    if(decode_hook==5U)tirtc_ai_set_audio_config(8,7,false,true);
    if(decode_hook==6U)tirtc_ai_set_audio_config(0,7,true,true);
    return decode_samples_override ? decode_samples_override : n;
}
static int demo_tirtc_get_send_buffer_used(unsigned owner,uint32_t gen,size_t *used) {
    (void)owner;(void)gen;++query_calls;*used=query_used;
    if(query_hook==1U)++s_request;
    if(query_hook==2U)tirtc_ai_set_audio_config(8,7,true,false);
#if DEMO_AI_AUDIO_AEC_ENABLE
    if(query_hook==3U) {
        tirtc_ai_set_audio_config(8,7,true,false);tirtc_ai_set_audio_config(8,7,true,true);
    }
#endif
    if(query_hook==4U)route_valid=false;
    if(query_hook==5U)network_ready=false;
    return query_error;
}
static int demo_tirtc_send_audio(unsigned owner,uint32_t gen,const TIRTCFRAMEINFO *f,const void *p) {
    (void)owner;(void)gen;(void)p;
    assert(f->stream_id==1 && f->media==TIRTC_AUDIO_OPUS && f->flags==TIRTC_AUDIOSAMPLE_16K16B1C);
    assert(f->length==60U && sends<2000U);
    sent_first[sends]=((const uint8_t *)p)[1];sent_ts[sends++]=f->ts;return 60;
}
static void *count_alloc(size_t size) { void *p=malloc(size); if(p)++allocations; return p; }
static void count_free(void *p) { if(p) { assert(allocations); --allocations; free(p); } }
'''

SUFFIX = r'''
static void reset(void) {
    assert(!allocations);
    memset(&s_ui,0,sizeof(s_ui)); s_ui.volume=8; s_ui.mic_gain=7;
    s_ui.speaker_enabled=s_ui.mic_enabled=true;
    s_phase=AI_ACTIVE; s_want=s_callbacks=s_audio_owned=s_audio_started=true;
    s_request=s_running=7; s_generation=9; s_order=0; s_connection=(void *)1;
    network_ready=route_valid=true; sdk_busy=done_stuck=bounded_queue=false;
    hal_stopped=cancel_on_stop=audio_on_stop=false; queued_at_stop=0;
    record_error=prepare_error=record_marker=0; prepare_calls=0; discard_debt=0;
    query_error=0; query_used=0U; query_calls=query_hook=0U;
    now_ms=1000; drain_until=1000; record_delay=20;
    s_lost=s_promoted_attempt=false; s_deadline=0; s_call_ticket=0;
    s_whip_done=s_standby=s_runtime_owned=s_resume_after_cleanup=false;
    s_lost_error=s_whip_error=0; s_connected_at=s_last_ui=0;
    s_first_rx_logged=calls_busy=false; lookup_steps=0; memset(s_commands,0,sizeof(s_commands));
    s_rx_head=s_rx_count=s_rx_samples=0; s_decode_samples=s_decode_offset=0;
    memset(s_rx_packets,0,sizeof(s_rx_packets));
    encode_calls=decode_calls=decoder_reset_calls=codec_reset_calls=0;
    encode_error=decode_error=codec_init_error=decode_samples_override=last_decode_bytes=0;
    last_decode_marker=0;
    s_rx_read=s_rx_queued=s_rx_peak=s_rx_drop_bytes=s_rx_bad_frames=s_cmd_drops=s_play_errors=0;
    s_tx=s_rx_frames=s_played=s_rx_drops=s_tx_drops=s_suppressed=s_capture=0;
    s_play_audio_until=s_preroll_since=s_play_until=s_timestamp=0;
    s_play_continuing=s_preroll_waiting=s_play_guard=s_controls_dirty=false;
    s_capture_paused=s_speaker_mute_pending=s_speaker_draining=false;
    s_play_sample_remainder=0;
    s_capture_pause_at=s_capture_pause_remainder=0; s_play_retry=false; s_play_retry_since=0;
    s_suppression_hint=s_prime_wait_hint=false;
    s_applied_volume=8; s_applied_mic_gain=7; s_applied_speaker=s_applied_mic=true;
    s_tx_failure_active=s_tx_failure_logged=false; s_tx_failure_since=s_tx_failure_last_log=0;
    s_media_step_seen=false; s_media_step_at=s_media_gap_max=s_rebuffers=0;
    s_first_signal_logged=s_first_pcm_logged=s_first_rx_seen=false;
    playback_error=finish_error=finish_count=0;
    captures=sends=level_calls=discards=stop_calls=warm_calls=captions=actions=cancelled=0;
    capture_hook=decode_hook=played_samples=0; strcpy(s_rpc_id,"pro-start-7");
#if DEMO_AI_AUDIO_AEC_ENABLE
    s_record_failure_active=false; s_record_failure_since=s_record_failure_generation=0;
    s_remote_tail=s_remote_tail_quiet=false;
    s_remote_tail_progress=s_remote_tail_quiet_since=s_mic_capture_generation=0;
#endif
}
static void put_samples(unsigned length,uint8_t value,unsigned samples) {
    uint8_t data[2048]; TIRTCFRAMEINFO f={1,TIRTC_AUDIO_OPUS,TIRTC_AUDIOSAMPLE_16K16B1C,0,0};
    assert(length>=2 && length<=sizeof(data)); memset(data,value,length); f.length=length;
    switch(samples) {
        case 40:data[0]=0x80U;break;
        case 80:data[0]=0x88U;break;
        case 160:data[0]=0x00U;break;
        case 320:data[0]=0x08U;break;
        case 640:data[0]=0x10U;break;
        case 960:data[0]=0x18U;break;
        case 1920:data[0]=0x19U;break;
        default:assert(!"unsupported test duration");
    }
    audio_received(s_connection,&f,data);
}
static void put(unsigned length,uint8_t value) { put_samples(length,value,320U); }
static void command_bytes(const char *json,size_t length) {
    ai_cmd_t m; memset(&m,0,sizeof(m)); assert(length<=AI_CMD_BYTES);
    m.epoch=s_running; m.length=(uint16_t)length; memcpy(m.data,json,length);
    handle_command(&m); assert(!allocations);
}
static void command(const char *json) { command_bytes(json,strlen(json)); }
static void protocols(void) {
    const char *ack="{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"result\":{\"session_id\":\"x\"}}";
    const char *end="{\"jsonrpc\":\"2.0\",\"method\":\"end_session\"}";
    char bytes[300];
    reset(); s_phase=AI_NEGOTIATE; command(ack); assert(s_phase==AI_ACTIVE && !finish_count); ++cases;
    reset(); s_phase=AI_NEGOTIATE; command("{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"result\":{\"session_id\":\"x\",\"output_audio\":{\"codec\":\"opus\",\"sample_rate\":16000,\"channels\":1}}}"); assert(s_phase==AI_ACTIVE); ++cases;
    reset(); s_phase=AI_NEGOTIATE; command("{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"result\":{\"session_id\":\"x\",\"output_audio\":{\"codec\":\"g711a\",\"sample_rate\":8000,\"channels\":1}}}"); assert(finish_error==AI_ERR_PROTOCOL); ++cases;
    reset(); command("{\"jsonrpc\":\"2.0\",\"method\":\"caption\",\"params\":{\"text\":\"hi\"}}"); assert(captions==1); ++cases;
    reset(); command("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"call_intent\",\"params\":{}}"); assert(actions==1); ++cases;
    reset(); command("{\"jsonrpc\":\"2.0\",\"id\":\"call\",\"method\":\"ai_call_intent\",\"params\":{}}"); assert(actions==1); ++cases;
    reset(); command("{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"end_session\"}"); assert(!finish_count);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(!s_remote_tail);
#endif
    ++cases;
    reset(); command("{\"method\":\"end_session\"}"); assert(!finish_count); ++cases;
    reset(); snprintf(bytes,sizeof(bytes),"%s junk",end); command(bytes); assert(!finish_count); ++cases;
    reset(); memcpy(bytes,end,strlen(end)+1); memcpy(bytes+strlen(end)+1,"{}",2); command_bytes(bytes,strlen(end)+3); assert(!finish_count); ++cases;
    reset(); command_bytes(end,strlen(end)+1);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(s_remote_tail && !finish_count);
#else
    assert(finish_count==1);
#endif
    ++cases;
}
static void media_cases(void) {
    unsigned i;
    reset(); assert(prepare_audio());
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(!s_play_guard && !s_play_audio_until);
#else
    assert(s_play_guard && s_play_audio_until==now_ms+1200U);
#endif
    ++cases;
    reset(); put(160,10); s_preroll_waiting=true; s_preroll_since=now_ms-AI_PREROLL_WAIT_MS;
    assert(submit_playback(true,8) && warm_calls==1 && played_samples==320);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(s_play_audio_until==now_ms+20U && s_play_until==now_ms+140U);
#else
    assert(s_play_audio_until==now_ms+100U && s_play_until==now_ms+220U);
    reset(); s_controls_dirty=true; s_timestamp=1000U; discard_debt=60U; media_step();
    assert(sends==1 && sent_ts[0]==1060U && s_timestamp==1080U); ++cases;
#endif
    ++cases;
    reset(); put(160,11); s_play_continuing=true; s_play_audio_until=now_ms+20; media_step();
    assert(played_samples==320 && s_rx_queued==0);
    for(i=0;i<320;++i)assert(played_pcm[i]==11);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(sends==1 && captures==1 && sent_ts[0]==now_ms);
#else
    assert(!sends && s_capture_paused);
#endif
    ++cases;
    reset(); put(160,12); s_play_continuing=true; s_play_audio_until=now_ms+1000; sdk_busy=true;
    media_step(); now_ms+=300; media_step(); assert(s_rx_queued==160 && !played_samples);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(!finish_count && !s_play_errors && sends==2);
    sdk_busy=false; put(160,13); media_step(); assert(!s_rx_queued && played_samples==640);
    for(i=0;i<320;++i)assert(played_pcm[i]==12 && played_pcm[i+320]==13);
#else
    assert(finish_error==AI_ERR_AUDIO && s_play_errors==2);
#endif
    ++cases;
    reset(); s_ui.mic_enabled=false; media_step(); assert(!captures && !sends && s_capture_paused); ++cases;
    reset(); capture_hook=1; media_step(); assert(captures==1 && !sends); ++cases;
    reset(); capture_hook=2; media_step(); assert(captures==1 && !sends); ++cases;
#if DEMO_AI_AUDIO_AEC_ENABLE
    reset(); capture_hook=3; media_step(); assert(captures==1 && !sends); ++cases;
    reset(); media_step(); now_ms+=95; media_step(); assert(sends==2 && sent_ts[1]-sent_ts[0]==115U); ++cases;
    reset(); now_ms=0xfffffff0U; drain_until=now_ms; media_step(); media_step(); assert(sends==2 && sent_ts[1]-sent_ts[0]==20U); ++cases;
    reset(); record_error=DEMO_AI_AUDIO_ERR_BUSY; media_step(); assert(!finish_count && !sends && now_ms==1000+AI_PLAY_POLL_MS);
    record_error=0; media_step(); assert(sends==1); ++cases;
    reset(); put(160,33); s_play_continuing=true; s_play_audio_until=now_ms+1000;
    playback_error=-5; media_step(); assert(s_play_retry && !finish_count);
    sdk_busy=true; now_ms+=500; media_step(); assert(!s_play_retry && !finish_count);
    sdk_busy=false; media_step(); assert(s_play_retry && !finish_count); ++cases;
    reset(); s_speaker_mute_pending=true; s_play_guard=true; s_play_until=now_ms+100; put(160,22);
    media_step(); assert(!s_rx_queued && sends==1 && s_speaker_draining); ++cases;
#endif
    reset(); put(160,33); s_play_continuing=true; s_play_audio_until=now_ms+20; playback_error=DEMO_AI_AUDIO_ERR_WARM_PARTIAL; media_step(); assert(finish_error==AI_ERR_AUDIO && s_rx_queued==160); ++cases;
    reset(); put_samples(3,44,40); media_step(); assert(!played_samples); now_ms+=AI_PREROLL_WAIT_MS; media_step();
    assert(played_samples==40 && !s_rx_queued && played_pcm[0]==44 && played_pcm[39]==44); ++cases;
    reset(); { uint8_t b=1; TIRTCFRAMEINFO f={2,TIRTC_AUDIO_OPUS,TIRTC_AUDIOSAMPLE_16K16B1C,0,1}; audio_received(s_connection,&f,&b); }
    assert(!s_rx_queued && s_rx_bad_frames==1); ++cases;
}



static void opus_packet_cases(void) {
    unsigned i; uint32_t before; uint16_t n;
    /* Known duration vectors exercise SILK, hybrid, CELT and multi-frame
     * parsing through the repository's original libopus implementation. */
    { static const struct {uint8_t toc,count;int samples;} vectors[]={
        {0x00,0,160},{0x08,0,320},{0x10,0,640},{0x18,0,960},
        {0x60,0,160},{0x68,0,320},{0x80,0,40},{0x88,0,80},
        {0x90,0,160},{0x98,0,320},{0x19,0,1920},{0x83,48,1920}};
      uint8_t p[2];for(i=0;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
          p[0]=vectors[i].toc;p[1]=vectors[i].count;
          assert(demo_ai_opus_packet_samples(p,2)==vectors[i].samples);
      }
      p[0]=0x83;p[1]=49;assert(demo_ai_opus_packet_samples(p,2)<0);
      assert(demo_ai_opus_packet_samples(p,1)<0);
      assert(demo_ai_opus_packet_samples(NULL,2)<0);
      assert(demo_ai_opus_packet_samples(p,0)<0);
      assert(demo_ai_opus_packet_samples(p,1276)<0);++cases;
    }
    /* The next packet may be longer in bytes but shorter in PCM duration.
     * Consume a complete decoded packet, never byte slices from two packets. */
    reset(); put_samples(11,11,160); put_samples(1275,22,640);
    assert(s_rx_count==2 && queued_audio_samples()==800);
    n=peek_audio(); assert(n==160 && decode_calls==1 && last_decode_bytes==11);
    for(i=0;i<n;++i)assert(s_pcm[i]==11);consume_audio(n);
    assert(s_rx_count==1 && s_rx_queued==1275 && queued_audio_samples()==640);
    for(i=0;i<2;++i) {
        n=peek_audio(); assert(n==320 && decode_calls==2 && last_decode_bytes==1275);
        assert(s_pcm[0]==22 && s_pcm[319]==22); consume_audio(n);
    }
    assert(!s_rx_count && !s_rx_queued && !queued_audio_samples());++cases;
    /* Maximum 120 ms packet is decoded exactly once and split into six
     * hardware-sized 20 ms blocks; duration tracks only the remaining PCM. */
    reset(); put_samples(1201,33,1920);
    for(i=0;i<6;++i) {
        assert(queued_audio_samples()==1920-i*320);
        n=peek_audio();assert(n==320 && decode_calls==1);
        assert(s_pcm[0]==33 && s_pcm[319]==33);consume_audio(n);
        assert(s_rx_count==(i==5 ? 0U : 1U));
    }
    assert(!s_rx_queued && !queued_audio_samples());++cases;
    /* Pre-roll is measured in audio time, not compressed byte count. A
     * quiet low-bitrate packet and a large one must obey the same deadline. */
    reset();put_samples(3,11,1920);put_samples(3,22,1920);
    assert(s_rx_queued==6 && playback_buffer_ready(true,8));++cases;
    reset();for(i=0;i<4;++i)put(1275,33);
    assert(s_rx_queued>AI_PREROLL_SAMPLES && !playback_buffer_ready(true,8));
    now_ms+=AI_PREROLL_WAIT_MS;assert(playback_buffer_ready(true,8));++cases;
    /* Byte ring wrap retains the exact compressed packet. */
    reset();s_rx_read=AI_RX_BYTES-5U;put_samples(37,44,320);
    assert(peek_audio()==320 && last_decode_bytes==37 && s_pcm[319]==44);
    consume_audio(320);assert(!s_rx_queued && s_rx_read==32U);++cases;
    /* Packet descriptors, compressed bytes and decoded duration are separate
     * admission limits. Rejection leaves every earlier packet undamaged. */
    reset();for(i=0;i<AI_RX_PACKETS;++i)put_samples(3,55,40);
    before=s_rx_drops;put_samples(3,66,40);
    assert(s_rx_count==AI_RX_PACKETS && s_rx_drops==before+1 && s_rx_queued==AI_RX_PACKETS*3U);
    assert(peek_audio()==40 && s_pcm[0]==55);consume_audio(40);put_samples(3,66,40);
    assert(s_rx_count==AI_RX_PACKETS);++cases;
    reset();for(i=0;i<AI_RX_MAX_SAMPLES/1920U;++i)put_samples(3,55,1920);
    before=s_rx_drops;put(160,66);
    assert(queued_audio_samples()==AI_RX_MAX_SAMPLES && s_rx_drops==before+1);
    assert(s_rx_count==AI_RX_MAX_SAMPLES/1920U);++cases;
    reset();for(i=0;i<AI_RX_BYTES/1275U;++i)put(1275,55);
    before=s_rx_drops;put(1275,66);
    assert(s_rx_queued==(AI_RX_BYTES/1275U)*1275U && s_rx_drops==before+1);
    assert(peek_audio()==320 && s_pcm[0]==55);consume_audio(320);put(1275,66);
    assert(s_rx_count==AI_RX_BYTES/1275U);++cases;
    /* Corrupt packet framing and a decoder error retire a whole packet;
     * they cannot be mistaken for silence or poison the next boundary. */
    reset();{uint8_t invalid[2]={0x03U,0U};TIRTCFRAMEINFO f={1,TIRTC_AUDIO_OPUS,TIRTC_AUDIOSAMPLE_16K16B1C,0,2};
        audio_received(s_connection,&f,invalid);assert(!s_rx_count && !s_rx_queued && s_rx_bad_frames==1);}
    put(1276,66);assert(!s_rx_count && !s_rx_queued && s_rx_bad_frames==2);++cases;
    reset();put(77,55);put(79,66);decode_error=-4;
    assert(peek_audio()==0 && s_rx_count==1 && s_rx_queued==79 && decoder_reset_calls==1);
    decode_error=0;assert(peek_audio()==320 && s_pcm[0]==66 && last_decode_bytes==79);
    consume_audio(320);assert(!s_rx_count && !s_rx_queued);++cases;
    reset();put(77,55);decode_samples_override=160;
    assert(peek_audio()==0 && !s_rx_count && !s_rx_queued && decoder_reset_calls==1);++cases;
    /* A busy HAL must retain decoded PCM and must not feed the same Opus
     * packet into a stateful decoder twice. */
    reset();put_samples(1201,77,1920);s_play_continuing=true;s_play_audio_until=now_ms+20;sdk_busy=true;
    submit_playback(true,8);submit_playback(true,8);
    assert(decode_calls==1 && !played_samples && s_decode_offset==0 && s_rx_count==1);
    sdk_busy=false;submit_playback(true,8);
    assert(decode_calls==1 && played_samples==1920 && !s_rx_count && !s_rx_queued);++cases;
    /* Interrupt/mute queue clearing must also remove partial decoded output
     * and reset decoder history before accepting a replacement response. */
    reset();put_samples(1201,77,1920);assert(peek_audio()==320);consume_audio(320);
    before=decoder_reset_calls;discard_audio_queue();
    assert(!s_rx_count && !s_rx_queued && !s_rx_samples && !s_decode_samples && !s_decode_offset);
    assert(decoder_reset_calls==before+1 && queued_audio_samples()==0);
    put(160,88);assert(peek_audio()==320 && s_pcm[0]==88);consume_audio(320);++cases;
    /* Encode receives the entire original 16 kHz frame, without A-law
     * downsampling. A codec failure must not submit a stale packet. */
    reset();media_step();assert(encode_calls==1 && sends==1);
    for(i=0;i<DEMO_AI_OPUS_FRAME_SAMPLES;++i)assert(last_encoded_pcm[i]==(int16_t)(100+i));++cases;
    reset();encode_error=-4;media_step();assert(encode_calls==1 && !sends);++cases;
}

static void uplink_backpressure_cases(void) {
    const size_t limit=12U*1024U;unsigned i;uint32_t first;
    /* Match Lite's 12 KiB boundary: equality remains admissible. */
    for(i=0U;i<3U;++i) {
        reset();query_used=limit+(int)i-1;media_step();
        assert(query_calls==1U && !finish_count && captures==1U);
        assert(sends==(i<2U ? 1U : 0U));assert(s_tx_drops==(i==2U ? 1U : 0U));++cases;
    }
    /* Backlog never stops playback. AEC also keeps fresh acquisition running. */
    reset();query_used=limit+1U;media_step();assert(!sends && s_tx_drops==1U);
    put(160U,77U);media_step();now_ms+=AI_PREROLL_WAIT_MS;media_step();
    assert(played_samples==320U && !sends && !finish_count);++cases;
    /* Congestion drops PCM rather than retaining speech for replay later. */
    reset();query_used=limit+1U;record_marker=0;media_step();assert(!sends);
    query_used=0U;record_marker=55;media_step();
    assert(sends==1U && sent_first[0]==155U && captures==2U && !finish_count);
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(sent_ts[0]==now_ms);
#else
    assert(sent_ts[0]==20U);
#endif
    ++cases;
    /* SDK query errors must not masquerade as accepted PCM or immediate exit. */
    reset();query_error=-77;media_step();first=now_ms;
    assert(!sends && !finish_count && s_tx_drops==1U);
    for(i=0U;i<149U;++i) {media_step();assert(!finish_count && !sends);}
    assert((uint32_t)(now_ms-first)==2980U);media_step();
    assert((uint32_t)(now_ms-first)==3000U && finish_count==1 && finish_error==AI_ERR_UPLINK);++cases;
    reset();query_error=-77;media_step();now_ms+=2800U;
    query_error=0;media_step();assert(sends==1U && !finish_count);
    now_ms+=300U;query_error=-77;media_step();assert(sends==1U && !finish_count);++cases;
    /* A successful congestion query also retires a previous SDK error budget. */
    reset();query_error=-77;media_step();now_ms+=3000U;
    query_error=0;query_used=limit+1U;media_step();assert(!finish_count && !sends);
    query_error=-77;query_used=0U;media_step();assert(!finish_count && !sends);++cases;
    /* Cancellation/mute may race a query, just as they can race capture. */
    for(i=1U;i<=5U;++i) {
#if !DEMO_AI_AUDIO_AEC_ENABLE
        if(i==3U)continue; /* Capture epoch fencing is an AEC contract. */
#endif
        reset();query_hook=i;media_step();
        assert(query_calls==1U && !sends && !finish_count);++cases;
    }
    reset();now_ms=UINT32_MAX-1000U;drain_until=now_ms;query_error=-77;
    media_step();first=now_ms;
    for(i=0U;i<149U;++i) {media_step();assert(!finish_count);}
    media_step();assert((uint32_t)(now_ms-first)==3000U && finish_error==AI_ERR_UPLINK);++cases;
}

static void queue_start_ack(const char *ack) {
    command_received(s_connection,AI_CMD,ack,(uint32_t)strlen(ack));
    assert(s_commands[0].used);
}
static void negotiation_audio_cases(void) {
    const char *ack="{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"result\":{\"session_id\":\"x\"}}";
    const char *bad_profile="{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"result\":{\"session_id\":\"x\",\"output_audio\":{\"codec\":\"g711a\",\"sample_rate\":8000,\"channels\":1}}}";
    const char *rejected="{\"jsonrpc\":\"2.0\",\"id\":\"pro-start-7\",\"error\":{\"code\":-1}}";
    unsigned i;
    /* Real SDK command callbacks enqueue ACK; the worker handles it later.
     * Audio racing behind that ACK must survive without bypassing negotiation. */
    reset();s_phase=AI_NEGOTIATE;queue_start_ack(ack);put_samples(1201U,77U,1920U);
    assert(s_rx_queued==1201U && s_rx_frames==1U && !played_samples);
    assert(s_phase==AI_NEGOTIATE && !sends && !captures);
    step();assert(s_phase==AI_ACTIVE && !finish_count);
    for(i=0;i<200U && s_rx_queued;++i) {now_ms+=20U;step();}
    assert(!s_rx_queued && played_samples==1920U && !finish_count);
    for(i=0;i<played_samples;++i)assert(played_pcm[i]==77);++cases;
    /* An early packet without an ACK remains bounded queued data only. */
    reset();s_phase=AI_NEGOTIATE;s_deadline=now_ms+1000U;put_samples(1201U,66U,1920U);
    step();assert(s_phase==AI_NEGOTIATE && s_rx_queued==1201U && !played_samples && !captures && !sends);++cases;
    /* Bad/rejected ACK must close before any buffered PCM reaches playback. */
    for(i=0;i<2U;++i) {
        reset();s_phase=AI_NEGOTIATE;queue_start_ack(i ? rejected : bad_profile);put_samples(1201U,77U,1920U);
        step();assert(finish_count==1 && finish_error==AI_ERR_PROTOCOL && !played_samples && !sends && !captures);++cases;
    }
    reset();s_phase=AI_NEGOTIATE;s_deadline=now_ms+100U;put_samples(1201U,77U,1920U);
    step();assert(!finish_count && !played_samples);now_ms+=100U;step();
    assert(finish_count==1 && finish_error==AI_ERR_TIMEOUT && !played_samples && !sends);++cases;
    /* User cancellation rejects later packets and never plays the saved prefix. */
    reset();s_phase=AI_NEGOTIATE;put_samples(1201U,77U,1920U);++s_request;put(160U,88U);
    assert(s_rx_queued==1201U);step();assert(finish_count==1 && !played_samples && !sends);++cases;
    reset();s_phase=AI_NEGOTIATE;s_ui.speaker_enabled=false;put_samples(1201U,77U,1920U);
    assert(!s_rx_queued && !played_samples);++cases;
    reset();s_phase=AI_NEGOTIATE;
    {uint8_t b=1;TIRTCFRAMEINFO f={2,TIRTC_AUDIO_OPUS,TIRTC_AUDIOSAMPLE_16K16B1C,0,1};
     audio_received(s_connection,&f,&b);assert(!s_rx_queued && s_rx_bad_frames==1U);}
    ++cases;
    reset();s_phase=AI_CONNECT;put_samples(1201U,77U,1920U);assert(!s_rx_queued);++cases;
}

static void capture_timeout_cases(void) {
#if DEMO_AI_AUDIO_AEC_ENABLE
    uint32_t first; unsigned i;
    /* One timeout accepts no stale PCM and must recover with the next frame. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); assert(now_ms==1120U && !finish_count && !sends && !captures);
    record_error=0; record_delay=20; media_step();
    assert(!finish_count && sends==1 && sent_ts[0]==now_ms); ++cases;
    /* Fail only after 3 seconds of consecutive timed-out acquisition. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); first=now_ms;
    for(i=0;i<24U;++i) { media_step(); assert(!finish_count && !sends); }
    assert(now_ms-first==2880U); media_step();
    assert(now_ms-first==3000U && finish_count==1 && finish_error==AI_ERR_AUDIO); ++cases;
    /* A successful fresh capture restarts the next failure budget. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); now_ms+=2800U; record_error=0; record_delay=20; media_step();
    assert(sends==1 && !finish_count);
    now_ms+=300U; record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); assert(!finish_count && sends==1); ++cases;
    /* Driver faults and invalid leases are terminal without timeout retries. */
    for(i=0;i<2U;++i) {
        reset(); record_error=i ? DEMO_AI_AUDIO_ERR_INVALID_LEASE : -41;
        media_step(); assert(finish_count==1 && finish_error==AI_ERR_AUDIO && !sends); ++cases;
    }
    /* Mute, cancellation, off/on, network loss and route invalidation can
     * arrive while the real HAL waits. None may upload or keep the old budget. */
    for(i=1;i<=5U;++i) {
        reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
        media_step(); now_ms+=3000U; capture_hook=i; media_step();
        assert(!finish_count && !sends && !s_record_failure_active);
        if(i==1U) { s_ui.mic_enabled=true; capture_hook=0; media_step(); assert(!finish_count); }
        if(i==3U) { capture_hook=0; media_step(); assert(!finish_count); }
        ++cases;
    }
    /* A complete off/on between worker ticks also creates a new capture epoch. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); now_ms+=3000U;
    tirtc_ai_set_audio_config(8,7,true,false); tirtc_ai_set_audio_config(8,7,true,true);
    media_step(); assert(!finish_count && !sends); ++cases;
    /* Pausing a disabled microphone must not carry an old failure into unmute. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); s_ui.mic_enabled=false; media_step(); assert(s_capture_paused);
    now_ms+=4000U; s_ui.mic_enabled=true; media_step(); assert(!finish_count && !sends); ++cases;
    /* Interrupt rebuilds the codec without starting a new RTC attempt. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); now_ms+=3000U;
    command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(stop_calls==1 && prepare_calls==1 && !finish_count);
    media_step(); assert(!finish_count && !sends); ++cases;
    /* Entering the final playback tail retires capture failure state. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); begin_remote_tail(); assert(!s_record_failure_active);
    media_step(); assert(!finish_count && !sends && !captures); ++cases;
    /* Elapsed-time arithmetic must remain valid across uint32 clock wrap. */
    reset(); now_ms=UINT32_MAX-1000U; drain_until=now_ms;
    record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); first=now_ms;
    for(i=0;i<24U;++i) { media_step(); assert(!finish_count); }
    media_step(); assert((uint32_t)(now_ms-first)==3000U && finish_error==AI_ERR_AUDIO); ++cases;
#else
    /* The new timeout tolerance applies only to the AEC adapter. */
    reset(); record_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT; record_delay=120;
    media_step(); assert(finish_count==1 && finish_error==AI_ERR_AUDIO && !sends); ++cases;
    reset(); record_error=-41; media_step(); assert(finish_count==1 && !sends); ++cases;
#endif
}

#if DEMO_AI_AUDIO_AEC_ENABLE
static void interrupt_arrival_cases(void) {
    unsigned i;
    reset(); put(160,77); audio_on_stop=true;
    command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(stop_calls==1 && prepare_calls==1 && !hal_stopped && !finish_count);
    assert(queued_at_stop==0 && s_rx_queued==160 && now_ms==1200U && !played_samples);
    assert(s_rx[(s_rx_read+1U)%AI_RX_BYTES]==99U);
    media_step(); now_ms+=AI_PREROLL_WAIT_MS; media_step();
    assert(!s_rx_queued && played_samples==320 && !finish_count);
    for(i=0;i<320U;++i) assert(played_pcm[i]==99); /* cancelled 77 never replays */
    media_step(); assert(played_samples==320 && !finish_count); ++cases;
    reset(); put(160,77); audio_on_stop=cancel_on_stop=true;
    command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(stop_calls==1 && !prepare_calls && hal_stopped && finish_count==1 && !played_samples && !sends); ++cases;
}
static void tail_speaker_cases(void) {
    unsigned i;
    /* A completed tail may accept a later final packet after off/on. The
     * terminal session must still keep capture/uplink disabled. */
    reset(); begin_remote_tail();
    tirtc_ai_set_audio_config(8,7,false,true); media_step();
    tirtc_ai_set_audio_config(8,7,true,true); now_ms+=100U; media_step();
    assert(!s_speaker_draining && s_applied_speaker && !sends && !captures);
    put(160,88); assert(s_rx_queued==160);
    media_step(); now_ms+=AI_PREROLL_WAIT_MS; media_step();
    assert(played_samples==320 && !s_rx_queued && !sends && !captures && !finish_count);
    for(i=0;i<320U;++i) assert(played_pcm[i]==88); ++cases;
    /* Neither HAL done alone nor expiry of the app's guard alone is a
     * sufficient fence for restoring a previously muted speaker. */
    for(i=0;i<2U;++i) {
        reset(); begin_remote_tail(); s_play_guard=true;
        s_play_until=i ? now_ms+500U : now_ms-1U; done_stuck=!i;
        tirtc_ai_set_audio_config(8,7,false,true); media_step();
        tirtc_ai_set_audio_config(8,7,true,true); now_ms+=100U; media_step();
        assert(s_speaker_draining && s_play_guard && !s_applied_speaker && !sends && !captures);
        put(160,77); assert(!s_rx_queued); /* old DMA is still intentionally muted */
        done_stuck=false; now_ms+=500U; media_step();
        assert(!s_speaker_draining && !s_play_guard && !sends);
        media_step(); assert(s_applied_speaker && !sends && !captures && !finish_count);
        put(160,88); assert(s_rx_queued==160); ++cases;
    }
    /* A worker that still reports not-done cannot reopen the codec merely
     * because no application playback guard happened to be set. */
    reset(); begin_remote_tail(); done_stuck=true;
    tirtc_ai_set_audio_config(8,7,false,true); media_step();
    tirtc_ai_set_audio_config(8,7,true,true); media_step();
    assert(s_speaker_draining && !s_applied_speaker && !sends && !captures);
    now_ms+=AI_REMOTE_TAIL_STALL_MS; media_step();
    assert(finish_count==1 && !finish_error && !sends); ++cases;
}
static void tails(void) {
    uint32_t start; unsigned i;
    reset(); s_call_ticket=7; begin_remote_tail(); assert(cancelled==1 && !s_call_ticket);
    media_step(); assert(!sends && !finish_count); now_ms+=1199; media_step(); assert(!finish_count);
    now_ms++; media_step(); assert(finish_count==1 && !finish_error && !s_callbacks); ++cases;
    for(i=0;i<2;++i) {
        reset(); begin_remote_tail(); media_step(); now_ms+=i?900:250; put(160,55); media_step();
        now_ms+=120; media_step(); assert(played_samples==320 && !finish_count);
        now_ms+=30; media_step(); now_ms+=1199; media_step(); assert(!finish_count);
        now_ms++; media_step(); assert(finish_count==1 && !sends); ++cases;
    }
    reset(); bounded_queue=true; begin_remote_tail(); start=now_ms;
    for(i=0;i<250;++i) { put(160,(uint8_t)i); media_step(); now_ms+=20; assert(!finish_count); }
    assert(now_ms-start>3000 && played_samples && !sends);
    for(i=0;i<100 && !finish_count;++i) { media_step(); now_ms+=20; }
    assert(finish_count==1 && !s_rx_queued && played_samples==250U*320U); ++cases;
    reset(); sdk_busy=true; begin_remote_tail(); put(160,66); start=now_ms;
    for(i=0;i<300;++i) { media_step(); now_ms+=10; assert(!finish_count); }
    media_step(); assert(finish_count==1 && now_ms-start==3000 && s_rx_queued==160 && !sends); ++cases;
    reset(); done_stuck=true; begin_remote_tail(); media_step(); now_ms+=3000; media_step(); assert(finish_count==1); ++cases;
    reset(); begin_remote_tail(); command("{\"jsonrpc\":\"2.0\",\"method\":\"caption\",\"params\":{}}");
    command("{\"jsonrpc\":\"2.0\",\"id\":\"call\",\"method\":\"device_action\",\"params\":{}}");
    assert(captions==1 && !actions && !finish_count); ++cases;
    command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}"); assert(finish_count==1 && !stop_calls); ++cases;
    reset(); put(160,77); command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(stop_calls==1 && prepare_calls==1 && !hal_stopped && !s_rx_queued && !finish_count);
    media_step(); assert(sends==1 && !finish_count); ++cases;
    reset(); cancel_on_stop=true; command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(stop_calls==1 && !prepare_calls && hal_stopped && finish_count==1); ++cases;
    reset(); prepare_error=-41; command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(prepare_calls==1 && hal_stopped && finish_error==AI_ERR_AUDIO); ++cases;
    reset(); now_ms=0xfffffe00U; drain_until=now_ms; begin_remote_tail(); media_step(); now_ms+=1200; media_step(); assert(finish_count==1); ++cases;
}
#endif
static void decode_control_cases(void) {
    unsigned hook;
    for(hook=1U;hook<=6U;++hook) {
        reset(); decode_hook=hook; put(160,77);
        s_play_continuing=s_play_guard=true; s_play_audio_until=now_ms+100U;
        assert(!submit_playback(true,8));
        assert(decode_calls==1U && !played_samples && !warm_calls);
        assert(s_rx_queued==160U && s_decode_samples==320U && !s_decode_offset);
        /* The normal worker control path owns discard/reset, not the decoder. */
        discard_audio_queue(); assert(!s_rx_queued && !s_decode_samples);
        ++cases;
    }
}
static void lifecycle_cases(void) {
    unsigned i;
    reset(); disconnected(s_connection); step();
#if DEMO_AI_AUDIO_AEC_ENABLE
    assert(s_remote_tail && !finish_count && !sends && !lookup_steps);
#else
    assert(finish_count==1 && finish_error==AI_ERR_LOST);
#endif
    ++cases;
    reset(); connection_error(s_connection,-777); step(); assert(finish_count==1 && finish_error==-777 && !sends); ++cases;
#if DEMO_AI_AUDIO_AEC_ENABLE
    reset(); connection_error(s_connection,-777); disconnected(s_connection); step(); assert(finish_count==1 && finish_error==-777 && !s_remote_tail); ++cases;
    reset(); disconnected(s_connection); connection_error(s_connection,-888); step(); assert(finish_count==1 && finish_error==-888 && !s_remote_tail); ++cases;
    for(i=0;i<5;++i) {
        reset(); begin_remote_tail(); put(160,55);
        if(i==0) s_want=false;
        if(i==1) ++s_request;
        if(i==2) calls_busy=true;
        if(i==3) network_ready=false;
        if(i==4) route_valid=false;
        step(); assert(finish_count==1 && !played_samples && !sends && !lookup_steps); ++cases;
    }
    reset(); begin_remote_tail(); connection_error(s_connection,-888); step();
    assert(finish_count==1 && finish_error==-888); ++cases;
    reset(); {
        const char *end="{\"jsonrpc\":\"2.0\",\"method\":\"end_session\"}";
        s_commands[0].used=1; s_commands[0].epoch=s_running;
        s_commands[0].length=(uint16_t)strlen(end); strcpy(s_commands[0].data,end);
    }
    step(); assert(s_remote_tail && !finish_count && !sends && !lookup_steps && !s_commands[0].used); ++cases;
#else
    (void)i;
#endif
}
int main(void) {
    cJSON_Hooks hooks={count_alloc,count_free}; cJSON_InitHooks(&hooks);
#if defined(TEST_AI_DECODE_CONTROLS_ONLY)
    decode_control_cases();
#elif defined(TEST_AI_OPUS_PACKETS_ONLY)
    opus_packet_cases();
#elif defined(TEST_AI_UPLINK_BACKPRESSURE_ONLY)
    uplink_backpressure_cases();
#elif defined(TEST_AI_NEGOTIATION_AUDIO_ONLY)
    negotiation_audio_cases();
#elif defined(TEST_AI_CAPTURE_TIMEOUT_ONLY)
    capture_timeout_cases();
#elif defined(TEST_AI_INTERRUPT_ARRIVAL_ONLY)
#if DEMO_AI_AUDIO_AEC_ENABLE
    interrupt_arrival_cases();
#endif
#elif defined(TEST_AI_TAIL_SPEAKER_ONLY)
#if DEMO_AI_AUDIO_AEC_ENABLE
    tail_speaker_cases();
#endif
#else
    protocols(); media_cases(); opus_packet_cases(); capture_timeout_cases(); negotiation_audio_cases(); uplink_backpressure_cases();
#if DEMO_AI_AUDIO_AEC_ENABLE
    interrupt_arrival_cases(); tail_speaker_cases(); tails();
#else
    reset(); put(160,77); command("{\"jsonrpc\":\"2.0\",\"method\":\"interrupt\"}");
    assert(!stop_calls && !prepare_calls && s_rx_queued==160 && !finish_count); ++cases;
#endif
    decode_control_cases(); lifecycle_cases();
#endif
    assert(!allocations); cJSON_InitHooks(NULL);
    printf("PASS: Pro AI AEC=%d %u production cases; real cJSON/duration parser, fake Opus, packet FIFO, capture gates, tails\n",DEMO_AI_AUDIO_AEC_ENABLE,cases);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'tcc'))
    parser.add_argument('--cjson', type=Path, required=True)
    parser.add_argument('--source', type=Path, help='Alternative tirtc_ai.c for regression negative controls')
    parser.add_argument('--case', choices=('all', 'opus-packets', 'interrupt-arrival', 'tail-speaker', 'capture-timeout', 'negotiation-audio', 'uplink-backpressure', 'decode-controls'), default='all')
    args = parser.parse_args()
    cc = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
    if hashlib.sha256(args.cjson.read_bytes()).hexdigest() != CJSON_SHA256:
        parser.error('Expected unmodified upstream cJSON v1.7.16')
    app = Path(__file__).resolve().parents[1]
    sdk = app.parents[2]
    source = (args.source or app / 'ai/tirtc_ai.c').read_text(encoding='utf-8')
    protocol = (app / 'ai/ai_call_protocol.c').read_text(encoding='utf-8')
    codec = (app / 'media/opus_codec.c').read_text(encoding='utf-8')
    opus = sdk / 'components/thirdparty/opus/opus-1.4'
    opus_common = (opus / 'src/opus.c').read_text(encoding='utf-8')
    opus_decoder = (opus / 'src/opus_decoder.c').read_text(encoding='utf-8')
    constants = source[source.index('#define AI_CMD '):source.index('typedef enum')]
    names = ['pending', 'tirtc_ai_set_audio_config', 'discard_audio_queue', 'callback_current', 'connection_error', 'disconnected', 'audio_received', 'ai_command', 'command_received',
             'json_string', 'profile', 'command_notification', 'handle_command',
             'queued_audio_samples', 'consume_rx_packet', 'peek_audio', 'consume_audio', 'playback_buffer_ready',
             'submit_playback', 'account_capture_pause', 'pause_capture', 'media_step', 'prepare_audio', 'command_slot', 'step']
    bodies = function(opus_common, 'opus_packet_get_samples_per_frame') + '\n'
    bodies += function(opus_decoder, 'opus_packet_get_nb_frames') + '\n'
    bodies += function(opus_decoder, 'opus_packet_get_nb_samples') + '\n'
    bodies += function(codec, 'demo_ai_opus_packet_samples') + '\n'
    bodies += function(protocol, 'ai_call_json_complete') + '\n'
    bodies += '#if DEMO_AI_AUDIO_AEC_ENABLE\n' + function(source, 'begin_remote_tail') + '\n#endif\n'
    for name in names:
        bodies += function(source, name) + '\n'
        if name == 'submit_playback':
            bodies += '#if DEMO_AI_AUDIO_AEC_ENABLE\n' + function(source, 'remote_tail_step') + '\n#endif\n'
    with tempfile.TemporaryDirectory(prefix='pro-ai-aec-') as temp:
        c = Path(temp) / 'ai-media.c'
        c.write_text('#include "audio_device.h"\n' + constants + PREFIX + bodies + SUFFIX, encoding='utf-8')
        for enabled in (0, 1):
            exe = Path(temp) / ('ai-aec-%d.exe' % enabled)
            selected = [] if args.case == 'all' else ['-DTEST_AI_%s_ONLY=1' % args.case.upper().replace('-', '_')]
            subprocess.run([*cc, '-std=c99', '-DDEMO_AI_AUDIO_AEC_ENABLE=%d' % enabled, *selected,
                            '-DCJSON_HIDE_SYMBOLS', '-I' + str(app / 'media'), '-I' + str(app / 'platform'),
                            '-I' + str(sdk / 'components/thirdparty/CJSON'), '-I' + str(opus / 'include'), str(c), str(args.cjson),
                            '-o', str(exe), '-lm'], check=True)
            subprocess.run([str(exe)], check=True, timeout=20)


if __name__ == '__main__':
    main()
