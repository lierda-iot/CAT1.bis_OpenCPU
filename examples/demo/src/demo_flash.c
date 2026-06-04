/**
 * @file liot_flash_demo.c
 * @brief Demonstration example of user area flash operations.
 * @author L email:ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2023-09-08
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

/**
 * 1. Initialize flash driver
 * 2. Register flash callback function
 * 3. Erase flash data
 * 4. Write flash data
 * 5. Read flash data
 * 6. Verify flash data
 * 7. Unregister flash callback function
 * 8. Deinitialize flash driver
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_flash.h"
#include "liot_os.h"
#include "liot_type.h"
#include "mem_map.h"

// Partition header file is defined in mem_map_71xx.h. Please refer to and modify accordingly.
#ifndef FLASH_RESERVE_USER_REGION_START
#define FLASH_RESERVE_USER_REGION_START 0x003E4000
#endif

#ifndef FLASH_RESERVE_USER_REGION_SIZE
#define FLASH_RESERVE_USER_REGION_SIZE 0x001000
#endif

#ifndef FLASH_RESERVE_USER_REGION_END
#define FLASH_RESERVE_USER_REGION_END 0x003E5000
#endif

/**
 * @brief This project demonstrates the use of user area flash.
 *        In this case, we will erase a user area, write data to it,
 *        and then read the written data for printing.
 *        Non-user areas, such as the LIOT_BEFORE_APP_ADDR and LIOT_AFTER_APP_ADDR
 *        areas, cannot be erased, written, or read.
 */
#define LIOT_APP_ADDR_START FLASH_RESERVE_USER_REGION_START
#define LIOT_APP_ADDR_END   FLASH_RESERVE_USER_REGION_END
#define LIOT_APP_ADDR_SIZE  FLASH_RESERVE_USER_REGION_SIZE

/** 
 * @brief User area flash operation demonstration task.
 * @details Demonstration process: Erase user area -> Write test data -> Read and verify data.
 * @param argv Task parameters (not used).
 * @note Non-user areas (e.g., LIOT_BEFORE_APP_ADDR, LIOT_AFTER_APP_ADDR) are prohibited from operation.
 */
void liot_flash_demo_task(void *argv)
{
    const char write_buff[11] = "BBBBBBBBBB"; // Make write_buff constant
    char read_buff[11] = {0};
    int ret = 0;

    liot_rtos_task_sleep_ms(2000);

    liot_trace("========== Flash demo start ==========");
    liot_trace("=== Erase flash data====");
    ret = liot_flash_erase(LIOT_APP_ADDR_START, LIOT_APP_ADDR_SIZE);
    if (ret != 0) {
        liot_trace("!!! Flash erase failed, error code: %d !!!", ret);
        goto demo_end;  // Skip subsequent operations in case of error
    }
    liot_trace("=== Erase flash success, ret(%d) ====", ret);
    liot_rtos_task_sleep_ms(100);

    liot_trace("=== Write data to flash ====");
    ret = liot_flash_write((uint8_t *)write_buff, LIOT_APP_ADDR_START, sizeof(write_buff) - 1); // Use sizeof
    if (ret != 0) {
        liot_trace("!!! Flash write failed, error code: %d !!!", ret);
        goto demo_end;
    }
    liot_trace("=== Write flash success, write data[%s] ====", write_buff);
    liot_rtos_task_sleep_ms(100);

    liot_trace("=== Read data from flash ====");
    ret = liot_flash_read((uint8_t *)read_buff, LIOT_APP_ADDR_START, sizeof(read_buff) - 1); // Use sizeof
    if (ret != 0) {
        liot_trace("!!! Flash read failed, error code: %d !!!", ret);
        goto demo_end;
    }
    liot_trace("=== Read flash success, read data[%s] ===", read_buff);
    liot_rtos_task_sleep_ms(100);

demo_end:
    liot_trace("========== flash demo end ==========");
    liot_rtos_task_delete(NULL); // Kill itself
}
