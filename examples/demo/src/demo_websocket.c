/**
 * @file demo_websocket.c
 * @brief WebSocket connect/send/receive/close test
 * @version 2.0
 * @date 2025-08-22
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

#include "lierda_app_main.h"
#include "lwip/sockets.h"
#include "libwebsockets.h"
#include "liot_os.h"

/** @brief WebSocket server host */
static const char *ws_host = "49.235.147.4";

/** @brief WebSocket server path */
static const char *ws_path = "/";

/** @brief WebSocket server port */
static const int ws_port = 8765;

/** @brief Message to send */
#define REQUEST_TEXT "hello test message"

/**
 * @brief Connection state machine
 * 0 - initialized
 * 1 - connected
 * 2 - ready to send
 * 3 - sent, waiting response
 * 4 - done/closed
 */
static volatile int conn_state = 0;

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len)
{
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        liot_trace("ws connected\n");
        conn_state = 1;
        /* Request writable callback after connection established */
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (conn_state == 1) {
            /*
             * Critical: lws_write requires LWS_PRE bytes reserved at buffer start
             * for libwebsockets protocol frame header, otherwise memory corruption
             * occurs causing system crash
             */
            const char *msg = REQUEST_TEXT;
            int msg_len = strlen(msg);
            unsigned char buf[LWS_PRE + 128];

            memcpy(&buf[LWS_PRE], msg, msg_len);

            int n = lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
            liot_trace("ws sent %d/%d bytes : %s\n", n, msg_len, REQUEST_TEXT);

            if (n < 0) {
                liot_trace("ws send failed\n");
                return -1;
            }
            conn_state = 2;
        }
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        int print_len = (len > 256) ? 256 : (int)len;
        liot_trace("ws recv (%d bytes): %.*s\n", (int)len, print_len, (char *)in);

        if (conn_state == 2) {
            /* Response received, prepare to close */
            liot_trace("ws got response, closing\n");
            conn_state = 3;
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        liot_trace("ws connect error: %s\n", in ? (char *)in : "unknown");
        conn_state = 4;
        break;

    case LWS_CALLBACK_CLOSED:
        liot_trace("ws closed\n");
        conn_state = 4;
        break;

    default:
        break;
    }
    return 0;
}

static struct lws_protocols ws_protocols[] = {
    {
        .name = "test-protocol",
        .callback = ws_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 1024,
    },
    { NULL, NULL, 0, 0 }
};

void liot_websocket_demo_thread(void *argv)
{
    struct lws_context *context = NULL;

    /* Wait for network ready */
    liot_rtos_task_sleep_s(10);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = ws_protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context) {
        liot_trace("ws create context failed\n");
        goto exit;
    }
    liot_trace("ws create context success\n");
    /* Reset state */
    conn_state = 0;

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = ws_host;
    ccinfo.port = ws_port;
    ccinfo.path = ws_path;
    ccinfo.host = ws_host;
    ccinfo.origin = ws_host;
    ccinfo.protocol = ws_protocols[0].name;
    ccinfo.ssl_connection = 0;

    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        liot_trace("ws connect failed\n");
        goto exit;
    }

    liot_trace("ws connecting...\n");

    /* Event loop: max 60 iterations (~6 seconds) */
    for (int i = 0; i < 60 && conn_state < 3; i++) {
        int ret = lws_service(context, 100);
        if (ret < 0) {
            liot_trace("ws service error: %d\n", ret);
            break;
        }
    }

    /* If state is 3 (response received, pending close), run more cycles for graceful close */
    if (conn_state == 3) {
        for (int i = 0; i < 10 && conn_state != 4; i++) {
            lws_service(context, 100);
        }
    }

    liot_trace("ws demo done, final state: %d\n", conn_state);

exit:
    if (context)
        lws_context_destroy(context);

    liot_rtos_task_delete(NULL);
}
