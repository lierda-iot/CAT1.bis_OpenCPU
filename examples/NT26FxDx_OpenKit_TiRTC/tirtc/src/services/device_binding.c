/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 */

/**
 * @file device_binding.c
 * @brief Minimal Tange six-digit device provisioning worker.
 *
 * Protocol implemented here:
 *   service discovery -> device report -> temporary MQTT -> auth_grant ->
 *   persistent credentials -> application ACK -> server token validation.
 *
 * After server-authoritative validation it starts the permanent MQTT
 * transport used for online state, call signaling and remote unbind.  Media
 * sessions consume only the bounded credential snapshots exposed here.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"

#include "liot_dev.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_mqtt_client.h"
#include "liot_nv.h"
#include "liot_os.h"
#include "liot_power.h"
#include "liot_rtc.h"
#include "liot_sim.h"

#include "device_binding.h"
#include "formal_mqtt.h"
#include "network_manager.h"

#define BIND_SIM_ID                  0
#define BIND_PDP_CID                 1
#define BIND_DISCOVERY_URL           "http://ep-open.tangeopen.com/services"
#define BIND_CODE_TTL_MS             190000U
#define BIND_HTTP_TIMEOUT_MS         30000U
#define BIND_HTTP_CLOSE_TIMEOUT_MS   3000U
#define BIND_HTTP_PRE_RELEASE_S          1U
#define BIND_HTTP_WAIT_STEP_MS        20U
#define BIND_HTTP_SOCKET_SETTLE_S       10U
#define BIND_HTTP_NORMAL_SETTLE_S        2U
#define BIND_HTTP_SOCKET_OPEN_FAILED   (-10)
#define BIND_ERROR_RETRY_SECONDS     10U
#define BIND_HTTP_URL_MAX            512
#define BIND_HTTP_REQUEST_MAX        512
#define BIND_HTTP_REAPER_STACK       3072
#define BIND_HTTP_REAPER_RETRY_MS    1000U
#define BIND_HTTP_RECOVERY_PRIORITY  24U
#define BIND_MQTT_CONNECT_MS         30000U
#define BIND_MQTT_SUB_MS             10000U
#define BIND_MQTT_ACK_MS             8000U
#define BIND_MQTT_DISCONNECT_MS      10000U
#define BIND_MQTT_SETTLE_MS          5000U
#define BIND_MQTT_ASYNC_DEINIT_MS     5000U

#define BIND_DEVICE_SERVER_MAX       192
#define BIND_AI_SERVER_MAX           192
#define BIND_VOIP_SERVER_MAX         192
#define BIND_CALL_SERVER_MAX         192
#define BIND_TIRTC_SERVER_MAX        DEMO_BIND_TIRTC_ENDPOINT_MAX
#define BIND_MQTT_URL_MAX            192
#define BIND_TEMP_CLIENT_ID_MAX      64
#define BIND_TEMP_TOKEN_MAX          1024
#define BIND_DEVICE_ID_MAX           64
#define BIND_DEVICE_KEY_MAX          256
#define BIND_HTTP_RESPONSE_MAX       2048
#define BIND_AI_HTTP_RESPONSE_MAX    4096
#define BIND_AUTH_HEADERS_MAX        384
#define BIND_AI_AUTH_HEADERS_MAX     1152
#define BIND_MQTT_TOKEN_MAX          1024

#define BIND_NVM_FILE                "tirtc_device_auth.nvm"
#define BIND_NVM_MAGIC               0x444E4254UL /* "TBND" little endian */
#define BIND_NVM_VERSION             1U

enum
{
    BIND_ERR_NONE = 0,
    BIND_ERR_IDENTITY = -101,
    BIND_ERR_DISCOVERY_HTTP = -102,
    BIND_ERR_DISCOVERY_JSON = -103,
    BIND_ERR_REPORT_HTTP = -104,
    BIND_ERR_REPORT_JSON = -105,
    BIND_ERR_MQTT_INIT = -106,
    BIND_ERR_MQTT_CONNECT = -107,
    BIND_ERR_MQTT_SUBSCRIBE = -108,
    BIND_ERR_GRANT_TIMEOUT = -109,
    BIND_ERR_GRANT_JSON = -110,
    BIND_ERR_STORAGE = -111,
    BIND_ERR_ACK = -112,
    BIND_ERR_TOKEN_HTTP = -113,
    BIND_ERR_TOKEN_JSON = -114,
    BIND_ERR_TIME = -115,
    BIND_ERR_SIGNATURE = -116,
    BIND_ERR_FORMAL_MQTT = -117,
    BIND_ERR_RESTART = -118,
    BIND_ERR_HTTP_MUTEX = -119,
    BIND_ERR_AI_NOT_READY = -120,
    BIND_ERR_AI_HTTP = -121,
    BIND_ERR_AI_JSON = -122,
};

enum
{
    BIND_TOKEN_VALID = 0,
    BIND_TOKEN_UNBOUND = 1,
};

typedef struct
{
    char device_server[BIND_DEVICE_SERVER_MAX];
    char ai_server[BIND_AI_SERVER_MAX];
    char voip_server[BIND_VOIP_SERVER_MAX];
    char call_server[BIND_CALL_SERVER_MAX];
    char tirtc_server[BIND_TIRTC_SERVER_MAX];
    char mqtt_url[BIND_MQTT_URL_MAX];
} binding_services_t;

typedef struct
{
    char code[7];
    char temp_client_id[BIND_TEMP_CLIENT_ID_MAX];
    char temp_token[BIND_TEMP_TOKEN_MAX];
} binding_report_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    char device_id[BIND_DEVICE_ID_MAX];
    char device_key[BIND_DEVICE_KEY_MAX];
    uint32_t crc32;
} binding_nvm_record_t;

typedef struct
{
    liot_http_client_t client;
    char request_body[BIND_HTTP_REQUEST_MAX];
    int request_len;
    int request_offset;
    char response[BIND_AI_HTTP_RESPONSE_MAX];
    int response_capacity;
    int response_len;
    int http_status;
    int event_result;
    volatile bool done;
    volatile bool closed;
    volatile bool overflow;
    volatile bool open_failed;
    volatile bool socket_open_failed;
} binding_http_ctx_t;

static demo_binding_snapshot_t s_snapshot = {
    .state = DEMO_BIND_IDLE,
};
static volatile uint32_t s_request_sequence;

static binding_services_t s_services;
static binding_report_t s_report;
static binding_nvm_record_t s_credentials;
static bool s_have_credentials;
static volatile bool s_http_cleanup_pending;
static binding_http_ctx_t *s_http_cleanup_ctx;
static volatile bool s_http_reaper_running;
static volatile bool s_http_queue_reset_guard;
static liot_mutex_t s_http_mutex;

static liot_mqtt_client_t s_mqtt_client;
static liot_sem_t s_mqtt_sem;
static volatile bool s_mqtt_connect_done;
static volatile bool s_mqtt_connect_ok;
static volatile bool s_mqtt_sub_done;
static volatile bool s_mqtt_ack_done;
static volatile bool s_mqtt_disconnect_done;
static volatile bool s_mqtt_lost;
static volatile bool s_mqtt_cleanup_pending;
static volatile bool s_grant_ready;
static volatile bool s_grant_invalid;
static volatile bool s_grant_use_existing;
static char s_grant_device_id[BIND_DEVICE_ID_MAX];
static char s_grant_device_key[BIND_DEVICE_KEY_MAX];
static char s_cmd_topic[128];

static void binding_secure_zero(void *data, size_t size)
{
    volatile uint8_t *cursor = (volatile uint8_t *)data;

    if (cursor == NULL)
    {
        return;
    }
    while (size-- > 0U)
    {
        *cursor++ = 0U;
    }
}

static void binding_snapshot_set(demo_binding_state_e state,
                                 const char *code,
                                 uint16_t seconds_left,
                                 int error)
{
    char safe_code[7] = {0};

    if (code != NULL)
    {
        size_t code_len = strlen(code);
        if (code_len > 6U)
        {
            code_len = 6U;
        }
        memcpy(safe_code, code, code_len);
    }

    liot_rtos_enter_critical();
    s_snapshot.state = state;
    memcpy(s_snapshot.code, safe_code, sizeof(s_snapshot.code));
    s_snapshot.seconds_left = seconds_left;
    s_snapshot.error = error;
    liot_rtos_exit_critical();

    liot_trace("[BIND] state=%d error=%d free_heap=%u\r\n",
               (int)state, error,
               (unsigned int)liot_xPortGetFreeHeapSize());
}

static void binding_snapshot_set_seconds(uint16_t seconds_left)
{
    liot_rtos_enter_critical();
    s_snapshot.seconds_left = seconds_left;
    liot_rtos_exit_critical();
}

void demo_binding_request(void)
{
    liot_rtos_enter_critical();
    if (s_snapshot.state == DEMO_BIND_IDLE ||
        s_snapshot.state == DEMO_BIND_ERROR)
    {
        s_request_sequence++;
    }
    liot_rtos_exit_critical();
}

void demo_binding_get_snapshot(demo_binding_snapshot_t *out)
{
    if (out == NULL)
    {
        return;
    }

    liot_rtos_enter_critical();
    *out = s_snapshot;
    liot_rtos_exit_critical();
}

static uint32_t binding_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t i;
    int bit;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^
                  ((crc & 1U) ? 0xEDB88320UL : 0U);
        }
    }
    return ~crc;
}

static bool binding_nvm_record_valid(const binding_nvm_record_t *record)
{
    uint32_t expected_crc;

    if (record == NULL ||
        record->magic != BIND_NVM_MAGIC ||
        record->version != BIND_NVM_VERSION ||
        record->size != sizeof(*record))
    {
        return false;
    }
    if (record->device_id[0] == '\0' || record->device_key[0] == '\0' ||
        record->device_id[sizeof(record->device_id) - 1] != '\0' ||
        record->device_key[sizeof(record->device_key) - 1] != '\0')
    {
        return false;
    }

    expected_crc = binding_crc32((const uint8_t *)record,
                                 offsetof(binding_nvm_record_t, crc32));
    return expected_crc == record->crc32;
}

static int binding_load_credentials(void)
{
    binding_nvm_record_t record;
    int ret = -1;

    memset(&record, 0, sizeof(record));
    if (liot_nvm_fread(BIND_NVM_FILE, &record, sizeof(record), 1) !=
        (int)sizeof(record))
    {
        goto cleanup;
    }
    if (!binding_nvm_record_valid(&record))
    {
        goto cleanup;
    }

    s_credentials = record;
    s_have_credentials = true;
    ret = 0;

cleanup:
    memset(&record, 0, sizeof(record));
    return ret;
}

static int binding_store_credentials(const char *device_id,
                                     const char *device_key)
{
    binding_nvm_record_t record;
    binding_nvm_record_t verify;
    size_t id_len;
    size_t key_len;

    if (device_id == NULL || device_key == NULL)
    {
        return -1;
    }
    id_len = strlen(device_id);
    key_len = strlen(device_key);
    if (id_len == 0 || id_len >= BIND_DEVICE_ID_MAX ||
        key_len == 0 || key_len >= BIND_DEVICE_KEY_MAX)
    {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    record.magic = BIND_NVM_MAGIC;
    record.version = BIND_NVM_VERSION;
    record.size = sizeof(record);
    memcpy(record.device_id, device_id, id_len + 1U);
    memcpy(record.device_key, device_key, key_len + 1U);
    record.crc32 = binding_crc32((const uint8_t *)&record,
                                 offsetof(binding_nvm_record_t, crc32));

    if (liot_nvm_fwrite(BIND_NVM_FILE, &record, sizeof(record), 1) !=
        (int)sizeof(record))
    {
        memset(&record, 0, sizeof(record));
        return -1;
    }

    memset(&verify, 0, sizeof(verify));
    if (liot_nvm_fread(BIND_NVM_FILE, &verify, sizeof(verify), 1) !=
            (int)sizeof(verify) ||
        !binding_nvm_record_valid(&verify) ||
        strcmp(record.device_id, verify.device_id) != 0 ||
        strcmp(record.device_key, verify.device_key) != 0)
    {
        memset(&record, 0, sizeof(record));
        memset(&verify, 0, sizeof(verify));
        return -1;
    }

    s_credentials = verify;
    s_have_credentials = true;

    memset(&record, 0, sizeof(record));
    memset(&verify, 0, sizeof(verify));
    return 0;
}

static int binding_make_pseudo_mac(char mac[18])
{
    char imei[20];
    unsigned char digest[32];
    int ret;

    memset(imei, 0, sizeof(imei));
    memset(digest, 0, sizeof(digest));
    ret = liot_dev_get_imei(imei, sizeof(imei), BIND_SIM_ID);
    if (ret != LIOT_DEV_SUCCESS || imei[0] == '\0')
    {
        return -1;
    }
    if (mbedtls_sha256_ret((const unsigned char *)imei,
                           strlen(imei), digest, 0) != 0)
    {
        memset(imei, 0, sizeof(imei));
        return -1;
    }

    /* IEEE locally administered, unicast address derived from stable IMEI. */
    digest[0] = (unsigned char)((digest[0] | 0x02U) & 0xFEU);
    snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             digest[0], digest[1], digest[2],
             digest[3], digest[4], digest[5]);
    memset(imei, 0, sizeof(imei));
    memset(digest, 0, sizeof(digest));
    return 0;
}

static int binding_build_auth_headers(char *headers, size_t headers_size)
{
    const mbedtls_md_info_t *md_info;
    uint64_t timestamp_ms = 0;
    uint32_t timestamp_s;
    char timestamp[16];
    char nonce[17];
    char sign_input[BIND_DEVICE_ID_MAX + 40];
    unsigned char digest[32];
    unsigned char signature[64];
    size_t signature_len = 0;
    int written;
    int ret = BIND_ERR_SIGNATURE;

    memset(timestamp, 0, sizeof(timestamp));
    memset(nonce, 0, sizeof(nonce));
    memset(sign_input, 0, sizeof(sign_input));
    memset(digest, 0, sizeof(digest));
    memset(signature, 0, sizeof(signature));

    if (headers == NULL || headers_size == 0U || !s_have_credentials)
    {
        goto cleanup;
    }
    headers[0] = '\0';
    if (Liot_GetTimestamp(&timestamp_ms) != 0 || timestamp_ms == 0ULL)
    {
        ret = BIND_ERR_TIME;
        goto cleanup;
    }

    timestamp_s = (uint32_t)(timestamp_ms / 1000ULL);
    snprintf(timestamp, sizeof(timestamp), "%u",
             (unsigned int)timestamp_s);
    liot_trace("[BIND] auth timestamp=%u\r\n",
               (unsigned int)timestamp_s);
    snprintf(nonce, sizeof(nonce), "%08X%08X",
             (unsigned int)liot_true_rand(),
             (unsigned int)liot_true_rand());
    written = snprintf(sign_input, sizeof(sign_input), "%s%s%s",
                       s_credentials.device_id, timestamp, nonce);
    if (written < 0 || written >= (int)sizeof(sign_input))
    {
        goto cleanup;
    }

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL ||
        mbedtls_md_hmac(md_info,
                        (const unsigned char *)s_credentials.device_key,
                        strlen(s_credentials.device_key),
                        (const unsigned char *)sign_input,
                        (size_t)written, digest) != 0 ||
        mbedtls_base64_encode(signature, sizeof(signature) - 1U,
                              &signature_len, digest, sizeof(digest)) != 0 ||
        signature_len >= sizeof(signature))
    {
        goto cleanup;
    }
    signature[signature_len] = '\0';

    /* WRITE_HEADER is transmitted verbatim; do not append a final CRLF. */
    written = snprintf(headers, headers_size,
                       "X-Device-Id: %s\r\n"
                       "X-Timestamp: %s\r\n"
                       "X-Nonce: %s\r\n"
                       "X-Signature: %s",
                       s_credentials.device_id, timestamp, nonce, signature);
    if (written < 0 || written >= (int)headers_size)
    {
        headers[0] = '\0';
        goto cleanup;
    }
    ret = 0;

cleanup:
    memset(timestamp, 0, sizeof(timestamp));
    memset(nonce, 0, sizeof(nonce));
    memset(sign_input, 0, sizeof(sign_input));
    memset(digest, 0, sizeof(digest));
    memset(signature, 0, sizeof(signature));
    return ret;
}

static void binding_http_event_cb(liot_http_client_t *client,
                                  int event,
                                  int event_code,
                                  void *arg)
{
    binding_http_ctx_t *ctx = (binding_http_ctx_t *)arg;

    if (ctx == NULL)
    {
        return;
    }

    liot_trace("[BIND-HTTP] event=%d code=0x%x\r\n",
               event, (unsigned int)event_code);

    switch (event)
    {
    case LIOT_HTTPC_SESSION_OPEN:
        if (event_code != LIOT_HTTPC_SUCCESS)
        {
            ctx->open_failed = true;
            ctx->socket_open_failed =
                event_code == LIOT_HTTPC_ERR_SOCKET_FAILURE;
            ctx->event_result = event_code;
            ctx->done = true;
        }
        break;

    case LIOT_HTTPC_UPLOAD_START:
        if (ctx->request_len > 0)
        {
            liot_httpc_user_notify(client, LIOT_HTTPC_READ);
        }
        break;

    case LIOT_HTTPC_RESPONSE_STATUS:
        if (event_code == LIOT_HTTPC_SUCCESS)
        {
            int info_ret = liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE,
                                              &ctx->http_status);
            if (info_ret != LIOT_HTTPC_SUCCESS)
            {
                ctx->event_result = info_ret;
                ctx->done = true;
            }
        }
        else
        {
            ctx->event_result = event_code;
            ctx->done = true;
        }
        break;

    case LIOT_HTTPC_RESPONSE_COMPLETE:
        ctx->event_result = event_code;
        ctx->done = true;
        break;

    case LIOT_HTTPC_RESPONSE_TIMEOUT:
        ctx->event_result = event_code;
        ctx->done = true;
        break;

    case LIOT_HTTPC_SESSION_CLOSE:
        if (!ctx->done)
        {
            ctx->event_result = (event_code != LIOT_HTTPC_SUCCESS)
                                    ? event_code
                                    : LIOT_HTTPC_ERR_SOCKET_FAILURE;
            ctx->done = true;
        }
        ctx->closed = true;
        break;

    default:
        break;
    }
}

static int binding_http_write_cb(liot_http_client_t *client,
                                 void *arg,
                                 char *data,
                                 int size,
                                 unsigned char end)
{
    binding_http_ctx_t *ctx = (binding_http_ctx_t *)arg;
    int available;
    int copy_len;

    (void)client;
    (void)end;
    if (ctx == NULL || data == NULL || size <= 0)
    {
        return 0;
    }

    available = ctx->response_capacity - ctx->response_len - 1;
    copy_len = (size < available) ? size : available;
    if (copy_len > 0)
    {
        memcpy(ctx->response + ctx->response_len, data, copy_len);
        ctx->response_len += copy_len;
        ctx->response[ctx->response_len] = '\0';
    }
    if (copy_len != size)
    {
        ctx->overflow = true;
    }

    /* Consume the whole HTTP chunk even when the bounded buffer is full. */
    return size;
}

static int binding_http_read_cb(liot_http_client_t *client,
                                void *arg,
                                char *data,
                                int size)
{
    binding_http_ctx_t *ctx = (binding_http_ctx_t *)arg;
    int remaining;
    int copy_len;

    (void)client;
    if (ctx == NULL || data == NULL || size <= 0)
    {
        return 0;
    }

    remaining = ctx->request_len - ctx->request_offset;
    if (remaining <= 0)
    {
        return 0;
    }
    copy_len = (remaining < size) ? remaining : size;
    memcpy(data, ctx->request_body + ctx->request_offset, copy_len);
    ctx->request_offset += copy_len;
    return copy_len;
}

static int binding_http_wait_flag(binding_http_ctx_t *ctx,
                                  volatile bool *flag,
                                  uint32_t timeout_ms)
{
    uint32_t waited_ms = 0;

    (void)ctx;

    while (!(*flag) && waited_ms < timeout_ms)
    {
        liot_rtos_task_sleep_ms(BIND_HTTP_WAIT_STEP_MS);
        waited_ms += BIND_HTTP_WAIT_STEP_MS;
    }
    return *flag ? 0 : -1;
}

/*
 * libliot_https owns one global worker.  Releasing before SESSION_CLOSE can
 * free objects that a DNS/connect worker still uses.  A timed-out request is
 * therefore transferred to one retained context until both close and release
 * complete.  The pointer is never kept only on a failed task-creation stack:
 * later requests can re-kick the reaper, or release it inline once CLOSED.
 */
static int binding_http_wait_close(binding_http_ctx_t *ctx, uint32_t timeout_ms)
{
    uint32_t waited_ms = 0;
    uint32_t retry_ms = 0;

    while (!ctx->closed)
    {
        if (waited_ms > 0 && waited_ms >= timeout_ms)
        {
            break;
        }
        /* F6D_A exits directly after a failed OPEN, without SESSION_CLOSE or
         * consuming STOP.  stop() reports CLOSE synchronously once its worker
         * has exited.  Only this error path may retry STOP; normal responses
         * must keep the existing single-stop rule. */
        if (ctx->open_failed && waited_ms >= retry_ms)
        {
            s_http_queue_reset_guard = true;
            (void)liot_httpc_stop(&ctx->client);
            retry_ms = waited_ms + BIND_HTTP_REAPER_RETRY_MS;
        }
        if (ctx->closed || waited_ms >= timeout_ms)
        {
            break;
        }
        liot_rtos_task_sleep_ms(BIND_HTTP_WAIT_STEP_MS);
        waited_ms += BIND_HTTP_WAIT_STEP_MS;
    }
    return ctx->closed ? 0 : -1;
}

static int binding_http_perform(binding_http_ctx_t *ctx)
{
    liot_task_t task = NULL;
    uint8 old_priority;
    uint8 ignored_priority;
    int ret;

    if (!s_http_queue_reset_guard)
    {
        return liot_httpc_perform(&ctx->client);
    }

    /* The failed-OPEN stop may leave a command in the vendor global queue.
     * F6D_A perform creates its priority-23 worker BEFORE resetting that queue.
     * Keep this recovery-only caller above it until perform has reset the queue.
     * Do not suspend scheduling: perform itself sleeps. Recheck on SDK updates.
     * The vendor priority wrapper requires a non-NULL old_priority even on restore. */
    if (liot_rtos_task_get_current_ref(&task) != LIOT_OSI_SUCCESS ||
        liot_rtos_task_change_priority(task, BIND_HTTP_RECOVERY_PRIORITY,
                                       &old_priority) != LIOT_OSI_SUCCESS)
    {
        return -1;
    }
    ret = liot_httpc_perform(&ctx->client);
    (void)liot_rtos_task_change_priority(task, old_priority, &ignored_priority);
    if (ret == LIOT_HTTPC_SUCCESS)
    {
        s_http_queue_reset_guard = false;
    }
    return ret;
}

static void binding_http_retained_cleanup_complete(binding_http_ctx_t *ctx)
{
    bool socket_open_failed;
    bool detached = false;

    if (ctx == NULL)
    {
        return;
    }

    socket_open_failed = ctx->socket_open_failed;
    liot_rtos_task_sleep_s(socket_open_failed ?
                               BIND_HTTP_SOCKET_SETTLE_S :
                               BIND_HTTP_NORMAL_SETTLE_S);

    /* release() succeeded and its worker-settle interval has elapsed, so the
     * singleton can admit a new request.  Detach global ownership before
     * freeing the local context; this leaves no globally reachable stale
     * pointer for a concurrent request to observe. */
    liot_rtos_enter_critical();
    if (s_http_cleanup_ctx == ctx)
    {
        s_http_cleanup_ctx = NULL;
        s_http_cleanup_pending = false;
        detached = true;
    }
    s_http_reaper_running = false;
    liot_rtos_exit_critical();

    if (!detached)
    {
        /* Ownership mismatch is unreachable with the singleton mutex.  Keep
         * callback storage alive rather than risk freeing a referenced ctx. */
        liot_trace("[BIND-HTTP] cleanup detach invariant failed\r\n");
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    liot_rtos_free(ctx);
}

static void binding_http_reaper_task(void *arg)
{
    binding_http_ctx_t *ctx = (binding_http_ctx_t *)arg;
    int release_ret;

    if (ctx == NULL)
    {
        liot_rtos_enter_critical();
        s_http_reaper_running = false;
        liot_rtos_exit_critical();
        liot_rtos_task_delete(NULL);
        return;
    }

    liot_trace("[BIND-HTTP] reaper waiting for close\r\n");
    while (!ctx->closed)
    {
        (void)binding_http_wait_close(ctx, BIND_HTTP_REAPER_RETRY_MS);
    }

    /* CLOSE is raised from the HTTP worker before that callback has fully
     * unwound.  Keep the grace interval used by the previously stable build
     * before releasing the singleton. */
    liot_rtos_task_sleep_s(BIND_HTTP_PRE_RELEASE_S);
    for (;;)
    {
        liot_trace("[BIND-HTTP] reaper release begin\r\n");
        release_ret = liot_httpc_release(&ctx->client);
        liot_trace("[BIND-HTTP] reaper release ret=0x%x\r\n",
                   (unsigned int)release_ret);
        if (release_ret == LIOT_HTTPC_SUCCESS)
        {
            break;
        }

        /* A failed release still leaves callbacks able to reference ctx.
         * Retain both the context and gate, then retry instead of leaking the
         * pointer and leaving a permanently ownerless pending flag. */
        liot_rtos_task_sleep_ms(BIND_HTTP_REAPER_RETRY_MS);
    }

    binding_http_retained_cleanup_complete(ctx);
    liot_rtos_task_delete(NULL);
}

static bool binding_http_start_reaper(void)
{
    binding_http_ctx_t *ctx = NULL;
    liot_task_t reaper_task = NULL;
    bool start = false;

    liot_rtos_enter_critical();
    if (s_http_cleanup_pending && s_http_cleanup_ctx != NULL &&
        !s_http_reaper_running)
    {
        s_http_reaper_running = true;
        ctx = s_http_cleanup_ctx;
        start = true;
    }
    liot_rtos_exit_critical();

    if (!start)
    {
        return true;
    }
    if (liot_rtos_task_create(&reaper_task,
                              BIND_HTTP_REAPER_STACK,
                              LIOT_APP_TASK_PRIORITY,
                              "bind_http_reaper",
                              binding_http_reaper_task,
                              ctx) == LIOT_OSI_SUCCESS)
    {
        return true;
    }

    liot_rtos_enter_critical();
    s_http_reaper_running = false;
    liot_rtos_exit_critical();
    liot_trace("[BIND-HTTP] reaper create failed; retained for retry\r\n");
    return false;
}

static void binding_http_retain_for_cleanup(binding_http_ctx_t *ctx)
{
    bool accepted = false;

    liot_rtos_enter_critical();
    if (ctx != NULL &&
        (s_http_cleanup_ctx == NULL || s_http_cleanup_ctx == ctx))
    {
        s_http_cleanup_ctx = ctx;
        s_http_cleanup_pending = true;
        accepted = true;
    }
    liot_rtos_exit_critical();

    if (!accepted)
    {
        /* The HTTP mutex and pending gate make this unreachable.  Do not free
         * ctx here: a worker may still reference it. */
        liot_trace("[BIND-HTTP] cleanup ownership invariant failed\r\n");
        return;
    }
    (void)binding_http_start_reaper();
}

/* Return true when this invocation observed an older retained request. */
static bool binding_http_resume_retained_cleanup(void)
{
    binding_http_ctx_t *ctx = NULL;
    bool pending;
    bool claimed = false;
    int release_ret;

    liot_rtos_enter_critical();
    pending = s_http_cleanup_pending;
    if (pending && s_http_cleanup_ctx != NULL &&
        !s_http_reaper_running)
    {
        s_http_reaper_running = true;
        ctx = s_http_cleanup_ctx;
        claimed = true;
    }
    liot_rtos_exit_critical();

    if (!pending || !claimed)
    {
        return pending;
    }

    /* Also recover failed OPEN when no reaper task could be allocated. */
    (void)binding_http_wait_close(ctx, 0);
    if (ctx->closed)
    {
        /* No new worker task is needed after CLOSE.  This is the no-allocation
         * fallback when the original reaper task could not be created. */
        liot_rtos_task_sleep_s(BIND_HTTP_PRE_RELEASE_S);
        release_ret = liot_httpc_release(&ctx->client);
        liot_trace("[BIND-HTTP] retained inline release ret=0x%x\r\n",
                   (unsigned int)release_ret);
        if (release_ret == LIOT_HTTPC_SUCCESS)
        {
            binding_http_retained_cleanup_complete(ctx);
            return true;
        }
    }
    else
    {
        /* Keep retaining callback storage until a real CLOSE is observed. */
        liot_trace("[BIND-HTTP] retained close pending; restart reaper\r\n");
    }

    liot_rtos_enter_critical();
    s_http_reaper_running = false;
    liot_rtos_exit_critical();
    (void)binding_http_start_reaper();
    return true;
}

/* A deferred AI-token cleanup still owns libliot_https' singleton worker.
 * Later feature requests wait for that worker to exit instead of failing with
 * a transient "previous request is still closing" error. */
static int binding_http_wait_retained_cleanup(uint32_t timeout_ms)
{
    uint32_t started_ms = liot_rtos_get_running_time();

    for (;;)
    {
        bool pending;

        liot_rtos_enter_critical();
        pending = s_http_cleanup_pending;
        liot_rtos_exit_critical();
        if (!pending)
        {
            return 0;
        }

        (void)binding_http_resume_retained_cleanup();
        if ((liot_rtos_get_running_time() - started_ms) >= timeout_ms)
        {
            return -1;
        }
        liot_rtos_task_sleep_ms(BIND_HTTP_WAIT_STEP_MS);
    }
}

static int binding_http_request_unlocked(const char *url_string,
                                         liot_httpc_method_e method,
                                         const char *request_body,
                                         const char *extra_headers,
                                         char *response,
                                         int response_capacity,
                                         int *http_status,
                                         uint32_t timeout_ms,
                                         bool defer_success_cleanup)
{
    binding_http_ctx_t *ctx = NULL;
    liot_httpc_url_s url;
    char url_buffer[BIND_HTTP_URL_MAX];
    bool url_option_applied = false;
    bool perform_submitted = false;
    int ret = -1;
    int perform_ret;
    int url_length;
    int request_length;
    int timeout_seconds;
    uint32_t cleanup_wait_started_ms;
    uint32_t cleanup_wait_elapsed_ms;

#define BIND_HTTP_SETOPT_OR_FAIL(option, value)                              \
    do                                                                       \
    {                                                                        \
        int option_ret = liot_httpc_setopt(&ctx->client, (option), (value)); \
        if (option_ret != LIOT_HTTPC_SUCCESS)                                \
        {                                                                    \
            liot_trace("[BIND-HTTP] setopt=%d failed ret=0x%x\r\n",        \
                       (int)(option), (unsigned int)option_ret);              \
            ret = -5;                                                        \
            goto cleanup;                                                    \
        }                                                                    \
    } while (0)

    if (url_string == NULL || response == NULL || response_capacity < 2 ||
        timeout_ms < BIND_HTTP_WAIT_STEP_MS)
    {
        return -1;
    }

    cleanup_wait_started_ms = liot_rtos_get_running_time();
    if (binding_http_wait_retained_cleanup(timeout_ms) != 0)
    {
        liot_trace("[BIND-HTTP] previous request cleanup timeout\r\n");
        return -9;
    }
    cleanup_wait_elapsed_ms =
        liot_rtos_get_running_time() - cleanup_wait_started_ms;
    if (cleanup_wait_elapsed_ms >= timeout_ms ||
        timeout_ms - cleanup_wait_elapsed_ms < BIND_HTTP_WAIT_STEP_MS)
    {
        liot_trace("[BIND-HTTP] request budget expired during cleanup\r\n");
        return -9;
    }
    timeout_ms -= cleanup_wait_elapsed_ms;

    request_length = (request_body != NULL) ? (int)strlen(request_body) : 0;
    if ((method == LIOT_HTTPC_METHOD_POST ||
         method == LIOT_HTTPC_METHOD_PUT) && request_length == 0)
    {
        liot_trace("[BIND-HTTP] F6D_A rejects a zero-length upload\r\n");
        return -2;
    }
    if (request_length >= BIND_HTTP_REQUEST_MAX)
    {
        liot_trace("[BIND-HTTP] request body too long len=%d\r\n",
                   request_length);
        return -2;
    }

    ctx = (binding_http_ctx_t *)liot_rtos_malloc(sizeof(*ctx));
    if (ctx == NULL)
    {
        liot_trace("[BIND-HTTP] job allocation failed\r\n");
        return -3;
    }
    memset(ctx, 0, sizeof(*ctx));
    memset(&url, 0, sizeof(url));
    memset(url_buffer, 0, sizeof(url_buffer));
    response[0] = '\0';
    if (request_length > 0)
    {
        memcpy(ctx->request_body, request_body, (size_t)request_length);
    }
    ctx->request_body[request_length] = '\0';
    ctx->request_len = request_length;
    ctx->response_capacity = (response_capacity < BIND_AI_HTTP_RESPONSE_MAX)
                                 ? response_capacity
                                 : BIND_AI_HTTP_RESPONSE_MAX;
    ctx->event_result = LIOT_HTTPC_SUCCESS;

    url_length = snprintf(url_buffer, sizeof(url_buffer), "%s", url_string);
    if (url_length < 0 || url_length >= (int)sizeof(url_buffer))
    {
        liot_trace("[BIND-HTTP] URL too long\r\n");
        ret = -2;
        goto cleanup;
    }

    liot_trace("[BIND-HTTP] begin method=%d url=%s free_heap=%u\r\n",
               method, url_buffer,
               (unsigned int)liot_xPortGetFreeHeapSize());
    if (!liot_httpc_url_parse(url_buffer, &url))
    {
        liot_trace("[BIND-HTTP] URL parse failed\r\n");
        ret = -2;
        goto cleanup;
    }
    liot_trace("[BIND-HTTP] parsed host=%s port=%u uri=%s scheme=%d\r\n",
               (url.host != NULL) ? url.host : "(null)",
               (unsigned int)url.port,
               (url.uri != NULL) ? url.uri : "(null)",
               url.scheme);
    if (liot_httpc_new(&ctx->client, binding_http_event_cb, ctx) !=
        LIOT_HTTPC_SUCCESS)
    {
        liot_trace("[BIND-HTTP] client create failed\r\n");
        ret = -4;
        goto cleanup;
    }
    liot_trace("[BIND-HTTP] client=%d created\r\n", ctx->client);

    /* F6D_A HTTP is SIM0-only; its SIM_ID option returns NOT_SUPPORT. */
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_PDPCID, BIND_PDP_CID);
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_METHOD, method);
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_URL, &url);
    url_option_applied = true;
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_WRITE_FUNC,
                             binding_http_write_cb);
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ctx);
    timeout_seconds = (int)((timeout_ms + 999U) / 1000U);
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT,
                             timeout_seconds);
    BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT,
                             timeout_seconds);

    if (extra_headers != NULL && extra_headers[0] != '\0')
    {
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_WRITE_HEADER,
                                 extra_headers);
    }

    if (method == LIOT_HTTPC_METHOD_POST)
    {
        /* REQUEST_HEADER only accepts Content-Type or Range in this SDK. */
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER,
                                 "Content-Type: application/json");
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_READ_FUNC,
                                 binding_http_read_cb);
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_READ_DATA, ctx);
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN,
                                 ctx->request_len);
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE,
                                 LIOT_HTTPC_RAW_DATA);
        BIND_HTTP_SETOPT_OR_FAIL(LIOT_HTTP_CLIENT_OPT_RAW_REQUEST, 0);
    }

    liot_trace("[BIND-HTTP] perform begin\r\n");
    perform_ret = binding_http_perform(ctx);
    liot_trace("[BIND-HTTP] perform ret=0x%x\r\n",
               (unsigned int)perform_ret);
    if (perform_ret != LIOT_HTTPC_SUCCESS)
    {
        ret = -5;
        goto cleanup;
    }
    perform_submitted = true;
    if (binding_http_wait_flag(ctx, &ctx->done, timeout_ms) != 0)
    {
        liot_trace("[BIND-HTTP] response timeout after %u ms\r\n",
                   (unsigned int)timeout_ms);
        ret = -6;
        goto cleanup;
    }
    if (ctx->event_result != LIOT_HTTPC_SUCCESS || ctx->overflow)
    {
        liot_trace("[BIND-HTTP] response failed event=0x%x overflow=%d\r\n",
                   (unsigned int)ctx->event_result, ctx->overflow ? 1 : 0);
        ret = ctx->socket_open_failed && !ctx->overflow ?
                  BIND_HTTP_SOCKET_OPEN_FAILED : -7;
        goto cleanup;
    }

    if (http_status != NULL)
    {
        *http_status = ctx->http_status;
    }
    memcpy(response, ctx->response, (size_t)ctx->response_len + 1U);
    liot_trace("[BIND-HTTP] response complete status=%d bytes=%d\r\n",
               ctx->http_status, ctx->response_len);
    ret = 0;

cleanup:
    if (ctx != NULL && ctx->client != 0)
    {
        bool stop_required = true;
        int stop_ret;
        int release_ret;

        /* Once perform() succeeds the worker can move from RECV_COMP to its
         * own STOP/CLOSE path between the response callback and this task.
         * Do not queue another stop after that transition: a late duplicate
         * CLOSE can otherwise run after release has freed the client. */
        if (perform_submitted)
        {
            if (!ctx->closed)
            {
                liot_rtos_task_sleep_ms(BIND_HTTP_WAIT_STEP_MS);
            }
            stop_required = !ctx->closed &&
                            liot_httpc_is_running(&ctx->client);
        }

        /* A completed AI-token response already lives in caller-owned
         * storage.  Let an existing STOP/CLOSE continue, or submit one stop
         * while still running, then let the reaper retain callback storage
         * until SESSION_CLOSE while the AI task proceeds with WHIP. */
        if (ret == 0 && defer_success_cleanup)
        {
            liot_trace("[BIND-HTTP] successful cleanup deferred\r\n");
            if (stop_required)
            {
                stop_ret = liot_httpc_stop(&ctx->client);
                liot_trace("[BIND-HTTP] deferred stop ret=0x%x\r\n",
                           (unsigned int)stop_ret);
            }
            else
            {
                liot_trace("[BIND-HTTP] deferred stop skipped: closing/closed\r\n");
            }
            binding_http_retain_for_cleanup(ctx);
            ctx = NULL;
            goto done;
        }

        if (stop_required)
        {
            liot_trace("[BIND-HTTP] stop begin ret=%d\r\n", ret);
            stop_ret = liot_httpc_stop(&ctx->client);
            liot_trace("[BIND-HTTP] stop ret=0x%x\r\n",
                       (unsigned int)stop_ret);
        }
        else
        {
            liot_trace("[BIND-HTTP] stop skipped: closing/closed ret=%d\r\n",
                       ret);
        }
        if (!ctx->closed)
        {
            if (binding_http_wait_close(ctx, BIND_HTTP_CLOSE_TIMEOUT_MS) != 0)
            {
                liot_trace("[BIND-HTTP] close event timeout after %u ms\r\n",
                           (unsigned int)BIND_HTTP_CLOSE_TIMEOUT_MS);

                /* Never release stack/live callback state while worker runs. */
                if (ret == 0)
                {
                    ret = -8;
                }
                binding_http_retain_for_cleanup(ctx);
                liot_trace("[BIND-HTTP] cleanup retained for reaper\r\n");
                ctx = NULL;
                goto done;
            }
        }

        /* Match the stable F6D_A lifecycle: let the CLOSE callback unwind
         * before release touches the singleton. */
        liot_rtos_task_sleep_s(BIND_HTTP_PRE_RELEASE_S);
        liot_trace("[BIND-HTTP] release begin\r\n");
        release_ret = liot_httpc_release(&ctx->client);
        liot_trace("[BIND-HTTP] release ret=0x%x\r\n",
                   (unsigned int)release_ret);
        if (release_ret != LIOT_HTTPC_SUCCESS)
        {
            ret = -8;
            binding_http_retain_for_cleanup(ctx);
            liot_trace("[BIND-HTTP] release retry retained for reaper\r\n");
            ctx = NULL;
            goto done;
        }

        /* release only posts the worker-exit message; let it consume it. */
        liot_trace("[BIND-HTTP] worker settle begin socket_fail=%u\r\n",
                   ctx->socket_open_failed ? 1U : 0U);
        liot_rtos_task_sleep_s(ctx->socket_open_failed ?
                                   BIND_HTTP_SOCKET_SETTLE_S :
                                   BIND_HTTP_NORMAL_SETTLE_S);
        liot_trace("[BIND-HTTP] worker settle done\r\n");
    }
    if (ctx != NULL)
    {
        memset(ctx, 0, sizeof(*ctx));
        liot_rtos_free(ctx);
    }

done:
    /* Before URL setopt succeeds, the parser allocations still belong here. */
    if (!url_option_applied && url.host != NULL)
    {
        liot_rtos_free(url.host);
        url.host = NULL;
    }
    if (!url_option_applied && url.uri != NULL)
    {
        liot_rtos_free(url.uri);
        url.uri = NULL;
    }
    liot_trace("[BIND-HTTP] end ret=%d\r\n", ret);
#undef BIND_HTTP_SETOPT_OR_FAIL
    return ret;
}

static int binding_http_request_timeout(const char *url_string,
                                        liot_httpc_method_e method,
                                        const char *request_body,
                                        const char *extra_headers,
                                        char *response,
                                        int response_capacity,
                                        int *http_status,
                                        uint32_t timeout_ms);

static int binding_http_request_timeout_mode(const char *url_string,
                                             liot_httpc_method_e method,
                                             const char *request_body,
                                             const char *extra_headers,
                                             char *response,
                                             int response_capacity,
                                             int *http_status,
                                             uint32_t timeout_ms,
                                             bool defer_success_cleanup);

/* libliot_https owns a singleton worker.  Provisioning and AI token fetches
 * run in different application tasks, so every complete HTTP lifecycle must
 * be serialized, including stop/release/worker-settle. */
static int binding_http_request(const char *url_string,
                                liot_httpc_method_e method,
                                const char *request_body,
                                const char *extra_headers,
                                char *response,
                                int response_capacity,
                                int *http_status)
{
    return binding_http_request_timeout(
        url_string, method, request_body, extra_headers, response,
        response_capacity, http_status, BIND_HTTP_TIMEOUT_MS);
}

static int binding_http_request_timeout(const char *url_string,
                                        liot_httpc_method_e method,
                                        const char *request_body,
                                        const char *extra_headers,
                                        char *response,
                                        int response_capacity,
                                        int *http_status,
                                        uint32_t timeout_ms)
{
    return binding_http_request_timeout_mode(
        url_string, method, request_body, extra_headers, response,
        response_capacity, http_status, timeout_ms, false);
}

static int binding_http_request_deferred(const char *url_string,
                                         liot_httpc_method_e method,
                                         const char *request_body,
                                         const char *extra_headers,
                                         char *response,
                                         int response_capacity,
                                         int *http_status)
{
    return binding_http_request_timeout_mode(
        url_string, method, request_body, extra_headers, response,
        response_capacity, http_status, BIND_HTTP_TIMEOUT_MS, true);
}

static int binding_http_request_timeout_mode(const char *url_string,
                                             liot_httpc_method_e method,
                                             const char *request_body,
                                             const char *extra_headers,
                                             char *response,
                                             int response_capacity,
                                             int *http_status,
                                             uint32_t timeout_ms,
                                             bool defer_success_cleanup)
{
    uint32_t started_ms;
    uint32_t elapsed_ms;
    uint32_t remaining_ms;
    int ret;

    started_ms = liot_rtos_get_running_time();
    /* A feature request must never wait forever behind the singleton HTTP
     * client.  Use the caller's own operation budget for lock acquisition;
     * the caller can retry while media/control workers remain responsive. */
    if (s_http_mutex == NULL ||
        liot_rtos_mutex_lock(s_http_mutex, timeout_ms) !=
            LIOT_OSI_SUCCESS)
    {
        liot_trace("[BIND-HTTP] mutex unavailable\r\n");
        return BIND_ERR_HTTP_MUTEX;
    }

    elapsed_ms = liot_rtos_get_running_time() - started_ms;
    if (elapsed_ms >= timeout_ms ||
        timeout_ms - elapsed_ms < BIND_HTTP_WAIT_STEP_MS)
    {
        liot_trace("[BIND-HTTP] request budget expired waiting for mutex\r\n");
        (void)liot_rtos_mutex_unlock(s_http_mutex);
        return BIND_ERR_HTTP_MUTEX;
    }
    remaining_ms = timeout_ms - elapsed_ms;

    ret = binding_http_request_unlocked(url_string, method, request_body,
                                        extra_headers, response,
                                        response_capacity, http_status,
                                        remaining_ms,
                                        defer_success_cleanup);
    if (ret == BIND_HTTP_SOCKET_OPEN_FAILED)
    {
        /* SESSION_OPEN failed before any HTTP bytes left the device.  The
         * vendor client is a singleton and its reference demo waits ten
         * seconds after release, so one fresh-client retry is safe even for a
         * POST and avoids turning a transient modem socket failure into a
         * failed call.  Never replay failures from later HTTP stages. */
        elapsed_ms = liot_rtos_get_running_time() - started_ms;
        if (elapsed_ms < timeout_ms &&
            timeout_ms - elapsed_ms >= BIND_HTTP_WAIT_STEP_MS)
        {
            remaining_ms = timeout_ms - elapsed_ms;
            liot_trace("[BIND-HTTP] socket-open failure; fresh-client retry "
                       "budget=%u ms\r\n",
                       (unsigned int)remaining_ms);
            ret = binding_http_request_unlocked(
                url_string, method, request_body, extra_headers, response,
                response_capacity, http_status, remaining_ms,
                defer_success_cleanup);
        }
    }
    if (ret == BIND_HTTP_SOCKET_OPEN_FAILED)
    {
        /* Preserve the public adapter's historical transport-error value. */
        ret = -7;
    }
    if (liot_rtos_mutex_unlock(s_http_mutex) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[BIND-HTTP] mutex unlock failed\r\n");
        if (ret == 0)
        {
            ret = BIND_ERR_HTTP_MUTEX;
        }
    }
    return ret;
}

static int binding_copy_json_string(const cJSON *object,
                                    const char *name,
                                    char *output,
                                    size_t output_size)
{
    const cJSON *item;
    size_t length;

    if (object == NULL || name == NULL || output == NULL || output_size == 0)
    {
        return -1;
    }
    item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL)
    {
        return -1;
    }
    length = strlen(item->valuestring);
    if (length == 0 || length >= output_size)
    {
        return -1;
    }
    memcpy(output, item->valuestring, length + 1U);
    return 0;
}

static int binding_normalize_http_base(char *url)
{
    size_t length;

    if (url == NULL)
    {
        return -1;
    }
    length = strlen(url);
    while (length > 0U && url[length - 1U] == '/')
    {
        url[--length] = '\0';
    }
    /* The current public discovery endpoint advertises plain HTTP.  HTTPS
     * must only be enabled together with the deployment CA configuration. */
    if (length <= 7U || strncmp(url, "http://", 7U) != 0)
    {
        return -1;
    }
    return 0;
}

static int binding_discover_services(void)
{
    char response[BIND_HTTP_RESPONSE_MAX];
    cJSON *root;
    int http_status = 0;

    memset(&s_services, 0, sizeof(s_services));
    if (binding_http_request(BIND_DISCOVERY_URL, LIOT_HTTPC_METHOD_GET,
                             NULL, NULL, response, sizeof(response),
                             &http_status) != 0 || http_status != 200)
    {
        liot_trace("[BIND] service discovery HTTP failed status=%d\r\n",
                   http_status);
        return BIND_ERR_DISCOVERY_HTTP;
    }

    root = cJSON_Parse(response);
    if (root == NULL ||
        binding_copy_json_string(root, "device-srv",
                                 s_services.device_server,
                                 sizeof(s_services.device_server)) != 0 ||
        binding_copy_json_string(root, "ai-srv",
                                 s_services.ai_server,
                                 sizeof(s_services.ai_server)) != 0 ||
        binding_copy_json_string(root, "tirtc-srv",
                                 s_services.tirtc_server,
                                 sizeof(s_services.tirtc_server)) != 0 ||
        binding_copy_json_string(root, "mqtt-srv",
                                 s_services.mqtt_url,
                                 sizeof(s_services.mqtt_url)) != 0)
    {
        if (root != NULL)
        {
            cJSON_Delete(root);
        }
        memset(&s_services, 0, sizeof(s_services));
        memset(response, 0, sizeof(response));
        return BIND_ERR_DISCOVERY_JSON;
    }
    /* Older deployments do not advertise the optional WX VoIP service.
     * Binding/AI must remain usable there, while the WX worker reports OFF. */
    if (binding_copy_json_string(root, "voip-srv",
                                 s_services.voip_server,
                                 sizeof(s_services.voip_server)) != 0)
    {
        s_services.voip_server[0] = '\0';
        liot_trace("[BIND] discovery has no voip-srv; WX disabled\r\n");
    }
    /* Device-to-device calling is a separate call-server business API.  Do
     * not silently route it through voip-srv or the TiRTC media endpoint. */
    if (binding_copy_json_string(root, "call-srv",
                                 s_services.call_server,
                                 sizeof(s_services.call_server)) != 0)
    {
        s_services.call_server[0] = '\0';
        liot_trace("[BIND] discovery has no call-srv; DEV call disabled\r\n");
    }
    cJSON_Delete(root);

    /*
     * The public demo currently advertises plain HTTP/MQTT.  Do not silently
     * downgrade certificate verification if discovery later switches to TLS;
     * add the deployment CA first, then enable those schemes explicitly.
     */
    if (binding_normalize_http_base(s_services.device_server) != 0 ||
        binding_normalize_http_base(s_services.ai_server) != 0 ||
        binding_normalize_http_base(s_services.tirtc_server) != 0 ||
        strncmp(s_services.mqtt_url, "mqtt://", 7) != 0)
    {
        memset(&s_services, 0, sizeof(s_services));
        memset(response, 0, sizeof(response));
        return BIND_ERR_DISCOVERY_JSON;
    }
    if (s_services.voip_server[0] != '\0' &&
        binding_normalize_http_base(s_services.voip_server) != 0)
    {
        s_services.voip_server[0] = '\0';
        liot_trace("[BIND] invalid voip-srv; WX disabled\r\n");
    }
    if (s_services.call_server[0] != '\0' &&
        binding_normalize_http_base(s_services.call_server) != 0)
    {
        s_services.call_server[0] = '\0';
        liot_trace("[BIND] invalid call-srv; DEV call disabled\r\n");
    }

    liot_trace("[BIND] service discovery OK\r\n");
    memset(response, 0, sizeof(response));
    return 0;
}

int demo_binding_service_request(demo_binding_service_e service,
                                 demo_binding_http_method_e method,
                                 const char *path,
                                 const char *json_body,
                                 char *response,
                                 size_t response_size,
                                 int *http_status)
{
    return demo_binding_service_request_timeout(
        service, method, path, json_body, response, response_size,
        http_status, BIND_HTTP_TIMEOUT_MS);
}

int demo_binding_service_request_timeout(
    demo_binding_service_e service,
    demo_binding_http_method_e method,
    const char *path,
    const char *json_body,
    char *response,
    size_t response_size,
    int *http_status,
    uint32_t timeout_ms)
{
    binding_services_t services;
    char mqtt_token[BIND_MQTT_TOKEN_MAX];
    char auth_headers[BIND_AI_AUTH_HEADERS_MAX];
    char request_url[BIND_HTTP_URL_MAX];
    const char *base = NULL;
    liot_httpc_method_e http_method;
    bool ready;
    int written;
    int ret = BIND_ERR_AI_NOT_READY;

    if (path == NULL || path[0] != '/' || response == NULL ||
        response_size < 2U ||
        response_size > (size_t)BIND_AI_HTTP_RESPONSE_MAX ||
        http_status == NULL ||
        timeout_ms < BIND_HTTP_WAIT_STEP_MS || timeout_ms > 120000U ||
        (method != DEMO_BIND_HTTP_GET && method != DEMO_BIND_HTTP_POST))
    {
        return -1;
    }
    response[0] = '\0';
    *http_status = 0;
    memset(&services, 0, sizeof(services));
    memset(mqtt_token, 0, sizeof(mqtt_token));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(request_url, 0, sizeof(request_url));

    liot_rtos_enter_critical();
    ready = s_snapshot.state == DEMO_BIND_BOUND;
    if (ready)
    {
        services = s_services;
    }
    liot_rtos_exit_critical();
    if (!ready || !demo_formal_mqtt_copy_token(mqtt_token,
                                                sizeof(mqtt_token)))
    {
        goto cleanup;
    }

    switch (service)
    {
    case DEMO_BIND_SERVICE_DEVICE:
        base = services.device_server;
        break;
    case DEMO_BIND_SERVICE_AI:
        base = services.ai_server;
        break;
    case DEMO_BIND_SERVICE_VOIP:
        base = services.voip_server;
        break;
    case DEMO_BIND_SERVICE_CALL:
        base = services.call_server;
        break;
    default:
        ret = -1;
        goto cleanup;
    }
    if (base == NULL || base[0] == '\0')
    {
        ret = -2;
        goto cleanup;
    }

    written = snprintf(request_url, sizeof(request_url), "%s%s", base, path);
    if (written < 0 || written >= (int)sizeof(request_url))
    {
        ret = -3;
        goto cleanup;
    }
    /* WRITE_HEADER is sent verbatim by this SDK.  Never log this buffer. */
    written = snprintf(auth_headers, sizeof(auth_headers),
                       "Authorization: Bearer %s", mqtt_token);
    if (written < 0 || written >= (int)sizeof(auth_headers))
    {
        ret = -3;
        goto cleanup;
    }

    http_method = method == DEMO_BIND_HTTP_POST ?
                      LIOT_HTTPC_METHOD_POST : LIOT_HTTPC_METHOD_GET;
    ret = binding_http_request_timeout(
        request_url, http_method, json_body, auth_headers, response,
        (int)response_size, http_status, timeout_ms);

cleanup:
    binding_secure_zero(&services, sizeof(services));
    binding_secure_zero(mqtt_token, sizeof(mqtt_token));
    binding_secure_zero(auth_headers, sizeof(auth_headers));
    binding_secure_zero(request_url, sizeof(request_url));
    return ret;
}

void demo_binding_clear_ai_access(demo_binding_ai_access_t *access)
{
    if (access != NULL)
    {
        binding_secure_zero(access, sizeof(*access));
    }
}

int demo_binding_get_tirtc_identity(demo_binding_tirtc_identity_t *out)
{
    binding_services_t services;
    binding_nvm_record_t credentials;
    bool ready;
    int ret = BIND_ERR_AI_NOT_READY;

    if (out == NULL)
    {
        return BIND_ERR_IDENTITY;
    }
    memset(out, 0, sizeof(*out));
    memset(&services, 0, sizeof(services));
    memset(&credentials, 0, sizeof(credentials));

    liot_rtos_enter_critical();
    ready = s_snapshot.state == DEMO_BIND_BOUND &&
            s_have_credentials &&
            s_credentials.device_id[0] != '\0' &&
            s_credentials.device_key[0] != '\0' &&
            s_services.tirtc_server[0] != '\0';
    if (ready)
    {
        services = s_services;
        credentials = s_credentials;
    }
    liot_rtos_exit_critical();
    if (!ready)
    {
        goto cleanup;
    }

    if (binding_make_pseudo_mac(out->client_id) != 0 ||
        liot_sim_get_iccid(BIND_SIM_ID, out->iccid,
                           sizeof(out->iccid)) != LIOT_SIM_SUCCESS ||
        out->iccid[0] == '\0')
    {
        ret = BIND_ERR_IDENTITY;
        goto cleanup;
    }
    memcpy(out->device_id, credentials.device_id,
           strlen(credentials.device_id) + 1U);
    memcpy(out->device_key, credentials.device_key,
           strlen(credentials.device_key) + 1U);
    memcpy(out->tirtc_endpoint, services.tirtc_server,
           strlen(services.tirtc_server) + 1U);
    liot_trace("[BIND] TiRTC identity ready device_id_len=%u key_len=%u "
               "client_id_len=%u iccid_len=%u\r\n",
               (unsigned int)strlen(out->device_id),
               (unsigned int)strlen(out->device_key),
               (unsigned int)strlen(out->client_id),
               (unsigned int)strlen(out->iccid));
    ret = 0;

cleanup:
    binding_secure_zero(&services, sizeof(services));
    binding_secure_zero(&credentials, sizeof(credentials));
    if (ret != 0)
    {
        binding_secure_zero(out, sizeof(*out));
    }
    return ret;
}

int demo_binding_fetch_ai_access(demo_binding_ai_access_t *out)
{
    binding_services_t services;
    binding_nvm_record_t credentials;
    char mqtt_token[BIND_MQTT_TOKEN_MAX];
    char auth_headers[BIND_AI_AUTH_HEADERS_MAX];
    char request_url[BIND_HTTP_URL_MAX];
    char response[BIND_AI_HTTP_RESPONSE_MAX];
    cJSON *root = NULL;
    const cJSON *business_code = NULL;
    const cJSON *data = NULL;
    const cJSON *peer_id = NULL;
    const cJSON *token = NULL;
    const cJSON *role_id = NULL;
    const cJSON *app_id = NULL;
    bool ready;
    int http_status = 0;
    int written;
    int ret = BIND_ERR_AI_NOT_READY;

    if (out == NULL)
    {
        return BIND_ERR_AI_JSON;
    }
    demo_binding_clear_ai_access(out);
    memset(&services, 0, sizeof(services));
    memset(&credentials, 0, sizeof(credentials));
    memset(mqtt_token, 0, sizeof(mqtt_token));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(request_url, 0, sizeof(request_url));
    memset(response, 0, sizeof(response));

    /* Snapshot the immutable post-binding inputs together.  Service discovery
     * and credential replacement only occur while the public state is not
     * BOUND, so this critical section prevents a partially updated copy. */
    liot_rtos_enter_critical();
    ready = s_snapshot.state == DEMO_BIND_BOUND &&
            s_have_credentials &&
            s_services.ai_server[0] != '\0' &&
            s_services.tirtc_server[0] != '\0';
    if (ready)
    {
        services = s_services;
        credentials = s_credentials;
    }
    liot_rtos_exit_critical();
    if (!ready || !demo_formal_mqtt_copy_token(mqtt_token,
                                                sizeof(mqtt_token)))
    {
        goto cleanup;
    }

    written = snprintf(request_url, sizeof(request_url), "%s/v1/ai/token",
                       services.ai_server);
    if (written < 0 || written >= (int)sizeof(request_url))
    {
        ret = BIND_ERR_AI_JSON;
        goto cleanup;
    }
    /* WRITE_HEADER is sent verbatim by libliot_https.  Never log this buffer. */
    written = snprintf(auth_headers, sizeof(auth_headers),
                       "Authorization: Bearer %s", mqtt_token);
    if (written < 0 || written >= (int)sizeof(auth_headers))
    {
        ret = BIND_ERR_AI_JSON;
        goto cleanup;
    }

    if (binding_http_request_deferred(request_url, LIOT_HTTPC_METHOD_GET,
                                      NULL, auth_headers, response,
                                      sizeof(response), &http_status) != 0 ||
        http_status != 200)
    {
        liot_trace("[BIND-AI] token HTTP failed status=%d\r\n",
                   http_status);
        ret = BIND_ERR_AI_HTTP;
        goto cleanup;
    }

    root = cJSON_Parse(response);
    business_code = (root != NULL)
                        ? cJSON_GetObjectItemCaseSensitive(root, "code")
                        : NULL;
    data = (root != NULL)
               ? cJSON_GetObjectItemCaseSensitive(root, "data")
               : NULL;
    peer_id = cJSON_IsObject(data)
                  ? cJSON_GetObjectItemCaseSensitive(data, "peer_id")
                  : NULL;
    token = cJSON_IsObject(data)
                ? cJSON_GetObjectItemCaseSensitive(data, "token")
                : NULL;
    role_id = cJSON_IsObject(data)
                  ? cJSON_GetObjectItemCaseSensitive(data, "role_id")
                  : NULL;
    app_id = cJSON_IsObject(data)
                 ? cJSON_GetObjectItemCaseSensitive(data, "app_id")
                 : NULL;
    if (!cJSON_IsNumber(business_code) ||
        (business_code->valueint != 0 && business_code->valueint != 200) ||
        !cJSON_IsObject(data) ||
        binding_copy_json_string(data, "peer_id", out->peer_id,
                                 sizeof(out->peer_id)) != 0 ||
        binding_copy_json_string(data, "token", out->token,
                                 sizeof(out->token)) != 0 ||
        binding_copy_json_string(data, "role_id", out->role_id,
                                 sizeof(out->role_id)) != 0 ||
        (app_id != NULL && !cJSON_IsNull(app_id) &&
         (!cJSON_IsString(app_id) || app_id->valuestring == NULL ||
          (app_id->valuestring[0] != '\0' &&
           binding_copy_json_string(data, "app_id", out->app_id,
                                    sizeof(out->app_id)) != 0))))
    {
        liot_trace("[BIND-AI] token response invalid\r\n");
        ret = BIND_ERR_AI_JSON;
        goto cleanup;
    }

    memcpy(out->device_id, credentials.device_id,
           strlen(credentials.device_id) + 1U);
    memcpy(out->device_key, credentials.device_key,
           strlen(credentials.device_key) + 1U);
    memcpy(out->tirtc_endpoint, services.tirtc_server,
           strlen(services.tirtc_server) + 1U);
    liot_trace("[BIND-AI] credentials ready peer_len=%u token_len=%u role_len=%u app_id=%s\r\n",
               (unsigned int)strlen(out->peer_id),
               (unsigned int)strlen(out->token),
               (unsigned int)strlen(out->role_id),
               out->app_id[0] != '\0' ? "present" : "absent");
    ret = 0;

cleanup:
    /* cJSON owns duplicated strings.  Scrub credential values before free so
     * a later heap allocation cannot observe stale JWT material. */
    if (cJSON_IsString(peer_id) && peer_id->valuestring != NULL)
    {
        binding_secure_zero(peer_id->valuestring, strlen(peer_id->valuestring));
    }
    if (cJSON_IsString(token) && token->valuestring != NULL)
    {
        binding_secure_zero(token->valuestring, strlen(token->valuestring));
    }
    if (cJSON_IsString(role_id) && role_id->valuestring != NULL)
    {
        binding_secure_zero(role_id->valuestring, strlen(role_id->valuestring));
    }
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    binding_secure_zero(&services, sizeof(services));
    binding_secure_zero(&credentials, sizeof(credentials));
    binding_secure_zero(mqtt_token, sizeof(mqtt_token));
    binding_secure_zero(auth_headers, sizeof(auth_headers));
    binding_secure_zero(request_url, sizeof(request_url));
    binding_secure_zero(response, sizeof(response));
    if (ret != 0)
    {
        demo_binding_clear_ai_access(out);
    }
    return ret;
}

static bool binding_code_is_valid(const char *code)
{
    int i;

    if (code == NULL || strlen(code) != 6U)
    {
        return false;
    }
    for (i = 0; i < 6; i++)
    {
        if (code[i] < '0' || code[i] > '9')
        {
            return false;
        }
    }
    return true;
}

static int binding_report_device(bool signed_request)
{
    char mac[18];
    char request_body[40];
    char report_url[256];
    char response[BIND_HTTP_RESPONSE_MAX];
    char auth_headers[BIND_AUTH_HEADERS_MAX];
    cJSON *root = NULL;
    const cJSON *business_code;
    const cJSON *data;
    int http_status = 0;
    int ret = BIND_ERR_REPORT_JSON;

    memset(mac, 0, sizeof(mac));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(&s_report, 0, sizeof(s_report));
    if (binding_make_pseudo_mac(mac) != 0)
    {
        return BIND_ERR_IDENTITY;
    }
    if (snprintf(request_body, sizeof(request_body),
                 "{\"mac\":\"%s\"}", mac) >= (int)sizeof(request_body) ||
        snprintf(report_url, sizeof(report_url), "%s/v1/device/report",
                 s_services.device_server) >= (int)sizeof(report_url))
    {
        return BIND_ERR_REPORT_JSON;
    }

    if (signed_request && binding_build_auth_headers(auth_headers,
                                                      sizeof(auth_headers)) != 0)
    {
        ret = BIND_ERR_SIGNATURE;
        goto cleanup;
    }

    if (binding_http_request(report_url, LIOT_HTTPC_METHOD_POST,
                             request_body,
                             signed_request ? auth_headers : NULL,
                             response, sizeof(response),
                             &http_status) != 0 || http_status != 200)
    {
        liot_trace("[BIND] device report HTTP failed status=%d\r\n",
                   http_status);
        ret = BIND_ERR_REPORT_HTTP;
        goto cleanup;
    }

    root = cJSON_Parse(response);
    business_code = (root != NULL)
                        ? cJSON_GetObjectItemCaseSensitive(root, "code")
                        : NULL;
    data = (root != NULL)
               ? cJSON_GetObjectItemCaseSensitive(root, "data")
               : NULL;
    if (!cJSON_IsNumber(business_code) || business_code->valueint != 200 ||
        !cJSON_IsObject(data) ||
        binding_copy_json_string(data, "code", s_report.code,
                                 sizeof(s_report.code)) != 0 ||
        binding_copy_json_string(data, "temp_client_id",
                                 s_report.temp_client_id,
                                 sizeof(s_report.temp_client_id)) != 0 ||
        binding_copy_json_string(data, "temp_token", s_report.temp_token,
                                 sizeof(s_report.temp_token)) != 0 ||
        !binding_code_is_valid(s_report.code))
    {
        ret = BIND_ERR_REPORT_JSON;
        goto cleanup;
    }

    liot_trace("[BIND] report OK: code issued, temporary credentials hidden\r\n");
    ret = 0;

cleanup:
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    if (ret != 0)
    {
        memset(&s_report, 0, sizeof(s_report));
    }
    memset(mac, 0, sizeof(mac));
    memset(request_body, 0, sizeof(request_body));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(response, 0, sizeof(response));
    return ret;
}

static int binding_verify_server_binding(char *mqtt_token_out,
                                         size_t mqtt_token_out_size)
{
    char token_url[256];
    char auth_headers[BIND_AUTH_HEADERS_MAX];
    char response[BIND_HTTP_RESPONSE_MAX];
    cJSON *root = NULL;
    const cJSON *business_code;
    const cJSON *data;
    const cJSON *mqtt_token;
    int http_status = 0;
    int code = 0;
    int ret = BIND_ERR_TOKEN_JSON;
    size_t token_len;

    memset(token_url, 0, sizeof(token_url));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(response, 0, sizeof(response));
    if (mqtt_token_out == NULL || mqtt_token_out_size == 0U ||
        !s_have_credentials ||
        snprintf(token_url, sizeof(token_url), "%s/v1/device/token",
                 s_services.device_server) >= (int)sizeof(token_url))
    {
        ret = BIND_ERR_TOKEN_JSON;
        goto cleanup;
    }
    mqtt_token_out[0] = '\0';
    ret = binding_build_auth_headers(auth_headers, sizeof(auth_headers));
    if (ret != 0)
    {
        goto cleanup;
    }

    /*
     * The F6D_A HTTP worker closes a POST with Content-Length 0 before it can
     * receive the response.  The server ignores an empty JSON object here,
     * while its HMAC contract remains device_id + timestamp + nonce.
     */
    if (binding_http_request(token_url, LIOT_HTTPC_METHOD_POST,
                             "{}", auth_headers,
                             response, sizeof(response),
                             &http_status) != 0)
    {
        liot_trace("[BIND] token HTTP transport failed\r\n");
        ret = BIND_ERR_TOKEN_HTTP;
        goto cleanup;
    }

    root = cJSON_Parse(response);
    business_code = (root != NULL)
                        ? cJSON_GetObjectItemCaseSensitive(root, "code")
                        : NULL;
    if (cJSON_IsNumber(business_code))
    {
        code = business_code->valueint;
    }
    if (http_status == 410 && code == 6006)
    {
        liot_trace("[BIND] server reports device unbound; rebind required\r\n");
        ret = BIND_TOKEN_UNBOUND;
        goto cleanup;
    }

    data = (root != NULL)
               ? cJSON_GetObjectItemCaseSensitive(root, "data")
               : NULL;
    mqtt_token = cJSON_IsObject(data)
                     ? cJSON_GetObjectItemCaseSensitive(data, "mqtt_token")
                     : NULL;
    if (http_status != 200 || code != 200 ||
        !cJSON_IsString(mqtt_token) || mqtt_token->valuestring == NULL ||
        mqtt_token->valuestring[0] == '\0')
    {
        liot_trace("[BIND] token rejected HTTP=%d code=%d\r\n",
                   http_status, code);
        ret = BIND_ERR_TOKEN_JSON;
        goto cleanup;
    }

    token_len = strlen(mqtt_token->valuestring);
    if (token_len >= mqtt_token_out_size)
    {
        liot_trace("[BIND] mqtt token too long len=%u\r\n",
                   (unsigned int)token_len);
        ret = BIND_ERR_TOKEN_JSON;
        goto cleanup;
    }
    memcpy(mqtt_token_out, mqtt_token->valuestring, token_len + 1U);
    liot_trace("[BIND] server binding verified; mqtt token issued len=%u\r\n",
               (unsigned int)token_len);
    ret = BIND_TOKEN_VALID;

cleanup:
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    memset(token_url, 0, sizeof(token_url));
    memset(auth_headers, 0, sizeof(auth_headers));
    memset(response, 0, sizeof(response));
    if (ret != BIND_TOKEN_VALID && mqtt_token_out != NULL &&
        mqtt_token_out_size > 0U)
    {
        mqtt_token_out[0] = '\0';
    }
    return ret;
}

static void binding_mqtt_signal(void)
{
    if (s_mqtt_sem != NULL)
    {
        liot_rtos_semaphore_release(s_mqtt_sem);
    }
}

static void binding_mqtt_event_cb(liot_mqtt_client_t *client,
                                  int event,
                                  void *arg,
                                  void *data)
{
    int value = (data != NULL) ? *(int *)data : -1;

    (void)client;
    (void)arg;
    switch (event)
    {
    case LIOT_MQTT_CONNECT_EVENT:
        liot_trace("[BIND-MQTT] connect status=%d\r\n", value);
        s_mqtt_connect_ok = (value == 0);
        s_mqtt_connect_done = true;
        binding_mqtt_signal();
        break;

    case LIOT_MQTT_SUB_EVENT:
        /* SUB/PUB event data is a packet id in the Lierda implementation. */
        liot_trace("[BIND-MQTT] SUBACK packet_id=%d\r\n", value);
        s_mqtt_sub_done = true;
        binding_mqtt_signal();
        break;

    case LIOT_MQTT_PUB_EVENT:
        liot_trace("[BIND-MQTT] PUBACK packet_id=%d\r\n", value);
        s_mqtt_ack_done = true;
        binding_mqtt_signal();
        break;

    case LIOT_MQTT_CLOSE_EVENT:
        liot_trace("[BIND-MQTT] close event result=%d\r\n", value);
        s_mqtt_lost = true;
        binding_mqtt_signal();
        break;

    case LIOT_MQTT_DISCONNECT_EVENT:
        liot_trace("[BIND-MQTT] disconnect event result=%d\r\n", value);
        s_mqtt_disconnect_done = true;
        s_mqtt_lost = true;
        binding_mqtt_signal();
        break;

    default:
        break;
    }
}

static void binding_mqtt_exception_cb(liot_mqtt_client_t *client)
{
    (void)client;
    s_mqtt_lost = true;
    binding_mqtt_signal();
}

static void binding_mqtt_incoming_cb(liot_mqtt_client_t *client,
                                     void *arg,
                                     int packet_id,
                                     const char *topic,
                                     const unsigned char *payload,
                                     unsigned short payload_len)
{
    cJSON *root;
    const cJSON *type;
    const cJSON *grant;
    const cJSON *device_id;
    const cJSON *device_key;
    size_t id_len;
    size_t key_len;

    (void)client;
    (void)arg;
    (void)packet_id;
    if (topic == NULL || strcmp(topic, s_cmd_topic) != 0 ||
        payload == NULL || payload_len == 0 || s_grant_ready)
    {
        return;
    }

    liot_trace("[BIND-MQTT] command received bytes=%u\r\n",
               (unsigned int)payload_len);

    root = cJSON_ParseWithLength((const char *)payload, payload_len);
    type = (root != NULL)
               ? cJSON_GetObjectItemCaseSensitive(root, "type")
               : NULL;
    if (!cJSON_IsString(type) || type->valuestring == NULL ||
        strcmp(type->valuestring, "auth_grant") != 0)
    {
        if (root != NULL)
        {
            cJSON_Delete(root);
        }
        return;
    }

    grant = cJSON_GetObjectItemCaseSensitive(root, "payload");
    device_id = cJSON_IsObject(grant)
                    ? cJSON_GetObjectItemCaseSensitive(grant, "device_id")
                    : NULL;
    device_key = cJSON_IsObject(grant)
                     ? cJSON_GetObjectItemCaseSensitive(grant, "device_key")
                     : NULL;
    if (!cJSON_IsString(device_id) || device_id->valuestring == NULL ||
        !cJSON_IsString(device_key) || device_key->valuestring == NULL)
    {
        if (s_have_credentials && device_id == NULL && device_key == NULL)
        {
            s_grant_use_existing = true;
            s_grant_ready = true;
            cJSON_Delete(root);
            liot_trace("[BIND] empty auth_grant; reusing local credentials\r\n");
            binding_mqtt_signal();
            return;
        }
        liot_trace("[BIND] auth_grant credentials invalid\r\n");
        s_grant_invalid = true;
        cJSON_Delete(root);
        binding_mqtt_signal();
        return;
    }

    id_len = strlen(device_id->valuestring);
    key_len = strlen(device_key->valuestring);
    if (id_len == 0 || id_len >= sizeof(s_grant_device_id) ||
        key_len == 0 || key_len >= sizeof(s_grant_device_key))
    {
        s_grant_invalid = true;
        cJSON_Delete(root);
        binding_mqtt_signal();
        return;
    }

    memcpy(s_grant_device_id, device_id->valuestring, id_len + 1U);
    memcpy(s_grant_device_key, device_key->valuestring, key_len + 1U);
    s_grant_ready = true;
    cJSON_Delete(root);
    liot_trace("[BIND] auth_grant received; secret fields hidden\r\n");
    binding_mqtt_signal();
}

static void binding_mqtt_reset_flags(void)
{
    s_mqtt_connect_done = false;
    s_mqtt_connect_ok = false;
    s_mqtt_sub_done = false;
    s_mqtt_ack_done = false;
    s_mqtt_disconnect_done = false;
    s_mqtt_lost = false;
    s_grant_ready = false;
    s_grant_invalid = false;
    s_grant_use_existing = false;
    memset(s_grant_device_id, 0, sizeof(s_grant_device_id));
    memset(s_grant_device_key, 0, sizeof(s_grant_device_key));
}

static bool binding_mqtt_state_is_stopped(int state)
{
    return state == MQTT_CONN_DEFAULT ||
           state == MQTT_CONN_NOT_OPEN ||
           state == MQTT_CONN_OPEN_FAIL ||
           state == MQTT_CONN_CONNECT_FAIL ||
           state == MQTT_CONN_CLOSED ||
           state == MQTT_CONN_CLOSED_FAIL ||
           state == MQTT_CONN_DISCONNECTED ||
           state == MQTT_CONN_DISCONNECTED_FAIL ||
           state == MQTT_CONN_RECONNECTING_FAIL;
}

static int binding_mqtt_cleanup(void)
{
    liot_sem_t old_sem;
    uint32_t timeout;
    int state;
    int ret = LIOT_MQTTCLIENT_SUCCESS;

    if (s_mqtt_client != 0)
    {
        state = liot_mqtt_client_state(&s_mqtt_client);
        liot_trace("[BIND-MQTT] cleanup begin state=%d\r\n", state);
        s_mqtt_disconnect_done = false;
        if (state == MQTT_CONN_CONNECTED)
        {
            ret = liot_mqtt_disconnect(&s_mqtt_client, NULL, NULL);
            liot_trace("[BIND-MQTT] disconnect submit ret=%d\r\n", ret);
        }

        state = liot_mqtt_client_state(&s_mqtt_client);
        if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK ||
            state == MQTT_CONN_IS_DISCONNECTING)
        {
            timeout = liot_rtos_get_running_time() +
                      BIND_MQTT_DISCONNECT_MS;
            while ((int32_t)(timeout - liot_rtos_get_running_time()) > 0)
            {
                state = liot_mqtt_client_state(&s_mqtt_client);
                if (s_mqtt_disconnect_done ||
                    binding_mqtt_state_is_stopped(state))
                {
                    break;
                }
                if (s_mqtt_sem != NULL)
                {
                    liot_rtos_semaphore_wait(s_mqtt_sem, 250);
                }
                else
                {
                    liot_rtos_task_sleep_ms(250);
                }
            }
        }

        state = liot_mqtt_client_state(&s_mqtt_client);
        liot_trace("[BIND-MQTT] disconnect settled state=%d done=%d\r\n",
                   state, s_mqtt_disconnect_done ? 1 : 0);
        if (!binding_mqtt_state_is_stopped(state))
        {
            s_mqtt_cleanup_pending = true;
            liot_trace("[BIND-MQTT] cleanup deferred: client still running\r\n");
            return LIOT_MQTTCLIENT_WOUNDBLOCK;
        }

        /* Follow the Lierda MQTT example: let DISCONNECT settle before
         * submitting deinit.  The vendor port returns -2 when command 7 has
         * been queued successfully; it does not guarantee another CLOSE
         * event.  Keep the static handle and semaphore alive for a bounded
         * retirement interval and never dereference the handle after -2. */
        liot_trace("[BIND-MQTT] settle before deinit %u ms\r\n",
                   (unsigned int)BIND_MQTT_SETTLE_MS);
        liot_rtos_task_sleep_ms(BIND_MQTT_SETTLE_MS);
        liot_trace("[BIND-MQTT] deinit begin\r\n");
        ret = liot_mqtt_client_deinit(&s_mqtt_client);
        liot_trace("[BIND-MQTT] deinit submit ret=%d\r\n", ret);
        if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
        {
            s_mqtt_cleanup_pending = true;
            liot_trace("[BIND-MQTT] async deinit accepted; retire %u ms\r\n",
                       (unsigned int)BIND_MQTT_ASYNC_DEINIT_MS);
            liot_rtos_task_sleep_ms(BIND_MQTT_ASYNC_DEINIT_MS);
            ret = LIOT_MQTTCLIENT_SUCCESS;
        }
        if (ret != LIOT_MQTTCLIENT_SUCCESS)
        {
            /* A real submission failure may leave the worker owning the
             * callback. Keep both handle and semaphore for a later retry. */
            s_mqtt_cleanup_pending = true;
            liot_trace("[BIND-MQTT] deinit failed ret=%d; resources retained\r\n",
                       ret);
            return ret;
        }

        s_mqtt_client = 0;
        s_mqtt_cleanup_pending = false;
    }
    if (s_mqtt_sem != NULL)
    {
        old_sem = s_mqtt_sem;
        if (liot_rtos_semaphore_delete(old_sem) != LIOT_OSI_SUCCESS)
        {
            /* The client is gone, so no callback can use the semaphore now.
             * Retain its handle and let the next binding retry delete it. */
            s_mqtt_cleanup_pending = true;
            liot_trace("[BIND-MQTT] semaphore delete failed; retained for retry\r\n");
            memset(s_report.temp_token, 0, sizeof(s_report.temp_token));
            memset(s_grant_device_key, 0, sizeof(s_grant_device_key));
            return LIOT_MQTTCLIENT_INVALID_PARAM;
        }
        s_mqtt_sem = NULL;
    }
    memset(s_report.temp_token, 0, sizeof(s_report.temp_token));
    memset(s_grant_device_key, 0, sizeof(s_grant_device_key));
    s_mqtt_cleanup_pending = false;
    return LIOT_MQTTCLIENT_SUCCESS;
}

static uint16_t binding_seconds_remaining(uint32_t deadline)
{
    uint32_t now = liot_rtos_get_running_time();
    int32_t delta_ms = (int32_t)(deadline - now);
    uint32_t remaining_ms = (delta_ms > 0) ? (uint32_t)delta_ms : 0U;
    uint32_t remaining = (remaining_ms + 999U) / 1000U;

    return (remaining > UINT16_MAX) ? UINT16_MAX : (uint16_t)remaining;
}

static bool binding_deadline_pending(uint32_t deadline)
{
    return (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

static int binding_wait_mqtt_connected(uint32_t deadline)
{
    uint32_t timeout = liot_rtos_get_running_time() +
                       BIND_MQTT_CONNECT_MS;

    while (binding_deadline_pending(timeout) &&
           binding_deadline_pending(deadline))
    {
        if (liot_mqtt_client_state(&s_mqtt_client) == MQTT_CONN_CONNECTED)
        {
            return 0;
        }
        if ((s_mqtt_connect_done && !s_mqtt_connect_ok) || s_mqtt_lost)
        {
            return -1;
        }
        binding_snapshot_set_seconds(binding_seconds_remaining(deadline));
        liot_rtos_semaphore_wait(s_mqtt_sem, 250);
    }
    return -1;
}

static int binding_wait_subscribed(uint32_t deadline)
{
    uint32_t timeout = liot_rtos_get_running_time() + BIND_MQTT_SUB_MS;

    while (binding_deadline_pending(timeout) &&
           binding_deadline_pending(deadline))
    {
        if (s_mqtt_sub_done)
        {
            return 0;
        }
        if (s_mqtt_lost)
        {
            return -1;
        }
        binding_snapshot_set_seconds(binding_seconds_remaining(deadline));
        liot_rtos_semaphore_wait(s_mqtt_sem, 250);
    }
    return -1;
}

static int binding_send_grant_ack(void)
{
    static const char ack[] = "{\"ack\":true}";
    char ack_topic[128];
    uint32_t timeout;
    int ret;

    if (snprintf(ack_topic, sizeof(ack_topic), "device/%s/ack",
                 s_report.temp_client_id) >= (int)sizeof(ack_topic))
    {
        return -1;
    }

    s_mqtt_ack_done = false;
    ret = liot_mqtt_publish(&s_mqtt_client, ack_topic, ack,
                            (unsigned short)strlen(ack), 1, 0,
                            NULL, NULL);
    if (ret == LIOT_MQTTCLIENT_SUCCESS)
    {
        return 0;
    }
    if (ret != LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        return -1;
    }

    timeout = liot_rtos_get_running_time() + BIND_MQTT_ACK_MS;
    while (binding_deadline_pending(timeout))
    {
        if (s_mqtt_ack_done)
        {
            return 0;
        }
        if (s_mqtt_lost)
        {
            return -1;
        }
        liot_rtos_semaphore_wait(s_mqtt_sem, 250);
    }
    return -1;
}

static int binding_wait_for_grant(uint32_t deadline)
{
    liot_mqtt_client_option options;
    int ret;

    /* A previous asynchronous deinit must finish before these globals are
     * reused.  Overwriting them would orphan the old worker and its callback. */
    if (s_mqtt_cleanup_pending || s_mqtt_client != 0 || s_mqtt_sem != NULL)
    {
        ret = binding_mqtt_cleanup();
        if (ret != LIOT_MQTTCLIENT_SUCCESS)
        {
            liot_trace("[BIND-MQTT] previous cleanup not complete ret=%d\r\n",
                       ret);
            return BIND_ERR_MQTT_INIT;
        }
    }

    binding_mqtt_reset_flags();
    memset(&options, 0, sizeof(options));
    s_mqtt_client = 0;
    s_mqtt_sem = NULL;

    if (snprintf(s_cmd_topic, sizeof(s_cmd_topic), "device/%s/cmd",
                 s_report.temp_client_id) >= (int)sizeof(s_cmd_topic) ||
        liot_rtos_semaphore_create(&s_mqtt_sem, 0) != LIOT_OSI_SUCCESS)
    {
        return BIND_ERR_MQTT_INIT;
    }
    if (liot_mqtt_client_init_ex(&s_mqtt_client, BIND_PDP_CID,
                                 binding_mqtt_event_cb, NULL) !=
        LIOT_MQTTCLIENT_SUCCESS)
    {
        (void)binding_mqtt_cleanup();
        return BIND_ERR_MQTT_INIT;
    }
    ret = liot_mqtt_set_inpub_callback(&s_mqtt_client,
                                       binding_mqtt_incoming_cb, NULL);
    if (ret != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("[BIND-MQTT] incoming callback setup failed ret=%d\r\n",
                   ret);
        (void)binding_mqtt_cleanup();
        return BIND_ERR_MQTT_INIT;
    }

    options.version = LIOT_MQTT_VERSION_4;
    options.pdp_cid = BIND_PDP_CID;
    options.client_id = s_report.temp_client_id;
    options.client_user = s_report.temp_client_id;
    options.client_pass = s_report.temp_token;
    options.clean_session = 1;
    options.kalive_time = 60;
    options.delivery_time = 5;
    options.delivery_cnt = 3;
    options.ping_timeout = 5;

    ret = liot_mqtt_connect(&s_mqtt_client, s_services.mqtt_url,
                            NULL, NULL, &options,
                            binding_mqtt_exception_cb);
    if (ret != LIOT_MQTTCLIENT_SUCCESS &&
        ret != LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        (void)binding_mqtt_cleanup();
        return BIND_ERR_MQTT_CONNECT;
    }
    if (binding_wait_mqtt_connected(deadline) != 0)
    {
        (void)binding_mqtt_cleanup();
        return BIND_ERR_MQTT_CONNECT;
    }

    ret = liot_mqtt_sub_unsub(&s_mqtt_client, s_cmd_topic, 1,
                              NULL, NULL, 1);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        if (binding_wait_subscribed(deadline) != 0)
        {
            (void)binding_mqtt_cleanup();
            return BIND_ERR_MQTT_SUBSCRIBE;
        }
    }
    else if (ret != LIOT_MQTTCLIENT_SUCCESS)
    {
        (void)binding_mqtt_cleanup();
        return BIND_ERR_MQTT_SUBSCRIBE;
    }

    /* Do not expose the code until the device can actually receive grant. */
    binding_snapshot_set(DEMO_BIND_WAIT_GRANT, s_report.code,
                         binding_seconds_remaining(deadline), BIND_ERR_NONE);
    liot_trace("[BIND] temporary MQTT online; waiting for auth_grant\r\n");
    while (binding_deadline_pending(deadline))
    {
        binding_snapshot_set_seconds(binding_seconds_remaining(deadline));
        if (s_grant_invalid)
        {
            (void)binding_mqtt_cleanup();
            return BIND_ERR_GRANT_JSON;
        }
        if (s_grant_ready)
        {
            if (!s_grant_use_existing &&
                binding_store_credentials(s_grant_device_id,
                                          s_grant_device_key) != 0)
            {
                (void)binding_mqtt_cleanup();
                return BIND_ERR_STORAGE;
            }
            if (binding_send_grant_ack() != 0)
            {
                (void)binding_mqtt_cleanup();
                return BIND_ERR_ACK;
            }
            liot_trace("[BIND] credentials ready and grant ACK confirmed\r\n");
            /* Leave the countdown page immediately.  MQTT cleanup below is
             * deliberately conservative and can take several seconds. */
            binding_snapshot_set(DEMO_BIND_VERIFYING, NULL, 0,
                                 BIND_ERR_NONE);
            if (binding_mqtt_cleanup() != LIOT_MQTTCLIENT_SUCCESS)
            {
                return BIND_ERR_MQTT_INIT;
            }
            return 0;
        }
        if (s_mqtt_lost)
        {
            (void)binding_mqtt_cleanup();
            return BIND_ERR_MQTT_CONNECT;
        }
        liot_rtos_semaphore_wait(s_mqtt_sem, 250);
    }

    (void)binding_mqtt_cleanup();
    return BIND_ERR_GRANT_TIMEOUT;
}

static int binding_start_formal_mqtt(char *mqtt_token,
                                     size_t mqtt_token_size)
{
    int ret;

    ret = demo_formal_mqtt_connect(s_services.mqtt_url,
                                   s_credentials.device_id,
                                   mqtt_token);
    memset(mqtt_token, 0, mqtt_token_size);
    if (ret != 0)
    {
        liot_trace("[BIND] permanent MQTT connect failed\r\n");
        return BIND_ERR_FORMAL_MQTT;
    }

    binding_snapshot_set(DEMO_BIND_BOUND, NULL, 0, BIND_ERR_NONE);
    liot_trace("[BIND] provisioning complete; permanent MQTT online\r\n");
    return 0;
}

static int binding_reboot_after_unbind(void)
{
    liot_power_errcode_e power_ret;

    binding_snapshot_set(DEMO_BIND_RESTARTING, NULL, 0, BIND_ERR_NONE);
    liot_trace("[BIND] remote unbind confirmed; preserving device ID/key\r\n");
    /* Give the best-effort cmd ACK time to leave the MQTT worker before
     * tearing down the connection.  Never reset from inside its callback. */
    liot_rtos_task_sleep_ms(500);
    demo_formal_mqtt_disconnect();
    liot_rtos_task_sleep_ms(500);
    liot_trace("[BIND] rebooting into signed rebind flow\r\n");
    power_ret = liot_power_reset(LIOT_RESET_NORMAL);
    liot_trace("[BIND] reboot returned unexpectedly ret=0x%x\r\n",
               (unsigned int)power_ret);
    return BIND_ERR_RESTART;
}

void demo_binding_task(void *argv)
{
    uint32_t handled_request = 0;
    uint32_t request;
    uint32_t deadline;
    char mqtt_token[BIND_MQTT_TOKEN_MAX];
    liot_mutex_t http_mutex;
    bool signed_rebind;
    int ret;

    (void)argv;
    while (s_http_mutex == NULL)
    {
        http_mutex = NULL;
        if (liot_rtos_mutex_create(&http_mutex) == LIOT_OSI_SUCCESS)
        {
            s_http_mutex = http_mutex;
            break;
        }
        liot_trace("[BIND-HTTP] mutex create failed; retrying\r\n");
        liot_rtos_task_sleep_s(1);
    }
    memset(mqtt_token, 0, sizeof(mqtt_token));
    memset(&s_credentials, 0, sizeof(s_credentials));
    s_have_credentials = false;
    if (binding_load_credentials() == 0)
    {
        liot_trace("[BIND] persistent credentials found; server check pending\r\n");
    }
    else
    {
        liot_trace("[BIND] no persistent credentials; auto provisioning\r\n");
    }
    binding_snapshot_set(DEMO_BIND_IDLE, NULL, 0, BIND_ERR_NONE);
    demo_binding_request();

    while (1)
    {
        if (s_snapshot.state == DEMO_BIND_BOUND)
        {
            if (demo_formal_mqtt_take_unbind())
            {
                ret = binding_reboot_after_unbind();
                binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                continue;
            }

            /* The LIOT client may reconnect internally.  Restore its
             * persistent subscriptions before deciding the owner is lost. */
            demo_formal_mqtt_maintenance();
            if (!demo_formal_mqtt_is_online())
            {
                /* The server can kick the formal client immediately after an
                 * unbind publish.  If the message itself was lost, /token's
                 * 6006 response is the authoritative fallback. */
                liot_trace("[BIND] permanent MQTT lost; validating token\r\n");
                demo_formal_mqtt_disconnect();
                binding_snapshot_set(DEMO_BIND_VERIFYING, NULL, 0,
                                     BIND_ERR_NONE);
                memset(mqtt_token, 0, sizeof(mqtt_token));
                ret = binding_verify_server_binding(mqtt_token,
                                                    sizeof(mqtt_token));
                if (ret == BIND_TOKEN_UNBOUND)
                {
                    ret = binding_reboot_after_unbind();
                    binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                    continue;
                }
                if (ret != BIND_TOKEN_VALID)
                {
                    memset(mqtt_token, 0, sizeof(mqtt_token));
                    binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                    continue;
                }
                ret = binding_start_formal_mqtt(mqtt_token,
                                                sizeof(mqtt_token));
                if (ret != 0)
                {
                    binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                }
                continue;
            }

            liot_rtos_task_sleep_ms(500);
            continue;
        }

        liot_rtos_enter_critical();
        request = s_request_sequence;
        liot_rtos_exit_critical();
        if (request == handled_request)
        {
            if (s_snapshot.state == DEMO_BIND_ERROR)
            {
                liot_trace("[BIND] automatic retry in %u seconds\r\n",
                           (unsigned int)BIND_ERROR_RETRY_SECONDS);
                liot_rtos_task_sleep_s(BIND_ERROR_RETRY_SECONDS);
                demo_binding_request();
            }
            else
            {
                liot_rtos_task_sleep_ms(200);
            }
            continue;
        }
        handled_request = request;

        binding_snapshot_set(DEMO_BIND_WAIT_NETWORK, NULL, 0,
                             BIND_ERR_NONE);
        while (!demo_net_time_is_data_ready() ||
               demo_net_time_get_state() != DEMO_NET_READY)
        {
            liot_rtos_task_sleep_s(1);
        }

        /* Every retry, including the stored-credentials path, must finish a
         * previous temporary MQTT retirement before formal MQTT can start. */
        if (s_mqtt_cleanup_pending || s_mqtt_client != 0 ||
            s_mqtt_sem != NULL)
        {
            ret = binding_mqtt_cleanup();
            if (ret != LIOT_MQTTCLIENT_SUCCESS)
            {
                liot_trace("[BIND-MQTT] retry cleanup not complete ret=%d\r\n",
                           ret);
                binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0,
                                     BIND_ERR_MQTT_INIT);
                continue;
            }
        }

        binding_snapshot_set(DEMO_BIND_DISCOVERING, NULL, 0,
                             BIND_ERR_NONE);
        ret = binding_discover_services();
        if (ret != 0)
        {
            binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
            continue;
        }

        signed_rebind = false;
        if (s_have_credentials)
        {
            binding_snapshot_set(DEMO_BIND_VERIFYING, NULL, 0,
                                 BIND_ERR_NONE);
            memset(mqtt_token, 0, sizeof(mqtt_token));
            ret = binding_verify_server_binding(mqtt_token,
                                                sizeof(mqtt_token));
            if (ret == BIND_TOKEN_VALID)
            {
                ret = binding_start_formal_mqtt(mqtt_token,
                                                sizeof(mqtt_token));
                if (ret != 0)
                {
                    binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                }
                continue;
            }
            memset(mqtt_token, 0, sizeof(mqtt_token));
            if (ret != BIND_TOKEN_UNBOUND)
            {
                binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
                continue;
            }
            signed_rebind = true;
        }

        binding_snapshot_set(DEMO_BIND_REPORTING, NULL, 0,
                             BIND_ERR_NONE);
        ret = binding_report_device(signed_rebind);
        if (ret != 0)
        {
            binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
            continue;
        }

        deadline = liot_rtos_get_running_time() + BIND_CODE_TTL_MS;
        ret = binding_wait_for_grant(deadline);
        if (ret != 0)
        {
            memset(&s_report, 0, sizeof(s_report));
            binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
            continue;
        }

        memset(&s_report, 0, sizeof(s_report));
        binding_snapshot_set(DEMO_BIND_VERIFYING, NULL, 0,
                             BIND_ERR_NONE);
        memset(mqtt_token, 0, sizeof(mqtt_token));
        ret = binding_verify_server_binding(mqtt_token,
                                            sizeof(mqtt_token));
        if (ret != BIND_TOKEN_VALID)
        {
            memset(mqtt_token, 0, sizeof(mqtt_token));
            binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0,
                                 (ret == BIND_TOKEN_UNBOUND)
                                     ? BIND_ERR_TOKEN_JSON
                                     : ret);
            continue;
        }
        ret = binding_start_formal_mqtt(mqtt_token, sizeof(mqtt_token));
        if (ret != 0)
        {
            binding_snapshot_set(DEMO_BIND_ERROR, NULL, 0, ret);
        }
    }
}
