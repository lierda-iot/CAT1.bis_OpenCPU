#include "demo_lvgl_photo_fs_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

#define DEMO_LVGL_PHOTO_FS_ALLOW_AUTO_FORMAT 1
#define DEMO_LVGL_PHOTO_FS_LOG_PREFIX        "[demo_lvgl_photo_fs_store]"

#define DEMO_FLASH_SPI_PORT          1u
#define DEMO_FLASH_BASE_ADDR         0x000000u
#define DEMO_FLASH_TOTAL_SIZE        0x800000u

#define DEMO_FS_BASE_ADDR            0x010000u
#define DEMO_FS_TOTAL_SIZE           0x7F0000u
#define DEMO_FS_BLOCK_SIZE           4096u
#define DEMO_FS_READ_SIZE            256u
#define DEMO_FS_PROG_SIZE            256u

#define DEMO_LDO33_STABLE_DELAY_MS   10u
#define DEMO_VCC3V3_STABLE_DELAY_MS  20u

#define DEMO_LDO33_EN_PAD            106
#define DEMO_LDO33_EN_GPIO           L_GPIO_25
#define DEMO_VCC3V3_EN_PAD           16
#define DEMO_VCC3V3_EN_GPIO          L_GPIO_27

#define DEMO_SPI_MOSI_PAD            63
#define DEMO_SPI_MISO_PAD            62
#define DEMO_SPI_SCLK_PAD            49
#define DEMO_SPI_CS_PAD              64
#define DEMO_SPI_CS_GPIO             L_GPIO_12
#define DEMO_SPI_PIN_FUNC            L_PIN_FUNC_1

static const liot_ext_flash_cfg_t g_demo_lvgl_photo_fs_flash_cfg = {
    .spi_port = DEMO_FLASH_SPI_PORT,
    .base_addr = DEMO_FLASH_BASE_ADDR,
    .total_size = DEMO_FLASH_TOTAL_SIZE,
};

static const liot_ext_fs_cfg_t g_demo_lvgl_photo_fs_cfg = {
    .base_addr = BAJI_FLASH_LFS_BASE,
    .total_size = BAJI_FLASH_LFS_TOTAL,
    .block_size = BAJI_FLASH_LFS_BLOCK,
    .read_size = BAJI_FLASH_LFS_READ,
    .prog_size = BAJI_FLASH_LFS_PROG,
};

static bool g_demo_lvgl_photo_fs_store_ready;
static liot_mutex_t g_demo_lvgl_photo_fs_store_mutex;

static const char *demo_lvgl_photo_fs_format_name(demo_lvgl_photo_fs_format_t format)
{
    switch (format) {
    case DEMO_LVGL_PHOTO_FS_FORMAT_GIF:
        return "gif";
    case DEMO_LVGL_PHOTO_FS_FORMAT_JPEG:
        return "jpeg";
    case DEMO_LVGL_PHOTO_FS_FORMAT_PNG:
        return "png";
    case DEMO_LVGL_PHOTO_FS_FORMAT_BJP:
    default:
        return "bjp";
    }
}

static demo_lvgl_photo_fs_format_t demo_lvgl_photo_fs_format_from_name(const char *name)
{
    if (name == NULL) {
        return DEMO_LVGL_PHOTO_FS_FORMAT_BJP;
    }
    if ((strcmp(name, "gif") == 0) || (strcmp(name, "GIF") == 0)) {
        return DEMO_LVGL_PHOTO_FS_FORMAT_GIF;
    }
    if ((strcmp(name, "rgb565") == 0) || (strcmp(name, "RGB565") == 0) ||
        (strcmp(name, "raw_rgb565") == 0) || (strcmp(name, "RAW_RGB565") == 0)) {
        return DEMO_LVGL_PHOTO_FS_FORMAT_BJP;
    }
    if ((strcmp(name, "jpeg") == 0) || (strcmp(name, "jpg") == 0) ||
        (strcmp(name, "JPEG") == 0) || (strcmp(name, "JPG") == 0)) {
        return DEMO_LVGL_PHOTO_FS_FORMAT_JPEG;
    }
    if ((strcmp(name, "png") == 0) || (strcmp(name, "PNG") == 0)) {
        return DEMO_LVGL_PHOTO_FS_FORMAT_PNG;
    }
    return DEMO_LVGL_PHOTO_FS_FORMAT_BJP;
}

static bool demo_lvgl_photo_fs_store_id_is_valid(const char *id)
{
    unsigned int i;

    if ((id == NULL) || (id[0] == '\0')) {
        return false;
    }

    for (i = 0; id[i] != '\0'; ++i) {
        char c = id[i];

        if (i >= DEMO_LVGL_PHOTO_FS_ID_MAX_LEN) {
            return false;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') || (c == '-')) {
            continue;
        }
        return false;
    }

    return true;
}

static int demo_lvgl_photo_fs_store_mutex_ensure(void)
{
    liot_mutex_t mutex = NULL;
    liot_mutex_t extra_mutex = NULL;
    int ret;

    if (g_demo_lvgl_photo_fs_store_mutex != NULL) {
        return 0;
    }

    ret = liot_rtos_mutex_create(&mutex);
    if (ret != 0) {
        return ret;
    }

    liot_rtos_enter_critical();
    if (g_demo_lvgl_photo_fs_store_mutex == NULL) {
        g_demo_lvgl_photo_fs_store_mutex = mutex;
        mutex = NULL;
    } else {
        extra_mutex = mutex;
        mutex = NULL;
    }
    liot_rtos_exit_critical();

    if (extra_mutex != NULL) {
        (void)liot_rtos_mutex_delete(extra_mutex);
    }

    return 0;
}

static int demo_lvgl_photo_fs_store_lock(void)
{
    int ret;

    ret = demo_lvgl_photo_fs_store_mutex_ensure();
    if (ret != 0) {
        return ret;
    }

    return liot_rtos_mutex_lock(g_demo_lvgl_photo_fs_store_mutex, LIOT_WAIT_FOREVER);
}

void demo_lvgl_photo_fs_store_access_end(void)
{
    if (g_demo_lvgl_photo_fs_store_mutex != NULL) {
        (void)liot_rtos_mutex_unlock(g_demo_lvgl_photo_fs_store_mutex);
    }
}

static int demo_lvgl_photo_fs_gpio_expect(liot_gpioerr_e ret, const char *step)
{
    if (ret != L_GPIO_ERR_SUCCESS) {
        liot_trace("%s %s failed ret=%ld",
                   DEMO_LVGL_PHOTO_FS_LOG_PREFIX,
                   step,
                   (long)ret);
        return (int)ret;
    }
    return 0;
}

static int demo_lvgl_photo_fs_power_gpio_enable(const char *name, int pad, liot_gpio_e gpio)
{
    liot_gpioerr_e ret;

    ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret == L_GPIO_ERR_SUCCESS) {
        return 0;
    }

    ret = Liot_SetPinFunc(pad, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS) {
        liot_trace("%s %s pinmux failed ret=%ld",
                   DEMO_LVGL_PHOTO_FS_LOG_PREFIX,
                   name,
                   (long)ret);
        return (int)ret;
    }

    return demo_lvgl_photo_fs_gpio_expect(Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL), name);
}

static int demo_lvgl_photo_fs_flash_hw_init(void)
{
    int ret;

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_AonPowerCtl(true), "Liot_AonPowerCtl");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V), "Liot_SetVoltage");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_power_gpio_enable("LDO33", DEMO_LDO33_EN_PAD, DEMO_LDO33_EN_GPIO);
    if (ret != 0) {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_LDO33_STABLE_DELAY_MS);

    ret = demo_lvgl_photo_fs_power_gpio_enable("VCC3V3", DEMO_VCC3V3_EN_PAD, DEMO_VCC3V3_EN_GPIO);
    if (ret != 0) {
        return ret;
    }
    liot_rtos_task_sleep_ms(DEMO_VCC3V3_STABLE_DELAY_MS);

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_SetPinFunc(DEMO_SPI_MOSI_PAD, DEMO_SPI_PIN_FUNC), "SPI MOSI");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_SetPinFunc(DEMO_SPI_MISO_PAD, DEMO_SPI_PIN_FUNC), "SPI MISO");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_SetPinFunc(DEMO_SPI_SCLK_PAD, DEMO_SPI_PIN_FUNC), "SPI SCLK");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_SetPinFunc(DEMO_SPI_CS_PAD, L_PIN_FUNC_0), "SPI CS pinmux");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_gpio_expect(Liot_GpioInit(DEMO_SPI_CS_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL), "SPI CS gpio");
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int demo_lvgl_photo_fs_store_ensure_dir(const char *dir_path)
{
    liot_stat_ext_s st;
    int ret;

    ret = liot_mkdir_ext(dir_path, 0);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(dir_path, &st);
    if ((ret == LIOT_EXTFLASH_OK) && (st.type == LIOT_EXTFLASH_TYPE_DIR)) {
        return 0;
    }

    return LIOT_EXTFLASH_MKDIR_FAIL;
}

static int demo_lvgl_photo_fs_store_mount_fs(void)
{
    int ret;

    ret = liot_finit_ext(&g_demo_lvgl_photo_fs_cfg);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }

#if DEMO_LVGL_PHOTO_FS_ALLOW_AUTO_FORMAT
    liot_trace("%s liot_finit_ext failed ret=%d, try format once",
               DEMO_LVGL_PHOTO_FS_LOG_PREFIX,
               ret);
    ret = liot_fformat_ext();
    if (ret != LIOT_EXTFLASH_OK) {
        return ret;
    }

    (void)liot_fdeinit_ext();
    ret = liot_finit_ext(&g_demo_lvgl_photo_fs_cfg);
    if (ret == LIOT_EXTFLASH_OK) {
        return 0;
    }
#endif

    return ret;
}

static int demo_lvgl_photo_fs_store_mount_locked(void)
{
    int ret;

    if (g_demo_lvgl_photo_fs_store_ready) {
        return 0;
    }

    ret = demo_lvgl_photo_fs_flash_hw_init();
    if (ret != 0) {
        return ret;
    }

    ret = liot_flash_init_ext(&g_demo_lvgl_photo_fs_flash_cfg);
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_mount_fs();
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_ensure_dir("/flash");
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_ROOT_DIR);
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_PHOTO_DIR);
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_GIF_DIR);
    if (ret != 0) {
        return ret;
    }

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_JPEG_DIR);
    if (ret != 0) {
        return ret;
    }
#endif

#if DEMO_LVGL_PHOTO_FS_ENABLE_PNG
    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_PNG_DIR);
    if (ret != 0) {
        return ret;
    }
#endif

    ret = demo_lvgl_photo_fs_store_ensure_dir(DEMO_LVGL_PHOTO_FS_TMP_DIR);
    if (ret != 0) {
        return ret;
    }

    g_demo_lvgl_photo_fs_store_ready = true;
    return 0;
}

int demo_lvgl_photo_fs_store_access_begin(void)
{
    int ret;

    ret = demo_lvgl_photo_fs_store_lock();
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_mount_locked();
    if (ret != 0) {
        demo_lvgl_photo_fs_store_access_end();
        return ret;
    }

    return 0;
}

int demo_lvgl_photo_fs_store_mount(void)
{
    int ret;

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    demo_lvgl_photo_fs_store_access_end();
    return 0;
}

uint32_t demo_lvgl_photo_fs_store_crc32_update(uint32_t crc, const void *data, unsigned int len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    unsigned int i;
    unsigned int bit;

    if ((bytes == NULL) && (len != 0u)) {
        return crc;
    }

    for (i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint32_t demo_lvgl_photo_fs_store_crc32_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

int demo_lvgl_photo_fs_store_build_path(const char *id,
                                        demo_lvgl_photo_fs_format_t format,
                                        char *out_path,
                                        unsigned int out_len)
{
    int written;
    const char *dir = DEMO_LVGL_PHOTO_FS_PHOTO_DIR;
    const char *prefix = "photo_";
    const char *suffix = ".bjp";

    if (!demo_lvgl_photo_fs_store_id_is_valid(id) || (out_path == NULL) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    switch (format) {
    case DEMO_LVGL_PHOTO_FS_FORMAT_GIF:
        dir = DEMO_LVGL_PHOTO_FS_GIF_DIR;
        prefix = "";
        suffix = ".gif";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_JPEG:
        dir = DEMO_LVGL_PHOTO_FS_JPEG_DIR;
        prefix = "";
        suffix = ".jpg";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_PNG:
        dir = DEMO_LVGL_PHOTO_FS_PNG_DIR;
        prefix = "";
        suffix = ".png";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_BJP:
    default:
        dir = DEMO_LVGL_PHOTO_FS_PHOTO_DIR;
        prefix = "photo_";
        suffix = ".bjp";
        break;
    }

    written = snprintf(out_path, out_len, "%s/%s%s%s", dir, prefix, id, suffix);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

int demo_lvgl_photo_fs_store_build_tmp_path(const char *id,
                                            demo_lvgl_photo_fs_format_t format,
                                            char *out_path,
                                            unsigned int out_len)
{
    int written;
    const char *base = "photo";

    if (!demo_lvgl_photo_fs_store_id_is_valid(id) || (out_path == NULL) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    switch (format) {
    case DEMO_LVGL_PHOTO_FS_FORMAT_GIF:
        base = "photo";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_JPEG:
        base = "jpeg";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_PNG:
        base = "png";
        break;
    case DEMO_LVGL_PHOTO_FS_FORMAT_BJP:
    default:
        base = "photo";
        break;
    }

    written = snprintf(out_path, out_len, "%s/%s_%s.tmp",
                       DEMO_LVGL_PHOTO_FS_TMP_DIR,
                       base,
                       id);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    return 0;
}

static int demo_lvgl_photo_fs_store_write_file_atomic(const char *tmp_path,
                                                      const char *final_path,
                                                      const void *data,
                                                      unsigned int data_len)
{
    LFILE_EXT fd;
    int ret;
    liot_stat_ext_s tmp_st;
    liot_stat_ext_s final_st;
    int stat_ret;
    int final_exist_ret;
    int final_exist_after_ret;
    int tmp_exist_after_ret;

    fd = liot_fopen_ext(tmp_path, "w+");
    if (fd <= 0) {
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    ret = liot_fwrite_ext((void *)data, data_len, 1, fd);
    if (ret == (int)data_len) {
        ret = liot_fsync_ext(fd);
    } else {
        ret = LIOT_EXTFLASH_WRITE_FAIL;
    }

    if ((liot_fclose_ext(fd) != LIOT_EXTFLASH_OK) && (ret == LIOT_EXTFLASH_OK)) {
        ret = LIOT_EXTFLASH_CLOSE_FAIL;
    }

    if (ret != LIOT_EXTFLASH_OK) {
        (void)liot_remove_ext(tmp_path);
        return ret;
    }

    memset(&tmp_st, 0, sizeof(tmp_st));
    stat_ret = liot_stat_ext(tmp_path, &tmp_st);
    if ((stat_ret != LIOT_EXTFLASH_OK) ||
        (tmp_st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (tmp_st.size != data_len)) {
        (void)liot_remove_ext(tmp_path);
        if (stat_ret != LIOT_EXTFLASH_OK) {
            return stat_ret;
        }
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    memset(&final_st, 0, sizeof(final_st));
    final_exist_ret = liot_file_exist_ext(final_path);
    stat_ret = liot_stat_ext(final_path, &final_st);
    if (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST) {
        ret = liot_remove_ext(final_path);
        final_exist_ret = liot_file_exist_ext(final_path);
        memset(&final_st, 0, sizeof(final_st));
        stat_ret = liot_stat_ext(final_path, &final_st);
        if ((ret != LIOT_EXTFLASH_OK) && (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST)) {
            (void)liot_remove_ext(tmp_path);
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
        if (final_exist_ret == LIOT_EXTFLASH_OK) {
            (void)liot_remove_ext(tmp_path);
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
    }

    ret = liot_rename_ext(tmp_path, final_path);
    if (ret != LIOT_EXTFLASH_OK) {
        final_exist_after_ret = liot_file_exist_ext(final_path);
        memset(&final_st, 0, sizeof(final_st));
        stat_ret = liot_stat_ext(final_path, &final_st);
        tmp_exist_after_ret = liot_file_exist_ext(tmp_path);
        if ((final_exist_after_ret == LIOT_EXTFLASH_OK) &&
            (stat_ret == LIOT_EXTFLASH_OK) &&
            (final_st.type == LIOT_EXTFLASH_TYPE_FILE) &&
            (final_st.size == data_len) &&
            (tmp_exist_after_ret == LIOT_EXTFLASH_NOT_EXIST)) {
            return 0;
        }
        (void)liot_remove_ext(tmp_path);
        return LIOT_EXTFLASH_RENAME_FAIL;
    }

    return 0;
}

int demo_lvgl_photo_fs_store_write_photo_file(const char *id,
                                              const void *data,
                                              unsigned int data_len)
{
    char tmp_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    char final_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    int ret;

    ret = demo_lvgl_photo_fs_store_build_tmp_path(id,
                                                  DEMO_LVGL_PHOTO_FS_FORMAT_BJP,
                                                  tmp_path,
                                                  sizeof(tmp_path));
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_build_path(id,
                                              DEMO_LVGL_PHOTO_FS_FORMAT_BJP,
                                              final_path,
                                              sizeof(final_path));
    if (ret != 0) {
        return ret;
    }

    return demo_lvgl_photo_fs_store_write_file_atomic(tmp_path, final_path, data, data_len);
}

int demo_lvgl_photo_fs_store_write_raw_file(const char *id,
                                            demo_lvgl_photo_fs_format_t format,
                                            const void *data,
                                            unsigned int data_len)
{
    char tmp_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    char final_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    int ret;

    ret = demo_lvgl_photo_fs_store_build_tmp_path(id, format, tmp_path, sizeof(tmp_path));
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_build_path(id, format, final_path, sizeof(final_path));
    if (ret != 0) {
        return ret;
    }

    return demo_lvgl_photo_fs_store_write_file_atomic(tmp_path, final_path, data, data_len);
}

int demo_lvgl_photo_fs_store_read_photo_header(const char *id,
                                               demo_lvgl_photo_fs_file_header_t *out_header)
{
    char path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    LFILE_EXT fd;
    int ret;

    if (out_header == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_build_path(id,
                                              DEMO_LVGL_PHOTO_FS_FORMAT_BJP,
                                              path,
                                              sizeof(path));
    if (ret != 0) {
        demo_lvgl_photo_fs_store_access_end();
        return ret;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    ret = liot_fread_ext(out_header, sizeof(*out_header), 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();

    if (ret != (int)sizeof(*out_header)) {
        return LIOT_EXTFLASH_READ_FAIL;
    }

    return 0;
}

static int demo_lvgl_photo_fs_store_resolve_item_path(const demo_lvgl_photo_fs_item_t *item,
                                                      char *path,
                                                      unsigned int path_len)
{
    if ((item == NULL) || (path == NULL) || (path_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (item->local_path[0] != '\0') {
        int written;

        written = snprintf(path, path_len, "%s", item->local_path);
        if ((written < 0) || ((unsigned int)written >= path_len)) {
            return LIOT_EXTFLASH_INVALID_PARAMETER;
        }
        return 0;
    }

    return demo_lvgl_photo_fs_store_build_path(item->id, item->format, path, path_len);
}

int demo_lvgl_photo_fs_store_save_index(const demo_lvgl_photo_fs_item_t *items,
                                        unsigned int count)
{
    char *buf = NULL;
    size_t buf_size = DEMO_LVGL_PHOTO_FS_INDEX_MAX_SIZE + 1u;
    size_t offset = 0u;
    unsigned int i;
    int ret;

    if ((items == NULL) && (count != 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    buf = (char *)liot_rtos_malloc(buf_size);
    if (buf == NULL) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    ret = snprintf(buf, buf_size, "{\"result\":[\n");
    if ((ret <= 0) || ((size_t)ret >= buf_size)) {
        liot_rtos_free(buf);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    offset = (size_t)ret;

    for (i = 0; i < count; ++i) {
        char path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
        const char *name;
        const char *remote_path;
        const char *format_name;
        int len;

        ret = demo_lvgl_photo_fs_store_resolve_item_path(&items[i], path, sizeof(path));
        if (ret != 0) {
            liot_rtos_free(buf);
            demo_lvgl_photo_fs_store_access_end();
            return ret;
        }

        name = (items[i].name[0] != '\0') ? items[i].name : items[i].id;
        remote_path = (items[i].remote_path[0] != '\0') ? items[i].remote_path : path;
        format_name = demo_lvgl_photo_fs_format_name(items[i].format);

        len = snprintf(buf + offset,
                       buf_size - offset,
                       "%s{\"id\":\"%s\",\"name\":\"%s\",\"remote_path\":\"%s\","
                       "\"local_path\":\"%s\",\"file_size\":%lu,\"crc32\":\"%08lX\","
                       "\"width\":%u,\"height\":%u,\"format\":\"%s\"}\n",
                       (i == 0u) ? "" : ",",
                       items[i].id,
                       name,
                       remote_path,
                       path,
                       (unsigned long)items[i].file_size,
                       (unsigned long)items[i].crc32,
                       (unsigned int)items[i].width,
                       (unsigned int)items[i].height,
                       format_name);
        if ((len <= 0) || ((size_t)len >= (buf_size - offset))) {
            liot_rtos_free(buf);
            demo_lvgl_photo_fs_store_access_end();
            return LIOT_EXTFLASH_INVALID_PARAMETER;
        }
        offset += (size_t)len;
    }

    ret = snprintf(buf + offset, buf_size - offset, "]}\n");
    if ((ret <= 0) || ((size_t)ret >= (buf_size - offset))) {
        liot_rtos_free(buf);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    offset += (size_t)ret;

    ret = demo_lvgl_photo_fs_store_write_file_atomic(DEMO_LVGL_PHOTO_FS_INDEX_TMP_FILE,
                                                     DEMO_LVGL_PHOTO_FS_INDEX_FILE,
                                                     buf,
                                                     (unsigned int)offset);
    liot_rtos_free(buf);
    demo_lvgl_photo_fs_store_access_end();
    return ret;
}

static int demo_lvgl_photo_fs_store_parse_index_line(const char *line,
                                                     demo_lvgl_photo_fs_item_t *out_item)
{
    char id[DEMO_LVGL_PHOTO_FS_ID_MAX_LEN + 1u];
    char name[DEMO_LVGL_PHOTO_FS_NAME_MAX_LEN + 1u];
    char remote_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    char local_path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    char crc_hex[9];
    char format[16];
    unsigned long file_size = 0u;
    unsigned long width = 0u;
    unsigned long height = 0u;
    unsigned long cf = 0u;
    int matched;
    bool cf_present = false;
    const char *p = line;

    if ((line == NULL) || (out_item == NULL)) {
        return -1;
    }

    while ((*p == ' ') || (*p == '\t') || (*p == ',')) {
        ++p;
    }
    if (strncmp(p, "{\"id\":\"", 7) != 0) {
        return -1;
    }

    memset(id, 0, sizeof(id));
    memset(name, 0, sizeof(name));
    memset(remote_path, 0, sizeof(remote_path));
    memset(local_path, 0, sizeof(local_path));
    memset(crc_hex, 0, sizeof(crc_hex));
    memset(format, 0, sizeof(format));

    matched = sscanf(p,
                     "{\"id\":\"%32[^\"]\",\"name\":\"%48[^\"]\",\"remote_path\":\"%160[^\"]\","
                     "\"local_path\":\"%160[^\"]\",\"file_size\":%lu,\"crc32\":\"%8[0-9A-Fa-f]\","
                     "\"width\":%lu,\"height\":%lu,\"cf\":%lu,\"format\":\"%15[^\"]\"}",
                     id,
                     name,
                     remote_path,
                     local_path,
                     &file_size,
                     crc_hex,
                     &width,
                     &height,
                     &cf,
                     format);
    if (matched == 10) {
        cf_present = true;
    } else {
        matched = sscanf(p,
                         "{\"id\":\"%32[^\"]\",\"name\":\"%48[^\"]\",\"remote_path\":\"%160[^\"]\","
                         "\"local_path\":\"%160[^\"]\",\"file_size\":%lu,\"crc32\":\"%8[0-9A-Fa-f]\","
                         "\"width\":%lu,\"height\":%lu,\"format\":\"%15[^\"]\"}",
                         id,
                         name,
                         remote_path,
                         local_path,
                         &file_size,
                         crc_hex,
                         &width,
                         &height,
                         format);
        if (matched != 9) {
            return -1;
        }
    }
    if (!demo_lvgl_photo_fs_store_id_is_valid(id)) {
        return -1;
    }

    memset(out_item, 0, sizeof(*out_item));
    (void)snprintf(out_item->id, sizeof(out_item->id), "%s", id);
    (void)snprintf(out_item->name, sizeof(out_item->name), "%s", name);
    (void)snprintf(out_item->remote_path, sizeof(out_item->remote_path), "%s", remote_path);
    (void)snprintf(out_item->local_path, sizeof(out_item->local_path), "%s", local_path);
    out_item->file_size = (uint32_t)file_size;
    out_item->crc32 = (uint32_t)strtoul(crc_hex, NULL, 16);
    out_item->width = (uint16_t)width;
    out_item->height = (uint16_t)height;
    out_item->format = demo_lvgl_photo_fs_format_from_name(format);
    if (cf_present) {
        out_item->cf = (uint16_t)cf;
    } else {
        switch (out_item->format) {
        case DEMO_LVGL_PHOTO_FS_FORMAT_GIF:
        case DEMO_LVGL_PHOTO_FS_FORMAT_JPEG:
        case DEMO_LVGL_PHOTO_FS_FORMAT_PNG:
            out_item->cf = LV_IMG_CF_RAW;
            break;
        case DEMO_LVGL_PHOTO_FS_FORMAT_BJP:
        default:
            out_item->cf = LV_IMG_CF_TRUE_COLOR;
            break;
        }
    }
    return 0;
}

int demo_lvgl_photo_fs_store_load_index(demo_lvgl_photo_fs_item_t *items,
                                        unsigned int max_items,
                                        unsigned int *out_count)
{
    liot_stat_ext_s st;
    char *buf = NULL;
    char *line;
    LFILE_EXT fd;
    unsigned int count = 0u;
    int read_len;
    int ret;

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if ((out_count == NULL) || ((items == NULL) && (max_items != 0u))) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(DEMO_LVGL_PHOTO_FS_INDEX_FILE, &st);
    if ((ret == LIOT_EXTFLASH_NOT_EXIST) || (ret == LIOT_EXTFLASH_STAT_FAIL)) {
        demo_lvgl_photo_fs_store_access_end();
        return 0;
    }
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE)) {
        demo_lvgl_photo_fs_store_access_end();
        return ret;
    }
    if ((st.size == 0u) || (st.size > DEMO_LVGL_PHOTO_FS_INDEX_MAX_SIZE)) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    buf = (char *)liot_rtos_malloc(st.size + 1u);
    if (buf == NULL) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(DEMO_LVGL_PHOTO_FS_INDEX_FILE, "r");
    if (fd <= 0) {
        liot_rtos_free(buf);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(buf, st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();
    if (read_len != (int)st.size) {
        liot_rtos_free(buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }
    buf[st.size] = '\0';

    line = buf;
    while ((line != NULL) && (*line != '\0') && (count < max_items)) {
        char *next;

        next = strchr(line, '\n');
        if (next != NULL) {
            *next = '\0';
            ++next;
        }

        if (demo_lvgl_photo_fs_store_parse_index_line(line, &items[count]) == 0) {
            ++count;
        }

        line = next;
    }

    liot_rtos_free(buf);
    *out_count = count;
    return 0;
}
