#include "xiaozhi_core.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_os.h"
#ifdef FEATURE_HTTP_TLS_ENABLE
#include "liot_ssl.h"
#endif

#define XZ_HTTP_BODY_MAX 8192

typedef struct {
    const char *upload;
    int upload_len;
    int upload_pos;
    char body[XZ_HTTP_BODY_MAX];
    int body_len;
    int status;
    int result;
    volatile bool done;
    volatile bool closed;
    liot_sem_t sem;
} xz_http_ctx_t;

/* The demo entry task has a 10 KB stack. Keep HTTP buffers out of that stack. */
static xz_http_ctx_t g_xz_http_ctx;
static char g_xz_ota_response[XZ_HTTP_BODY_MAX];

static void xz_http_signal(xz_http_ctx_t *ctx)
{
    if (ctx != NULL && ctx->sem != NULL) liot_rtos_semaphore_release(ctx->sem);
}

static void xz_http_event(liot_http_client_t *client, int event, int code, void *arg)
{
    xz_http_ctx_t *ctx = (xz_http_ctx_t *)arg;
    if (ctx == NULL) return;
    liot_trace("[xiaozhi] HTTP event=%d code=%d", event, code);
    if (event == LIOT_HTTPC_UPLOAD_START) {
        liot_httpc_user_notify(client, LIOT_HTTPC_READ);
    } else if (event == LIOT_HTTPC_RESPONSE_STATUS && code == LIOT_HTTPC_SUCCESS) {
        liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &ctx->status);
    } else if (event == LIOT_HTTPC_RESPONSE_COMPLETE || event == LIOT_HTTPC_RESPONSE_TIMEOUT) {
        ctx->result = code;
        ctx->done = true;
        xz_http_signal(ctx);
    } else if (event == LIOT_HTTPC_SESSION_OPEN && code != LIOT_HTTPC_SUCCESS) {
        ctx->result = code;
        ctx->done = true;
        xz_http_signal(ctx);
    } else if (event == LIOT_HTTPC_SESSION_CLOSE) {
        ctx->closed = true;
        xz_http_signal(ctx);
    }
}

static int xz_http_write(liot_http_client_t *client, void *arg, char *data, int size,
                         unsigned char end)
{
    xz_http_ctx_t *ctx = (xz_http_ctx_t *)arg;
    (void)client;
    (void)end;
    if (ctx == NULL || data == NULL || size <= 0) return 0;
    if (ctx->body_len + size >= XZ_HTTP_BODY_MAX) return size;
    memcpy(ctx->body + ctx->body_len, data, size);
    ctx->body_len += size;
    ctx->body[ctx->body_len] = '\0';
    return size;
}

static int xz_http_read(liot_http_client_t *client, void *arg, char *data, int size)
{
    xz_http_ctx_t *ctx = (xz_http_ctx_t *)arg;
    int remain;
    (void)client;
    if (ctx == NULL || data == NULL || size <= 0) return 0;
    remain = ctx->upload_len - ctx->upload_pos;
    if (remain <= 0) return 0;
    if (size > remain) size = remain;
    memcpy(data, ctx->upload + ctx->upload_pos, size);
    ctx->upload_pos += size;
    return size;
}

static int xz_http_post(const char *url, const char *headers, const char *body,
                        char *response, int response_size, int *status)
{
    liot_http_client_t client = 0;
    liot_httpc_url_s parsed;
    xz_http_ctx_t *ctx = &g_xz_http_ctx;
    int ret = -1;
    int waited;
    int header_ret;
    int raw_ret;

    memset(ctx, 0, sizeof(*ctx));
    memset(&parsed, 0, sizeof(parsed));
    ctx->upload = body;
    ctx->upload_len = (int)strlen(body);
    if (!liot_httpc_url_parse((char *)url, &parsed)) {
        liot_trace("[xiaozhi] HTTP URL parse failed url=%s", url);
        return -1;
    }
    if (liot_rtos_semaphore_create(&ctx->sem, 0) != LIOT_OSI_SUCCESS) {
        liot_trace("[xiaozhi] HTTP semaphore create failed");
        return -2;
    }
    ret = liot_httpc_new(&client, xz_http_event, ctx);
    if (ret != LIOT_HTTPC_SUCCESS) {
        liot_trace("[xiaozhi] HTTP client create failed ret=%d", ret);
        ret = -3;
        goto exit;
    }

    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SIM_ID, 0);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_PDPCID, 1);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_POST);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_URL, &parsed);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, xz_http_write);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ctx);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_READ_FUNC, xz_http_read);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_READ_DATA, ctx);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN, ctx->upload_len);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE, LIOT_HTTPC_RAW_DATA);
    raw_ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_RAW_REQUEST, 0);
    header_ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER,
                                   (char *)headers);
    liot_trace("[xiaozhi] HTTP setopt raw=%d headers=%d upload_len=%d",
               raw_ret, header_ret, ctx->upload_len);
#ifdef FEATURE_HTTP_TLS_ENABLE
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSLCTXID, 3);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_VERSION, LIOT_SSL_VERSION_3);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT, 300);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_NONE);
#endif
    ret = liot_httpc_perform(&client);
    liot_trace("[xiaozhi] HTTP perform ret=%d", ret);
    if (ret != LIOT_HTTPC_SUCCESS) {
        ret = -4;
        goto exit;
    }
    for (waited = 0; !ctx->done && waited < 60000; waited += 1000)
        liot_rtos_semaphore_wait(ctx->sem, 1000);
    liot_trace("[xiaozhi] HTTP done=%d result=%d status=%d body_len=%d",
               ctx->done, ctx->result, ctx->status, ctx->body_len);
    if (!ctx->done || ctx->result != LIOT_HTTPC_SUCCESS) {
        ret = -5;
        goto exit;
    }
    if (response != NULL && response_size > 0) {
        int copy_len = ctx->body_len;
        if (copy_len >= response_size) copy_len = response_size - 1;
        memcpy(response, ctx->body, copy_len);
        response[copy_len] = '\0';
    }
    if (status != NULL) *status = ctx->status;
    ret = 0;
exit:
    if (client != 0) {
        liot_httpc_stop(&client);
        liot_rtos_task_sleep_ms(200);
        liot_httpc_release(&client);
    }
    if (ctx->sem != NULL) {
        liot_rtos_semaphore_delete(ctx->sem);
        ctx->sem = NULL;
    }
    liot_trace("[xiaozhi] HTTP request exit ret=%d", ret);
    return ret;
}

static void xz_copy_json(cJSON *object, const char *key, char *out, size_t size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsString(item) && item->valuestring != NULL)
        snprintf(out, size, "%s", item->valuestring);
}

int xiaozhi_ota_check(xiaozhi_config_t *config)
{
    char headers[512];
    char request[768];
    char *response = g_xz_ota_response;
    int status = 0;
    int ret;
    cJSON *root;
    cJSON *activation;
    cJSON *websocket;
    cJSON *version;

    snprintf(headers, sizeof(headers),
             "Content-type: application/json\r\n"
             "Activation-Version: 1\r\nDevice-Id: %s\r\nClient-Id: %s\r\n"
             "User-Agent: L_CT4IT02_1698W/demo_xiaozhi-0.1\r\n"
             "Accept-Language: zh-CN",
             config->device_id, config->client_id);
    snprintf(request, sizeof(request),
             "{\"version\":2,\"language\":\"zh-CN\",\"flash_size\":0,"
             "\"minimum_free_heap_size\":\"0\",\"mac_address\":\"%s\","
             "\"uuid\":\"%s\",\"chip_model_name\":\"NT26F9D0\","
             "\"application\":{\"name\":\"demo_xiaozhi\",\"version\":\"0.1\"},"
             "\"board\":{\"type\":\"L_CT4IT02_1698W\"}}",
             config->device_id, config->client_id);
    ret = xz_http_post(XZ_OTA_URL, headers, request, response, XZ_HTTP_BODY_MAX, &status);
    liot_trace("[xiaozhi] OTA HTTP ret=%d status=%d body=%.*s", ret, status,
               256, response);
    if (ret != 0 || status != 200)
        return -1;
    root = cJSON_Parse(response);
    if (root == NULL) return -2;
    memset(config->activation_code, 0, sizeof(config->activation_code));
    config->activation_required = false;
    activation = cJSON_GetObjectItemCaseSensitive(root, "activation");
    if (cJSON_IsObject(activation)) {
        xz_copy_json(activation, "code", config->activation_code, sizeof(config->activation_code));
        xz_copy_json(activation, "message", config->activation_message, sizeof(config->activation_message));
        xz_copy_json(activation, "challenge", config->activation_challenge, sizeof(config->activation_challenge));
        config->activation_required = config->activation_code[0] != '\0';
    }
    websocket = cJSON_GetObjectItemCaseSensitive(root, "websocket");
    if (cJSON_IsObject(websocket)) {
        xz_copy_json(websocket, "url", config->websocket_url, sizeof(config->websocket_url));
        xz_copy_json(websocket, "token", config->websocket_token, sizeof(config->websocket_token));
        version = cJSON_GetObjectItemCaseSensitive(websocket, "version");
        config->websocket_version = cJSON_IsNumber(version) ? version->valueint : 1;
    }
    cJSON_Delete(root);
    return 0;
}

int xiaozhi_ota_activate(const xiaozhi_config_t *config)
{
    char headers[512];
    char response[512];
    int status = 0;
    snprintf(headers, sizeof(headers),
             "Content-type: application/json\r\n"
             "Activation-Version: 1\r\nDevice-Id: %s\r\nClient-Id: %s\r\n"
             "User-Agent: L_CT4IT02_1698W/demo_xiaozhi-0.1\r\n"
             "Accept-Language: zh-CN",
             config->device_id, config->client_id);
    if (xz_http_post(XZ_OTA_URL "activate", headers, "{}", response, sizeof(response), &status) != 0)
        return -1;
    return status == 200 ? 0 : (status == 202 ? 1 : -2);
}
