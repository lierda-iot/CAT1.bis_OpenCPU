/**
 * @file demo_nvm.c
 * @brief Demonstration example of NVRAM read/write operations.
 * @author L email:ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 *
 */

/**
 * 1. Read (12 + 32) bytes from NVRAM to check if data already exists
 * 2. If all 0xFF (no data), write two pieces of data at different offsets
 * 3. Read back and verify consistency
 *
 * Note: The NVRAM reserved partition is 2KB in size and cannot be erased by the user.
 *       NVRAM has limited write endurance. Avoid frequent writes.
 *       Write only during initialization or configuration changes,
 *       and use Liot_nvmRead for subsequent accesses.
 */

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_nv.h"
#include "liot_os.h"
#include "liot_type.h"

#define DEMO_NVM_DATA1_OFFSET   0
#define DEMO_NVM_DATA1_SIZE     12

#define DEMO_NVM_DATA2_OFFSET   12
#define DEMO_NVM_DATA2_SIZE     32

#define DEMO_NVM_TOTAL_SIZE     (DEMO_NVM_DATA1_SIZE + DEMO_NVM_DATA2_SIZE)

static BOOL demo_nvm_check_empty(UINT8 *buf, UINT32 len)
{
    for (UINT32 i = 0; i < len; i++)
    {
        if (buf[i] != 0xFF && buf[i] != 0x00)
        {
            return false;
        }
    }
    return true;
}

void demo_nvm_task(void *argv)
{
    UINT8 readBuf[DEMO_NVM_TOTAL_SIZE] = {0};

    liot_rtos_task_sleep_s(5);

    BOOL ret = Liot_nvmRead(readBuf, DEMO_NVM_TOTAL_SIZE, DEMO_NVM_DATA1_OFFSET);
    if (ret != true)
    {
        liot_trace("Liot_nvmRead check failed\r\n");
        goto exit;
    }

    if (demo_nvm_check_empty(readBuf, DEMO_NVM_TOTAL_SIZE))
    {
        liot_trace("NVM is empty, writing two data segments...\r\n");

        UINT8 writeData1[DEMO_NVM_DATA1_SIZE] = {0};
        UINT8 writeData2[DEMO_NVM_DATA2_SIZE] = {0};

        for (int i = 0; i < DEMO_NVM_DATA1_SIZE; i++)
            writeData1[i] = (UINT8)(0xA0 + i);

        for (int i = 0; i < DEMO_NVM_DATA2_SIZE; i++)
            writeData2[i] = (UINT8)(0xB0 + i);

        ret = Liot_nvmWrite(writeData1, DEMO_NVM_DATA1_SIZE, DEMO_NVM_DATA1_OFFSET);
        if (ret != true)
        {
            liot_trace("Liot_nvmWrite data1 failed\r\n");
            goto exit;
        }
        liot_trace("Write data1 OK: %d bytes at offset %d\r\n", DEMO_NVM_DATA1_SIZE, DEMO_NVM_DATA1_OFFSET);

        ret = Liot_nvmWrite(writeData2, DEMO_NVM_DATA2_SIZE, DEMO_NVM_DATA2_OFFSET);
        if (ret != true)
        {
            liot_trace("Liot_nvmWrite data2 failed\r\n");
            goto exit;
        }
        liot_trace("Write data2 OK: %d bytes at offset %d\r\n", DEMO_NVM_DATA2_SIZE, DEMO_NVM_DATA2_OFFSET);

        UINT8 verifyBuf1[DEMO_NVM_DATA1_SIZE] = {0};
        UINT8 verifyBuf2[DEMO_NVM_DATA2_SIZE] = {0};

        ret = Liot_nvmRead(verifyBuf1, DEMO_NVM_DATA1_SIZE, DEMO_NVM_DATA1_OFFSET);
        if (ret != true)
        {
            liot_trace("Liot_nvmRead data1 verify failed\r\n");
            goto exit;
        }

        ret = Liot_nvmRead(verifyBuf2, DEMO_NVM_DATA2_SIZE, DEMO_NVM_DATA2_OFFSET);
        if (ret != true)
        {
            liot_trace("Liot_nvmRead data2 verify failed\r\n");
            goto exit;
        }

        liot_trace("data1 verify: ");
        if (memcmp(verifyBuf1, writeData1, DEMO_NVM_DATA1_SIZE) == 0)
        {
            liot_trace("OK\r\n");
        }
        else
        {
            liot_trace("FAILED, read: ");
            for (int i = 0; i < DEMO_NVM_DATA1_SIZE; i++)
                liot_trace("%02X ", verifyBuf1[i]);
        }

        liot_trace("data2 verify: ");
        if (memcmp(verifyBuf2, writeData2, DEMO_NVM_DATA2_SIZE) == 0)
        {
            liot_trace("OK\r\n");
        }
        else
        {
            liot_trace("FAILED, read: ");
            for (int i = 0; i < DEMO_NVM_DATA2_SIZE; i++)
                liot_trace("%02X ", verifyBuf2[i]);
        }
    }
    else
    {
        liot_trace("NVM already has data, skip write\r\n");
        liot_trace("data1[%d bytes at offset %d]: ", DEMO_NVM_DATA1_SIZE, DEMO_NVM_DATA1_OFFSET);
        for (int i = 0; i < DEMO_NVM_DATA1_SIZE; i++)
            liot_trace("%02X ", readBuf[i]);

        liot_trace("data2[%d bytes at offset %d]: ", DEMO_NVM_DATA2_SIZE, DEMO_NVM_DATA2_OFFSET);
        for (int i = DEMO_NVM_DATA1_SIZE; i < DEMO_NVM_TOTAL_SIZE; i++)
            liot_trace("%02X ", readBuf[i]);
    }

exit:
    liot_trace("nvm demo done\n");
    liot_rtos_task_delete(NULL);
}

