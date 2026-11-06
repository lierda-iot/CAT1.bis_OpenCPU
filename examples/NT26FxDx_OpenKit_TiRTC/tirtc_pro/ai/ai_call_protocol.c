/* SPDX-License-Identifier: MIT
 * Device-action envelope/aliases follow xiaotai-lierda ai_chat.c. Only parsing
 * is shared: media, UI and call ownership remain this V04 application's. */
#include "ai_call_protocol.h"
#include <stdio.h>

static const char *string(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool equal(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}
static tirtc_contact_scope_t scope(const char *s)
{
    if (equal(s,"wechat") || equal(s,"wechat_voip") || equal(s,"wx") ||
        equal(s,"voip") || equal(s,"微信") || equal(s,"微信联系人")) return TIRTC_CONTACT_WECHAT;
    if (equal(s,"device") || equal(s,"device_call") || equal(s,"tirtc") || equal(s,"call") ||
        equal(s,"设备") || equal(s,"设备联系人")) return TIRTC_CONTACT_DEVICE;
    return TIRTC_CONTACT_ANY;
}
static bool duplicate_keys(const cJSON *o)
{
    const cJSON *a, *b;
    for (a=o->child; a; a=a->next) {
        if (cJSON_IsObject(o)) for (b=a->next; b; b=b->next)
            if (a->string && b->string && !strcmp(a->string,b->string)) return true;
        if ((cJSON_IsObject(a) || cJSON_IsArray(a)) && duplicate_keys(a)) return true;
    }
    return false;
}
bool ai_call_json_complete(const char *data, size_t length)
{
    size_t i;
    if (!data || !length) return false;
    if (!data[length-1]) --length; /* SDK may include one terminal NUL. */
    if (memchr(data,0,length)) return false;
    for (i=0; i<length; ++i) if (data[i]=='\\' && i+1<length) {
        if (length-i>=6 && !memcmp(data+i,"\\u0000",6)) return false;
        ++i;
    }
    return true; /* cJSON's require_null_terminated checks trailing data. */
}
static const cJSON *payload(const cJSON *p)
{
    static const char *const keys[]={"data","arguments","args","input","payload","parameters"};
    unsigned i;
    for (i=0; i<sizeof(keys)/sizeof(keys[0]); ++i) {
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(p,keys[i]);
        if ((!i && v) || cJSON_IsObject(v)) return v;
    }
    return p;
}
static const char *action(const cJSON *p, const cJSON *d)
{
    static const char *const keys[]={"action","name","tool","function"};
    unsigned i; const char *s;
    if (cJSON_GetObjectItemCaseSensitive(p,"action")) return string(p,"action");
    for (i=1; i<4; ++i) { s=string(p,keys[i]); if(s && *s) return s; }
    for (i=0; i<4; ++i) if(i!=1) { s=string(d,keys[i]); if(s && *s) return s; }
    return NULL;
}
static const cJSON *field(const cJSON *p, const cJSON *d, const char *act,
                          const char *const *keys, unsigned count)
{
    unsigned i;
    for (i=0; i<count; ++i) {
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(d,keys[i]);
        if (!v && p!=d) v=cJSON_GetObjectItemCaseSensitive(p,keys[i]);
        if (!strcmp(keys[i],"name") && cJSON_IsString(v) && v->valuestring==act &&
            v==cJSON_GetObjectItemCaseSensitive(p,"name")) continue;
        if (v) return v; /* Invalid canonical fields never fall back to aliases. */
    }
    return NULL;
}
static bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static bool read_id(const cJSON *root, ai_call_request_t *out)
{
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(root,"id");
    const char *text=NULL; size_t n;
    if (cJSON_IsString(id)) text=id->valuestring;
    else if (cJSON_IsNumber(id) && id->valuedouble>=-9007199254740991.0 &&
             id->valuedouble<=9007199254740991.0 &&
             (double)(int64_t)id->valuedouble==id->valuedouble) {
        int length=snprintf(out->rpc_id,sizeof(out->rpc_id),"%lld",(long long)(int64_t)id->valuedouble);
        if (length>0 && (size_t)length<sizeof(out->rpc_id)) { out->rpc_id_numeric=true; return true; }
        out->rpc_id[0]=0; return false;
    }
    n=text?strlen(text):0;
    if (n && n<sizeof(out->rpc_id)) {
        memcpy(out->rpc_id,text,n+1);
    }
    return out->rpc_id[0]!=0;
}
const char *ai_call_parse(const cJSON *root, ai_call_request_t *out)
{
    static const char *const targets[]={"target","target_name","target_device","target_device_id","target_id","device","device_id",
        "device_name","device_alias","contact","contact_name","name","alias","remark",
        "callee","callee_device_id","peer","peer_id","nickname","query"};
    static const char *const media_keys[]={"call_type","media","mode","room_type"};
    static const char *const type_keys[]={"contact_type","target_type","contact_source","route","channel","source"};
    static const char *const legacy_keys[]={"type"}, *const id_keys[]={"target_id"};
    const cJSON *p,*d,*t,*m,*r,*old_type,*expected;
    const char *version,*method,*act,*target,*media,*route,*old;
    bool wx,legacy; size_t n,i;
    memset(out,0,sizeof(*out));
    if (!cJSON_IsObject(root) || duplicate_keys(root)) return "invalid_params";
    if (!read_id(root,out)) return "invalid_params";
    p=cJSON_GetObjectItemCaseSensitive(root,"params"); d=payload(p); act=action(p,d);
    version=string(root,"jsonrpc"); method=string(root,"method");
    legacy=equal(method,"call_intent") || equal(method,"ai_call_intent");
    if (!legacy && !equal(method,"device_action")) return "invalid_params";
    if (legacy && !cJSON_GetObjectItemCaseSensitive(p,"action") &&
        !cJSON_GetObjectItemCaseSensitive(d,"action")) act="call_contact";
    if (!version || strcmp(version,"2.0") || !cJSON_IsObject(p) || !cJSON_IsObject(d) || !act) return "invalid_params";
    wx=equal(act,"call_wechat") || equal(act,"wechat_call") || equal(act,"call_wechat_contact") ||
       equal(act,"call_voip_contact") || equal(act,"voip_call") || equal(act,"start_voip_call");
    if (!wx && !equal(act,"call_contact") && !equal(act,"call_device") &&
        !equal(act,"device_call") && !equal(act,"start_device_call") && !equal(act,"call")) return "unsupported";
    t=field(p,d,act,targets,sizeof(targets)/sizeof(targets[0]));
    m=field(p,d,act,media_keys,4); r=field(p,d,act,type_keys,6);
    old_type=field(p,d,act,legacy_keys,1); old=cJSON_IsString(old_type)?old_type->valuestring:NULL;
    if (old_type && !old) return "invalid_params";
    /* ESP32 uses type for the contact route. Never let type=wechat override
     * an explicit media=audio (or type=audio override media=video). */
    if (!m && old_type && !scope(old)) m=old_type;
    target=cJSON_IsString(t)?t->valuestring:NULL;
    media=cJSON_IsString(m)?m->valuestring:NULL; route=cJSON_IsString(r)?r->valuestring:NULL;
    if (!target || (m && !media) || (r && !route)) return "invalid_params";
    out->video=!media || !*media || equal(media,"video");
    /* Legacy contact-type aliases also use this camera board's default. */
    if (scope(media)) out->video=true;
    if (media && *media && !out->video && !equal(media,"audio") && !equal(media,"voice") && !scope(media))
        return "unsupported_call_type";
    if ((!route || !*route) && scope(media)) route=media;
    if ((!route || !*route) && scope(old)) route=old;
    out->scope=wx?TIRTC_CONTACT_WECHAT:TIRTC_CONTACT_ANY;
    if (route && *route) {
        out->scope=scope(route);
        if (!out->scope || (wx && out->scope!=TIRTC_CONTACT_WECHAT)) return "invalid_params";
    }
    while (space(*target)) ++target;
    n=strlen(target); while(n && space(target[n-1])) --n;
    if (!n || n>=sizeof(out->target)) return "invalid_params";
    for (i=0; i<n; ++i) if ((unsigned char)target[i]<32 || (unsigned char)target[i]==127) return "invalid_params";
    memcpy(out->target,target,n); out->target[n]=0;
    expected=field(p,d,act,id_keys,1);
    if (expected) {
        const char *value=cJSON_IsString(expected)?expected->valuestring:NULL;
        n=value?strlen(value):0;
        if (!n || n>=sizeof(out->expected_id)) return "invalid_params";
        for(i=0;i<n;i++) if((unsigned char)value[i]<=32 || (unsigned char)value[i]==127) return "invalid_params";
        memcpy(out->expected_id,value,n+1);
    }
    return NULL;
}
char *ai_call_reply_json(const char *id, bool ok, const char *status,
                         const char *message, bool wechat, bool video, bool numeric_id)
{
    cJSON *root=cJSON_CreateObject(), *body, *details, *number=NULL; char *text=NULL;
    if (!root || !cJSON_AddStringToObject(root,"jsonrpc","2.0")) goto done;
    if (numeric_id) {
        number=cJSON_ParseWithOpts(id,NULL,true);
        /* cJSON's general double formatter may round a large integer RPC id.
         * The validated decimal text from read_id must remain exact. */
        if (!cJSON_IsNumber(number) || !cJSON_AddRawToObject(root,"id",id)) goto done;
    } else if (!cJSON_AddStringToObject(root,"id",id)) goto done;
    body=cJSON_AddObjectToObject(root,ok?"result":"error");
    if (!body || !cJSON_AddStringToObject(body,"message",message)) goto done;
    details=body;
    if (!ok) {
        if (!cJSON_AddNumberToObject(body,"code",-32000)) goto done;
        details=cJSON_AddObjectToObject(body,"data");
    }
    if (!details || !cJSON_AddBoolToObject(details,"ok",ok) || !cJSON_AddStringToObject(details,"status",status)) goto done;
    if (ok && (!cJSON_AddStringToObject(body,"contact_type",wechat?"wechat":"device") ||
        !cJSON_AddStringToObject(body,"call_type",video?"video":"audio"))) goto done;
    text=cJSON_PrintUnformatted(root);
done:
    cJSON_Delete(number); cJSON_Delete(root); return text;
}
