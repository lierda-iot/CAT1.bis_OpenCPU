/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai_http_token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "ai_app_log.h"
#include "liot_http.h"
#include "liot_os.h"
#include "liot_rtc.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"

#ifdef FEATURE_HTTP_TLS_ENABLE
#include "liot_ssl.h"
#endif

#define AI_HTTP_RESPONSE_MAX 8192
#define AI_HTTP_WAIT_STEP_MS 1000
#define AI_HTTP_RESPONSE_TIMEOUT_MS 60000
#define AI_HTTP_CLOSE_TIMEOUT_MS 5000
#define AI_HTTP_MD5_HEX_LEN 32
#define AI_HTTP_AUTHOR_TOKEN_MAX 256

typedef struct {
    const char *request_body;
    int request_len;
    int request_offset;

    char response[AI_HTTP_RESPONSE_MAX];
    int response_len;

    liot_sem_t sem;
    bool done;
    bool closed;
    int event_result;
    int http_status;
} ai_http_ctx_t;

static int ai_http_wait_until(ai_http_ctx_t *ctx, bool *flag, int timeout_ms)
{
    int waited_ms = 0;

    while (!(*flag) && waited_ms < timeout_ms) {
        liot_rtos_semaphore_wait(ctx->sem, AI_HTTP_WAIT_STEP_MS);
        waited_ms += AI_HTTP_WAIT_STEP_MS;
    }

    return (*flag) ? 0 : -1;
}

static void ai_http_signal(ai_http_ctx_t *ctx)
{
    if ((ctx != NULL) && (ctx->sem != NULL)) {
        liot_rtos_semaphore_release(ctx->sem);
    }
}

static void ai_http_event_cb(liot_http_client_t *client, int evt, int evt_code, void *arg)
{
    ai_http_ctx_t *ctx = (ai_http_ctx_t *)arg;

    switch (evt) {
    case LIOT_HTTPC_SESSION_OPEN:
        if (evt_code != LIOT_HTTPC_SUCCESS) {
            liot_trace("ai_http session open failed evt_code=%d; HTTP TLS/connect stage", evt_code);
            if (ctx != NULL) {
                ctx->event_result = evt_code;
                ctx->done = true;
                ai_http_signal(ctx);
            }
        }
        break;

    case LIOT_HTTPC_UPLOAD_START:
        liot_httpc_user_notify(client, LIOT_HTTPC_READ);
        break;

    case LIOT_HTTPC_RESPONSE_STATUS:
        if ((ctx != NULL) && (evt_code == LIOT_HTTPC_SUCCESS)) {
            int status_code = 0;
            liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &status_code);
            ctx->http_status = status_code;
            liot_trace("ai_http response status=%d", status_code);
        } else {
            liot_trace("ai_http response status event failed evt_code=%d", evt_code);
        }
        break;

    case LIOT_HTTPC_RESPONSE_COMPLETE:
        if (ctx != NULL) {
            ctx->event_result = evt_code;
            ctx->done = true;
            ai_http_signal(ctx);
        }
        liot_trace("ai_http response complete evt_code=%d body_len=%d",
                   evt_code,
                   (ctx != NULL) ? ctx->response_len : 0);
        break;

    case LIOT_HTTPC_RESPONSE_TIMEOUT:
        if (ctx != NULL) {
            ctx->event_result = evt_code;
            ctx->done = true;
            ai_http_signal(ctx);
        }
        liot_trace("ai_http response timeout evt_code=%d", evt_code);
        break;

    case LIOT_HTTPC_SESSION_CLOSE:
        if (ctx != NULL) {
            ctx->closed = true;
            ai_http_signal(ctx);
        }
        liot_trace("ai_http session close evt_code=%d", evt_code);
        break;

    default:
        break;
    }
}

static int ai_http_response_write_cb(liot_http_client_t *client,
                                     void *arg,
                                     char *data,
                                     int size,
                                     unsigned char end)
{
    ai_http_ctx_t *ctx = (ai_http_ctx_t *)arg;
    (void)client;
    (void)end;

    if ((ctx == NULL) || (data == NULL) || (size <= 0)) {
        return 0;
    }

    if ((ctx->response_len + size) >= AI_HTTP_RESPONSE_MAX) {
        liot_trace("ai_http response buffer full, dropping %d bytes", size);
        return size;
    }

    memcpy(ctx->response + ctx->response_len, data, size);
    ctx->response_len += size;
    ctx->response[ctx->response_len] = '\0';

    return size;
}

static int ai_http_request_read_cb(liot_http_client_t *client, void *arg, char *data, int size)
{
    ai_http_ctx_t *ctx = (ai_http_ctx_t *)arg;
    int remain = 0;
    int copy_len = 0;
    (void)client;

    if ((ctx == NULL) || (data == NULL) || (size <= 0)) {
        return 0;
    }

    remain = ctx->request_len - ctx->request_offset;
    if (remain <= 0) {
        return 0;
    }

    copy_len = (remain < size) ? remain : size;
    memcpy(data, ctx->request_body + ctx->request_offset, copy_len);
    ctx->request_offset += copy_len;

    return copy_len;
}

static int ai_http_build_md5_hex(const char *plain, char *out, size_t out_size)
{
    unsigned char digest[16] = {0};
    static const char hex[] = "0123456789abcdef";

    if ((plain == NULL) || (out == NULL) || (out_size < (AI_HTTP_MD5_HEX_LEN + 1U))) {
        return -1;
    }

    if (mbedtls_md5_ret((const unsigned char *)plain, strlen(plain), digest) != 0) {
        return -2;
    }

    for (int i = 0; i < 16; i++) {
        out[(i * 2)] = hex[(digest[i] >> 4) & 0x0F];
        out[(i * 2) + 1] = hex[digest[i] & 0x0F];
    }
    out[AI_HTTP_MD5_HEX_LEN] = '\0';

    return 0;
}

static int ai_http_build_author_token(const char *plain, char *out, size_t out_size)
{
    size_t olen = 0;
    int ret = 0;

    if ((plain == NULL) || (out == NULL) || (out_size == 0U)) {
        return -1;
    }

    ret = mbedtls_base64_encode((unsigned char *)out,
                                out_size - 1U,
                                &olen,
                                (const unsigned char *)plain,
                                strlen(plain));
    if (ret != 0) {
        return -2;
    }

    out[olen] = '\0';
    return 0;
}

static char *ai_http_build_request_body(const ai_app_config_t *cfg)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    char *json = NULL;

    root = cJSON_CreateObject();
    data = cJSON_CreateObject();
    if ((root == NULL) || (data == NULL)) {
        goto exit;
    }

    if (!cJSON_AddStringToObject(root, "deviceType", cfg->device_type) ||
        !cJSON_AddStringToObject(root, "deviceId", cfg->device_id) ||
        !cJSON_AddStringToObject(root, "aiSdkVer", cfg->ai_sdk_ver) ||
        !cJSON_AddItemToObject(root, "data", data)) {
        goto exit;
    }
    data = NULL;

    json = cJSON_PrintUnformatted(root);

exit:
    if (data != NULL) {
        cJSON_Delete(data);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return json;
}

static int ai_http_get_timestamp_string(char *out, size_t out_size)
{
    uint64_t timestamp = 0;
    uint32_t hi = 0;
    uint32_t lo = 0;

    if ((out == NULL) || (out_size == 0U)) {
        return -1;
    }

    if (Liot_GetTimestamp(&timestamp) != 0 || timestamp == 0ULL) {
        liot_trace("ai_http timestamp unavailable from RTC");
        return -1;
    }

    hi = (uint32_t)(timestamp / 1000000000ULL);
    lo = (uint32_t)(timestamp % 1000000000ULL);
    if (hi > 0) {
        snprintf(out, out_size, "%u%09u", (unsigned)hi, (unsigned)lo);
    } else {
        snprintf(out, out_size, "%u", (unsigned)lo);
    }

    return 0;
}


static const cJSON *ai_http_get_object_item(const cJSON *object, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static int ai_http_json_int(const cJSON *item, int default_value)
{
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return atoi(item->valuestring);
    }
    return default_value;
}

static uint64_t ai_http_json_u64(const cJSON *item)
{
    if (cJSON_IsNumber(item)) {
        return (uint64_t)item->valuedouble;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return (uint64_t)strtoull(item->valuestring, NULL, 10);
    }
    return 0ULL;
}

static void ai_http_copy_json_string(const cJSON *item, char *out, size_t out_size)
{
    if ((out == NULL) || (out_size == 0U)) {
        return;
    }

    out[0] = '\0';
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        snprintf(out, out_size, "%s", item->valuestring);
    }
}

static int ai_http_parse_response(const ai_http_ctx_t *ctx, ai_http_token_result_t *out)
{
    cJSON *root = NULL;
    const cJSON *resp_code_item = NULL;
    const cJSON *ai_data = NULL;
    const cJSON *token = NULL;

    if ((ctx == NULL) || (out == NULL) || (ctx->response_len <= 0)) {
        return -1;
    }

    root = cJSON_ParseWithLength(ctx->response, (size_t)ctx->response_len);
    if (root == NULL) {
        liot_trace("ai_http JSON parse failed raw=%.*s", ctx->response_len, ctx->response);
        return -2;
    }

    out->http_status = ctx->http_status;
    resp_code_item = ai_http_get_object_item(root, "respCode");
    if (resp_code_item == NULL) {
        resp_code_item = ai_http_get_object_item(root, "code");
    }
    out->resp_code = ai_http_json_int(resp_code_item, -1);

    liot_trace("ai_http business respCode=%d body=%.*s", out->resp_code, ctx->response_len, ctx->response);
    if (out->resp_code != 0) {
        cJSON_Delete(root);
        return -3;
    }

    ai_data = ai_http_get_object_item(root, "aiData");
    if (ai_data == NULL) {
        ai_data = ai_http_get_object_item(root, "data");
    }
    if (!cJSON_IsObject(ai_data)) {
        liot_trace("ai_http response missing aiData");
        cJSON_Delete(root);
        return -4;
    }

    token = ai_http_get_object_item(ai_data, "aiToken");
    if (!cJSON_IsString(token) || token->valuestring == NULL || token->valuestring[0] == '\0') {
        liot_trace("ai_http response missing aiToken");
        cJSON_Delete(root);
        return -5;
    }

    ai_http_copy_json_string(token, out->ai_token, sizeof(out->ai_token));
    ai_http_copy_json_string(ai_http_get_object_item(ai_data, "aiUrl"),
                             out->ai_url,
                             sizeof(out->ai_url));
    out->server_timestamp = ai_http_json_u64(ai_http_get_object_item(root, "timestamp"));
    ai_http_copy_json_string(ai_http_get_object_item(root, "timezone"),
                             out->timezone,
                             sizeof(out->timezone));

    liot_trace("ai_http token request success token_len=%d aiUrl_len=%d",
               (int)strlen(out->ai_token),
               (int)strlen(out->ai_url));

    cJSON_Delete(root);
    return 0;
}

typedef struct {
    char all_headers[512];
} ai_http_headers_t;

static int ai_http_configure_client(liot_http_client_t *client,
                                    const ai_app_config_t *cfg,
                                    ai_http_ctx_t *ctx,
                                    liot_httpc_url_s *url,
                                    ai_http_headers_t *hdrs)
{
    char sig_hex[AI_HTTP_MD5_HEX_LEN + 1] = {0};
    char author_token[AI_HTTP_AUTHOR_TOKEN_MAX] = {0};
    char timestamp[32] = {0};
    char sig_plain[512] = {0};
    char author_plain[192] = {0};
    int n = 0;

    if (!liot_httpc_url_parse((char *)cfg->http_url, url)) {
        liot_trace("ai_http URL parse failed");
        return -1;
    }

    if (ai_http_get_timestamp_string(timestamp, sizeof(timestamp)) != 0) {
        liot_trace("ai_http timestamp failed");
        return -2;
    }
    liot_trace("ai_http sign timestamp=%s", timestamp);

    n = snprintf(sig_plain, sizeof(sig_plain), "%s%s%s%s",
                 cfg->device_id, cfg->device_type, cfg->auth_secret_key, timestamp);
    if ((n < 0) || ((size_t)n >= sizeof(sig_plain))) {
        return -3;
    }

    if (ai_http_build_md5_hex(sig_plain, sig_hex, sizeof(sig_hex)) != 0) {
        return -4;
    }

    n = snprintf(author_plain, sizeof(author_plain), "%s:%s", cfg->device_type, timestamp);
    if ((n < 0) || ((size_t)n >= sizeof(author_plain))) {
        return -5;
    }

    if (ai_http_build_author_token(author_plain, author_token, sizeof(author_token)) != 0) {
        return -6;
    }

    n = snprintf(hdrs->all_headers, sizeof(hdrs->all_headers),
                 "Content-type: application/json\r\n"
                 "Accept-Charset: utf-8\r\n"
                 "Sig: %s\r\n"
                 "AuthorToken: %s",
                 sig_hex, author_token);
    if ((n < 0) || ((size_t)n >= sizeof(hdrs->all_headers))) {
        return -7;
    }

    liot_trace("ai_http headers=[%s]", hdrs->all_headers);

    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SIM_ID, cfg->sim_id);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_PDPCID, cfg->pdp_cid);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_POST);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_URL, url);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, ai_http_response_write_cb);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ctx);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_READ_FUNC, ai_http_request_read_cb);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_READ_DATA, ctx);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN, ctx->request_len);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE, LIOT_HTTPC_RAW_DATA);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_RAW_REQUEST, 0);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, hdrs->all_headers);

#ifdef FEATURE_HTTP_TLS_ENABLE
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSLCTXID, 1);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERSION, LIOT_SSL_VERSION_3);
    liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT, 300);
    if (cfg->http_ssl_verify_none) {
        liot_httpc_setopt(client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_NONE);
        liot_trace("ai_http TLS verify level: none");
    }
#endif

    return 0;
}

int ai_http_fetch_token(const ai_app_config_t *cfg, ai_http_token_result_t *out)
{
    ai_http_ctx_t ctx;
    ai_http_headers_t hdrs;
    liot_http_client_t client = 0;
    liot_httpc_url_s url;
    char *json_body = NULL;
    char request_buf[256];
    int ret = -1;
    int perform_ret = 0;

    if ((cfg == NULL) || (out == NULL)) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    memset(&hdrs, 0, sizeof(hdrs));
    memset(&url, 0, sizeof(url));
    memset(out, 0, sizeof(*out));

    json_body = ai_http_build_request_body(cfg);
    if (json_body == NULL) {
        liot_trace("ai_http request body build failed");
        return -2;
    }

    memset(request_buf, 0, sizeof(request_buf));
    snprintf(request_buf, sizeof(request_buf), "%s", json_body);
    cJSON_free(json_body);
    json_body = NULL;

    ctx.request_body = request_buf;
    ctx.request_len = (int)strlen(request_buf);

    liot_trace("ai_http token request start url=%s body_len=%d",
               cfg->http_url,
               ctx.request_len);

    if (liot_rtos_semaphore_create(&ctx.sem, 0) != LIOT_OSI_SUCCESS) {
        liot_trace("ai_http semaphore create failed");
        ret = -3;
        goto exit;
    }

    if (liot_httpc_new(&client, ai_http_event_cb, &ctx) != LIOT_HTTPC_SUCCESS) {
        liot_trace("ai_http client create failed");
        ret = -4;
        goto exit;
    }

    ret = ai_http_configure_client(&client, cfg, &ctx, &url, &hdrs);
    if (ret != 0) {
        goto exit;
    }

    perform_ret = liot_httpc_perform(&client);
    if (perform_ret != LIOT_HTTPC_SUCCESS) {
        liot_trace("ai_http perform failed ret=%d; HTTP TLS/connect stage", perform_ret);
        ret = -5;
        goto exit;
    }

    if (ai_http_wait_until(&ctx, &ctx.done, AI_HTTP_RESPONSE_TIMEOUT_MS) != 0) {
        liot_trace("ai_http response wait timeout; HTTP network/TLS/server stage");
        ret = -6;
        goto exit;
    }

    if (ctx.event_result != LIOT_HTTPC_SUCCESS) {
        liot_trace("ai_http transfer failed evt_code=%d", ctx.event_result);
        ret = -7;
        goto exit;
    }

    if ((ctx.http_status < 200) || (ctx.http_status >= 300)) {
        liot_trace("ai_http HTTP status failure status=%d", ctx.http_status);
        ret = -8;
        goto exit;
    }

    ret = ai_http_parse_response(&ctx, out);

exit:
    liot_trace("ai_http cleanup start ret=%d", ret);
    if (client != 0) {
        liot_httpc_stop(&client);
        if (ctx.sem != NULL && !ctx.closed) {
            ai_http_wait_until(&ctx, &ctx.closed, AI_HTTP_CLOSE_TIMEOUT_MS);
        }
        liot_rtos_task_sleep_s(1);
        liot_httpc_release(&client);
    }
    liot_trace("ai_http cleanup done ret=%d", ret);

    return ret;
}
