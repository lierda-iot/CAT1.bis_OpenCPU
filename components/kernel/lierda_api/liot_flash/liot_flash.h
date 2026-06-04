/**
 * @file liot_flash.h
 * @brief This file defines functions and types related to flash operations.
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef LIOT_FLASH_H
#define LIOT_FLASH_H

/*----------------------------------------------------------------------------*
 *                    INCLUDES                                                *
 *----------------------------------------------------------------------------*/
#include "liot_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*
 *                   DATA TYPE DEFINITION                                     *
 *----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*
 *                    FUNCTIONS DECLEARATION                           		  *
 *----------------------------------------------------------------------------*/
/**
 * @brief Erase the data stored in the flash.
 * 
 * @param SectorAddress The starting address of the erase area, must be a multiple of 0x1000 and within the user flash area.
 * @param size The length to be erased.
 * @return uint32_t The operation result, 0 indicates success, other values indicate failure.
 */
extern uint32_t liot_flash_erase(uint32_t SectorAddress, uint32_t size);

/**
 * @brief Read data from the flash.
 * 
 * @param pData The buffer used to store the data read from the flash.
 * @param ReadAddr The starting address of the data to be read, must be a multiple of 0x1000 and within the user flash area.
 * @param Size The length to be read.
 * @return uint32_t The operation result, 0 indicates success, other values indicate failure.
 */
extern uint32_t liot_flash_read(uint8_t *pData, uint32_t ReadAddr, uint32_t Size);

/**
 * @brief Write data to the flash.
 * 
 * @param pData The starting address of the data to be written.
 * @param WriteAddr The starting address where the data will be stored, must be a multiple of 0x1000 and within the user flash area.
 * @param Size The length to be written.
 * @return uint32_t The operation result, 0 indicates success, other values indicate failure.
 */
extern uint32_t liot_flash_write(uint8_t *pData, uint32_t WriteAddr, uint32_t Size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif