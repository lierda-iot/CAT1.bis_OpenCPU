/**
 * @file demo_extflash_p25q64_mgr.c
 * @brief Unified app-layer extflash lifecycle manager (refcount + shared init).
 */

#include "demo_extflash_p25q64_mgr.h"

#include <string.h>

#include "liot_external_flash.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

#define DEMO_MGR_LOG_PREFIX           "[extflash_mgr]"

#define DEMO_LDO33_STABLE_DELAY_MS    (10U)
#define DEMO_VCC3V3_STABLE_DELAY_MS   (20U)

#define DEMO_LDO33_EN_PAD             (106)
#define DEMO_LDO33_EN_GPIO            (L_GPIO_25)
#define DEMO_VCC3V3_EN_PAD            (16)
#define DEMO_VCC3V3_EN_GPIO           (L_GPIO_27)

#define DEMO_SPI_MOSI_PAD             (63)
#define DEMO_SPI_MISO_PAD             (62)
#define DEMO_SPI_SCLK_PAD             (49)
#define DEMO_SPI_CS_PAD               (64)
#define DEMO_SPI_CS_GPIO              (L_GPIO_12)
#define DEMO_SPI_PIN_FUNC             (L_PIN_FUNC_1)

static liot_mutex_t g_demo_extflash_mgr_mutex;
static liot_mutex_t g_demo_extflash_mgr_access_mutex;
static liot_ext_flash_cfg_t g_demo_extflash_mgr_cfg;
static int32_t g_demo_extflash_mgr_refcount;
static bool g_demo_extflash_mgr_ready;
static bool g_demo_extflash_mgr_cfg_saved;

static int32_t demo_extflash_mgr_expect_gpio_ok(liot_gpioerr_e ret,
                                                const char *step)
{
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s failed: %ld\n",
                   DEMO_MGR_LOG_PREFIX,
                   step,
                   (long)ret);
        return (int32_t)ret;
    }

    return 0;
}

static int32_t demo_extflash_mgr_power_gpio_enable(const char *name,
                                                   int pad,
                                                   liot_gpio_e gpio)
{
    liot_gpioerr_e ret;

    ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret == L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s enabled via direct gpio=%d\n",
                   DEMO_MGR_LOG_PREFIX,
                   name,
                   (int)gpio);
        return 0;
    }

    liot_trace("%s %s direct gpio init failed: %ld, fallback pad=%d\n",
               DEMO_MGR_LOG_PREFIX,
               name,
               (long)ret,
               pad);

    ret = Liot_SetPinFunc(pad, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("%s %s pinmux fallback failed: %ld\n",
                   DEMO_MGR_LOG_PREFIX,
                   name,
                   (long)ret);
        return (int32_t)ret;
    }

    return demo_extflash_mgr_expect_gpio_ok(
        Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL),
        name);
}

static int32_t demo_extflash_mgr_hw_init(void)
{
    int32_t ret;

    ret = demo_extflash_mgr_expect_gpio_ok(Liot_AonPowerCtl(true),
                                           "Liot_AonPowerCtl");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V),
        "Liot_SetVoltage");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_power_gpio_enable("LDO33 enable",
                                              DEMO_LDO33_EN_PAD,
                                              DEMO_LDO33_EN_GPIO);
    if (ret != 0)
    {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_LDO33_STABLE_DELAY_MS);

    ret = demo_extflash_mgr_power_gpio_enable("VCC3V3 enable",
                                              DEMO_VCC3V3_EN_PAD,
                                              DEMO_VCC3V3_EN_GPIO);
    if (ret != 0)
    {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_VCC3V3_STABLE_DELAY_MS);

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_SetPinFunc(DEMO_SPI_MOSI_PAD, DEMO_SPI_PIN_FUNC),
        "SPI MOSI pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_SetPinFunc(DEMO_SPI_MISO_PAD, DEMO_SPI_PIN_FUNC),
        "SPI MISO pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_SetPinFunc(DEMO_SPI_SCLK_PAD, DEMO_SPI_PIN_FUNC),
        "SPI SCLK pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_SetPinFunc(DEMO_SPI_CS_PAD, L_PIN_FUNC_0),
        "SPI CS pinmux");
    if (ret != 0)
    {
        return ret;
    }

    ret = demo_extflash_mgr_expect_gpio_ok(
        Liot_GpioInit(DEMO_SPI_CS_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL),
        "SPI CS gpio");
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

static int32_t demo_extflash_mgr_mutex_ensure(void)
{
    liot_mutex_t mutex = NULL;
    liot_mutex_t extra_mutex = NULL;
    int32_t ret;

    if (g_demo_extflash_mgr_mutex != NULL)
    {
        return 0;
    }

    ret = (int32_t)liot_rtos_mutex_create(&mutex);
    if (ret != 0)
    {
        return ret;
    }

    liot_rtos_enter_critical();
    if (g_demo_extflash_mgr_mutex == NULL)
    {
        g_demo_extflash_mgr_mutex = mutex;
        mutex = NULL;
    }
    else
    {
        extra_mutex = mutex;
        mutex = NULL;
    }
    liot_rtos_exit_critical();

    if (extra_mutex != NULL)
    {
        (void)liot_rtos_mutex_delete(extra_mutex);
    }

    return 0;
}

/*
 * Separate mutex for the coarse cross-demo SPI access lock. It must not share
 * the refcount mutex: photo-lfs holds the access lock while calling mgr_init()
 * (which locks the refcount mutex), and raw/lfs lock the refcount mutex first
 * and then release it before taking the access lock. Sharing one mutex would
 * deadlock when photo-lfs mounts while another owner initializes.
 */
static int32_t demo_extflash_mgr_access_mutex_ensure(void)
{
    liot_mutex_t mutex = NULL;
    liot_mutex_t extra_mutex = NULL;
    int32_t ret;

    if (g_demo_extflash_mgr_access_mutex != NULL)
    {
        return 0;
    }

    ret = (int32_t)liot_rtos_mutex_create(&mutex);
    if (ret != 0)
    {
        return ret;
    }

    liot_rtos_enter_critical();
    if (g_demo_extflash_mgr_access_mutex == NULL)
    {
        g_demo_extflash_mgr_access_mutex = mutex;
        mutex = NULL;
    }
    else
    {
        extra_mutex = mutex;
        mutex = NULL;
    }
    liot_rtos_exit_critical();

    if (extra_mutex != NULL)
    {
        (void)liot_rtos_mutex_delete(extra_mutex);
    }

    return 0;
}

int32_t demo_extflash_p25q64_mgr_init(const liot_ext_flash_cfg_t *cfg,
                                      const char *owner)
{
    int32_t ret;

    if (cfg == NULL)
    {
        return -1;
    }

    ret = demo_extflash_mgr_mutex_ensure();
    if (ret != 0)
    {
        liot_trace("%s mutex ensure failed ret=%ld\n",
                   DEMO_MGR_LOG_PREFIX,
                   (long)ret);
        return ret;
    }

    ret = (int32_t)liot_rtos_mutex_lock(g_demo_extflash_mgr_mutex,
                                        LIOT_WAIT_FOREVER);
    if (ret != 0)
    {
        return ret;
    }

    if (g_demo_extflash_mgr_ready)
    {
        ++g_demo_extflash_mgr_refcount;
        liot_trace("%s owner=%s refcount=%ld (already ready)\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?",
                   (long)g_demo_extflash_mgr_refcount);
        (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
        return 0;
    }

    memcpy(&g_demo_extflash_mgr_cfg, cfg, sizeof(g_demo_extflash_mgr_cfg));
    g_demo_extflash_mgr_cfg_saved = true;

    ret = demo_extflash_mgr_hw_init();
    if (ret != 0)
    {
        liot_trace("%s owner=%s hw init failed ret=%ld\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?",
                   (long)ret);
        goto fail_unlock;
    }

    ret = (int32_t)liot_flash_init_ext(&g_demo_extflash_mgr_cfg);
    if (ret != 0)
    {
        liot_trace("%s owner=%s liot_flash_init_ext failed ret=%ld\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?",
                   (long)ret);
        goto fail_unlock;
    }

    g_demo_extflash_mgr_ready = true;
    g_demo_extflash_mgr_refcount = 1;
    liot_trace("%s owner=%s flash ready refcount=%ld spi=%u base=0x%06lX total=0x%08lX\n",
               DEMO_MGR_LOG_PREFIX,
               (owner != NULL) ? owner : "?",
               (long)g_demo_extflash_mgr_refcount,
               (unsigned int)g_demo_extflash_mgr_cfg.spi_port,
               (unsigned long)g_demo_extflash_mgr_cfg.base_addr,
               (unsigned long)g_demo_extflash_mgr_cfg.total_size);
    (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
    return 0;

fail_unlock:
    g_demo_extflash_mgr_cfg_saved = false;
    (void)memset(&g_demo_extflash_mgr_cfg, 0, sizeof(g_demo_extflash_mgr_cfg));
    (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
    return ret;
}

int32_t demo_extflash_p25q64_mgr_deinit(const char *owner)
{
    int32_t ret;

    ret = demo_extflash_mgr_mutex_ensure();
    if (ret != 0)
    {
        return ret;
    }

    ret = (int32_t)liot_rtos_mutex_lock(g_demo_extflash_mgr_mutex,
                                        LIOT_WAIT_FOREVER);
    if (ret != 0)
    {
        return ret;
    }

    if (!g_demo_extflash_mgr_ready || (g_demo_extflash_mgr_refcount <= 0))
    {
        liot_trace("%s owner=%s deinit ignored (not ready)\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?");
        (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
        return 0;
    }

    --g_demo_extflash_mgr_refcount;
    if (g_demo_extflash_mgr_refcount == 0)
    {
        ret = (int32_t)liot_flash_deinit_ext();
        if (ret != 0)
        {
            liot_trace("%s owner=%s liot_flash_deinit_ext failed ret=%ld\n",
                       DEMO_MGR_LOG_PREFIX,
                       (owner != NULL) ? owner : "?",
                       (long)ret);
            ++g_demo_extflash_mgr_refcount;
            (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
            return ret;
        }
        g_demo_extflash_mgr_ready = false;
        liot_trace("%s owner=%s last owner deinit, refcount=0\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?");
    }
    else
    {
        liot_trace("%s owner=%s refcount=%ld (still held)\n",
                   DEMO_MGR_LOG_PREFIX,
                   (owner != NULL) ? owner : "?",
                   (long)g_demo_extflash_mgr_refcount);
    }

    (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_mutex);
    return 0;
}

bool demo_extflash_p25q64_mgr_is_ready(void)
{
    return g_demo_extflash_mgr_ready;
}

int32_t demo_extflash_p25q64_mgr_lock(void)
{
    int32_t ret;

    ret = demo_extflash_mgr_access_mutex_ensure();
    if (ret != 0)
    {
        return ret;
    }

    ret = (int32_t)liot_rtos_mutex_lock(g_demo_extflash_mgr_access_mutex,
                                        LIOT_WAIT_FOREVER);
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

void demo_extflash_p25q64_mgr_unlock(void)
{
    if (g_demo_extflash_mgr_access_mutex != NULL)
    {
        (void)liot_rtos_mutex_unlock(g_demo_extflash_mgr_access_mutex);
    }
}

int32_t demo_extflash_p25q64_mgr_refcount_for_test(void)
{
    return g_demo_extflash_mgr_refcount;
}

bool demo_extflash_p25q64_mgr_ready_for_test(void)
{
    return g_demo_extflash_mgr_ready;
}
