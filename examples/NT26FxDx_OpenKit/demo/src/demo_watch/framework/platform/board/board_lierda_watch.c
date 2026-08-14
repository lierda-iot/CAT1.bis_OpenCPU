#include "board_lierda_watch.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "app_board.h"
#include "app_osal.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"

#define BOARD_LIERDA_WATCH_SHARED_3V3_PAD 16
#define BOARD_LIERDA_WATCH_SHARED_3V3_GPIO (L_GPIO_25)
#define BOARD_LIERDA_WATCH_SHARED_3V3_DELAY_MS 500U
#define BOARD_LIERDA_WATCH_FLASH_SPI_PORT 1U
#define BOARD_LIERDA_WATCH_FLASH_SIZE (0x200000U)
#define BOARD_LIERDA_WATCH_EXT_FS_BLOCK_SIZE 4096U
#define BOARD_LIERDA_WATCH_EXT_FS_READ_SIZE 256U
#define BOARD_LIERDA_WATCH_EXT_FS_PROG_SIZE 256U
#define BOARD_LIERDA_WATCH_SPI_MOSI_PAD 63
#define BOARD_LIERDA_WATCH_SPI_MISO_PAD 62
#define BOARD_LIERDA_WATCH_SPI_SCLK_PAD 49
#define BOARD_LIERDA_WATCH_SPI_SSN0_PAD 64
#define BOARD_LIERDA_WATCH_SPI_SSN0_GPIO (L_GPIO_12)
#define BOARD_LIERDA_WATCH_SPI_FUNC (1U)

static bool s_ext_flash_ready;
static bool s_ext_fs_ready;

static int board_lierda_watch_init(void)
{
    liot_ext_flash_cfg_t flash_cfg;
    liot_ext_fs_cfg_t fs_cfg;
    liot_gpioerr_e gpio_ret;
    int32_t flash_ret;
    int fs_ret;

    if (s_ext_fs_ready) {
        app_log("board ext storage already ready");
        return APP_OK;
    }

    if (!s_ext_flash_ready) {
        app_log("board init: shared 3V3 rail");
        (void)Liot_AonPowerCtl(true);
        (void)Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);

        (void)Liot_SetPinFunc(BOARD_LIERDA_WATCH_SHARED_3V3_PAD, L_PIN_FUNC_0);
        gpio_ret = Liot_GpioInit(BOARD_LIERDA_WATCH_SHARED_3V3_GPIO,
                                 L_IO_OUTPUT,
                                 L_IO_HIGH,
                                 NULL);
        if (gpio_ret != L_GPIO_ERR_SUCCESS) {
            app_log("board shared 3V3 GPIO%d init failed: %d",
                    BOARD_LIERDA_WATCH_SHARED_3V3_GPIO,
                    (int)gpio_ret);
            return APP_ERR_FAIL;
        }

        app_log("board shared 3V3 settling: %u ms",
                (unsigned int)BOARD_LIERDA_WATCH_SHARED_3V3_DELAY_MS);
        app_os_task_delay_ms(BOARD_LIERDA_WATCH_SHARED_3V3_DELAY_MS);

        app_log("board init: ext flash io");
        (void)Liot_SetPinFunc(BOARD_LIERDA_WATCH_SPI_MOSI_PAD, BOARD_LIERDA_WATCH_SPI_FUNC);
        (void)Liot_SetPinFunc(BOARD_LIERDA_WATCH_SPI_MISO_PAD, BOARD_LIERDA_WATCH_SPI_FUNC);
        (void)Liot_SetPinFunc(BOARD_LIERDA_WATCH_SPI_SCLK_PAD, BOARD_LIERDA_WATCH_SPI_FUNC);
        (void)Liot_SetPinFunc(BOARD_LIERDA_WATCH_SPI_SSN0_PAD, L_PIN_FUNC_0);
        gpio_ret = Liot_GpioInit(BOARD_LIERDA_WATCH_SPI_SSN0_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL);
        if (gpio_ret != L_GPIO_ERR_SUCCESS) {
            app_log("board ext flash CS GPIO%d init failed: %d",
                    BOARD_LIERDA_WATCH_SPI_SSN0_GPIO,
                    (int)gpio_ret);
            return APP_ERR_FAIL;
        }

        memset(&flash_cfg, 0, sizeof(flash_cfg));
        flash_cfg.spi_port = BOARD_LIERDA_WATCH_FLASH_SPI_PORT;
        flash_cfg.base_addr = 0U;
        flash_cfg.total_size = BOARD_LIERDA_WATCH_FLASH_SIZE;
        flash_ret = liot_flash_init_ext(&flash_cfg);
        if (flash_ret != 0) {
            app_log("board ext flash init failed: %ld", (long)flash_ret);
            return APP_ERR_FAIL;
        }
        s_ext_flash_ready = true;

        app_log("board ext flash ready: port=%u size=%lu",
                (unsigned int)BOARD_LIERDA_WATCH_FLASH_SPI_PORT,
                (unsigned long)BOARD_LIERDA_WATCH_FLASH_SIZE);
    }

    app_log("board init: ext flash fs");
    memset(&fs_cfg, 0, sizeof(fs_cfg));
    fs_cfg.base_addr = 0U;
    fs_cfg.total_size = BOARD_LIERDA_WATCH_FLASH_SIZE;
    fs_cfg.block_size = BOARD_LIERDA_WATCH_EXT_FS_BLOCK_SIZE;
    fs_cfg.read_size = BOARD_LIERDA_WATCH_EXT_FS_READ_SIZE;
    fs_cfg.prog_size = BOARD_LIERDA_WATCH_EXT_FS_PROG_SIZE;
    fs_ret = liot_finit_ext(&fs_cfg);
    if (fs_ret != LIOT_EXTFLASH_OK) {
        app_log("board ext flash fs init failed: %d", fs_ret);
        return APP_ERR_FAIL;
    }
    s_ext_fs_ready = true;

    app_log("board ext flash fs ready: base=0x%lx size=%lu block=%lu",
            (unsigned long)fs_cfg.base_addr,
            (unsigned long)fs_cfg.total_size,
            (unsigned long)fs_cfg.block_size);
    app_log("board init complete");
    return APP_OK;
}

int app_board_lierda_watch_register(void)
{
    static const app_board_ops_t s_board_ops = {
        .board_name = "lierda-watch",
        .init = board_lierda_watch_init,
    };

    app_log("board register: %s", s_board_ops.board_name);
    return app_board_register(&s_board_ops);
}
