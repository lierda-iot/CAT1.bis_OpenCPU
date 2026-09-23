/**
 * @file api_lan.c
 * @brief Web API - LAN网关设置 / DHCP地址池配置 / 静态IP配置
 *
 * 复用 liot_router_nv 的持久化(gNvCfg)。配置写入后提示重启生效。
 */

#include "liot_router_http_server.h"
#include "liot_router_nv.h"
#include "liot_router.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* "a.b.c.d" → 主机序 uint32；严格校验：4段、每段0~255、无多余字符。
 * 非法返回 -1(调用方据此拒绝)。 */
static int parse_ip_strict(const char *s, uint32_t *out) {
    if (!s || !out) return -1;
    unsigned a, b, c, d;
    char extra;
    /* sscanf 多加个 %c：如果多余字符(如 1.2.3.4.5)则 extra 会被赋值，返回 5 而非 4 */
    int n = sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra);
    if (n != 4) return -1;   /* 段数不对 或 有多余内容 */
    if (a > 255 || b > 255 || c > 255 || d > 255) return -1;
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}
static int parse_mac(const char *s, uint8_t *mac) {
    unsigned m[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x",&m[0],&m[1],&m[2],&m[3],&m[4],&m[5]) != 6) return -1;
    for (int i=0;i<6;i++) mac[i]=(uint8_t)m[i];
    return 0;
}
/* 主机序展开：高字节在前 */
#define IP4(ip) (unsigned)(((ip)>>24)&0xFF),(unsigned)(((ip)>>16)&0xFF),\
                (unsigned)(((ip)>>8)&0xFF),(unsigned)((ip)&0xFF)
/* 网络序展开：低字节在前(用于 dns，NV 存网络序) */
#define IP4N(ip) (unsigned)((ip)&0xFF),(unsigned)(((ip)>>8)&0xFF),\
                 (unsigned)(((ip)>>16)&0xFF),(unsigned)(((ip)>>24)&0xFF)

static int h_config(Liot_HttpCtx_t *ctx) {
    const Liot_RouterNv_t *nv = Liot_RouterNvGet();
    char buf[2048];   /* 最坏 binds(8)+nat(16) 约1750B，留足余量防截断 */
    int off = snprintf(buf, sizeof(buf),
        "{\"ok\":true,"
        "\"gateway\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\","
        "\"poolStart\":\"%u.%u.%u.%u\",\"poolEnd\":\"%u.%u.%u.%u\","
        "\"dns1\":\"%u.%u.%u.%u\",\"dns2\":\"%u.%u.%u.%u\",\"binds\":[",
        IP4(nv->gatewayIp), IP4(nv->subnetMask),
        IP4(nv->poolStart), IP4(nv->poolEnd),
        IP4N(nv->dns1), IP4N(nv->dns2));
    for (uint8_t i=0;i<nv->staticBindCount && (size_t)off<sizeof(buf)-80;i++){
        if(i)off+=snprintf(buf+off,sizeof(buf)-off,",");
        off+=snprintf(buf+off,sizeof(buf)-off,
            "{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"ip\":\"%u.%u.%u.%u\"}",
            nv->staticBind[i].mac[0],nv->staticBind[i].mac[1],nv->staticBind[i].mac[2],
            nv->staticBind[i].mac[3],nv->staticBind[i].mac[4],nv->staticBind[i].mac[5],
            (unsigned)(nv->staticBind[i].ip&0xFF),(unsigned)((nv->staticBind[i].ip>>8)&0xFF),
            (unsigned)((nv->staticBind[i].ip>>16)&0xFF),(unsigned)((nv->staticBind[i].ip>>24)&0xFF));
    }
    off+=snprintf(buf+off,sizeof(buf)-off,"],\"nat\":[");
    for (uint8_t i=0;i<nv->natRuleCount && (size_t)off<sizeof(buf)-80;i++){
        if(i)off+=snprintf(buf+off,sizeof(buf)-off,",");
        off+=snprintf(buf+off,sizeof(buf)-off,
            "{\"proto\":%u,\"extPort\":%u,\"intIp\":\"%u.%u.%u.%u\",\"intPort\":%u}",
            nv->natRules[i].proto, nv->natRules[i].extPort,
            IP4N(nv->natRules[i].intIp), nv->natRules[i].intPort);
    }
    off+=snprintf(buf+off,sizeof(buf)-off,"]}");
    Liot_HttpSendJson(ctx->sock, buf);
    return 0;
}

static int h_gateway(Liot_HttpCtx_t *ctx) {
    char gw[20]={0}, mask[20]={0};
    if(!ctx->body||Liot_HttpParamGet(ctx->body,"gw",gw,sizeof(gw))!=0||
       Liot_HttpParamGet(ctx->body,"mask",mask,sizeof(mask))!=0){
        Liot_HttpSendStatus(ctx->sock,400,"missing param");return 0;
    }
    Liot_RouterNv_t nv; memcpy(&nv, Liot_RouterNvGet(), sizeof(nv));
    if(parse_ip_strict(gw,&nv.gatewayIp)!=0 || parse_ip_strict(mask,&nv.subnetMask)!=0){
        Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0;
    }
    Liot_RouterNvSet(&nv);
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Takes effect after reboot\"}");
    return 0;
}

static int h_dns(Liot_HttpCtx_t *ctx) {
    char d1[20]={0}, d2[20]={0};
    if(!ctx->body){ Liot_HttpSendStatus(ctx->sock,400,"missing param"); return 0; }
    /* 两个都可留空：空 = 0 = 使用运营商 DNS */
    Liot_HttpParamGet(ctx->body,"dns1",d1,sizeof(d1));
    Liot_HttpParamGet(ctx->body,"dns2",d2,sizeof(d2));
    Liot_RouterNv_t nv; memcpy(&nv, Liot_RouterNvGet(), sizeof(nv));
    /* 两个都可留空(0=用运营商)；非空则必须是合法 IP。NV 存网络序。 */
    uint32_t v1=0, v2=0;
    if(d1[0] && parse_ip_strict(d1,&v1)!=0){ Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0; }
    if(d2[0] && parse_ip_strict(d2,&v2)!=0){ Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0; }
    nv.dns1 = d1[0] ? liot_router_htonl(v1) : 0;
    nv.dns2 = d2[0] ? liot_router_htonl(v2) : 0;
    Liot_RouterNvSet(&nv);
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Takes effect after reboot\"}");
    return 0;
}

static int h_dhcp(Liot_HttpCtx_t *ctx) {
    char s[20]={0}, e[20]={0};
    if(!ctx->body||Liot_HttpParamGet(ctx->body,"start",s,sizeof(s))!=0||
       Liot_HttpParamGet(ctx->body,"end",e,sizeof(e))!=0){
        Liot_HttpSendStatus(ctx->sock,400,"missing param");return 0;
    }
    Liot_RouterNv_t nv; memcpy(&nv, Liot_RouterNvGet(), sizeof(nv));
    if(parse_ip_strict(s,&nv.poolStart)!=0 || parse_ip_strict(e,&nv.poolEnd)!=0){
        Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0;
    }
    Liot_RouterNvSet(&nv);
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Takes effect after reboot\"}");
    return 0;
}

static int h_bind(Liot_HttpCtx_t *ctx) {
    char act[8]={0}, mac[20]={0}, ip[20]={0};
    if(!ctx->body||Liot_HttpParamGet(ctx->body,"action",act,sizeof(act))!=0){
        Liot_HttpSendStatus(ctx->sock,400,"missing action");return 0;
    }
    Liot_RouterNv_t nv; memcpy(&nv, Liot_RouterNvGet(), sizeof(nv));

    if(strcmp(act,"add")==0){
        if(nv.staticBindCount>=LIOT_NV_MAX_STATIC_BIND){
            Liot_HttpSendStatus(ctx->sock,400,"binds full");return 0;
        }
        Liot_HttpParamGet(ctx->body,"mac",mac,sizeof(mac));
        Liot_HttpParamGet(ctx->body,"ip",ip,sizeof(ip));
        uint8_t idx=nv.staticBindCount;
        if(parse_mac(mac,nv.staticBind[idx].mac)!=0){
            Liot_HttpSendStatus(ctx->sock,400,"Invalid MAC address");return 0;
        }
        uint32_t bip;
        if(parse_ip_strict(ip,&bip)!=0){
            Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0;
        }
        nv.staticBind[idx].ip = liot_router_htonl(bip);
        nv.staticBindCount++;
    } else if(strcmp(act,"del")==0){
        Liot_HttpParamGet(ctx->body,"mac",mac,sizeof(mac));
        uint8_t m[6];
        if(parse_mac(mac,m)==0){
            for(uint8_t i=0;i<nv.staticBindCount;i++){
                if(memcmp(nv.staticBind[i].mac,m,6)==0){
                    for(uint8_t j=i;j<nv.staticBindCount-1;j++) nv.staticBind[j]=nv.staticBind[j+1];
                    nv.staticBindCount--; break;
                }
            }
        }
    }
    Liot_RouterNvSet(&nv);
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Takes effect after reboot\"}");
    return 0;
}

/* NAT 端口映射：action=add(proto/extPort/intIp/intPort) | del(extPort) */
static int h_nat(Liot_HttpCtx_t *ctx) {
    char act[8]={0};
    if(!ctx->body||Liot_HttpParamGet(ctx->body,"action",act,sizeof(act))!=0){
        Liot_HttpSendStatus(ctx->sock,400,"missing action");return 0;
    }
    Liot_RouterNv_t nv; memcpy(&nv, Liot_RouterNvGet(), sizeof(nv));

    if(strcmp(act,"add")==0){
        if(nv.natRuleCount>=LIOT_NV_MAX_NAT_RULES){
            Liot_HttpSendStatus(ctx->sock,400,"nat rules full");return 0;
        }
        char proto[6]={0}, ext[8]={0}, ip[20]={0}, in[8]={0};
        Liot_HttpParamGet(ctx->body,"proto",proto,sizeof(proto));
        Liot_HttpParamGet(ctx->body,"extPort",ext,sizeof(ext));
        Liot_HttpParamGet(ctx->body,"intIp",ip,sizeof(ip));
        Liot_HttpParamGet(ctx->body,"intPort",in,sizeof(in));
        /* 用 int 先校验范围，再转 uint16：否则 atoi 超 65535 强转会截断回绕 */
        int extV=atoi(ext), intV=atoi(in);
        if(extV<1||extV>65535||intV<1||intV>65535){
            Liot_HttpSendStatus(ctx->sock,400,"Port must be 1-65535");return 0;
        }
        uint32_t iip;
        if(parse_ip_strict(ip,&iip)!=0){
            Liot_HttpSendStatus(ctx->sock,400,"Invalid IP address");return 0;
        }
        uint16_t extPort=(uint16_t)extV, intPort=(uint16_t)intV;
        uint8_t idx=nv.natRuleCount;
        nv.natRules[idx].proto   = proto[0] ? (uint8_t)atoi(proto) : 6;  /* 默认 TCP */
        nv.natRules[idx].extPort = extPort;
        nv.natRules[idx].intIp   = liot_router_htonl(iip);  /* NV 存网络序 */
        nv.natRules[idx].intPort = intPort;
        nv.natRuleCount++;
    } else if(strcmp(act,"del")==0){
        char ext[8]={0};
        Liot_HttpParamGet(ctx->body,"extPort",ext,sizeof(ext));
        uint16_t extPort=(uint16_t)atoi(ext);
        for(uint8_t i=0;i<nv.natRuleCount;i++){
            if(nv.natRules[i].extPort==extPort){
                for(uint8_t j=i;j<nv.natRuleCount-1;j++) nv.natRules[j]=nv.natRules[j+1];
                nv.natRuleCount--; break;
            }
        }
    }
    Liot_RouterNvSet(&nv);
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Takes effect after reboot\"}");
    return 0;
}

void Liot_ApiLanRegister(void) {
    Liot_HttpRouteRegister("/api/lan/config",  HTTP_METHOD_GET,  1, h_config);
    Liot_HttpRouteRegister("/api/lan/gateway", HTTP_METHOD_POST, 1, h_gateway);
    Liot_HttpRouteRegister("/api/lan/dhcp",    HTTP_METHOD_POST, 1, h_dhcp);
    Liot_HttpRouteRegister("/api/lan/dns",     HTTP_METHOD_POST, 1, h_dns);
    Liot_HttpRouteRegister("/api/lan/bind",    HTTP_METHOD_POST, 1, h_bind);
    Liot_HttpRouteRegister("/api/lan/nat",     HTTP_METHOD_POST, 1, h_nat);
}
