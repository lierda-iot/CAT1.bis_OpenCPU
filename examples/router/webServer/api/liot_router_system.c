/**
 * @file api_system.c
 * @brief Web API - 设备重启 / 恢复出厂设置 / 本地固件升级
 *
 * 固件升级采用流式：httpd 不缓冲大 body(超过阈值时 body=NULL 仅置 contentLen)，
 * 本处理器按 contentLen 从 socket 分片 recv → Liot_WebHalFwWrite → finish 校验。
 */

#include <stdbool.h>   /* lwip sockets.h 用到 bool，须在 liot_sockets.h 之前引入 */
#include "liot_router_http_server.h"
#include "liot_router_hal.h"
#include "liot_sockets.h"
#include "liot_os.h"
#include "cmsis_os2.h"
#include <string.h>

/* 延时复位：先让 HTTP 响应发出去，再复位。
 * gFwApply=1 走 FOTA 升级复位(Liot_WebHalFwApply)，否则普通重启。 */
static uint8_t gFwApply = 0;
static void reboot_task(void *arg) {
    (void)arg;
    liot_rtos_task_sleep_ms(800);
    if (gFwApply) Liot_WebHalFwApply();   /* 固件升级：正常会重启；若失败(校验错等)会返回 */
    else          Liot_WebHalReboot();     /* 普通重启/恢复出厂 */
    /* 关键：任务函数绝不能 return，否则触发 FreeRTOS prvTaskExitError 断言崩溃。
     * 升级失败/未重启时会走到这里，必须显式删除自身干净退出。 */
    liot_rtos_task_delete(NULL);
}
static void schedule_reboot(void) {
    static liot_task_t t = NULL;
    liot_rtos_task_create(&t, 2048, APP_PRIORITY_NORMAL, "web_reboot", &reboot_task, NULL);
}

static int h_reboot(Liot_HttpCtx_t *ctx) {
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"rebooting\"}");
    schedule_reboot();
    return 0;
}

static int h_factory(Liot_HttpCtx_t *ctx) {
    /* 删除失败则不重启、返回错误，避免用户误以为已清干净 */
    if (Liot_WebHalFactoryReset() != 0) {
        Liot_HttpSendStatus(ctx->sock, 500, "Factory reset failed, config may remain, please retry");
        return 0;
    }
    Liot_HttpSessionDestroyAll();
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Factory reset done, rebooting\"}");
    schedule_reboot();
    return 0;
}

static int h_upgrade(Liot_HttpCtx_t *ctx) {
    int total = ctx->contentLen;
    if (total <= 0) { Liot_HttpSendStatus(ctx->sock, 400, "empty firmware"); return 0; }

    if (Liot_WebHalFwBegin((uint32_t)total) != 0) {
        Liot_HttpSendStatus(ctx->sock, 500, "fw begin fail");
        return 0;
    }

    /* 若 httpd 已缓冲(小固件)，先写缓冲部分 */
    int written = 0;
    if (ctx->body && ctx->bodyLen > 0) {
        if (Liot_WebHalFwWrite(ctx->body, ctx->bodyLen) != 0) {
            Liot_WebHalFwAbort(); Liot_HttpSendStatus(ctx->sock, 500, "fw write fail"); return 0;
        }
        written = ctx->bodyLen;
    }

    /* 其余从 socket 流式读取 */
    uint8_t chunk[1024];
    while (written < total) {
        int want = total - written;
        if (want > (int)sizeof(chunk)) want = sizeof(chunk);
        int n = recv(ctx->sock, chunk, want, 0);
        if (n <= 0) break;
        if (Liot_WebHalFwWrite(chunk, n) != 0) {
            Liot_WebHalFwAbort(); Liot_HttpSendStatus(ctx->sock, 500, "fw write fail"); return 0;
        }
        written += n;
    }

    if (written != total) {
        Liot_WebHalFwAbort();
        Liot_HttpSendStatus(ctx->sock, 400, "incomplete upload");
        return 0;
    }

    /* 落地整包(fota2 的校验+升级绑定在 apply 里、且会重启，无法在回响应前拿到
     * 校验结果)。故此处只确认落地成功；校验在延时任务的 apply 内进行，失败则
     * 设备留在旧固件(有日志)，用户可重传。 */
    if (Liot_WebHalFwFinish(0) != 0) {
        Liot_HttpSendStatus(ctx->sock, 500, "fw save fail");
        return 0;
    }
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Firmware uploaded, verifying & upgrading on reboot\"}");
    gFwApply = 1;         /* 让延时任务调 Liot_WebHalFwApply() 触发升级 */
    schedule_reboot();
    return 0;
}

void Liot_ApiSystemRegister(void) {
    Liot_HttpRouteRegister("/api/system/reboot",  HTTP_METHOD_POST, 1, h_reboot);
    Liot_HttpRouteRegister("/api/system/factory", HTTP_METHOD_POST, 1, h_factory);
    Liot_HttpRouteRegister("/api/system/upgrade", HTTP_METHOD_POST, 1, h_upgrade);
}
