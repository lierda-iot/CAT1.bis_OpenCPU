/**
 * @file liot_i2c.h
 * @brief Header file for I2C bus operation interface
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef _LIOT_I2C_H_
#define _LIOT_I2C_H_

#include "liot_api_common.h"
#include "liot_type.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Macro Definition
===========================================================================*/
/**
 * @def LIOT_I2C_ERRCODE_BASE
 * @brief Base address of I2C module error codes (obtained by shifting the component ID left by 16 bits)
 */
#define LIOT_I2C_ERRCODE_BASE (LIOT_COMPONENT_BSP_I2C << 16)

/*===========================================================================
 * Enum
===========================================================================*/
/**
 * @enum liot_i2c_channel_e
 * @brief Enumeration type for I2C channels
 */
typedef enum {
    liot_i2c_1 = 0,  /**< I2C channel 1 */
    liot_i2c_2,      /**< I2C channel 2 */
    liot_i2c_3,      /**< I2C channel 3 (not supported) */
} liot_i2c_channel_e;

/**
 * @enum liot_i2c_mode_e
 * @brief Enumeration type for I2C communication speed modes
 */
typedef enum {
    LIOT_STANDARD_MODE = 0,  /**< Standard mode (100Kbps) */
    LIOT_FAST_MODE     = 1,  /**< Fast mode (400Kbps) */
} liot_i2c_mode_e;

/**
 * @enum liot_errcode_i2c_e
 * @brief Enumeration type for I2C module error codes
 */
typedef enum {
    LIOT_I2C_SUCCESS = LIOT_SUCCESS,  /**< Operation succeeded */

    LIOT_I2C_INIT_ERR = 1 | LIOT_I2C_ERRCODE_BASE,  /**< Initialization failed */
    LIOT_I2C_NOT_INIT_ERR,                          /**< Uninitialized error */
    LIOT_I2C_INVALID_PARAM_ERR,                      /**< Invalid parameter error */

    LIOT_I2C_WRITE_ERR = 5 | LIOT_I2C_ERRCODE_BASE,  /**< Write operation failed */
    LIOT_I2C_READ_ERR,                               /**< Read operation failed */
    LIOT_I2C_RELEASE_ERR,                            /**< Resource release failed */
} liot_errcode_i2c_e;



/*===========================================================================
 * Function
===========================================================================*/

/**
 * @brief Get the CMSIS I2C driver interface for the specified channel
 * @param i2cNum [in] I2C channel number (0-based)
 * @return Pointer to ARM_DRIVER_I2C, or NULL if invalid channel
 */
void* __Liot_I2cGetInterface(int8_t i2cNum);

/**
 * @brief Initialize the I2C master
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param Mode [in] I2C communication speed mode (enumeration value of liot_i2c_mode_e)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Initialization succeeded
 *         - LIOT_I2C_INIT_ERR: Initialization failed
 */
liot_errcode_i2c_e liot_I2cInit(liot_i2c_channel_e i2c_no, liot_i2c_mode_e Mode);

/**
 * @brief I2C master write operation (8-bit register address)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param slave [in] I2C slave address (7-bit or 10-bit address)
 * @param addr [in] I2C slave register address (8-bit)
 * @param data [in] Pointer to the buffer of data to be sent
 * @param length [in] Length of data to be sent (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Write operation succeeded
 *         - LIOT_I2C_WRITE_ERR: Write operation failed
 */
liot_errcode_i2c_e liot_I2cWrite(
    liot_i2c_channel_e i2c_no, uint8_t slave, uint8_t addr, uint8_t *data, uint32_t length);

/**
 * @brief I2C master read operation (8-bit register address)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param slave [in] I2C slave address (7-bit or 10-bit address)
 * @param addr [in] I2C slave register address (8-bit)
 * @param buf [out] Pointer to the buffer for received data
 * @param length [in] Length of data to be received (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Read operation succeeded
 *         - LIOT_I2C_READ_ERR: Read operation failed
 */
liot_errcode_i2c_e liot_I2cRead(
    liot_i2c_channel_e i2c_no, uint8_t slave, uint8_t addr, uint8_t *buf, uint32_t length);

/**
 * @brief Release I2C master resources
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Resource release succeeded
 *         - LIOT_I2C_RELEASE_ERR: Resource release failed
 */
liot_errcode_i2c_e liot_I2cRelease(liot_i2c_channel_e i2c_no);

/**
 * @brief I2C master write operation (16-bit register address)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param slave [in] I2C slave address (7-bit or 10-bit address)
 * @param addr [in] I2C slave register address (16-bit)
 * @param data [in] Pointer to the buffer of data to be sent
 * @param length [in] Length of data to be sent (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Write operation succeeded
 *         - LIOT_I2C_WRITE_ERR: Write operation failed
 */
liot_errcode_i2c_e liot_I2cWrite_16bit_addr(
    liot_i2c_channel_e i2c_no, uint8_t slave, uint16_t addr, uint8_t *data, uint32_t length);

/**
 * @brief I2C master read operation (16-bit register address)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param slave [in] I2C slave address (7-bit or 10-bit address)
 * @param addr [in] I2C slave register address (16-bit)
 * @param buf [out] Pointer to the buffer for received data
 * @param length [in] Length of data to be received (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Read operation succeeded
 *         - LIOT_I2C_READ_ERR: Read operation failed
 */
liot_errcode_i2c_e liot_I2cRead_16bit_addr(
    liot_i2c_channel_e i2c_no, uint8_t slave, uint16_t addr, uint8_t *buf, uint32_t length);

/**
 * @brief I2C master write operation (compatible with 8-bit and 16-bit register addresses)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param devAddr [in] I2C slave address (7-bit or 10-bit address)
 * @param reg [in] I2C slave register address (8-bit or 16-bit)
 * @param addr16 [in] Address width flag (false: 8-bit address, true: 16-bit address)
 * @param data [in] Pointer to the buffer of data to be sent
 * @param len [in] Length of data to be sent (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Write operation succeeded
 *         - LIOT_I2C_WRITE_ERR: Write operation failed
 */
liot_errcode_i2c_e Liot_I2cWriteReg(
    liot_i2c_channel_e i2c_no, uint8_t devAddr, uint16_t reg, BOOL addr16, const uint8_t *data, uint32_t len);

/**
 * @brief I2C master read operation (compatible with 8-bit and 16-bit register addresses)
 *
 * @param i2c_no [in] I2C channel (enumeration value of liot_i2c_channel_e)
 * @param devAddr [in] I2C slave address (7-bit or 10-bit address)
 * @param reg [in] I2C slave register address (8-bit or 16-bit)
 * @param addr16 [in] Address width flag (false: 8-bit address, true: 16-bit address)
 * @param data [out] Pointer to the buffer for received data
 * @param len [in] Length of data to be received (in bytes)
 *
 * @return liot_errcode_i2c_e Operation result
 *         - LIOT_I2C_SUCCESS: Read operation succeeded
 *         - LIOT_I2C_READ_ERR: Read operation failed
 */
liot_errcode_i2c_e Liot_I2cReadReg(
    liot_i2c_channel_e i2c_no, uint8_t devAddr, uint16_t reg, BOOL addr16, uint8_t *data, uint32_t len);

/**
 * @brief I2C master write: send raw command bytes followed by data bytes
 *
 * @param i2c_no    [in] I2C channel
 * @param slave     [in] I2C slave address (7-bit)
 * @param cmd       [in] Command byte array
 * @param cmd_len   [in] Length of command bytes
 * @param data      [in] Data byte array (may be NULL if data_len == 0)
 * @param data_len  [in] Length of data bytes
 * @return liot_errcode_i2c_e
 */
liot_errcode_i2c_e liot_I2cWrite_RawCmd(
    liot_i2c_channel_e i2c_no, uint8_t slave,
    const uint8_t *cmd, uint16_t cmd_len,
    const uint8_t *data, uint16_t data_len);

/**
 * @brief I2C master read: send raw command bytes then receive data bytes
 *
 * @param i2c_no    [in] I2C channel
 * @param slave     [in] I2C slave address (7-bit)
 * @param cmd       [in] Command byte array
 * @param cmd_len   [in] Length of command bytes
 * @param data      [out] Buffer for received data
 * @param data_len  [in] Number of bytes to receive
 * @return liot_errcode_i2c_e
 */
liot_errcode_i2c_e liot_I2cRead_RawCmd(
    liot_i2c_channel_e i2c_no, uint8_t slave,
    const uint8_t *cmd, uint16_t cmd_len,
    uint8_t *data, uint16_t data_len);

/**
 * @brief I2C master write with multi-byte register address
 *
 * @param i2c_no     [in] I2C channel
 * @param slave      [in] I2C slave address (7-bit)
 * @param reg        [in] Register address (up to 16-bit)
 * @param addr_bytes [in] Number of address bytes (1 or 2)
 * @param data       [in] Data to write
 * @param len        [in] Length of data
 * @return liot_errcode_i2c_e
 */
liot_errcode_i2c_e liot_I2cWrite_MultiByteAddr(
    liot_i2c_channel_e i2c_no, uint8_t slave,
    uint16_t reg, uint8_t addr_bytes,
    const uint8_t *data, uint16_t len);

/**
 * @brief I2C master read with multi-byte register address
 *
 * @param i2c_no     [in] I2C channel
 * @param slave      [in] I2C slave address (7-bit)
 * @param reg        [in] Register address (up to 16-bit)
 * @param addr_bytes [in] Number of address bytes (1 or 2)
 * @param data       [out] Buffer for received data
 * @param len        [in] Number of bytes to receive
 * @return liot_errcode_i2c_e
 */
liot_errcode_i2c_e liot_I2cRead_MultiByteAddr(
    liot_i2c_channel_e i2c_no, uint8_t slave,
    uint16_t reg, uint8_t addr_bytes,
    uint8_t *data, uint16_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIOT_I2C_H */