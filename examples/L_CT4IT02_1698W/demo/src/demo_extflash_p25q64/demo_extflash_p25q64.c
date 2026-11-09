/**
 * @file demo_extflash_p25q64.c
 * @brief Basic raw external flash demo for L_CT4IT02_1698W with onboard P25Q64
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_external_flash.h"
#include "liot_os.h"

#include "demo_extflash_p25q64_core.h"
#include "demo_extflash_p25q64_mgr.h"

#define DEMO_LOG_PREFIX              "[p25q64_raw]"
#define DEMO_FLASH_NAME              "P25Q64_8MB"

#define DEMO_BOOT_DELAY_MS           (2000U)

#define DEMO_FLASH_SPI_PORT          (1U)
#define DEMO_FLASH_BASE_ADDR         (0x000000U)
#define DEMO_FLASH_TOTAL_SIZE        (0x800000U)
#define DEMO_FLASH_BLOCK_SIZE        (4096U)

#define DEMO_TEST_OFFSET             (0x000000U)
#define DEMO_TEST_SIZE               (256U)
#define DEMO_RMW_OFFSET              (0x001000U)

static const liot_ext_flash_cfg_t g_demo_flash_cfg = {
    .spi_port = DEMO_FLASH_SPI_PORT,
    .base_addr = DEMO_FLASH_BASE_ADDR,
    .total_size = DEMO_FLASH_TOTAL_SIZE,
};

static uint32_t demo_tick_now(void)
{
    return liot_rtos_get_system_tick();
}

static uint32_t demo_tick_elapsed(uint32_t start_tick)
{
    return demo_extflash_p25q64_core_tick_elapsed(demo_tick_now(), start_tick);
}

void liot_extflash_p25q64_demo_thread(void *argv)
{
    uint8_t write_buf[DEMO_TEST_SIZE];
    uint8_t read_buf[DEMO_TEST_SIZE];
    uint8_t *rmw_buf = NULL;
    uint8_t *verify_buf = NULL;
    uint32_t tick_total_start;
    uint32_t tick_step_start;
    uint32_t erase_ticks = 0U;
    uint32_t write_ticks = 0U;
    uint32_t read_ticks = 0U;
    uint32_t verify_ticks = 0U;
    uint32_t rmw_read_ticks = 0U;
    uint32_t rmw_erase_ticks = 0U;
    uint32_t rmw_write_ticks = 0U;
    uint32_t rmw_verify_ticks = 0U;
    uint32_t total_ticks;
    int32_t ret;

    (void)argv;

    liot_rtos_task_sleep_ms(DEMO_BOOT_DELAY_MS);
    liot_trace("%s demo start board=L_CT4IT02_1698W flash=%s total=0x%08lX\n",
               DEMO_LOG_PREFIX,
               DEMO_FLASH_NAME,
               (unsigned long)DEMO_FLASH_TOTAL_SIZE);

    ret = demo_extflash_p25q64_mgr_init(&g_demo_flash_cfg,
                                        DEMO_EXTFLASH_MGR_OWNER_RAW);
    if (ret != 0)
    {
        liot_trace("%s flash lifecycle init failed ret=%ld\n", DEMO_LOG_PREFIX, (long)ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    ret = demo_extflash_p25q64_mgr_lock();
    if (ret != 0)
    {
        liot_trace("%s access lock failed ret=%ld\n", DEMO_LOG_PREFIX, (long)ret);
        (void)demo_extflash_p25q64_mgr_deinit(DEMO_EXTFLASH_MGR_OWNER_RAW);
        liot_rtos_task_delete(NULL);
        return;
    }

    liot_trace("%s flash init ok spi=%u base=0x%06lX total=0x%08lX\n",
               DEMO_LOG_PREFIX,
               (unsigned int)g_demo_flash_cfg.spi_port,
               (unsigned long)g_demo_flash_cfg.base_addr,
               (unsigned long)g_demo_flash_cfg.total_size);

    tick_total_start = demo_tick_now();
    demo_extflash_p25q64_core_fill_pattern(write_buf, DEMO_TEST_SIZE, 0x31U);

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_erase_ext(DEMO_TEST_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    erase_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s erase failed offset=0x%06lX size=%lu ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_TEST_OFFSET,
                   (unsigned long)DEMO_FLASH_BLOCK_SIZE,
                   (long)ret,
                   (unsigned long)erase_ticks);
        goto cleanup;
    }

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_write_ext(write_buf, DEMO_TEST_OFFSET, DEMO_TEST_SIZE);
    write_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s write failed offset=0x%06lX size=%lu ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_TEST_OFFSET,
                   (unsigned long)DEMO_TEST_SIZE,
                   (long)ret,
                   (unsigned long)write_ticks);
        goto cleanup;
    }

    memset(read_buf, 0x00, sizeof(read_buf));
    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_read_ext(read_buf, DEMO_TEST_OFFSET, DEMO_TEST_SIZE);
    read_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s read failed offset=0x%06lX size=%lu ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_TEST_OFFSET,
                   (unsigned long)DEMO_TEST_SIZE,
                   (long)ret,
                   (unsigned long)read_ticks);
        goto cleanup;
    }

    tick_step_start = demo_tick_now();
    ret = (memcmp(write_buf, read_buf, DEMO_TEST_SIZE) == 0) ? 0 : -1;
    verify_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s verify failed offset=0x%06lX size=%lu tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_TEST_OFFSET,
                   (unsigned long)DEMO_TEST_SIZE,
                   (unsigned long)verify_ticks);
        goto cleanup;
    }

    liot_trace("%s raw_basic pass offset=0x%06lX size=%lu erase_tick=%lu write_tick=%lu read_tick=%lu verify_tick=%lu tail=0x%02X\n",
               DEMO_LOG_PREFIX,
               (unsigned long)DEMO_TEST_OFFSET,
               (unsigned long)DEMO_TEST_SIZE,
               (unsigned long)erase_ticks,
               (unsigned long)write_ticks,
               (unsigned long)read_ticks,
               (unsigned long)verify_ticks,
               read_buf[DEMO_TEST_SIZE - 1U]);

    rmw_buf = (uint8_t *)malloc(DEMO_FLASH_BLOCK_SIZE);
    verify_buf = (uint8_t *)malloc(DEMO_FLASH_BLOCK_SIZE);
    if ((rmw_buf == NULL) || (verify_buf == NULL))
    {
        liot_trace("%s malloc failed for rmw buffer\n", DEMO_LOG_PREFIX);
        ret = -2;
        goto cleanup;
    }

    demo_extflash_p25q64_core_fill_pattern(rmw_buf, DEMO_FLASH_BLOCK_SIZE, 0x51U);

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_erase_ext(DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_erase_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw erase failed offset=0x%06lX ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret,
                   (unsigned long)rmw_erase_ticks);
        goto cleanup;
    }

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_write_ext(rmw_buf, DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_write_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw initial write failed offset=0x%06lX ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret,
                   (unsigned long)rmw_write_ticks);
        goto cleanup;
    }

    memset(verify_buf, 0x00, DEMO_FLASH_BLOCK_SIZE);
    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_read_ext(verify_buf, DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_read_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw initial read failed offset=0x%06lX ret=%ld tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret,
                   (unsigned long)rmw_read_ticks);
        goto cleanup;
    }

    if (memcmp(rmw_buf, verify_buf, DEMO_FLASH_BLOCK_SIZE) != 0)
    {
        liot_trace("%s rmw initial verify failed offset=0x%06lX\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET);
        ret = -3;
        goto cleanup;
    }

    rmw_buf[100] = 0xAAU;
    rmw_buf[101] = 0xBBU;
    rmw_buf[102] = 0xCCU;
    rmw_buf[103] = 0xDDU;

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_erase_ext(DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_erase_ticks += demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw second erase failed offset=0x%06lX ret=%ld\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret);
        goto cleanup;
    }

    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_write_ext(rmw_buf, DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_write_ticks += demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw second write failed offset=0x%06lX ret=%ld\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret);
        goto cleanup;
    }

    memset(verify_buf, 0x00, DEMO_FLASH_BLOCK_SIZE);
    tick_step_start = demo_tick_now();
    ret = (int32_t)liot_flash_read_ext(verify_buf, DEMO_RMW_OFFSET, DEMO_FLASH_BLOCK_SIZE);
    rmw_read_ticks += demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw second read failed offset=0x%06lX ret=%ld\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET,
                   (long)ret);
        goto cleanup;
    }

    tick_step_start = demo_tick_now();
    ret = (memcmp(rmw_buf, verify_buf, DEMO_FLASH_BLOCK_SIZE) == 0) ? 0 : -4;
    rmw_verify_ticks = demo_tick_elapsed(tick_step_start);
    if (ret != 0)
    {
        liot_trace("%s rmw verify failed offset=0x%06lX\n",
                   DEMO_LOG_PREFIX,
                   (unsigned long)DEMO_RMW_OFFSET);
        goto cleanup;
    }

    total_ticks = demo_tick_elapsed(tick_total_start);
    liot_trace("%s raw_rmw pass offset=0x%06lX size=%lu read_tick=%lu erase_tick=%lu write_tick=%lu verify_tick=%lu data=%02X %02X %02X %02X total_tick=%lu\n",
               DEMO_LOG_PREFIX,
               (unsigned long)DEMO_RMW_OFFSET,
               (unsigned long)DEMO_FLASH_BLOCK_SIZE,
               (unsigned long)rmw_read_ticks,
               (unsigned long)rmw_erase_ticks,
               (unsigned long)rmw_write_ticks,
               (unsigned long)rmw_verify_ticks,
               verify_buf[100],
               verify_buf[101],
               verify_buf[102],
               verify_buf[103],
               (unsigned long)total_ticks);

    ret = 0;

cleanup:
    if (ret != 0)
    {
        liot_trace("%s demo failed ret=%ld\n", DEMO_LOG_PREFIX, (long)ret);
    }

    if (rmw_buf != NULL)
    {
        free(rmw_buf);
    }
    if (verify_buf != NULL)
    {
        free(verify_buf);
    }

    demo_extflash_p25q64_mgr_unlock();
    (void)demo_extflash_p25q64_mgr_deinit(DEMO_EXTFLASH_MGR_OWNER_RAW);
    liot_trace("%s demo done\n", DEMO_LOG_PREFIX);
    liot_rtos_task_delete(NULL);
}
