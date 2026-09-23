#ifndef __LIOT_SFDT_SLAVE_H__
#define __LIOT_SFDT_SLAVE_H__

#include <stdint.h>
#include "liot_spi.h"
#include "liot_gpio2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LIOT_SFDT_SLAVE_SUCCESS = 0,
    LIOT_SFDT_SLAVE_INVALID_PARAM = -1,
    LIOT_SFDT_SLAVE_NOT_INITIALIZED = -2,
    LIOT_SFDT_SLAVE_ALREADY_INITIALIZED = -3,
    LIOT_SFDT_SLAVE_SPI_ERROR = -4,
    LIOT_SFDT_SLAVE_CCIO_ERROR = -5,
    LIOT_SFDT_SLAVE_OS_ERROR = -6,
    LIOT_SFDT_SLAVE_UNSUPPORTED = -7,
    LIOT_SFDT_SLAVE_TX_FULL = -8,
    LIOT_SFDT_SLAVE_TX_TOO_LARGE = -9,
} Liot_SfdtSlaveErr_e;

/**
 * @brief Select the data backend used by the SFDT worker.
 *
 * CCIO keeps the existing 4G/IPOS/ETH path. PASSTHROUGH bypasses CCIO and
 * reports valid payloads through rx_callback; reverse-direction data is
 * queued with Liot_SfdtSlaveSend().
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
 * LIOT_SFDT_SLAVE_SUCCESS when the payload was accepted; the return value is
 * reserved for application diagnostics and does not automatically change
 * flow control.
 */
typedef int32_t (*Liot_SfdtRxCallback)(const uint8_t *data,
                                       uint16_t len,
                                       void *arg);

/* Maximum number of packets retained by the API in passthrough TX mode. */
#define LIOT_SFDT_MAX_TX_QUEUE             (8U)

/**
 * @brief MRDY interrupt input source.
 *
 * Wakeup is kept for the original SFDT board wiring.  GPIO uses the GPIO2
 * API (Liot_GpioInit + liot_intcb_t) and is suitable for MRDY pins such as
 * EC718 GPIO17 that are not connected to a wakeup pad.
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

typedef struct
{
    liot_spi_config_s spi;
    uint8_t at_uart_num;             /* LN882H AT UART number (0..3) when enabled */
    uint32_t bus_speed_hz;           /* 0: use spi.spiclk; allows arbitrary SFDT rate */
    uint32_t default_tx_value;

    liot_gpio_e srdy_gpio;          /* GPIO2/module GPIO number; mapping is owned by liot_gpio2 */

    Liot_SfdtMrdySource_e mrdy_source;
    liot_gpio_e mrdy_gpio;           /* used when mrdy_source == GPIO */
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
} Liot_SfdtSlaveConfig_t;

int32_t Liot_SfdtSlaveGetDefaultConfig(Liot_SfdtSlaveConfig_t *cfg);
int32_t Liot_SfdtSlaveInit(const Liot_SfdtSlaveConfig_t *cfg);
int32_t Liot_SfdtSlaveDeinit(void);
int32_t Liot_SfdtSlaveNotifyTxReady(void);
int32_t Liot_SfdtSlaveSetFlowControl(uint8_t xoff);
/**
 * @brief Queue one payload for transmission in passthrough mode.
 *
 * The payload is copied before this function returns.  The caller may reuse
 * or release its input buffer immediately.  One queued payload maps to one
 * SFDT frame; callers must keep len <= frame_mtu - SFDT frame header size.
 */
int32_t Liot_SfdtSlaveSend(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __LIOT_SFDT_SLAVE_H__ */
