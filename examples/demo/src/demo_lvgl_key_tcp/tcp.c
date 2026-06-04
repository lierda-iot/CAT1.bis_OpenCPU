/**
 * @brief TCP connection demo with key control and LVGL display (LSDK port)
 *
 * Key events (from demo_tgai_key.c):
 *   Single click  -> Connect to TCP server
 *   Double click  -> Send "Lierda" via TCP
 *   Triple click  -> Disconnect TCP
 *
 * OLED display (updated every 2 seconds):
 *   Line 1: TCP: Connected / Disconnected / Connecting
 *   Line 2: Data Recv: NBytes
 */

#include <string.h>
#include "liot_os.h"
#include "liot_log.h"
#include "liot_datacall.h"
#include "liot_sockets.h"
#include "lcd_gui.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */
#define TCP_SERVER_IP           "121.89.205.240"
#define TCP_SERVER_PORT         41005
#define TCP_BUFFER_SIZE         128
#define TCP_RETRY_DELAY_MS      4000
#define NET_WAIT_TIMES          5
#define NET_WAIT_TIMEOUT_S      60

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static liot_task_t s_display_task = NULL;

static int s_socket_fd = -1;
static uint32_t s_recv_bytes = 0;
static int s_tcp_status = -1;   /* -1=idle, 0=connected, 2=failed, 4=disconnected */

static uint8_t s_recv_buf[TCP_BUFFER_SIZE];

static int s_data_call_active = 0;  /* 0=not started, 1=active */
static int s_nSim = 0;
static int s_cid = 1;

/* ------------------------------------------------------------------ */
/* Socket callback                                                     */
/* ------------------------------------------------------------------ */
static void tcp_socket_callback(Liot_SocketEvent_e event,
                                int socket_fd,
                                uint8_t *data,
                                uint32_t data_len,
                                void *user_data)
{
    switch (event) {
        case LIOT_SOCKET_CONNECT_SUCCESS:
            liot_trace("TCP connected, fd:%d", socket_fd);
            s_socket_fd = socket_fd;
            s_tcp_status = LIOT_SOCKET_CONNECT_SUCCESS;
            break;

        case LIOT_SOCKET_RECV_DATA:
            s_recv_bytes += data_len;
            liot_trace("TCP recv %lu bytes, total %lu",
                       (unsigned long)data_len, (unsigned long)s_recv_bytes);
            if (data && data_len > 0) {
                int copy_len = (int)data_len < TCP_BUFFER_SIZE - 1
                               ? (int)data_len : TCP_BUFFER_SIZE - 1;
                memcpy(s_recv_buf, data, copy_len);
                s_recv_buf[copy_len] = '\0';
            }
            break;

        case LIOT_SOCKET_DISCONNECT:
            liot_trace("TCP disconnected");
            s_socket_fd = -1;
            s_tcp_status = LIOT_SOCKET_DISCONNECT;
            break;

        case LIOT_SOCKET_CONNECT_FAIL:
            liot_trace("TCP connect failed");
            s_socket_fd = -1;
            s_tcp_status = LIOT_SOCKET_CONNECT_FAIL;
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Data call helpers                                                   */
/* ------------------------------------------------------------------ */
static int tcp_start_data_call(void)
{
    int times = 0;
    int ret = -1;

    while (LIOT_DATACALL_SUCCESS !=
           (ret = liot_network_register_wait(s_nSim, NET_WAIT_TIMEOUT_S))
           && times < NET_WAIT_TIMES) {
        times++;
        liot_trace("network register wait ret:0x%x", ret);
        liot_rtos_task_sleep_s(1);
    }

    if (ret != LIOT_DATACALL_SUCCESS) {
        liot_trace("network register failed");
        return ret;
    }

    liot_set_data_call_asyn_mode(s_nSim, s_cid, 0);

    ret = liot_start_data_call(s_nSim, s_cid, LIOT_DATA_TYPE_IPV4V6,
                               "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_rtos_task_sleep_ms(TCP_RETRY_DELAY_MS);
    return ret;
}

/* ------------------------------------------------------------------ */
/* Key callbacks (override weak functions from demo_tgai_key.c)       */
/* ------------------------------------------------------------------ */
void tgai_key_single_click_cb(void)
{
    liot_trace("key: single click -> connect TCP");

    if (s_socket_fd >= 0) {
        liot_trace("TCP already connected, fd:%d", s_socket_fd);
        return;
    }
    s_tcp_status = LIOT_SOCKET_DISCONNECT;
    /* Start data call if not active */
    if (!s_data_call_active) {
        if (tcp_start_data_call() != LIOT_DATACALL_SUCCESS) {
            liot_trace("data call failed");
            return;
        }
        s_data_call_active = 1;
        liot_rtos_task_sleep_ms(2000);
    }

    /* Open socket */
    Liot_SocketInfo_t socket_info;
    memset(&socket_info, 0, sizeof(socket_info));
    socket_info.cid = s_cid;
    socket_info.socket_type = LIOT_SOCKET_TYPE_TCP;
    strncpy((char *)socket_info.host, TCP_SERVER_IP, sizeof(socket_info.host) - 1);
    socket_info.port = TCP_SERVER_PORT;
    socket_info.local_port = 0;
    socket_info.keepidle = 20;
    socket_info.keepinterval = 35;
    socket_info.keepcount = 5;

    s_tcp_status = -1;  /* connecting */
    s_socket_fd = Liot_SocketOpen(&socket_info, tcp_socket_callback, NULL);
    if (s_socket_fd < 0) {
        liot_trace("socket open failed");
        s_tcp_status = LIOT_SOCKET_CONNECT_FAIL;
    }
    else {
        liot_trace("socket opening, fd:%d", s_socket_fd);
    }
}

void tgai_key_double_click_cb(void)
{
    liot_trace("key: double click -> send 'Lierda'");

    if (s_socket_fd < 0) {
        liot_trace("TCP not connected");
        return;
    }

    const char *msg = "Lierda";
    int send_len = Liot_SocketSend(s_socket_fd, (uint8_t *)msg, strlen(msg));
    if (send_len > 0) {
        liot_trace("sent %d bytes", send_len);
    }
    else {
        liot_trace("send failed");
    }
}

void tgai_key_triple_click_cb(void)
{
    liot_trace("key: triple click -> disconnect TCP");

    if (s_socket_fd >= 0) {
        Liot_SocketClose(s_socket_fd);
        s_socket_fd = -1;
        s_recv_bytes = 0;
        s_tcp_status = LIOT_SOCKET_DISCONNECT;
        liot_trace("socket closed");
    }
    else {
        liot_trace("TCP not connected");
    }
}

/* ------------------------------------------------------------------ */
/* Display update task                                                 */
/* ------------------------------------------------------------------ */
static void display_update_task(void *arg)
{
    while (1) {
        lvgl_tcp_update(s_tcp_status, s_recv_bytes);
        liot_rtos_task_sleep_ms(1000);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void demo_tgai_csq_start(void)
{
    if (!s_display_task) {
        liot_rtos_task_create(&s_display_task, 10 * 1024, 14,
                              "display_task", display_update_task, NULL);
    }
}

void demo_tgai_csq_stop(void)
{
    if (s_display_task) {
        liot_rtos_task_delete(s_display_task);
        s_display_task = NULL;
    }

    if (s_socket_fd >= 0) {
        Liot_SocketClose(s_socket_fd);
        s_socket_fd = -1;
    }

    if (s_data_call_active) {
        liot_stop_data_call(s_nSim, s_cid);
        s_data_call_active = 0;
    }
}
