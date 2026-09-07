/**
 * @file demo_extflash_fs_p25q64.c
 * @brief Basic LittleFS external flash demo for L_CT4IT02_1698W with onboard P25Q64
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"
#include "liot_os.h"

#define DEMO_LOG_PREFIX              "[p25q64_fs]"
#define DEMO_FLASH_NAME              "P25Q64_8MB"

#define DEMO_BOOT_DELAY_MS           (2000U)
#define DEMO_LDO33_STABLE_DELAY_MS   (10U)
#define DEMO_VCC3V3_STABLE_DELAY_MS  (20U)

#define DEMO_LDO33_EN_PAD            (106)
#define DEMO_LDO33_EN_GPIO           (L_GPIO_25)
#define DEMO_VCC3V3_EN_PAD           (16)
#define DEMO_VCC3V3_EN_GPIO          (L_GPIO_27)

#define DEMO_SPI_MOSI_PAD            (63)
#define DEMO_SPI_MISO_PAD            (62)
#define DEMO_SPI_SCLK_PAD            (49)
#define DEMO_SPI_CS_PAD              (64)
#define DEMO_SPI_CS_GPIO             (L_GPIO_12)
#define DEMO_SPI_PIN_FUNC            (L_PIN_FUNC_1)

#define DEMO_FLASH_SPI_PORT          (1U)
#define DEMO_FLASH_BASE_ADDR         (0x000000U)
#define DEMO_FLASH_TOTAL_SIZE        (0x800000U)

#define DEMO_FS_BASE_ADDR            (0x010000U)
#define DEMO_FS_TOTAL_SIZE           (0x200000U)
#define DEMO_FS_BLOCK_SIZE           (4096U)
#define DEMO_FS_READ_SIZE            (256U)
#define DEMO_FS_PROG_SIZE            (256U)

#define DEMO_TEST_DIR                "/flash"
#define DEMO_TEST_FILE               "/flash/p25q64_basic.bin"
#define DEMO_TEST_SIZE               (640U)

static const liot_ext_flash_cfg_t g_demo_flash_cfg = {
    .spi_port = DEMO_FLASH_SPI_PORT,
    .base_addr = DEMO_FLASH_BASE_ADDR,
    .total_size = DEMO_FLASH_TOTAL_SIZE,
};

static const liot_ext_fs_cfg_t g_demo_fs_cfg = {
    .base_addr = DEMO_FS_BASE_ADDR,
    .total_size = DEMO_FS_TOTAL_SIZE,
    .block_size = DEMO_FS_BLOCK_SIZE,
    .read_size = DEMO_FS_READ_SIZE,
    .prog_size = DEMO_FS_PROG_SIZE,
};

static int32_t demo_expect_gpio_ok(liot_gpioerr_e ret, const char *step)
{
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s failed: %ld\n", DEMO_LOG_PREFIX, step, (long)ret);
        return (int32_t)ret;
    }

    return 0;
}

static int32_t demo_power_gpio_enable(const char *name, int pad, liot_gpio_e gpio)
{
    liot_gpioerr_e ret;

    ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret == L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s enabled via direct gpio=%d\n",
                   DEMO_LOG_PREFIX,
                   name,
                   (int)gpio);
        return 0;
    }

    liot_trace("%s %s direct gpio init failed: %ld, fallback pad=%d\n",
               DEMO_LOG_PREFIX,
               name,
               (long)ret,
               pad);

    ret = Liot_SetPinFunc(pad, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s pinmux fallback failed: %ld\n",
                   DEMO_LOG_PREFIX,
                   name,
                   (long)ret);
        return (int32_t)ret;
    }

    return demo_expect_gpio_ok(Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL), name);
}

static uint32_t demo_tick_now(void)
{
    return liot_rtos_get_system_tick();
}

static uint32_t demo_tick_elapsed(uint32_t start_tick)
{
    return (uint32_t)(demo_tick_now() - start_tick);
}

static void demo_fill_pattern(uint8_t *buffer, uint32_t len, uint8_t seed)
{
    uint32_t i;

    for (i = 0; i < len; ++i)
    {
        buffer[i] = (uint8_t)(seed + (uint8_t)(i * 3U));
    }
}

static int32_t demo_flash_hw_init(void)
{
    int32_t ret;

    ret = demo_expect_gpio_ok(Liot_AonPowerCtl(true), "Liot_AonPowerCtl");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_expect_gpio_ok(Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V), "Liot_SetVoltage");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_power_gpio_enable("LDO33 enable", DEMO_LDO33_EN_PAD, DEMO_LDO33_EN_GPIO);
    if (ret != 0)
    {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_LDO33_STABLE_DELAY_MS);

    ret = demo_power_gpio_enable("VCC3V3 enable", DEMO_VCC3V3_EN_PAD, DEMO_VCC3V3_EN_GPIO);
    if (ret != 0)
    {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_VCC3V3_STABLE_DELAY_MS);

    ret = demo_expect_gpio_ok(Liot_SetPinFunc(DEMO_SPI_MOSI_PAD, DEMO_SPI_PIN_FUNC), "SPI MOSI pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_expect_gpio_ok(Liot_SetPinFunc(DEMO_SPI_MISO_PAD, DEMO_SPI_PIN_FUNC), "SPI MISO pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_expect_gpio_ok(Liot_SetPinFunc(DEMO_SPI_SCLK_PAD, DEMO_SPI_PIN_FUNC), "SPI SCLK pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_expect_gpio_ok(Liot_SetPinFunc(DEMO_SPI_CS_PAD, L_PIN_FUNC_0), "SPI CS pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_expect_gpio_ok(Liot_GpioInit(DEMO_SPI_CS_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL), "SPI CS gpio");
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

static int32_t demo_ensure_dir(const char *dir_path)
{
    liot_stat_ext_s st;
    int ret;

    ret = liot_mkdir_ext(dir_path, 0);
    if (ret == LIOT_EXTFLASH_OK)
    {
        return 0;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(dir_path, &st);
    if ((ret == LIOT_EXTFLASH_OK) && (st.type == LIOT_EXTFLASH_TYPE_DIR))
    {
        return 0;
    }

    return LIOT_EXTFLASH_MKDIR_FAIL;
}

void liot_extflash_fs_p25q64_demo_thread(void *argv)
{
    uint8_t *write_buf = NULL;
    uint8_t *read_buf = NULL;
    LFILE_EXT fd = -1;
    LDIR_EXT *dir = NULL;
    ldirent_ext *entry = NULL;
    uint32_t total_tick_start;
    uint32_t tick_start;
    uint32_t mount_ticks = 0U;
    uint32_t write_ticks = 0U;
    uint32_t read_ticks = 0U;
    uint32_t verify_ticks = 0U;
    uint32_t total_ticks;
    int ret;
    bool fs_ready = false;

    (void)argv;

    liot_rtos_task_sleep_ms(DEMO_BOOT_DELAY_MS);
    liot_trace("%s demo start board=L_CT4IT02_1698W flash=%s flash_total=0x%08lX fs_base=0x%06lX fs_total=0x%06lX\n",
               DEMO_LOG_PREFIX,
               DEMO_FLASH_NAME,
               (unsigned long)DEMO_FLASH_TOTAL_SIZE,
               (unsigned long)DEMO_FS_BASE_ADDR,
               (unsigned long)DEMO_FS_TOTAL_SIZE);

    ret = demo_flash_hw_init();
    if (ret != 0)
    {
        liot_trace("%s hardware init failed ret=%d\n", DEMO_LOG_PREFIX, ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    ret = liot_flash_init_ext(&g_demo_flash_cfg);
    if (ret != 0)
    {
        liot_trace("%s liot_flash_init_ext failed ret=%d\n", DEMO_LOG_PREFIX, ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    total_tick_start = demo_tick_now();
    tick_start = demo_tick_now();
    ret = liot_finit_ext(&g_demo_fs_cfg);
    mount_ticks = demo_tick_elapsed(tick_start);
    if (ret != LIOT_EXTFLASH_OK)
    {
        liot_trace("%s liot_finit_ext failed ret=%d tick=%lu, try format once\n",
                   DEMO_LOG_PREFIX,
                   ret,
                   (unsigned long)mount_ticks);

        ret = liot_fformat_ext();
        if (ret != LIOT_EXTFLASH_OK)
        {
            liot_trace("%s liot_fformat_ext failed ret=%d\n", DEMO_LOG_PREFIX, ret);
            goto cleanup;
        }

        (void)liot_fdeinit_ext();
        tick_start = demo_tick_now();
        ret = liot_finit_ext(&g_demo_fs_cfg);
        mount_ticks += demo_tick_elapsed(tick_start);
        if (ret != LIOT_EXTFLASH_OK)
        {
            liot_trace("%s liot_finit_ext retry failed ret=%d\n", DEMO_LOG_PREFIX, ret);
            goto cleanup;
        }
    }
    fs_ready = true;

    liot_trace("%s fs mount ok block=%lu read=%lu prog=%lu free_size=%d mount_tick=%lu\n",
               DEMO_LOG_PREFIX,
               (unsigned long)g_demo_fs_cfg.block_size,
               (unsigned long)g_demo_fs_cfg.read_size,
               (unsigned long)g_demo_fs_cfg.prog_size,
               liot_exflash_free_size_get(),
               (unsigned long)mount_ticks);

    ret = demo_ensure_dir(DEMO_TEST_DIR);
    if (ret != 0)
    {
        liot_trace("%s ensure dir failed path=%s ret=%d\n", DEMO_LOG_PREFIX, DEMO_TEST_DIR, ret);
        goto cleanup;
    }

    write_buf = (uint8_t *)malloc(DEMO_TEST_SIZE);
    read_buf = (uint8_t *)malloc(DEMO_TEST_SIZE);
    if ((write_buf == NULL) || (read_buf == NULL))
    {
        liot_trace("%s malloc failed size=%lu\n", DEMO_LOG_PREFIX, (unsigned long)DEMO_TEST_SIZE);
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }

    demo_fill_pattern(write_buf, DEMO_TEST_SIZE, 0x41U);
    memset(read_buf, 0x00, DEMO_TEST_SIZE);

    fd = liot_fopen_ext(DEMO_TEST_FILE, "w+");
    if (fd <= 0)
    {
        liot_trace("%s liot_fopen_ext failed file=%s fd=%d\n", DEMO_LOG_PREFIX, DEMO_TEST_FILE, (int)fd);
        ret = LIOT_EXTFLASH_OPEN_FAIL;
        goto cleanup;
    }

    tick_start = demo_tick_now();
    ret = liot_fwrite_ext(write_buf, DEMO_TEST_SIZE, 1, fd);
    write_ticks = demo_tick_elapsed(tick_start);
    if (ret != (int)DEMO_TEST_SIZE)
    {
        liot_trace("%s liot_fwrite_ext failed file=%s expect=%lu ret=%d tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   DEMO_TEST_FILE,
                   (unsigned long)DEMO_TEST_SIZE,
                   ret,
                   (unsigned long)write_ticks);
        ret = LIOT_EXTFLASH_WRITE_FAIL;
        goto cleanup;
    }

    ret = liot_fseek_ext(fd, 0, LIOT_EXTFLASH_SEEK_SET);
    if (ret < 0)
    {
        liot_trace("%s liot_fseek_ext failed file=%s ret=%d\n", DEMO_LOG_PREFIX, DEMO_TEST_FILE, ret);
        ret = LIOT_EXTFLASH_SEEK_FAIL;
        goto cleanup;
    }

    tick_start = demo_tick_now();
    ret = liot_fread_ext(read_buf, DEMO_TEST_SIZE, 1, fd);
    read_ticks = demo_tick_elapsed(tick_start);
    if (ret != (int)DEMO_TEST_SIZE)
    {
        liot_trace("%s liot_fread_ext failed file=%s expect=%lu ret=%d tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   DEMO_TEST_FILE,
                   (unsigned long)DEMO_TEST_SIZE,
                   ret,
                   (unsigned long)read_ticks);
        ret = LIOT_EXTFLASH_READ_FAIL;
        goto cleanup;
    }

    tick_start = demo_tick_now();
    ret = (memcmp(write_buf, read_buf, DEMO_TEST_SIZE) == 0) ? 0 : LIOT_EXTFLASH_READ_FAIL;
    verify_ticks = demo_tick_elapsed(tick_start);
    if (ret != 0)
    {
        liot_trace("%s file verify failed file=%s size=%lu tick=%lu\n",
                   DEMO_LOG_PREFIX,
                   DEMO_TEST_FILE,
                   (unsigned long)DEMO_TEST_SIZE,
                   (unsigned long)verify_ticks);
        goto cleanup;
    }

    ret = liot_fclose_ext(fd);
    if (ret != LIOT_EXTFLASH_OK)
    {
        liot_trace("%s liot_fclose_ext failed file=%s ret=%d\n", DEMO_LOG_PREFIX, DEMO_TEST_FILE, ret);
        fd = -1;
        goto cleanup;
    }
    fd = -1;

    liot_trace("%s file_basic pass file=%s size=%lu write_tick=%lu read_tick=%lu verify_tick=%lu tail=0x%02X\n",
               DEMO_LOG_PREFIX,
               DEMO_TEST_FILE,
               (unsigned long)DEMO_TEST_SIZE,
               (unsigned long)write_ticks,
               (unsigned long)read_ticks,
               (unsigned long)verify_ticks,
               read_buf[DEMO_TEST_SIZE - 1U]);

    dir = liot_opendir_ext(DEMO_TEST_DIR);
    if (dir != NULL)
    {
        liot_trace("%s dir list path=%s\n", DEMO_LOG_PREFIX, DEMO_TEST_DIR);
        while ((entry = liot_readdir_ext(dir)) != NULL)
        {
            liot_trace("%s dir entry name=%s type=%d\n",
                       DEMO_LOG_PREFIX,
                       entry->d_name,
                       entry->d_type);
        }
        liot_closedir_ext(dir);
        dir = NULL;
    }

    total_ticks = demo_tick_elapsed(total_tick_start);
    liot_trace("%s summary flash=%s flash_total=0x%08lX fs_base=0x%06lX fs_total=0x%06lX free_size=%d mount_tick=%lu write_tick=%lu read_tick=%lu verify_tick=%lu total_tick=%lu\n",
               DEMO_LOG_PREFIX,
               DEMO_FLASH_NAME,
               (unsigned long)DEMO_FLASH_TOTAL_SIZE,
               (unsigned long)DEMO_FS_BASE_ADDR,
               (unsigned long)DEMO_FS_TOTAL_SIZE,
               liot_exflash_free_size_get(),
               (unsigned long)mount_ticks,
               (unsigned long)write_ticks,
               (unsigned long)read_ticks,
               (unsigned long)verify_ticks,
               (unsigned long)total_ticks);

    ret = 0;

cleanup:
    if (dir != NULL)
    {
        liot_closedir_ext(dir);
    }

    if (fd > 0)
    {
        (void)liot_fclose_ext(fd);
    }

    if (fs_ready)
    {
        liot_trace("%s cleanup remove file=%s ret=%d\n",
                   DEMO_LOG_PREFIX,
                   DEMO_TEST_FILE,
                   liot_remove_ext(DEMO_TEST_FILE));
        liot_trace("%s cleanup remove dir=%s ret=%d\n",
                   DEMO_LOG_PREFIX,
                   DEMO_TEST_DIR,
                   liot_remove_ext(DEMO_TEST_DIR));
        (void)liot_fdeinit_ext();
    }

    if (write_buf != NULL)
    {
        free(write_buf);
    }
    if (read_buf != NULL)
    {
        free(read_buf);
    }

    (void)liot_flash_deinit_ext();
    if (ret != 0)
    {
        liot_trace("%s demo failed ret=%d\n", DEMO_LOG_PREFIX, ret);
    }
    liot_trace("%s demo done\n", DEMO_LOG_PREFIX);
    liot_rtos_task_delete(NULL);
}
