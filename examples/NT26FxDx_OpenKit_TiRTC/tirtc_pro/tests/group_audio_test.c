/* SPDX-License-Identifier: MIT
 * Playback contract tests using the real Room worker and existing host fakes.
 * Network packet boundaries must not change the decoded sample stream.
 */
#define main group_backend_test_main
#include "group_backend_test.c"
#undef main

static const uint8_t audio_codes[]={0xd5,0x55,0xaa,0x2a};
static void receive_sequence(uint32_t offset,uint32_t count) {
    TIRTCFRAMEINFO frame;uint8_t data[640];uint32_t i;
    CHECK(count<=sizeof(data));
    for(i=0;i<count;i++)data[i]=audio_codes[(offset+i)%4U];
    memset(&frame,0,sizeof(frame));frame.stream_id=1U;
    frame.media=TIRTC_AUDIO_ALAW;frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=count;
    room_on_audio(s_wire_connection,&frame,data);
}
static void check_sequence(uint32_t count) {
    uint32_t i;CHECK(played_count==count*2U);
    for(i=0;i<count;i++) {
        int16_t sample=demo_g711_alaw_decode_sample(audio_codes[i%4U]);
        CHECK(played[i*2U]==sample && played[i*2U+1U]==sample);
    }
}
static void continuing(void) {
    s_play_continuing=true;s_play_guard=true;s_play_tail=clock_ms+80U;
    playback_done=false;
}
static void test_packet_boundaries(void) {
    continuing();receive_sequence(0,159);receive_sequence(159,1);
    room_audio_tick();check_sequence(160);CHECK(s_rx_count==0U);CHECK(warm_plays==0);
}
static void test_batch_refill(void) {
    warm_extra_ms=80U;receive_sequence(0,640);room_audio_tick();
    check_sequence(640);CHECK(plays==4);CHECK(warm_plays==1);
    CHECK(s_play_tail-clock_ms==160U);CHECK(s_rx_count==0U);
}
static void test_unfinished_playback(void) {
    continuing();s_play_tail=clock_ms-1U;receive_sequence(0,160);
    room_audio_tick();check_sequence(160);CHECK(warm_plays==0);
}
static void test_format_filter(void) {
    TIRTCFRAMEINFO frame;uint8_t data[160];
    memset(data,0xd5,sizeof(data));memset(&frame,0,sizeof(frame));
    frame.stream_id=1U;frame.media=TIRTC_AUDIO_ALAW;
    frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=sizeof(data);
    frame.flags=TIRTC_AUDIOSAMPLE_16K16B1C;room_on_audio(s_wire_connection,&frame,data);
    frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.media=TIRTC_AUDIO_PCM;
    room_on_audio(s_wire_connection,&frame,data);
    frame.media=TIRTC_AUDIO_ALAW;frame.stream_id=2U;room_on_audio(s_wire_connection,&frame,data);
    frame.stream_id=1U;room_on_audio((void *)0x5678,&frame,data);
    room_audio_tick();CHECK(!s_rx_count && !s_rx_bytes && !plays && !records);
}
static void test_initial_prebuffer(void) {
    warm_extra_ms=80U;receive_sequence(0,160);room_audio_tick();CHECK(!plays);
    clock_ms+=79U;room_audio_tick();CHECK(!plays);
    ++clock_ms;room_audio_tick();check_sequence(160);CHECK(warm_plays==1);
    CHECK(s_play_tail-clock_ms==100U);CHECK(!records);
}
static void test_odd_tail_wait(void) {
    continuing();receive_sequence(0,1);room_audio_tick();CHECK(!plays);
    clock_ms+=79U;room_audio_tick();CHECK(!plays);
    ++clock_ms;room_audio_tick();CHECK(played_count==2U);
    CHECK(played[0]==8 && played[1]==8);CHECK(!s_rx_count && !s_rx_bytes);
    CHECK(!warm_plays);
}
static void test_odd_tail_joins_next_packet(void) {
    continuing();receive_sequence(0,1);room_audio_tick();CHECK(!plays);
    clock_ms+=79U;receive_sequence(1,1);room_audio_tick();check_sequence(2);
    CHECK(!s_rx_count && !s_rx_bytes && !warm_plays);
}
static void test_jitter_continuity(void) {
    const uint32_t arrivals[]={20U,20U,40U,10U,30U,20U,20U,20U};uint32_t i;
    warm_extra_ms=80U;receive_sequence(0,640);room_audio_tick();playback_done=false;
    CHECK(warm_plays==1);CHECK(played_count==1280U);
    for(i=0;i<sizeof(arrivals)/sizeof(arrivals[0]);i++) {
        clock_ms+=arrivals[i];receive_sequence(640U+160U*i,160U);room_audio_tick();
        CHECK(warm_plays==1);CHECK(!s_rx_count);CHECK(!records);
    }
    check_sequence(1920U);CHECK(s_play_tail-clock_ms==140U);
}
static void test_new_burst(void) {
    warm_extra_ms=80U;receive_sequence(0,640);room_audio_tick();CHECK(warm_plays==1);
    clock_ms=s_play_tail+1U;playback_done=true;
    receive_sequence(640,160);room_audio_tick();CHECK(played_count==1280U);
    clock_ms+=80U;room_audio_tick();check_sequence(800U);CHECK(warm_plays==2);
    CHECK(s_play_tail-clock_ms==100U);
}
static void test_fractional_time(void) {
    uint32_t i,tail;continuing();tail=s_play_tail;
    for(i=0;i<4;i++){receive_sequence(i*6U,6U);room_audio_tick();}
    check_sequence(24U);CHECK(s_play_tail-tail==3U);CHECK(!warm_plays);
}
static void test_busy_without_replay(void) {
    continuing();receive_sequence(0,159);receive_sequence(159,1);playback_busy=1;
    room_audio_tick();CHECK(!played_count && !plays && !warm_plays);
    CHECK(s_rx_bytes==160U && !s_play_offset);
    playback_busy=0;room_audio_tick();check_sequence(160U);CHECK(!s_rx_count);
    receive_sequence(160,160);room_audio_tick();check_sequence(320U);CHECK(!warm_plays);
}
static void test_play_ahead_bound(void) {
    uint32_t i;warm_extra_ms=80U;
    for(i=0;i<4;i++)receive_sequence(i*640U,640U);
    room_audio_tick();CHECK(played_count>=1280U && played_count<=2560U);
    CHECK(s_rx_count>0U);CHECK(s_play_tail-clock_ms<=240U);playback_done=false;
    for(i=0;i<32U && s_rx_count;i++) {
        clock_ms+=20U;room_audio_tick();CHECK(s_play_tail-clock_ms<=240U);
    }
    CHECK(!s_rx_count);check_sequence(2560U);CHECK(warm_plays==1);CHECK(!records);
}
static void test_teardown_pending_audio(void) {
    continuing();s_claimed=true;receive_sequence(0,640);playback_busy=1;room_audio_tick();
    CHECK(s_rx_count>0U && !plays);room_close(DEMO_GROUP_SUSPENDED,0,"suspended");
    CHECK(room_cleanup());CHECK(!s_rx_count && !s_rx_bytes && !s_play_offset);
    CHECK(!s_play_guard && !s_play_continuing && !s_audio.generation);
    receive_sequence(640,160);room_audio_tick();CHECK(!s_rx_count && !plays && !records);
    CHECK(audio_release_order>0 && runtime_release_order>audio_release_order);
}
static void test_clock_wrap(void) {
    clock_ms=UINT32_MAX-50U;continuing();receive_sequence(0,640);room_audio_tick();
    check_sequence(640U);CHECK(s_play_tail-clock_ms==160U);CHECK(!warm_plays);
}
static void test_expired_partial_refresh(void) {
    continuing();receive_sequence(0,1);room_audio_tick();CHECK(!plays);
    clock_ms+=ROOM_RX_MAX_AGE+1U;receive_sequence(1,1);room_audio_tick();
    /* The old byte expires. Its wait cannot authorize padding the fresh byte. */
    CHECK(s_snapshot.rx_dropped==1U);CHECK(!plays && s_rx_count==1U);
    clock_ms+=79U;room_audio_tick();CHECK(!plays);
    ++clock_ms;room_audio_tick();CHECK(played_count==2U);
    CHECK(played[0]==-8 && played[1]==-8);CHECK(!s_rx_count && !s_rx_bytes);
    CHECK(!warm_plays && !records);
}
static void test_diagnostic_windows(void) {
#if ROOM_AUDIO_DIAGNOSTICS
    TIRTCFRAMEINFO frame;uint8_t data[641];uint32_t i;
    room_audio_diag_begin();continuing();room_audio_diag_report(false);CHECK(!audio_diag_lines);
    memset(data,0xd5,sizeof(data));memset(&frame,0,sizeof(frame));
    frame.stream_id=ROOM_STREAM;frame.media=TIRTC_AUDIO_PCM;
    frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=641U;frame.ts=100U;
    room_on_audio(s_wire_connection,&frame,data);
    room_on_audio((void *)0x5678,&frame,data);
    CHECK(s_audio_diag.window.packets==1U && s_audio_diag.window.bytes==641U);
    CHECK(s_audio_diag.window.bad_format==1U && s_audio_diag.window.oversized==1U);
    frame.media=TIRTC_AUDIO_ALAW;frame.length=160U;
    clock_ms+=20U;room_on_audio(s_wire_connection,&frame,data);
    clock_ms+=40U;frame.ts=80U;room_on_audio(s_wire_connection,&frame,data);
    frame.stream_id=2U;room_on_audio(s_wire_connection,&frame,data);frame.stream_id=ROOM_STREAM;
    CHECK(s_audio_diag.window.observed_bad_format);
    CHECK(s_audio_diag.window.observed_stream==ROOM_STREAM &&
          s_audio_diag.window.observed_media==TIRTC_AUDIO_PCM &&
          s_audio_diag.window.observed_flags==TIRTC_AUDIOSAMPLE_8K16B1C);
    CHECK(s_audio_diag.window.timestamp_repeat==1U && s_audio_diag.window.timestamp_back==1U);
    CHECK(s_audio_diag.window.callback_gap==40U);
    CHECK(s_audio_diag.window.length_min==160U && s_audio_diag.window.length_max==641U);
    playback_block_ms=3U;room_audio_tick();
    CHECK(s_audio_diag.window.pcm_samples==640U && s_audio_diag.window.plays==2U);
    CHECK(s_audio_diag.window.tick_work==6U && !s_audio_diag.window.warm);
    clock_ms+=30U;frame.ts=120U;room_on_audio(s_wire_connection,&frame,data);
    playback_busy=1;room_audio_tick();CHECK(s_audio_diag.window.busy==1U);
    CHECK(s_audio_diag.window.tick_gap==36U && s_audio_diag.window.pcm_samples==640U);
    playback_busy=0;room_audio_tick();CHECK(s_audio_diag.window.pcm_samples==960U);
    playback_block_ms=0U;
    for(i=0U;i<17U;++i)room_on_audio(s_wire_connection,&frame,data);
    CHECK(s_audio_diag.window.drop_slot==1U && s_audio_diag.window.peak_slots==16U);
    clock_ms+=ROOM_RX_MAX_AGE+1U;room_audio_tick();CHECK(s_audio_diag.window.drop_age==16U);
    frame.length=640U;for(i=0U;i<7U;++i)room_on_audio(s_wire_connection,&frame,data);
    CHECK(s_audio_diag.window.drop_bytes==1U && s_audio_diag.window.peak_bytes==3840U);
    clock_ms+=ROOM_RX_MAX_AGE+1U;room_audio_tick();CHECK(s_audio_diag.window.drop_age==22U);
    CHECK(!audio_diag_lines);clock_ms=s_audio_diag.window_at+5000U;room_audio_diag_report(false);
    CHECK(audio_diag_lines==0U && !s_audio_diag.window.packets);
    room_audio_diag_report(false);CHECK(audio_diag_lines==0U);
    frame.length=160U;room_on_audio(s_wire_connection,&frame,data);
    CHECK(s_audio_diag.window.packets==1U && s_audio_diag.window.timestamp_repeat==1U);
    CHECK(!s_audio_diag.window.observed_bad_format &&
          s_audio_diag.window.observed_media==TIRTC_AUDIO_ALAW);
    frame.stream_id=2U;room_on_audio(s_wire_connection,&frame,data);frame.stream_id=ROOM_STREAM;
    CHECK(s_audio_diag.window.observed_bad_format && s_audio_diag.window.observed_stream==2U);
    s_claimed=true;room_close(DEMO_GROUP_SUSPENDED,0,"suspended");CHECK(room_cleanup());
    CHECK(audio_diag_lines==0U && !s_audio_diag.active && !s_audio_diag.window.packets);
    room_audio_diag_begin();s_wire_connection=(void *)0x1234;ready_audio();
    room_on_audio(s_wire_connection,&frame,data);
    CHECK(s_audio_diag.window.packets==1U && !s_audio_diag.window.timestamp_repeat);
    CHECK(!s_audio_diag.window.observed_bad_format && s_audio_diag.window.observed_stream==ROOM_STREAM);
#endif
}
#if DEMO_AI_AUDIO_AEC_ENABLE
static void test_aec_backpressure(void) {
    uint32_t i;continuing();room_audio_diag_begin();
    CHECK(ptt(1,s_ui_generation)==0);
    receive_sequence(0,159);receive_sequence(159,1);
    playback_busy=DEMO_AI_AUDIO_ERR_PLAY_BUSY;
    for(i=0;i<20U;i++) {
        room_audio_tick();CHECK(!s_stopping && !played_count);
        CHECK(s_rx_bytes==160U && !s_play_offset);
        clock_ms+=20U;
    }
    CHECK(audio_sends==20 && records==20);
#if ROOM_AUDIO_DIAGNOSTICS
    CHECK(s_audio_diag.window.busy==20U && !s_audio_diag.window.errors);
#endif
    CHECK(ptt(0,s_ui_generation)==0);
    playback_busy=0;room_audio_tick();check_sequence(160);
    CHECK(audio_sends==20 && records==20 && !s_rx_count);
    room_audio_tick();check_sequence(160);
}
static void test_aec_zero_warmup(void) {
    warm_extra_ms=0;receive_sequence(0,640);room_audio_tick();
    check_sequence(640);CHECK(warm_plays==1 && !records);
    CHECK(s_play_tail-clock_ms==80U && !s_rx_count);
}
static void test_aec_real_error(void) {
    continuing();receive_sequence(0,160);playback_busy=-43;
    room_audio_tick();CHECK(s_stopping && !plays && !records);
}
#endif
int main(int argc,char **argv) {
    CHECK(argc==2);setup();ready_audio();
    if(!strcmp(argv[1],"packet_boundaries"))test_packet_boundaries();
    else if(!strcmp(argv[1],"batch_refill"))test_batch_refill();
    else if(!strcmp(argv[1],"unfinished_playback"))test_unfinished_playback();
    else if(!strcmp(argv[1],"format_filter"))test_format_filter();
    else if(!strcmp(argv[1],"initial_prebuffer"))test_initial_prebuffer();
    else if(!strcmp(argv[1],"odd_tail_wait"))test_odd_tail_wait();
    else if(!strcmp(argv[1],"odd_tail_joins_next_packet"))test_odd_tail_joins_next_packet();
    else if(!strcmp(argv[1],"jitter_continuity"))test_jitter_continuity();
    else if(!strcmp(argv[1],"new_burst"))test_new_burst();
    else if(!strcmp(argv[1],"fractional_time"))test_fractional_time();
    else if(!strcmp(argv[1],"busy_without_replay"))test_busy_without_replay();
    else if(!strcmp(argv[1],"play_ahead_bound"))test_play_ahead_bound();
    else if(!strcmp(argv[1],"teardown_pending_audio"))test_teardown_pending_audio();
    else if(!strcmp(argv[1],"clock_wrap"))test_clock_wrap();
    else if(!strcmp(argv[1],"expired_partial_refresh"))test_expired_partial_refresh();
    else if(!strcmp(argv[1],"diagnostic_windows"))test_diagnostic_windows();
#if DEMO_AI_AUDIO_AEC_ENABLE
    else if(!strcmp(argv[1],"aec_backpressure"))test_aec_backpressure();
    else if(!strcmp(argv[1],"aec_zero_warmup"))test_aec_zero_warmup();
    else if(!strcmp(argv[1],"aec_real_error"))test_aec_real_error();
#endif
    else CHECK(0);
    CHECK(critical_depth==0);printf("PASS audio %s\n",argv[1]);return 0;
}
