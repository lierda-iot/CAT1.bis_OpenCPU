/**
 * @file liot_uart2.h
 * @brief This header file defines macros, enumerations, structures, 
 *        and function interfaces related to UART.
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
*/

#ifndef _LIOT_UART2_H_
#define _LIOT_UART2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_type.h"

/**
 * @enum liot_uart_errcode_e
 * @brief Enumeration of UART error codes.
 */
typedef enum
{
    L_UART_SUCCESS     = 0,
    L_UART_ERR_EXECUTE,
    L_UART_ERR_ADDR_NULL,
    L_UART_ERR_INVALID_PARAM,
    L_UART_ERR_OPEN_REPEAT,
    L_UART_ERR_NOT_OPEN,
} liot_uart_err_e;

/**
 * @enum liot_uart_port_number_e
 * @brief Enumeration of UART port numbers.
 */
typedef enum
{
    L_PORT_NONE = -1,
    L_UART0,
    L_UART1,
    L_UART2,
    L_UART3,
    L_USBCOM,
    L_PORT_MAX,
} liot_uart_e;

/**
 * @enum liot_uart_flowctrl_e
 * @brief Enumeration of UART flow control modes.
 */
typedef enum
{
    L_UART_FC_NONE = 0,
    L_UART_FC_HW,
} liot_uart_flowctrl_e;

/**
 * @enum liot_uart_baud_e
 * @brief Enumeration of UART baud rates.
 */
typedef enum
{
    L_UART_BR_AUTO   = 0,
    L_UART_BR_600    = 600,
    L_UART_BR_1200   = 1200,
    L_UART_BR_2400   = 2400,
    L_UART_BR_4800   = 4800,
    L_UART_BR_9600   = 9600,
    L_UART_BR_14400  = 14400,
    L_UART_BR_19200  = 19200,
    L_UART_BR_28800  = 28800,
    L_UART_BR_38400  = 38400,
    L_UART_BR_57600  = 57600,
    L_UART_BR_115200 = 115200,
    L_UART_BR_230400 = 230400,
    L_UART_BR_460800 = 460800,
    L_UART_BR_921600 = 921600,
} liot_uart_baudrate_e;

/**
 * @enum liot_uart_databit_e
 * @brief Enumeration of UART data bits.
 */
typedef enum
{
    L_UART_DATA_7 = 7,
    L_UART_DATA_8 = 8,
} liot_uart_databit_e;

/**
 * @enum liot_uart_stopbit_e
 * @brief Enumeration of UART stop bits.
 */
typedef enum
{
    L_UART_STOP_1 = 1,
    L_UART_STOP_2 = 2,
} liot_uart_stopbit_e;

/**
 * @enum liot_uart_paritybit_e
 * @brief Enumeration of UART parity bits.
 */
typedef enum
{
    L_UART_PARITY_NONE,
    L_UART_PARITY_ODD,
    L_UART_PARITY_EVEN,
} liot_uart_paritybit_e;

/**
 * @enum liot_uart_event_e
 * @brief Enumeration of UART events.
 */
typedef enum
{
    L_UART_EVENT_RX_ARRIVED  = (1 << 0), ///< Received new data
    L_UART_EVENT_RX_OVERFLOW = (1 << 1), ///< Rx fifo overflowed
    L_UART_EVENT_TX_COMPLETE = (1 << 2)  ///< All data had been sent
} liot_uart_event_e;

/**
 * @enum liot_uart_tx_way_e
 * @brief Enumeration of UART data transmission methods.
 */
typedef enum
{
    L_UART_TX_OPAQ       = 0,
    L_UART_TX_DRIVER,
    L_UART_TX_DRIVER_DMA
} liot_uart_txway_e;

/**
 * @struct Liot_UartConfig_t
 * @brief UART configuration structure.
 */
typedef struct
{
    liot_uart_baudrate_e baudrate;    ///< UART baud rate
    liot_uart_databit_e data_bit;      ///< UART data bits
    liot_uart_stopbit_e stop_bit;      ///< UART stop bits
    liot_uart_paritybit_e parity_bit;  ///< UART parity bit
    liot_uart_flowctrl_e flow_ctrl;    ///< UART flow control mode
    liot_uart_txway_e tx_way;          ///< UART transmission method
    bool cts_enable;                   ///< enable cts or not, not supported now
    bool rts_enable;                   ///< enable rts or not, not supported now
    uint32_t rx_buf_size;              ///< rx buffer size, not supported now
    uint32_t tx_buf_size;              ///< tx buffer size, not supported now
    bool lpuart_enable;                ///< is lpuart or not, not supported now
}Liot_UartConfig_t;

/**
 * @typedef L_UartCallback_f
 * @brief UART callback function type for receiving data.
 * @param port UART port number
 * @param data Pointer to received data
 * @param size Size of received data
 * @param argc User-defined argument
 */
typedef void (*L_UartCallback_f)( liot_uart_e port, char *data, uint32_t size, void *argc);

/**
 * @brief Initialize UART port with specified configuration.
 * @param port UART port number
 * @param uart_config UART configuration structure
 * @param uart_cb Callback function for receiving data
 * @param argc User-defined argument for callback
 * @return L_UART_SUCCESS if success, otherwise error code
 */
liot_uart_err_e Liot_UartInit(liot_uart_e port, Liot_UartConfig_t *uart_config, L_UartCallback_f uart_cb, void *argc);

/**
 * @brief Deinitialize UART port.
 * @param port UART port number
 * @return L_UART_SUCCESS if success, otherwise error code
 */
liot_uart_err_e Liot_UartDeinit(liot_uart_e port);

/**
 * @brief Send data through UART port.
 * @param port UART port number
 * @param data Pointer to data to send
 * @param data_len Length of data to send
 * @return Number of bytes sent
 */
uint32_t Liot_UartSend(liot_uart_e port, unsigned char *data, unsigned int data_len);

#ifdef __cplusplus
}
#endif
#endif