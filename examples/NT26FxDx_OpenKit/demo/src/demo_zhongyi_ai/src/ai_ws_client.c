/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Persistent WebSocket client using SDK's liot_ssl2.
 * Lifecycle: connect (TCP+TLS+upgrade+hello) → send/recv → close.
 */

#include "ai_ws_client.h"

#include <stdio.h>
#include <string.h>

#include "ai_app_log.h"
#include "liot_os.h"
#include "liot_ssl2.h"

#define AI_WS_SSL_CLIENT_ID    2
#define AI_WS_RECV_BUF_SIZE    (32 * 1024)
#define AI_WS_UPGRADE_BUF_SIZE 1024
#define AI_WS_CONNECT_TIMEOUT_MS 30000
#define AI_WS_FRAME_HEADER_MAX 14

/* --- Receive ring buffer bridging callback to sync reads --- */

typedef struct {
    uint8_t buf[AI_WS_RECV_BUF_SIZE];
    volatile int write_pos;
    volatile int read_pos;
    liot_sem_t data_sem;
    volatile bool connected;
} ai_ws_rx_t;

static ai_ws_rx_t s_rx;
static Liot_sslClientInfo_t s_ssl_info;
static Liot_sslContext_t s_ssl_ctx;
static char s_session_id[AI_PROTOCOL_SESSION_ID_MAX];

static void ai_ws_rx_init(void)
{
    memset(&s_rx, 0, sizeof(s_rx));
    liot_rtos_semaphore_create(&s_rx.data_sem, 0);
}

static void ai_ws_rx_deinit(void)
{
    if (s_rx.data_sem != NULL) {
        liot_rtos_semaphore_delete(s_rx.data_sem);
        s_rx.data_sem = NULL;
    }
}

static void ai_ws_ssl_recv_cb(uint8_t clientId, void *data, uint32_t dataLen, void *arg)
{
    (void)clientId;
    (void)arg;

    if (data == NULL || dataLen == 0) {
        return;
    }

    /* Compact: move unread data to front if write_pos is near end */
    int avail = s_rx.write_pos - s_rx.read_pos;
    if (s_rx.write_pos + (int)dataLen > AI_WS_RECV_BUF_SIZE && avail < AI_WS_RECV_BUF_SIZE / 2) {
        if (avail > 0) {
            memmove(s_rx.buf, s_rx.buf + s_rx.read_pos, avail);
        }
        s_rx.write_pos = avail;
        s_rx.read_pos = 0;
    }

    int space = AI_WS_RECV_BUF_SIZE - s_rx.write_pos;
    if ((int)dataLen > space) {
        liot_trace("ai_ws rx overflow drop=%u", (unsigned)dataLen);
        return;
    }

    memcpy(s_rx.buf + s_rx.write_pos, data, dataLen);
    s_rx.write_pos += (int)dataLen;

    if (s_rx.data_sem != NULL) {
        liot_rtos_semaphore_release(s_rx.data_sem);
    }
}

static int ai_ws_read(uint8_t *out, int len, int timeout_ms)
{
    int elapsed = 0;
    int copied = 0;

    while (copied < len) {
        int avail = s_rx.write_pos - s_rx.read_pos;
        if (avail > 0) {
            int to_copy = (avail < (len - copied)) ? avail : (len - copied);
            memcpy(out + copied, s_rx.buf + s_rx.read_pos, to_copy);
            s_rx.read_pos += to_copy;
            copied += to_copy;

            if (s_rx.read_pos == s_rx.write_pos) {
                s_rx.read_pos = 0;
                s_rx.write_pos = 0;
            }
        } else {
            if (elapsed >= timeout_ms) {
                break;
            }
            liot_rtos_semaphore_wait(s_rx.data_sem, 500);
            elapsed += 500;
        }
    }

    return copied;
}

/* --- SSL2 connection management --- */

static int ai_ws_ssl2_connect(const char *host, int port)
{
    int ret = 0;
    int status = 0;
    int waited = 0;

    memset(&s_ssl_info, 0, sizeof(s_ssl_info));
    memset(&s_ssl_ctx, 0, sizeof(s_ssl_ctx));

    s_ssl_ctx.auth_type = LIOT_SSL_VERIFY_NONE;
    s_ssl_ctx.ssl_version = LIOT_SSL_VERSION_3;
    s_ssl_ctx.sni_support = 1;
    s_ssl_ctx.ciphersuite[0] = 0xFFFF;
    s_ssl_ctx.r_timeout = 30;
    s_ssl_ctx.s_timeout = 30;

    s_ssl_info.sslClientId = AI_WS_SSL_CLIENT_ID;
    s_ssl_info.socket_type = 1;
    s_ssl_info.host = (uint8_t *)host;
    s_ssl_info.port = (uint16_t)port;
    s_ssl_info.ssl_context = &s_ssl_ctx;
    s_ssl_info.sslClient_cb = ai_ws_ssl_recv_cb;

    ret = Liot_SSLSetCfg(&s_ssl_info);
    if (ret != LIOT_SSL_SUCCESS) {
        liot_trace("ai_ws SSLSetCfg failed ret=%d", ret);
        return -1;
    }

    ret = Liot_SSLSocketOpen(AI_WS_SSL_CLIENT_ID);
    if (ret != LIOT_SSL_SUCCESS) {
        liot_trace("ai_ws SSLSocketOpen failed ret=%d", ret);
        return -2;
    }

    while (waited < AI_WS_CONNECT_TIMEOUT_MS) {
        status = Liot_SSLSocketGetStatus(AI_WS_SSL_CLIENT_ID);
        if (status == LIOT_SSL_CLIENT_STATUS_CONNECTED) {
            break;
        }
        if (status == LIOT_SSL_CLIENT_STATUS_CLOSED ||
            status == LIOT_SSL_CLIENT_STATUS_DISCONNECTED) {
            liot_trace("ai_ws connection failed status=%d", status);
            return -3;
        }
        liot_rtos_task_sleep_ms(200);
        waited += 200;
    }

    if (status != LIOT_SSL_CLIENT_STATUS_CONNECTED) {
        liot_trace("ai_ws connect timeout status=%d", status);
        return -4;
    }

    liot_trace("ai_ws SSL2 connected");
    return 0;
}

static int ai_ws_ssl2_write(const void *buf, int len)
{
    return Liot_SSLSocketSend(AI_WS_SSL_CLIENT_ID, (uint8_t *)buf, (uint16_t)len);
}

/* --- WebSocket framing --- */

static void ai_ws_generate_mask(uint8_t mask[4])
{
    uint32_t r = liot_rtos_rand();
    mask[0] = (uint8_t)(r & 0xFF);
    mask[1] = (uint8_t)((r >> 8) & 0xFF);
    mask[2] = (uint8_t)((r >> 16) & 0xFF);
    mask[3] = (uint8_t)((r >> 24) & 0xFF);
}

static int ai_ws_send_frame(uint8_t opcode, const void *payload, int payload_len)
{
    uint8_t header[AI_WS_FRAME_HEADER_MAX];
    uint8_t mask[4];
    int hdr_len = 0;
    int ret = 0;
    uint8_t *send_buf = NULL;

    header[0] = 0x80 | (opcode & 0x0F);
    if (payload_len < 126) {
        header[1] = 0x80 | (uint8_t)payload_len;
        hdr_len = 2;
    } else {
        header[1] = 0x80 | 126;
        header[2] = (uint8_t)((payload_len >> 8) & 0xFF);
        header[3] = (uint8_t)(payload_len & 0xFF);
        hdr_len = 4;
    }

    ai_ws_generate_mask(mask);
    memcpy(header + hdr_len, mask, 4);
    hdr_len += 4;

    send_buf = liot_rtos_malloc(hdr_len + payload_len);
    if (send_buf == NULL) {
        return -1;
    }

    memcpy(send_buf, header, hdr_len);
    for (int i = 0; i < payload_len; i++) {
        send_buf[hdr_len + i] = ((const uint8_t *)payload)[i] ^ mask[i % 4];
    }

    ret = ai_ws_ssl2_write(send_buf, hdr_len + payload_len);
    liot_rtos_free(send_buf);

    return (ret >= 0) ? 0 : -2;
}

/* --- WebSocket upgrade handshake --- */

static void ai_ws_generate_trace_id(char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";
    uint32_t random_value = 0;

    if ((out == NULL) || (out_size < (AI_PROTOCOL_TRACE_ID_LEN + 1U))) {
        return;
    }

    for (int i = 0; i < AI_PROTOCOL_TRACE_ID_LEN; i++) {
        if ((i % 8) == 0) {
            random_value = liot_rtos_rand();
        }
        out[i] = hex[(random_value >> ((i % 8) * 4)) & 0x0FU];
    }
    out[AI_PROTOCOL_TRACE_ID_LEN] = '\0';
}

static int ai_ws_do_upgrade(const ai_app_config_t *cfg,
                            const char *ai_token, const char *trace_id)
{
    char buf[AI_WS_UPGRADE_BUF_SIZE];
    int n = 0;
    int ret = 0;
    int total_read = 0;

    n = snprintf(buf, sizeof(buf),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                 "Sec-WebSocket-Version: 13\r\n"
                 "trace-id: %s\r\n"
                 "device-id: %s\r\n"
                 "device-type: %s\r\n"
                 "client-id: aiDevice\r\n"
                 "Authorization: Bearer %s\r\n"
                 "\r\n",
                 cfg->ws_path, cfg->ws_host,
                 trace_id, cfg->device_id, cfg->device_type, ai_token);

    if (n <= 0 || (size_t)n >= sizeof(buf)) {
        liot_trace("ai_ws upgrade request too long");
        return -1;
    }

    ret = ai_ws_ssl2_write(buf, n);
    if (ret < 0) {
        liot_trace("ai_ws upgrade send failed ret=%d", ret);
        return -2;
    }

    memset(buf, 0, sizeof(buf));
    total_read = 0;
    while (total_read < (int)(sizeof(buf) - 1)) {
        int rd = ai_ws_read((uint8_t *)buf + total_read, 1, 30000);
        if (rd <= 0) {
            liot_trace("ai_ws upgrade read failed rd=%d pos=%d", rd, total_read);
            return -3;
        }
        total_read += rd;
        if (total_read >= 4 &&
            buf[total_read - 4] == '\r' && buf[total_read - 3] == '\n' &&
            buf[total_read - 2] == '\r' && buf[total_read - 1] == '\n') {
            break;
        }
    }
    buf[total_read] = '\0';

    if (strstr(buf, "101") == NULL) {
        liot_trace("ai_ws upgrade rejected: %.80s", buf);
        return -4;
    }

    liot_trace("ai_ws upgrade success");
    return 0;
}

/* --- Public API --- */

int ai_ws_client_connect(const ai_app_config_t *cfg, const char *token,
                         ai_ws_client_result_t *result)
{
    char trace_id[AI_PROTOCOL_TRACE_ID_LEN + 1] = {0};
    char *frame_buf = NULL;
    int opcode = 0;
    int frame_len = 0;
    int ret = -1;

    if ((cfg == NULL) || ai_app_config_string_empty(token)) {
        return -1;
    }

    s_session_id[0] = '\0';
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }

    ai_ws_generate_trace_id(trace_id, sizeof(trace_id));
    ai_ws_rx_init();

    frame_buf = liot_rtos_malloc(AI_WS_RECV_BUF_SIZE);
    if (frame_buf == NULL) {
        liot_trace("ai_ws malloc frame_buf failed");
        ai_ws_rx_deinit();
        return -2;
    }

    liot_trace("ai_ws connecting to %s:%d", cfg->ws_host, cfg->ws_port);
    ret = ai_ws_ssl2_connect(cfg->ws_host, cfg->ws_port);
    if (ret != 0) {
        ret = -10 + ret;
        goto fail;
    }

    ret = ai_ws_do_upgrade(cfg, token, trace_id);
    if (ret != 0) {
        ret = -30;
        goto fail;
    }

    /* Wait for "connection" message */
    frame_len = ai_ws_client_recv_frame((uint8_t *)frame_buf, AI_WS_RECV_BUF_SIZE,
                                        &opcode, 30000);
    if (frame_len <= 0 || opcode != AI_WS_OPCODE_TEXT) {
        liot_trace("ai_ws no connection msg len=%d opcode=%d", frame_len, opcode);
        ret = -40;
        goto fail;
    }

    frame_buf[frame_len] = '\0';
    liot_trace("ai_ws recv connection len=%d", frame_len);
    if (!ai_protocol_extract_session_id(frame_buf, (size_t)frame_len,
                                        s_session_id, sizeof(s_session_id))) {
        liot_trace("ai_ws no session_id in connection msg");
        ret = -41;
        goto fail;
    }
    liot_trace("ai_ws session_id=%s", s_session_id);

    /* Send hello — use snprintf to avoid cJSON heap alloc (HTTP lib may corrupt heap) */
    {
        char hello_buf[512];
        int hello_len = snprintf(hello_buf, sizeof(hello_buf),
            "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
            "\"audio_params\":{\"sample_rate\":16000,\"channels\":1,"
            "\"format\":\"opus\",\"frame_duration\":60,\"down_format\":\"opus\"},"
            "\"session_id\":\"%s\",\"wake_type\":\"1\","
            "\"wake_id\":\"\",\"msg_id\":\"\",\"trace_id\":\"%s\"}",
            s_session_id, trace_id);
        if (hello_len <= 0 || (size_t)hello_len >= sizeof(hello_buf)) {
            ret = -50;
            goto fail;
        }
        liot_trace("ai_ws sending hello len=%d", hello_len);
        ret = ai_ws_send_frame(AI_WS_OPCODE_TEXT, hello_buf, hello_len);
        if (ret != 0) {
            liot_trace("ai_ws send hello failed ret=%d", ret);
            ret = -51;
            goto fail;
        }
    }
    liot_trace("ai_ws sent hello");

    /* Wait for hello ack */
    frame_len = ai_ws_client_recv_frame((uint8_t *)frame_buf, AI_WS_RECV_BUF_SIZE,
                                        &opcode, 30000);
    if (frame_len <= 0 || opcode != AI_WS_OPCODE_TEXT) {
        liot_trace("ai_ws no hello ack len=%d opcode=%d", frame_len, opcode);
        ret = -60;
        goto fail;
    }
    frame_buf[frame_len] = '\0';

    if (!ai_protocol_is_hello_ack(frame_buf, (size_t)frame_len)) {
        liot_trace("ai_ws unexpected msg: %.128s", frame_buf);
        ret = -61;
        goto fail;
    }
    liot_trace("ai_ws hello ack confirmed, connected");

    s_rx.connected = true;
    ret = 0;
    if (result != NULL) {
        snprintf(result->trace_id, sizeof(result->trace_id), "%s", trace_id);
        snprintf(result->session_id, sizeof(result->session_id), "%s", s_session_id);
    }

    liot_rtos_free(frame_buf);
    return 0;

fail:
    liot_rtos_free(frame_buf);
    Liot_SSLSocketClose(AI_WS_SSL_CLIENT_ID);
    ai_ws_rx_deinit();
    return ret;
}

int ai_ws_client_send_text(const char *text)
{
    if (text == NULL || !s_rx.connected) {
        return -1;
    }
    return ai_ws_send_frame(AI_WS_OPCODE_TEXT, text, (int)strlen(text));
}

int ai_ws_client_send_binary(const uint8_t *data, int len)
{
    if (data == NULL || len <= 0 || !s_rx.connected) {
        return -1;
    }
    return ai_ws_send_frame(AI_WS_OPCODE_BINARY, data, len);
}

int ai_ws_client_recv_frame(uint8_t *buf, int buf_size, int *opcode,
                            int timeout_ms)
{
    uint8_t hdr[2] = {0};
    int payload_len = 0;
    int n = 0;

    if (buf == NULL || buf_size <= 0 || opcode == NULL) {
        return -1;
    }

    n = ai_ws_read(hdr, 2, timeout_ms);
    if (n < 2) {
        return -1;
    }

    *opcode = hdr[0] & 0x0F;
    payload_len = hdr[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2] = {0};
        n = ai_ws_read(ext, 2, timeout_ms);
        if (n < 2) {
            return -2;
        }
        payload_len = ((int)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8] = {0};
        n = ai_ws_read(ext, 8, timeout_ms);
        if (n < 8) {
            return -3;
        }
        payload_len = ((int)ext[4] << 24) | ((int)ext[5] << 16) |
                      ((int)ext[6] << 8) | ext[7];
    }

    if (payload_len >= buf_size) {
        liot_trace("ai_ws frame too large len=%d buf=%d", payload_len, buf_size);
        return -4;
    }

    n = ai_ws_read(buf, payload_len, timeout_ms);
    if (n < payload_len) {
        return -5;
    }

    /* Auto-reply to ping with pong */
    if (*opcode == AI_WS_OPCODE_PING) {
        ai_ws_send_frame(AI_WS_OPCODE_PONG, buf, payload_len);
    }

    return payload_len;
}

void ai_ws_client_close(void)
{
    if (s_rx.connected) {
        ai_ws_send_frame(AI_WS_OPCODE_CLOSE, NULL, 0);
        liot_rtos_task_sleep_ms(100);
    }
    s_rx.connected = false;
    Liot_SSLSocketClose(AI_WS_SSL_CLIENT_ID);
    ai_ws_rx_deinit();
    s_session_id[0] = '\0';
}

bool ai_ws_client_is_connected(void)
{
    if (!s_rx.connected) {
        return false;
    }
    int status = Liot_SSLSocketGetStatus(AI_WS_SSL_CLIENT_ID);
    if (status != LIOT_SSL_CLIENT_STATUS_CONNECTED) {
        s_rx.connected = false;
        return false;
    }
    return true;
}

void ai_ws_client_flush_rx(void)
{
    s_rx.read_pos = 0;
    s_rx.write_pos = 0;
}

bool ai_ws_client_has_data(void)
{
    return (s_rx.write_pos > s_rx.read_pos);
}
