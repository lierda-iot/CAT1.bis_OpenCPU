/* SPDX-License-Identifier: MIT
 * Tests compile the production worker verbatim. Only its device/network/RTOS
 * boundaries are faked; cJSON and the A-law codec are their real implementations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../group/tirtc_group.c"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)

static uint32_t clock_ms=1000U;
static int init_stage,init_fail_stage,init_failures,sem_created,http_created,room_created,handler_registered,room_publications;
static tirtc_ui_room_t last_room;
static int init_fail(void) { return ++init_stage==init_fail_stage && ++init_failures; }
static int critical_depth, audio_sends, records, discards, plays, disconnects, releases;
static int page_requests, http_calls, capture_interrupt, playback_busy;
static int capture_error;
static int warm_plays;
static uint32_t warm_extra_ms, capture_block_ms, playback_block_ms;
static uint32_t prewarm_extra_ms, prewarm_calls, join_commands;
static unsigned int audio_diag_lines;
static bool playback_done=true;
static int audio_release_busy, release_events, audio_release_order, runtime_release_order;
static int configured_speaker=-1, configured_mic=-1;
static int set_levels_ret;
static int response_status=200, response_ret, send_ret=160;
static uint32_t current_credential_epoch=1U;
static bool context_change_on_prewarm;
static unsigned int whip_submissions;
static bool context_valid=true, network_ready=true, calls_active, ai_active, shown_valid=true;
static tirtc_ui_page_t shown_page=TIRTC_PAGE_ROOM;
static demo_binding_state_e binding_state=DEMO_BIND_BOUND;
static char response_json[4096], last_path[128], last_body[4096], last_command[4096];
static TIRTCFRAMEINFO sent_frame;
static uint8_t sent_audio[160];
static int16_t played[8192];
static uint32_t played_count, play_sizes[64], requested_generation;
static size_t send_buffer_used;

void liot_rtos_enter_critical(void) { ++critical_depth; }
void liot_rtos_exit_critical(void) { CHECK(critical_depth>0); --critical_depth; }
uint32_t liot_rtos_get_running_time(void) { return clock_ms; }
int liot_rtos_semaphore_create(liot_sem_t *s,uint32_t n) {(void)n;if(init_fail())return -1;*s=(void *)(uintptr_t)++sem_created;return 0;}
int liot_rtos_semaphore_release(liot_sem_t s) {(void)s;return 0;}
int liot_rtos_semaphore_wait(liot_sem_t s,uint32_t n) {(void)s;(void)n;return 0;}
int liot_rtos_task_create(liot_task_t *t,uint32_t n,uint32_t p,const char *name,
                         void (*entry)(void *),void *arg) {
    (void)n;(void)p;(void)entry;(void)arg;if(init_fail())return -1;if(!strcmp(name,"room_http"))++http_created;else if(!strcmp(name,"group_room"))++room_created;*t=(void *)1;return 0;
}
uint32_t liot_true_rand(void) { return 0x12345678U; }
void liot_trace(const char *fmt,...) {
    if (!strncmp(fmt,"[ROOM-AUDIO51]",14U)) {CHECK(critical_depth==0);++audio_diag_lines;}
}
bool tirtc_calls_has_session(void) {return calls_active;}
bool tirtc_ai_has_session(void) {return ai_active;}
bool tirtc_remote_has_session(void) {return false;}
bool tirtc_ui_get_rendered_page(tirtc_ui_page_t *p) {*p=shown_page;return shown_valid;}
int tirtc_ui_request_page(tirtc_ui_page_t p) {(void)p;++page_requests;return 0;}
int tirtc_ui_publish_room(const tirtc_ui_room_t *r) {last_room=*r;++room_publications;return 0;}
bool tirtc_network_is_ready(void) {return network_ready;}
bool demo_formal_mqtt_is_online(void) {return network_ready;}
int demo_formal_mqtt_register_handler(demo_formal_mqtt_message_handler_t h,void *u) {(void)h;(void)u;if(init_fail())return -1;++handler_registered;return 0;}
void demo_formal_mqtt_unregister_handler(demo_formal_mqtt_message_handler_t h,void *u) {(void)h;(void)u;}
void demo_binding_get_snapshot(demo_binding_snapshot_t *p) {memset(p,0,sizeof(*p));p->state=binding_state;}
bool demo_binding_service_context(demo_binding_service_context_t *p) {
    memset(p,0,sizeof(*p));p->credential_epoch=current_credential_epoch;return context_valid;
}
bool demo_binding_service_context_current(const demo_binding_service_context_t *p) {
    return context_valid && p->credential_epoch==current_credential_epoch;
}
int demo_binding_get_tirtc_identity(demo_binding_tirtc_identity_t *p) {
    memset(p,0,sizeof(*p));strcpy(p->device_id,"device-a");return 0;
}
int demo_binding_service_request_guarded(demo_binding_service_e service,
    demo_binding_http_method_e method,const char *path,const char *body,
    char *response,size_t capacity,int *status,uint32_t timeout,
    const demo_binding_service_context_t *expected) {
    CHECK(service==DEMO_BIND_SERVICE_CALL);CHECK(expected->credential_epoch==current_credential_epoch);
    CHECK(timeout==ROOM_HTTP_TIMEOUT);CHECK(critical_depth==0);
    CHECK(method==DEMO_BIND_HTTP_GET || method==DEMO_BIND_HTTP_POST);
    ++http_calls;snprintf(last_path,sizeof(last_path),"%s",path);
    snprintf(last_body,sizeof(last_body),"%s",body?body:"");
    snprintf(response,capacity,"%s",response_json);*status=response_status;return response_ret;
}
void demo_tirtc_set_feature_listener(demo_tirtc_feature_e f,const demo_tirtc_listener_t *l) {(void)f;(void)l;}
bool demo_tirtc_is_ready(void) {return network_ready;}
void demo_tirtc_get_connection_snapshot(demo_tirtc_connection_snapshot_t *p) {memset(p,0,sizeof(*p));}
int demo_tirtc_session_claim(demo_tirtc_owner_e owner,uint32_t *g) {
    CHECK(owner==DEMO_TIRTC_OWNER_GROUP_ROOM);*g=++requested_generation;return 0;
}
int demo_tirtc_session_release(demo_tirtc_owner_e owner,uint32_t g) {
    (void)owner;(void)g;++releases;runtime_release_order=++release_events;return 0;
}
int demo_tirtc_disconnect(demo_tirtc_owner_e owner,uint32_t g) {(void)owner;(void)g;++disconnects;return 0;}
int demo_tirtc_whip_connect(demo_tirtc_owner_e owner,uint32_t g,const char *peer,
    const char *token,TIRTCCONNECTCALLBACK cb,void *u) {
    (void)owner;(void)g;(void)peer;(void)token;(void)cb;(void)u;++whip_submissions;return 0;
}
int demo_tirtc_subscribe_audio(demo_tirtc_owner_e owner,uint32_t g,uint8_t stream) {
    (void)owner;(void)g;CHECK(stream==1U);return 0;
}
int demo_tirtc_send_command(demo_tirtc_owner_e owner,uint32_t g,uint32_t command,
    const void *data,uint32_t length) {
    (void)owner;(void)g;CHECK(command==0x2200U);CHECK(length<sizeof(last_command));
    memcpy(last_command,data,length);last_command[length]=0;
    if(strstr(last_command,"\"method\":\"join_room\""))++join_commands;
    return (int)length;
}
int demo_tirtc_get_send_buffer_used(demo_tirtc_owner_e owner,uint32_t g,size_t *used) {
    (void)owner;(void)g;*used=send_buffer_used;return 0;
}
int demo_tirtc_send_audio(demo_tirtc_owner_e owner,uint32_t g,const TIRTCFRAMEINFO *f,const void *data) {
    CHECK(owner==DEMO_TIRTC_OWNER_GROUP_ROOM);CHECK(g==s_generation);CHECK(critical_depth==0);
    CHECK(f->length==160U);sent_frame=*f;memcpy(sent_audio,data,160U);++audio_sends;return send_ret;
}
bool demo_ai_audio_lease_is_valid(const demo_ai_audio_lease_t *l) {return l->generation!=0U;}
int demo_ai_audio_acquire(demo_ai_audio_owner_e o,demo_ai_audio_lease_t *l) {l->owner=o;l->generation=1U;return 0;}
int demo_ai_audio_release(demo_ai_audio_lease_t *l) {
    if(audio_release_busy)return DEMO_AI_AUDIO_ERR_BUSY;
    l->generation=0U;audio_release_order=++release_events;return 0;
}
int demo_ai_audio_prepare_session(const demo_ai_audio_lease_t *l,uint8_t s,uint8_t m) {(void)l;(void)s;(void)m;return 0;}
int demo_ai_audio_prewarm_session(const demo_ai_audio_lease_t *l,uint32_t *extra) {
    (void)l;++prewarm_calls;*extra=prewarm_extra_ms;prewarm_extra_ms=0U;
    if(context_change_on_prewarm)++current_credential_epoch;return 0;
}
int demo_ai_audio_set_levels(const demo_ai_audio_lease_t *l,uint8_t s,uint8_t m) {
    (void)l;configured_speaker=s;configured_mic=m;return set_levels_ret;
}
int demo_ai_audio_stop(const demo_ai_audio_lease_t *l) {(void)l;return 0;}
int demo_ai_audio_discard_capture(const demo_ai_audio_lease_t *l) {(void)l;++discards;return 0;}
static int ptt(int on,uint32_t generation) {
    tirtc_ui_action_t a;memset(&a,0,sizeof(a));a.type=TIRTC_ACTION_ROOM_PTT;a.value=on;
    strcpy(a.text,s_assignment.room_code);snprintf(a.extra,sizeof(a.extra),"%u",generation);
    return tirtc_group_action(&a);
}
int demo_ai_audio_record_20ms(const demo_ai_audio_lease_t *l,int16_t pcm[320]) {
    unsigned int i;(void)l;CHECK(critical_depth==0);++records;
    clock_ms+=capture_block_ms;
    for(i=0U;i<320U;++i)pcm[i]=(i%3U)==0U?0:(i%3U)==1U?32767:-32768;
    if(capture_interrupt==1) CHECK(ptt(0,s_ui_generation)==0);
    if(capture_interrupt==2) tirtc_group_suspend();
    if(capture_interrupt==3) clock_ms+=750U;
    if(capture_interrupt==4) {
        CHECK(ptt(0,s_ui_generation)==0);CHECK(ptt(1,s_ui_generation)==0);
    }
    if(capture_interrupt==5) tirtc_group_set_audio_config(8,7,true,false);
    return capture_error;
}
int demo_ai_audio_play(const demo_ai_audio_lease_t *l,const int16_t *pcm,uint32_t samples) {
    (void)l;CHECK(critical_depth==0);CHECK(samples>=2U && samples<=320U && !(samples&1U));
    CHECK(played_count+samples<=sizeof(played)/sizeof(played[0]));CHECK(plays<64);
    if(playback_busy)return playback_busy==1 ? DEMO_AI_AUDIO_ERR_BUSY : playback_busy;
    memcpy(played+played_count,pcm,samples*sizeof(*pcm));played_count+=samples;
    play_sizes[plays++]=samples;clock_ms+=playback_block_ms;return 0;
}
int demo_ai_audio_play_warm(const demo_ai_audio_lease_t *l,const int16_t *pcm,
    uint32_t samples,uint32_t *extra) {
    int result=demo_ai_audio_play(l,pcm,samples);*extra=0U;
    if(!result){++warm_plays;*extra=warm_extra_ms;}
    return result;
}
bool demo_ai_audio_play_done(const demo_ai_audio_lease_t *l) {(void)l;return playback_done;}

static void setup(void) {
    tirtc_ui_action_t action;
    s_initialized=true;s_enabled=true;s_foreground=true;s_media_idle=false;
    memset(&action,0,sizeof(action));action.type=TIRTC_ACTION_ROOM_OPEN;action.value=1;
    CHECK(tirtc_group_action(&action)==0);
    s_intent=s_seen_intent=3U;s_epoch=4U;s_generation=5U;s_ui_generation=6U;
    s_assignment.version=10U;s_assignment.desired=true;
    strcpy(s_assignment.room_id,"room-a");strcpy(s_assignment.room_code,"001234");
    strcpy(s_session,"session-old");strcpy(s_device,"device-a");
    s_wire_connection=(void *)0x1234;s_lease_deadline=clock_ms+60000U;
    s_session_context.credential_epoch=1U;s_join_id=7U;s_session_started=clock_ms;
    s_audio.owner=DEMO_AI_AUDIO_OWNER_GROUP_ROOM;s_audio.generation=1U;
    s_heartbeat_at=clock_ms+10000U;s_sync_at=clock_ms+60000U;
    s_transport_was_ready=true;s_assignment_known=true;
}
static void ready_audio(void) {s_joined=true;s_accept_audio=true;s_accept_commands=true;}
static void signal_json(const char *json) {
    cJSON *root=room_parse_json(json,strlen(json));CHECK(root!=NULL);
    room_signal_json(root);cJSON_Delete(root);
}
static const char *assignment_json=
    "{\"code\":200,\"data\":{\"assignment_version\":11,\"desired_state\":\"joined\","
    "\"room_id\":\"room-b\",\"room_code\":\"000007\"}}";
static const char *ack_json=
    "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"session_id\":\"media-a\","
    "\"room_id\":\"tenant:room-a\",\"input_audio\":{\"codec\":\"g711a\",\"sample_rate\":8000,\"channels\":1},"
    "\"output_audio\":{\"codec\":\"g711a\",\"sample_rate\":8000,\"channels\":1}}}";
static void execute(room_job_kind_e kind,const char *json) {
    memset(&s_http_job,0,sizeof(s_http_job));s_http_job.kind=kind;
    s_http_job.intent=s_seen_intent;s_http_job.epoch=s_epoch;
    s_http_job.timeout_ms=ROOM_HTTP_TIMEOUT;s_http_job.context.credential_epoch=1U;
    s_http_job.version=s_assignment.version;strcpy(s_http_job.room_id,s_assignment.room_id);
    strcpy(s_http_job.session_id,s_session);strcpy(s_http_job.code,"000007");strcpy(s_http_job.password,"0000");
    snprintf(response_json,sizeof(response_json),"%s",json);
    s_http_busy=true;s_http_done=false;room_execute_http();s_http_done=true;
}
static void receive_audio(uint32_t length) {
    static const uint8_t codes[]={0xd5,0x55,0xaa,0x2a};
    uint8_t bytes[641];unsigned int i;TIRTCFRAMEINFO frame;
    CHECK(length<=sizeof(bytes));for(i=0U;i<length;++i)bytes[i]=codes[i%4U];
    memset(&frame,0,sizeof(frame));frame.stream_id=1U;frame.media=TIRTC_AUDIO_ALAW;
    frame.flags=TIRTC_AUDIOSAMPLE_8K16B1C;frame.length=length;
    room_on_audio(s_wire_connection,&frame,bytes);
}

static void test_http(void) {
    cJSON *body;room_job_kind_e kinds[]={ROOM_JOB_CREATE,ROOM_JOB_JOIN,ROOM_JOB_LEAVE};
    const char *paths[]={"/v1/call/group/device/create","/v1/call/group/device/join","/v1/call/group/device/leave"};
    unsigned int i;
    for(i=0U;i<3U;++i) {
        execute(kinds[i],assignment_json);CHECK(s_http_result.code==200);
        CHECK(!strcmp(s_http_result.assignment.room_code,"000007"));CHECK(!strcmp(last_path,paths[i]));
        body=cJSON_Parse(last_body);CHECK(body!=NULL);
        if(i==0U)CHECK(cJSON_GetObjectItemCaseSensitive(body,"room_code")==NULL);
        if(i==1U)CHECK(!strcmp(room_string(body,"room_code"),"000007"));
        if(i<2U)CHECK(!strcmp(room_string(body,"password"),"0000"));
        else {CHECK(!strcmp(room_string(body,"room_id"),"room-a"));CHECK(cJSON_GetObjectItemCaseSensitive(body,"password")==NULL);}
        cJSON_Delete(body);CHECK(s_http_response[0]==0);
    }
    response_status=201;execute(ROOM_JOB_CREATE,assignment_json);CHECK(s_http_result.code==201);
    response_status=200;execute(ROOM_JOB_JOIN,"{\"code\":40301}");CHECK(s_http_result.code==40301);
    execute(ROOM_JOB_JOIN,"{\"code\":200.5}");CHECK(s_http_result.code==ROOM_ERR_PROTOCOL);
    execute(ROOM_JOB_JOIN,"{\"code\":200,\"data\":{\"assignment_version\":11,\"desired_state\":\"joined\",\"room_id\":\"x\",\"room_code\":7}}");
    CHECK(s_http_result.code==ROOM_ERR_PROTOCOL);
    execute(ROOM_JOB_LEAVE,"{\"code\":200,\"data\":{\"assignment_version\":12,\"desired_state\":\"left\"}}");
    CHECK(s_http_result.code==200 && !s_http_result.assignment.desired);
}
static void test_assignment(void) {
    cJSON *root;room_assignment_t assignment;uint32_t generation;
    memset(&assignment,0,sizeof(assignment));
    root=cJSON_Parse("{\"assignment_version\":11,\"desired_state\":\"joined\",\"room_id\":\"x\",\"room_code\":\"000007\"}");
    CHECK(room_parse_assignment(root,&assignment));CHECK(!strcmp(assignment.room_code,"000007"));cJSON_Delete(root);
    root=cJSON_Parse("{\"assignment_version\":11,\"desired_state\":\"joined\",\"room_id\":\"x\",\"room_code\":\"12345x\"}");
    CHECK(!room_parse_assignment(root,&assignment));cJSON_Delete(root);
    CHECK(!room_generation("4294967296",&generation));CHECK(room_generation("4294967295",&generation));
    CHECK(generation==UINT32_MAX);CHECK(!room_generation("0",&generation));
    execute(ROOM_JOB_ASSIGNMENT,assignment_json);s_http_result.assignment.version=9U;
    room_consume_http();CHECK(!strcmp(s_assignment.room_id,"room-a"));CHECK(!s_stopping);
    s_token_requested=true;execute(ROOM_JOB_ASSIGNMENT,assignment_json);room_consume_http();
    CHECK(!strcmp(s_assignment.room_id,"room-b"));CHECK(s_stopping && s_terminal_pending);
    CHECK(!strcmp(s_terminal.room_id,"room-a"));CHECK(s_terminal.version==10U);
    CHECK(!strcmp(s_terminal.session_id,"session-old"));CHECK(s_ui_generation!=6U);
}
static void test_ack(void) {
    CHECK(ptt(1,s_ui_generation)==-1);receive_audio(160U);room_audio_tick();
    CHECK(audio_sends==0 && records==0 && plays==0 && s_rx_count==0U);
    signal_json("{\"jsonrpc\":\"2.0\",\"id\":6,\"result\":{}}");CHECK(!s_joined);
    signal_json(ack_json);CHECK(s_joined && s_accept_audio && !s_mic_gate);
    CHECK(!strcmp(s_media_session,"media-a"));CHECK(!strcmp(s_wire_room,"tenant:room-a"));
    CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();CHECK(audio_sends==1);
    CHECK(sent_frame.stream_id==1U && sent_frame.media==TIRTC_AUDIO_ALAW);
    CHECK(sent_frame.flags==TIRTC_AUDIOSAMPLE_8K16B1C && sent_frame.length==160U);
    /* The hardware returns 16 kHz PCM. Each network byte represents the
     * average of a PCM pair: (0,32767), (-32768,0), (32767,-32768). */
    for (unsigned int i=0U;i<sizeof(sent_audio);++i) {
        const int16_t expected[]={16383,-16384,0};
        CHECK(sent_audio[i]==demo_g711_alaw_encode_sample(expected[i%3U]));
    }
    CHECK(s_snapshot.tx_dropped==0U);
}
static void test_ptt(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);clock_ms+=749U;
    room_foreground_tick();CHECK(s_mic_gate);CHECK(ptt(1,s_ui_generation)==0);
    clock_ms+=749U;room_foreground_tick();CHECK(s_mic_gate);
    ++clock_ms;room_foreground_tick();CHECK(!s_mic_gate);room_audio_tick();CHECK(audio_sends==0);
    CHECK(ptt(1,s_ui_generation-1U)==-1);CHECK(!s_mic_gate);
    CHECK(ptt(1,s_ui_generation)==0);tirtc_group_set_audio_config(8,0,true,true);CHECK(!s_mic_gate);
    tirtc_group_set_audio_config(8,10,true,true);clock_ms=UINT32_MAX-500U;
    CHECK(ptt(1,s_ui_generation)==0);clock_ms+=749U;room_foreground_tick();CHECK(s_mic_gate);
    ++clock_ms;room_foreground_tick();CHECK(!s_mic_gate);
    CHECK(ptt(1,s_ui_generation)==0);shown_page=TIRTC_PAGE_HOME;room_foreground_tick();
    CHECK(!s_mic_gate && !s_accept_audio && s_suspend_requested);
}
static void test_capture(void) {
    int mode;ready_audio();
    for(mode=1;mode<=4;++mode) {
        s_enabled=true;s_foreground=true;s_accept_audio=true;s_suspend_requested=false;
        CHECK(ptt(1,s_ui_generation)==0);capture_interrupt=mode;room_audio_tick();
        CHECK(audio_sends==0);CHECK(discards>=mode);CHECK(s_capture[0]==0 && s_capture[159]==0);
    }
    capture_interrupt=0;CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();CHECK(audio_sends==1);
    send_buffer_used=ROOM_SEND_HIGH_WATER+1U;room_audio_tick();
    CHECK(audio_sends==1 && s_snapshot.tx_dropped==1U);
}
static void test_playback(void) {
    const uint32_t lengths[]={1U,159U,160U,161U,640U};
    const int16_t expected[]={8,-8,32256,-32256};
    unsigned int k,i;ready_audio();
    for(k=0U;k<sizeof(lengths)/sizeof(lengths[0]);++k) {
        uint32_t start=played_count,n=lengths[k],expected_samples=n*2U;
        receive_audio(n);CHECK(s_rx_count==1U);
        while(s_rx_count) {room_audio_tick();clock_ms+=20U;}
        CHECK(played_count-start==expected_samples);
        for(i=0U;i<n;++i) {
            CHECK(played[start+i*2U]==expected[i%4U]);
            CHECK(played[start+i*2U+1U]==expected[i%4U]);
        }
    }
    receive_audio(641U);CHECK(s_rx_count==0U);
    receive_audio(160U);playback_busy=1;room_audio_tick();CHECK(s_rx_count==1U && s_play_offset==0U);
    playback_busy=0;clock_ms+=ROOM_RX_MAX_AGE+1U;room_audio_tick();CHECK(s_rx_count==0U);
    receive_audio(160U);++s_epoch;room_audio_tick();CHECK(s_rx_count==0U);
    CHECK(s_snapshot.rx_dropped==2U);
}
static void test_stale_http(void) {
    int kind;uint32_t generation=s_ui_generation;
    for(kind=ROOM_JOB_ASSIGNMENT;kind<=ROOM_JOB_LEAVE;++kind) {
        if(kind!=ROOM_JOB_ASSIGNMENT && kind<ROOM_JOB_CREATE)continue;
        execute((room_job_kind_e)kind,assignment_json);context_valid=false;
        room_consume_http();CHECK(s_assignment.version==10U);CHECK(s_ui_generation==generation);
        CHECK(!s_http_busy && !s_http_done && !s_stopping);context_valid=true;
    }
    CHECK(page_requests==0);
    s_claimed=true;
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60}}");
    CHECK(s_http_result.code==200);++s_epoch;room_consume_http();CHECK(!s_connect_pending && !s_http_busy);
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60}}");
    ++s_seen_intent;room_consume_http();CHECK(!s_connect_pending && !s_http_busy);
    s_joined=true;s_lease_deadline=12000U;
    execute(ROOM_JOB_PRESENCE,"{\"code\":200}");++s_epoch;room_consume_http();CHECK(s_lease_deadline==12000U);
}
static void test_preemption(void) {
    uint32_t ticket,old_generation=s_ui_generation;ready_audio();s_claimed=true;s_token_requested=true;
    CHECK(ptt(1,s_ui_generation)==0);
    CHECK(demo_group_intercom_reserve_call(DEMO_GROUP_CALL_DEVICE,&ticket)==1);
    CHECK(ticket!=0U && !s_accept_audio && !s_mic_gate);
    CHECK(!demo_group_intercom_call_ready(DEMO_GROUP_CALL_DEVICE,ticket));
    room_close(DEMO_GROUP_SUSPENDED,0,"suspended");
    s_producers=1U;CHECK(!room_cleanup());CHECK(releases==0);
    s_producers=0U;s_sdk_submit_busy=true;CHECK(!room_cleanup());CHECK(releases==0);
    s_sdk_submit_busy=false;CHECK(room_cleanup());CHECK(releases==1 && disconnects==1);
    CHECK(demo_group_intercom_call_ready(DEMO_GROUP_CALL_DEVICE,ticket));
    CHECK(s_ui_generation!=old_generation);CHECK(ptt(1,old_generation)==-1);
    demo_group_intercom_release_call(DEMO_GROUP_CALL_DEVICE,ticket+1U);CHECK(s_call==DEMO_GROUP_CALL_DEVICE);
    demo_group_intercom_release_call(DEMO_GROUP_CALL_DEVICE,ticket);CHECK(s_call==DEMO_GROUP_CALL_NONE);
    CHECK(!strcmp(s_terminal.room_id,"room-a") && !strcmp(s_terminal.session_id,"session-old"));
    CHECK(s_terminal.context.credential_epoch==1U);
}
static void test_network(void) {
    ready_audio();s_claimed=true;s_token_requested=true;CHECK(ptt(1,s_ui_generation)==0);
    network_ready=false;room_step();CHECK(s_media_idle && !s_joined && !s_mic_gate);
    CHECK(audio_sends==0 && records==0 && releases==1);
    CHECK(s_snapshot.state==DEMO_GROUP_RECONNECTING);
}
static void test_commands(void) {
    char deep[80];unsigned int i;ready_audio();
    room_on_command((void *)0xabcd,ROOM_COMMAND,ack_json,(uint32_t)strlen(ack_json));CHECK(s_cmd_count==0U);
    room_on_command(s_wire_connection,ROOM_COMMAND,ack_json,(uint32_t)strlen(ack_json));CHECK(s_cmd_count==1U);
    ++s_epoch;room_process_commands();CHECK(s_cmd_count==0U && !s_stopping);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"room_closed\",\"params\":{\"room_id\":\"other:room-b\"}}");
    CHECK(!s_stopping && s_assignment.desired);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"room_snapshot\",\"params\":{\"room_id\":\"tenant:room-a\",\"self\":{\"participant_id\":\"self\"},\"participants\":[{\"participant_id\":\"self\",\"device_name\":\"A\",\"mic_state\":\"on\"}]}}");
    CHECK(s_ui.member_count==1U && s_ui.members[0].self && !s_ui.members[0].speaking);
    for(i=0U;i<17U;++i)deep[i]='[';deep[17]='0';for(i=18U;i<35U;++i)deep[i]=']';deep[35]=0;
    CHECK(room_parse_json(deep,35U)==NULL);
    CHECK(room_parse_json("{}\0{}",5U)==NULL);CHECK(room_parse_json("{} x",4U)==NULL);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"room_closed\",\"params\":{\"room_id\":\"tenant:room-a\"}}");
    CHECK(s_stopping && !s_assignment.desired && !s_accept_audio);
}
static void room_open(int value) {
    tirtc_ui_action_t action;memset(&action,0,sizeof(action));
    action.type=TIRTC_ACTION_ROOM_OPEN;action.value=value;CHECK(tirtc_group_action(&action)==0);
}
static void test_foreground(void) {
    /* The UI enqueues entry before its next rendered-page snapshot is visible. */
    s_media_idle=true;s_audio.generation=0U;shown_page=TIRTC_PAGE_HOME;
    room_open(1);room_step();CHECK(!s_joined && !s_mic_gate);
    shown_page=TIRTC_PAGE_ROOM;room_step();
    CHECK(s_enabled && s_foreground);
    /* An unavailable page snapshot during redraw must also retain entry intent. */
    room_open(0);room_step();shown_valid=false;
    room_open(1);room_step();shown_valid=true;room_step();
    CHECK(s_enabled && s_foreground);
}
static void test_resume(void) {
    uint32_t old_generation=s_ui_generation;
    ready_audio();s_claimed=true;s_token_requested=true;CHECK(ptt(1,s_ui_generation)==0);
    calls_active=true;tirtc_group_suspend();room_step();
    CHECK(s_media_idle && !s_mic_gate && !s_accept_audio);
    /* A very short call may leave the rendered UI on ROOM for its entire life. */
    calls_active=false;room_step();CHECK(s_enabled && s_foreground);
    CHECK(!s_mic_gate);CHECK(ptt(1,old_generation)==-1);
    /* Explicit navigation away revokes the retained room intent. */
    room_open(0);room_step();room_step();CHECK(!s_enabled && !s_foreground);
    CHECK(!s_mic_gate && !s_accept_audio);
}
static void test_cleanup_order(void) {
    s_claimed=true;ready_audio();room_close(DEMO_GROUP_SUSPENDED,0,"suspended");
    audio_release_busy=1;CHECK(!room_cleanup());CHECK(releases==0 && s_claimed);
    CHECK(!demo_group_intercom_is_idle());
    audio_release_busy=0;CHECK(room_cleanup());CHECK(releases==1);
    CHECK(audio_release_order>0 && runtime_release_order>audio_release_order);
}
static void test_audio_config(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    tirtc_group_set_audio_config(8,10,false,true);
    CHECK(s_speaker==0U && s_mic_level==10U && s_mic_gate);
    room_step();CHECK(configured_speaker==0 && configured_mic==10);
    tirtc_group_set_audio_config(8,10,true,false);
    CHECK(!s_mic_gate && s_mic_level==0U && s_speaker==8U);
    CHECK(ptt(1,s_ui_generation)==-1);room_step();
    CHECK(configured_speaker==8 && configured_mic==0);
    tirtc_group_set_audio_config(255,255,true,true);CHECK(s_speaker==10U && s_mic_level==10U);
    CHECK(!s_mic_gate);
}
static void test_invalid_ack(void) {
    s_lease_deadline=clock_ms;signal_json(ack_json);
    CHECK(!s_joined && !s_accept_audio && !s_mic_gate);
    s_lease_deadline=clock_ms+60000U;
    signal_json("{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"session_id\":\"wrong-format\",\"room_id\":\"room-a\","
        "\"input_audio\":{\"codec\":\"g711a\",\"sample_rate\":16000,\"channels\":1},"
        "\"output_audio\":{\"codec\":\"g711a\",\"sample_rate\":8000,\"channels\":1}}}");
    CHECK(s_stopping && s_manual_error && !s_joined && !s_accept_audio);
    CHECK(s_cleanup_error==ROOM_ERR_PROTOCOL);CHECK(ptt(1,s_ui_generation)==-1);
}
static void test_token_lease(void) {
    s_claimed=true;
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":20}}");
    CHECK(s_http_result.code==ROOM_ERR_PROTOCOL);
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60,\"expires_at\":1}}");
    CHECK(s_http_result.code==ROOM_ERR_LEASE);
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60}}");
    CHECK(s_http_result.code==200);clock_ms+=60000U;room_consume_http();
    CHECK(s_stopping && !s_connect_pending && s_cleanup_error==ROOM_ERR_LEASE);
}
static void test_queued_control_rebind(void) {
    tirtc_ui_action_t action;
    /* A tap can wait behind an assignment request throughout reprovisioning. */
    s_media_idle=true;s_audio.generation=0U;s_bound_seen=true;
    memset(&s_assignment,0,sizeof(s_assignment));
    CHECK(room_schedule(ROOM_JOB_ASSIGNMENT,NULL));
    memset(&action,0,sizeof(action));action.type=TIRTC_ACTION_ROOM_JOIN;
    strcpy(action.text,"000007");strcpy(action.extra,"1234");
    CHECK(tirtc_group_action(&action)==0);
    binding_state=DEMO_BIND_WAIT_GRANT;context_valid=false;room_step();
    CHECK(!s_enabled && s_http_busy);
    binding_state=DEMO_BIND_BOUND;context_valid=true;current_credential_epoch=2U;
    s_http_done=true;s_http_result.code=ROOM_ERR_OFFLINE;room_step();
    CHECK(!s_control_pending && s_control_password[0]==0);
    CHECK(!s_http_busy || s_http_job.kind!=ROOM_JOB_JOIN);
    CHECK(http_calls==0 && page_requests==0);
}
static void test_session_context_schedule(void) {
    /* Audio preparation may yield while the binding credentials change. */
    s_media_idle=true;s_audio.generation=0U;context_change_on_prewarm=true;
    CHECK(room_start_session());
    CHECK(s_session_context.credential_epoch==1U && current_credential_epoch==2U);
    if(s_http_busy)room_execute_http();
    CHECK(http_calls==0 && s_stopping);
}
static void test_connect_context_submit(void) {
    s_claimed=true;
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60}}");
    room_consume_http();CHECK(s_http_job.kind==ROOM_JOB_CONNECT);
    /* The helper must reject a token after consumption but before SDK entry. */
    current_credential_epoch=2U;room_execute_http();
    CHECK(whip_submissions==0U && !s_connect_pending);
}
static void test_late_control_navigation(void) {
    tirtc_ui_action_t action={0};
    s_media_idle=true; s_audio.generation=0U;
    memset(&s_assignment,0,sizeof(s_assignment));
    action.type=TIRTC_ACTION_ROOM_JOIN; strcpy(action.text,"000007");
    CHECK(tirtc_group_action(&action)==0);
    room_control_tick(); CHECK(s_http_busy && s_http_job.kind==ROOM_JOB_JOIN);
    /* A delayed response may update assignment, but cannot navigate a later
     * visit to the form after the user explicitly left the original visit. */
    room_open(0); room_open(1); room_foreground_tick();
    snprintf(response_json,sizeof(response_json),"%s",assignment_json);
    room_execute_http(); s_http_done=true; room_consume_http();
    CHECK(s_assignment.desired && !strcmp(s_assignment.room_code,"000007"));
    CHECK(page_requests==0 && !s_control_pending);
}
static void fast_cold_connection(void) {
    s_media_idle=true;s_audio.generation=0U;
    s_sync_at=clock_ms+ROOM_SYNC_INTERVAL;
    prewarm_extra_ms=DEMO_AI_AUDIO_SESSION_WARMUP_MS-DEMO_AI_AUDIO_WARMUP_MS;
    CHECK(room_start_session());
    CHECK(s_play_tail-clock_ms==1200U && !s_accept_audio);
    execute(ROOM_JOB_TOKEN,"{\"code\":200,\"data\":{\"peer_id\":\"p\",\"token\":\"t\",\"heartbeat_seconds\":10,\"lease_seconds\":60}}");
    room_consume_http();CHECK(s_http_job.kind==ROOM_JOB_CONNECT);
    room_execute_http();s_http_done=true;
    room_connect_result(0,(void *)0x1234,&s_connect_context);
    room_step();
}
static void acknowledge_current_join(void) {
    cJSON *root=room_parse_json(ack_json,strlen(ack_json));CHECK(root!=NULL);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(root,"id"),s_join_id);
    room_signal_json(root);cJSON_Delete(root);
}
static void test_fast_join_warm(void) {
    fast_cold_connection();
    CHECK(!join_commands && !s_accept_audio && s_snapshot.state==DEMO_GROUP_JOINING);
    receive_audio(640U);CHECK(!s_rx_count && !s_snapshot.rx_dropped);
    clock_ms+=1199U;room_step();CHECK(!join_commands);
    /* A late SDK FINISH cannot hold up a preamble whose media time elapsed. */
    ++clock_ms;playback_done=false;room_step();CHECK(join_commands==1U && !s_accept_audio);
    CHECK(s_deadline-clock_ms==ROOM_JOIN_TIMEOUT);
    acknowledge_current_join();CHECK(s_joined && s_accept_audio);
    warm_extra_ms=80U;receive_audio(640U);room_audio_tick();
    CHECK(played_count==1280U && s_rx_count==0U && s_snapshot.rx_dropped==0U);
    CHECK(s_play_tail-clock_ms==160U);
}
static void test_join_warm_wrap(void) {
    clock_ms=UINT32_MAX-600U;fast_cold_connection();CHECK(!join_commands);
    clock_ms+=1199U;room_step();CHECK(!join_commands);
    ++clock_ms;room_step();CHECK(join_commands==1U);
}
static void test_join_warm_cancel(void) {
    fast_cold_connection();CHECK(!join_commands);
    calls_active=true;tirtc_group_suspend();room_step();
    CHECK(s_media_idle && !s_accept_audio && !join_commands);
    CHECK(releases==1 && audio_release_order<runtime_release_order);
    clock_ms+=1200U;room_step();CHECK(!join_commands);
}
static void test_join_warm_levels(void) {
    fast_cold_connection();CHECK(!join_commands);
    tirtc_group_set_audio_config(6U,7U,true,true);room_step();
    CHECK(configured_speaker==6 && configured_mic==7 && !join_commands);
    clock_ms+=1200U;room_step();CHECK(join_commands==1U);
}
static void test_join_warm_timeout(void) {
    fast_cold_connection();CHECK(!join_commands);
    clock_ms+=1200U;room_step();CHECK(join_commands==1U);
    clock_ms+=ROOM_JOIN_TIMEOUT-1U;room_step();CHECK(!s_stopping);
    ++clock_ms;room_step();
    CHECK(s_stopping && join_commands==1U && s_cleanup_error==TIRTC_E_TIMEOUTED);
}
static void test_join_warm_muted_restore(void) {
    /* HAL's GROUP contract queues the cold PCM clocks even when muted;
     * the real HAL tests separately verify that contract and gain writes. */
    tirtc_group_set_audio_config(7U,7U,false,true);
    fast_cold_connection();CHECK(!join_commands && configured_speaker==0);
    clock_ms+=1200U;room_step();CHECK(join_commands==1U);
    acknowledge_current_join();CHECK(s_joined && !plays);
    tirtc_group_set_audio_config(7U,7U,true,true);room_step();
    CHECK(configured_speaker==7 && !plays);
    warm_extra_ms=80U;receive_audio(640U);room_audio_tick();
    CHECK(played_count==1280U && s_rx_count==0U && s_snapshot.rx_dropped==0U);
    CHECK(s_play_tail-clock_ms==160U && warm_plays==1);
}
static void test_join_warm_first_volume(void) {
    fast_cold_connection();clock_ms+=1200U;room_step();
    CHECK(join_commands==1U);acknowledge_current_join();CHECK(!plays);
    tirtc_group_set_audio_config(4U,7U,true,true);room_step();
    CHECK(configured_speaker==4 && !plays);
    warm_extra_ms=80U;receive_audio(640U);room_audio_tick();
    CHECK(played_count==1280U && s_snapshot.rx_dropped==0U);
    CHECK(s_play_tail-clock_ms==160U && warm_plays==1);
}
static void test_join_warm_early_ack(void) {
    fast_cold_connection();CHECK(!join_commands && s_join_pending);
    acknowledge_current_join();
    CHECK(!s_joined && !s_accept_audio && s_media_session[0]=='\0');
    receive_audio(640U);CHECK(s_rx_count==0U);
    clock_ms+=1200U;room_step();CHECK(join_commands==1U && !s_join_pending);
    acknowledge_current_join();CHECK(s_joined && s_accept_audio);
}
static void test_joined_levels_busy(void) {
    ready_audio();s_members_due=clock_ms+10000U;CHECK(ptt(1,s_ui_generation)==0);
    tirtc_group_set_audio_config(6U,7U,true,true);receive_audio(640U);
    set_levels_ret=DEMO_AI_AUDIO_ERR_BUSY;CHECK(room_step()==20U);
    CHECK(s_levels_dirty && s_mic_dirty && !s_stopping);
    CHECK(records==0 && plays==0 && audio_sends==0 && s_rx_count==1U);
    /* Retry the latest controls, retaining the unsent mic notification too. */
    tirtc_group_set_audio_config(5U,6U,true,true);set_levels_ret=0;room_step();
    CHECK(configured_speaker==5 && configured_mic==6);
    CHECK(!s_levels_dirty && !s_mic_dirty && !s_stopping);
    CHECK(strstr(last_command,"set_mic_state") && strstr(last_command,"speaking"));
    CHECK(records==1 && plays==4 && audio_sends==1 && s_rx_count==0U);
}
static void test_joined_levels_failure(void) {
    ready_audio();s_claimed=true;s_token_requested=true;CHECK(ptt(1,s_ui_generation)==0);
    tirtc_group_set_audio_config(6U,7U,true,true);receive_audio(640U);
    set_levels_ret=-20;room_step();
    CHECK(s_stopping && s_cleanup_error==ROOM_ERR_AUDIO && s_manual_error);
    CHECK(!s_accept_audio && !s_mic_gate && records==0 && plays==0 && audio_sends==0);
    room_step();CHECK(s_media_idle && s_rx_count==0U && releases==1);
    CHECK(audio_release_order>0 && runtime_release_order>audio_release_order);
}

static void test_capture_timeout_recover(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
    room_audio_tick();CHECK(records==1 && audio_sends==0);
#if DEMO_AI_AUDIO_AEC_ENABLE
    CHECK(!s_stopping && !s_manual_error);
    capture_error=0;capture_block_ms=20U;room_audio_tick();
    CHECK(records==2 && audio_sends==1 && !s_stopping);
#else
    CHECK(s_stopping && s_manual_error && s_cleanup_error==ROOM_ERR_AUDIO);
#endif
}
static void test_capture_timeout_persistent(void) {
    unsigned i;uint32_t first;
    ready_audio();capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
    CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();first=clock_ms;
#if DEMO_AI_AUDIO_AEC_ENABLE
    for(i=0U;i<24U;++i) {
        CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();CHECK(!s_stopping && !audio_sends);
    }
    CHECK(clock_ms-first==2880U);CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();
    CHECK(clock_ms-first==3000U && s_stopping && s_manual_error && s_cleanup_error==ROOM_ERR_AUDIO);
#else
    (void)i;(void)first;CHECK(s_stopping && s_manual_error && !audio_sends);
#endif
}
static void test_capture_fault(int error) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);capture_error=error;
    room_audio_tick();CHECK(s_stopping && s_manual_error && s_cleanup_error==ROOM_ERR_AUDIO && !audio_sends);
}
static void test_capture_timeout_interrupt(int mode) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
#if DEMO_AI_AUDIO_AEC_ENABLE
    room_audio_tick();CHECK(!s_stopping);clock_ms+=3000U;
    CHECK(ptt(1,s_ui_generation)==0);
#endif
    capture_interrupt=mode;room_audio_tick();
    CHECK(!s_stopping && !s_manual_error && !audio_sends);
#if DEMO_AI_AUDIO_AEC_ENABLE
    CHECK(!s_record_failure_active);
#endif
    if(mode==1 || mode==3 || mode==4 || mode==5) {
        capture_interrupt=0;tirtc_group_set_audio_config(8,7,true,true);
        CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();
#if DEMO_AI_AUDIO_AEC_ENABLE
        CHECK(!s_stopping && !audio_sends);
#else
        CHECK(s_stopping && !audio_sends);
#endif
    }
}
static void test_capture_timeout_segment(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
    room_audio_tick();
#if DEMO_AI_AUDIO_AEC_ENABLE
    CHECK(!s_stopping);clock_ms+=3000U;
    CHECK(ptt(0,s_ui_generation)==0);CHECK(ptt(1,s_ui_generation)==0);
    room_audio_tick();CHECK(!s_stopping && !audio_sends);
#else
    CHECK(s_stopping && !audio_sends);
#endif
}
static void test_capture_timeout_success_reset(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
    room_audio_tick();
#if DEMO_AI_AUDIO_AEC_ENABLE
    clock_ms+=2800U;capture_error=0;capture_block_ms=20U;
    CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();CHECK(audio_sends==1 && !s_stopping);
    clock_ms+=300U;capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;
    CHECK(ptt(1,s_ui_generation)==0);room_audio_tick();CHECK(!s_stopping && audio_sends==1);
#else
    CHECK(s_stopping && !audio_sends);
#endif
}
static void test_capture_timeout_wrap(void) {
    clock_ms=UINT32_MAX-1000U;
    s_session_started=clock_ms;test_capture_timeout_persistent();
}
static void test_capture_timeout_close(void) {
    ready_audio();CHECK(ptt(1,s_ui_generation)==0);
    capture_error=DEMO_AI_AUDIO_ERR_CAPTURE_TIMEOUT;capture_block_ms=120U;room_audio_tick();
#if DEMO_AI_AUDIO_AEC_ENABLE
    CHECK(s_record_failure_active && !s_stopping);
    room_close(DEMO_GROUP_IDLE,0,"left");CHECK(!s_record_failure_active && s_stopping);
#else
    CHECK(s_stopping && s_cleanup_error==ROOM_ERR_AUDIO);
#endif
}

int main(int argc,char **argv) {
    CHECK(argc==2);
    if (!strncmp(argv[1],"init",4)) {
        tirtc_ui_action_t a={0};
        init_fail_stage=atoi(argv[1]+4);CHECK(init_fail_stage>=1&&init_fail_stage<=5);
        CHECK(tirtc_group_start_service()==ROOM_ERR_INIT);CHECK(!s_initialized);
        a.type=TIRTC_ACTION_ROOM_OPEN;a.value=1;CHECK(tirtc_group_action(&a)<0);
        printf("failure-stage=%d first-result=%d published=%d remembered-open=%d\n",init_fail_stage,ROOM_ERR_INIT,room_publications,s_enabled);
        CHECK(tirtc_group_start_service()==0);CHECK(s_initialized);
        CHECK(s_enabled);CHECK(room_publications>0&&strstr(last_room.message,"(-6105)")!=NULL);CHECK(!last_room.busy&&!last_room.connected);
        CHECK(sem_created==2&&handler_registered==1&&http_created==1&&room_created==1);
        CHECK(tirtc_group_start_service()==0);
        CHECK(sem_created==2&&handler_registered==1&&http_created==1&&room_created==1);
        s_foreground=false;room_step();CHECK(s_foreground);
        CHECK(critical_depth==0);
        printf("PASS init stage=%d recovered; sem=%d handler=%d http=%d room=%d retained-open=%d\n",init_fail_stage,sem_created,handler_registered,http_created,room_created,s_enabled);return 0;
    }
    setup();
    if(!strcmp(argv[1],"http"))test_http();
    else if(!strcmp(argv[1],"assignment"))test_assignment();
    else if(!strcmp(argv[1],"ack"))test_ack();
    else if(!strcmp(argv[1],"ptt"))test_ptt();
    else if(!strcmp(argv[1],"capture"))test_capture();
    else if(!strcmp(argv[1],"capture_timeout_recover"))test_capture_timeout_recover();
    else if(!strcmp(argv[1],"capture_timeout_persistent"))test_capture_timeout_persistent();
    else if(!strcmp(argv[1],"capture_fault"))test_capture_fault(-41);
    else if(!strcmp(argv[1],"capture_invalid_lease"))test_capture_fault(DEMO_AI_AUDIO_ERR_INVALID_LEASE);
    else if(!strcmp(argv[1],"capture_timeout_release"))test_capture_timeout_interrupt(1);
    else if(!strcmp(argv[1],"capture_timeout_suspend"))test_capture_timeout_interrupt(2);
    else if(!strcmp(argv[1],"capture_timeout_ptt_expiry"))test_capture_timeout_interrupt(3);
    else if(!strcmp(argv[1],"capture_timeout_quick_ptt"))test_capture_timeout_interrupt(4);
    else if(!strcmp(argv[1],"capture_timeout_mute"))test_capture_timeout_interrupt(5);
    else if(!strcmp(argv[1],"capture_timeout_segment"))test_capture_timeout_segment();
    else if(!strcmp(argv[1],"capture_timeout_success_reset"))test_capture_timeout_success_reset();
    else if(!strcmp(argv[1],"capture_timeout_wrap"))test_capture_timeout_wrap();
    else if(!strcmp(argv[1],"capture_timeout_close"))test_capture_timeout_close();
    else if(!strcmp(argv[1],"playback"))test_playback();
    else if(!strcmp(argv[1],"stale_http"))test_stale_http();
    else if(!strcmp(argv[1],"preemption"))test_preemption();
    else if(!strcmp(argv[1],"network"))test_network();
    else if(!strcmp(argv[1],"commands"))test_commands();
    else if(!strcmp(argv[1],"foreground"))test_foreground();
    else if(!strcmp(argv[1],"resume"))test_resume();
    else if(!strcmp(argv[1],"cleanup_order"))test_cleanup_order();
    else if(!strcmp(argv[1],"audio_config"))test_audio_config();
    else if(!strcmp(argv[1],"invalid_ack"))test_invalid_ack();
    else if(!strcmp(argv[1],"token_lease"))test_token_lease();
    else if(!strcmp(argv[1],"queued_control_rebind"))test_queued_control_rebind();
    else if(!strcmp(argv[1],"session_context_schedule"))test_session_context_schedule();
    else if(!strcmp(argv[1],"connect_context_submit"))test_connect_context_submit();
    else if(!strcmp(argv[1],"late_control_navigation"))test_late_control_navigation();
    else if(!strcmp(argv[1],"fast_join_warm"))test_fast_join_warm();
    else if(!strcmp(argv[1],"join_warm_wrap"))test_join_warm_wrap();
    else if(!strcmp(argv[1],"join_warm_cancel"))test_join_warm_cancel();
    else if(!strcmp(argv[1],"join_warm_levels"))test_join_warm_levels();
    else if(!strcmp(argv[1],"join_warm_timeout"))test_join_warm_timeout();
    else if(!strcmp(argv[1],"join_warm_muted_restore"))test_join_warm_muted_restore();
    else if(!strcmp(argv[1],"join_warm_first_volume"))test_join_warm_first_volume();
    else if(!strcmp(argv[1],"join_warm_early_ack"))test_join_warm_early_ack();
    else if(!strcmp(argv[1],"joined_levels_busy"))test_joined_levels_busy();
    else if(!strcmp(argv[1],"joined_levels_failure"))test_joined_levels_failure();
    else CHECK(0);
    CHECK(critical_depth==0);printf("PASS %s\n",argv[1]);return 0;
}
