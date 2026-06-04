/**
 * @file liot_external_flash_fs_demo.c
 * @brief LIoT External Flash File System Demo Application
 * @author ljz email:ciot_iot_support@lierda.com
 * @date 2026-05-27
 * @version 1.0
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 */
/**
 * This demonstration application showcases the functionality of the LIoT external
 * flash file system (LittleFS) by implementing file creation, read/write operations,
 * directory management, and cleanup. It provides practical examples of SPI flash
 * initialization, file system mounting, and standard file I/O operations.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gpio2.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"

#define FILE_DATA_LENGTH     (580)               /**< Test file write data length */
#define TEST_DATA            "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz" \
                             "abcdefghijklmnopqrstuvwxyz"

#define FLASH_DIR            "/flash"             /**< File system root directory */
#define TEST_FILENAME        FLASH_DIR "/test.file" /**< Test file full path */

#define SPI_MOSI_PAD        (63)            /**< SPI MOSI pin number */
#define SPI_MISO_PAD        (62)            /**< SPI MISO pin number */
#define SPI_SCLK_PAD        (49)            /**< SPI SCLK pin number */
#define SPI_SSN0_PAD        (64)            /**< SPI SSN0 pin number */
#define SPI_SSN0_GPIO       (L_GPIO_12)     /**< SPI SSN0 pin number */
#define SPI_FUNC            (1)             /**< SPI pin function selection */

#define FLASH_PWR_PAD       (16)            /**< Flash power supply pin */
#define FLASH_PWR_GPIO      (L_GPIO_25)     /**< Flash power supply gpio */

/**
 * @brief External flash hardware configuration
 */
static liot_ext_flash_cfg_t flash_cfg = {
    .spi_port   = 1,                /**< SPI port number */
    .base_addr  = 0x00,   /**< Flash file system start address */
    .total_size = 0x200000,  /**< Flash file system total size */
};

/**
 * @brief LittleFS file system configuration
 */
static liot_ext_fs_cfg_t fs_cfg = {
    .base_addr  = 0,                /**< File system base address offset */
    .total_size = 0x200000,         /**< File system total size */
    .block_size = 4096,             /**< Erase block size in bytes */
    .read_size  = 256,              /**< Minimum read unit in bytes */
    .prog_size  = 256,              /**< Minimum program unit in bytes */
};

/**
 * @brief Initialize external flash SPI pins and power supply
 *
 * Configure AON power domain, GPIO voltage, flash power pin, and SPI pins (MOSI/MISO/SCLK/CS)
 */
static void liot_flash_io_init(void)
{
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    /* Power on external flash */
    Liot_SetPinFunc(FLASH_PWR_PAD, L_PIN_FUNC_0);
    Liot_GpioInit(FLASH_PWR_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL);

    /* Initialize external storage */
    // Configure SPI0 GPIO pins
    Liot_SetPinFunc(SPI_MOSI_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_MISO_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_SCLK_PAD, SPI_FUNC);
    Liot_SetPinFunc(SPI_SSN0_PAD, L_PIN_FUNC_0);

    /* CS */
    Liot_GpioInit(SPI_SSN0_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL);
}

/**
 * @brief External flash file system demo thread
 *
 * Demonstrates the complete workflow of the external flash file system:
 * 1. Initialize SPI flash hardware
 * 2. Mount LittleFS file system
 * 3. Create directory
 * 4. Create file and write test data
 * 5. Read file and verify data integrity
 * 6. List directory contents
 * 7. Clean up files and directories
 * 8. Unmount file system
 *
 * @param[in] argv Thread argument (not used)
 */
void liot_extflash_fs_demo_thread(void *argv)
{
    int ret;
    LFILE_EXT fp;
    LDIR_EXT *dir;
    ldirent_ext *entry;
    char buffer[1024] = {0};

    // Wait for system to stabilize before initializing peripherals
    liot_rtos_task_sleep_ms(2000);

    /* Step 1: Initialize flash hardware (SPI pins + power) */
    liot_flash_io_init();
    ret = liot_flash_init_ext(&flash_cfg);
    if (ret != 0) {
        liot_trace("ext flash init error: %d\n", ret);
        liot_rtos_task_delete(NULL);
    }
    liot_trace("ext flash init ok\n");

    /* Step 2: Mount LittleFS file system */
    ret = liot_finit_ext(&fs_cfg);
    if (ret != LIOT_EXTFLASH_OK) {
        liot_trace("ext fs init error: %d\n", ret);
        liot_flash_deinit_ext();
        liot_rtos_task_delete(NULL);
    }
    liot_trace("ext fs init ok\n");

    /* Step 3: Create directory */
    ret = liot_mkdir_ext(FLASH_DIR, 0);
    if (ret != LIOT_EXTFLASH_OK) {
        liot_trace("ext fs mkdir error: %d\n", ret);
    } else {
        liot_trace("ext fs mkdir: %s ok\n", FLASH_DIR);
    }

    /* Step 4: Create and open file (read/write mode) */
    fp = liot_fopen_ext(TEST_FILENAME, "w+");
    if (fp <= 0) {
        liot_trace("ext fs fopen error: %d\n", (int)fp);
        goto cleanup;
    }
    liot_trace("ext fs fopen: %s ok\n", TEST_FILENAME);

    /* Step 5: Write test data */
    ret = liot_fwrite_ext((void *)TEST_DATA, FILE_DATA_LENGTH, 1, fp);
    if (ret != FILE_DATA_LENGTH) {
        liot_trace("ext fs fwrite error: %d\n", ret);
    } else {
        liot_trace("ext fs fwrite: %d bytes ok\n", ret);
    }

    /* Step 6: Seek file pointer back to beginning for read verification */
    ret = liot_fseek_ext(fp, 0, LIOT_EXTFLASH_SEEK_SET);
    if (ret < 0) {
        liot_trace("ext fs fseek error: %d\n", ret);
    }

    /* Step 7: Read data and compare with written data for verification */
    ret = liot_fread_ext(buffer, FILE_DATA_LENGTH, 1, fp);
    if (ret != FILE_DATA_LENGTH) {
        liot_trace("ext fs fread error: %d\n", ret);
    } else {
        liot_trace("ext fs fread: %d bytes ok\n", ret);
        liot_trace("Read match: %d\n", memcmp(buffer, TEST_DATA, FILE_DATA_LENGTH) == 0);
    }

    /* Step 8: Close file */
    ret = liot_fclose_ext(fp);
    if (ret != LIOT_EXTFLASH_OK) {
        liot_trace("ext fs fclose error: %d\n", ret);
    } else {
        liot_trace("ext fs fclose ok\n");
    }

    /* Step 9: List all files and subdirectories in the directory */
    dir = liot_opendir_ext(FLASH_DIR);
    if (dir != NULL) {
        liot_trace("ext fs opendir: %s ok\n", FLASH_DIR);
        while ((entry = liot_readdir_ext(dir)) != NULL) {
            liot_trace("  name: %s, type: %d\n", entry->d_name, entry->d_type);
        }
        liot_closedir_ext(dir);
        liot_trace("ext fs closedir ok\n");
    }

    /* Step 10: Cleanup - remove test file and directory */
    ret = liot_remove_ext(TEST_FILENAME);
    if (ret == LIOT_EXTFLASH_OK) {
        liot_trace("ext fs remove: %s ok\n", TEST_FILENAME);
    }

    ret = liot_remove_ext(FLASH_DIR);
    if (ret == LIOT_EXTFLASH_OK) {
        liot_trace("ext fs remove dir: %s ok\n", FLASH_DIR);
    }

cleanup:
    /* Unmount file system and release flash resources */
    liot_fdeinit_ext();
    liot_flash_deinit_ext();
    liot_trace("ext fs demo done\n");
    liot_rtos_task_delete(NULL);
}
