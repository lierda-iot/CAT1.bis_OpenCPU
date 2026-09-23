#include "app_ai_ws_transport_lws.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "libwebsockets.h"

#define APP_AI_WS_LWS_DEFAULT_SERVICE_TIMEOUT_MS 10U
#define APP_AI_WS_LWS_DEFAULT_STACK_BYTES (48U * 1024U)
#define APP_AI_WS_LWS_DEFAULT_PRIORITY 20U
#define APP_AI_WS_LWS_DEFAULT_QUEUE_DEPTH 8U
#define APP_AI_WS_LWS_SERVICE_IDLE_MS 1U
#define APP_AI_WS_LWS_CONNECT_TIMEOUT_SEC 15U

static int lws_transport_connect(void *ctx, const app_ai_ws_config_t *config);
static int lws_transport_disconnect(void *ctx);
static int lws_transport_send_text(void *ctx, const char *text);
static int lws_transport_send_binary(void *ctx, const uint8_t *data, uint32_t len);
static int lws_transport_callback(struct lws *wsi,
                                  enum lws_callback_reasons reason,
                                  void *user,
                                  void *in,
                                  size_t len);

static struct lws_protocols s_lws_protocols[] = {
    {
        .name = "app-ai-ws",
        .callback = lws_transport_callback,
        .per_session_data_size = 0U,
        .rx_buffer_size = 0U,
    },
    { NULL, NULL, 0U, 0U }
};

static const app_ai_ws_transport_ops_t s_lws_transport_ops = {
    .connect = lws_transport_connect,
    .disconnect = lws_transport_disconnect,
    .send_text = lws_transport_send_text,
    .send_binary = lws_transport_send_binary,
};

static void transport_copy_string(char *dst, uint32_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static uint16_t transport_default_port(bool use_ssl)
{
    return use_ssl ? 443U : 80U;
}

static bool transport_is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static int transport_parse_port(const char *begin, const char *end, uint16_t *port)
{
    uint32_t value = 0U;
    const char *cursor = begin;

    if (begin == NULL || end == NULL || port == NULL || begin >= end) {
        return APP_ERR_INVALID_ARG;
    }

    while (cursor < end) {
        if (!transport_is_digit(*cursor)) {
            return APP_ERR_INVALID_ARG;
        }
        value = (value * 10U) + (uint32_t)(*cursor - '0');
        if (value > 65535U) {
            return APP_ERR_INVALID_ARG;
        }
        cursor++;
    }
    if (value == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    *port = (uint16_t)value;
    return APP_OK;
}

static int transport_parse_endpoint(app_ai_ws_lws_transport_t *ctx, const char *endpoint)
{
    const char *host_begin;
    const char *host_end;
    const char *path_begin;
    const char *port_begin;
    size_t host_len;
    size_t path_len;

    if (ctx == NULL || endpoint == NULL || endpoint[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    if (strncmp(endpoint, "wss://", 6U) == 0) {
        ctx->use_ssl = true;
        host_begin = endpoint + 6;
    } else if (strncmp(endpoint, "ws://", 5U) == 0) {
        ctx->use_ssl = false;
        host_begin = endpoint + 5;
    } else {
        return APP_ERR_INVALID_ARG;
    }

    host_end = host_begin;
    while (*host_end != '\0' && *host_end != '/' && *host_end != ':') {
        host_end++;
    }
    if (host_end == host_begin) {
        return APP_ERR_INVALID_ARG;
    }

    ctx->port = transport_default_port(ctx->use_ssl);
    port_begin = NULL;
    if (*host_end == ':') {
        port_begin = host_end + 1;
        path_begin = port_begin;
        while (*path_begin != '\0' && *path_begin != '/') {
            path_begin++;
        }
        if (transport_parse_port(port_begin, path_begin, &ctx->port) != APP_OK) {
            return APP_ERR_INVALID_ARG;
        }
    } else {
        path_begin = host_end;
    }

    host_len = (size_t)(host_end - host_begin);
    if (host_len >= sizeof(ctx->host)) {
        return APP_ERR_NO_MEMORY;
    }
    memcpy(ctx->host, host_begin, host_len);
    ctx->host[host_len] = '\0';

    if (*path_begin == '\0') {
        transport_copy_string(ctx->path, sizeof(ctx->path), "/");
    } else {
        path_len = strlen(path_begin);
        if (path_len >= sizeof(ctx->path)) {
            return APP_ERR_NO_MEMORY;
        }
        memcpy(ctx->path, path_begin, path_len + 1U);
    }

    return APP_OK;
}

static app_ai_ws_lws_transport_t *transport_from_wsi(struct lws *wsi)
{
    struct lws_context *context;

    if (wsi == NULL) {
        return NULL;
    }

    context = lws_get_context(wsi);
    if (context == NULL) {
        return NULL;
    }
    return (app_ai_ws_lws_transport_t *)lws_context_user(context);
}

static int transport_build_auth_header(app_ai_ws_lws_transport_t *ctx,
                                       const app_ai_ws_config_t *config)
{
    int written;

    if (ctx == NULL || config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    ctx->auth_header[0] = '\0';
    if (config->token[0] == '\0') {
        return APP_OK;
    }

    written = snprintf(ctx->auth_header,
                       sizeof(ctx->auth_header),
                       "Bearer %s",
                       config->token);
    if (written < 0 || (uint32_t)written >= sizeof(ctx->auth_header)) {
        ctx->auth_header[0] = '\0';
        return APP_ERR_NO_MEMORY;
    }
    return APP_OK;
}

static void transport_release_send_msg(app_ai_ws_lws_send_msg_t *msg)
{
    if (msg == NULL) {
        return;
    }
    if (msg->buffer != NULL) {
        app_os_free(msg->buffer);
    }
    msg->buffer = NULL;
    msg->len = 0U;
    msg->write_protocol = 0;
}

static int transport_enqueue(app_ai_ws_lws_transport_t *ctx,
                             const uint8_t *data,
                             uint32_t len,
                             int write_protocol)
{
    app_ai_ws_lws_send_msg_t msg;
    uint32_t alloc_len;
    int ret;

    if (ctx == NULL || data == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (!ctx->initialized || ctx->send_queue == NULL || !ctx->running) {
        return APP_ERR_NOT_READY;
    }

    alloc_len = (uint32_t)LWS_PRE + len;
    if (alloc_len < len) {
        return APP_ERR_NO_MEMORY;
    }

    memset(&msg, 0, sizeof(msg));
    msg.buffer = (uint8_t *)app_os_malloc(alloc_len);
    if (msg.buffer == NULL) {
        return APP_ERR_NO_MEMORY;
    }
    memcpy(msg.buffer + LWS_PRE, data, len);
    msg.len = len;
    msg.write_protocol = write_protocol;

    ret = app_os_queue_send(ctx->send_queue, &msg, sizeof(msg), APP_OS_NO_WAIT);
    if (ret != APP_OK) {
        transport_release_send_msg(&msg);
        return ret;
    }

    if (ctx->wsi != NULL) {
        (void)lws_callback_on_writable((struct lws *)ctx->wsi);
    }
    return APP_OK;
}

static void transport_drain_queue(app_ai_ws_lws_transport_t *ctx)
{
    app_ai_ws_lws_send_msg_t msg;

    if (ctx == NULL || ctx->send_queue == NULL) {
        return;
    }

    while (app_os_queue_recv(ctx->send_queue,
                             &msg,
                             sizeof(msg),
                             APP_OS_NO_WAIT) == APP_OK) {
        transport_release_send_msg(&msg);
    }
}

static int transport_handle_receive(app_ai_ws_lws_transport_t *ctx,
                                    struct lws *wsi,
                                    void *in,
                                    size_t len)
{
    char *text;
    int ret;

    if (ctx == NULL || ctx->protocol == NULL || in == NULL || len == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (len > UINT32_MAX) {
        return APP_ERR_NO_MEMORY;
    }
    if (lws_remaining_packet_payload(wsi) != 0U || !lws_is_final_fragment(wsi)) {
        app_log("app_ai_ws fragmented frame is not supported yet");
        return APP_ERR_NOT_SUPPORTED;
    }

    if (lws_frame_is_binary(wsi)) {
        return app_ai_ws_protocol_handle_audio(ctx->protocol, (const uint8_t *)in, (uint32_t)len, 0U);
    }

    text = (char *)app_os_malloc(len + 1U);
    if (text == NULL) {
        return APP_ERR_NO_MEMORY;
    }
    memcpy(text, in, len);
    text[len] = '\0';
    ret = app_ai_ws_protocol_handle_text(ctx->protocol, text);
    app_os_free(text);
    return ret;
}

static int transport_send_next(app_ai_ws_lws_transport_t *ctx, struct lws *wsi)
{
    app_ai_ws_lws_send_msg_t msg;
    uint32_t expected_len;
    int written;

    if (ctx == NULL || wsi == NULL || ctx->send_queue == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(&msg, 0, sizeof(msg));
    if (app_os_queue_recv(ctx->send_queue, &msg, sizeof(msg), APP_OS_NO_WAIT) != APP_OK) {
        return APP_OK;
    }

    if (msg.buffer == NULL || msg.len == 0U) {
        transport_release_send_msg(&msg);
        return APP_ERR_INVALID_ARG;
    }

    expected_len = msg.len;
    written = lws_write(wsi, msg.buffer + LWS_PRE, expected_len, (enum lws_write_protocol)msg.write_protocol);
    transport_release_send_msg(&msg);
    if (written < 0 || (uint32_t)written != expected_len) {
        return APP_ERR_FAIL;
    }

    if (ctx->send_queue != NULL) {
        (void)lws_callback_on_writable(wsi);
    }
    return APP_OK;
}

static int transport_append_auth_header(app_ai_ws_lws_transport_t *ctx,
                                        struct lws *wsi,
                                        void *in,
                                        size_t len)
{
    unsigned char **p;
    unsigned char *end;

    if (ctx == NULL || wsi == NULL || in == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (ctx->auth_header[0] == '\0') {
        return APP_OK;
    }

    p = (unsigned char **)in;
    end = (*p) + len;
    if (lws_add_http_header_by_token(wsi,
                                     WSI_TOKEN_HTTP_AUTHORIZATION,
                                     (unsigned char *)ctx->auth_header,
                                     (int)strlen(ctx->auth_header),
                                     p,
                                     end) != 0) {
        return APP_ERR_NO_MEMORY;
    }
    return APP_OK;
}

static int lws_transport_callback(struct lws *wsi,
                                  enum lws_callback_reasons reason,
                                  void *user,
                                  void *in,
                                  size_t len)
{
    app_ai_ws_lws_transport_t *ctx = transport_from_wsi(wsi);
    int ret;

    (void)user;
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        if (ctx != NULL) {
            ctx->wsi = wsi;
            ctx->connected = true;
            (void)lws_callback_on_writable(wsi);
        }
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
        if (ctx != NULL) {
            ret = transport_handle_receive(ctx, wsi, in, len);
            if (ret != APP_OK && ret != APP_ERR_NOT_SUPPORTED) {
                app_log("app_ai_ws receive error: %d", ret);
                return -1;
            }
        }
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (ctx != NULL) {
            ret = transport_send_next(ctx, wsi);
            if (ret != APP_OK) {
                app_log("app_ai_ws write error: %d", ret);
                return -1;
            }
        }
        break;
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
        if (ctx != NULL) {
            ret = transport_append_auth_header(ctx, wsi, in, len);
            if (ret != APP_OK) {
                app_log("app_ai_ws auth header error: %d", ret);
                return -1;
            }
        }
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        if (ctx != NULL) {
            ctx->connected = false;
            ctx->wsi = NULL;
            app_log("app_ai_ws connection error");
        }
        break;
    case LWS_CALLBACK_CLIENT_CLOSED:
    case LWS_CALLBACK_CLOSED:
        if (ctx != NULL) {
            ctx->connected = false;
            ctx->wsi = NULL;
        }
        break;
    default:
        break;
    }
    return 0;
}

static int transport_create_context(app_ai_ws_lws_transport_t *ctx)
{
    struct lws_context_creation_info info;

    if (ctx == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = s_lws_protocols;
    info.gid = -1;
    info.uid = -1;
    info.user = ctx;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.connect_timeout_secs = APP_AI_WS_LWS_CONNECT_TIMEOUT_SEC;

    ctx->context = lws_create_context(&info);
    return (ctx->context != NULL) ? APP_OK : APP_ERR_FAIL;
}

static int transport_create_connection(app_ai_ws_lws_transport_t *ctx)
{
    struct lws_client_connect_info ccinfo;
    int ssl_flags = 0;

    if (ctx == NULL || ctx->context == NULL || ctx->host[0] == '\0' || ctx->path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    if (ctx->use_ssl) {
        ssl_flags |= LCCSCF_USE_SSL;
        if (ctx->skip_cert_verify) {
            ssl_flags |= LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        }
    }

    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = (struct lws_context *)ctx->context;
    ccinfo.address = ctx->host;
    ccinfo.port = ctx->port;
    ccinfo.path = ctx->path;
    ccinfo.host = ctx->host;
    ccinfo.origin = ctx->host;
    ccinfo.protocol = s_lws_protocols[0].name;
    ccinfo.ssl_connection = ssl_flags;

    ctx->wsi = lws_client_connect_via_info(&ccinfo);
    return (ctx->wsi != NULL) ? APP_OK : APP_ERR_FAIL;
}

static void transport_service_task(void *arg)
{
    app_ai_ws_lws_transport_t *ctx = (app_ai_ws_lws_transport_t *)arg;

    if (ctx == NULL) {
        return;
    }

    while (ctx->running) {
        if (ctx->context != NULL) {
            (void)lws_service((struct lws_context *)ctx->context, (int)ctx->service_timeout_ms);
        }
        app_os_task_delay_ms(APP_AI_WS_LWS_SERVICE_IDLE_MS);
    }

    if (ctx->context != NULL) {
        lws_context_destroy((struct lws_context *)ctx->context);
        ctx->context = NULL;
    }
    ctx->wsi = NULL;
    ctx->connected = false;
    transport_drain_queue(ctx);
}

static int lws_transport_connect(void *transport_ctx, const app_ai_ws_config_t *config)
{
    app_ai_ws_lws_transport_t *ctx = (app_ai_ws_lws_transport_t *)transport_ctx;
    int ret;

    if (ctx == NULL || config == NULL || config->endpoint[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return APP_ERR_NOT_READY;
    }
    if (ctx->running) {
        return APP_OK;
    }

    ret = transport_parse_endpoint(ctx, config->endpoint);
    if (ret != APP_OK) {
        return ret;
    }
    ret = transport_build_auth_header(ctx, config);
    if (ret != APP_OK) {
        return ret;
    }

    ret = transport_create_context(ctx);
    if (ret != APP_OK) {
        return ret;
    }
    ret = transport_create_connection(ctx);
    if (ret != APP_OK) {
        lws_context_destroy((struct lws_context *)ctx->context);
        ctx->context = NULL;
        ctx->wsi = NULL;
        return ret;
    }

    ctx->running = true;
    ret = app_os_task_create(&ctx->task,
                             "app_ai_ws",
                             transport_service_task,
                             ctx,
                             ctx->task_stack_bytes,
                             ctx->task_priority);
    if (ret != APP_OK) {
        ctx->running = false;
        lws_context_destroy((struct lws_context *)ctx->context);
        ctx->context = NULL;
        ctx->wsi = NULL;
        return ret;
    }

    app_log("app_ai_ws connect setup: %s:%u%s",
            ctx->host,
            (unsigned int)ctx->port,
            ctx->path);
    return APP_OK;
}

static int lws_transport_disconnect(void *transport_ctx)
{
    app_ai_ws_lws_transport_t *ctx = (app_ai_ws_lws_transport_t *)transport_ctx;

    if (ctx == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return APP_ERR_NOT_READY;
    }

    ctx->running = false;
    ctx->connected = false;
    ctx->wsi = NULL;
    transport_drain_queue(ctx);
    return APP_OK;
}

static int lws_transport_send_text(void *transport_ctx, const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    return transport_enqueue((app_ai_ws_lws_transport_t *)transport_ctx,
                             (const uint8_t *)text,
                             (uint32_t)strlen(text),
                             LWS_WRITE_TEXT);
}

static int lws_transport_send_binary(void *transport_ctx, const uint8_t *data, uint32_t len)
{
    return transport_enqueue((app_ai_ws_lws_transport_t *)transport_ctx,
                             data,
                             len,
                             LWS_WRITE_BINARY);
}

int app_ai_ws_lws_transport_init(app_ai_ws_lws_transport_t *ctx,
                                  const app_ai_ws_lws_config_t *config)
{
    uint8_t queue_depth;
    int ret;

    if (ctx == NULL || config == NULL || config->protocol == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->protocol = config->protocol;
    ctx->skip_cert_verify = config->skip_cert_verify;
    ctx->service_timeout_ms = (config->service_timeout_ms != 0U) ?
                              config->service_timeout_ms :
                              APP_AI_WS_LWS_DEFAULT_SERVICE_TIMEOUT_MS;
    ctx->task_stack_bytes = (config->task_stack_bytes != 0U) ?
                            config->task_stack_bytes :
                            APP_AI_WS_LWS_DEFAULT_STACK_BYTES;
    ctx->task_priority = (config->task_priority != 0U) ?
                         config->task_priority :
                         APP_AI_WS_LWS_DEFAULT_PRIORITY;
    queue_depth = (config->send_queue_depth != 0U) ?
                  config->send_queue_depth :
                  APP_AI_WS_LWS_DEFAULT_QUEUE_DEPTH;
    ctx->send_queue_depth = queue_depth;
    transport_copy_string(ctx->protocol_name, sizeof(ctx->protocol_name), s_lws_protocols[0].name);

    ret = app_os_queue_create(&ctx->send_queue, queue_depth, sizeof(app_ai_ws_lws_send_msg_t));
    if (ret != APP_OK) {
        memset(ctx, 0, sizeof(*ctx));
        return ret;
    }

    ctx->initialized = true;
    return APP_OK;
}

int app_ai_ws_lws_transport_register(app_ai_ws_lws_transport_t *ctx)
{
    if (ctx == NULL || !ctx->initialized || ctx->protocol == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    return app_ai_ws_protocol_set_transport(ctx->protocol, &s_lws_transport_ops, ctx);
}

const app_ai_ws_transport_ops_t *app_ai_ws_lws_transport_ops(void)
{
    return &s_lws_transport_ops;
}
