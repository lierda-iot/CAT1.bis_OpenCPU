#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "liot_type.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "lwip/sockets.h"
#include "libwebsockets.h"

#include "liot_os.h"
#include "liot_dev.h"
#include "aiLog.h"

#include "ws_ai.h"
#include "coze_ai.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"

#define WS_AI_TASK_STACK_SIZE       (75 * 1024)
#define WS_AI_TASK_PRIORITY         (5U)
#define WS_AI_WRITE_QUEUE_COUNT     (3U)

#define WS_AI_HOST                  "ws.coze.cn"
#define WS_AI_PORT                  (80)
#define WS_AI_PATH_PREFIX           "/v1/chat?bot_id="

#define WS_RECONNECT_BASE_MS        (1000U)
#define WS_MAX_RECONNECT_ATTEMPTS   (6U)
#define WS_SEND_SYNC_TIMEOUT_MS     (5000U)
#define WS_CONNECT_DONE_TIMEOUT_MS  (30000U)
#define WS_AUDIO_TRANSCRIPT_TIMEOUT (5000U)

#define WS_CONNECTED_BIT            (1U << 0)
#define WS_DISCONNECTED_BIT         (1U << 1)
#define WS_ERROR_BIT                (1U << 2)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef struct {
    uint8_t *data;
    uint32_t len;
    enum lws_write_protocol protocol;
    bool sync;
} ws_write_msg_t;

typedef struct {
    struct lws_context *context;
    struct lws *wsi;
    uint32_t events;
    uint32_t reconnect_attempt;
    bool is_running;
    bool is_connected;
    bool force_exit;
    char server_address[64];
    int server_port;
    int ssl_connection;
    char server_path[128];
} ws_client_state_t;

static ws_client_state_t g_ws = {0};
static ws_ai_cfg_t g_ws_cfg = {0};

static liot_task_t g_ws_task_ref = NULL;
static QueueHandle_t g_ws_write_queue = NULL;
static liot_sem_t g_ws_write_sem = NULL;
static liot_sem_t g_ws_connect_sem = NULL;
static liot_sem_t g_ws_transcript_sem = NULL;

static int ws_ai_callback(struct lws *wsi, enum lws_callback_reasons reason,
                          void *user, void *in, size_t len);
static void ws_post_framework_event(eventId_E evtId);

static struct lws_protocols g_protocols[] = {
    {
        .name = "coze-protocol",
        .callback = ws_ai_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    {NULL, NULL, 0, 0}
};

void ws_ai_transcript_release(void)
{
    if (g_ws_transcript_sem) {
        liot_rtos_semaphore_release(g_ws_transcript_sem);
    }
}

void ws_ai_connect_done_release(void)
{
    if (g_ws_connect_sem) {
        liot_rtos_semaphore_release(g_ws_connect_sem);
    }
}

static int ws_ai_callback(struct lws *wsi, enum lws_callback_reasons reason,
                          void *user, void *in, size_t len)
{
    ws_write_msg_t msg = {0};
    (void)user;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        g_ws.events |= WS_CONNECTED_BIT;
        lws_callback_on_writable(wsi);
        ws_post_framework_event(EVT_AI_CONNECTED);
        LOG_INFO("WS connected");
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        g_ws.events |= WS_DISCONNECTED_BIT;
        LOG_INFO("WS closed");
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        g_ws.events |= WS_ERROR_BIT;
        LOG_ERROR("WS connection error: %s", in ? (char *)in : "unknown");
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        coze_ai_recv_message((uint8_t *)in, (uint32_t)len);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (xQueueReceive(g_ws_write_queue, &msg, 0) == pdPASS) {
            if (msg.data != NULL) {
                lws_write(wsi, msg.data + LWS_PRE, msg.len, msg.protocol);
                liot_rtos_free(msg.data);
            }
            if (msg.sync && g_ws_write_sem) {
                liot_rtos_semaphore_release(g_ws_write_sem);
            }
        }
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        unsigned char **p = (unsigned char **)in;
        unsigned char *end = (*p) + len;
        char auth[200] = {0};
        snprintf(auth, sizeof(auth), "Bearer %s", g_ws_cfg.token);
        int ret = lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_AUTHORIZATION,
                                               (unsigned char *)auth, (int)strlen(auth), p, end);
        if (ret)
            LOG_WARN("add auth header failed: %d", ret);
        break;
    }

    default:
        break;
    }
    return 0;
}

static bool ws_do_connect(void)
{
    if (g_ws.context) {
        lws_context_destroy(g_ws.context);
        g_ws.context = NULL;
    }

    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = g_protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.pt_serv_buf_size = 12 * 1024;
    info.ka_time = 10;
    info.ka_interval = 10;
    info.ka_probes = 3;

    g_ws.context = lws_create_context(&info);
    if (g_ws.context == NULL) {
        LOG_ERROR("lws_create_context failed");
        return false;
    }

    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = g_ws.context;
    ccinfo.address = g_ws.server_address;
    ccinfo.port = g_ws.server_port;
    ccinfo.path = g_ws.server_path;
    ccinfo.protocol = g_protocols[0].name;
    ccinfo.host = g_ws.server_address;
    ccinfo.ssl_connection = g_ws.ssl_connection;

    g_ws.wsi = lws_client_connect_via_info(&ccinfo);
    if (!g_ws.wsi) {
        LOG_ERROR("lws_client_connect_via_info failed");
        lws_context_destroy(g_ws.context);
        g_ws.context = NULL;
        return false;
    }

    g_ws.events = 0;
    LOG_INFO("WS connecting to %s:%d", g_ws.server_address, g_ws.server_port);
    return true;
}

static void ws_post_framework_event(eventId_E evtId)
{
    event_t evt = {
        .eventId = evtId,
        .arg1 = 0U,
        .arg2 = 0U,
        .data = NULL,
        .ownerJobId = 0U,
    };
    frameworkPostEvent(&evt);
}

static void ws_ai_task(void *arg)
{
    uint32_t retry_delay = 0;
    (void)arg;

    g_ws_write_queue = xQueueCreate(WS_AI_WRITE_QUEUE_COUNT, sizeof(ws_write_msg_t));
    if (g_ws_write_queue == NULL) {
        LOG_ERROR("Failed to create write queue");
        goto exit;
    }

    memset(&g_ws, 0, sizeof(g_ws));
    strncpy(g_ws.server_address, WS_AI_HOST, sizeof(g_ws.server_address) - 1);
    g_ws.server_port = WS_AI_PORT;
    g_ws.ssl_connection = (WS_AI_PORT == 443) ? 1 : 0;
    snprintf(g_ws.server_path, sizeof(g_ws.server_path), "%s%s", WS_AI_PATH_PREFIX, g_ws_cfg.botid);

    g_ws.is_running = true;
    g_ws.reconnect_attempt = 0;

    while (g_ws.is_running) {
        if (g_ws.force_exit) {
            break;
        }

        if (!g_ws.context) {
            if (!ws_do_connect()) {
                retry_delay = WS_RECONNECT_BASE_MS * (1U << MIN(g_ws.reconnect_attempt, 6U));
                g_ws.reconnect_attempt++;
                LOG_WARN("WS connect failed, retry %u after %u ms", g_ws.reconnect_attempt, retry_delay);

                if (g_ws.reconnect_attempt > WS_MAX_RECONNECT_ATTEMPTS) {
                    ws_ai_connect_done_release();
                    ws_post_framework_event(EVT_NETWORK_FAIL);
                    continue;
                }
                liot_rtos_task_sleep_ms(retry_delay);
                continue;
            }
        }

        lws_service(g_ws.context, 10);
        liot_rtos_task_sleep_ms(1);

        if (g_ws.events & WS_CONNECTED_BIT) {
            g_ws.events &= ~WS_CONNECTED_BIT;
            g_ws.is_connected = true;
            g_ws.reconnect_attempt = 0;

            coze_ai_chat_update(g_ws_cfg.imei, g_ws_cfg.voiceid);
        }

        if (g_ws.events & (WS_DISCONNECTED_BIT | WS_ERROR_BIT)) {
            g_ws.events &= ~(WS_DISCONNECTED_BIT | WS_ERROR_BIT);
            g_ws.is_connected = false;

            retry_delay = WS_RECONNECT_BASE_MS * (1U << MIN(g_ws.reconnect_attempt, 6U));
            g_ws.reconnect_attempt++;
            LOG_WARN("WS disconnected, retry %u after %u ms", g_ws.reconnect_attempt, retry_delay);

            if (g_ws.reconnect_attempt > WS_MAX_RECONNECT_ATTEMPTS) {
                ws_ai_connect_done_release();
                ws_post_framework_event(EVT_AI_DISCONNECTED);
                continue;
            }

            liot_rtos_task_sleep_ms(retry_delay);
            if (g_ws.context) {
                lws_context_destroy(g_ws.context);
                g_ws.context = NULL;
            }
        }
    }

exit:
    if (g_ws.context) {
        lws_context_destroy(g_ws.context);
        g_ws.context = NULL;
    }

    if (g_ws_write_queue) {
        vQueueDelete(g_ws_write_queue);
        g_ws_write_queue = NULL;
    }

    g_ws.is_running = false;
    g_ws_task_ref = NULL;
    LOG_INFO("WS task exit");
    liot_rtos_task_delete(NULL);
}

int ws_ai_send_raw(uint8_t *data, uint32_t len, uint8_t protocol, bool sync)
{
    if (!g_ws.is_running || g_ws_write_queue == NULL) {
        return WS_AI_DISCONNECT;
    }

    ws_write_msg_t msg = {0};
    msg.data = liot_rtos_malloc(len + LWS_PRE);
    if (msg.data == NULL) {
        return WS_AI_NO_MEMORY;
    }
    memcpy(msg.data + LWS_PRE, data, len);
    msg.len = len;
    msg.protocol = (enum lws_write_protocol)protocol;
    msg.sync = sync;

    if (xQueueSend(g_ws_write_queue, &msg, 0) != pdPASS) {
        liot_rtos_free(msg.data);
        return WS_AI_ERROR;
    }

    if (sync && g_ws_write_sem) {
        if (liot_rtos_semaphore_wait(g_ws_write_sem, WS_SEND_SYNC_TIMEOUT_MS) != 0) {
            return WS_AI_TIMEOUT;
        }
    }

    return WS_AI_OK;
}

int ws_ai_init(const ws_ai_cfg_t *cfg)
{
    if (cfg == NULL) {
        return WS_AI_ERROR;
    }

    memcpy(&g_ws_cfg, cfg, sizeof(g_ws_cfg));

    LOG_WARN("[ws_ai_init] botid=%s, imei=%s", g_ws_cfg.botid, g_ws_cfg.imei);

    if (g_ws_write_sem == NULL) {
        liot_rtos_semaphore_create(&g_ws_write_sem, 0);
    }
    if (g_ws_connect_sem == NULL) {
        liot_rtos_semaphore_create(&g_ws_connect_sem, 0);
    }
    if (g_ws_transcript_sem == NULL) {
        liot_rtos_semaphore_create(&g_ws_transcript_sem, 0);
    }

    liot_rtos_task_create(&g_ws_task_ref, WS_AI_TASK_STACK_SIZE, WS_AI_TASK_PRIORITY,
                          "ws_ai", ws_ai_task, NULL);

    return WS_AI_OK;
}

void ws_ai_deinit(void)
{
    if (!g_ws.is_running) {
        return;
    }

    g_ws.force_exit = true;

    while (g_ws.is_running) {
        liot_rtos_task_sleep_ms(100);
    }

    if (g_ws_write_sem) {
        liot_rtos_semaphore_delete(g_ws_write_sem);
        g_ws_write_sem = NULL;
    }
    if (g_ws_connect_sem) {
        liot_rtos_semaphore_delete(g_ws_connect_sem);
        g_ws_connect_sem = NULL;
    }
    if (g_ws_transcript_sem) {
        liot_rtos_semaphore_delete(g_ws_transcript_sem);
        g_ws_transcript_sem = NULL;
    }
}

int ws_ai_connect(const ws_ai_cfg_t *cfg)
{
    if (cfg == NULL) {
        return WS_AI_ERROR;
    }

    int ret = coze_ai_chat_update(cfg->imei, cfg->voiceid);
    if (ret != 0) {
        return WS_AI_ERROR;
    }

    liot_rtos_semaphore_wait(g_ws_connect_sem, WS_CONNECT_DONE_TIMEOUT_MS);
    return WS_AI_OK;
}

int ws_ai_disconnect(void)
{
    ws_ai_deinit();
    return WS_AI_OK;
}

int ws_ai_send_audio(uint8_t *data, uint32_t len)
{
    if (!g_ws.is_connected) {
        return WS_AI_DISCONNECT;
    }
    return coze_ai_chat_upload_audio(g_ws_cfg.imei, data, len);
}

int ws_ai_send_audio_complete(const char *imei)
{
    if (!g_ws.is_connected) {
        return WS_AI_DISCONNECT;
    }
    return coze_ai_chat_upload_complete(imei);
}

int ws_ai_recv_play(const char *imei)
{
    if (!g_ws.is_connected) {
        return WS_AI_DISCONNECT;
    }

    int ret = coze_ai_chat_upload_complete(imei);
    if (ret != 0) {
        return ret;
    }

    if (g_ws_transcript_sem) {
        if (liot_rtos_semaphore_wait(g_ws_transcript_sem, WS_AUDIO_TRANSCRIPT_TIMEOUT) != 0) {
            return WS_AI_TIMEOUT;
        }
    }
    return WS_AI_OK;
}

int ws_ai_cancel(const char *imei)
{
    if (!g_ws.is_connected) {
        return WS_AI_DISCONNECT;
    }
    return coze_ai_chat_cancel(imei);
}

bool ws_ai_is_connected(void)
{
    return g_ws.is_connected;
}
