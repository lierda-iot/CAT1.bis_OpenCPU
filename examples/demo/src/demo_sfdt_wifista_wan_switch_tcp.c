/**
 * @file demo_sfdt_wifista_wan_switch_tcp.c
 * @brief Wi-Fi STA/Cat1 default-route switching with TCP traffic.
 *
 * Bring up Cat1 and Wi-Fi STA, then alternate their default route.
 * TCP reconnects after each switch; DNS or a fixed IPv4 server is selectable.
 * The LN882H must associate with an upstream AP independently, and its
 * actual IPv4 configuration must match the configuration below.
 * Enable EXDEMO_SFDT_WIFISTA_WAN_SWITCH_TCP_EN and link libliot_sfdt.a built with
 * FEATURE_WIFI_SWITCH_ENABLE and FEATURE_CCIO_ENABLE.
 *
 * @copyright Copyright (c) 2026 Lierda Technology Co., Ltd.
 * @date 2026-09-28
 * @version 1.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_log.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "liot_datacall.h"
#include "liot_sockets.h"
#include "liot_sfdt.h"

/* ==================== Tunables ==================== */

/* The IPOS device and the cellular bearer must use the same CID. */
#define LIOT_SFDT_DEMO_DATA_CID           (1U)

/* LN882H AT port UART number (adjust to the target schematic). */
#define LIOT_SFDT_DEMO_AT_UART_NUM        (1U)

/* Wait budget for the one-shot wifi_cli attach. */
#define LIOT_SFDT_DEMO_WAN_WIFI_WAIT_MS   (30000U)

/* Duration of each Cat1/Wi-Fi network test phase. */
#define LIOT_SFDT_DEMO_WAN_PHASE_MS       (20000U)

/* 0: direct IPv4 debug server; 1: company server resolved through DNS. */
#define LIOT_SFDT_DEMO_WAN_TCP_USE_DNS    (0)
#define LIOT_SFDT_DEMO_WAN_TCP_HOST       "ai.iwg.senthink.com"
#define LIOT_SFDT_DEMO_WAN_TCP_DNS_PORT   (10003U)

#define LIOT_SFDT_DEMO_WAN_TCP_IP         "121.89.205.240"
#define LIOT_SFDT_DEMO_WAN_TCP_IP_PORT    (41000U)
#if LIOT_SFDT_DEMO_WAN_TCP_USE_DNS
#define LIOT_SFDT_DEMO_WAN_TCP_PORT       LIOT_SFDT_DEMO_WAN_TCP_DNS_PORT
#else
#define LIOT_SFDT_DEMO_WAN_TCP_PORT       LIOT_SFDT_DEMO_WAN_TCP_IP_PORT
#endif
#define LIOT_SFDT_DEMO_WAN_TCP_GAP_MS     (5000U)
#define LIOT_SFDT_DEMO_WAN_TCP_RX_MS      (1000U)
#define LIOT_SFDT_DEMO_WAN_TCP_CONNECT_MS (5000U)
#define LIOT_SFDT_DEMO_WAN_TCP_PAYLOAD    "SFDT TCP TEST"

#if LIOT_SFDT_DEMO_WAN_TCP_USE_DNS
#define LIOT_SFDT_DEMO_WAN_TEST_NAME    "DNS+TCP"
#else
#define LIOT_SFDT_DEMO_WAN_TEST_NAME    "TCP"
#endif

/* Debug hardware sample values: adjust to the schematic and RTE_Device.h. */
#define LIOT_SFDT_DEMO_SPI_PORT          LIOT_SPI_PORT1
#define LIOT_SFDT_DEMO_SPI_CS            LIOT_SPI_CS0
#define LIOT_SFDT_DEMO_SPI_SPEED_HZ      (20000000U)
/* Module physical pins, NOT chip GPIO/PAD numbers; GPIO2 owns the mapping. */
#define LIOT_SFDT_DEMO_SRDY_PIN          (38)
#define LIOT_SFDT_DEMO_MRDY_PIN          (39)
#define LIOT_SFDT_DEMO_MRDY_EDGE         L_INT_EDGE_RISE
#define LIOT_SFDT_DEMO_MRDY_PULL         LIOT_FORCE_PULL_UP

/* ==================== WAN-only state ==================== */

/**
 * @brief Egress published to the TCP task. Each committed switch increments
 * epoch so the TCP task closes the old connection and opens a new one.
 */
typedef struct
{
    volatile Liot_SfdtWanType_e egress;
    volatile uint8_t ready;        /**< 1 after the first egress is committed */
    volatile uint32_t epoch;       /**< ++ on every committed switch */
} Liot_SfdtWanDemoState_t;

static Liot_SfdtWanDemoState_t g_liot_sfdt_demo_wan_state;

static const char *_liot_sfdt_demo_egress_str(Liot_SfdtWanType_e t)
{
    return t == LIOT_SFDT_WAN_WIFI ? "WIFI" : "CELLULAR";
}

static const char *_liot_sfdt_demo_wan_err_str(Liot_SfdtWanErr_e e)
{
    switch (e)
    {
        case LIOT_SFDT_WAN_SUCCESS:            return "SUCCESS";
        case LIOT_SFDT_WAN_INVALID_PARAM_ERR:  return "INVALID_PARAM";
        case LIOT_SFDT_WAN_NOT_INIT_ERR:       return "NOT_INIT";
        case LIOT_SFDT_WAN_BUSY_ERR:           return "BUSY";
        case LIOT_SFDT_WAN_QUEUE_FULL_ERR:     return "QUEUE_FULL";
        case LIOT_SFDT_WAN_LINK_FAIL_ERR:      return "LINK_FAIL";
        case LIOT_SFDT_WAN_ROUTE_FAIL_ERR:     return "ROUTE_FAIL";
        case LIOT_SFDT_WAN_UNSUPPORTED_ERR:    return "UNSUPPORTED";
        case LIOT_SFDT_WAN_RECOVERY_FAIL_ERR:  return "RECOVERY_FAIL";
        default:                               return "UNKNOWN";
    }
}

/* ==================== Switch task ==================== */

/**
 * @brief Override board-level SPI and handshake parameters.
 *
 * Only hardware fields are touched. Protocol-fixed items stay as
 * Liot_SfdtGetDefaultConfig() supplied them.
 */
static void _liot_sfdt_demo_apply_board(Liot_SfdtConfig_t *cfg)
{
    cfg->spi.port = LIOT_SFDT_DEMO_SPI_PORT;
    cfg->spi.cs = LIOT_SFDT_DEMO_SPI_CS;
    cfg->bus_speed_hz = LIOT_SFDT_DEMO_SPI_SPEED_HZ;

    cfg->srdy_pin = LIOT_SFDT_DEMO_SRDY_PIN;
    cfg->mrdy_source = LIOT_SFDT_MRDY_SOURCE_GPIO;
    cfg->mrdy_pin = LIOT_SFDT_DEMO_MRDY_PIN;
    cfg->mrdy_edge = LIOT_SFDT_DEMO_MRDY_EDGE;
    cfg->mrdy_pull = LIOT_SFDT_DEMO_MRDY_PULL;
}

/**
 * @brief Build the Wi-Fi STA IPv4 configuration.
 *
 * Sample values for bring-up. A real product must use the address, netmask,
 * gateway, DNS and MTU that the LN882H reports after it associates.
 */
static void _liot_sfdt_demo_build_ip(Liot_SfdtIpConfig_t *ip)
{
    static const uint8_t addr[4]    = {192U, 168U, 137U, 25U};
    static const uint8_t netmask[4] = {255U, 255U, 255U, 0U};
    static const uint8_t gateway[4] = {192U, 168U, 137U, 1U};
    static const uint8_t mac[6]     = {0x00U, 0x50U, 0x7BU,
                                       0x7EU, 0x50U, 0xF4U};
    static const uint8_t dns0[4]    = {192U, 168U, 137U, 1U};

    memset(ip, 0, sizeof(*ip));
    ip->ip_type = LIOT_SFDT_IP_V4;
    ip->mtu = 1500U;
    ip->mtu_present = 1U;
    ip->ipv4_netmask_present = 1U;
    ip->ipv4_address.type = LIOT_SFDT_ADDR_V4;
    memcpy(ip->ipv4_address.address, addr, sizeof(addr));
    ip->ipv4_netmask.type = LIOT_SFDT_ADDR_V4;
    memcpy(ip->ipv4_netmask.address, netmask, sizeof(netmask));
    ip->ipv4_gateway_present = 1U;
    ip->ipv4_gateway.type = LIOT_SFDT_ADDR_V4;
    memcpy(ip->ipv4_gateway.address, gateway, sizeof(gateway));
    ip->mac_present = 1U;
    memcpy(ip->mac, mac, sizeof(mac));
    ip->dns_count = 1U;
    ip->dns[0].type = LIOT_SFDT_ADDR_V4;
    memcpy(ip->dns[0].address, dns0, sizeof(dns0));
}

static volatile uint8_t g_liot_sfdt_demo_attach_done;
static volatile Liot_SfdtWanErr_e g_liot_sfdt_demo_attach_result;

/**
 * @brief wifi_cli attach callback, runs on the liot_sfdt worker task.
 */
static void _liot_sfdt_demo_attach_cb(Liot_SfdtWanErr_e result,
                       Liot_SfdtWanType_e wan_type,
                       void *user_data)
{
    (void)user_data;
    g_liot_sfdt_demo_attach_result = result;
    g_liot_sfdt_demo_attach_done = 1U;
    liot_trace("[sfdt-demo] attach DONE: wan=%s result=%s",
               _liot_sfdt_demo_egress_str(wan_type), _liot_sfdt_demo_wan_err_str(result));
}

/**
 * @brief Bring up both bearers and alternate the default route every 20 seconds.
 *
 * Cat1 is activated first so its netif exists before the Wi-Fi attach. After
 * Wi-Fi is attached, route changes are synchronous and do not recreate either
 * netif or the OPAQ channel.
 */
static void _liot_sfdt_demo_run_switch(void)
{
    Liot_SfdtWanHandle_t handle;
    Liot_SfdtConfig_t cfg;
    Liot_SfdtIpConfig_t ip;
    Liot_SfdtWanErr_e err;
    liot_datacall_errcode_e dret;
    uint32_t waited;
    uint32_t cycle = 1U;
    int32_t ret;

    if (Liot_SfdtGetDefaultConfig(&cfg) != LIOT_SFDT_TRANSPORT_SUCCESS)
    {
        liot_trace("[sfdt-demo] GetDefaultConfig failed");
        return;
    }
    cfg.at_uart_num = LIOT_SFDT_DEMO_AT_UART_NUM;
    cfg.at_uart_enable = 1U;
    _liot_sfdt_demo_apply_board(&cfg);

    /* Cat1 first so the cellular netif exists when the WAN service is created. */
    dret = liot_start_data_call(0, LIOT_SFDT_DEMO_DATA_CID,
                                LIOT_DATA_TYPE_IP, "", "",
                                "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("[sfdt-demo] start_data_call cid=%u ret=%d",
               (unsigned int)LIOT_SFDT_DEMO_DATA_CID, (int)dret);
    if (dret != LIOT_DATACALL_SUCCESS)
    {
        liot_trace("[sfdt-demo] no Cat1 bearer: cellular egress impossible");
        return;
    }

    /* Initialize SFDT before attaching Wi-Fi. Both WANs are then present for
     * the route-only alternation below. */
    Liot_GpioInit(L_GPIO_5, L_IO_OUTPUT, L_IO_HIGH, NULL);
    ret = Liot_SfdtInit(&cfg);
    if (ret != LIOT_SFDT_TRANSPORT_SUCCESS &&
        ret != LIOT_SFDT_TRANSPORT_ALREADY_INITIALIZED)
    {
        liot_trace("[sfdt-demo] Liot_SfdtInit failed: %ld", (long)ret);
        return;
    }
    liot_trace("[sfdt-demo] transport ready: ret=%ld", (long)ret);

    handle = Liot_SfdtWanInit();
    if (handle == NULL)
    {
        liot_trace("[sfdt-demo] Liot_SfdtWanInit failed");
        return;
    }

    /* Attach wifi_cli once. Keep Wi-Fi as the first tested default route;
     * Cat1 is already active and remains available for the next phase. */
    g_liot_sfdt_demo_attach_done = 0U;
    _liot_sfdt_demo_build_ip(&ip);
    err = Liot_SfdtWanSwitchToWifi(handle, &ip, _liot_sfdt_demo_attach_cb, NULL);
    liot_trace("[sfdt-demo] wifi attach request: %s", _liot_sfdt_demo_wan_err_str(err));
    if (err != LIOT_SFDT_WAN_SUCCESS)
        return;

    waited = 0U;
    while (!g_liot_sfdt_demo_attach_done && waited < LIOT_SFDT_DEMO_WAN_WIFI_WAIT_MS)
    {
        liot_rtos_task_sleep_ms(100U);
        waited += 100U;
    }
    if (!g_liot_sfdt_demo_attach_done)
    {
        liot_trace("[sfdt-demo] wifi attach callback timeout");
        return;
    }
    if (g_liot_sfdt_demo_attach_result != LIOT_SFDT_WAN_SUCCESS)
    {
        liot_trace("[sfdt-demo] wifi attach failed: %s",
                   _liot_sfdt_demo_wan_err_str(g_liot_sfdt_demo_attach_result));
        return;
    }

    err = Liot_SfdtWanSetEgress(handle, LIOT_SFDT_WAN_WIFI, 0U);
    liot_trace("[sfdt-demo] initial SetEgress(WIFI) -> %s",
               _liot_sfdt_demo_wan_err_str(err));
    if (err != LIOT_SFDT_WAN_SUCCESS)
    {
        liot_trace("[sfdt-demo] initial Wi-Fi route unavailable; phase test stopped");
        return;
    }

    g_liot_sfdt_demo_wan_state.egress = LIOT_SFDT_WAN_WIFI;
    g_liot_sfdt_demo_wan_state.epoch++;
    g_liot_sfdt_demo_wan_state.ready = 1U;

    while (1)
    {
        liot_trace("[sfdt-demo] ===== CYCLE %lu: %s WIFI BEGIN duration=%lu ms =====",
                   (unsigned long)cycle,
                   LIOT_SFDT_DEMO_WAN_TEST_NAME,
                   (unsigned long)LIOT_SFDT_DEMO_WAN_PHASE_MS);
        liot_rtos_task_sleep_ms(LIOT_SFDT_DEMO_WAN_PHASE_MS);
        liot_trace("[sfdt-demo] ===== CYCLE %lu: %s WIFI END; switching WIFI -> CAT1 =====",
                   (unsigned long)cycle, LIOT_SFDT_DEMO_WAN_TEST_NAME);

        err = Liot_SfdtWanSetEgress(handle, LIOT_SFDT_WAN_CELLULAR,
                                    LIOT_SFDT_DEMO_DATA_CID);
        liot_trace("[sfdt-demo] SetEgress(CELLULAR) -> %s",
                   _liot_sfdt_demo_wan_err_str(err));
        if (err != LIOT_SFDT_WAN_SUCCESS)
        {
            liot_trace("[sfdt-demo] Cat1 route failed; phase test stopped");
            return;
        }
        g_liot_sfdt_demo_wan_state.egress = LIOT_SFDT_WAN_CELLULAR;
        g_liot_sfdt_demo_wan_state.epoch++;

        liot_trace("[sfdt-demo] ===== CYCLE %lu: %s CAT1 BEGIN duration=%lu ms =====",
                   (unsigned long)cycle,
                   LIOT_SFDT_DEMO_WAN_TEST_NAME,
                   (unsigned long)LIOT_SFDT_DEMO_WAN_PHASE_MS);
        liot_rtos_task_sleep_ms(LIOT_SFDT_DEMO_WAN_PHASE_MS);
        liot_trace("[sfdt-demo] ===== CYCLE %lu: %s CAT1 END; switching CAT1 -> WIFI =====",
                   (unsigned long)cycle, LIOT_SFDT_DEMO_WAN_TEST_NAME);

        err = Liot_SfdtWanSetEgress(handle, LIOT_SFDT_WAN_WIFI, 0U);
        liot_trace("[sfdt-demo] SetEgress(WIFI) -> %s",
                   _liot_sfdt_demo_wan_err_str(err));
        if (err != LIOT_SFDT_WAN_SUCCESS)
        {
            liot_trace("[sfdt-demo] Wi-Fi route failed; phase test stopped");
            return;
        }
        g_liot_sfdt_demo_wan_state.egress = LIOT_SFDT_WAN_WIFI;
        g_liot_sfdt_demo_wan_state.epoch++;
        cycle++;
    }
}

/* ==================== TCP task ==================== */

/**
 * @brief Open a nonblocking TCP connection using DNS or a literal IPv4 address.
 *
 * lwip_getaddrinfo() and the socket are intentionally not CID-pinned, so
 * both operations follow the default route selected by the WAN switch task.
 */
static int _liot_sfdt_demo_tcp_open(Liot_SfdtWanType_e egress)
{
#if LIOT_SFDT_DEMO_WAN_TCP_USE_DNS
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *ai;
#endif
    struct timeval timeout;
    fd_set write_set;
    struct sockaddr_in peer;
    socklen_t error_len;
    int socket_fd = -1;
    int flags;
    int socket_error;
    int ret;

#if LIOT_SFDT_DEMO_WAN_TCP_USE_DNS
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ret = lwip_getaddrinfo(LIOT_SFDT_DEMO_WAN_TCP_HOST, NULL,
                           &hints, &result);
    if (ret != 0 || result == NULL)
    {
        liot_trace("[sfdt-demo] TCP DNS FAIL egress=%s host=%s ret=%d",
                   _liot_sfdt_demo_egress_str(egress),
                   LIOT_SFDT_DEMO_WAN_TCP_HOST, ret);
        if (result != NULL)
            lwip_freeaddrinfo(result);
        return -1;
    }

    liot_trace("[sfdt-demo] TCP DNS OK egress=%s host=%s",
               _liot_sfdt_demo_egress_str(egress),
               LIOT_SFDT_DEMO_WAN_TCP_HOST);

    for (ai = result; ai != NULL; ai = ai->ai_next)
    {
        if (ai->ai_family != AF_INET || ai->ai_addr == NULL)
            continue;

        memcpy(&peer, ai->ai_addr, sizeof(peer));
        liot_trace("[sfdt-demo] TCP DNS RESULT egress=%s ip=%s",
                   _liot_sfdt_demo_egress_str(egress), inet_ntoa(peer.sin_addr));
        peer.sin_port = htons(LIOT_SFDT_DEMO_WAN_TCP_PORT);

        socket_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_fd < 0)
            continue;

        flags = lwip_fcntl(socket_fd, F_GETFL, 0);
        if (flags < 0 || lwip_fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            lwip_close(socket_fd);
            socket_fd = -1;
            continue;
        }

        ret = lwip_connect(socket_fd, (struct sockaddr *)&peer, sizeof(peer));
        if (ret < 0 && sock_get_errno(socket_fd) != EINPROGRESS)
        {
            lwip_close(socket_fd);
            socket_fd = -1;
            continue;
        }

        if (ret < 0)
        {
            FD_ZERO(&write_set);
            FD_SET(socket_fd, &write_set);
            timeout.tv_sec = LIOT_SFDT_DEMO_WAN_TCP_CONNECT_MS / 1000U;
            timeout.tv_usec = (LIOT_SFDT_DEMO_WAN_TCP_CONNECT_MS % 1000U) * 1000U;
            ret = lwip_select(socket_fd + 1, NULL, &write_set, NULL, &timeout);
            if (ret <= 0 || !FD_ISSET(socket_fd, &write_set))
            {
                lwip_close(socket_fd);
                socket_fd = -1;
                continue;
            }

            error_len = sizeof(socket_error);
            if (lwip_getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                                &socket_error, &error_len) < 0 ||
                socket_error != 0)
            {
                lwip_close(socket_fd);
                socket_fd = -1;
                continue;
            }
        }

        break;
    }

    lwip_freeaddrinfo(result);
#else
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons(LIOT_SFDT_DEMO_WAN_TCP_PORT);
    if (!inet_aton(LIOT_SFDT_DEMO_WAN_TCP_IP, &peer.sin_addr))
    {
        liot_trace("[sfdt-demo] TCP IP CONFIG FAIL egress=%s ip=%s",
                   _liot_sfdt_demo_egress_str(egress),
                   LIOT_SFDT_DEMO_WAN_TCP_IP);
        return -1;
    }
    liot_trace("[sfdt-demo] TCP IP target egress=%s ip=%s port=%u",
               _liot_sfdt_demo_egress_str(egress),
               LIOT_SFDT_DEMO_WAN_TCP_IP,
               (unsigned int)LIOT_SFDT_DEMO_WAN_TCP_PORT);

    socket_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd >= 0)
    {
        flags = lwip_fcntl(socket_fd, F_GETFL, 0);
        if (flags < 0 || lwip_fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            lwip_close(socket_fd);
            socket_fd = -1;
        }
    }
    if (socket_fd >= 0)
    {
        ret = lwip_connect(socket_fd, (struct sockaddr *)&peer, sizeof(peer));
        if (ret < 0 && sock_get_errno(socket_fd) != EINPROGRESS)
        {
            lwip_close(socket_fd);
            socket_fd = -1;
        }
        else if (ret < 0)
        {
            FD_ZERO(&write_set);
            FD_SET(socket_fd, &write_set);
            timeout.tv_sec = LIOT_SFDT_DEMO_WAN_TCP_CONNECT_MS / 1000U;
            timeout.tv_usec = (LIOT_SFDT_DEMO_WAN_TCP_CONNECT_MS % 1000U) * 1000U;
            ret = lwip_select(socket_fd + 1, NULL, &write_set, NULL, &timeout);
            if (ret <= 0 || !FD_ISSET(socket_fd, &write_set))
            {
                lwip_close(socket_fd);
                socket_fd = -1;
            }
            else
            {
                error_len = sizeof(socket_error);
                if (lwip_getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                                    &socket_error, &error_len) < 0 ||
                    socket_error != 0)
                {
                    lwip_close(socket_fd);
                    socket_fd = -1;
                }
            }
        }
    }
#endif
    if (socket_fd < 0)
    {
        liot_trace("[sfdt-demo] TCP CONNECT FAIL egress=%s target=%s port=%u",
                   _liot_sfdt_demo_egress_str(egress),
                   LIOT_SFDT_DEMO_WAN_TCP_USE_DNS ?
                       LIOT_SFDT_DEMO_WAN_TCP_HOST :
                       LIOT_SFDT_DEMO_WAN_TCP_IP,
                   (unsigned int)LIOT_SFDT_DEMO_WAN_TCP_PORT);
        return -1;
    }

    liot_trace("[sfdt-demo] TCP CONNECT OK egress=%s target=%s port=%u",
               _liot_sfdt_demo_egress_str(egress),
               LIOT_SFDT_DEMO_WAN_TCP_USE_DNS ?
                   LIOT_SFDT_DEMO_WAN_TCP_HOST :
                   LIOT_SFDT_DEMO_WAN_TCP_IP,
               (unsigned int)LIOT_SFDT_DEMO_WAN_TCP_PORT);
    return socket_fd;
}

static int _liot_sfdt_demo_tcp_exchange(int socket_fd,
                                        Liot_SfdtWanType_e egress,
                                        uint32_t sequence,
                                        uint32_t phase_sample,
                                        uint32_t planned_samples,
                                        uint32_t phase_remaining_ms)
{
    char tx[96];
    char rx[128];
    fd_set read_set;
    struct timeval timeout;
    int sent;
    int ret;
    int received;
    int tx_len;

    tx_len = snprintf(tx, sizeof(tx), "%s seq=%lu egress=%s\r\n",
                      LIOT_SFDT_DEMO_WAN_TCP_PAYLOAD,
                      (unsigned long)sequence,
                      _liot_sfdt_demo_egress_str(egress));
    if (tx_len <= 0 || (size_t)tx_len >= sizeof(tx))
    {
        liot_trace("[sfdt-demo] TCP TX BUILD FAIL egress=%s seq=%lu",
                   _liot_sfdt_demo_egress_str(egress),
                   (unsigned long)sequence);
        return -1;
    }

    sent = lwip_send(socket_fd, tx, (size_t)tx_len, 0);
    if (sent < 0)
    {
        liot_trace("[sfdt-demo] TCP TX FAIL egress=%s errno=%d",
                   _liot_sfdt_demo_egress_str(egress),
                   sock_get_errno(socket_fd));
        return -1;
    }

    liot_trace("[sfdt-demo] TCP TX egress=%s sample=%lu/%lu remaining=%lu ms seq=%lu len=%d data=%s",
               _liot_sfdt_demo_egress_str(egress),
               (unsigned long)phase_sample,
               (unsigned long)planned_samples,
               (unsigned long)phase_remaining_ms,
               (unsigned long)sequence, sent, tx);

    FD_ZERO(&read_set);
    FD_SET(socket_fd, &read_set);
    timeout.tv_sec = LIOT_SFDT_DEMO_WAN_TCP_RX_MS / 1000U;
    timeout.tv_usec = (LIOT_SFDT_DEMO_WAN_TCP_RX_MS % 1000U) * 1000U;
    ret = lwip_select(socket_fd + 1, &read_set, NULL, NULL, &timeout);
    if (ret == 0)
    {
        liot_trace("[sfdt-demo] TCP RX WAIT egress=%s sample=%lu/%lu remaining=%lu ms seq=%lu",
                   _liot_sfdt_demo_egress_str(egress),
                   (unsigned long)phase_sample,
                   (unsigned long)planned_samples,
                   (unsigned long)phase_remaining_ms,
                   (unsigned long)sequence);
        return 0;
    }
    if (ret < 0)
    {
        liot_trace("[sfdt-demo] TCP RX SELECT FAIL egress=%s sample=%lu/%lu remaining=%lu ms errno=%d",
                   _liot_sfdt_demo_egress_str(egress),
                   (unsigned long)phase_sample,
                   (unsigned long)planned_samples,
                   (unsigned long)phase_remaining_ms,
                   sock_get_errno(socket_fd));
        return -1;
    }

    received = lwip_recv(socket_fd, rx, sizeof(rx) - 1U, 0);
    if (received <= 0)
    {
        liot_trace("[sfdt-demo] TCP RX CLOSE egress=%s sample=%lu/%lu remaining=%lu ms len=%d",
                   _liot_sfdt_demo_egress_str(egress),
                   (unsigned long)phase_sample,
                   (unsigned long)planned_samples,
                   (unsigned long)phase_remaining_ms,
                   received);
        return -1;
    }
    rx[received] = '\0';
    liot_trace("[sfdt-demo] TCP RX egress=%s sample=%lu/%lu remaining=%lu ms seq=%lu len=%d data=%s",
               _liot_sfdt_demo_egress_str(egress),
               (unsigned long)phase_sample,
               (unsigned long)planned_samples,
               (unsigned long)phase_remaining_ms,
               (unsigned long)sequence, received, rx);
    return 0;
}

/**
 * @brief Keep one TCP session per egress phase and renew it after a switch.
 *
 * Optional DNS resolution and TCP exchange run in this task and are tagged
 * with the active egress.
 */
static void _liot_sfdt_demo_run_tcp(void)
{
    Liot_SfdtWanType_e egress;
    Liot_SfdtWanType_e connected_egress = LIOT_SFDT_WAN_CELLULAR;
    uint32_t connected_epoch = 0xFFFFFFFFU;
    uint32_t epoch;
    uint32_t waited = 0U;
    uint32_t sequence = 0U;
    uint32_t phase_epoch = 0xFFFFFFFFU;
    uint32_t phase_start_ms = 0U;
    uint32_t phase_sample = 0U;
    uint32_t phase_remaining_ms;
    uint32_t now_ms;
    uint32_t planned_samples =
        (LIOT_SFDT_DEMO_WAN_PHASE_MS + LIOT_SFDT_DEMO_WAN_TCP_GAP_MS - 1U) /
        LIOT_SFDT_DEMO_WAN_TCP_GAP_MS;
    int socket_fd = -1;

    while (!g_liot_sfdt_demo_wan_state.ready &&
           waited < LIOT_SFDT_DEMO_WAN_WIFI_WAIT_MS)
    {
        liot_rtos_task_sleep_ms(200U);
        waited += 200U;
    }
    if (!g_liot_sfdt_demo_wan_state.ready)
    {
        liot_trace("[sfdt-demo] egress never came up; TCP task gives up");
        return;
    }

    liot_trace("[sfdt-demo] TCP test target=%s:%u payload=%s seq+egress",
               LIOT_SFDT_DEMO_WAN_TCP_USE_DNS ?
                   LIOT_SFDT_DEMO_WAN_TCP_HOST :
                   LIOT_SFDT_DEMO_WAN_TCP_IP,
               (unsigned int)LIOT_SFDT_DEMO_WAN_TCP_PORT,
               LIOT_SFDT_DEMO_WAN_TCP_PAYLOAD);

    while (1)
    {
        epoch = g_liot_sfdt_demo_wan_state.epoch;
        egress = g_liot_sfdt_demo_wan_state.egress;

        if (phase_epoch != epoch)
        {
            phase_epoch = epoch;
            phase_start_ms = liot_rtos_get_running_time();
            phase_sample = 0U;
            liot_trace("[sfdt-demo] TCP PHASE START egress=%s duration=%lu ms planned_samples=%lu",
                       _liot_sfdt_demo_egress_str(egress),
                       (unsigned long)LIOT_SFDT_DEMO_WAN_PHASE_MS,
                       (unsigned long)planned_samples);
        }

        if (socket_fd < 0 || connected_epoch != epoch)
        {
            if (socket_fd >= 0)
            {
                liot_trace("[sfdt-demo] TCP CLOSE egress=%s: route changed",
                           _liot_sfdt_demo_egress_str(connected_egress));
                lwip_close(socket_fd);
                socket_fd = -1;
            }
            socket_fd = _liot_sfdt_demo_tcp_open(egress);
            if (socket_fd >= 0)
            {
                connected_epoch = epoch;
                connected_egress = egress;
            }
        }

        now_ms = liot_rtos_get_running_time();
        if (now_ms - phase_start_ms >= LIOT_SFDT_DEMO_WAN_PHASE_MS)
            phase_remaining_ms = 0U;
        else
            phase_remaining_ms = LIOT_SFDT_DEMO_WAN_PHASE_MS -
                                 (now_ms - phase_start_ms);

        if (socket_fd >= 0)
        {
            phase_sample++;
            if (_liot_sfdt_demo_tcp_exchange(socket_fd, egress, sequence++,
                                             phase_sample, planned_samples,
                                             phase_remaining_ms) < 0)
            {
                lwip_close(socket_fd);
                socket_fd = -1;
            }
        }
        liot_rtos_task_sleep_ms(LIOT_SFDT_DEMO_WAN_TCP_GAP_MS);
    }
}

/* ==================== Thread entries ==================== */

/**
 * @brief Switch task entry: configures power/sleep, then runs the test stages.
 */
void liot_sfdt_wifista_wan_switch_thread(void *argv)
{
    (void)argv;

    liot_rtos_task_sleep_ms(500U);
    liot_trace("==== sfdt demo: Wi-Fi STA/Cat1 %lu ms %s route phase test ====",
               (unsigned long)LIOT_SFDT_DEMO_WAN_PHASE_MS,
               LIOT_SFDT_DEMO_WAN_TEST_NAME);

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    _liot_sfdt_demo_run_switch();
    liot_trace("[sfdt-demo] switch task exit");
    liot_rtos_task_delete(NULL);
}

/**
 * @brief Run TCP traffic with optional DNS on the current default route.
 */
void liot_sfdt_wifista_wan_tcp_thread(void *argv)
{
    (void)argv;
    _liot_sfdt_demo_run_tcp();
    liot_trace("[sfdt-demo] TCP task exit");
    liot_rtos_task_delete(NULL);
}
