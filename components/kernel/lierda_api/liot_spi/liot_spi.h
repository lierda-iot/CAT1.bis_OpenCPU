/**
 * @file liot_spi.h
 * @brief SPI bus related interface definitions
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef LIOT_SPI_H
#define LIOT_SPI_H

#include "liot_api_common.h"
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif
// #include "liot_gpio.h"

/*========================================================================
 *  Variable Definition
 *========================================================================*/

/**
 * @enum liot_errcode_spi_e
 * @brief SPI bus error code enumeration
 */
typedef enum
{
    LIOT_SPI_SET_CB_ERR = -1,
    LIOT_SPI_SUCCESS    = 0,

    LIOT_SPI_ERROR = 1 | (LIOT_COMPONENT_BSP_SPI << 16), // Other SPI bus errors
    LIOT_SPI_PARAM_TYPE_ERROR,                           // Parameter type error
    LIOT_SPI_PARAM_DATA_ERROR,                           // Parameter data error
    LIOT_SPI_PARAM_ACQUIRE_ERROR,                        // Parameter cannot be acquired
    LIOT_SPI_PARAM_NULL_ERROR,                           // Parameter NULL error
    LIOT_SPI_DEV_NOT_ACQUIRE_ERROR,                      // Cannot acquire SPI bus
    LIOT_SPI_PARAM_LENGTH_ERROR,                         // Parameter length error
    LIOT_SPI_MALLOC_MEM_ERROR,                           // Memory allocation error
    LIOT_SPI_ADDR_ALIGNED_ERROR,                         // Address is not 4-byte aligned
    LIOT_SPI_MUTEX_CREATE_ERROR,                         // Mutex creation failed
    LIOT_SPI_MUTEX_LOCK_ERROR,                           // Mutex lock timeout error
    LIOT_SPI_UNKNOWN_ERROR,
} liot_errcode_spi_e;

/**
 * @enum liot_spi_cs_sel_e
 * @brief SPI chip select pin selection enumeration
 */
typedef enum
{
    LIOT_SPI_CS0 = 0, // Select cs0 as SPI chip select CS pin
    LIOT_SPI_CS1,     // Select cs1 as SPI chip select CS pin
    LIOT_SPI_CS2,     // Select cs2 as SPI chip select CS pin, not used now
    LIOT_SPI_CS3,     // Select cs3 as SPI chip select CS pin, not used now
    LIOT_SPI_CS_NULL, // Do not use chip select, user can control externally
} liot_spi_cs_sel_e;

/**
 * @enum liot_spi_input_mode_e
 * @brief SPI input mode enumeration
 */
typedef enum
{
    LIOT_SPI_INPUT_FALSE, // SPI input (read) not allowed
    LIOT_SPI_INPUT_TRUE,  // SPI input (read) allowed
} liot_spi_input_mode_e;

/**
 * @enum liot_spi_port_e
 * @brief SPI port selection enumeration
 */
typedef enum
{
    LIOT_SPI_PORT0 = 0, // SPI0 bus
    LIOT_SPI_PORT1,     // SPI1 bus
} liot_spi_port_e;

/**
 * @enum liot_spi_cs_pol_e
 * @brief SPI chip select polarity enumeration
 */
typedef enum
{
    LIOT_SPI_CS_ACTIVE_HIGH, // CS pin is high during SPI bus operation
    LIOT_SPI_CS_ACTIVE_LOW,  // CS pin is low during SPI bus operation
} liot_spi_cs_pol_e;

/**
 * @enum liot_spi_cpol_pol_e
 * @brief SPI clock polarity enumeration (CPOL)
 */
typedef enum
{
    LIOT_SPI_CPOL_LOW = 0, // CLK line is low when SPI is disabled, first edge is rising
    LIOT_SPI_CPOL_HIGH,    // CLK line is high when SPI is disabled, first edge is falling
} liot_spi_cpol_pol_e;

/**
 * @enum liot_spi_cpha_pol_e
 * @brief SPI clock phase enumeration (CPHA)
 */
typedef enum
{
    LIOT_SPI_CPHA_1Edge, // MOSI delayed by one edge, CLK and MISO delayed by two edges (data ready before CLK)
    LIOT_SPI_CPHA_2Edge, // MOSI delayed by two edges, CLK delayed by two edges, MISO delayed by three edges (data and CLK ready simultaneously)
} liot_spi_cpha_pol_e;

// SPI mode0: liot_spi_cpol_pol_e selects LIOT_SPI_CPOL_LOW, liot_spi_cpha_pol_e selects LIOT_SPI_CPHA_1Edge
// SPI mode1: liot_spi_cpol_pol_e selects LIOT_SPI_CPOL_LOW, liot_spi_cpha_pol_e selects LIOT_SPI_CPHA_2Edge
// SPI mode2: liot_spi_cpol_pol_e selects LIOT_SPI_CPOL_HIGH, liot_spi_cpha_pol_e selects LIOT_SPI_CPHA_1Edge
// SPI mode3: liot_spi_cpol_pol_e selects LIOT_SPI_CPOL_HIGH, liot_spi_cpha_pol_e selects LIOT_SPI_CPHA_2Edge

/**
 * @enum liot_spi_input_sel_e
 * @brief SPI input pin selection enumeration
 */
typedef enum
{
    LIOT_SPI_DI_0 = 0, // Select DI0 as data input pin, not used now
    LIOT_SPI_DI_1,     // Select DI1 as data input pin
    LIOT_SPI_DI_2,     // Select DI2 as data input pin, not used now
} liot_spi_input_sel_e;

/**
 * @enum liot_spi_transfer_mode_e
 * @brief SPI transfer mode enumeration
 */
typedef enum
{
    LIOT_SPI_DIRECT_POLLING = 0, // FIFO read/write, polling wait
    LIOT_SPI_DIRECT_IRQ,         // FIFO read/write, interrupt notification, not used now
    LIOT_SPI_DMA_IRQ,            // DMA read/write, interrupt notification, not used now
} liot_spi_transfer_mode_e;

// Transfer rate, 100M frequency division, starting from 2 divisions.
/**
 * @enum liot_spi_clk_e
 * @brief SPI clock rate enumeration
 */
typedef enum
{
    LIOT_SPI_CLK_INVALID  = -1,       // Invalid clock selection
    LIOT_SPI_CLK_812_5KHZ = 812500,   // Clock: 812.5K
    LIOT_SPI_CLK_1_625MHZ = 1625000,  // Clock: 1.625M
    LIOT_SPI_CLK_3_25MHZ  = 3250000,  // Clock: 3.125M
    LIOT_SPI_CLK_6_5MHZ   = 6500000,  // Clock: 6.5M
    LIOT_SPI_CLK_13MHZ    = 13000000, // Clock: 13M
} liot_spi_clk_e;

/**
 * @enum liot_spi_clk_delay_e
 * @brief SPI clock delay enumeration
 */
typedef enum
{
    LIOT_SPI_CLK_DELAY_0 = 0, // No delay, default state
    LIOT_SPI_CLK_DELAY_1,     // MISO delay by one edge sampling
} liot_spi_clk_delay_e;

/**
 * @enum liot_spi_device_mode_e
 * @brief SPI device mode enumeration
 */
typedef enum
{
    LIOT_SPI_DEVICE_MODE_INVALID = 0,
    LIOT_SPI_DEVICE_MODE_MASTER,            // SPI master mode, full duplex
    LIOT_SPI_DEVICE_MODE_SLAVE,             // SPI slave mode, full duplex
    LIOT_SPI_DEVICE_MODE_MASTER_SIMPLEX,    // SPI master mode, half duplex, shared MOSI for transmit/receive
    LIOT_SPI_DEVICE_MODE_SLAVE_SIMPLEX,     // SPI slave mode, half duplex, shared MISO for transmit/receive
} liot_spi_device_mode_e;

/**
 * @enum liot_spi_data_msb_lsb_e
 * @brief SPI data bit order enumeration
 */
typedef enum
{
    LIOT_SPI_DATA_MSB_LSB = 0,  // MSB first
    LIOT_SPI_DATA_LSB_MSB,      // LSB first
} liot_spi_data_msb_lsb_e;

/**
 * @enum liot_spi_event_e
 * @brief SPI Transfer event enumeration
 */
typedef enum
{
    LIOT_SPI_EVENT_TRANSFER_COMPLETE = (1UL << 0),  // Data Transfer completed
    LIOT_SPI_EVENT_DATA_LOST    = (1UL << 1),       // Data lost: Receive overflow / Transmit underflow
    LIOT_SPI_EVENT_MODE_FAULT   = (1UL << 2),       // Master Mode Fault (SS deactivated when Master)
    LIOT_SPI_EVENT_RX_TIMEOUT   = (1UL << 3),       // Receive timeout
} liot_spi_event_e;

/**
 * @brief SPI interrupt callback function type
 * @param event Interrupt event type
 */
typedef void (*liot_spi_irq_callback)(uint32_t event);

/**
 * @struct liot_spi_config_s
 * @brief SPI bus configuration structure
 */
typedef struct
{
    liot_spi_input_mode_e input_mode;
    liot_spi_port_e port;
    unsigned int framesize;
    liot_spi_clk_e spiclk;
    liot_spi_cs_pol_e cs_polarity0;
    liot_spi_cs_pol_e cs_polarity1;
    liot_spi_cpol_pol_e cpol;
    liot_spi_cpha_pol_e cpha;
    liot_spi_input_sel_e input_sel;
    liot_spi_transfer_mode_e transmode;
    liot_spi_cs_sel_e cs;
    liot_spi_clk_delay_e clk_delay;
    liot_spi_device_mode_e device_mode;
    liot_spi_data_msb_lsb_e data_msb_lsb;
    liot_spi_irq_callback irq_callback;
} liot_spi_config_s;

/**
 * @enum liot_spi_threshold_e
 * @brief SPI interrupt trigger threshold enumeration
 */
typedef enum
{
    LIOT_SPI_TRIGGER_1_DATA,  // FIFO has 1 byte, trigger interrupt
    LIOT_SPI_TRIGGER_4_DATA,  // FIFO has 4 bytes, trigger interrupt
    LIOT_SPI_TRIGGER_8_DATA,  // FIFO has 8 bytes, trigger interrupt
    LIOT_SPI_TRIGGER_12_DATA, // FIFO has 12 bytes, trigger interrupt
} liot_spi_threshold_e;

/**
 * @struct liot_spi_irq_s
 * @brief SPI interrupt status structure
 */
typedef struct
{
    unsigned int rx_ovf : 1;
    unsigned int tx_th : 1;
    unsigned int tx_dma_done : 1;
    unsigned int rx_th : 1;
    unsigned int rx_dma_done : 1;
    liot_spi_threshold_e tx_threshold;
    liot_spi_threshold_e rx_threshold;
} liot_spi_irq_s;

/*========================================================================
 *  function Definition
 *========================================================================*/

/**
 * @brief Initialize the SPI bus
 * @param port SPI bus selection (liot_spi_port_e enumeration value)
 * @param transmode SPI bus transfer mode (only LIOT_SPI_DIRECT_POLLING mode is supported)
 * @param spiclk SPI bus transfer rate configuration (liot_spi_clk_e enumeration value)
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_init(liot_spi_port_e port,
                                        liot_spi_transfer_mode_e transmode,
                                        liot_spi_clk_e spiclk);

/**
 * @brief Extended initialization of SPI bus (supports more configuration options)
 * @param spi_config SPI bus configuration structure (liot_spi_config_s type)
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_init_ext(liot_spi_config_s spi_config);

/**
 * @brief Synchronous read/write SPI data (using the same clock)
 * 
 * liot_spi_cs_low, liot_spi_cs_high, or liot_spi_cs_auto should be used to control the CS pin before and after calling this interface
 * 
 * @param port SPI bus selection (liot_spi_port_e enumeration value)
 * @param[in] inbuf Receive data buffer (read data will be stored here)
 * @param[out] outbuf Transmit data buffer (data to be sent is stored here)
 * @param len Data length (same length for read and write)
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_write_read(liot_spi_port_e port,
                                              unsigned char *inbuf,
                                              unsigned char *outbuf,
                                              unsigned int len);

/**
 * @brief Perform read operation on SPI bus
 *        liot_spi_cs_low, liot_spi_cs_high, or liot_spi_cs_auto should be used to 
 *        control the CS pin before and after calling this interface
 * @param port SPI bus selection (liot_spi_port_e enumeration value)
 * @param[out] buf Buffer for read data (stores read results)
 * @param len Length of data to read
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_read(liot_spi_port_e port, unsigned char *buf, unsigned int len);

/**
 * @brief Perform write operation on SPI bus
 *        liot_spi_cs_low, liot_spi_cs_high, or liot_spi_cs_auto should be 
 *        used to control the CS pin before and after calling this interface
 * @param port SPI bus selection (liot_spi_port_e enumeration value)
 * @param[in] buf Buffer for write data (data to be sent is stored here)
 * @param len Length of data to write
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_write(liot_spi_port_e port, unsigned char *buf, unsigned int len);

/**
 * @brief Release the SPI bus
 * @param port SPI bus selection (liot_spi_port_e enumeration value)
 * @return liot_errcode_spi_e Operation result error code
 */
extern liot_errcode_spi_e liot_spi_release(liot_spi_port_e port);

#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LIOT_SPI_H */