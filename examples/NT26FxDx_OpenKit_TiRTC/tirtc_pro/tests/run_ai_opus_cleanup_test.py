#!/usr/bin/env python3
"""Check production AI cleanup/prepare ownership around Opus heap lifetime.

HAL and RTC are controlled fakes; production cleanup ordering is extracted
unchanged. Real encoder/decoder idempotence is covered by run_ai_opus_codec_test.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_ai_aec_media_test import function

PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "audio_device.h"
#define liot_trace(...) ((void)0)
#define DEMO_TIRTC_OWNER_AI 1U
enum {AI_IDLE,AI_STOPPING,AI_BLOCKED,AI_ERROR};
enum {TIRTC_AI_ERROR,TIRTC_AI_IDLE};
static int s_phase,s_finish_error,s_audio_cleanup_error;
static bool s_audio_owned,s_audio_started,s_runtime_owned,s_audio_cleanup_retry;
static bool s_restart_requested,s_background_cleanup,s_resume_after_cleanup;
static bool s_want,s_callbacks,s_whip_done,s_lost,s_play_guard,s_suppression_hint,s_prime_wait_hint;
static bool s_standby,s_access_ready,s_start_sent;
static uint32_t s_stopping_since,s_audio_cleanup_retry_at,s_generation,s_request,s_running;
static uint32_t s_play_audio_until,s_play_until,now_ms;
static void *s_connection;
static demo_ai_audio_lease_t s_lease;
static struct {bool ai_active,speaker_enabled,mic_enabled;uint8_t volume,mic_gain;} s_ui;
static bool codec_live;
static int stop_error,release_error,rtc_error,acquire_error,prepare_error;
static unsigned allocated,freed,stops,releases,rtc_releases,finishes,cases;
static char events[128];static unsigned event_count;
static void event(char c) {assert(event_count+1<sizeof(events));events[event_count++]=c;events[event_count]=0;}
static uint32_t tick(void) {return now_ms;}
static void liot_rtos_enter_critical(void) {}
static void liot_rtos_exit_critical(void) {}
static void show(int state,const char *text) {(void)state;(void)text;}
static void tirtc_status_event(const char *text) {(void)text;}
static void clear_caption(void) {}
static void text_copy(char *to,size_t cap,const char *from) {snprintf(to,cap,"%s",from);}
static void startup_stage(const char *a,uint32_t b,int c) {(void)a;(void)b;(void)c;}
static bool requested(void) {return s_want;}
static bool ready(void) {return true;}
static bool route_current(void) {return true;}
static void finish(int error) {++finishes;s_finish_error=error;s_phase=AI_STOPPING;}
static int demo_ai_opus_init(void) {event('I');if(!codec_live){codec_live=true;++allocated;}return 0;}
static void demo_ai_opus_reset(void) {assert(codec_live);event('E');}
static void demo_ai_opus_deinit(void) {event('D');if(codec_live){codec_live=false;++freed;}}
int demo_ai_audio_stop(const demo_ai_audio_lease_t *lease) {(void)lease;event('S');++stops;return stop_error;}
int demo_ai_audio_release(demo_ai_audio_lease_t *lease) {(void)lease;event('A');++releases;return release_error;}
int demo_ai_audio_acquire(demo_ai_audio_owner_e owner,demo_ai_audio_lease_t *lease) {(void)owner;(void)lease;event('Q');assert(codec_live);return acquire_error;}
int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *lease,uint8_t a,uint8_t b) {(void)lease;(void)a;(void)b;event('P');assert(codec_live);return prepare_error;}
int demo_ai_audio_prewarm_session(const demo_ai_audio_lease_t *lease,uint32_t *extra) {(void)lease;event('W');*extra=0;return 0;}
static int demo_tirtc_disconnect(unsigned owner,uint32_t generation) {(void)owner;(void)generation;assert(!codec_live);event('C');return 0;}
static int demo_tirtc_session_release(unsigned owner,uint32_t generation) {(void)owner;(void)generation;assert(!codec_live);event('R');++rtc_releases;return rtc_error;}
static void demo_tirtc_require_restart(int error) {(void)error;}
'''
SUFFIX = r'''
static void reset(void) {
    s_phase=AI_STOPPING;s_audio_owned=s_audio_started=s_runtime_owned=true;
    s_audio_cleanup_retry=s_restart_requested=s_background_cleanup=s_resume_after_cleanup=false;
    s_want=s_callbacks=s_whip_done=s_lost=s_play_guard=s_suppression_hint=s_prime_wait_hint=false;
    s_standby=s_access_ready=s_start_sent=false;s_finish_error=s_audio_cleanup_error=0;
    now_ms=s_stopping_since=1000;s_audio_cleanup_retry_at=0;s_generation=7;s_request=s_running=8;
    s_connection=(void *)1;memset(&s_ui,0,sizeof(s_ui));s_ui.volume=s_ui.mic_gain=8;
    s_ui.speaker_enabled=s_ui.mic_enabled=true;s_play_audio_until=s_play_until=0;
    codec_live=true;allocated=1;freed=stops=releases=rtc_releases=finishes=event_count=0;events[0]=0;
    stop_error=release_error=rtc_error=acquire_error=prepare_error=0;
}
int main(void) {
    reset();stop_error=-42;cleanup_step();
    assert(codec_live && !freed && !releases && !rtc_releases && s_audio_owned);
    now_ms+=999;cleanup_step();assert(stops==1 && codec_live);
    now_ms++;stop_error=0;cleanup_step();
    assert(strcmp(events,"SSADCR")==0 && freed==1 && !s_runtime_owned && !s_audio_owned);++cases;
    reset();release_error=-42;cleanup_step();
    assert(codec_live && !freed && !rtc_releases && s_audio_owned);
    now_ms+=1000;release_error=0;cleanup_step();
    assert(strcmp(events,"SASADCR")==0 && freed==1 && !codec_live);++cases;
    reset();s_phase=AI_BLOCKED;cleanup_step();assert(codec_live && !freed && !stops && !rtc_releases);++cases;
    reset();stop_error=-42;now_ms+=AI_AUDIO_CLEANUP_LIMIT_MS;cleanup_step();
    assert(s_phase==AI_BLOCKED && codec_live && !freed && !rtc_releases);++cases;
    reset();rtc_error=-42;cleanup_step();
    assert(strcmp(events,"SADCR")==0 && !codec_live && freed==1 && s_runtime_owned && !s_audio_owned);
    cleanup_step();assert(strcmp(events,"SADCRDCR")==0 && freed==1 && stops==1 && releases==1);
    rtc_error=0;cleanup_step();assert(!s_runtime_owned && freed==1 && s_phase==AI_IDLE);
    cleanup_step();assert(freed==1 && stops==1 && releases==1);++cases;
    s_want=true;assert(prepare_audio());assert(codec_live && allocated==2 && s_audio_owned && s_audio_started);
    s_phase=AI_STOPPING;s_want=false;cleanup_step();assert(!codec_live && freed==2);++cases;
    reset();cleanup_step();s_want=true;acquire_error=-1;
    assert(!prepare_audio() && codec_live && !s_audio_owned && finishes==1);
    cleanup_step();assert(!codec_live && freed==2 && !s_audio_owned);++cases;
    reset();cleanup_step();s_want=true;prepare_error=-1;
    assert(!prepare_audio() && codec_live && s_audio_owned && finishes==1);
    cleanup_step();assert(!codec_live && freed==2 && !s_audio_owned);++cases;
    printf("PASS: Pro AI AEC=%d Opus cleanup: %u production cases, HAL completion, codec-before-RTC release, idempotence, reinit\n",DEMO_AI_AUDIO_AEC_ENABLE,cases);
    return 0;
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--source', type=Path, help='Alternative production source for negative controls')
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    source = (args.source or app / 'ai/tirtc_ai.c').read_text(encoding='utf-8')
    constants = source[source.index('#define AI_CMD '):source.index('typedef enum')]
    functions = '\n'.join(function(source, n) for n in ('pending', 'audio_cleanup_blocked', 'audio_cleanup_failed', 'cleanup_step', 'prepare_audio'))
    cc = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
    with tempfile.TemporaryDirectory(prefix='pro-opus-cleanup-') as temp:
        temp = Path(temp)
        unit = temp / 'test.c'
        unit.write_text('#include "audio_device.h"\n' + constants + PREFIX + functions + SUFFIX, encoding='utf-8')
        for enabled in (0, 1):
            exe = temp / ('test-%d.exe' % enabled)
            subprocess.run([*cc, '-std=c99', '-DDEMO_AI_AUDIO_AEC_ENABLE=%d' % enabled,
                            '-I' + str(app / 'media'), str(unit), '-o', str(exe)], check=True, timeout=60)
            subprocess.run([str(exe)], check=True, timeout=20)

if __name__ == '__main__':
    main()
