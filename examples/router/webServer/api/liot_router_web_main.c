/**
 * @file liot_router_web_main.c
 * @brief Web 管理服务入口 - 注册所有模块 + 静态资源 handler + 启动 httpd
 *
 * 新增功能模块只需：写 liot_router_xxx.c + 在 liot_router_modules.h 声明 + 此处调用其 register。
 */

#include "liot_router_http_server.h"
#include "api/liot_router_modules.h"
#include "liot_router_resource.h"
#include "lierda_app_main.h"
#include <string.h>

/* 静态资源：查表返回内嵌前端。gzipped=1 的才带 Content-Encoding: gzip，
 * 已压缩格式(png等)按原始字节直接发，不加编码头。 */
static int h_static(Liot_HttpCtx_t *ctx) {
    /* [DEBUG] 探针：能进到这里 = accept→收请求→路由分发 全链路通 */
    liot_trace("[web] h_static enter, path=%s sock=%d", ctx->path, ctx->sock);
    for (unsigned i = 0; i < gWebAssetCount; i++) {
        if (strcmp(gWebAssets[i].path, ctx->path) == 0) {
            const char *enc = gWebAssets[i].gzipped ? "gzip" : NULL;
            liot_trace("[web] h_static hit asset[%u], len=%u", i, gWebAssets[i].len);
            Liot_HttpSendAsset(ctx->sock, gWebAssets[i].mime, enc,
                            gWebAssets[i].data, gWebAssets[i].len);
            return 0;
        }
    }
    liot_trace("[web] h_static no match, send 404 for path=%s", ctx->path);
    Liot_HttpSendStatus(ctx->sock, 404, "not found");
    return 0;
}

void Liot_WebInit(void) {
    /* 业务 API 模块(自注册路由) */
    Liot_ApiAuthRegister();
    Liot_ApiSysinfoRegister();
    Liot_ApiNetworkRegister();
    Liot_ApiLanRegister();
    Liot_ApiSystemRegister();

    /* 静态资源路由：首页与前端文件(免认证，认证在 API 层) */
    liot_trace("[web] register %u static routes", gWebAssetCount);
    for (unsigned i = 0; i < gWebAssetCount; i++) {
        liot_trace("[web] route[%u] path=%s", i, gWebAssets[i].path);
        Liot_HttpRouteRegister(gWebAssets[i].path, HTTP_METHOD_GET, 0, h_static);
    }

    Liot_HttpdStart(80);
    liot_trace("liot web manager started");
}
