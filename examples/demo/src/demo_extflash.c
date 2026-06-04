/**
 * @file liot_external_flash_demo.c
 * @brief LIoT External Flash Raw Read/Write Demo Application
 * @author ljz email:ciot_iot_support@lierda.com
 * @date 2026-05-29
 * @version 1.0
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 */
/**
 * This demonstration application showcases raw data read/write operations
 * on external SPI flash without a file system. It demonstrates direct
 * erase, write, and read operations using absolute flash offsets.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_external_flash.h"

#define EXT_FLASH_BASE_ADDR     (0x00)          /**< External flash base address */
#define EXT_FLASH_TOTAL_SIZE    (0x400000)      /**< External flash total size (4MB) */

#define FLASH_BLOCK_SIZE        (4096)          /**< Flash erase block size (4KB) */
#define TEST_DATA_OFFSET        (0)             /**< Test data write offset */
#define TEST_DATA_SIZE          (256)           /**< Test data size in bytes */

#define SPI_MOSI_PAD            (63)
#define SPI_MISO_PAD            (62)
#define SPI_SCLK_PAD            (49)
#define SPI_SSN0_PAD            (64)
#define SPI_SSN0_GPIO           (L_GPIO_12)
#define SPI_FUNC                (1)

#define FLASH_PWR_PAD           (16)
#define FLASH_PWR_GPIO          (L_GPIO_25)

/**
 * @brief External flash hardware configuration
 */
static liot_ext_flash_cfg_t flash_cfg = {
    .spi_port   = 1,
    .base_addr  = EXT_FLASH_BASE_ADDR,
    .total_size = EXT_FLASH_TOTAL_SIZE,
};

/**
 * @brief Initialize external flash SPI pins and power supply
 */
static void liot_flash_io_init(void)
{
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

    Liot_SetPinFunc(FLASH_PWR_PAD, L_PIN_FUNC_0);
    Liot_GpioInit(FLASH_PWR_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL);

    Liot_SetPinFunc(SPI_MOSI_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_MISO_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_SCLK_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_SSN0_PAD, L_PIN_FUNC_0);
    Liot_GpioInit(SPI_SSN0_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL);
}

/**
 * @brief External flash raw read/write demo thread
 *
 * Demonstrates raw flash operations without file system:
 * 1. Initialize SPI flash hardware
 * 2. Erase a flash block (must erase before writing)
 * 3. Write raw data to flash
 * 4. Read back and verify data integrity
 * 5. Demonstrate partial block update (read-modify-write)
 *
 * @param[in] argv Thread argument (not used)
 */
void liot_extflash_demo_thread(void *argv)
{
    uint32_t ret;
    uint8_t write_buf[TEST_DATA_SIZE];
    uint8_t read_buf[TEST_DATA_SIZE];

    liot_rtos_task_sleep_ms(2000);

    /* Step 1: Initialize flash hardware */
    liot_flash_io_init();
    ret = liot_flash_init_ext(&flash_cfg);
    if (ret != 0) {
        liot_trace("ext flash init error: %d\n", ret);
        liot_rtos_task_delete(NULL);
        return;
    }
    liot_trace("ext flash init ok\n");

    /* Step 2: Erase one block before writing (NOR flash requirement) */
    ret = liot_flash_erase_ext(TEST_DATA_OFFSET, FLASH_BLOCK_SIZE);
    if (ret != 0) {
        liot_trace("ext flash erase error\n");
        goto cleanup;
    }
    liot_trace("ext flash erase ok, offset=0x%X, size=0x%X\n",
              TEST_DATA_OFFSET, FLASH_BLOCK_SIZE);

    /* Step 3: Prepare and write test data */
    for (uint32_t i = 0; i < TEST_DATA_SIZE; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    ret = liot_flash_write_ext(write_buf, TEST_DATA_OFFSET, TEST_DATA_SIZE);
    if (ret != 0) {
        liot_trace("ext flash write error\n");
        goto cleanup;
    }
    liot_trace("ext flash write ok, %d bytes at offset 0x%X\n",
              TEST_DATA_SIZE, TEST_DATA_OFFSET);

    /* Step 4: Read back and verify */
    memset(read_buf, 0, sizeof(read_buf));
    ret = liot_flash_read_ext(read_buf, TEST_DATA_OFFSET, TEST_DATA_SIZE);
    if (ret != 0) {
        liot_trace("ext flash read error\n");
        goto cleanup;
    }

    if (memcmp(write_buf, read_buf, TEST_DATA_SIZE) == 0) {
        liot_trace("ext flash verify ok, data match\n");
    } else {
        liot_trace("ext flash verify failed, data mismatch!\n");
    }

    /* Step 5: Partial block update (read-modify-write pattern)
     * When updating part of a block, you must:
     *   a) Read the entire block into RAM
     *   b) Modify the desired portion
     *   c) Erase the block
     *   d) Write the entire block back
     */
    uint8_t *block_buf = (uint8_t *)malloc(FLASH_BLOCK_SIZE);
    if (block_buf != NULL) {
        /* a) Read entire block */
        ret = liot_flash_read_ext(block_buf, TEST_DATA_OFFSET, FLASH_BLOCK_SIZE);
        if (ret != 0) {
            liot_trace("ext flash block read error\n");
            free(block_buf);
            goto cleanup;
        }

        /* b) Modify bytes 100~103 */
        block_buf[100] = 0xAA;
        block_buf[101] = 0xBB;
        block_buf[102] = 0xCC;
        block_buf[103] = 0xDD;

        /* c) Erase block */
        ret = liot_flash_erase_ext(TEST_DATA_OFFSET, FLASH_BLOCK_SIZE);
        if (ret != 0) {
            liot_trace("ext flash block erase error\n");
            free(block_buf);
            goto cleanup;
        }

        /* d) Write entire block back */
        ret = liot_flash_write_ext(block_buf, TEST_DATA_OFFSET, FLASH_BLOCK_SIZE);
        if (ret != 0) {
            liot_trace("ext flash block write error\n");
            free(block_buf);
            goto cleanup;
        }

        liot_trace("ext flash partial update ok (read-modify-write)\n");
        free(block_buf);
    } else {
        liot_trace("ext flash malloc failed, skip partial update\n");
    }

cleanup:
    liot_flash_deinit_ext();
    liot_trace("ext flash raw demo done\n");
    liot_rtos_task_delete(NULL);
}
