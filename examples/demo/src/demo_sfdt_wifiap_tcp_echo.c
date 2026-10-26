/**
 * @file demo_sfdt_wifiap_tcp_echo.c
 * @brief Wi-Fi AP raw-IP through SFDT to a multi-client lwIP TCP echo server.
 *
 * LN882H must already run as an AP and forward raw IPv4 packets over SFDT.
 * The logical WIFI_STA channel below selects the EC718 lwIP input path;
 * it does not configure the LN882H radio as a station. This uses CCIO,
 * NOT LIOT_SFDT_DATA_MODE_PASSTHROUGH or the hand-built TCP demo.
 *
 * Phone/PC: TCP Client -> 1.1.1.1:7777 through the LN882H AP.
 * Each client receives only its own bytes, unchanged (including binary data).
 * One task uses nonblocking sockets/select; no Cat1 dial-up, Ping or DNS.
 * Requires libliot_sfdt.a with FEATURE_WIFI_SWITCH_ENABLE/FEATURE_CCIO_ENABLE.
 *
 * @copyright Copyright (c) 2026 Lierda Technology Co., Ltd.
 * @date 2026-09-30
 */

#include <stdint.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_log.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "liot_sockets.h"
#include "liot_sfdt.h"

/* ==================== Test configuration ==================== */

/* EC718 raw-IP endpoint, NOT the phone/PC address assigned by AP DHCP. */
#define LIOT_SFDT_DEMO_TCP_IP             "1.1.1.1"
#define LIOT_SFDT_DEMO_TCP_PORT           (7777U)
#define LIOT_SFDT_DEMO_TCP_MAX_CLIENTS    (4U)
#define LIOT_SFDT_DEMO_TCP_BUFFER_SIZE    (512U)
#define LIOT_SFDT_DEMO_TCP_DUMP_MAX       (64U)
#define LIOT_SFDT_DEMO_ATTACH_WAIT_MS     (30000U)

/* Same board configuration as the working SFDT AP raw-IP demo. */
#define LIOT_SFDT_DEMO_AT_UART_NUM        (1U)
#define LIOT_SFDT_DEMO_SPI_PORT           LIOT_SPI_PORT1
#define LIOT_SFDT_DEMO_SPI_CS             LIOT_SPI_CS0
#define LIOT_SFDT_DEMO_SPI_SPEED_HZ       (20000000U)
/* Module physical pins, NOT chip GPIO/PAD numbers. */
#define LIOT_SFDT_DEMO_SRDY_PIN           (38)
#define LIOT_SFDT_DEMO_MRDY_PIN           (39)
#define LIOT_SFDT_DEMO_MRDY_EDGE          L_INT_EDGE_RISE
#define LIOT_SFDT_DEMO_MRDY_PULL          LIOT_FORCE_PULL_UP

#if LIOT_SFDT_DEMO_TCP_MAX_CLIENTS < 1 || \
    LIOT_SFDT_DEMO_TCP_MAX_CLIENTS >= FD_SETSIZE
#error "Reserve a socket for the listener as well as the TCP clients"
#endif

typedef struct
{
    int fd;
    uint32_t id;
    char peer_ip[16];
    uint16_t peer_port;
    uint16_t tx_len;
    uint16_t tx_offset;
    uint32_t rx_total;
    uint32_t tx_total;
    uint8_t data[LIOT_SFDT_DEMO_TCP_BUFFER_SIZE];
} Liot_SfdtDemoTcpClient_t;

/* Only the demo task owns client state. Keep buffers off the task stack. */
static Liot_SfdtDemoTcpClient_t
    g_liot_sfdt_demo_clients[LIOT_SFDT_DEMO_TCP_MAX_CLIENTS];
static uint32_t g_liot_sfdt_demo_connection_id;
static volatile uint8_t g_liot_sfdt_demo_attach_done;
static volatile uint8_t g_liot_sfdt_demo_attach_ready;
static volatile Liot_SfdtWanErr_e g_liot_sfdt_demo_attach_result;

/* ==================== SFDT / lwIP attachment ==================== */

static void _liot_sfdt_demo_attach_cb(Liot_SfdtWanErr_e result,
                                      Liot_SfdtWanType_e type,
                                      void *user_data)
{
    (void)user_data;
    g_liot_sfdt_demo_attach_result = result;
    g_liot_sfdt_demo_attach_ready =
        (uint8_t)(result == LIOT_SFDT_WAN_SUCCESS &&
                  type == LIOT_SFDT_WAN_WIFI);
    g_liot_sfdt_demo_attach_done = 1U;
    liot_trace("[sfdt-ap-tcp] attach result=%d logical_channel=%s",
               result, type == LIOT_SFDT_WAN_WIFI ? "WIFI_STA" : "MIFI_AP");
}

static int _liot_sfdt_demo_attach(void)
{
    Liot_SfdtConfig_t cfg;
    Liot_SfdtIpConfig_t ip;
    Liot_SfdtWanHandle_t handle;
    Liot_SfdtWanErr_e result;
    struct in_addr local_ip;
    uint32_t waited = 0U;
    int32_t ret;

    if (!inet_aton(LIOT_SFDT_DEMO_TCP_IP, &local_ip))
    {
        liot_trace("[sfdt-ap-tcp] invalid local IP=%s", LIOT_SFDT_DEMO_TCP_IP);
        return -1;
    }
    if (Liot_SfdtGetDefaultConfig(&cfg) != LIOT_SFDT_TRANSPORT_SUCCESS)
    {
        liot_trace("[sfdt-ap-tcp] GetDefaultConfig failed");
        return -1;
    }
    cfg.at_uart_num = LIOT_SFDT_DEMO_AT_UART_NUM;
    cfg.at_uart_enable = 1U;
    cfg.data_mode = LIOT_SFDT_DATA_MODE_CCIO;
    cfg.spi.port = LIOT_SFDT_DEMO_SPI_PORT;
    cfg.spi.cs = LIOT_SFDT_DEMO_SPI_CS;
    cfg.bus_speed_hz = LIOT_SFDT_DEMO_SPI_SPEED_HZ;
    cfg.srdy_pin = LIOT_SFDT_DEMO_SRDY_PIN;
    cfg.mrdy_source = LIOT_SFDT_MRDY_SOURCE_GPIO;
    cfg.mrdy_pin = LIOT_SFDT_DEMO_MRDY_PIN;
    cfg.mrdy_edge = LIOT_SFDT_DEMO_MRDY_EDGE;
    cfg.mrdy_pull = LIOT_SFDT_DEMO_MRDY_PULL;

    Liot_GpioInit(L_GPIO_5, L_IO_OUTPUT, L_IO_HIGH, NULL);
    ret = Liot_SfdtInit(&cfg);
    liot_trace("[sfdt-ap-tcp] transport init=%ld", (long)ret);
    if (ret != LIOT_SFDT_TRANSPORT_SUCCESS &&
        ret != LIOT_SFDT_TRANSPORT_ALREADY_INITIALIZED)
        return -1;

    handle = Liot_SfdtWanInit();
    if (handle == NULL)
    {
        liot_trace("[sfdt-ap-tcp] WAN init failed");
        return -1;
    }

    memset(&ip, 0, sizeof(ip));
    ip.ip_type = LIOT_SFDT_IP_V4;
    ip.mtu = 1500U;
    ip.mtu_present = 1U;
    ip.ipv4_address.type = LIOT_SFDT_ADDR_V4;
    memcpy(ip.ipv4_address.address, &local_ip.s_addr, sizeof(local_ip.s_addr));
    ip.ipv4_netmask_present = 1U;
    ip.ipv4_netmask.type = LIOT_SFDT_ADDR_V4;
    memset(ip.ipv4_netmask.address, 0xFF, 4U);
    /* /32 identifies EC718. Replies use the default raw-IP SFDT route;
     * LN882H forwards them to the AP client, without EC718 Ethernet ARP. */
    g_liot_sfdt_demo_attach_done = 0U;
    g_liot_sfdt_demo_attach_ready = 0U;
    g_liot_sfdt_demo_attach_result = LIOT_SFDT_WAN_LINK_FAIL_ERR;
    result = Liot_SfdtWanSwitchToWifi(handle, &ip,
                                     _liot_sfdt_demo_attach_cb, NULL);
    liot_trace("[sfdt-ap-tcp] attach request=%d local=%s/32",
               result, LIOT_SFDT_DEMO_TCP_IP);
    if (result != LIOT_SFDT_WAN_SUCCESS)
        return -1;

    while (!g_liot_sfdt_demo_attach_done &&
           waited < LIOT_SFDT_DEMO_ATTACH_WAIT_MS)
    {
        liot_rtos_task_sleep_ms(100U);
        waited += 100U;
    }
    if (!g_liot_sfdt_demo_attach_ready)
    {
        liot_trace("[sfdt-ap-tcp] attach failed: done=%u result=%d waited=%lu ms",
                   (unsigned int)g_liot_sfdt_demo_attach_done,
                   g_liot_sfdt_demo_attach_result, (unsigned long)waited);
        return -1;
    }
    return 0;
}

/* ==================== TCP clients ==================== */

static int _liot_sfdt_demo_socket_prepare(int fd)
{
    /* FD_SET in this SDK is indexed relative to LWIP_SOCKET_OFFSET. */
    if (fd < LWIP_SOCKET_OFFSET || fd - LWIP_SOCKET_OFFSET >= FD_SETSIZE)
    {
        liot_trace("[sfdt-ap-tcp] socket fd=%d outside select range", fd);
        return -1;
    }
    if (lwip_fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    {
        liot_trace("[sfdt-ap-tcp] nonblocking failed fd=%d errno=%d",
                   fd, sock_get_errno(fd));
        return -1;
    }
    return 0;
}

static int _liot_sfdt_demo_socket_retry(int fd)
{
    int error = sock_get_errno(fd);
    return error == EWOULDBLOCK || error == EAGAIN || error == EINTR;
}

static unsigned int _liot_sfdt_demo_client_count(void)
{
    unsigned int i;
    unsigned int count = 0U;
    for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
        if (g_liot_sfdt_demo_clients[i].fd >= 0)
            count++;
    return count;
}

static void _liot_sfdt_demo_client_close(Liot_SfdtDemoTcpClient_t *client,
                                         const char *reason)
{
    liot_trace("[sfdt-ap-tcp] CLOSE id=%lu peer=%s:%u reason=%s RX=%lu TX=%lu",
               (unsigned long)client->id, client->peer_ip,
               (unsigned int)client->peer_port, reason,
               (unsigned long)client->rx_total, (unsigned long)client->tx_total);
    lwip_close(client->fd);
    client->fd = -1;
    client->tx_len = 0U;
    client->tx_offset = 0U;
    liot_trace("[sfdt-ap-tcp] active=%u/%u", _liot_sfdt_demo_client_count(),
               (unsigned int)LIOT_SFDT_DEMO_TCP_MAX_CLIENTS);
}

static void _liot_sfdt_demo_client_accept(int listener)
{
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    Liot_SfdtDemoTcpClient_t *client = NULL;
    unsigned int i;
    int fd;

    memset(&peer, 0, sizeof(peer));
    /* Accept at most one per iteration so new arrivals cannot starve data. */
    fd = lwip_accept(listener, (struct sockaddr *)&peer, &peer_len);
    if (fd < 0)
    {
        if (!_liot_sfdt_demo_socket_retry(listener))
            liot_trace("[sfdt-ap-tcp] accept failed errno=%d",
                       sock_get_errno(listener));
        return;
    }
    if (_liot_sfdt_demo_socket_prepare(fd) < 0)
    {
        lwip_close(fd);
        return;
    }
    for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
    {
        if (g_liot_sfdt_demo_clients[i].fd < 0)
        {
            client = &g_liot_sfdt_demo_clients[i];
            break;
        }
    }
    if (client == NULL)
    {
        liot_trace("[sfdt-ap-tcp] FULL reject peer=%s:%u max=%u",
                   inet_ntoa(peer.sin_addr), (unsigned int)ntohs(peer.sin_port),
                   (unsigned int)LIOT_SFDT_DEMO_TCP_MAX_CLIENTS);
        lwip_close(fd);
        return;
    }
    memset(client, 0, sizeof(*client));
    client->fd = fd;
    client->id = ++g_liot_sfdt_demo_connection_id;
    client->peer_port = ntohs(peer.sin_port);
    /* inet_ntoa uses static storage; keep a private copy for this client. */
    strncpy(client->peer_ip, inet_ntoa(peer.sin_addr), sizeof(client->peer_ip) - 1U);
    liot_trace("[sfdt-ap-tcp] CONNECT id=%lu fd=%d peer=%s:%u active=%u/%u",
               (unsigned long)client->id, fd, client->peer_ip,
               (unsigned int)client->peer_port, _liot_sfdt_demo_client_count(),
               (unsigned int)LIOT_SFDT_DEMO_TCP_MAX_CLIENTS);
}

static void _liot_sfdt_demo_client_io(Liot_SfdtDemoTcpClient_t *client,
                                      fd_set *read_set, fd_set *write_set)
{
    char preview[LIOT_SFDT_DEMO_TCP_DUMP_MAX + 1U];
    unsigned int shown;
    unsigned int i;
    int result;

    if (client->tx_offset < client->tx_len)
    {
        if (!FD_ISSET(client->fd, write_set))
            return;
        result = lwip_send(client->fd, client->data + client->tx_offset,
                           (size_t)(client->tx_len - client->tx_offset), 0);
        if (result > 0)
        {
            client->tx_offset += (uint16_t)result;
            client->tx_total += (uint32_t)result;
            if (client->tx_offset == client->tx_len)
            {
                liot_trace("[sfdt-ap-tcp] TX id=%lu peer=%s:%u len=%u total=%lu",
                           (unsigned long)client->id, client->peer_ip,
                           (unsigned int)client->peer_port,
                           (unsigned int)client->tx_len,
                           (unsigned long)client->tx_total);
                client->tx_offset = 0U;
                client->tx_len = 0U;
            }
        }
        else if (result == 0 || !_liot_sfdt_demo_socket_retry(client->fd))
        {
            liot_trace("[sfdt-ap-tcp] send failed id=%lu ret=%d errno=%d",
                       (unsigned long)client->id, result, sock_get_errno(client->fd));
            _liot_sfdt_demo_client_close(client, "send error");
        }
        return;
    }

    if (!FD_ISSET(client->fd, read_set))
        return;
    result = lwip_recv(client->fd, client->data, sizeof(client->data), 0);
    if (result > 0)
    {
        client->rx_total += (uint32_t)result;
        client->tx_len = (uint16_t)result;
        client->tx_offset = 0U;
        /* Only the log is truncated/sanitized, never the echoed bytes.
         * A recv chunk is not necessarily one application message. */
        shown = (unsigned int)result;
        if (shown > LIOT_SFDT_DEMO_TCP_DUMP_MAX)
            shown = LIOT_SFDT_DEMO_TCP_DUMP_MAX;
        for (i = 0U; i < shown; i++)
            preview[i] = client->data[i] >= 0x20U && client->data[i] <= 0x7EU
                         ? (char)client->data[i] : '.';
        preview[shown] = '\0';
        liot_trace("[sfdt-ap-tcp] RX id=%lu peer=%s:%u len=%d total=%lu data=%s%s",
                   (unsigned long)client->id, client->peer_ip,
                   (unsigned int)client->peer_port, result,
                   (unsigned long)client->rx_total, preview,
                   (unsigned int)result > shown ? " (truncated)" : "");
    }
    else if (result == 0)
        _liot_sfdt_demo_client_close(client, "peer EOF");
    else if (!_liot_sfdt_demo_socket_retry(client->fd))
    {
        liot_trace("[sfdt-ap-tcp] recv failed id=%lu errno=%d",
                   (unsigned long)client->id, sock_get_errno(client->fd));
        _liot_sfdt_demo_client_close(client, "recv error");
    }
}

/* ==================== Single-task multi-client server ==================== */

static void _liot_sfdt_demo_tcp_echo(void)
{
    struct sockaddr_in local;
    struct timeval timeout;
    fd_set read_set;
    fd_set write_set;
    Liot_SfdtDemoTcpClient_t *client;
    unsigned int i;
    int listener;
    int max_fd;
    int result;

    for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
        g_liot_sfdt_demo_clients[i].fd = -1;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(LIOT_SFDT_DEMO_TCP_PORT);
    if (!inet_aton(LIOT_SFDT_DEMO_TCP_IP, &local.sin_addr))
        return;
    listener = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0)
    {
        liot_trace("[sfdt-ap-tcp] socket failed");
        return;
    }
    if (_liot_sfdt_demo_socket_prepare(listener) < 0)
        goto cleanup;
    if (lwip_bind(listener, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        liot_trace("[sfdt-ap-tcp] bind failed errno=%d", sock_get_errno(listener));
        goto cleanup;
    }
    if (lwip_listen(listener, LIOT_SFDT_DEMO_TCP_MAX_CLIENTS) < 0)
    {
        liot_trace("[sfdt-ap-tcp] listen failed errno=%d", sock_get_errno(listener));
        goto cleanup;
    }
    liot_trace("[sfdt-ap-tcp] LISTEN %s:%u max_clients=%u",
               LIOT_SFDT_DEMO_TCP_IP, (unsigned int)LIOT_SFDT_DEMO_TCP_PORT,
               (unsigned int)LIOT_SFDT_DEMO_TCP_MAX_CLIENTS);

    while (1)
    {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(listener, &read_set);
        max_fd = listener;
        for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
        {
            client = &g_liot_sfdt_demo_clients[i];
            if (client->fd < 0)
                continue;
            /* Backpressure: finish this client's pending echo before reading
             * more. A slow reader consumes only its own fixed buffer. */
            if (client->tx_offset < client->tx_len)
                FD_SET(client->fd, &write_set);
            else
                FD_SET(client->fd, &read_set);
            if (client->fd > max_fd)
                max_fd = client->fd;
        }
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        result = lwip_select(max_fd + 1, &read_set, &write_set, NULL, &timeout);
        if (result < 0)
        {
            liot_trace("[sfdt-ap-tcp] select failed ret=%d; stopping server", result);
            break;
        }
        if (result == 0)
            continue;
        for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
        {
            client = &g_liot_sfdt_demo_clients[i];
            if (client->fd >= 0)
                _liot_sfdt_demo_client_io(client, &read_set, &write_set);
        }
        /* Accept after serving old descriptors: a reused fd must not consume
         * readiness bits belonging to the connection just closed above. */
        if (FD_ISSET(listener, &read_set))
            _liot_sfdt_demo_client_accept(listener);
    }

cleanup:
    for (i = 0U; i < LIOT_SFDT_DEMO_TCP_MAX_CLIENTS; i++)
        if (g_liot_sfdt_demo_clients[i].fd >= 0)
            _liot_sfdt_demo_client_close(&g_liot_sfdt_demo_clients[i], "server exit");
    lwip_close(listener);
}

void liot_sfdt_wifiap_tcp_echo_demo_thread(void *argv)
{
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    (void)argv;

    liot_rtos_task_sleep_ms(500U);
    liot_trace("==== SFDT Wi-Fi AP raw-IP multi-client TCP echo ====");
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_SleepSetMode(&mode_cfg);
    if (_liot_sfdt_demo_attach() == 0)
        _liot_sfdt_demo_tcp_echo();
    liot_trace("[sfdt-ap-tcp] demo task exit");
    liot_rtos_task_delete(NULL);
}
