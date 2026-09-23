/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * TiRTC runtime adapter for NT26F6D0/F6D_A.
 *
 * This follows the official minimal-system-examples adapter on NT26F6D0:
 * provisioning supplies a long-lived device identity, the SDK is
 * initialized and started exactly once in device mode, and AI/WX/DEV feature
 * modules share that runtime. A feature leaving its UI page closes only its
 * connection; it never stops or reinitializes TiRTC.
 */

#include "tirtc_runtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_binding.h"
#include "liot_log.h"
#include "liot_os.h"
#include "mbedtls/md.h"
#include "tirtc/tgtrp.h"

#ifndef TIRTC_MAX_SEND_BUFFER_BYTES
#define TIRTC_MAX_SEND_BUFFER_BYTES   (128U * 1024U)
#endif
#ifndef TIRTC_LIBRARY_LOG_LEVEL
#define TIRTC_LIBRARY_LOG_LEVEL       3
#endif
#ifndef TIRTC_TRANSPORT_LOG_LEVEL
#define TIRTC_TRANSPORT_LOG_LEVEL     3
#endif
#ifndef TIRTC_TRANSPORT_STATS_ENABLE
#define TIRTC_TRANSPORT_STATS_ENABLE  0
#endif
#define DEMO_TIRTC_SEND_BUFFER_BYTES  TIRTC_MAX_SEND_BUFFER_BYTES
#define DEMO_TIRTC_RTC_THREAD_STACK_REQUEST_BYTES (16U * 1024U)
#define DEMO_TIRTC_START_TIMEOUT_MS   30000U
#define DEMO_TIRTC_IDENTITY_RETRY_MS  500U
#define DEMO_TIRTC_START_RETRY_MS     5000U
#define DEMO_TIRTC_STOP_DRAIN_TIMEOUT_MS 5000U
#define DEMO_TIRTC_STOP_EVENT_TIMEOUT_MS 10000U
#define DEMO_TIRTC_REJECT_QUEUE_DEPTH 2U
#define DEMO_TIRTC_LOG_LEVEL          TIRTC_LIBRARY_LOG_LEVEL
#define DEMO_TIRTC_RESTRICTED_NETWORK 1
#if TIRTC_TRANSPORT_STATS_ENABLE
#define DEMO_TIRTC_TRANSPORT_LOG_FLAGS \
    (TIRTC_TRANSPORT_LOG_LEVEL | TGTRP_LOG_FLAG_STAT)
#else
#define DEMO_TIRTC_TRANSPORT_LOG_FLAGS TIRTC_TRANSPORT_LOG_LEVEL
#endif
#define DEMO_TIRTC_LOG_CHUNK_BYTES    320U
/*
 * The managed adapter keeps asynchronous connect arguments alive until the
 * SDK callback.  Size these buffers for the largest value any feature may
 * actually submit: WeChat permits 1151-byte service descriptions and tokens,
 * while AI and device calls enforce a 600-byte token ceiling before here.
 * Keeping the limits aligned prevents a valid feature payload from being
 * rejected by this adapter before it reaches libTiRTC.
 */
#define DEMO_TIRTC_CONNECT_TEXT_BYTES 1152U
#define DEMO_TIRTC_CONNECT_TOKEN_BYTES 1152U
#define DEMO_TIRTC_DISCONNECT_RETRIES 3U
#define DEMO_TIRTC_DISCONNECT_RETRY_MS 200U
#define DEMO_TIRTC_DEFAULT_ENDPOINT    "http://ep-tirtc.tange365.com"
#define DEMO_TIRTC_DEFAULT_ENDPOINT_TLS "https://ep-tirtc.tange365.com"

#define DEMO_TIRTC_ERR_NOT_READY      (-2001)
#define DEMO_TIRTC_ERR_INIT           (-2002)
#define DEMO_TIRTC_ERR_OPTION         (-2003)
#define DEMO_TIRTC_ERR_TIMEOUT        (-2005)

static const demo_tirtc_listener_t * volatile
    s_feature_listeners[DEMO_TIRTC_FEATURE_COUNT];
static const demo_tirtc_incoming_listener_t * volatile s_incoming_listener;
static volatile bool s_admission_busy;
static liot_sem_t s_runtime_sem;
static liot_queue_t s_reject_queue;

typedef enum
{
    RUNTIME_REJECT_FREE = 0,
    RUNTIME_REJECT_QUEUED,
    RUNTIME_REJECT_READY,
    RUNTIME_REJECT_SUBMITTED,
    RUNTIME_REJECT_RETRY_WAIT,
} runtime_reject_state_e;

typedef struct
{
    tirtc_conn_t handle;
    runtime_reject_state_e state;
    uint8_t attempts;
    uint32_t retry_at;
} runtime_reject_slot_t;

/* Reject reservations are installed before the callback queues any work.
 * Consequently a synchronous/early on_disconnected can always find the
 * handle, and a queue failure can fail closed without losing ownership. */
static runtime_reject_slot_t
    s_reject_slots[DEMO_TIRTC_REJECT_QUEUE_DEPTH];
static volatile uint32_t s_reject_pending;
static volatile uint32_t s_accept_callbacks_pending;
static volatile bool s_started_event;
static volatile bool s_stopped_event;
static volatile bool s_sdk_initialized;
static volatile bool s_start_submitted;
static volatile bool s_stop_requested;
static volatile bool s_ready;
static volatile int s_error;
static volatile bool s_retrying;
static volatile bool s_restart_required;
static volatile int s_restart_error;

typedef enum
{
    RUNTIME_CONNECT_NONE = 0,
    RUNTIME_CONNECT_DEVICE,
    RUNTIME_CONNECT_WHIP,
} runtime_connect_kind_e;

/*
 * Fixed-memory equivalent of the connection core in the official P4
 * tirtc_adapter.  Every field below is protected by the RTOS critical section.
 * In particular, `closing` is a reservation rather than merely an application
 * handle: it remains non-NULL after TiRtcDisconnect() returns and is cleared
 * only by on_disconnected (or an explicit INVALID_HANDLE result).
 */
typedef struct
{
    demo_tirtc_owner_e owner;
    uint32_t session_generation;
    uint32_t next_session_generation;
    uint32_t connection_generation;

    demo_tirtc_owner_e expected_owner;
    uint32_t expected_generation;

    bool connect_request_pending;
    bool connect_callback_pending;
    demo_tirtc_owner_e pending_owner;
    uint32_t pending_generation;
    uint32_t pending_request_generation;
    uint32_t next_request_generation;
    tirtc_conn_t pending_handle;
    bool pending_handle_released;
    int pending_error;

    tirtc_conn_t active;
    demo_tirtc_owner_e active_owner;
    uint32_t active_generation;
    uint32_t users;

    tirtc_conn_t closing;
    demo_tirtc_owner_e closing_owner;
    uint32_t closing_generation;
    bool disconnect_submitted;
    uint8_t disconnect_attempts;
    uint32_t disconnect_retry_at;
} runtime_connection_state_t;

/* TiRtcConnect/TiRtcWhipConnect borrow their inputs until the asynchronous
 * callback.  One fixed context is sufficient because MAX_CONNECTIONS is one
 * and the adapter rejects a second pending request. */
typedef struct
{
    bool pending;
    runtime_connect_kind_e kind;
    demo_tirtc_owner_e owner;
    uint32_t session_generation;
    uint32_t request_generation;
    TIRTCCONNECTCALLBACK callback;
    void *user_data;
    bool token_present;
    char remote[DEMO_TIRTC_CONNECT_TEXT_BYTES];
    char token[DEMO_TIRTC_CONNECT_TOKEN_BYTES];
} runtime_connect_context_t;

static runtime_connection_state_t s_connection;
static runtime_connect_context_t s_connect_context;
/* A runtime recovery deliberately does not wait for a feature task to release
 * its logical owner: the feature may itself be waiting for SDK teardown.  The
 * retired generation makes that late release idempotent after restart. */
static uint32_t s_retired_session_generation[DEMO_TIRTC_OWNER_COUNT];

/*
 * The supplied target archive creates one high-priority worker named
 * "rtc_thread" for
 * every WHIP connection.  Its platform shim converts the requested byte count
 * to FreeRTOS stack depth, so the archive's 8192-byte request becomes a 16 KiB
 * stack on this 32-bit port.  That worker may run before TiRtcWhipConnect()
 * returns; a stack fault therefore used to reset the module with the last
 * application log stuck at AI CONNECTING.
 *
 * Keep the supplied archive intact and enlarge only that connection worker to
 * a 16 KiB effective FreeRTOS stack.  The wrapper also records task creation
 * before and after the SDK call, which makes any remaining platform failure
 * unambiguous without enabling the archive's incompatible logger.
 */
extern int __real_freertos_ThreadCreateWithStackSize(
    void **thread_handle, void (*entry)(void *), void *argument,
    int stack_size, const char *name);

int __wrap_freertos_ThreadCreateWithStackSize(
    void **thread_handle, void (*entry)(void *), void *argument,
    int stack_size, const char *name)
{
    int effective_stack_size = stack_size;
    int ret;

    if (name != NULL && strcmp(name, "rtc_thread") == 0 &&
        effective_stack_size < (int)DEMO_TIRTC_RTC_THREAD_STACK_REQUEST_BYTES)
    {
        effective_stack_size =
            (int)DEMO_TIRTC_RTC_THREAD_STACK_REQUEST_BYTES;
        liot_trace("[TIRTC-RTOS] rtc_thread create requested=%d "
                   "effective=%d free_heap=%u\r\n",
                   stack_size, effective_stack_size,
                   (unsigned int)liot_xPortGetFreeHeapSize());
    }

    ret = __real_freertos_ThreadCreateWithStackSize(
        thread_handle, entry, argument, effective_stack_size, name);

    if (name != NULL && strcmp(name, "rtc_thread") == 0)
    {
        liot_trace("[TIRTC-RTOS] rtc_thread create ret=%d handle=%p "
                   "free_heap=%u\r\n",
                   ret,
                   thread_handle != NULL ? *thread_handle : NULL,
                   (unsigned int)liot_xPortGetFreeHeapSize());
    }
    return ret;
}

/*
 * rtc_thread emits an unconditional timing diagnostic when a connection
 * callback takes more than 100 ms.  The archive formats that prefix through
 * app_log_time(), whose gettimeofday()/localtime_r() path enters newlib's
 * timezone allocator.  That allocator does not share the OpenCPU RTOS heap
 * and can reset the module during the first WHIP negotiation.
 *
 * Keep the diagnostic itself usable, but provide an empty, valid prefix and
 * avoid the incompatible time conversion path.  This is deliberately scoped
 * to app_log_time references from libTiRTC.a through the linker wrapper.
 */
void __wrap_app_log_time(char *buffer, size_t buffer_size)
{
    if (buffer != NULL && buffer_size > 0U)
    {
        buffer[0] = '\0';
    }
}

/* The supplied archive contains diagnostic formats that can include bearer
 * tokens, decrypted service configuration and long-term keys.  A binary
 * format string is harmless by itself; forwarding its formatted payload to a
 * public UART is not.  Filter every SDK-owned log sink before it reaches the
 * platform logger.  Application logs already report only lengths/presence. */
static char runtime_ascii_lower(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return (char)(value + ('a' - 'A'));
    }
    return value;
}

static bool runtime_log_contains_text(const char *data, size_t length,
                                      const char *needle)
{
    size_t needle_length;
    size_t offset;
    size_t index;

    if (data == NULL || needle == NULL)
    {
        return false;
    }
    needle_length = strlen(needle);
    if (needle_length == 0U || needle_length > length)
    {
        return false;
    }
    for (offset = 0U; offset <= length - needle_length; ++offset)
    {
        for (index = 0U; index < needle_length; ++index)
        {
            if (runtime_ascii_lower(data[offset + index]) !=
                runtime_ascii_lower(needle[index]))
            {
                break;
            }
        }
        if (index == needle_length)
        {
            return true;
        }
    }
    return false;
}

static bool runtime_log_is_sensitive(const char *data, size_t length)
{
    static const char * const sensitive_terms[] = {
        "authorization", "bearer ", "token", "secret", "credential",
        "decrypcfg", "device_key", "private_key", "accesskey",
        /* Level-15 SDK diagnostics contain formatted ICE/TURN credentials.
         * Match the rendered values as well as the SDP field names because
         * these messages may bypass the normal TiRTC log callback. */
        "password", "passwd", "pwd=", "pass=", "ice-pwd", "ice_pwd",
        "ice-ufrag", "ice_ufrag", "nonce=", "signal_listen config=",
    };
    size_t index;

    for (index = 0U;
         index < sizeof(sensitive_terms) / sizeof(sensitive_terms[0]);
         ++index)
    {
        if (runtime_log_contains_text(data, length, sensitive_terms[index]))
        {
            return true;
        }
    }
    return false;
}

/*
 * Several TiRTC objects bypass the SDK log callback and call newlib printf()
 * directly.  The first WHIP offer unconditionally prints
 * "peer_connection state change ..." from the SDK call path. Raw newlib stdout is
 * not compatible with the F6D_A OpenCPU logging context and can reset the
 * module before TiRtcWhipConnect() returns.
 *
 * Drop the two noisy internal timing/state messages and route every other raw
 * printf through the platform logger.  This keeps libTiRTC.a unchanged while
 * avoiding newlib FILE/_impure_ptr on SDK worker threads.
 */
int __wrap_printf(const char *format, ...)
{
    char line[384];
    va_list args;
    int written;

    if (format == NULL)
    {
        return 0;
    }
    if (strncmp(format, "peer_connection state change", 28U) == 0 ||
        strncmp(format, "[RTC_THREAD_STAT]", 17U) == 0)
    {
        return 0;
    }

    va_start(args, format);
    written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (written > 0)
    {
        size_t output_length = (size_t)written;

        line[sizeof(line) - 1U] = '\0';
        if (output_length >= sizeof(line))
        {
            output_length = sizeof(line) - 1U;
        }
        if (!runtime_log_is_sensitive(line, output_length))
        {
            syslogPrintf("%s", line);
        }
    }
    return written;
}

/*
 * Route the SDK callback directly to the OpenCPU logger.  SDK strings are not
 * guaranteed to be NUL terminated, so copy bounded chunks before printing.
 * Keeping this callback avoids the archive's default stdout/file logger and
 * its incompatible gettimeofday()/localtime_r() path on NT26F6D0.
 */
static void tirtc_sdk_log_callback(const char *log, uint32_t length)
{
    char chunk[DEMO_TIRTC_LOG_CHUNK_BYTES + 1U];
    uint32_t offset = 0U;

    if (log == NULL || length == 0U)
    {
        return;
    }
    if (runtime_log_is_sensitive(log, (size_t)length))
    {
        return;
    }

    while (offset < length)
    {
        uint32_t count = length - offset;

        if (count > DEMO_TIRTC_LOG_CHUNK_BYTES)
        {
            count = DEMO_TIRTC_LOG_CHUNK_BYTES;
        }
        memcpy(chunk, log + offset, count);
        chunk[count] = '\0';
        syslogPrintf("[TIRTC-SDK] %s", chunk);
        offset += count;
    }
}

/* Level 15 enables the TiRTC transport/WebRTC logger at transport level 5.
 * Register its va_list callback explicitly so it never falls back to the
 * EC71x archive's raw vprintf path. */
static void tirtc_transport_log_callback(const char *format, va_list args)
{
    char line[384];
    int written;

    if (format == NULL)
    {
        return;
    }
    written = vsnprintf(line, sizeof(line), format, args);
    if (written > 0)
    {
        size_t output_length = (size_t)written;

        line[sizeof(line) - 1U] = '\0';
        if (output_length >= sizeof(line))
        {
            output_length = sizeof(line) - 1U;
        }
        if (!runtime_log_is_sensitive(line, output_length))
        {
            syslogPrintf("[TIRTC-TRP] %s%s",
                         line,
                         (strchr(line, '\n') != NULL ||
                          strchr(line, '\r') != NULL) ? "" : "\r\n");
        }
    }
}

/*
 * Some objects in libTiRTC.a call Logf directly.  The linker redirects those
 * calls here so verbose logging remains usable without entering the SDK's
 * non-OpenCPU-safe timestamp formatter.  This changes no code in libTiRTC.a.
 */
void __wrap_Logf(int level, const char *module, const char *format, ...)
{
    char line[384];
    va_list args;
    int written;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (written > 0)
    {
        size_t output_length = (size_t)written;

        line[sizeof(line) - 1U] = '\0';
        if (output_length >= sizeof(line))
        {
            output_length = sizeof(line) - 1U;
        }
        if (!runtime_log_is_sensitive(line, output_length))
        {
            syslogPrintf("[TIRTC-SDK][L%d][%s] %s%s",
                         level,
                         module != NULL ? module : "-",
                         line,
                         (strchr(line, '\n') != NULL ||
                          strchr(line, '\r') != NULL) ? "" : "\r\n");
        }
    }
}

/*
 * libTiRTC.a v2.3.0 was built with mbedTLS 3.6.4, where
 * MBEDTLS_MD_SHA256 is 9.  The F6D_A base firmware exports an older mbedTLS
 * ABI where MBEDTLS_MD_SHA256 is 6.  The archive's sha256_hmac() therefore
 * receives NULL from mbedtls_md_info_from_type(9), and /v1/start is signed
 * with invalid bytes.
 *
 * The linker redirects only the archive's semantic sha256_hmac() calls here.
 * Using the platform header selects its correct enum without changing the
 * supplied archive or globally remapping ambiguous mbedTLS enum values.
 */
int __wrap_sha256_hmac(const unsigned char *key, size_t key_length,
                       const unsigned char *input, size_t input_length,
                       unsigned char *output)
{
    const mbedtls_md_info_t *md_info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int ret;

    if (output == NULL)
    {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    /* signReq ignores this helper's return value, so never leave stale stack
     * bytes available for encoding when the platform crypto call fails. */
    memset(output, 0, 32U);
    if (md_info == NULL)
    {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    ret = mbedtls_md_hmac(md_info, key, key_length,
                          input, input_length, output);
    if (ret != 0)
    {
        memset(output, 0, 32U);
    }
    return ret;
}

static bool runtime_is_default_endpoint(const char *endpoint)
{
    return endpoint == NULL || endpoint[0] == '\0' ||
           strcmp(endpoint, DEMO_TIRTC_DEFAULT_ENDPOINT) == 0 ||
           strcmp(endpoint, DEMO_TIRTC_DEFAULT_ENDPOINT_TLS) == 0;
}

/*
 * libTiRTC's EC71x port calls strdup()/strndup(). newlib-nano implements
 * those via _malloc_r()/sbrk, while this OpenCPU image owns an RTOS heap via
 * malloc()/free(). These global ABI shims keep all TiRTC allocations on the
 * platform heap without modifying the supplied libTiRTC.a.
 */
char *strdup(const char *source)
{
    size_t length;
    char *copy;

    length = strlen(source) + 1U;
    copy = (char *)malloc(length);
    if (copy != NULL)
    {
        memcpy(copy, source, length);
    }
    return copy;
}

char *strndup(const char *source, size_t max_length)
{
    size_t length = 0U;
    char *copy;

    while (length < max_length && source[length] != '\0')
    {
        ++length;
    }
    copy = (char *)malloc(length + 1U);
    if (copy != NULL)
    {
        if (length != 0U)
        {
            memcpy(copy, source, length);
        }
        copy[length] = '\0';
    }
    return copy;
}

static bool runtime_deadline_pending(uint32_t deadline)
{
    return (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

static bool runtime_start_error_retryable(int error)
{
    switch (error)
    {
        case TIRTC_E_SERVER_ERROR:
        case TIRTC_E_TIMEOUTED:
        case TIRTC_E_HTTP_TIMEOUT:
        case TIRTC_E_HTTP_RESET:
        case TIRTC_E_HTTP_PEER_CLOSED:
        case TIRTC_E_HTTP_RESOLVE:
        case TIRTC_E_HTTP_CONNECT:
        case TIRTC_E_HTTP_SSL:
        case TIRTC_E_HTTP_GENERIC:
        case TIRTC_E_HTTP_SERVER_DOWN:
        case TIRTC_E_HTTP_SOCKET:
            return true;
        default:
            return false;
    }
}

static void runtime_signal(void)
{
    if (s_runtime_sem != NULL)
    {
        (void)liot_rtos_semaphore_release(s_runtime_sem);
    }
}

static uint32_t runtime_next_generation(uint32_t generation)
{
    ++generation;
    return generation != 0U ? generation : 1U;
}

static bool runtime_owner_valid(demo_tirtc_owner_e owner)
{
    return owner > DEMO_TIRTC_OWNER_NONE && owner < DEMO_TIRTC_OWNER_COUNT;
}

static const demo_tirtc_listener_t *runtime_owner_feature_snapshot(
    demo_tirtc_owner_e owner)
{
    const demo_tirtc_listener_t *listener = NULL;
    demo_tirtc_feature_e feature;

    switch (owner)
    {
        case DEMO_TIRTC_OWNER_AI:
            feature = DEMO_TIRTC_FEATURE_AI;
            break;
        case DEMO_TIRTC_OWNER_WECHAT:
            feature = DEMO_TIRTC_FEATURE_WECHAT;
            break;
        case DEMO_TIRTC_OWNER_DEV_CHAT:
            feature = DEMO_TIRTC_FEATURE_DEV_CHAT;
            break;
        default:
            return NULL;
    }
    liot_rtos_enter_critical();
    listener = (const demo_tirtc_listener_t *)s_feature_listeners[feature];
    liot_rtos_exit_critical();
    return listener;
}

static const demo_tirtc_incoming_listener_t *
runtime_owner_incoming_snapshot(demo_tirtc_owner_e owner)
{
    const demo_tirtc_incoming_listener_t *listener = NULL;

    if (owner == DEMO_TIRTC_OWNER_LIVE)
    {
        liot_rtos_enter_critical();
        listener =
            (const demo_tirtc_incoming_listener_t *)s_incoming_listener;
        liot_rtos_exit_critical();
    }
    return listener;
}

static int runtime_owner_on_accepted(demo_tirtc_owner_e owner,
                                     tirtc_conn_t hconn)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_conn_accepted != NULL)
    {
        return feature->on_conn_accepted(hconn);
    }
    if (incoming != NULL && incoming->on_conn_accepted != NULL)
    {
        return incoming->on_conn_accepted(hconn);
    }
    return TIRTC_E_INVALID_HANDLE;
}

static void runtime_owner_on_conn_error(demo_tirtc_owner_e owner,
                                        tirtc_conn_t hconn, int error)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_conn_error != NULL)
    {
        feature->on_conn_error(hconn, error);
    }
    else if (incoming != NULL && incoming->on_conn_error != NULL)
    {
        incoming->on_conn_error(hconn, error);
    }
}

static void runtime_owner_on_disconnected(demo_tirtc_owner_e owner,
                                          tirtc_conn_t hconn)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_disconnected != NULL)
    {
        feature->on_disconnected(hconn);
    }
    else if (incoming != NULL && incoming->on_disconnected != NULL)
    {
        incoming->on_disconnected(hconn);
    }
}

static void runtime_owner_on_audio(demo_tirtc_owner_e owner,
                                   tirtc_conn_t hconn,
                                   const TIRTCFRAMEINFO *frame, void *data)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_audio != NULL)
    {
        feature->on_audio(hconn, frame, data);
    }
    else if (incoming != NULL && incoming->on_audio != NULL)
    {
        incoming->on_audio(hconn, frame, data);
    }
}

static void runtime_owner_on_command(demo_tirtc_owner_e owner,
                                     tirtc_conn_t hconn, uint32_t cmdw,
                                     const void *data, uint32_t length)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_command != NULL)
    {
        feature->on_command(hconn, cmdw, data, length);
    }
    else if (incoming != NULL && incoming->on_command != NULL)
    {
        incoming->on_command(hconn, cmdw, data, length);
    }
}

static int runtime_owner_on_subscribe_audio(demo_tirtc_owner_e owner,
                                            tirtc_conn_t hconn,
                                            uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_subscribe_audio != NULL)
    {
        return feature->on_subscribe_audio(hconn, stream_id);
    }
    if (incoming != NULL && incoming->on_subscribe_audio != NULL)
    {
        return incoming->on_subscribe_audio(hconn, stream_id);
    }
    return TIRTC_E_INVALID_HANDLE;
}

static void runtime_owner_on_unsubscribe_audio(demo_tirtc_owner_e owner,
                                               tirtc_conn_t hconn,
                                               uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature =
        runtime_owner_feature_snapshot(owner);
    const demo_tirtc_incoming_listener_t *incoming =
        runtime_owner_incoming_snapshot(owner);

    if (feature != NULL && feature->on_unsubscribe_audio != NULL)
    {
        feature->on_unsubscribe_audio(hconn, stream_id);
    }
    else if (incoming != NULL && incoming->on_unsubscribe_audio != NULL)
    {
        incoming->on_unsubscribe_audio(hconn, stream_id);
    }
}

static void runtime_connect_context_clear_locked(void)
{
    memset(s_connect_context.remote, 0, sizeof(s_connect_context.remote));
    memset(s_connect_context.token, 0, sizeof(s_connect_context.token));
    s_connect_context.pending = false;
    s_connect_context.kind = RUNTIME_CONNECT_NONE;
    s_connect_context.owner = DEMO_TIRTC_OWNER_NONE;
    s_connect_context.session_generation = 0U;
    s_connect_context.request_generation = 0U;
    s_connect_context.callback = NULL;
    s_connect_context.user_data = NULL;
    s_connect_context.token_present = false;
}

static void runtime_pending_clear_locked(void)
{
    s_connection.connect_request_pending = false;
    s_connection.connect_callback_pending = false;
    s_connection.pending_owner = DEMO_TIRTC_OWNER_NONE;
    s_connection.pending_generation = 0U;
    s_connection.pending_request_generation = 0U;
    s_connection.pending_handle = NULL;
    s_connection.pending_handle_released = false;
    s_connection.pending_error = 0;
}

static void runtime_closing_clear_locked(void)
{
    s_connection.closing = NULL;
    s_connection.closing_owner = DEMO_TIRTC_OWNER_NONE;
    s_connection.closing_generation = 0U;
    s_connection.disconnect_submitted = false;
    s_connection.disconnect_attempts = 0U;
    s_connection.disconnect_retry_at = 0U;
}

static void runtime_active_clear_locked(void)
{
    s_connection.active = NULL;
    s_connection.active_owner = DEMO_TIRTC_OWNER_NONE;
    s_connection.active_generation = 0U;
}

static bool runtime_reserve_closing_locked(tirtc_conn_t hconn,
                                           demo_tirtc_owner_e owner,
                                           uint32_t generation)
{
    if (hconn == NULL)
    {
        return false;
    }
    if (s_connection.closing == hconn)
    {
        return true;
    }
    if (s_connection.closing != NULL)
    {
        return false;
    }
    s_connection.closing = hconn;
    s_connection.closing_owner = owner;
    s_connection.closing_generation = generation;
    s_connection.disconnect_submitted = false;
    s_connection.disconnect_attempts = 0U;
    s_connection.disconnect_retry_at = 0U;
    return true;
}

static tirtc_conn_t runtime_active_acquire(demo_tirtc_owner_e owner,
                                           uint32_t generation)
{
    tirtc_conn_t hconn = NULL;

    liot_rtos_enter_critical();
    if (s_connection.owner == owner &&
        s_connection.session_generation == generation &&
        s_connection.active != NULL &&
        s_connection.active_owner == owner &&
        s_connection.active_generation == generation)
    {
        hconn = s_connection.active;
        ++s_connection.users;
    }
    liot_rtos_exit_critical();
    return hconn;
}

static bool runtime_active_callback_acquire(tirtc_conn_t hconn,
                                            demo_tirtc_owner_e *owner)
{
    bool acquired = false;

    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == s_connection.active &&
        runtime_owner_valid(s_connection.active_owner) &&
        s_connection.active_owner == s_connection.owner &&
        s_connection.active_generation == s_connection.session_generation)
    {
        ++s_connection.users;
        *owner = s_connection.active_owner;
        acquired = true;
    }
    liot_rtos_exit_critical();
    return acquired;
}

static void runtime_active_release(tirtc_conn_t hconn)
{
    bool wake = false;

    if (hconn == NULL)
    {
        return;
    }
    liot_rtos_enter_critical();
    if (s_connection.users != 0U)
    {
        --s_connection.users;
        wake = s_connection.users == 0U && s_connection.closing != NULL;
    }
    liot_rtos_exit_critical();
    if (wake)
    {
        runtime_signal();
    }
}

static void runtime_managed_connect_result(int error, tirtc_conn_t hconn,
                                           void *user_data)
{
    runtime_connect_context_t *context =
        (runtime_connect_context_t *)user_data;
    TIRTCCONNECTCALLBACK callback = NULL;
    void *callback_data = NULL;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t generation = 0U;
    uint32_t request_generation = 0U;
    bool matching = false;
    bool installed = false;
    bool cleanup_reserved = false;
    bool callback_completed = false;
    bool require_restart = false;
    int callback_error = error;

    liot_rtos_enter_critical();
    if (context == &s_connect_context && context->pending)
    {
        request_generation = context->request_generation;
        matching = s_connection.connect_callback_pending &&
                   request_generation != 0U &&
                   request_generation ==
                       s_connection.pending_request_generation;
    }
    if (matching)
    {
        callback = context->callback;
        callback_data = context->user_data;
        owner = context->owner;
        generation = context->session_generation;
        if (s_connection.pending_error != 0)
        {
            callback_error = s_connection.pending_error;
        }
        if (callback_error == 0 && hconn == NULL)
        {
            callback_error = TIRTC_E_INVALID_HANDLE;
            /* A successful result without a handle violates the SDK callback
             * contract.  Recycle only the SDK lifetime, never the module. */
            require_restart = true;
        }
        if (callback_error == 0 && hconn != NULL &&
            s_connection.connect_request_pending &&
            s_connection.owner == owner &&
            s_connection.session_generation == generation &&
            s_connection.active == NULL && s_connection.closing == NULL &&
            s_connection.users == 0U && !s_restart_required)
        {
            s_connection.active = hconn;
            s_connection.active_owner = owner;
            s_connection.active_generation = generation;
            s_connection.connection_generation = runtime_next_generation(
                s_connection.connection_generation);
            installed = true;
        }
        else if (hconn != NULL)
        {
            /* on_conn_error may precede both on_disconnected and this result.
             * If that exact pending handle is already released, reserving the
             * reused pointer again would issue a second Disconnect (ABA). */
            if (s_connection.pending_handle_released &&
                s_connection.pending_handle == hconn)
            {
                cleanup_reserved = false;
            }
            else
            {
                cleanup_reserved = runtime_reserve_closing_locked(
                    hconn, owner, generation);
                if (!cleanup_reserved)
                {
                    require_restart = true;
                }
            }
            if (callback_error == 0)
            {
                callback_error = TIRTC_E_BUSY;
            }
        }
        /* This is only the request-completion boundary.  Keep
         * connect_callback_pending and the borrowed context asserted until
         * the feature callback below has returned.  Stop/Uninit is forbidden
         * while application code is still running on the SDK callback stack. */
        s_connection.connect_request_pending = false;
    }
    else if (hconn != NULL)
    {
        cleanup_reserved = runtime_reserve_closing_locked(
            hconn, DEMO_TIRTC_OWNER_NONE, 0U);
        require_restart = !cleanup_reserved;
    }
    liot_rtos_exit_critical();

    if (cleanup_reserved)
    {
        runtime_signal();
    }
    if (callback != NULL)
    {
        callback(callback_error, installed ? hconn : NULL, callback_data);
    }

    liot_rtos_enter_critical();
    if (matching && s_connection.connect_callback_pending &&
        s_connection.pending_request_generation == request_generation &&
        s_connect_context.pending &&
        s_connect_context.request_generation == request_generation)
    {
        runtime_pending_clear_locked();
        runtime_connect_context_clear_locked();
        callback_completed = true;
    }
    liot_rtos_exit_critical();
    if (callback_completed)
    {
        runtime_signal();
    }
    if (require_restart)
    {
        demo_tirtc_require_restart(callback_error != 0 ? callback_error :
                                                        TIRTC_E_INTERNAL_ERROR);
    }
}

static void runtime_process_managed_disconnect(void)
{
    tirtc_conn_t hconn = NULL;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t now = liot_rtos_get_running_time();
    bool completed_without_callback = false;
    bool require_restart = false;
    int ret;

    liot_rtos_enter_critical();
    if (s_connection.closing != NULL &&
        !s_connection.disconnect_submitted && s_connection.users == 0U &&
        (s_connection.disconnect_retry_at == 0U ||
         !runtime_deadline_pending(s_connection.disconnect_retry_at)))
    {
        hconn = s_connection.closing;
        owner = s_connection.closing_owner;
        s_connection.disconnect_submitted = true;
        ++s_connection.disconnect_attempts;
    }
    liot_rtos_exit_critical();
    if (hconn == NULL)
    {
        return;
    }

    /* Never call Disconnect on the callback stack that requested teardown.
     * This mirrors the official adapter's one-tick callback-drain barrier. */
    liot_rtos_task_sleep_ms(1U);
    liot_rtos_enter_critical();
    if (s_connection.closing != hconn ||
        !s_connection.disconnect_submitted)
    {
        liot_rtos_exit_critical();
        return;
    }
    liot_rtos_exit_critical();

    ret = TiRtcDisconnect(hconn);
    liot_rtos_enter_critical();
    if (s_connection.closing == hconn)
    {
        if (ret == TIRTC_E_INVALID_HANDLE)
        {
            if (s_connection.pending_handle == hconn)
            {
                /* INVALID_HANDLE is the only non-callback release boundary.
                 * Preserve that fact until the pending connect result drains
                 * so it cannot reserve the same opaque address again. */
                s_connection.pending_handle_released = true;
            }
            runtime_closing_clear_locked();
            completed_without_callback = true;
        }
        else if (ret != 0)
        {
            if (s_connection.disconnect_attempts >=
                DEMO_TIRTC_DISCONNECT_RETRIES)
            {
                /* Keep the reservation asserted.  Reusing a handle slot after
                 * an unreleased SDK connection is more dangerous than
                 * stopping admission and attempting controlled SDK recovery. */
                s_connection.disconnect_submitted = false;
                s_connection.disconnect_attempts = 0U;
                s_connection.disconnect_retry_at = now +
                    DEMO_TIRTC_START_RETRY_MS;
                require_restart = true;
            }
            else
            {
                s_connection.disconnect_submitted = false;
                s_connection.disconnect_retry_at = now +
                    DEMO_TIRTC_DISCONNECT_RETRY_MS;
            }
        }
        /* ret == 0 intentionally leaves `closing` reserved until the SDK's
         * on_disconnected callback confirms its internal release boundary. */
    }
    liot_rtos_exit_critical();

    liot_trace("[TIRTC-ADAPTER] disconnect handle=%p ret=%d owner=%u\r\n",
               hconn, ret, (unsigned int)owner);
    if (completed_without_callback)
    {
        runtime_owner_on_disconnected(owner, hconn);
        runtime_signal();
    }
    if (require_restart)
    {
        demo_tirtc_require_restart(ret);
    }
}

static void runtime_reject_slot_clear_locked(uint32_t slot)
{
    if (slot >= DEMO_TIRTC_REJECT_QUEUE_DEPTH ||
        s_reject_slots[slot].handle == NULL)
    {
        return;
    }
    s_reject_slots[slot].handle = NULL;
    s_reject_slots[slot].state = RUNTIME_REJECT_FREE;
    s_reject_slots[slot].attempts = 0U;
    s_reject_slots[slot].retry_at = 0U;
    if (s_reject_pending != 0U)
    {
        --s_reject_pending;
    }
}

static void runtime_reject_later(tirtc_conn_t hconn)
{
    uint32_t slot = DEMO_TIRTC_REJECT_QUEUE_DEPTH;
    uint32_t i;
    bool already_owned = false;

    if (hconn == NULL)
    {
        demo_tirtc_require_restart(TIRTC_E_INVALID_HANDLE);
        return;
    }

    /* Own the handle before publishing queue work.  on_disconnected may race
     * immediately after on_conn_accepted returns, and must still be able to
     * complete this exact reservation. */
    liot_rtos_enter_critical();
    for (i = 0U; i < DEMO_TIRTC_REJECT_QUEUE_DEPTH; ++i)
    {
        if (s_reject_slots[i].handle == hconn)
        {
            already_owned = true;
            break;
        }
        if (slot == DEMO_TIRTC_REJECT_QUEUE_DEPTH &&
            s_reject_slots[i].handle == NULL)
        {
            slot = i;
        }
    }
    if (!already_owned && slot < DEMO_TIRTC_REJECT_QUEUE_DEPTH)
    {
        s_reject_slots[slot].handle = hconn;
        s_reject_slots[slot].state = RUNTIME_REJECT_QUEUED;
        s_reject_slots[slot].attempts = 0U;
        s_reject_slots[slot].retry_at = 0U;
        ++s_reject_pending;
    }
    liot_rtos_exit_critical();

    if (already_owned)
    {
        return;
    }
    if (slot == DEMO_TIRTC_REJECT_QUEUE_DEPTH || s_reject_queue == NULL)
    {
        liot_trace("[TIRTC] reject ownership capacity violated handle=%p\r\n",
                   hconn);
        demo_tirtc_require_restart(TIRTC_E_LACK_OF_RESOURCE);
        return;
    }
    if (liot_rtos_queue_release(s_reject_queue, sizeof(hconn),
                                 (uint8 *)&hconn,
                                 LIOT_NO_WAIT) != LIOT_OSI_SUCCESS)
    {
        /* Preserve the reservation and make it directly runnable so graceful
         * recovery can release it.  The queue contract violation still
         * poisons admission; silently dropping an SDK handle is never safe. */
        liot_rtos_enter_critical();
        if (s_reject_slots[slot].handle == hconn &&
            s_reject_slots[slot].state == RUNTIME_REJECT_QUEUED)
        {
            s_reject_slots[slot].state = RUNTIME_REJECT_READY;
        }
        liot_rtos_exit_critical();
        liot_trace("[TIRTC] reject queue failed handle=%p; "
                   "runtime recovery required\r\n", hconn);
        demo_tirtc_require_restart(TIRTC_E_LACK_OF_RESOURCE);
    }
    runtime_signal();
}

static bool runtime_reject_complete(tirtc_conn_t hconn)
{
    uint32_t i;
    bool matched = false;

    liot_rtos_enter_critical();
    for (i = 0U; i < DEMO_TIRTC_REJECT_QUEUE_DEPTH; ++i)
    {
        if (s_reject_slots[i].handle == hconn)
        {
            runtime_reject_slot_clear_locked(i);
            matched = true;
            break;
        }
    }
    liot_rtos_exit_critical();
    if (matched)
    {
        runtime_signal();
    }
    return matched;
}

static void runtime_process_rejects(void)
{
    tirtc_conn_t hconn;
    uint32_t i;
    uint32_t slot;
    uint32_t now;
    bool run;
    bool completed;
    bool require_restart;
    int ret;

    while (s_reject_queue != NULL &&
           liot_rtos_queue_wait(s_reject_queue, (uint8 *)&hconn,
                                sizeof(hconn),
                                LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        if (hconn != NULL)
        {
            liot_rtos_enter_critical();
            for (i = 0U; i < DEMO_TIRTC_REJECT_QUEUE_DEPTH; ++i)
            {
                if (s_reject_slots[i].handle == hconn &&
                    s_reject_slots[i].state == RUNTIME_REJECT_QUEUED)
                {
                    s_reject_slots[i].state = RUNTIME_REJECT_READY;
                    break;
                }
            }
            liot_rtos_exit_critical();
        }
    }

    /* At most two connections can be awaiting rejection.  Process the fixed
     * reservation table without allocating work items or blocking callbacks. */
    for (slot = 0U; slot < DEMO_TIRTC_REJECT_QUEUE_DEPTH; ++slot)
    {
        run = false;
        completed = false;
        require_restart = false;
        hconn = NULL;
        now = liot_rtos_get_running_time();
        liot_rtos_enter_critical();
        if (s_reject_slots[slot].handle != NULL &&
            (s_reject_slots[slot].state == RUNTIME_REJECT_READY ||
             (s_reject_slots[slot].state == RUNTIME_REJECT_RETRY_WAIT &&
              !runtime_deadline_pending(s_reject_slots[slot].retry_at))))
        {
            hconn = s_reject_slots[slot].handle;
            s_reject_slots[slot].state = RUNTIME_REJECT_SUBMITTED;
            ++s_reject_slots[slot].attempts;
            run = true;
        }
        liot_rtos_exit_critical();
        if (!run)
        {
            continue;
        }

        /* One scheduling tick separates callback publication and teardown. */
        liot_rtos_task_sleep_ms(1U);
        liot_rtos_enter_critical();
        run = s_reject_slots[slot].handle == hconn &&
              s_reject_slots[slot].state == RUNTIME_REJECT_SUBMITTED;
        liot_rtos_exit_critical();
        if (!run)
        {
            continue; /* an early on_disconnected completed the reservation */
        }

        ret = TiRtcDisconnect(hconn);
        liot_rtos_enter_critical();
        if (s_reject_slots[slot].handle == hconn &&
            s_reject_slots[slot].state == RUNTIME_REJECT_SUBMITTED)
        {
            if (ret == TIRTC_E_INVALID_HANDLE)
            {
                runtime_reject_slot_clear_locked(slot);
                completed = true;
            }
            else if (ret != 0)
            {
                if (s_reject_slots[slot].attempts >=
                    DEMO_TIRTC_DISCONNECT_RETRIES)
                {
                    /* Keep owning the handle and retry after backoff.  A soft
                     * runtime recovery still has to drain this reservation. */
                    s_reject_slots[slot].state = RUNTIME_REJECT_RETRY_WAIT;
                    s_reject_slots[slot].attempts = 0U;
                    s_reject_slots[slot].retry_at = now +
                        DEMO_TIRTC_START_RETRY_MS;
                    require_restart = true;
                }
                else
                {
                    s_reject_slots[slot].state = RUNTIME_REJECT_RETRY_WAIT;
                    s_reject_slots[slot].retry_at = now +
                        DEMO_TIRTC_DISCONNECT_RETRY_MS;
                }
            }
            /* ret == 0 remains SUBMITTED until on_disconnected. */
        }
        liot_rtos_exit_critical();
        liot_trace("[TIRTC] rejected connection disconnect handle=%p "
                   "ret=%d\r\n", hconn, ret);
        if (completed)
        {
            runtime_signal();
        }
        if (require_restart)
        {
            demo_tirtc_require_restart(ret);
        }
    }
}

static void runtime_on_event(int event, const void *data, int len)
{
    (void)data;
    (void)len;

    if (event == TIRTC_EVENT_SYS_STARTED)
    {
        liot_rtos_enter_critical();
        s_started_event = true;
        s_stopped_event = false;
        if (!s_restart_required)
        {
            s_ready = true;
        }
        liot_rtos_exit_critical();
        liot_trace("[TIRTC] SYS_STARTED\r\n");
    }
    else if (event == TIRTC_EVENT_SYS_STOPPED)
    {
        bool requested;

        liot_rtos_enter_critical();
        requested = s_stop_requested;
        s_stopped_event = true;
        s_start_submitted = false;
        s_ready = false;
        if (!requested)
        {
            /* A spontaneous SDK stop is recoverable through the same
             * Stop/Uninit/Start lifecycle as an explicitly poisoned
             * connection.  Do not leave the adapter permanently half-started. */
            if (!s_restart_required)
            {
                s_restart_required = true;
                s_restart_error = TIRTC_E_NOT_INITIALIZED;
            }
            s_error = s_restart_error;
        }
        liot_rtos_exit_critical();
        liot_trace("[TIRTC] SYS_STOPPED\r\n");
    }
    else
    {
        liot_trace("[TIRTC] event=%d\r\n", event);
    }
    runtime_signal();
}

/* Snapshot both long-lived routers without holding the critical section while
 * invoking application code from the SDK callback thread.  Treat an
 * accidentally reused structure address as the incoming router only, so one
 * callback can never be invoked twice through two registrations. */
static void runtime_snapshot_listeners(
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT],
    const demo_tirtc_incoming_listener_t **incoming)
{
    const demo_tirtc_incoming_listener_t *incoming_snapshot;
    uint32_t i;

    liot_rtos_enter_critical();
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        feature[i] = (const demo_tirtc_listener_t *)s_feature_listeners[i];
    }
    incoming_snapshot =
        (const demo_tirtc_incoming_listener_t *)s_incoming_listener;
    liot_rtos_exit_critical();

    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if ((const void *)feature[i] == (const void *)incoming_snapshot)
        {
            feature[i] = NULL;
        }
    }
    *incoming = incoming_snapshot;
}

static void runtime_on_conn_accepted(tirtc_conn_t hconn)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e managed_owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t managed_generation = 0U;
    uint32_t owner = 0U;
    uint32_t i;
    int accepted = -1;
    bool accepting;
    bool managed_session = false;
    bool managed_installed = false;
    bool cleanup_reserved = false;
    bool wake = false;

    liot_rtos_enter_critical();
    ++s_accept_callbacks_pending;
    accepting = s_ready && !s_restart_required && s_reject_pending == 0U;
    managed_session = runtime_owner_valid(s_connection.owner);
    if (managed_session)
    {
        managed_owner = s_connection.owner;
        managed_generation = s_connection.session_generation;
        if (accepting && hconn != NULL &&
            s_connection.expected_owner == managed_owner &&
            s_connection.expected_generation == managed_generation &&
            s_connection.active == NULL &&
            !s_connection.connect_request_pending &&
            !s_connection.connect_callback_pending &&
            s_connection.closing == NULL && s_connection.users == 0U)
        {
            s_connection.active = hconn;
            s_connection.active_owner = managed_owner;
            s_connection.active_generation = managed_generation;
            s_connection.expected_owner = DEMO_TIRTC_OWNER_NONE;
            s_connection.expected_generation = 0U;
            s_connection.connection_generation = runtime_next_generation(
                s_connection.connection_generation);
            /* Protect the installed handle while the feature's short accept
             * callback records it. */
            ++s_connection.users;
            managed_installed = true;
        }
        else if (hconn != NULL)
        {
            cleanup_reserved = runtime_reserve_closing_locked(
                hconn, managed_owner, managed_generation);
        }
    }
    liot_rtos_exit_critical();

    if (managed_session)
    {
        if (managed_installed)
        {
            accepted = runtime_owner_on_accepted(managed_owner, hconn);
            if (accepted != 0)
            {
                liot_rtos_enter_critical();
                if (s_connection.active == hconn)
                {
                    runtime_active_clear_locked();
                    cleanup_reserved = runtime_reserve_closing_locked(
                        hconn, managed_owner, managed_generation);
                }
                else if (s_connection.closing == hconn)
                {
                    cleanup_reserved = true;
                }
                liot_rtos_exit_critical();
            }
            runtime_active_release(hconn);
        }
        liot_trace("[TIRTC-ADAPTER] incoming handle=%p owner=%u result=%s\r\n",
                   hconn, (unsigned int)managed_owner,
                   managed_installed && accepted == 0 ? "accepted" :
                                                        "rejected");
        if (cleanup_reserved)
        {
            runtime_signal();
        }
        else if (!managed_installed || accepted != 0)
        {
            /* This can only occur after an SDK invariant violation such as a
             * second incoming handle while the sole closing slot is owned. */
            runtime_reject_later(hconn);
            demo_tirtc_require_restart(TIRTC_E_LACK_OF_RESOURCE);
        }
        goto callback_done;
    }
    if (!accepting)
    {
        liot_trace("[TIRTC] incoming connection rejected: runtime closing\r\n");
        runtime_reject_later(hconn);
        goto callback_done;
    }

    runtime_snapshot_listeners(feature, &incoming);
    /* Feature routes get first refusal.  In the official device-call flow
     * the caller deliberately waits for one inbound P2P connection from the
     * answering device.  If no feature claims it, the passive platform LIVE
     * route keeps its existing HOME-only admission policy. */
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_conn_accepted != NULL &&
            feature[i]->on_conn_accepted(hconn) == 0)
        {
            accepted = 0;
            owner = i + 1U;
            break;
        }
    }
    if (accepted != 0 && incoming != NULL &&
        incoming->on_conn_accepted != NULL)
    {
        accepted = incoming->on_conn_accepted(hconn);
        if (accepted == 0)
        {
            owner = DEMO_TIRTC_FEATURE_COUNT + 1U;
        }
    }
    liot_trace("[TIRTC] incoming connection handle=%p owner=%u result=%s\r\n",
               hconn, (unsigned int)owner,
               accepted == 0 ? "accepted" : "rejected");
    if (accepted != 0)
    {
        runtime_reject_later(hconn);
    }

callback_done:
    liot_rtos_enter_critical();
    if (s_accept_callbacks_pending != 0U)
    {
        --s_accept_callbacks_pending;
        wake = s_accept_callbacks_pending == 0U;
    }
    liot_rtos_exit_critical();
    if (wake)
    {
        runtime_signal();
    }
}

static void runtime_on_conn_error(tirtc_conn_t hconn, int error)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t generation = 0U;
    uint32_t i;
    bool managed = false;
    bool cleanup_reserved = false;
    bool notify_owner = true;
    bool require_restart = false;

    /* Outbound connection attempts report their terminal result through the
     * managed connect callback.  A NULL handle is not a connection object and
     * must not be broadcast to unrelated feature listeners. */
    if (hconn == NULL)
    {
        liot_trace("[TIRTC-ADAPTER] ignored connection error without handle "
                   "error=%d\r\n", error);
        return;
    }

    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == s_connection.active)
    {
        owner = s_connection.active_owner;
        generation = s_connection.active_generation;
        cleanup_reserved = runtime_reserve_closing_locked(
            hconn, owner, generation);
        if (cleanup_reserved)
        {
            runtime_active_clear_locked();
        }
        else
        {
            require_restart = true;
        }
        managed = true;
    }
    else if (hconn != NULL && hconn == s_connection.closing)
    {
        owner = s_connection.closing_owner;
        generation = s_connection.closing_generation;
        cleanup_reserved = true;
        managed = true;
    }
    else if (hconn != NULL && s_connection.connect_callback_pending &&
             runtime_owner_valid(s_connection.pending_owner) &&
             s_connection.pending_generation ==
                 s_connection.session_generation &&
             (s_connection.pending_handle == NULL ||
              s_connection.pending_handle == hconn))
    {
        /* Some SDK paths report on_conn_error before the asynchronous connect
         * callback publishes its handle.  Record and own that handle now so
         * the later callback cannot turn it into an orphan. */
        owner = s_connection.pending_owner;
        generation = s_connection.pending_generation;
        if (s_connection.pending_handle == hconn &&
            s_connection.pending_handle_released)
        {
            /* A late duplicate error after on_disconnected belongs to the
             * same still-pending connect callback.  Do not re-own/release the
             * opaque address after the SDK release boundary. */
            notify_owner = false;
        }
        else
        {
            notify_owner = s_connection.pending_error == 0;
            s_connection.pending_handle = hconn;
            s_connection.pending_handle_released = false;
            s_connection.pending_error = error;
            s_connection.connect_request_pending = false;
            cleanup_reserved = runtime_reserve_closing_locked(
                hconn, owner, generation);
            require_restart = !cleanup_reserved;
        }
        managed = true;
    }
    liot_rtos_exit_critical();

    if (managed)
    {
        liot_trace("[TIRTC-ADAPTER] connection error handle=%p owner=%u "
                   "generation=%u error=%d\r\n",
                   hconn, (unsigned int)owner,
                   (unsigned int)generation, error);
        if (notify_owner)
        {
            runtime_owner_on_conn_error(owner, hconn, error);
        }
        if (cleanup_reserved)
        {
            runtime_signal();
        }
        if (require_restart)
        {
            demo_tirtc_require_restart(error != 0 ? error :
                                                    TIRTC_E_INTERNAL_ERROR);
        }
        return;
    }

    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_conn_error != NULL)
    {
        incoming->on_conn_error(hconn, error);
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_conn_error != NULL)
        {
            feature[i]->on_conn_error(hconn, error);
        }
    }
}

static void runtime_on_disconnected(tirtc_conn_t hconn)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t i;
    bool managed = false;
    bool closing_match = false;
    bool callback_user = false;

    if (runtime_reject_complete(hconn))
    {
        liot_trace("[TIRTC-ADAPTER] rejected handle released=%p\r\n", hconn);
        return;
    }
    liot_rtos_enter_critical();
    if (hconn != NULL && hconn == s_connection.closing)
    {
        owner = s_connection.closing_owner;
        closing_match = true;
        if (s_connection.pending_handle == hconn)
        {
            s_connection.pending_handle_released = true;
        }
        managed = true;
    }
    else if (hconn != NULL && hconn == s_connection.active)
    {
        owner = s_connection.active_owner;
        runtime_active_clear_locked();
        /* Keep logical owner release and SDK Stop behind this callback. */
        ++s_connection.users;
        callback_user = true;
        managed = true;
    }
    else if (hconn != NULL && hconn == s_connection.pending_handle)
    {
        owner = s_connection.pending_owner;
        s_connection.pending_handle_released = true;
        managed = true;
    }
    liot_rtos_exit_critical();
    if (managed)
    {
        liot_trace("[TIRTC-ADAPTER] disconnected handle=%p owner=%u\r\n",
                   hconn, (unsigned int)owner);
        runtime_owner_on_disconnected(owner, hconn);
        liot_rtos_enter_critical();
        if (closing_match && s_connection.closing == hconn)
        {
            runtime_closing_clear_locked();
        }
        if (callback_user && s_connection.users != 0U)
        {
            --s_connection.users;
        }
        liot_rtos_exit_critical();
        runtime_signal();
        return;
    }
    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_disconnected != NULL)
    {
        incoming->on_disconnected(hconn);
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_disconnected != NULL)
        {
            feature[i]->on_disconnected(hconn);
        }
    }
}

static void runtime_on_audio(tirtc_conn_t hconn,
                             const TIRTCFRAMEINFO *frame,
                             void *data)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t i;

    if (runtime_active_callback_acquire(hconn, &owner))
    {
        runtime_owner_on_audio(owner, hconn, frame, data);
        runtime_active_release(hconn);
        return;
    }
    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_audio != NULL)
    {
        incoming->on_audio(hconn, frame, data);
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_audio != NULL)
        {
            feature[i]->on_audio(hconn, frame, data);
        }
    }
}

static void runtime_on_video(tirtc_conn_t hconn,
                             const TIRTCFRAMEINFO *frame,
                             void *data)
{
    (void)hconn;
    (void)frame;
    (void)data;
}

static void runtime_on_message(tirtc_conn_t hconn,
                               const TIRTCFRAMEINFO *frame,
                               void *data)
{
    (void)hconn;
    (void)frame;
    (void)data;
}

static void runtime_on_command(tirtc_conn_t hconn, uint32_t cmdw,
                               const void *data, uint32_t length)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t i;

    if (runtime_active_callback_acquire(hconn, &owner))
    {
        runtime_owner_on_command(owner, hconn, cmdw, data, length);
        runtime_active_release(hconn);
        return;
    }
    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_command != NULL)
    {
        incoming->on_command(hconn, cmdw, data, length);
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_command != NULL)
        {
            feature[i]->on_command(hconn, cmdw, data, length);
        }
    }
}

static void runtime_on_request_key_frame(tirtc_conn_t hconn,
                                         uint8_t stream_id)
{
    (void)hconn;
    (void)stream_id;
}

static int runtime_on_subscribe_video(tirtc_conn_t hconn,
                                      uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    int accepted = -1;

    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_subscribe_video != NULL)
    {
        accepted = incoming->on_subscribe_video(hconn, stream_id);
    }
    liot_trace("[TIRTC] subscribe video handle=%p stream=%u result=%s\r\n",
               hconn, (unsigned int)stream_id,
               accepted == 0 ? "accepted" : "rejected");
    return accepted;
}

static void runtime_on_unsubscribe_video(tirtc_conn_t hconn,
                                         uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;

    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_unsubscribe_video != NULL)
    {
        incoming->on_unsubscribe_video(hconn, stream_id);
    }
    liot_trace("[TIRTC] unsubscribe video handle=%p stream=%u\r\n",
               hconn, (unsigned int)stream_id);
}

static int runtime_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    int accepted = -1;
    uint32_t i;

    if (runtime_active_callback_acquire(hconn, &owner))
    {
        accepted = runtime_owner_on_subscribe_audio(owner, hconn, stream_id);
        runtime_active_release(hconn);
        liot_trace("[TIRTC-ADAPTER] subscribe audio handle=%p owner=%u "
                   "stream=%u result=%s\r\n",
                   hconn, (unsigned int)owner, (unsigned int)stream_id,
                   accepted == 0 ? "accepted" : "rejected");
        return accepted;
    }
    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_subscribe_audio != NULL &&
        incoming->on_subscribe_audio(hconn, stream_id) == 0)
    {
        accepted = 0;
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_subscribe_audio != NULL &&
            feature[i]->on_subscribe_audio(hconn, stream_id) == 0)
        {
            accepted = 0;
        }
    }
    liot_trace("[TIRTC] subscribe audio handle=%p stream=%u result=%s\r\n",
               hconn, (unsigned int)stream_id,
               accepted == 0 ? "accepted" : "rejected");
    return accepted;
}

static void runtime_on_unsubscribe_audio(tirtc_conn_t hconn,
                                         uint8_t stream_id)
{
    const demo_tirtc_listener_t *feature[DEMO_TIRTC_FEATURE_COUNT];
    const demo_tirtc_incoming_listener_t *incoming;
    demo_tirtc_owner_e owner = DEMO_TIRTC_OWNER_NONE;
    uint32_t i;

    if (runtime_active_callback_acquire(hconn, &owner))
    {
        runtime_owner_on_unsubscribe_audio(owner, hconn, stream_id);
        runtime_active_release(hconn);
        return;
    }
    runtime_snapshot_listeners(feature, &incoming);
    if (incoming != NULL && incoming->on_unsubscribe_audio != NULL)
    {
        incoming->on_unsubscribe_audio(hconn, stream_id);
    }
    for (i = 0U; i < DEMO_TIRTC_FEATURE_COUNT; ++i)
    {
        if (feature[i] != NULL && feature[i]->on_unsubscribe_audio != NULL)
        {
            feature[i]->on_unsubscribe_audio(hconn, stream_id);
        }
    }
    liot_trace("[TIRTC] unsubscribe audio handle=%p stream=%u\r\n",
               hconn, (unsigned int)stream_id);
}

static void runtime_on_update_bitrate(tirtc_conn_t hconn,
                                      uint8_t stream_id,
                                      uint32_t target_bitrate_bps)
{
    (void)hconn;
    (void)stream_id;
    (void)target_bitrate_bps;
}

/* The SDK retains this pointer for its entire lifetime. */
static const TIRTCCALLBACKS s_callbacks = {
    .on_event = runtime_on_event,
    .on_conn_accepted = runtime_on_conn_accepted,
    .on_conn_error = runtime_on_conn_error,
    .on_disconnected = runtime_on_disconnected,
    .on_audio = runtime_on_audio,
    .on_video = runtime_on_video,
    .on_message = runtime_on_message,
    .on_command = runtime_on_command,
    .on_request_key_frame = runtime_on_request_key_frame,
    .on_subscribe_video = runtime_on_subscribe_video,
    .on_unsubscribe_video = runtime_on_unsubscribe_video,
    .on_subscribe_audio = runtime_on_subscribe_audio,
    .on_unsubscribe_audio = runtime_on_unsubscribe_audio,
    .on_update_bitrate = runtime_on_update_bitrate,
};

static int runtime_set_option(TIRTCOPTION option, const void *data,
                              uint32_t length, const char *name)
{
    int ret = TiRtcSetOption(option, data, length);

    if (ret != 0)
    {
        liot_trace("[TIRTC] option %s failed ret=%d (%s)\r\n",
                   name, ret, TiRtcGetErrorStr(ret));
    }
    return ret;
}

static int runtime_start(const demo_binding_tirtc_identity_t *identity)
{
    uint32_t send_buffer = DEMO_TIRTC_SEND_BUFFER_BYTES;
    uint32_t deadline;
    int max_connections = 1;
    int network_type = TIRTC_NETCONN_4G;
    int restricted_network = DEMO_TIRTC_RESTRICTED_NETWORK;
    int ret;

    if (identity == NULL || identity->device_id[0] == '\0' ||
        identity->device_key[0] == '\0' ||
        identity->client_id[0] == '\0' || identity->iccid[0] == '\0')
    {
        return DEMO_TIRTC_ERR_NOT_READY;
    }

    /* Install both OpenCPU-safe sinks before enabling the configured TiRTC
     * library level.  Production builds normally keep this below verbose
     * transport tracing because serial output competes with media work. */
    TiRtcLogSetCallback(tirtc_sdk_log_callback);
    tgtrp_set_log_callback(tirtc_transport_log_callback);
    TiRtcLogSetLevel(DEMO_TIRTC_LOG_LEVEL);
    tgtrp_set_log_level(DEMO_TIRTC_TRANSPORT_LOG_FLAGS);

    /* MAX_SEND_BUFFER is the only option that must precede TiRtcInit(). */
    liot_trace("[TIRTC] stage=max-send-buffer free_heap=%u\r\n",
               (unsigned int)liot_xPortGetFreeHeapSize());
    ret = runtime_set_option(TIRTC_OPT_MAX_SEND_BUFFER, &send_buffer,
                             sizeof(send_buffer), "max-send-buffer");
    if (ret != 0)
    {
        return DEMO_TIRTC_ERR_OPTION;
    }
    liot_trace("[TIRTC] stage=init begin (library log level=%d)\r\n",
               DEMO_TIRTC_LOG_LEVEL);
    ret = TiRtcInit();
    if (ret != 0)
    {
        liot_trace("[TIRTC] TiRtcInit failed ret=%d (%s)\r\n",
                   ret, TiRtcGetErrorStr(ret));
        return DEMO_TIRTC_ERR_INIT;
    }
    liot_rtos_enter_critical();
    s_sdk_initialized = true;
    s_start_submitted = false;
    s_stop_requested = false;
    s_started_event = false;
    s_stopped_event = false;
    liot_rtos_exit_critical();

    liot_trace("[TIRTC] stage=init OK free_heap=%u\r\n",
               (unsigned int)liot_xPortGetFreeHeapSize());
    /* Common minimal adapter options, followed by the CAT1.bis 4G route. */
    liot_trace("[TIRTC] stage=options begin network=4G restricted=%d "
               "log=%d transport=0x%x\r\n",
               restricted_network, DEMO_TIRTC_LOG_LEVEL,
               DEMO_TIRTC_TRANSPORT_LOG_FLAGS);
    ret = runtime_set_option(TIRTC_OPT_MAX_CONNECTIONS, &max_connections,
                             sizeof(max_connections), "max-connections");
    if (ret == 0)
    {
        ret = runtime_set_option(TIRTC_OPT_NETWORK_TYPE, &network_type,
                                 sizeof(network_type), "network-type");
    }
    if (ret == 0)
    {
        ret = runtime_set_option(TIRTC_OPT_ICCID, identity->iccid,
                                 (uint32_t)strlen(identity->iccid), "iccid");
    }
    if (ret == 0)
    {
        ret = runtime_set_option(TIRTC_OPT_RESTRICTED_NETWORK,
                                 &restricted_network,
                                 sizeof(restricted_network),
                                 "restricted-network");
    }
    /* The official minimal adapter leaves the SDK's default endpoint unset. */
    if (ret == 0 &&
        !runtime_is_default_endpoint(identity->tirtc_endpoint))
    {
        ret = runtime_set_option(TIRTC_OPT_SERVICE_ENDPOINT,
                                 identity->tirtc_endpoint,
                                 (uint32_t)strlen(identity->tirtc_endpoint),
                                 "endpoint");
    }
    if (ret == 0)
    {
        ret = runtime_set_option(TIRTC_OPT_DEVICE_SECRET_KEY,
                                 identity->device_key,
                                 (uint32_t)strlen(identity->device_key),
                                 "device-secret");
    }
    if (ret == 0)
    {
        ret = runtime_set_option(TIRTC_OPT_CLIENT_ID, identity->client_id,
                                 (uint32_t)strlen(identity->client_id),
                                 "client-id");
    }
    if (ret != 0)
    {
        TiRtcUninit();
        liot_rtos_enter_critical();
        s_sdk_initialized = false;
        liot_rtos_exit_critical();
        return DEMO_TIRTC_ERR_OPTION;
    }

    liot_trace("[TIRTC] identity ready device_id_len=%u endpoint_mode=%s\r\n",
               (unsigned int)strlen(identity->device_id),
               runtime_is_default_endpoint(identity->tirtc_endpoint)
                   ? "SDK-default"
                   : "configured");
    liot_trace("[TIRTC] stage=options OK; start device mode "
               "id_len=%u client_len=%u "
               "iccid_len=%u buffer=%u\r\n",
               (unsigned int)strlen(identity->device_id),
               (unsigned int)strlen(identity->client_id),
               (unsigned int)strlen(identity->iccid),
               (unsigned int)send_buffer);
    liot_rtos_enter_critical();
    s_started_event = false;
    s_stopped_event = false;
    s_stop_requested = false;
    s_start_submitted = true;
    liot_rtos_exit_critical();
    ret = TiRtcStart(identity->device_id, &s_callbacks);
    if (ret != 0)
    {
        liot_trace("[TIRTC] TiRtcStart failed ret=%d (%s)\r\n",
                   ret, TiRtcGetErrorStr(ret));
        TiRtcUninit();
        liot_rtos_enter_critical();
        s_sdk_initialized = false;
        s_start_submitted = false;
        liot_rtos_exit_critical();
        /* Preserve the SDK result; do not hide -40012 behind app -2004. */
        return ret;
    }
    liot_trace("[TIRTC] TiRtcStart submitted; waiting SYS_STARTED\r\n");

    deadline = liot_rtos_get_running_time() + DEMO_TIRTC_START_TIMEOUT_MS;
    while (!s_started_event && runtime_deadline_pending(deadline))
    {
        (void)liot_rtos_semaphore_wait(s_runtime_sem, 100U);
    }
    if (!s_started_event)
    {
        liot_trace("[TIRTC] SYS_STARTED timeout; graceful stop required\r\n");
        return DEMO_TIRTC_ERR_TIMEOUT;
    }

    liot_rtos_enter_critical();
    if (!s_restart_required)
    {
        s_ready = true;
    }
    liot_rtos_exit_critical();
    liot_trace("[TIRTC] ready version=%s free_heap=%u\r\n",
               TiRtcGetVersion(),
               (unsigned int)liot_xPortGetFreeHeapSize());
    return 0;
}

void demo_tirtc_set_listener(const demo_tirtc_listener_t *listener)
{
    demo_tirtc_set_feature_listener(DEMO_TIRTC_FEATURE_AI, listener);
}

bool demo_tirtc_admission_try_enter(void)
{
    bool entered = false;

    liot_rtos_enter_critical();
    if (s_ready && !s_restart_required && !s_admission_busy &&
        s_connection.owner == DEMO_TIRTC_OWNER_NONE)
    {
        s_admission_busy = true;
        entered = true;
    }
    liot_rtos_exit_critical();
    return entered;
}

void demo_tirtc_admission_leave(void)
{
    liot_rtos_enter_critical();
    s_admission_busy = false;
    liot_rtos_exit_critical();
}

void demo_tirtc_set_feature_listener(
    demo_tirtc_feature_e feature,
    const demo_tirtc_listener_t *listener)
{
    if (feature < 0 || feature >= DEMO_TIRTC_FEATURE_COUNT)
    {
        return;
    }
    liot_rtos_enter_critical();
    s_feature_listeners[feature] = listener;
    liot_rtos_exit_critical();
}

void demo_tirtc_set_incoming_listener(
    const demo_tirtc_incoming_listener_t *listener)
{
    liot_rtos_enter_critical();
    s_incoming_listener = listener;
    liot_rtos_exit_critical();
}

int demo_tirtc_session_claim(demo_tirtc_owner_e owner,
                             uint32_t *session_generation)
{
    int ret = TIRTC_E_BUSY;

    if (!runtime_owner_valid(owner) || session_generation == NULL)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    *session_generation = 0U;
    liot_rtos_enter_critical();
    if (!s_ready || s_restart_required)
    {
        ret = TIRTC_E_NOT_INITIALIZED;
    }
    else if (s_connection.owner == DEMO_TIRTC_OWNER_NONE &&
             s_connection.active == NULL && s_connection.closing == NULL &&
             !s_connection.connect_request_pending &&
             !s_connection.connect_callback_pending &&
             s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE &&
             s_connection.users == 0U && s_reject_pending == 0U &&
             s_accept_callbacks_pending == 0U && !s_admission_busy)
    {
        s_connection.next_session_generation = runtime_next_generation(
            s_connection.next_session_generation);
        s_connection.owner = owner;
        s_connection.session_generation =
            s_connection.next_session_generation;
        *session_generation = s_connection.session_generation;
        ret = 0;
    }
    liot_rtos_exit_critical();
    if (ret == 0)
    {
        liot_trace("[TIRTC-ADAPTER] session claimed owner=%u generation=%u\r\n",
                   (unsigned int)owner,
                   (unsigned int)*session_generation);
    }
    return ret;
}

int demo_tirtc_session_release(demo_tirtc_owner_e owner,
                               uint32_t session_generation)
{
    int ret = TIRTC_E_BUSY;
    bool retired = false;

    if (!runtime_owner_valid(owner) || session_generation == 0U)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    liot_rtos_enter_critical();
    if (s_retired_session_generation[owner] == session_generation)
    {
        /* Recovery is allowed to invalidate the old SDK lifetime before a
         * feature task reaches its cleanup step.  A later release of that
         * exact retired epoch is successful and cannot touch a new owner. */
        s_retired_session_generation[owner] = 0U;
        retired = true;
        ret = 0;
    }
    else if (s_connection.owner != owner ||
        s_connection.session_generation != session_generation)
    {
        ret = TIRTC_E_INVALID_HANDLE;
    }
    else if (s_connection.active == NULL && s_connection.closing == NULL &&
             !s_connection.connect_request_pending &&
             !s_connection.connect_callback_pending &&
             s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE &&
             s_connection.users == 0U)
    {
        s_connection.owner = DEMO_TIRTC_OWNER_NONE;
        s_connection.session_generation = 0U;
        ret = 0;
    }
    liot_rtos_exit_critical();
    if (ret == 0)
    {
        liot_trace("[TIRTC-ADAPTER] session released owner=%u generation=%u "
                   "retired=%u\r\n",
                   (unsigned int)owner, (unsigned int)session_generation,
                   retired ? 1U : 0U);
    }
    return ret;
}

int demo_tirtc_expect_incoming(demo_tirtc_owner_e owner,
                               uint32_t session_generation)
{
    int ret = TIRTC_E_BUSY;

    if (!runtime_owner_valid(owner) || session_generation == 0U)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    liot_rtos_enter_critical();
    if (!s_ready || s_restart_required)
    {
        ret = TIRTC_E_NOT_INITIALIZED;
    }
    else if (s_connection.owner != owner ||
             s_connection.session_generation != session_generation)
    {
        ret = TIRTC_E_INVALID_HANDLE;
    }
    else if (s_connection.active == NULL && s_connection.closing == NULL &&
             !s_connection.connect_request_pending &&
             !s_connection.connect_callback_pending &&
             s_connection.users == 0U && s_reject_pending == 0U &&
             s_accept_callbacks_pending == 0U &&
             (s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE ||
              (s_connection.expected_owner == owner &&
               s_connection.expected_generation == session_generation)))
    {
        s_connection.expected_owner = owner;
        s_connection.expected_generation = session_generation;
        ret = 0;
    }
    liot_rtos_exit_critical();
    return ret;
}

int demo_tirtc_cancel_expected_incoming(demo_tirtc_owner_e owner,
                                        uint32_t session_generation)
{
    int ret = TIRTC_E_INVALID_HANDLE;

    if (!runtime_owner_valid(owner) || session_generation == 0U)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    liot_rtos_enter_critical();
    if (s_connection.owner == owner &&
        s_connection.session_generation == session_generation &&
        s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE)
    {
        ret = 0;
    }
    else if (s_connection.owner == owner &&
             s_connection.session_generation == session_generation &&
             s_connection.expected_owner == owner &&
             s_connection.expected_generation == session_generation)
    {
        s_connection.expected_owner = DEMO_TIRTC_OWNER_NONE;
        s_connection.expected_generation = 0U;
        ret = 0;
    }
    liot_rtos_exit_critical();
    return ret;
}

int demo_tirtc_adopt_incoming(demo_tirtc_owner_e owner,
                              tirtc_conn_t connection,
                              uint32_t *session_generation)
{
    int ret = TIRTC_E_BUSY;

    if (!runtime_owner_valid(owner) || connection == NULL ||
        session_generation == NULL)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    *session_generation = 0U;
    liot_rtos_enter_critical();
    if (!s_ready || s_restart_required)
    {
        ret = TIRTC_E_NOT_INITIALIZED;
    }
    else if (s_connection.owner == owner &&
             s_connection.active == connection &&
             s_connection.active_owner == owner &&
             s_connection.session_generation != 0U &&
             s_connection.active_generation ==
                 s_connection.session_generation)
    {
        *session_generation = s_connection.session_generation;
        ret = 0;
    }
    else if (s_connection.owner == DEMO_TIRTC_OWNER_NONE &&
             s_connection.active == NULL && s_connection.closing == NULL &&
             !s_connection.connect_request_pending &&
             !s_connection.connect_callback_pending &&
             s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE &&
             s_connection.users == 0U && s_reject_pending == 0U)
    {
        s_connection.next_session_generation = runtime_next_generation(
            s_connection.next_session_generation);
        s_connection.owner = owner;
        s_connection.session_generation =
            s_connection.next_session_generation;
        s_connection.active = connection;
        s_connection.active_owner = owner;
        s_connection.active_generation =
            s_connection.session_generation;
        s_connection.connection_generation = runtime_next_generation(
            s_connection.connection_generation);
        *session_generation = s_connection.session_generation;
        ret = 0;
    }
    liot_rtos_exit_critical();
    if (ret == 0)
    {
        liot_trace("[TIRTC-ADAPTER] incoming adopted owner=%u generation=%u "
                   "handle=%p\r\n",
                   (unsigned int)owner,
                   (unsigned int)*session_generation, connection);
    }
    return ret;
}

static int runtime_connect_submit(runtime_connect_kind_e kind,
                                  demo_tirtc_owner_e owner,
                                  uint32_t session_generation,
                                  const char *remote,
                                  const char *token,
                                  TIRTCCONNECTCALLBACK callback,
                                  void *user_data)
{
    size_t remote_length;
    size_t token_length = 0U;
    uint32_t request_generation;
    int ret;

    if ((kind != RUNTIME_CONNECT_DEVICE && kind != RUNTIME_CONNECT_WHIP) ||
        !runtime_owner_valid(owner) || session_generation == 0U ||
        remote == NULL || remote[0] == '\0' || callback == NULL ||
        (kind == RUNTIME_CONNECT_WHIP &&
         (token == NULL || token[0] == '\0')))
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    remote_length = strlen(remote);
    if (token != NULL)
    {
        token_length = strlen(token);
    }
    if (remote_length >= sizeof(s_connect_context.remote) ||
        token_length >= sizeof(s_connect_context.token))
    {
        return TIRTC_E_INVALID_PARAMETER;
    }

    liot_rtos_enter_critical();
    if (!s_ready || s_restart_required)
    {
        liot_rtos_exit_critical();
        return TIRTC_E_NOT_INITIALIZED;
    }
    if (s_connection.owner != owner ||
        s_connection.session_generation != session_generation)
    {
        liot_rtos_exit_critical();
        return TIRTC_E_INVALID_HANDLE;
    }
    if (s_connection.active != NULL || s_connection.closing != NULL ||
        s_connection.expected_owner != DEMO_TIRTC_OWNER_NONE ||
        s_connection.connect_request_pending ||
        s_connection.connect_callback_pending ||
        s_connection.users != 0U || s_connect_context.pending ||
        s_reject_pending != 0U || s_accept_callbacks_pending != 0U)
    {
        liot_rtos_exit_critical();
        return TIRTC_E_BUSY;
    }

    request_generation = runtime_next_generation(
        s_connection.next_request_generation);
    s_connection.next_request_generation = request_generation;
    s_connection.connect_request_pending = true;
    s_connection.connect_callback_pending = true;
    s_connection.pending_owner = owner;
    s_connection.pending_generation = session_generation;
    s_connection.pending_request_generation = request_generation;
    s_connection.pending_handle = NULL;
    s_connection.pending_handle_released = false;
    s_connection.pending_error = 0;

    s_connect_context.pending = true;
    s_connect_context.kind = kind;
    s_connect_context.owner = owner;
    s_connect_context.session_generation = session_generation;
    s_connect_context.request_generation = request_generation;
    s_connect_context.callback = callback;
    s_connect_context.user_data = user_data;
    s_connect_context.token_present = token != NULL;
    memcpy(s_connect_context.remote, remote, remote_length + 1U);
    if (token != NULL)
    {
        memcpy(s_connect_context.token, token, token_length + 1U);
    }
    else
    {
        s_connect_context.token[0] = '\0';
    }
    liot_rtos_exit_critical();

    if (kind == RUNTIME_CONNECT_DEVICE)
    {
        ret = TiRtcConnect(s_connect_context.remote,
                           s_connect_context.token_present ?
                               s_connect_context.token : NULL,
                           runtime_managed_connect_result,
                           &s_connect_context);
    }
    else
    {
        ret = TiRtcWhipConnect(s_connect_context.remote,
                               s_connect_context.token,
                               runtime_managed_connect_result,
                               &s_connect_context);
    }
    if (ret != 0)
    {
        liot_rtos_enter_critical();
        if (s_connect_context.pending &&
            s_connect_context.request_generation == request_generation &&
            s_connection.pending_request_generation == request_generation)
        {
            runtime_pending_clear_locked();
            runtime_connect_context_clear_locked();
        }
        liot_rtos_exit_critical();
    }
    liot_trace("[TIRTC-ADAPTER] %s submit owner=%u generation=%u ret=%d\r\n",
               kind == RUNTIME_CONNECT_DEVICE ? "connect" : "whip",
               (unsigned int)owner, (unsigned int)session_generation, ret);
    return ret;
}

int demo_tirtc_connect(demo_tirtc_owner_e owner,
                       uint32_t session_generation,
                       const char *remote_id,
                       const char *token,
                       TIRTCCONNECTCALLBACK callback,
                       void *user_data)
{
    return runtime_connect_submit(RUNTIME_CONNECT_DEVICE, owner,
                                  session_generation, remote_id, token,
                                  callback, user_data);
}

int demo_tirtc_whip_connect(demo_tirtc_owner_e owner,
                            uint32_t session_generation,
                            const char *service_description,
                            const char *token,
                            TIRTCCONNECTCALLBACK callback,
                            void *user_data)
{
    return runtime_connect_submit(RUNTIME_CONNECT_WHIP, owner,
                                  session_generation, service_description,
                                  token, callback, user_data);
}

int demo_tirtc_send_command(demo_tirtc_owner_e owner,
                            uint32_t session_generation,
                            uint32_t command,
                            const void *data,
                            uint32_t length)
{
    tirtc_conn_t hconn = runtime_active_acquire(owner, session_generation);
    int ret;

    if (hconn == NULL)
    {
        return TIRTC_E_INVALID_HANDLE;
    }
    ret = TiRtcSendCommand(hconn, command, data, length);
    runtime_active_release(hconn);
    return ret;
}

int demo_tirtc_send_audio(demo_tirtc_owner_e owner,
                          uint32_t session_generation,
                          const TIRTCFRAMEINFO *frame,
                          const void *data)
{
    tirtc_conn_t hconn;
    int ret;

    if (frame == NULL || data == NULL)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    hconn = runtime_active_acquire(owner, session_generation);
    if (hconn == NULL)
    {
        return TIRTC_E_INVALID_HANDLE;
    }
    ret = TiRtcSendAudioStream(hconn, frame, data);
    runtime_active_release(hconn);
    return ret;
}

int demo_tirtc_subscribe_audio(demo_tirtc_owner_e owner,
                               uint32_t session_generation,
                               uint8_t stream_id)
{
    tirtc_conn_t hconn = runtime_active_acquire(owner, session_generation);
    int ret;

    if (hconn == NULL)
    {
        return TIRTC_E_INVALID_HANDLE;
    }
    ret = TiRtcSubscribeAudio(hconn, stream_id);
    runtime_active_release(hconn);
    return ret;
}

int demo_tirtc_get_send_buffer_used(demo_tirtc_owner_e owner,
                                    uint32_t session_generation,
                                    size_t *used_bytes)
{
    tirtc_conn_t hconn;

    if (used_bytes == NULL)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    *used_bytes = 0U;
    hconn = runtime_active_acquire(owner, session_generation);
    if (hconn == NULL)
    {
        return TIRTC_E_INVALID_HANDLE;
    }
    *used_bytes = TiRtcGetSendBufferUsed(hconn);
    runtime_active_release(hconn);
    return 0;
}

int demo_tirtc_disconnect(demo_tirtc_owner_e owner,
                          uint32_t session_generation)
{
    bool wake = false;
    int ret = TIRTC_E_INVALID_HANDLE;

    if (!runtime_owner_valid(owner) || session_generation == 0U)
    {
        return TIRTC_E_INVALID_PARAMETER;
    }
    liot_rtos_enter_critical();
    if (s_connection.owner != owner ||
        s_connection.session_generation != session_generation)
    {
        ret = TIRTC_E_INVALID_HANDLE;
    }
    else if (s_connection.closing != NULL &&
             s_connection.closing_owner == owner &&
             s_connection.closing_generation == session_generation)
    {
        ret = 0; /* idempotent while the SDK release callback is pending */
    }
    else if (s_connection.active != NULL &&
             s_connection.active_owner == owner &&
             s_connection.active_generation == session_generation &&
             runtime_reserve_closing_locked(s_connection.active, owner,
                                            session_generation))
    {
        runtime_active_clear_locked();
        s_connection.expected_owner = DEMO_TIRTC_OWNER_NONE;
        s_connection.expected_generation = 0U;
        wake = true;
        ret = 0;
    }
    else if (s_connection.connect_callback_pending &&
             s_connection.pending_owner == owner &&
             s_connection.pending_generation == session_generation)
    {
        /* TiRTC exposes no cancel primitive.  Invalidate this logical request;
         * its callback remains reserved and will close any late success. */
        s_connection.connect_request_pending = false;
        ret = 0;
    }
    else if (s_connection.expected_owner == owner &&
             s_connection.expected_generation == session_generation)
    {
        s_connection.expected_owner = DEMO_TIRTC_OWNER_NONE;
        s_connection.expected_generation = 0U;
        ret = 0;
    }
    liot_rtos_exit_critical();
    if (wake)
    {
        runtime_signal();
    }
    return ret;
}

void demo_tirtc_get_connection_snapshot(
    demo_tirtc_connection_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    liot_rtos_enter_critical();
    snapshot->owner = s_connection.owner;
    snapshot->session_generation = s_connection.session_generation;
    snapshot->connection_generation = s_connection.connection_generation;
    snapshot->connection_users = s_connection.users;
    snapshot->expected_incoming =
        s_connection.expected_owner != DEMO_TIRTC_OWNER_NONE;
    snapshot->connect_pending = s_connection.connect_request_pending;
    snapshot->connect_callback_pending =
        s_connection.connect_callback_pending;
    snapshot->connected = s_connection.active != NULL;
    snapshot->disconnect_pending = s_connection.closing != NULL;
    liot_rtos_exit_critical();
}

bool demo_tirtc_is_ready(void)
{
    return s_ready && !s_restart_required;
}

int demo_tirtc_get_error(void)
{
    return s_error;
}

void demo_tirtc_require_restart(int error)
{
    bool first = false;

    liot_rtos_enter_critical();
    if (!s_restart_required)
    {
        s_restart_required = true;
        s_restart_error = error != 0 ? error : TIRTC_E_INTERNAL_ERROR;
        s_error = s_restart_error;
        s_ready = false;
        first = true;
    }
    liot_rtos_exit_critical();
    if (first)
    {
        liot_trace("[TIRTC] terminal failure error=%d; "
                   "SDK recovery required\r\n", s_restart_error);
    }
    runtime_signal();
}

static bool runtime_connections_drained_for_stop(void)
{
    bool drained;

    liot_rtos_enter_critical();
    /* `owner` is intentionally not a drain condition.  A feature task can be
     * waiting for this teardown before it reaches session_release().  The old
     * owner/generation is retired in runtime_reset_after_uninit(), allowing
     * that late release to complete without blocking recovery or touching the
     * next SDK lifetime. */
    drained = s_connection.active == NULL &&
              s_connection.closing == NULL &&
              !s_connection.connect_request_pending &&
              !s_connection.connect_callback_pending &&
              s_connection.expected_owner == DEMO_TIRTC_OWNER_NONE &&
              s_connection.users == 0U && !s_connect_context.pending &&
              s_reject_pending == 0U &&
              s_accept_callbacks_pending == 0U && !s_admission_busy;
    liot_rtos_exit_critical();
    return drained;
}

/* Close admission and turn the active managed handle into a closing
 * reservation.  Keeping the reservation until on_disconnected is the key
 * lifecycle invariant from the official P4 adapter: TiRtcDisconnect() merely
 * submits work; it is not the release boundary. */
static bool runtime_begin_graceful_shutdown(void)
{
    bool wake = false;
    bool valid = true;

    liot_rtos_enter_critical();
    s_ready = false;
    s_connection.expected_owner = DEMO_TIRTC_OWNER_NONE;
    s_connection.expected_generation = 0U;
    if (s_connection.connect_callback_pending)
    {
        /* The SDK has no connect-cancel API.  A late successful callback is
         * converted into a closing reservation by the managed trampoline. */
        s_connection.connect_request_pending = false;
    }
    if (s_connection.active != NULL)
    {
        if (runtime_reserve_closing_locked(
                s_connection.active, s_connection.active_owner,
                s_connection.active_generation))
        {
            runtime_active_clear_locked();
            wake = true;
        }
        else
        {
            valid = false;
        }
    }
    liot_rtos_exit_critical();
    if (wake)
    {
        runtime_signal();
    }
    return valid;
}

/* Stop is asynchronous in TiRTC 2.3.0.  Only SYS_STOPPED permits Uninit.
 * This function runs solely on the adapter task, never inside an SDK callback. */
static int runtime_graceful_stop_and_uninit(void)
{
    uint32_t deadline;
    bool initialized;
    bool start_submitted;
    bool stopped;
    bool submit_stop = false;
    int ret;

    if (!runtime_begin_graceful_shutdown())
    {
        return TIRTC_E_BUSY;
    }

    deadline = liot_rtos_get_running_time() +
               DEMO_TIRTC_STOP_DRAIN_TIMEOUT_MS;
    while (runtime_deadline_pending(deadline))
    {
        if (runtime_connections_drained_for_stop())
        {
            /* A callback clears its pending counter immediately before its C
             * function returns.  Give that callback one scheduler tick to
             * leave the SDK stack, then verify that no late callback acquired
             * a handle before Stop/Uninit. */
            liot_rtos_task_sleep_ms(1U);
            if (runtime_connections_drained_for_stop())
            {
                break;
            }
        }
        runtime_process_managed_disconnect();
        runtime_process_rejects();
        (void)liot_rtos_semaphore_wait(s_runtime_sem, 50U);
    }
    if (!runtime_connections_drained_for_stop())
    {
        liot_trace("[TIRTC] graceful stop drain timeout active=%u\r\n",
                   (unsigned int)s_admission_busy);
        return DEMO_TIRTC_ERR_TIMEOUT;
    }

    liot_rtos_enter_critical();
    initialized = s_sdk_initialized;
    start_submitted = s_start_submitted;
    stopped = s_stopped_event;
    if (initialized && start_submitted && !stopped && !s_stop_requested)
    {
        s_stop_requested = true;
        s_stopped_event = false;
        submit_stop = true;
    }
    liot_rtos_exit_critical();

    if (!initialized)
    {
        return 0;
    }
    if (start_submitted && !stopped)
    {
        if (submit_stop)
        {
            liot_trace("[TIRTC] graceful TiRtcStop begin\r\n");
            ret = TiRtcStop();
            if (ret != 0)
            {
                /* A spontaneous SYS_STOPPED can race the stop submission. */
                liot_rtos_enter_critical();
                stopped = s_stopped_event;
                if (!stopped)
                {
                    s_stop_requested = false;
                }
                liot_rtos_exit_critical();
                if (!stopped)
                {
                    liot_trace("[TIRTC] TiRtcStop failed ret=%d (%s)\r\n",
                               ret, TiRtcGetErrorStr(ret));
                    return ret;
                }
            }
        }

        deadline = liot_rtos_get_running_time() +
                   DEMO_TIRTC_STOP_EVENT_TIMEOUT_MS;
        while (!s_stopped_event && runtime_deadline_pending(deadline))
        {
            (void)liot_rtos_semaphore_wait(s_runtime_sem, 100U);
        }
        if (!s_stopped_event)
        {
            liot_trace("[TIRTC] SYS_STOPPED timeout\r\n");
            return DEMO_TIRTC_ERR_TIMEOUT;
        }
    }

    liot_trace("[TIRTC] graceful TiRtcUninit begin\r\n");
    TiRtcUninit();
    liot_rtos_enter_critical();
    s_sdk_initialized = false;
    s_start_submitted = false;
    s_stop_requested = false;
    liot_rtos_exit_critical();
    liot_trace("[TIRTC] graceful TiRtcUninit complete\r\n");
    return 0;
}

static void runtime_reset_after_uninit(void)
{
    demo_tirtc_owner_e retired_owner;
    uint32_t retired_generation;
    uint32_t next_session;
    uint32_t next_request;
    uint32_t next_connection;
    uint32_t i;

    liot_rtos_enter_critical();
    /* Advance, rather than zero, epochs so a delayed feature worker from the
     * old SDK lifetime can never acquire a new lifetime's handle by accident. */
    next_session = runtime_next_generation(
        s_connection.next_session_generation);
    next_request = runtime_next_generation(
        s_connection.next_request_generation);
    next_connection = runtime_next_generation(
        s_connection.connection_generation);
    retired_owner = s_connection.owner;
    retired_generation = s_connection.session_generation;
    if (runtime_owner_valid(retired_owner) && retired_generation != 0U)
    {
        s_retired_session_generation[retired_owner] = retired_generation;
    }
    memset(&s_connection, 0, sizeof(s_connection));
    s_connection.next_session_generation = next_session;
    s_connection.next_request_generation = next_request;
    s_connection.connection_generation = next_connection;
    runtime_connect_context_clear_locked();
    for (i = 0U; i < DEMO_TIRTC_REJECT_QUEUE_DEPTH; ++i)
    {
        memset(&s_reject_slots[i], 0, sizeof(s_reject_slots[i]));
    }
    s_reject_pending = 0U;
    s_accept_callbacks_pending = 0U;
    s_admission_busy = false;
    s_started_event = false;
    s_stopped_event = false;
    s_sdk_initialized = false;
    s_start_submitted = false;
    s_stop_requested = false;
    s_ready = false;
    s_error = 0;
    s_retrying = true;
    s_restart_required = false;
    s_restart_error = 0;
    liot_rtos_exit_critical();
}

static void runtime_start_until_terminal(void)
{
#ifdef HWDEMO_BINDING_EN
    demo_binding_tirtc_identity_t identity;
    int ret;

    while (!s_ready && !s_restart_required)
    {
        memset(&identity, 0, sizeof(identity));
        while (demo_binding_get_tirtc_identity(&identity) != 0)
        {
            liot_rtos_task_sleep_ms(DEMO_TIRTC_IDENTITY_RETRY_MS);
        }
        ret = runtime_start(&identity);
        memset(&identity, 0, sizeof(identity));
        if (ret == 0)
        {
            s_error = 0;
            s_retrying = false;
            return;
        }

        s_error = ret;
        if (ret == DEMO_TIRTC_ERR_TIMEOUT)
        {
            /* Start was accepted, so Stop/SYS_STOPPED/Uninit is still
             * mandatory before another TiRtcInit attempt. */
            demo_tirtc_require_restart(ret);
            return;
        }
        if (!runtime_start_error_retryable(ret))
        {
            s_retrying = false;
            liot_trace("[TIRTC] runtime start stopped error=%d "
                       "(not retryable)\r\n", ret);
            return;
        }

        s_retrying = true;
        liot_trace("[TIRTC] runtime start error=%d; retry in %u ms\r\n",
                   ret, (unsigned int)DEMO_TIRTC_START_RETRY_MS);
        liot_rtos_task_sleep_ms(DEMO_TIRTC_START_RETRY_MS);
    }
#else
    s_error = DEMO_TIRTC_ERR_NOT_READY;
    liot_trace("[TIRTC] binding module is required for device mode\r\n");
#endif
}

static void runtime_process_required_restart(void)
{
    int error;
    int stop_ret;

    liot_rtos_enter_critical();
    if (!s_restart_required)
    {
        liot_rtos_exit_critical();
        return;
    }
    error = s_restart_error;
    liot_rtos_exit_critical();

    liot_trace("[TIRTC] runtime recovery requested error=%d\r\n", error);
    stop_ret = runtime_graceful_stop_and_uninit();
    if (stop_ret != 0)
    {
        liot_trace("[TIRTC] graceful recovery deferred ret=%d; "
                   "retry in %u ms (module reset disabled)\r\n",
                   stop_ret, (unsigned int)DEMO_TIRTC_START_RETRY_MS);
        liot_rtos_task_sleep_ms(DEMO_TIRTC_START_RETRY_MS);
        return;
    }

    runtime_reset_after_uninit();
    liot_trace("[TIRTC] graceful recovery complete; restarting SDK\r\n");
    runtime_start_until_terminal();
}

int demo_tirtc_wait_ready(uint32_t timeout_ms)
{
    uint32_t deadline = liot_rtos_get_running_time() + timeout_ms;

    while (!s_ready && !s_restart_required &&
           (s_error == 0 || s_retrying) &&
           runtime_deadline_pending(deadline))
    {
        liot_rtos_task_sleep_ms(100U);
    }
    if (s_ready)
    {
        return 0;
    }
    return s_error != 0 ? s_error : DEMO_TIRTC_ERR_TIMEOUT;
}

bool demo_tirtc_wait_connections_idle(uint32_t timeout_ms)
{
    uint32_t deadline = liot_rtos_get_running_time() + timeout_ms;
    uint32_t pending;
    bool managed_idle;

    while (1)
    {
        liot_rtos_enter_critical();
        pending = s_reject_pending;
        managed_idle = s_connection.active == NULL &&
                       s_connection.closing == NULL &&
                       !s_connection.connect_request_pending &&
                       !s_connection.connect_callback_pending &&
                       s_connection.expected_owner ==
                           DEMO_TIRTC_OWNER_NONE &&
                       s_connection.users == 0U &&
                       s_accept_callbacks_pending == 0U &&
                       !s_connect_context.pending;
        liot_rtos_exit_critical();
        if (pending == 0U && managed_idle)
        {
            return true;
        }
        if (timeout_ms == 0U || !runtime_deadline_pending(deadline))
        {
            return false;
        }
        liot_rtos_task_sleep_ms(10U);
    }
}

void demo_tirtc_task(void *argv)
{
    (void)argv;
    memset(&s_connection, 0, sizeof(s_connection));
    memset(&s_connect_context, 0, sizeof(s_connect_context));
    memset(s_reject_slots, 0, sizeof(s_reject_slots));
    memset(s_retired_session_generation, 0,
           sizeof(s_retired_session_generation));
    s_reject_pending = 0U;
    s_accept_callbacks_pending = 0U;
    if (liot_rtos_semaphore_create(&s_runtime_sem, 0U) != LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }
    if (liot_rtos_queue_create(&s_reject_queue, sizeof(tirtc_conn_t),
                               DEMO_TIRTC_REJECT_QUEUE_DEPTH) !=
        LIOT_OSI_SUCCESS)
    {
        goto init_failed;
    }

    liot_trace("[TIRTC] minimal adapter waiting for provisioned identity\r\n");
    runtime_start_until_terminal();

    while (1)
    {
        runtime_process_required_restart();
        runtime_process_managed_disconnect();
        runtime_process_rejects();
        (void)liot_rtos_semaphore_wait(s_runtime_sem, 500U);
    }

init_failed:
    if (s_reject_queue != NULL)
    {
        (void)liot_rtos_queue_delete(s_reject_queue);
        s_reject_queue = NULL;
    }
    if (s_runtime_sem != NULL)
    {
        (void)liot_rtos_semaphore_delete(s_runtime_sem);
        s_runtime_sem = NULL;
    }
    s_error = DEMO_TIRTC_ERR_INIT;
    liot_trace("[TIRTC] adapter queue/semaphore init failed\r\n");
    liot_rtos_task_delete(NULL);
}
