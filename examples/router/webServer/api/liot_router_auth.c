/**
 * @file api_auth.c
 * @brief Web API - 账号密码登录认证 / 登录密码修改
 *
 * 凭据存储：{salt[16], sha256(salt+password)[32]} 持久化到 HAL KV("auth")。
 * 首次无凭据时使用默认账号密码，并在响应里提示需改密。
 * 登录成功签发 session token 写入 Cookie；改密后吊销全部会话。
 */

#include <stdbool.h>   /* lwip sockets.h 用到 bool，须在 liot_sockets.h 之前引入 */
#include "liot_router_http_server.h"
#include "liot_router_hal.h"
#include "liot_sockets.h"
#include <string.h>
#include <stdio.h>

#define AUTH_KV_KEY        "auth"
#define AUTH_DEF_USER      "admin"
#define AUTH_DEF_PASS      "admin123"
#define AUTH_SALT_LEN      16
#define AUTH_USER_MAX      32
#define AUTH_PASS_MAX      64

typedef struct {
    uint32_t magic;                 /* 0x41555448 "AUTH" */
    char     user[AUTH_USER_MAX];
    uint8_t  salt[AUTH_SALT_LEN];
    uint8_t  hash[32];
} Liot_AuthCred_t;

#define AUTH_MAGIC   0x41555448

/* 计算 sha256(salt || password) */
static int calc_hash(const uint8_t *salt, const char *pass, uint8_t out[32]) {
    uint8_t buf[AUTH_SALT_LEN + AUTH_PASS_MAX];
    size_t plen = strlen(pass);
    if (plen > AUTH_PASS_MAX) plen = AUTH_PASS_MAX;
    memcpy(buf, salt, AUTH_SALT_LEN);
    memcpy(buf + AUTH_SALT_LEN, pass, plen);
    return Liot_WebHalSha256(buf, AUTH_SALT_LEN + plen, out);
}

/* 读取凭据；无有效凭据则用默认账号密码构造(标记 isDefault) */
static int load_cred(Liot_AuthCred_t *c, int *isDefault) {
    if (Liot_WebHalKvRead(AUTH_KV_KEY, c, sizeof(*c)) == (int)sizeof(*c) &&
        c->magic == AUTH_MAGIC) {
        if (isDefault) *isDefault = 0;
        return 0;
    }
    /* 默认凭据 */
    memset(c, 0, sizeof(*c));
    c->magic = AUTH_MAGIC;
    strncpy(c->user, AUTH_DEF_USER, AUTH_USER_MAX - 1);
    memset(c->salt, 0xA5, AUTH_SALT_LEN);   /* 默认固定盐，仅默认口令用 */
    calc_hash(c->salt, AUTH_DEF_PASS, c->hash);
    if (isDefault) *isDefault = 1;
    return 0;
}

static int save_cred(const char *user, const char *pass) {
    Liot_AuthCred_t c;
    memset(&c, 0, sizeof(c));
    c.magic = AUTH_MAGIC;
    strncpy(c.user, user, AUTH_USER_MAX - 1);
    if (Liot_WebHalRandom(c.salt, AUTH_SALT_LEN) != 0) return -1;
    if (calc_hash(c.salt, pass, c.hash) != 0) return -1;
    return Liot_WebHalKvWrite(AUTH_KV_KEY, &c, sizeof(c));
}

static int verify(const char *user, const char *pass, int *isDefault) {
    Liot_AuthCred_t c;
    load_cred(&c, isDefault);
    if (strncmp(c.user, user, AUTH_USER_MAX) != 0) return 0;
    uint8_t h[32];
    if (calc_hash(c.salt, pass, h) != 0) return 0;
    return memcmp(h, c.hash, 32) == 0;
}

/* ───────── handlers ───────── */

static int h_login(Liot_HttpCtx_t *ctx) {
    char user[AUTH_USER_MAX] = {0}, pass[AUTH_PASS_MAX] = {0};
    if (!ctx->body ||
        Liot_HttpParamGet(ctx->body, "user", user, sizeof(user)) != 0 ||
        Liot_HttpParamGet(ctx->body, "pass", pass, sizeof(pass)) != 0) {
        Liot_HttpSendStatus(ctx->sock, 400, "missing user/pass");
        return 0;
    }

    int isDefault = 0;
    if (!verify(user, pass, &isDefault)) {
        Liot_HttpSendStatus(ctx->sock, 401, "invalid credentials");
        return 0;
    }

    char token[HTTP_SESSION_TOKEN_LEN + 1];
    if (Liot_HttpSessionCreate(token, sizeof(token)) != 0) {
        Liot_HttpSendStatus(ctx->sock, 500, "session alloc fail");
        return 0;
    }

    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":true,\"mustChangePwd\":%s}",
             isDefault ? "true" : "false");

    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Set-Cookie: SID=%s; Path=/; HttpOnly\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n", token, (int)strlen(body));
    send(ctx->sock, hdr, (size_t)n, 0);
    send(ctx->sock, body, strlen(body), 0);
    return 0;
}

static int h_logout(Liot_HttpCtx_t *ctx) {
    /* 从 Cookie 拿 SID 销毁；简单起见清全部当前会话由 destroy_all 处理更稳妥，
     * 这里只销毁本次(需要 SID)。因中间件已置 authed，直接吊销全部亦可接受。 */
    Liot_HttpSessionDestroyAll();
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true}");
    return 0;
}

static int h_change_pwd(Liot_HttpCtx_t *ctx) {
    char oldp[AUTH_PASS_MAX] = {0}, newp[AUTH_PASS_MAX] = {0};
    char user[AUTH_USER_MAX] = {0};
    if (!ctx->body ||
        Liot_HttpParamGet(ctx->body, "old", oldp, sizeof(oldp)) != 0 ||
        Liot_HttpParamGet(ctx->body, "new", newp, sizeof(newp)) != 0) {
        Liot_HttpSendStatus(ctx->sock, 400, "Please fill in both passwords");
        return 0;
    }
    if (strlen(newp) < 6) {
        Liot_HttpSendStatus(ctx->sock, 400, "New password must be at least 6 characters");
        return 0;
    }

    /* 用现有用户名校验旧密码 */
    Liot_AuthCred_t c; int isDef = 0;
    load_cred(&c, &isDef);
    strncpy(user, c.user, sizeof(user) - 1);
    if (!verify(user, oldp, NULL)) {
        /* 用 400 而非 401：能到此说明会话有效，这是"旧密码错"的业务校验失败，
         * 不是"未认证"。用 401 会被前端误判为会话失效而跳登录页。 */
        Liot_HttpSendStatus(ctx->sock, 400, "Old password incorrect");
        return 0;
    }

    if (save_cred(user, newp) != 0) {
        Liot_HttpSendStatus(ctx->sock, 500, "Save failed");
        return 0;
    }
    Liot_HttpSessionDestroyAll();   /* 改密后强制重新登录 */
    Liot_HttpSendJson(ctx->sock, "{\"ok\":true,\"msg\":\"Password changed, please log in again\"}");
    return 0;
}

void Liot_ApiAuthRegister(void) {
    Liot_HttpRouteRegister("/api/login",   HTTP_METHOD_POST, 0, h_login);
    Liot_HttpRouteRegister("/api/logout",  HTTP_METHOD_POST, 1, h_logout);
    Liot_HttpRouteRegister("/api/passwd",  HTTP_METHOD_POST, 1, h_change_pwd);
}
