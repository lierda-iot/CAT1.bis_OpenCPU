#ifndef __LIOT_SFDT_H__
#define __LIOT_SFDT_H__

#include <stdint.h>
#include "liot_spi.h"
#include "liot_gpio2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LIOT_SFDT_TRANSPORT_SUCCESS = 0,
    LIOT_SFDT_TRANSPORT_INVALID_PARAM = -1,
    LIOT_SFDT_TRANSPORT_NOT_INITIALIZED = -2,
    LIOT_SFDT_TRANSPORT_ALREADY_INITIALIZED = -3,
    LIOT_SFDT_TRANSPORT_SPI_ERROR = -4,
    LIOT_SFDT_TRANSPORT_CCIO_ERROR = -5,
    LIOT_SFDT_TRANSPORT_OS_ERROR = -6,
    LIOT_SFDT_TRANSPORT_UNSUPPORTED = -7,
    LIOT_SFDT_TRANSPORT_TX_FULL = -8,
    LIOT_SFDT_TRANSPORT_TX_TOO_LARGE = -9,
    LIOT_SFDT_TRANSPORT_BUSY = -10,
    LIOT_SFDT_TRANSPORT_TX_EMPTY = -11,
} Liot_SfdtTransportErr_e;

/**
 * @brief Logical OPAQ channel used by the shared SFDT transport.
 *
 * MIFI_AP is the Cat1/AP passthrough path. WIFI_STA is the EC718 wifi_cli
 * path. The implementation owns the platform-specific custFlags/custExtras
 * mapping and callers must not invoke spiDevTransform directly.
 */
typedef enum
{
    LIOT_SFDT_CHANNEL_MIFI_AP = 0,
    LIOT_SFDT_CHANNEL_WIFI_STA,
} Liot_SfdtChannelMode_e;

/**
 * @brief Select the data backend used by the SFDT worker.
 *
 * CCIO keeps the existing 4G/IPOS/ETH path. PASSTHROUGH bypasses CCIO and
 * reports valid payloads through rx_callback; reverse-direction data is
 * queued with Liot_SfdtSend().
 */
typedef enum
{
    LIOT_SFDT_DATA_MODE_CCIO = 0,
    LIOT_SFDT_DATA_MODE_PASSTHROUGH,
} Liot_SfdtDataMode_e;

/**
 * @brief Receive callback used by passthrough mode.
 *
 * The data pointer belongs to the API receive buffer and is valid only while
 * the callback executes. Copy it if it must be retained. The callback runs in
 * the SFDT worker task and should not block for a long time. Return
 * LIOT_SFDT_TRANSPORT_SUCCESS when the payload was accepted; the return value is
 * reserved for application diagnostics and does not automatically change
 * flow control.
 */
typedef int32_t (*Liot_SfdtRxCallback)(const uint8_t *data,
                                       uint16_t len,
                                       void *arg);

/* Maximum number of packets retained by the API in passthrough TX mode. */
#define LIOT_SFDT_MAX_TX_QUEUE             (8U)

/* No module physical pin selected. Not a GPIO enum value or a PAD index. */
#define LIOT_SFDT_INVALID_PIN             (-1)

/**
 * @brief MRDY interrupt input source.
 *
 * Wakeup is kept for the original SFDT board wiring. GPIO uses the GPIO2
 * API (Liot_GpioInitDirect + liot_intcb_t); mrdy_pin is a module physical
 * pin number. GPIO here describes the interrupt source, not the numbering.
 */
typedef enum
{
    LIOT_SFDT_MRDY_SOURCE_WAKEUP = 0,
    LIOT_SFDT_MRDY_SOURCE_GPIO,
} Liot_SfdtMrdySource_e;

/* CCIO values used by the public configuration.  These API-owned constants
 * keep applications independent of the platform's private CCIO headers. */
enum
{
    LIOT_SFDT_CCIO_USAGE_AT   = 0,
    LIOT_SFDT_CCIO_USAGE_PPP  = 1,
    LIOT_SFDT_CCIO_USAGE_DIAG = 2,
    LIOT_SFDT_CCIO_USAGE_IPOS = 3,
    LIOT_SFDT_CCIO_USAGE_AUDIO = 4,
    LIOT_SFDT_CCIO_USAGE_ETH  = 5,
};

#define LIOT_SFDT_CCIO_FLAG_UNDEF   (0U)
#define LIOT_SFDT_CCIO_FLAG_IPOS    (1U)
#define LIOT_SFDT_CCIO_FLAG_SPITEST (5U)
#define LIOT_SFDT_CCIO_CID_DEFAULT  (1U)
#define LIOT_SFDT_CCIO_RBUF_SPI     (8U)
#define LIOT_SFDT_CCIO_ETH_MIFI     (0U)

/* CCIO device task creation bitmap. */
#define LIOT_SFDT_CCIO_TASK_NONE    (0x00U)
#define LIOT_SFDT_CCIO_TASK_RX      (0x01U)
#define LIOT_SFDT_CCIO_TASK_TX1     (0x02U)
#define LIOT_SFDT_CCIO_TASK_TX2     (0x04U)
#define LIOT_SFDT_CCIO_TASK_TX3     (0x08U)
#define LIOT_SFDT_CCIO_TASK_TX      (LIOT_SFDT_CCIO_TASK_TX1 | \
                                     LIOT_SFDT_CCIO_TASK_TX2 | \
                                     LIOT_SFDT_CCIO_TASK_TX3)

/*
 * Public CCIO/SPI-device configuration.
 *
 * This is deliberately an API-owned type.  The implementation converts it
 * to the platform's private device configuration before creating the CCIO
 * channel, so applications only depend on this public API.  The field names
 * and widths cover the caller-configurable channel options.
 */
typedef struct
{
    uint8_t  mainUsage;       /* logical channel type */
    uint8_t  bmCreateFlag;    /* RX/TX task creation bitmap */
    uint16_t rbufFlags : 4;   /* receive-buffer selection bitmap */
    uint16_t rsvdBits  : 12;
    uint32_t custFlags : 4;   /* customer flags */
    uint32_t custExtras : 16; /* customer extra data */
    uint32_t custEthType : 4; /* Ethernet type */
    uint32_t rsvdBits2 : 8;
} Liot_SfdtSpiDevConfig_t;

/* Pin fields use int to match Liot_GpioInitDirect. This changes the layout
 * from the former liot_gpio_e fields: rebuild libliot_sfdt and APP together. */
typedef struct
{
    liot_spi_config_s spi;
    uint8_t at_uart_num;             /* LN882H AT UART number (0..3) when enabled */
    uint32_t bus_speed_hz;           /* 0: use spi.spiclk; allows arbitrary SFDT rate */
    uint32_t default_tx_value;

    int srdy_pin;                   /* module physical pin, NOT chip GPIO/PAD number */

    Liot_SfdtMrdySource_e mrdy_source;
    int mrdy_pin;                   /* module physical pin; used when mrdy_source == GPIO */
    liot_wakeuppad_e mrdy_wakeup_pad;
    int32_t mrdy_irq;                /* used for Wakeup; ignored for GPIO */
    liot_intsig_e mrdy_edge;
    liot_gpio_pull_mode_e mrdy_pull;

    uint8_t  task_priority;
    uint32_t handshake_timeout_ms;
    uint16_t frame_mtu;
    uint16_t task_stack_size;
    uint16_t tx_poll_interval_ms;
    uint8_t  device_index;

    Liot_SfdtSpiDevConfig_t spi_dev; /* CCIO channel/device options */

    uint8_t at_uart_enable;          /* 0: do not call SetAtUart; 1: configure AT UART */
    Liot_SfdtDataMode_e data_mode;   /* CCIO or application passthrough */
    Liot_SfdtRxCallback rx_callback; /* required by passthrough mode */
    void *rx_callback_arg;            /* opaque application context */
    uint8_t tx_queue_depth;           /* passthrough queue slots, 1..MAX_TX_QUEUE */
} Liot_SfdtConfig_t;

/**
 * @brief Fill an SFDT configuration with platform defaults.
 *
 * The board SRDY PAD is converted through the selected module's pin map;
 * srdy_pin is LIOT_SFDT_INVALID_PIN if unmapped. The application must select
 * a valid module pin before calling Liot_SfdtInit(). MRDY defaults to WAKEUP;
 * set mrdy_pin explicitly when selecting GPIO.
 *
 * @param[out] cfg Receives the default configuration; must not be NULL.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtGetDefaultConfig(Liot_SfdtConfig_t *cfg);

/**
 * @brief Initialize the SFDT transport and its worker task.
 *
 * Added as the public entry point for configuring the SPI/SFDT transport.
 * Call after obtaining defaults and selecting valid board/module pins.
 *
 * @param[in] cfg Transport configuration; must remain valid for this call.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtInit(const Liot_SfdtConfig_t *cfg);

/**
 * @brief Stop and release the SFDT transport resources.
 *
 * Use when the application is finished with the transport. In passthrough
 * mode, queued TX data and the receive callback are also released/reset.
 *
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtDeinit(void);

/**
 * @brief Notify the SFDT worker that transmit data may be available.
 *
 * Added for transport integrations that receive a TX-ready event outside the
 * SFDT worker. The worker then checks the active data path and services TX.
 *
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtNotifyTxReady(void);

/**
 * @brief Set local SFDT flow control to XON or XOFF.
 *
 * Added so an application can pause or resume transmission in passthrough or
 * other integrated data paths. This argument is a boolean-style selector, not
 * a bit mask: pass 0 to send XON/resume, or 1 to send XOFF/pause. The current
 * implementation treats any nonzero value as XOFF; callers should use only
 * 0 or 1.
 *
 * @param[in] xoff 0 for XON/resume, 1 for XOFF/pause.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtSetFlowControl(uint8_t xoff);

/**
 * @brief Rebind the existing CCIO device to the requested logical channel.
 *
 * The request is serialized by the SFDT worker and returns after the worker
 * has completed the transform. cid is used only by the MIFI_AP path and must
 * be a valid PDP CID (1..15); WIFI_STA always uses the wifi_cli channel.
 *
 * @param[in] mode Logical OPAQ channel to bind.
 * @param[in] cid  PDP CID for MIFI_AP; ignored for WIFI_STA.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtBindChannel(Liot_SfdtChannelMode_e mode, uint8_t cid);

/**
 * @brief Read the last committed logical channel and PDP CID.
 *
 * Added so callers can verify the channel selected by Liot_SfdtBindChannel().
 *
 * @param[out] mode Receives the committed channel; must not be NULL.
 * @param[out] cid  Receives the committed PDP CID, or INVALID_CID for Wi-Fi;
 *                  must not be NULL.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtGetChannel(Liot_SfdtChannelMode_e *mode, uint8_t *cid);

/**
 * @brief Queue one payload for transmission in passthrough mode.
 *
 * The payload is copied before this function returns.  The caller may reuse
 * or release its input buffer immediately.  One queued payload maps to one
 * SFDT frame; callers must keep len <= frame_mtu - SFDT frame header size.
 *
 * Added for applications that exchange raw payloads without routing packets
 * through the CCIO/lwIP data path.
 *
 * @param[in] data Payload to copy into the TX queue; must not be NULL.
 * @param[in] len  Payload length in bytes; must be nonzero and fit one frame.
 * @return LIOT_SFDT_TRANSPORT_SUCCESS on success, otherwise an error code.
 */
int32_t Liot_SfdtSend(const uint8_t *data, uint16_t len);

/* Wi-Fi STA/WAN switching API.  These declarations share the same public
 * header so callers do not need to know which SFDT data path owns the SPI
 * transport. */
#define LIOT_SFDT_API_VERSION 1U
#define LIOT_SFDT_MAX_DNS 4U
#define LIOT_SFDT_MIN_CID 1U
#define LIOT_SFDT_MAX_CID 15U
#define LIOT_SFDT_INVALID_CID 0xffU

typedef void *Liot_SfdtWanHandle_t;

typedef enum {
    LIOT_SFDT_WAN_CELLULAR = 0,
    LIOT_SFDT_WAN_WIFI = 1
} Liot_SfdtWanType_e;

typedef enum {
    LIOT_SFDT_WAN_SUCCESS = 0,
    LIOT_SFDT_WAN_INVALID_PARAM_ERR,
    LIOT_SFDT_WAN_NOT_INIT_ERR,
    LIOT_SFDT_WAN_BUSY_ERR,
    LIOT_SFDT_WAN_QUEUE_FULL_ERR,
    LIOT_SFDT_WAN_LINK_FAIL_ERR,
    LIOT_SFDT_WAN_ROUTE_FAIL_ERR,
    LIOT_SFDT_WAN_UNSUPPORTED_ERR,
    /* A failed switch could not restore the previous channel/route. The
     * service stays in this state until a later channel switch succeeds. */
    LIOT_SFDT_WAN_RECOVERY_FAIL_ERR
} Liot_SfdtWanErr_e;

typedef enum {
    LIOT_SFDT_IP_NONE = 0,
    LIOT_SFDT_IP_V4 = 1,
    LIOT_SFDT_IP_V6 = 2,
    LIOT_SFDT_IP_V4V6 = 3
} Liot_SfdtIpType_e;

typedef enum {
    LIOT_SFDT_ADDR_NONE = 0,
    LIOT_SFDT_ADDR_V4 = 4,
    LIOT_SFDT_ADDR_V6 = 6
} Liot_SfdtAddrType_e;

typedef struct {
    uint8_t type;
    uint8_t address[16];
} Liot_SfdtIpAddress_t;

typedef struct {
    Liot_SfdtIpType_e ip_type;
    uint16_t mtu;
    uint8_t mtu_present;
    uint8_t ipv4_netmask_present;
    Liot_SfdtIpAddress_t ipv4_address;
    Liot_SfdtIpAddress_t ipv4_netmask;
    uint8_t ipv4_gateway_present;
    Liot_SfdtIpAddress_t ipv4_gateway;
    uint8_t mac_present;
    uint8_t mac[6];
    Liot_SfdtIpAddress_t ipv6_link_local;
    Liot_SfdtIpAddress_t ipv6_global;
    uint8_t dns_count;
    Liot_SfdtIpAddress_t dns[LIOT_SFDT_MAX_DNS];
} Liot_SfdtIpConfig_t;

typedef void (*Liot_SfdtWanSwitchCallback_t)(
    Liot_SfdtWanErr_e result,
    Liot_SfdtWanType_e wan_type,
    void *user_data);

/**
 * @brief Initialize the singleton WAN switch service.
 *
 * Added to manage Wi-Fi STA/Cat1 channel switching and egress selection using
 * the shared SFDT transport. Repeated calls return the same handle. The initial
 * state is read from the transport's committed OPAQ channel.
 *
 * @return Service handle on success, or NULL if initialization fails.
 */
Liot_SfdtWanHandle_t Liot_SfdtWanInit(void);

/**
 * @brief Queue an asynchronous switch to the Wi-Fi STA WAN.
 *
 * The configuration is copied before this function returns. A successful
 * return means the request was queued, not that the switch has completed.
 *
 * @param[in] handle   Handle returned by Liot_SfdtWanInit().
 * @param[in] config   Wi-Fi netif configuration to apply.
 * @param[in] callback Optional completion callback; may be NULL.
 * @param[in] user_data Caller context passed to callback; may be NULL.
 * @return Request result: success when queued, otherwise an error code.
 */
Liot_SfdtWanErr_e Liot_SfdtWanSwitchToWifi(
    Liot_SfdtWanHandle_t handle,
    const Liot_SfdtIpConfig_t *config,
    Liot_SfdtWanSwitchCallback_t callback,
    void *user_data);

/**
 * @brief Queue an asynchronous switch to the Cat1 WAN and AP/MIFI channel.
 *
 * A successful return means the request was queued, not that the switch has
 * completed.
 *
 * @param[in] handle   Handle returned by Liot_SfdtWanInit().
 * @param[in] cid      Cat1 PDP CID; valid range is LIOT_SFDT_MIN_CID through
 *                     LIOT_SFDT_MAX_CID.
 * @param[in] callback Optional completion callback; may be NULL.
 * @param[in] user_data Caller context passed to callback; may be NULL.
 * @return Request result: success when queued, otherwise an error code.
 */
Liot_SfdtWanErr_e Liot_SfdtWanSwitchToCellular(
    Liot_SfdtWanHandle_t handle,
    uint8_t cid,
    Liot_SfdtWanSwitchCallback_t callback,
    void *user_data);

/**
 * @brief Move only the lwIP default route between an existing Wi-Fi STA netif
 *        and an existing Cat1 PDP netif.
 *
 * Unlike Liot_SfdtWanSwitchToWifi()/Liot_SfdtWanSwitchToCellular(), this call
 * does not rebind the OPAQ channel and does not create or remove any netif.
 * The LN882H therefore stays in STA mode and wifi_cli stays alive, so both
 * netifs remain usable at the same time: unbound sockets follow the default
 * route, while sockets bound with lwip_bind_cid() keep reaching their CID.
 *
 * This is synchronous and runs the route change on the tcpip task, so it must
 * not be called from an interrupt context.
 *
 * @param[in] handle  Handle returned by Liot_SfdtWanInit().
 * @param[in] type    Egress to activate: Wi-Fi STA or Cat1 cellular.
 * @param[in] cid     PDP CID for LIOT_SFDT_WAN_CELLULAR; ignored for Wi-Fi.
 * @return Result of the route change.
 * @retval LIOT_SFDT_WAN_SUCCESS        Default route now points at the target.
 * @retval LIOT_SFDT_WAN_INVALID_PARAM_ERR  Unknown type or out-of-range CID.
 * @retval LIOT_SFDT_WAN_NOT_INIT_ERR   Service not initialized or closing.
 * @retval LIOT_SFDT_WAN_BUSY_ERR       A queued channel switch is in progress.
 * @retval LIOT_SFDT_WAN_ROUTE_FAIL_ERR Target netif is absent or inactive.
 *
 * @note The target netif must already exist. For cellular, activate the PDP
 *       context first; for Wi-Fi, complete Liot_SfdtWanSwitchToWifi() first.
 */
Liot_SfdtWanErr_e Liot_SfdtWanSetEgress(
    Liot_SfdtWanHandle_t handle,
    Liot_SfdtWanType_e type,
    uint8_t cid);

/**
 * @brief Read the committed WAN/channel state.
 *
 * Added so applications can inspect the channel binding independently from
 * the lwIP default-route egress returned by Liot_SfdtWanGetEgress().
 *
 * @param[in] handle    Handle returned by Liot_SfdtWanInit().
 * @param[out] wan_type Receives the committed channel WAN type; not NULL.
 * @param[out] cid      Receives the associated PDP CID, or INVALID_CID for
 *                      Wi-Fi; not NULL.
 * @return Query result, or an error code if unavailable.
 */
Liot_SfdtWanErr_e Liot_SfdtWanGetState(
    Liot_SfdtWanHandle_t handle,
    Liot_SfdtWanType_e *wan_type,
    uint8_t *cid);

/**
 * @brief Read which netif currently holds the lwIP default route.
 *
 * Added to distinguish the active packet egress from the OPAQ channel
 * reported by Liot_SfdtWanGetState().
 *
 * @param[in] handle Handle returned by Liot_SfdtWanInit().
 * @param[out] type  Receives the egress WAN type; not NULL.
 * @param[out] cid   Receives the PDP CID for cellular or INVALID_CID for Wi-Fi;
 *                   not NULL.
 * @return Query result, or an error code if unavailable.
 */
Liot_SfdtWanErr_e Liot_SfdtWanGetEgress(
    Liot_SfdtWanHandle_t handle,
    Liot_SfdtWanType_e *type,
    uint8_t *cid);

/**
 * @brief Diagnose the lwIP route selected for an IPv4 destination.
 *
 * Added for Wi-Fi/Cat1 route integration testing. The function reports the
 * Wi-Fi netif/default-route selection and returns whether Wi-Fi is selected
 * for the requested target.
 *
 * @param[in] stage       Label included in diagnostic output; must not be NULL.
 * @param[in] target_ipv4 IPv4 destination in dotted-decimal form.
 * @return LIOT_SFDT_WAN_SUCCESS when the route diagnostics pass, otherwise an
 *         error code.
 */
Liot_SfdtWanErr_e Liot_SfdtWanDumpNetifState(
    const char *stage,
    const char *target_ipv4);

/**
 * @brief Stop the WAN switch service and release resources it owns.
 *
 * If Wi-Fi is active, this first restores the last Cat1 CID and removes
 * wifi_cli. A transport created by this service is then released; a
 * pre-existing shared transport remains alive in AP/MIFI mode.
 *
 * @param[in] handle Handle returned by Liot_SfdtWanInit().
 * @return LIOT_SFDT_WAN_SUCCESS on success, otherwise an error code.
 */
Liot_SfdtWanErr_e Liot_SfdtWanDeinit(Liot_SfdtWanHandle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __LIOT_SFDT_H__ */
