/* Protocol regressions using the real Room owner and the existing I/O fakes. */
#define main group_backend_original_main
#include "group_backend_test.c"
#undef main

static const char *protocol_snapshot =
    "{\"jsonrpc\":\"2.0\",\"method\":\"room_snapshot\",\"params\":{"
    "\"room_id\":\"tenant:room-a\",\"participant_id\":\"self\","
    "\"self\":{\"participant_id\":\"self\",\"device_id\":\"device-a\"},"
    "\"participants\":[{\"participant_id\":\"self\",\"device_id\":\"device-a\","
    "\"state\":\"active\",\"mic_state\":\"on\"},"
    "{\"participant_id\":\"peer\",\"device_id\":\"device-b\","
    "\"state\":\"active\",\"mic_state\":\"on\"}]}}";

static void protocol_scoped_pin(void) {
    ready_audio();signal_json(protocol_snapshot);
    CHECK(!strcmp(s_wire_room,"tenant:room-a"));
    CHECK(!room_match_wire("other:room-a"));
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"room_closed\","
                "\"params\":{\"room_id\":\"room-a\"}}");
    CHECK(!s_stopping && s_assignment.desired && s_accept_audio);
    CHECK(room_match_wire("tenant:room-a"));
}
static void protocol_raw_pin(void) {
    CHECK(room_match_wire("room-a"));
    CHECK(!strcmp(s_wire_room,"room-a"));
    CHECK(!room_match_wire("tenant:room-a"));
    CHECK(room_match_wire("room-a"));
}
static void protocol_mic_states(void) {
    ready_audio();signal_json(protocol_snapshot);
    CHECK(s_ui.member_count==2U && !s_ui.members[1].speaking);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"participant_mic_state_changed\","
                "\"params\":{\"room_id\":\"tenant:room-a\",\"participant_id\":\"peer\",\"mic_state\":\"speaking\"}}");
    CHECK(s_ui.members[1].speaking);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"participant_mic_state_changed\","
                "\"params\":{\"room_id\":\"tenant:room-a\",\"participant_id\":\"peer\",\"mic_state\":\"on\"}}");
    CHECK(!s_ui.members[1].speaking);
    signal_json("{\"jsonrpc\":\"2.0\",\"method\":\"participant_mic_state_changed\","
                "\"params\":{\"room_id\":\"tenant:room-a\",\"participant_id\":\"peer\",\"mic_state\":\"off\"}}");
    CHECK(!s_ui.members[1].speaking);
}
static void protocol_self_ptt(void) {
    ready_audio();signal_json(protocol_snapshot);room_publish();
    CHECK(s_published.members[0].self && !s_published.members[0].speaking);
    CHECK(ptt(1,s_ui_generation)==0);room_publish();
    CHECK(s_published.mic_on && s_published.members[0].speaking);
    CHECK(ptt(0,s_ui_generation)==0);room_publish();
    CHECK(!s_published.mic_on && !s_published.members[0].speaking);
}
static void protocol_ack_and_business_id(void) {
    room_job_t job; cJSON *body;
    s_accept_commands=true;
    signal_json("{\"jsonrpc\":\"2.0\",\"id\":6,\"result\":{}}");
    CHECK(!s_joined && !s_accept_audio && !s_stopping);
    signal_json(ack_json);CHECK(s_joined && s_accept_audio);
    CHECK(!strcmp(s_wire_room,"tenant:room-a"));
    memset(&job,0,sizeof(job));job.kind=ROOM_JOB_PRESENCE;
    job.version=s_assignment.version;strcpy(job.room_id,s_assignment.room_id);
    strcpy(job.session_id,s_session);strcpy(job.presence,"joined");
    body=room_job_body(&job);CHECK(body!=NULL);
    CHECK(!strcmp(room_string(body,"room_id"),"room-a"));cJSON_Delete(body);
}
int main(int argc,char **argv) {
    CHECK(argc==2);setup();
    if(!strcmp(argv[1],"scoped_pin"))protocol_scoped_pin();
    else if(!strcmp(argv[1],"raw_pin"))protocol_raw_pin();
    else if(!strcmp(argv[1],"mic_states"))protocol_mic_states();
    else if(!strcmp(argv[1],"self_ptt"))protocol_self_ptt();
    else if(!strcmp(argv[1],"ack_and_business_id"))protocol_ack_and_business_id();
    else CHECK(0);
    CHECK(critical_depth==0);printf("PASS protocol %s\n",argv[1]);return 0;
}
