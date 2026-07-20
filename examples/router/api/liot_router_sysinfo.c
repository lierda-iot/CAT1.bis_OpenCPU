/**
 * @file api_sysinfo.c
 * @brief Web API - 系统基础信息查看(型号/IMEI/SN/固件版本/运行时长)
 */

#include "liot_router_http_server.h"
#include "liot_router_hal.h"
#include <stdio.h>
#include <string.h>

static int h_sysinfo(Liot_HttpCtx_t *ctx) {
    Liot_WebHalDevInfo_t info;
    Liot_WebHalDevGetInfo(&info);
    uint32_t up = Liot_WebHalUptimeMs() / 1000;

    char buf[384];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,"
        "\"model\":\"%s\",\"imei\":\"%s\",\"sn\":\"%s\","
        "\"fwVer\":\"%s\",\"productId\":\"%s\","
        "\"uptimeSec\":%u}",
        info.model, info.imei, info.sn, info.fwVer, info.productId, up);
    Liot_HttpSendJson(ctx->sock, buf);
    return 0;
}

void Liot_ApiSysinfoRegister(void) {
    Liot_HttpRouteRegister("/api/sysinfo", HTTP_METHOD_GET, 1, h_sysinfo);
}
