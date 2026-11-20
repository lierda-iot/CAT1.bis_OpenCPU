/**
 * @file demo_tcp.c
 * @brief TCP demo for OpenKit - Lamp control project (Project 3)
 *
 * Architecture: message-queue driven
 *   - Socket callback  : copies events/data into the queue, never does business work
 *   - Heartbeat timer  : only posts MSG_HEARTBEAT_TICK into the queue
 *   - Main task        : the single consumer; owns socket state and all sends
 *
 * All session state lives in one demo_tcp_ctx_t created on the task stack.
 * Its address is passed to the socket user_data and the timer arg, so the
 * callbacks can reach the queue without any file-scope variables. The task
 * never returns in normal operation; on an init failure the timer/socket are
 * torn down before the stack frame goes away.
 *
 * Flow:
 * 1. Get device IMEI
 * 2. Wait for network registration
 * 3. Open TCP socket to 49.235.147.4:9003 (Liot Socket API, async callback)
 * 4. Send JSON dev.reg on connect, wait for sys.resp (code == 0)
 * 5. Start a 3 s periodic timer; each tick sends one heartbeat
 *    (0xAA binary frame, or JSON when USE_BINARY_HEARTBEAT = 0)
 * 6. The main task runs forever (while (1)); a broken session is reset and
 *    the task keeps waiting for the next event
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-20
 * @version 1.6 - JSON registration + selectable heartbeat encoding
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "liot_rtc.h"
#include "lierda_app_main.h"
#include "liot_dev.h"
#include "liot_log.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_sockets.h"
/* ============================================================================
 * Configuration
 * ========================================================================== */

#define TCP_SERVER_IP "49.235.147.4"
#define TCP_SERVER_PORT 9003  /* Lamp control port */

#define HEARTBEAT_INTERVAL_MS 3000
#define REG_TIMEOUT_MS 5000
#define QUEUE_POLL_MS 200

/* First send can race with the async connect (EINPROGRESS); retry briefly. */
#define SEND_RETRY_COUNT 10
#define SEND_RETRY_DELAY_MS 100

#define RECV_BUFFER_SIZE 512
#define SEND_BUFFER_SIZE 512
#define QUEUE_DEPTH 16

#define DATA_CALL_CID 1
#define SIM_ID 0
#define NET_REG_RETRY_TIMES 10

#define PROJECT_NUMBER 3
#define STUDENT_NUMBER "2025001"
#define FW_VERSION "1.0.0"

/* Heartbeat encoding: 1 = 0xAA binary frame, 0 = JSON.
 * Registration is ALWAYS JSON dev.reg: the server needs stuNo/proj/fwVer. */
#define USE_BINARY_HEARTBEAT 0

/* Binary frame layout, protocol B.1 */
#define BIN_FRAME_HEADER 0xAA
#define BIN_FRAME_FOOTER 0x0F
#define BIN_TYPE_HEARTBEAT 0x01
#define HEARTBEAT_PAYLOAD_LEN 10

/* ============================================================================
 * Types
 * ========================================================================== */

typedef enum {
    MSG_SOCKET_CONNECTED = 1,
    MSG_SOCKET_DISCONNECTED,
    MSG_SOCKET_DATA_RECEIVED,
    MSG_HEARTBEAT_TICK,
    MSG_SOCKET_ERROR
} tcp_msg_type_e;

typedef struct {
    tcp_msg_type_e type;
    int32_t fd;
    uint32_t data_len;
    uint8_t data[256];
} tcp_msg_t;

typedef enum {
    ST_IDLE = 0,
    ST_CONNECTING,
    ST_REGISTERING,
    ST_ONLINE
} tcp_state_e;

/**
 * @brief Per-session state. Owned by the main task, reached from callbacks
 *        through user_data / timer arg.
 *
 * Note: every frame builder also needs this pointer, even if it only reads
 * the IMEI or sequence number, so the struct stays the single source of truth.
 */
typedef struct {
    char imei[20];
    uint8_t seq;

    liot_queue_t queue;
    liot_timer_t timer;

    int32_t socket_fd;
    tcp_state_e state;

    uint8_t reg_seq;
    uint8_t last_hb_seq;
    uint32_t reg_deadline;
    uint32_t start_tick;
    uint32_t uptime;
    uint16_t hb_sent;
    uint16_t hb_ok;

    /* TCP is a byte stream: a JSON message may be split across callbacks, or
     * several messages may arrive in one callback. */
    char rx[RECV_BUFFER_SIZE];
    size_t rx_len;
} demo_tcp_ctx_t;

/* ============================================================================
 * Helpers
 * ========================================================================== */

static uint32_t now_tick(void)
{
    return liot_rtos_get_system_tick();
}

static const char *json_get_string(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int json_get_int(const cJSON *root, const char *key, int def)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) ? item->valueint : def;
}

/**
 * @brief Post one message to the queue (non-blocking, safe in callbacks).
 */
static void post_msg(demo_tcp_ctx_t *ctx, const tcp_msg_t *msg)
{
    if (ctx == NULL || ctx->queue == NULL) {
        return;
    }

    if (liot_rtos_queue_release(ctx->queue, sizeof(*msg), (uint8_t *)msg,
                                LIOT_NO_WAIT) != LIOT_SUCCESS) {
        liot_trace("[TCP] Queue full, drop msg type=%d", msg->type);
    }
}

static void log_hex(const uint8_t *data, uint32_t len)
{
    char text[3 * 16 + 1];
    uint32_t n = (len < 16) ? len : 16;
    size_t pos = 0;

    for (uint32_t i = 0; i < n && pos + 4 < sizeof(text); i++) {
        pos += (size_t)snprintf(text + pos, sizeof(text) - pos, "%02X ", data[i]);
    }
    text[pos] = '\0';
    liot_trace("[TCP] RX binary (%u bytes): %s", len, text);
}

/* ============================================================================
 * Socket / timer event producers
 * ========================================================================== */

static void tcp_socket_callback(Liot_SocketEvent_e event, int socket_fd,
                                uint8_t *data, uint32_t data_len, void *user_data)
{
    demo_tcp_ctx_t *ctx = (demo_tcp_ctx_t *)user_data;
    tcp_msg_t msg = {0};

    if (ctx == NULL) {
        return;
    }

    switch (event) {
        case LIOT_SOCKET_CONNECT_SUCCESS:
            msg.type = MSG_SOCKET_CONNECTED;
            msg.fd = socket_fd;
            liot_trace("[TCP] Socket connected cb: cb_fd=%d open_fd=%d",
                       socket_fd, ctx->socket_fd);
            post_msg(ctx, &msg);
            break;

        case LIOT_SOCKET_RECV_DATA:
            if (data == NULL || data_len == 0) {
                break;
            }
            msg.type = MSG_SOCKET_DATA_RECEIVED;
            msg.fd = socket_fd;
            if (data_len >= sizeof(msg.data)) {
                liot_trace("[TCP] RX too large (%u), truncated", data_len);
                data_len = sizeof(msg.data) - 1;
            }
            memcpy(msg.data, data, data_len);
            msg.data[data_len] = '\0';
            msg.data_len = data_len;
            post_msg(ctx, &msg);
            break;

        case LIOT_SOCKET_DISCONNECT:
            msg.type = MSG_SOCKET_DISCONNECTED;
            msg.fd = socket_fd;
            post_msg(ctx, &msg);
            break;

        case LIOT_SOCKET_DNS_FAIL:
        case LIOT_SOCKET_CONNECT_FAIL:
            msg.type = MSG_SOCKET_ERROR;
            msg.fd = socket_fd;
            msg.data_len = (uint32_t)event;
            post_msg(ctx, &msg);
            break;

        default:
            liot_trace("[TCP] Unknown socket event: %d", event);
            break;
    }
}

static void heartbeat_timer_callback(void *arg)
{
    demo_tcp_ctx_t *ctx = (demo_tcp_ctx_t *)arg;
    tcp_msg_t msg = {0};

    if (ctx == NULL) {
        return;
    }

    msg.type = MSG_HEARTBEAT_TICK;
    post_msg(ctx, &msg);
}

/* ============================================================================
 * Frame builders
 * ========================================================================== */

#if USE_BINARY_HEARTBEAT

static uint8_t imei_tail_digit(const char *imei, size_t from_end)
{
    size_t len = strlen(imei);
    if (len < from_end) {
        return 0;
    }

    char c = imei[len - from_end];
    return (c >= '0' && c <= '9') ? (uint8_t)(c - '0') : 0;
}

static uint8_t calculate_xor(const uint8_t *data, size_t len)
{
    uint8_t x = 0;
    for (size_t i = 0; i < len; i++) {
        x ^= data[i];
    }
    return x;
}

/**
 * @brief Build a 0xAA binary heartbeat frame (protocol B.1, type=0x01).
 */
static int build_binary_heartbeat(demo_tcp_ctx_t *ctx, uint8_t *buffer,
                                  uint8_t *seq_out)
{
    uint32_t uptime = ctx->uptime;
    uint16_t count = (uint16_t)(ctx->hb_sent + 1);
    uint16_t success = ctx->hb_ok;
    uint8_t seq = ctx->seq;

    int8_t rssi = -75;
    liot_nw_signal_strength_info_s signal_info;
    if (liot_nw_get_signal_strength(SIM_ID, &signal_info) == LIOT_NW_SUCCESS) {
        rssi = (int8_t)signal_info.rsrp;
    }

    int idx = 0;
    buffer[idx++] = BIN_FRAME_HEADER;
    buffer[idx++] = imei_tail_digit(ctx->imei, 2);
    buffer[idx++] = imei_tail_digit(ctx->imei, 1);
    buffer[idx++] = BIN_TYPE_HEARTBEAT;
    buffer[idx++] = seq;
    buffer[idx++] = (HEARTBEAT_PAYLOAD_LEN >> 8) & 0xFF;
    buffer[idx++] = HEARTBEAT_PAYLOAD_LEN & 0xFF;

    buffer[idx++] = (uint8_t)rssi;
    buffer[idx++] = (uptime >> 24) & 0xFF;
    buffer[idx++] = (uptime >> 16) & 0xFF;
    buffer[idx++] = (uptime >> 8) & 0xFF;
    buffer[idx++] = uptime & 0xFF;
    buffer[idx++] = (count >> 8) & 0xFF;
    buffer[idx++] = count & 0xFF;
    buffer[idx++] = (success >> 8) & 0xFF;
    buffer[idx++] = success & 0xFF;
    buffer[idx++] = 0x00;  /* reserved */

    buffer[idx] = calculate_xor(buffer, (size_t)idx);
    idx++;
    buffer[idx++] = BIN_FRAME_FOOTER;

    ctx->seq = (uint8_t)((seq + 1) & 0xFF);
    if (seq_out) {
        *seq_out = seq;
    }
    return idx;
}

#endif /* USE_BINARY_HEARTBEAT */

/**
 * @brief Protocol timestamp in Unix seconds.
 */
static uint32_t current_ts_s(void)
{
    return (uint32_t)liot_rtc_get_time_s();
}

/**
 * @brief Registration is always JSON: the server needs the full identity
 *        (stuNo / proj / fwVer), which a 0xAA heartbeat frame cannot carry.
 */
static int build_registration_json(demo_tcp_ctx_t *ctx, char *buffer,
                                   size_t buf_size, uint8_t *seq_out)
{
    uint8_t seq = ctx->seq;
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *text = NULL;
    int result = -1;
    uint32_t ts_s = current_ts_s();

    if (root == NULL || data == NULL) {
        goto done;
    }

    liot_trace("[TCP] ts=%u s", ts_s);
    cJSON_AddStringToObject(root, "imei", ctx->imei);
    cJSON_AddNumberToObject(root, "ts", ts_s);
    cJSON_AddStringToObject(root, "type", "dev.reg");
    cJSON_AddNumberToObject(root, "seq", seq);

    cJSON_AddStringToObject(data, "stuNo", STUDENT_NUMBER);
    cJSON_AddNumberToObject(data, "proj", PROJECT_NUMBER);
    cJSON_AddStringToObject(data, "fwVer", FW_VERSION);
    cJSON_AddItemToObject(root, "data", data);
    data = NULL;  /* ownership moved to root */

    text = cJSON_PrintUnformatted(root);
    if (text != NULL) {
        int n = snprintf(buffer, buf_size, "%s\n", text);
        if (n > 0 && (size_t)n < buf_size) {
            ctx->seq = (uint8_t)((seq + 1) & 0xFF);
            if (seq_out) {
                *seq_out = seq;
            }
            result = n;
        } else {
            liot_trace("[TCP] Registration buffer too small");
        }
    }

done:
    if (text != NULL) {
        cJSON_free(text);
    }
    if (data != NULL) {
        cJSON_Delete(data);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return result;
}

#if !USE_BINARY_HEARTBEAT

static int build_heartbeat_json(demo_tcp_ctx_t *ctx, char *buffer,
                                size_t buf_size, uint8_t *seq_out)
{
    uint8_t seq = ctx->seq;
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *text = NULL;
    int result = -1;
    uint32_t ts_s = current_ts_s();

    if (root == NULL || data == NULL) {
        goto done;
    }

    liot_trace("[TCP] ts=%u s", ts_s);

    int8_t rssi = -75;
    liot_nw_signal_strength_info_s signal_info;
    if (liot_nw_get_signal_strength(SIM_ID, &signal_info) == LIOT_NW_SUCCESS) {
        rssi = (int8_t)signal_info.rsrp;
    }

    cJSON_AddStringToObject(root, "imei", ctx->imei);
    cJSON_AddNumberToObject(root, "ts", ts_s);
    cJSON_AddStringToObject(root, "type", "dev.hb");
    cJSON_AddNumberToObject(root, "seq", seq);

    cJSON_AddNumberToObject(data, "rssi", rssi);
    cJSON_AddNumberToObject(data, "uptime", ctx->uptime);
    cJSON_AddNumberToObject(data, "count", ctx->hb_sent + 1);
    cJSON_AddNumberToObject(data, "success", ctx->hb_ok);
    cJSON_AddItemToObject(root, "data", data);
    data = NULL;

    text = cJSON_PrintUnformatted(root);
    if (text != NULL) {
        int n = snprintf(buffer, buf_size, "%s\n", text);
        if (n > 0 && (size_t)n < buf_size) {
            ctx->seq = (uint8_t)((seq + 1) & 0xFF);
            if (seq_out) {
                *seq_out = seq;
            }
            result = n;
        } else {
            liot_trace("[TCP] Heartbeat buffer too small");
        }
    }

done:
    if (text != NULL) {
        cJSON_free(text);
    }
    if (data != NULL) {
        cJSON_Delete(data);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return result;
}

#endif /* !USE_BINARY_HEARTBEAT */

/* ============================================================================
 * Network / device helpers
 * ========================================================================== */

static int wait_network_ready(void)
{
    int times = 0;
    liot_nw_reg_status_info_s reg_info;

    liot_trace("[TCP] Waiting for network registration...");

    while (times < NET_REG_RETRY_TIMES) {
        memset(&reg_info, 0, sizeof(reg_info));

        liot_nw_errcode_e ret = liot_nw_get_reg_status(SIM_ID, &reg_info);
        if (ret == LIOT_NW_SUCCESS) {
            if (reg_info.data_reg.state == LIOT_NW_REG_STATE_HOME_NETWORK ||
                reg_info.data_reg.state == LIOT_NW_REG_STATE_ROAMING) {
                liot_trace("[TCP] Network registered (state=%d)",
                           reg_info.data_reg.state);
                return 0;
            } else if (reg_info.data_reg.state == LIOT_NW_REG_STATE_DENIED) {
                liot_trace("[TCP] Network registration denied");
                return -1;
            }
        }

        times++;
        liot_rtos_task_sleep_s(2);
    }

    liot_trace("[TCP] Network registration timeout");
    return -1;
}

static int get_device_imei(demo_tcp_ctx_t *ctx)
{
    liot_errcode_dev_e ret = liot_dev_get_imei(ctx->imei, sizeof(ctx->imei),
                                               SIM_ID);
    if (ret != LIOT_DEV_SUCCESS) {
        liot_trace("[TCP] Failed to get IMEI: 0x%x", ret);
        return -1;
    }
    liot_trace("[TCP] IMEI: %s", ctx->imei);
    return 0;
}

/* ============================================================================
 * Socket send paths (main task only)
 * ========================================================================== */

static bool send_bytes(demo_tcp_ctx_t *ctx, const uint8_t *data, uint16_t len)
{
    if (ctx->socket_fd < 0) {
        liot_trace("[TCP] send_bytes: invalid fd=%d", ctx->socket_fd);
        return false;
    }

    for (int attempt = 1; attempt <= SEND_RETRY_COUNT; attempt++) {
        int32_t status = Liot_SocketGetStatus(ctx->socket_fd);
        int32_t sent = Liot_SocketSend(ctx->socket_fd, (uint8_t *)data, len);

        liot_trace("[TCP] send attempt %d/%d: fd=%d len=%u status=%d ret=%d",
                   attempt, SEND_RETRY_COUNT, ctx->socket_fd, len, status, sent);

        if (sent == (int32_t)len) {
            return true;
        }

        if (attempt < SEND_RETRY_COUNT) {
            liot_rtos_task_sleep_ms(SEND_RETRY_DELAY_MS);
        }
    }

    liot_trace("[TCP] Send failed after %d attempts: fd=%d len=%u state=%d",
               SEND_RETRY_COUNT, ctx->socket_fd, len, ctx->state);
    log_hex(data, len);
    return false;
}

static bool send_registration(demo_tcp_ctx_t *ctx)
{
    uint8_t seq = 0;
    char buffer[SEND_BUFFER_SIZE];
    int len;

    liot_trace("[TCP] send_registration: fd=%d next_seq=%u",
               ctx->socket_fd, ctx->seq);

    len = build_registration_json(ctx, buffer, sizeof(buffer), &seq);
    liot_trace("[TCP] registration built: seq=%u len=%d", seq, len);
    if (len <= 0) {
        return false;
    }

    liot_trace("[TCP] Sending registration: %.*s", len - 1, buffer);
    if (!send_bytes(ctx, (const uint8_t *)buffer, (uint16_t)len)) {
        return false;
    }

    ctx->reg_seq = seq;
    ctx->reg_deadline = now_tick() + REG_TIMEOUT_MS;
    ctx->state = ST_REGISTERING;
    return true;
}

static bool send_heartbeat(demo_tcp_ctx_t *ctx)
{
    uint8_t seq = 0;
    int len = 0;
    bool ok = false;

    ctx->uptime = (now_tick() - ctx->start_tick) / 1000U;

#if USE_BINARY_HEARTBEAT
    uint8_t frame[32];
    len = build_binary_heartbeat(ctx, frame, &seq);
    if (len > 0) {
        liot_trace("[TCP] Heartbeat #%u (binary) uptime=%us",
                   ctx->hb_sent + 1, ctx->uptime);
        ok = send_bytes(ctx, frame, (uint16_t)len);
    }
#else
    char buffer[SEND_BUFFER_SIZE];
    len = build_heartbeat_json(ctx, buffer, sizeof(buffer), &seq);
    if (len > 0) {
        liot_trace("[TCP] Heartbeat #%u (json) uptime=%us",
                   ctx->hb_sent + 1, ctx->uptime);
        ok = send_bytes(ctx, (const uint8_t *)buffer, (uint16_t)len);
    }
#endif

    if (!ok) {
        return false;
    }

    ctx->hb_sent++;
    ctx->last_hb_seq = seq;
    return true;
}

/* ============================================================================
 * Session lifecycle
 * ========================================================================== */

/**
 * @brief Close the current TCP session but keep the task, queue and timer.
 *        The task goes back to ST_IDLE and keeps waiting for messages.
 */
static void reset_connection(demo_tcp_ctx_t *ctx, const char *reason)
{
    liot_trace("[TCP] Session reset: %s", reason);

    if (ctx->timer != NULL) {
        liot_rtos_timer_stop(ctx->timer);
    }

    if (ctx->socket_fd >= 0) {
        Liot_SocketClose(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    ctx->state = ST_IDLE;
    ctx->rx_len = 0;
    ctx->hb_sent = 0;
    ctx->hb_ok = 0;
}

/**
 * @brief Final cleanup, used only when initialization fails.
 */
static void demo_tcp_cleanup(demo_tcp_ctx_t *ctx)
{
    reset_connection(ctx, "task cleanup");

    if (ctx->timer != NULL) {
        liot_rtos_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }

    if (ctx->queue != NULL) {
        /* Give a pending socket callback a chance to drain before freeing. */
        liot_rtos_task_sleep_ms(100);
        liot_queue_t q = ctx->queue;
        ctx->queue = NULL;
        liot_rtos_queue_delete(q);
    }
}

/* ============================================================================
 * Downlink handling
 * ========================================================================== */

static void handle_server_line(demo_tcp_ctx_t *ctx, char *line)
{
    while (*line == ' ' || *line == '\r' || *line == '\t') {
        line++;
    }
    if (*line == '\0') {
        return;
    }

    liot_trace("[TCP] RX line: %s", line);

    if (*line != '{') {
        log_hex((const uint8_t *)line, (uint32_t)strlen(line));
        return;
    }

    cJSON *root = cJSON_Parse(line);
    if (root == NULL) {
        liot_trace("[TCP] JSON parse failed: %s", line);
        return;
    }

    const char *type = json_get_string(root, "type");
    if (type != NULL && strcmp(type, "sys.resp") == 0) {
        int code = json_get_int(root, "code", -1);
        int seq = json_get_int(root, "seq", -1);
        const char *msg = json_get_string(root, "msg");

        if (ctx->state == ST_REGISTERING && seq == (int)ctx->reg_seq) {
            if (code == 0) {
                liot_trace("[TCP] Registration OK");
                ctx->state = ST_ONLINE;
                ctx->start_tick = now_tick();
                ctx->hb_sent = 0;
                ctx->hb_ok = 0;
                if (liot_rtos_timer_start(ctx->timer, HEARTBEAT_INTERVAL_MS)
                    != LIOT_OSI_SUCCESS) {
                    liot_trace("[TCP] Failed to start heartbeat timer");
                    reset_connection(ctx, "timer start failed");
                } else {
                    liot_trace("[TCP] Heartbeat timer started (%dms)",
                               HEARTBEAT_INTERVAL_MS);
                }
            } else {
                liot_trace("[TCP] Registration rejected: code=%d msg=%s",
                           code, msg ? msg : "");
                reset_connection(ctx, "registration rejected");
            }
        } else if (ctx->state == ST_ONLINE && seq == (int)ctx->last_hb_seq) {
            if (code == 0) {
                ctx->hb_ok++;
            } else {
                liot_trace("[TCP] Heartbeat rejected: code=%d msg=%s",
                           code, msg ? msg : "");
            }
        } else {
            liot_trace("[TCP] sys.resp seq=%d code=%d (unexpected)", seq, code);
        }
    } else {
        liot_trace("[TCP] Downlink type=%s: %s", type ? type : "?", line);
    }

    cJSON_Delete(root);
}

/**
 * @brief Append received bytes and dispatch every complete '\n' terminated line.
 */
static void rx_process(demo_tcp_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    if (len >= sizeof(ctx->rx) || ctx->rx_len + len >= sizeof(ctx->rx)) {
        liot_trace("[TCP] RX overflow (rx=%u, new=%u), reset",
                   (unsigned)ctx->rx_len, len);
        ctx->rx_len = 0;
        return;
    }

    memcpy(ctx->rx + ctx->rx_len, data, len);
    ctx->rx_len += len;
    ctx->rx[ctx->rx_len] = '\0';

    size_t start = 0;
    for (size_t i = 0; i < ctx->rx_len; i++) {
        if (ctx->rx[i] == '\n') {
            ctx->rx[i] = '\0';
            handle_server_line(ctx, ctx->rx + start);
            start = i + 1;
        }
    }

    if (start > 0) {
        ctx->rx_len -= start;
        memmove(ctx->rx, ctx->rx + start, ctx->rx_len);
        ctx->rx[ctx->rx_len] = '\0';
    }
}

/* ============================================================================
 * Message handlers (main task)
 * ========================================================================== */

static void handle_message(demo_tcp_ctx_t *ctx, const tcp_msg_t *msg)
{
    switch (msg->type) {
        case MSG_SOCKET_CONNECTED:
            liot_trace("[TCP] MSG_SOCKET_CONNECTED: cb_fd=%d open_fd=%d",
                       msg->fd, ctx->socket_fd);
            ctx->socket_fd = msg->fd;
            ctx->state = ST_CONNECTING;
            if (!send_registration(ctx)) {
                liot_trace("[TCP] Failed to send registration");
                reset_connection(ctx, "send registration failed");
            }
            break;

        case MSG_SOCKET_DATA_RECEIVED:
            rx_process(ctx, msg->data, msg->data_len);
            break;

        case MSG_HEARTBEAT_TICK:
            if (ctx->state != ST_ONLINE) {
                break;
            }
            if (!send_heartbeat(ctx)) {
                liot_trace("[TCP] Failed to send heartbeat");
                reset_connection(ctx, "heartbeat send failed");
            }
            break;

        case MSG_SOCKET_DISCONNECTED:
            reset_connection(ctx, "socket disconnected");
            break;

        case MSG_SOCKET_ERROR:
            liot_trace("[TCP] Socket error, event=%u", msg->data_len);
            reset_connection(ctx, "socket error");
            break;

        default:
            break;
    }
}

static void check_registration_timeout(demo_tcp_ctx_t *ctx)
{
    if (ctx->state == ST_REGISTERING &&
        (int32_t)(now_tick() - ctx->reg_deadline) >= 0) {
        liot_trace("[TCP] Registration response timeout");
        reset_connection(ctx, "registration timeout");
    }
}

/* ============================================================================
 * Entry
 * ========================================================================== */

void demo_tcp_demo_thread(void *arg)
{
    demo_tcp_ctx_t ctx;
    tcp_msg_t msg;
    Liot_SocketInfo_t socket_info;
    (void)arg;

    memset(&ctx, 0, sizeof(ctx));
    ctx.socket_fd = -1;

    liot_trace("[TCP] Demo task started");

    if (liot_rtos_queue_create(&ctx.queue, sizeof(tcp_msg_t), QUEUE_DEPTH)
        != LIOT_OSI_SUCCESS) {
        liot_trace("[TCP] Failed to create message queue");
        return;
    }

    if (liot_rtos_timer_create(&ctx.timer, LIOT_TimerPeriodic,
                               heartbeat_timer_callback, &ctx)
        != LIOT_OSI_SUCCESS) {
        liot_trace("[TCP] Failed to create heartbeat timer");
        ctx.timer = NULL;
        demo_tcp_cleanup(&ctx);
        return;
    }

    if (get_device_imei(&ctx) != 0) {
        demo_tcp_cleanup(&ctx);
        return;
    }

    if (wait_network_ready() != 0) {
        demo_tcp_cleanup(&ctx);
        return;
    }

    memset(&socket_info, 0, sizeof(socket_info));
    socket_info.cid = DATA_CALL_CID;
    socket_info.socket_type = LIOT_SOCKET_TYPE_TCP;
    strncpy((char *)socket_info.host, TCP_SERVER_IP,
            sizeof(socket_info.host) - 1);
    socket_info.port = TCP_SERVER_PORT;
    socket_info.local_port = 0;
    socket_info.keepidle = 30;
    socket_info.keepinterval = 30;
    socket_info.keepcount = 5;

    liot_trace("[TCP] Connecting to %s:%d", TCP_SERVER_IP, TCP_SERVER_PORT);

    /* Liot_SocketOpen returns a status code (0 = success), NOT the fd.
     * The real fd arrives later through tcp_socket_callback(). */
    int32_t open_ret = Liot_SocketOpen(&socket_info, tcp_socket_callback, &ctx);
    liot_trace("[TCP] Liot_SocketOpen ret=%d (0=ok, fd comes from callback)",
               open_ret);
    if (open_ret < 0) {
        liot_trace("[TCP] Failed to open socket");
        demo_tcp_cleanup(&ctx);
        return;
    }
    ctx.state = ST_CONNECTING;

    /* The message consumer never exits. A broken session only resets the
     * socket/state; the task keeps waiting for the next event. */
    while (1) {
        if (liot_rtos_queue_wait(ctx.queue, (uint8_t *)&msg, sizeof(msg),
                                 QUEUE_POLL_MS)
            == LIOT_SUCCESS) {
            handle_message(&ctx, &msg);
        } else {
            check_registration_timeout(&ctx);
        }
    }
}
