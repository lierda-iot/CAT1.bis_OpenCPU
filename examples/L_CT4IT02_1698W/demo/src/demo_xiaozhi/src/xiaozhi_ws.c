#include "xiaozhi_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_ssl2.h"

#define XZ_WS_SSL_ID             2
#define XZ_WS_RX_SIZE            (16 * 1024)
#define XZ_WS_FRAME_SIZE         (8 * 1024)
#define XZ_WS_TX_PACKET_SIZE     520
#define XZ_WS_CONNECT_TIMEOUT_MS 30000
#define XZ_WS_IDLE_PING_MS        20000
#define XZ_VOICE_RUNTIME 1
#define XZ_TALK_COOLDOWN_MS 800
#define XZ_TALK_WAIT_TTS_TIMEOUT_MS 8000
#define XZ_TALK_TASK_STACK_SIZE (32 * 1024)

typedef enum {
    XZ_TALK_IDLE = 0,
    XZ_TALK_RECORDING,
    XZ_TALK_WAIT_TTS,
    XZ_TALK_COOLDOWN
} xz_talk_state_t;

typedef struct {
    uint8_t data[XZ_WS_RX_SIZE];
    volatile int read_pos;
    volatile int write_pos;
    volatile bool overflow;
    liot_sem_t sem;
} xz_ws_rx_t;

static xz_ws_rx_t g_ws_rx;
static Liot_sslClientInfo_t g_ws_ssl_info;
static Liot_sslContext_t g_ws_ssl_ctx;
static uint8_t g_ws_frame[XZ_WS_FRAME_SIZE];
static uint8_t g_ws_tx_packet[XZ_WS_TX_PACKET_SIZE] __attribute__((aligned(4)));
static liot_mutex_t g_ws_send_lock;
static liot_task_t g_ws_talk_task;
static volatile bool g_ws_connected;
static volatile bool g_ws_protocol_ready;
static volatile bool g_ws_talk_pressed;
static volatile xz_talk_state_t g_ws_talk_state;
static volatile uint32_t g_ws_talk_ready_tick;
static volatile uint32_t g_ws_talk_wait_tick;
static char g_ws_session[96];

static void xz_ws_recv_cb(uint8_t client_id, void *data, uint32_t len, void *arg)
{
    int unread;
    int space;
    (void)client_id;
    (void)arg;
    if (data == NULL || len == 0) return;

    unread = g_ws_rx.write_pos - g_ws_rx.read_pos;
    if (g_ws_rx.write_pos + (int)len > XZ_WS_RX_SIZE && g_ws_rx.read_pos > 0) {
        if (unread > 0)
            memmove(g_ws_rx.data, g_ws_rx.data + g_ws_rx.read_pos, unread);
        g_ws_rx.read_pos = 0;
        g_ws_rx.write_pos = unread;
    }
    space = XZ_WS_RX_SIZE - g_ws_rx.write_pos;
    if ((int)len > space) {
        g_ws_rx.overflow = true;
        liot_trace("[xiaozhi] WS RX overflow incoming=%u space=%d", (unsigned)len, space);
        return;
    }
    memcpy(g_ws_rx.data + g_ws_rx.write_pos, data, len);
    g_ws_rx.write_pos += (int)len;
    if (g_ws_rx.sem != NULL) liot_rtos_semaphore_release(g_ws_rx.sem);
}

static int xz_ws_read(void *output, int length, int timeout_ms)
{
    uint8_t *out = (uint8_t *)output;
    int copied = 0;
    int waited = 0;
    while (copied < length) {
        int available = g_ws_rx.write_pos - g_ws_rx.read_pos;
        if (available > 0) {
            int count = length - copied;
            if (count > available) count = available;
            memcpy(out + copied, g_ws_rx.data + g_ws_rx.read_pos, count);
            g_ws_rx.read_pos += count;
            copied += count;
            if (g_ws_rx.read_pos == g_ws_rx.write_pos) {
                g_ws_rx.read_pos = 0;
                g_ws_rx.write_pos = 0;
            }
            continue;
        }
        if (waited >= timeout_ms) break;
        liot_rtos_semaphore_wait(g_ws_rx.sem, 250);
        waited += 250;
    }
    return copied;
}

static int xz_ws_parse_url(const char *url, char *host, int host_size,
                           char *path, int path_size, int *port)
{
    const char *start;
    const char *slash;
    const char *colon;
    int host_len;
    if (url == NULL || strncmp(url, "wss://", 6) != 0) return -1;
    start = url + 6;
    slash = strchr(start, '/');
    if (slash == NULL) slash = start + strlen(start);
    colon = strchr(start, ':');
    if (colon != NULL && colon < slash) {
        host_len = (int)(colon - start);
        *port = atoi(colon + 1);
    } else {
        host_len = (int)(slash - start);
        *port = 443;
    }
    if (host_len <= 0 || host_len >= host_size) return -2;
    memcpy(host, start, host_len);
    host[host_len] = '\0';
    snprintf(path, path_size, "%s", *slash != '\0' ? slash : "/");
    return 0;
}

static int xz_ws_ssl_connect(const char *host, int port)
{
    int ret;
    int status = 0;
    int waited;
    memset(&g_ws_ssl_info, 0, sizeof(g_ws_ssl_info));
    memset(&g_ws_ssl_ctx, 0, sizeof(g_ws_ssl_ctx));
    g_ws_ssl_ctx.auth_type = LIOT_SSL_VERIFY_NONE;
    g_ws_ssl_ctx.ssl_version = LIOT_SSL_VERSION_3;
    g_ws_ssl_ctx.sni_support = 1;
    g_ws_ssl_ctx.ciphersuite[0] = 0xFFFF;
    g_ws_ssl_ctx.r_timeout = 30;
    g_ws_ssl_ctx.s_timeout = 30;
    g_ws_ssl_info.sslClientId = XZ_WS_SSL_ID;
    g_ws_ssl_info.socket_type = 1;
    g_ws_ssl_info.host = (uint8_t *)host;
    g_ws_ssl_info.port = (uint16_t)port;
    g_ws_ssl_info.ssl_context = &g_ws_ssl_ctx;
    g_ws_ssl_info.sslClient_cb = xz_ws_recv_cb;
    ret = Liot_SSLSetCfg(&g_ws_ssl_info);
    liot_trace("[xiaozhi] WS SSL config ret=%d host=%s port=%d", ret, host, port);
    if (ret != LIOT_SSL_SUCCESS) return -1;
    ret = Liot_SSLSocketOpen(XZ_WS_SSL_ID);
    liot_trace("[xiaozhi] WS SSL open ret=%d", ret);
    if (ret != LIOT_SSL_SUCCESS) return -2;
    for (waited = 0; waited < XZ_WS_CONNECT_TIMEOUT_MS; waited += 200) {
        status = Liot_SSLSocketGetStatus(XZ_WS_SSL_ID);
        if (status == LIOT_SSL_CLIENT_STATUS_CONNECTED) return 0;
        if (status == LIOT_SSL_CLIENT_STATUS_CLOSED ||
            status == LIOT_SSL_CLIENT_STATUS_DISCONNECTED) return -3;
        liot_rtos_task_sleep_ms(200);
    }
    liot_trace("[xiaozhi] WS SSL connect timeout status=%d", status);
    return -4;
}

static int xz_ws_send_frame(int opcode, const void *payload, int length)
{
    uint8_t header[8];
    uint8_t mask[4];
    uint8_t *packet = g_ws_tx_packet;
    uint32_t random;
    int header_len;
    int i;
    int ret;
    if (length < 0 || length > 512) return -1;
    if (g_ws_send_lock != NULL)
        liot_rtos_mutex_lock(g_ws_send_lock, LIOT_WAIT_FOREVER);
    header[0] = (uint8_t)(0x80 | (opcode & 0x0f));
    if (length < 126) {
        header[1] = (uint8_t)(0x80 | length);
        header_len = 2;
    } else {
        header[1] = 0x80 | 126;
        header[2] = (uint8_t)(length >> 8);
        header[3] = (uint8_t)length;
        header_len = 4;
    }
    random = liot_rtos_rand();
    mask[0] = (uint8_t)random;
    mask[1] = (uint8_t)(random >> 8);
    mask[2] = (uint8_t)(random >> 16);
    mask[3] = (uint8_t)(random >> 24);
    memcpy(header + header_len, mask, 4);
    header_len += 4;
    if (header_len + length > sizeof(g_ws_tx_packet)) {
        if (g_ws_send_lock != NULL) liot_rtos_mutex_unlock(g_ws_send_lock);
        return -2;
    }
    memcpy(packet, header, header_len);
    for (i = 0; i < length; ++i)
        packet[header_len + i] = ((const uint8_t *)payload)[i] ^ mask[i & 3];
    ret = Liot_SSLSocketSend(XZ_WS_SSL_ID, packet, (uint16_t)(header_len + length));
    if (g_ws_send_lock != NULL) liot_rtos_mutex_unlock(g_ws_send_lock);
    return ret < 0 ? -3 : 0;
}

static int xz_ws_recv_frame(uint8_t *payload, int capacity, int *opcode, int timeout_ms)
{
    uint8_t head[2];
    uint8_t extra[8];
    uint8_t mask[4];
    int masked;
    int length;
    int i;
    if (xz_ws_read(head, 2, timeout_ms) != 2) return -1;
    *opcode = head[0] & 0x0f;
    masked = (head[1] & 0x80) != 0;
    length = head[1] & 0x7f;
    if (length == 126) {
        if (xz_ws_read(extra, 2, timeout_ms) != 2) return -2;
        length = ((int)extra[0] << 8) | extra[1];
    } else if (length == 127) {
        if (xz_ws_read(extra, 8, timeout_ms) != 8) return -3;
        if (extra[0] || extra[1] || extra[2] || extra[3] || extra[4] || extra[5])
            return -4;
        length = ((int)extra[6] << 8) | extra[7];
    }
    if (masked && xz_ws_read(mask, 4, timeout_ms) != 4) return -5;
    if (length >= capacity) {
        liot_trace("[xiaozhi] WS frame too large len=%d capacity=%d", length, capacity);
        return -6;
    }
    if (xz_ws_read(payload, length, timeout_ms) != length) return -7;
    if (masked)
        for (i = 0; i < length; ++i) payload[i] ^= mask[i & 3];
    return length;
}

static int xz_ws_upgrade(const xiaozhi_config_t *config, const char *host,
                         const char *path)
{
    char request[1024];
    char response[1024];
    const char *token_prefix = strchr(config->websocket_token, ' ') ? "" : "Bearer ";
    int length;
    int used = 0;
    length = snprintf(request, sizeof(request),
                      "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\n"
                      "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\nAuthorization: %s%s\r\n"
                      "Protocol-Version: %d\r\nDevice-Id: %s\r\nClient-Id: %s\r\n\r\n",
                      path, host, token_prefix, config->websocket_token,
                      config->websocket_version > 0 ? config->websocket_version : 1,
                      config->device_id, config->client_id);
    if (length <= 0 || length >= (int)sizeof(request)) return -1;
    if (Liot_SSLSocketSend(XZ_WS_SSL_ID, (uint8_t *)request, (uint16_t)length) < 0)
        return -2;
    while (used < (int)sizeof(response) - 1) {
        if (xz_ws_read(response + used, 1, 30000) != 1) return -3;
        ++used;
        if (used >= 4 && memcmp(response + used - 4, "\r\n\r\n", 4) == 0) break;
    }
    response[used] = '\0';
    liot_trace("[xiaozhi] WS upgrade response=%.*s", 80, response);
    return strstr(response, " 101 ") != NULL ? 0 : -4;
}

static int xz_ws_check_server_hello(uint8_t *data, int length, char *session, int session_size)
{
    cJSON *root;
    cJSON *type;
    cJSON *transport;
    cJSON *session_id;
    cJSON *audio;
    cJSON *rate;
    cJSON *duration;
    int ok = 0;
    data[length] = '\0';
    root = cJSON_Parse((char *)data);
    if (root == NULL) return -1;
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    transport = cJSON_GetObjectItemCaseSensitive(root, "transport");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "hello") == 0 &&
        cJSON_IsString(transport) && strcmp(transport->valuestring, "websocket") == 0) {
        session_id = cJSON_GetObjectItemCaseSensitive(root, "session_id");
        if (cJSON_IsString(session_id))
            snprintf(session, session_size, "%s", session_id->valuestring);
        audio = cJSON_GetObjectItemCaseSensitive(root, "audio_params");
        rate = cJSON_IsObject(audio) ? cJSON_GetObjectItemCaseSensitive(audio, "sample_rate") : NULL;
        duration = cJSON_IsObject(audio) ? cJSON_GetObjectItemCaseSensitive(audio, "frame_duration") : NULL;
        liot_trace("[xiaozhi] WS server hello session=%s rate=%d frame_ms=%d",
                   session, cJSON_IsNumber(rate) ? rate->valueint : 0,
                   cJSON_IsNumber(duration) ? duration->valueint : 0);
        ok = 1;
    }
    cJSON_Delete(root);
    return ok ? 0 : -2;
}

static void xz_ws_handle_mcp(uint8_t *data, int length, const char *session)
{
    cJSON *root;
    cJSON *type;
    cJSON *payload;
    cJSON *method;
    cJSON *id;
    char reply[512];
    int reply_len;
    data[length] = '\0';
    root = cJSON_Parse((char *)data);
    if (root == NULL) return;
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    method = cJSON_IsObject(payload) ?
        cJSON_GetObjectItemCaseSensitive(payload, "method") : NULL;
    id = cJSON_IsObject(payload) ?
        cJSON_GetObjectItemCaseSensitive(payload, "id") : NULL;
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "mcp") != 0 ||
        !cJSON_IsString(method) || !cJSON_IsNumber(id)) {
        cJSON_Delete(root);
        return;
    }
    if (strcmp(method->valuestring, "initialize") == 0) {
        reply_len = snprintf(reply, sizeof(reply),
            "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":{"
            "\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{"
            "\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
            "\"serverInfo\":{\"name\":\"L_CT4IT02_1698W\",\"version\":\"0.1\"}}}}",
            session, id->valueint);
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        reply_len = snprintf(reply, sizeof(reply),
            "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":{"
            "\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"tools\":[]}}}",
            session, id->valueint);
    } else {
        reply_len = snprintf(reply, sizeof(reply),
            "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":{"
            "\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{"
            "\"code\":-32601,\"message\":\"Method not implemented\"}}}",
            session, id->valueint);
    }
    if (reply_len > 0 && reply_len < (int)sizeof(reply)) {
        int send_ret = xz_ws_send_frame(1, reply, reply_len);
        liot_trace("[xiaozhi] WS MCP reply method=%s id=%d ret=%d",
                   method->valuestring, id->valueint, send_ret);
        if (strcmp(method->valuestring, "tools/list") == 0 && send_ret == 0) {
            g_ws_protocol_ready = true;
            liot_trace("[xiaozhi] protocol ready; talk enabled");
        }
    }
    cJSON_Delete(root);
}

#if XZ_VOICE_RUNTIME
static void xz_ws_handle_event(uint8_t *data, int length, const char *session)
{
    cJSON *root;
    cJSON *type;
    cJSON *state;
    cJSON *text;
    data[length] = '\0';
    root = cJSON_Parse((char *)data);
    if (root == NULL) return;
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type)) {
        cJSON_Delete(root);
        return;
    }
    if (strcmp(type->valuestring, "mcp") == 0) {
        cJSON_Delete(root);
        xz_ws_handle_mcp(data, length, session);
        return;
    }
    if (strcmp(type->valuestring, "stt") == 0) {
        text = cJSON_GetObjectItemCaseSensitive(root, "text");
        if (cJSON_IsString(text)) {
            liot_trace("[xiaozhi] STT text=%s", text->valuestring);
            xiaozhi_ui_status(text->valuestring);
        }
    } else if (strcmp(type->valuestring, "tts") == 0) {
        state = cJSON_GetObjectItemCaseSensitive(root, "state");
        text = cJSON_GetObjectItemCaseSensitive(root, "text");
        if (cJSON_IsString(state) && strcmp(state->valuestring, "start") == 0) {
            g_ws_talk_state = XZ_TALK_WAIT_TTS;
            xiaozhi_audio_tts_reset();
            xiaozhi_ui_status("Speaking...");
        } else if (cJSON_IsString(state) && strcmp(state->valuestring, "stop") == 0) {
            if (g_ws_talk_state == XZ_TALK_RECORDING) {
                liot_trace("[xiaozhi] ignore stale TTS stop during recording");
                cJSON_Delete(root);
                return;
            }
            int play_ret = xiaozhi_audio_tts_play();
            liot_trace("[xiaozhi] TTS stop play_ret=%d", play_ret);
            g_ws_talk_pressed = false;
            g_ws_talk_ready_tick = liot_rtos_get_system_tick() + XZ_TALK_COOLDOWN_MS;
            g_ws_talk_state = XZ_TALK_COOLDOWN;
            liot_trace("[xiaozhi] talk state COOLDOWN %dms", XZ_TALK_COOLDOWN_MS);
            xiaozhi_ui_status("Hold to talk");
        } else if (cJSON_IsString(text)) {
            liot_trace("[xiaozhi] TTS text=%s", text->valuestring);
            xiaozhi_ui_status(text->valuestring);
        }
    }
    cJSON_Delete(root);
}
#endif

#if XZ_VOICE_RUNTIME
static void xz_ws_talk_thread(void *argument)
{
    uint8_t opus_data[512];
    char message[192];
    int length;
    int frames;

    (void)argument;
    for (;;) {

        if (g_ws_talk_state == XZ_TALK_COOLDOWN &&
            (int32_t)(liot_rtos_get_system_tick() - g_ws_talk_ready_tick) >= 0) {
            g_ws_talk_state = XZ_TALK_IDLE;
            liot_trace("[xiaozhi] talk state IDLE");
        }

        if (!g_ws_talk_pressed || !g_ws_connected || g_ws_session[0] == '\0' ||
            g_ws_talk_state != XZ_TALK_RECORDING) {
            liot_rtos_task_sleep_ms(20);
            continue;
        }
        length = snprintf(message, sizeof(message),
                          "{\"session_id\":\"%s\",\"type\":\"listen\","
                          "\"state\":\"start\",\"mode\":\"manual\"}",
                          g_ws_session);
        if (xz_ws_send_frame(1, message, length) != 0) continue;
        liot_trace("[xiaozhi] listen start session=%s", g_ws_session);
        xiaozhi_audio_capture_session_begin();

        frames = 0;
        while (g_ws_talk_pressed && g_ws_connected) {
            length = xiaozhi_audio_capture_opus(opus_data, sizeof(opus_data));
            if (length <= 0) {
                liot_trace("[xiaozhi] record/encode failed ret=%d", length);
                break;
            }
            if (xz_ws_send_frame(2, opus_data, length) != 0) break;
            ++frames;
            /* AudioRecord may return immediately on this codec; yield so the
             * modem, LVGL and watchdog service tasks continue to run. */
            liot_rtos_task_sleep_ms(2);
            if ((frames % 20) == 0)
                liot_trace("[xiaozhi] uplink Opus frames=%d last_bytes=%d", frames, length);
        }
        xiaozhi_audio_capture_session_end();
        while (g_ws_connected) {
            length = xiaozhi_audio_capture_flush_opus(opus_data, sizeof(opus_data));
            if (length <= 0) break;
            if (xz_ws_send_frame(2, opus_data, length) != 0) break;
            ++frames;
        }
        if (g_ws_connected) {
            int stop_ret;
            length = snprintf(message, sizeof(message),
                              "{\"session_id\":\"%s\",\"type\":\"listen\","
                              "\"state\":\"stop\"}", g_ws_session);
            stop_ret = xz_ws_send_frame(1, message, length);
            liot_trace("[xiaozhi] listen stop send ret=%d", stop_ret);
        }
        liot_trace("[xiaozhi] listen stop frames=%d", frames);
        g_ws_talk_pressed = false;
        if (g_ws_connected) {
            g_ws_talk_wait_tick = liot_rtos_get_system_tick() +
                                  XZ_TALK_WAIT_TTS_TIMEOUT_MS;
            g_ws_talk_state = XZ_TALK_WAIT_TTS;
        }
    }
}

bool xiaozhi_talk_set(bool pressed)
{
    uint32_t now = liot_rtos_get_system_tick();
    bool was_recording = (g_ws_talk_state == XZ_TALK_RECORDING);
    if (pressed) {
        if (g_ws_talk_state == XZ_TALK_COOLDOWN &&
            (int32_t)(now - g_ws_talk_ready_tick) >= 0)
            g_ws_talk_state = XZ_TALK_IDLE;
        if (!g_ws_connected || !g_ws_protocol_ready) {
            liot_trace("[xiaozhi] talk press ignored state=%d connected=%d",
                       (int)g_ws_talk_state, g_ws_connected ? 1 : 0);
            g_ws_talk_pressed = false;
            return false;
        }
        if (g_ws_talk_state == XZ_TALK_WAIT_TTS ||
            g_ws_talk_state == XZ_TALK_COOLDOWN) {
            char abort_msg[160];
            int abort_len = snprintf(abort_msg, sizeof(abort_msg),
                "{\"session_id\":\"%s\",\"type\":\"abort\",\"reason\":\"wake_word_detected\"}",
                g_ws_session);
            int abort_ret = xz_ws_send_frame(1, abort_msg, abort_len);
            liot_trace("[xiaozhi] talk interrupt abort ret=%d state=%d",
                       abort_ret, (int)g_ws_talk_state);
            xiaozhi_audio_tts_abort();
            g_ws_talk_state = XZ_TALK_IDLE;
        }
        if (g_ws_talk_state != XZ_TALK_IDLE) return false;
        g_ws_talk_state = XZ_TALK_RECORDING;
        g_ws_talk_pressed = true;
        liot_trace("[xiaozhi] talk state RECORDING");
    } else {
        g_ws_talk_pressed = false;
        liot_trace("[xiaozhi] talk release state=%d", (int)g_ws_talk_state);
        return was_recording;
    }
    return true;
}

static int xz_ws_runtime_init(void)
{
    int ret;
    if (g_ws_send_lock == NULL &&
        liot_rtos_mutex_create(&g_ws_send_lock) != LIOT_OSI_SUCCESS) {
        liot_trace("[xiaozhi] runtime send mutex create failed");
        return -1;
    }
    if (g_ws_talk_task == NULL) {
        ret = liot_rtos_task_create(&g_ws_talk_task, XZ_TALK_TASK_STACK_SIZE,
                                    LIOT_APP_TASK_PRIORITY,
                                    "xiaozhi_talk", xz_ws_talk_thread, NULL);
        liot_trace("[xiaozhi] runtime talk task create ret=%d handle=%p", ret,
                   g_ws_talk_task);
        if (ret != LIOT_OSI_SUCCESS) return -2;
    }
    liot_trace("[xiaozhi] voice runtime ready");
    return 0;
}
#endif

int xiaozhi_ws_run(const xiaozhi_config_t *config)
{
    char host[128];
    char path[256];
    char session[96] = {0};
    const char hello[] =
        "{\"type\":\"hello\",\"version\":1,\"features\":{\"mcp\":true},"
        "\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\","
        "\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
    int port;
    int ret;
    int opcode;
    int length;
    if (config == NULL || config->websocket_url[0] == '\0') return -1;
#if XZ_VOICE_RUNTIME
    ret = xz_ws_runtime_init();
    if (ret != 0) {
        liot_trace("[xiaozhi] voice runtime init failed ret=%d", ret);
        return -100 + ret;
    }
#endif
    g_ws_connected = false;
    g_ws_talk_pressed = false;
    g_ws_talk_state = XZ_TALK_IDLE;
    g_ws_talk_ready_tick = 0;
    g_ws_session[0] = '\0';
    /* Keep the RX semaphore alive across reconnects. */
    if (g_ws_rx.sem == NULL) {
        memset(&g_ws_rx, 0, sizeof(g_ws_rx));
        if (liot_rtos_semaphore_create(&g_ws_rx.sem, 0) != LIOT_OSI_SUCCESS) return -2;
    } else {
        g_ws_rx.read_pos = 0;
        g_ws_rx.write_pos = 0;
        g_ws_rx.overflow = false;
    }
    ret = xz_ws_parse_url(config->websocket_url, host, sizeof(host), path, sizeof(path), &port);
    if (ret != 0) goto exit;
    liot_trace("[xiaozhi] WS connect url=%s protocol=%d", config->websocket_url,
               config->websocket_version);
    ret = xz_ws_ssl_connect(host, port);
    if (ret != 0) goto exit;
    ret = xz_ws_upgrade(config, host, path);
    if (ret != 0) goto exit;
    ret = xz_ws_send_frame(1, hello, (int)strlen(hello));
    liot_trace("[xiaozhi] WS device hello ret=%d", ret);
    if (ret != 0) goto exit;
    for (;;) {
        length = xz_ws_recv_frame(g_ws_frame, sizeof(g_ws_frame), &opcode,
                                  XZ_WS_IDLE_PING_MS);
        if (length < 0) {
            int ssl_status = Liot_SSLSocketGetStatus(XZ_WS_SSL_ID);
            if (length == -1 &&
                ssl_status == LIOT_SSL_CLIENT_STATUS_CONNECTED) {
                int ping_ret = xz_ws_send_frame(9, "", 0);
                liot_trace("[xiaozhi] WS idle ping ret=%d", ping_ret);
                if (ping_ret == 0) continue;
            }
            liot_trace("[xiaozhi] WS receive failed ret=%d status=%d", length,
                       ssl_status);
            ret = -20;
            break;
        }
        if (opcode == 9) {
            xz_ws_send_frame(10, g_ws_frame, length);
            continue;
        }
        if (opcode == 8) {
            liot_trace("[xiaozhi] WS server closed");
            ret = -21;
            break;
        }
        if (opcode == 1) {
            g_ws_frame[length] = '\0';
            liot_trace("[xiaozhi] WS text=%.*s", length > 512 ? 512 : length, g_ws_frame);
            if (session[0] == '\0') {
                ret = xz_ws_check_server_hello(g_ws_frame, length, session, sizeof(session));
                if (ret != 0) break;
                snprintf(g_ws_session, sizeof(g_ws_session), "%s", session);
                g_ws_connected = true;
                g_ws_talk_state = XZ_TALK_IDLE;
                g_ws_talk_pressed = false;
                liot_trace("[xiaozhi] websocket ready");
                xiaozhi_ui_status("Hold to talk");
            }
#if XZ_VOICE_RUNTIME
            else
                xz_ws_handle_event(g_ws_frame, length, session);
#else
            else if (strstr((char *)g_ws_frame, "\"type\":\"mcp\"") != NULL)
                xz_ws_handle_mcp(g_ws_frame, length, session);
#endif
        } else if (opcode == 2) {
            int decode_ret = xiaozhi_audio_decode_append(g_ws_frame, length);
            liot_trace("[xiaozhi] WS binary bytes=%d decode_ret=%d", length,
                       decode_ret);
        }
    }
exit:
    liot_trace("[xiaozhi] WS cleanup begin ret=%d", ret);
    g_ws_connected = false;
    g_ws_talk_pressed = false;
    g_ws_talk_state = XZ_TALK_IDLE;
    g_ws_session[0] = '\0';
#if XZ_VOICE_RUNTIME
    xiaozhi_ui_status("Reconnecting...");
#endif
    Liot_SSLSocketClose(XZ_WS_SSL_ID);
    liot_trace("[xiaozhi] WS SSL closed");
    /* The semaphore is intentionally retained for the next connection. */
    liot_trace("[xiaozhi] WS cleanup end");
    return ret;
}
