/**
 * @file liot_cspi.h
 * @brief CSPI (Camera Serial Peripheral Interface) API Header File
 * @details This file defines error codes and port enumeration types for CSPI interface
 */

#ifndef _LIOT_CSPI_H_
#define _LIOT_CSPI_H_

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief CSPI error code enumeration
 * @details Defines all possible error codes returned by CSPI operations
 */
typedef enum {
    LIOT_CSPI_SUCCESS = 0,        /**< @brief Operation succeeded */
    LIOT_CSPI_INIT_ERR,           /**< @brief Initialization failed */
    LIOT_CSPI_NOT_INIT_ERR,       /**< @brief Uninitialized error */
    LIOT_CSPI_INVALID_PARAM_ERR,  /**< @brief Invalid parameter error */
    LIOT_CSPI_RELEASE_ERR,        /**< @brief Resource release failed */
    LIOT_CSPI_TIMEOUT,            /**< @brief Wait timeout */
    LIOT_CSPI_RECV_ERROR          /**< @brief Receive data timeout */
} liot_errcode_cspi_e;

/**
 * @brief CSPI port enumeration
 * @details Defines available CSPI port numbers
 */
typedef enum
{
    LIOT_CSPI_PORT0 = 0,    /**< @brief CSPI port 0 */
    LIOT_CSPI_PORT1,        /**< @brief CSPI port 1 */
    LIOT_CSPI_PORT2,        /**< @brief CSPI port 2 */
    LIOT_CSPI_PORTMAX       /**< @brief Total number of CSPI ports */
} liot_cspi_port_e;
#ifdef __cplusplus
}
#endif
#endif /* _LIOT_CSPI_H_ */
