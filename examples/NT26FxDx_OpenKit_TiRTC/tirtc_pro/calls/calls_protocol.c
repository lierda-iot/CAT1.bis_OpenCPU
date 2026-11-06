/* ThingConnect call protocol adapted from pinned official examples.
 * Copyright (c) 2026 Tange Intelligent Technology.
 * SPDX-License-Identifier: MIT
 * Source/contract: https://github.com/tangeai/tirtc-server-example (thing-connect API). */
#include "calls_protocol.h"
#include "../platform/json_guard.h"
#include "../runtime/tirtc_runtime.h"
#include <limits.h>
#include <string.h>

/* Only the dedicated calls HTTP worker calls these synchronous functions.
 * No response/token/body is logged. cJSON objects live only through return. */
#define PROTO_RESPONSE_BYTES 4096U
#define PROTO_BODY_BYTES 2048U
#define PROTO_HTTP_TIMEOUT_MS 15000U

static bool text_ok(const char *s, size_t capacity, bool required, bool id)
{
    size_t i;
    if (s == NULL) return !required;
    for (i = 0U; i < capacity && s[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32U || c == 127U) return false;
        if (id && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                    c == '.' || c == ':')) return false;
    }
    return i < capacity && (!required || i != 0U);
}

static bool tree_unique(const cJSON *node, unsigned depth)
{
    const cJSON *a, *b;
    if (depth > DEMO_JSON_MAX_DEPTH) return false;
    for (a = node != NULL ? node->child : NULL; a != NULL; a = a->next) {
        if (cJSON_IsObject(node)) {
            if (a->string == NULL) return false;
            for (b = a->next; b != NULL; b = b->next)
                if (b->string == NULL || strcmp(a->string, b->string) == 0) return false;
        }
        if (!tree_unique(a, depth + 1U)) return false;
    }
    return true;
}

static bool has_nul_escape(const char *text)
{
    size_t i;
    for (i = 0U; text[i] != '\0'; ++i) {
        if (text[i] != '\\') continue;
        ++i;
        if (text[i] == '\0') return true;
        if (strncmp(text + i, "u0000", 5U) == 0) return true;
    }
    return false;
}

static cJSON *strict_json(const char *text)
{
    cJSON *root;
    if (text == NULL || has_nul_escape(text)) return NULL;
    root = demo_json_parse_with_length_opts(text, strlen(text) + 1U, NULL, true);
    if (!cJSON_IsObject(root) || !tree_unique(root, 0U)) {
        cJSON_Delete(root); return NULL;
    }
    return root;
}

static bool field(const cJSON *obj, const char *key, char *out, size_t size,
                  bool required, bool id)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(obj, key);
    out[0] = '\0';
    if (value == NULL) return !required;
    if (!cJSON_IsString(value) || !text_ok(value->valuestring, size, required, id)) return false;
    if (value->valuestring != NULL) memcpy(out, value->valuestring, strlen(value->valuestring) + 1U);
    return true;
}

static int business_code(const cJSON *root, int success)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0 ||
        value->valuedouble > (double)INT_MAX ||
        value->valuedouble != (double)value->valueint) return CALLS_PROTO_SCHEMA;
    if (value->valueint == success) return 0;
    return value->valueint > 0 ? value->valueint : CALLS_PROTO_SCHEMA;
}

static int request(demo_binding_service_e service, demo_binding_http_method_e method,
                   const char *path, const char *body, int success,
                   const demo_binding_service_context_t *context, cJSON **root)
{
    char response[PROTO_RESPONSE_BYTES];
    int status = 0, result;
    *root = NULL;
    if (context == NULL || !demo_binding_service_context_current(context)) return CALLS_PROTO_STALE;
    memset(response, 0, sizeof(response));
    result = demo_binding_service_request_guarded(service, method, path, body,
             response, sizeof(response), &status, PROTO_HTTP_TIMEOUT_MS, context);
    if (!demo_binding_service_context_current(context)) return CALLS_PROTO_STALE;
    if (result != 0) return result;
    if (status != 200) return CALLS_PROTO_HTTP;
    if (response[sizeof(response) - 1U] != '\0') return CALLS_PROTO_SCHEMA;
    *root = strict_json(response);
    if (*root == NULL) return CALLS_PROTO_SCHEMA;
    return business_code(*root, success);
}

static bool add(cJSON *obj, const char *key, const char *text)
{ return cJSON_AddStringToObject(obj, key, text) != NULL; }
static bool print_body(cJSON *obj, char *body, size_t size)
{ return obj != NULL && cJSON_PrintPreallocated(obj, body, (int)size, false); }
static const cJSON *data_object(const cJSON *root)
{
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    return cJSON_IsObject(data) ? data : NULL;
}

int calls_proto_device_dial(const tirtc_contact_t *contact, bool video,
    const demo_binding_service_context_t *context, calls_proto_dial_t *out)
{
    cJSON *obj = NULL, *targets, *root = NULL;
    const cJSON *data;
    char body[256]; int result = CALLS_PROTO_INVALID;
    if (out == NULL) return result;
    memset(out, 0, sizeof(*out));
    if (contact == NULL || contact->wechat || !text_ok(contact->id, sizeof(contact->id), true, true)) return result;
    obj = cJSON_CreateObject();
    if (obj == NULL) return CALLS_PROTO_MEMORY;
    targets = cJSON_AddArrayToObject(obj, "targets");
    if (targets == NULL || !cJSON_AddItemToArray(targets, cJSON_CreateString(contact->id)) ||
        !add(obj, "call_type", video ? "video" : "audio") || !print_body(obj, body, sizeof(body))) {
        result = CALLS_PROTO_MEMORY; goto done;
    }
    result = request(DEMO_BIND_SERVICE_CALL, DEMO_BIND_HTTP_POST, "/v1/call/request", body, 200, context, &root);
    data = data_object(root);
    if (result == 0 && (data == NULL || !field(data,"room_id",out->room,sizeof(out->room),true,true))) result = CALLS_PROTO_SCHEMA;
    else if (result == 40202 && data != NULL)
        (void)field(data,"room_id",out->room,sizeof(out->room),false,true);
done:
    cJSON_Delete(root); cJSON_Delete(obj);
    if (result != 0 && result != 40202) memset(out,0,sizeof(*out));
    return result;
}

int calls_proto_device_info(const char *caller, const char *room,
    const demo_binding_service_context_t *context, calls_proto_access_t *out)
{
    cJSON *obj, *root = NULL; const cJSON *data;
    char body[384]; int result = CALLS_PROTO_INVALID;
    if (out == NULL) return result;
    memset(out,0,sizeof(*out));
    if (!text_ok(caller,sizeof(out->peer),true,true) || !text_ok(room,129U,true,true)) return result;
    obj = cJSON_CreateObject();
    if (obj == NULL) return CALLS_PROTO_MEMORY;
    if (!add(obj,"device_id",caller) || !add(obj,"room_id",room) || !add(obj,"purpose","call") || !print_body(obj,body,sizeof(body))) {
        result = CALLS_PROTO_MEMORY; goto done;
    }
    result = request(DEMO_BIND_SERVICE_CALL,DEMO_BIND_HTTP_POST,"/v1/call/device/info",body,200,context,&root);
    data = data_object(root);
    if (result == 0 && (data == NULL || !field(data,"device_id",out->peer,sizeof(out->peer),true,true) ||
        strcmp(out->peer,caller) != 0 || !field(data,"token",out->token,sizeof(out->token),true,false))) result = CALLS_PROTO_SCHEMA;
done:
    cJSON_Delete(obj); cJSON_Delete(root);
    if (result != 0) memset(out,0,sizeof(*out));
    return result;
}

int calls_proto_room_get(const demo_binding_service_context_t *context, calls_proto_room_t *out)
{
    cJSON *root = NULL; const cJSON *data; char role[12],status[12],media[12]; int result;
    if (out == NULL) return CALLS_PROTO_INVALID;
    memset(out,0,sizeof(*out));
    result = request(DEMO_BIND_SERVICE_CALL,DEMO_BIND_HTTP_GET,"/v1/call/room",NULL,200,context,&root);
    data = cJSON_GetObjectItemCaseSensitive(root,"data");
    if (result == 0 && data != NULL && !cJSON_IsNull(data)) {
        if (!cJSON_IsObject(data) || !field(data,"room_id",out->room,sizeof(out->room),true,true) ||
            !field(data,"caller",out->caller,sizeof(out->caller),true,true) ||
            !field(data,"role",role,sizeof(role),true,false) ||
            !field(data,"status",status,sizeof(status),true,false) ||
            !field(data,"call_type",media,sizeof(media),true,false) ||
            (strcmp(role,"caller") != 0 && strcmp(role,"callee") != 0) ||
            (strcmp(status,"active") != 0 && strcmp(status,"answered") != 0) ||
            (strcmp(media,"audio") != 0 && strcmp(media,"video") != 0)) result = CALLS_PROTO_SCHEMA;
        else { out->present=true; out->caller_role=strcmp(role,"caller")==0;
               out->answered=strcmp(status,"answered")==0; out->video=strcmp(media,"video")==0; }
    }
    cJSON_Delete(root);
    if (result != 0) memset(out,0,sizeof(*out));
    return result;
}

int calls_proto_room_action(calls_proto_room_action_t action, const char *room,
    const char *reason, const demo_binding_service_context_t *context)
{
    static const char *const paths[]={"/v1/call/reject","/v1/call/cancel","/v1/call/hangup"};
    char body[512]; cJSON *obj,*root=NULL; int result;
    if ((unsigned)action >= sizeof(paths)/sizeof(paths[0]) || !text_ok(room,129U,true,true) ||
        !text_ok(reason,129U,false,false)) return CALLS_PROTO_INVALID;
    obj=cJSON_CreateObject();
    if (obj == NULL) return CALLS_PROTO_MEMORY;
    if (!add(obj,"room_id",room) || (reason != NULL && reason[0] != '\0' && !add(obj,"reason",reason)) ||
        !print_body(obj,body,sizeof(body))) result=CALLS_PROTO_MEMORY;
    else result=request(DEMO_BIND_SERVICE_CALL,DEMO_BIND_HTTP_POST,paths[action],body,200,context,&root);
    cJSON_Delete(root); cJSON_Delete(obj); return result;
}

int calls_proto_wx_dial(const tirtc_contact_t *contact,const char *device_id,bool video,
    const demo_binding_service_context_t *context,calls_proto_dial_t *out)
{
    char body[640]; cJSON *obj,*root=NULL; const cJSON *data; int result=CALLS_PROTO_INVALID;
    if (out == NULL) return result;
    memset(out,0,sizeof(*out));
    if (contact == NULL || !contact->wechat || !text_ok(contact->id,sizeof(contact->id),true,true) ||
        !text_ok(device_id,64U,true,true) || !text_ok(contact->wx_app_id,sizeof(contact->wx_app_id),false,true) ||
        !text_ok(contact->wx_model_id,sizeof(contact->wx_model_id),false,true)) return result;
    obj=cJSON_CreateObject();
    if (obj == NULL) return CALLS_PROTO_MEMORY;
    if (!add(obj,"device_id",device_id) || !add(obj,"wx_user_openid",contact->id) ||
        !add(obj,"wx_room_type",video?"video":"voice") || !cJSON_AddNumberToObject(obj,"wx_version_type",0) ||
        (contact->wx_app_id[0] && !add(obj,"wx_app_id",contact->wx_app_id)) ||
        (contact->wx_model_id[0] && !add(obj,"wx_model_id",contact->wx_model_id)) || !print_body(obj,body,sizeof(body))) {
        result=CALLS_PROTO_MEMORY; goto done;
    }
    result=request(DEMO_BIND_SERVICE_VOIP,DEMO_BIND_HTTP_POST,"/v1/voip/device/call",body,0,context,&root);
    data=data_object(root);
    if (result==0 && (data==NULL || !field(data,"call_id",out->call_id,sizeof(out->call_id),false,true))) result=CALLS_PROTO_SCHEMA;
done:
    cJSON_Delete(root);cJSON_Delete(obj);
    if (result!=0) memset(out,0,sizeof(*out));
    return result;
}

int calls_proto_profile(const char *json_body,const demo_binding_service_context_t *context)
{
    cJSON *obj,*root=NULL; int result;
    if (json_body==NULL || strlen(json_body)>=PROTO_BODY_BYTES) return CALLS_PROTO_INVALID;
    obj=strict_json(json_body);
    if(obj==NULL) return CALLS_PROTO_SCHEMA;
    cJSON_Delete(obj);
    result=request(DEMO_BIND_SERVICE_DEVICE,DEMO_BIND_HTTP_POST,"/v1/device/profile",json_body,200,context,&root);
    cJSON_Delete(root); return result;
}

int calls_proto_wx_reject(const calls_proto_event_t *event,unsigned reason,
    const demo_binding_service_context_t *context)
{
    char body[PROTO_BODY_BYTES]; cJSON *obj; int result;
    if(event==NULL || !event->wechat || event->type!=CALLS_PROTO_INCOMING || reason>255U ||
       !text_ok(event->room,sizeof(event->room),true,true) ||
       !text_ok(event->app_id,sizeof(event->app_id),true,true) ||
       !text_ok(event->model_id,sizeof(event->model_id),true,true) ||
       !text_ok(event->session_token,sizeof(event->session_token),true,false) ||
       !text_ok(event->payload,sizeof(event->payload),true,false)) return CALLS_PROTO_INVALID;
    if(context==NULL || !demo_binding_service_context_current(context)) return CALLS_PROTO_STALE;
    obj=cJSON_CreateObject();
    if(obj==NULL) return CALLS_PROTO_MEMORY;
    if(!add(obj,"wx_app_id",event->app_id) || !add(obj,"wx_model_id",event->model_id) ||
       !add(obj,"wx_session_token",event->session_token) || !add(obj,"wx_room_id",event->room) ||
       !add(obj,"wx_payload",event->payload) || !cJSON_AddNumberToObject(obj,"hangup_reason",reason) ||
       !print_body(obj,body,sizeof(body))) result=CALLS_PROTO_MEMORY;
    else result=demo_tirtc_service_request(context,"/v1/wxvoip/reject",body,NULL,NULL);
    cJSON_Delete(obj);
    return demo_binding_service_context_current(context)?result:CALLS_PROTO_STALE;
}

void calls_proto_clear_event(calls_proto_event_t *event)
{
    volatile unsigned char *p; size_t n;
    if(event==NULL) return;
    p=(volatile unsigned char *)event;
    for(n=0U;n<sizeof(*event);++n) p[n]=0U;
}

static bool mqtt_media(const cJSON *payload,bool wechat,bool *video)
{
    static const char *const keys[]={"wx_room_type","wxa_room_type","room_type","media_type"};
    char media[16]; size_t i; bool seen=false,current;
    *video=wechat; /* Official WX fallback: registered video profile. */
    if(!wechat) {
        if(!field(payload,"call_type",media,sizeof(media),true,false)) return false;
        if(strcmp(media,"audio")!=0 && strcmp(media,"video")!=0) return false;
        *video=strcmp(media,"video")==0; return true;
    }
    for(i=0U;i<sizeof(keys)/sizeof(keys[0]);++i) {
        if(cJSON_GetObjectItemCaseSensitive(payload,keys[i])==NULL) continue;
        if(!field(payload,keys[i],media,sizeof(media),true,false)) return false;
        if(strcmp(media,"voice")!=0 && strcmp(media,"audio")!=0 && strcmp(media,"video")!=0) return false;
        current=strcmp(media,"video")==0;
        if(seen && current!=*video) return false;
        *video=current;seen=true;
    }
    return true;
}

int calls_proto_parse_mqtt(demo_formal_mqtt_message_kind_e kind,const char *type,
    const char *channel,const cJSON *payload,calls_proto_event_t *out)
{
    bool valid=true,wx; calls_proto_event_type_t event_type;
    if(out==NULL) return CALLS_PROTO_INVALID;
    calls_proto_clear_event(out);
    if((kind!=DEMO_FORMAL_MQTT_COMMAND && kind!=DEMO_FORMAL_MQTT_NOTIFY) || type==NULL || channel==NULL) return 0;
    wx=strcmp(channel,"wx")==0;
    if(!wx && strcmp(channel,"device")!=0) return 0;
    if(strcmp(type,"call_incoming")==0) event_type=CALLS_PROTO_INCOMING;
    else if(strcmp(type,wx?"call_cancel":"room_cancel")==0) event_type=CALLS_PROTO_CANCEL;
    else if(!wx && strcmp(type,"callee_answered")==0) event_type=CALLS_PROTO_ANSWERED;
    else if(!wx && strcmp(type,"call_reject")==0) event_type=CALLS_PROTO_REJECT;
    else return 0;
    if(!cJSON_IsObject(payload) || !tree_unique(payload,0U)) return CALLS_PROTO_SCHEMA;
    out->type=event_type;out->wechat=wx;
    valid=field(payload,wx?"wx_room_id":"room_id",out->room,sizeof(out->room),!wx || event_type==CALLS_PROTO_INCOMING,true);
    if(wx) {
        valid=valid && field(payload,"wx_call_id",out->call_id,sizeof(out->call_id),false,true) &&
              field(payload,"wx_from",out->from,sizeof(out->from),false,false) &&
              field(payload,"wx_user_openid",out->contact_id,sizeof(out->contact_id),false,true);
        if(event_type==CALLS_PROTO_CANCEL && !out->room[0] && !out->call_id[0]) valid=false;
    }
    if(event_type==CALLS_PROTO_INCOMING) {
        valid=valid && mqtt_media(payload,wx,&out->video);
        if(wx) {
            valid=valid && field(payload,"peer_id",out->peer,sizeof(out->peer),true,false) &&
                field(payload,"token",out->token,sizeof(out->token),true,false) &&
                field(payload,"wx_app_id",out->app_id,sizeof(out->app_id),false,true) &&
                field(payload,"wx_model_id",out->model_id,sizeof(out->model_id),false,true) &&
                field(payload,"wx_server_token",out->session_token,sizeof(out->session_token),false,false) &&
                field(payload,"wx_payload",out->payload,sizeof(out->payload),false,false) &&
                field(payload,"wx_user_remark",out->name,sizeof(out->name),false,false);
            if(valid && !out->name[0]) valid=field(payload,"wx_user_nickname",out->name,sizeof(out->name),false,false);
            if(valid && !out->name[0]) valid=field(payload,"remark",out->name,sizeof(out->name),false,false);
        } else {
            valid=valid && field(payload,"caller_id",out->contact_id,65U,true,true) &&
                field(payload,"caller_name",out->name,sizeof(out->name),false,false);
            if(valid) memcpy(out->peer,out->contact_id,strlen(out->contact_id)+1U);
        }
    }
    if(!valid) { calls_proto_clear_event(out);return CALLS_PROTO_SCHEMA; }
    return 1;
}
