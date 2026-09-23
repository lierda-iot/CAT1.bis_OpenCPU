/**
 * @file api_network.c
 * @brief Web API - LAN链路状态、网络参数查看(WAN/LAN + DHCP 客户端)
 */

#include "liot_router_http_server.h"
#include "liot_router_hal.h"
#include "liot_router.h"
#include <stdio.h>
#include <string.h>

#define IP_FMT(ip) (unsigned)(((ip)>>24)&0xFF),(unsigned)(((ip)>>16)&0xFF),\
                   (unsigned)(((ip)>>8)&0xFF),(unsigned)((ip)&0xFF)

static int h_net_status(Liot_HttpCtx_t *ctx) {
    Liot_WebHalWanStatus_t wan;
    Liot_WebHalLanStatus_t lan;
    Liot_WebHalNetWanStatus(&wan);
    Liot_WebHalNetLanStatus(&lan);

    char buf[512];
    /* rssi 独立于 valid：未拿到 WAN IP 时也可能已注网、有信号 */
    int n = snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"wan\":{\"valid\":%d,\"rssi\":%d", wan.valid, (int)wan.rssi);
    if (wan.valid) {
        n += snprintf(buf + n, sizeof(buf) - n,
            ",\"ip\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\","
            "\"gw\":\"%u.%u.%u.%u\",\"dns1\":\"%u.%u.%u.%u\",\"dns2\":\"%u.%u.%u.%u\"",
            IP_FMT(wan.ip), IP_FMT(wan.mask), IP_FMT(wan.gw),
            IP_FMT(wan.dns1), IP_FMT(wan.dns2));
    }
    n += snprintf(buf + n, sizeof(buf) - n,
        "},\"lan\":{\"gw\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\","
        "\"linkUp\":%d,\"clients\":%d}}",
        IP_FMT(lan.gatewayIp), IP_FMT(lan.subnetMask),
        lan.linkUp, lan.clientCount);

    Liot_HttpSendJson(ctx->sock, buf);
    return 0;
}

/* DHCP 客户端列表(与旧实现一致的 lease 结构) */
static int h_clients(Liot_HttpCtx_t *ctx) {
    typedef struct { uint8_t mac[6]; uint8_t lan_port; uint8_t valid; uint32_t ip; uint32_t leaseStart; } Liot_Lease_t;
    uint8_t count = 0;
    const Liot_Lease_t *table = (const Liot_Lease_t *)liot_dhcp_get_lease_table(&count);

    char buf[1024];
    int off = snprintf(buf, sizeof(buf), "{\"ok\":true,\"clients\":[");
    int first = 1;
    for (uint8_t i = 0; i < count && (size_t)off < sizeof(buf) - 100; i++) {
        if (!table[i].valid) continue;
        if (!first) off += snprintf(buf + off, sizeof(buf) - off, ",");
        first = 0;
        off += snprintf(buf + off, sizeof(buf) - off,
            "{\"port\":%d,\"ip\":\"%u.%u.%u.%u\","
            "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
            table[i].lan_port,
            (unsigned)(table[i].ip&0xFF),(unsigned)((table[i].ip>>8)&0xFF),
            (unsigned)((table[i].ip>>16)&0xFF),(unsigned)((table[i].ip>>24)&0xFF),
            table[i].mac[0],table[i].mac[1],table[i].mac[2],
            table[i].mac[3],table[i].mac[4],table[i].mac[5]);
    }
    off += snprintf(buf + off, sizeof(buf) - off, "]}");
    Liot_HttpSendJson(ctx->sock, buf);
    return 0;
}

void Liot_ApiNetworkRegister(void) {
    Liot_HttpRouteRegister("/api/net/status",  HTTP_METHOD_GET, 1, h_net_status);
    Liot_HttpRouteRegister("/api/net/clients", HTTP_METHOD_GET, 1, h_clients);
}
