#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lierda_app_main.h"
#include "liot_audio2.h"
#include "liot_external_flash.h"
#include "liot_external_flash_fs.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "lvgl.h"
#include "gui_guider.h"

#include "demo_mp3.h"

#define MP3_LOG_PREFIX              "[demo_mp3]"
#define MP3_BOOT_DELAY_MS           (8000U)
#define MP3_AUDIO_READY_DELAY_MS    (2000U)

#define MP3_LDO33_EN_PAD            (106)
#define MP3_LDO33_EN_GPIO           (L_GPIO_25)
#define MP3_VCC3V3_EN_PAD           (16)
#define MP3_VCC3V3_EN_GPIO          (L_GPIO_27)

#define MP3_SPI_MOSI_PAD            (63)
#define MP3_SPI_MISO_PAD            (62)
#define MP3_SPI_SCLK_PAD            (49)
#define MP3_SPI_CS_PAD              (64)
#define MP3_SPI_CS_GPIO             (L_GPIO_12)
#define MP3_SPI_PIN_FUNC            (L_PIN_FUNC_1)

#define MP3_FLASH_SPI_PORT          (1U)
#define MP3_FLASH_BASE_ADDR         (0x000000U)
#define MP3_FLASH_TOTAL_SIZE        (0x800000U)
#define MP3_FS_BASE_ADDR            (0x010000U)
#define MP3_FS_TOTAL_SIZE           (0x200000U)
#define MP3_FS_BLOCK_SIZE           (4096U)
#define MP3_FS_READ_SIZE            (256U)
#define MP3_FS_PROG_SIZE            (256U)

#define MP3_DIR_PATH                "/flash/mp3"
#define MP3_COVER_W                 (360U)
#define MP3_COVER_H                 (360U)
#define MP3_DEFAULT_BITRATE         (56000U)
#define MP3_UI_UPDATE_MS            (250U)
#define MP3_SAFE_HW_VOLUME_MAX      (70U)
#define MP3_AUDIO_VOLUME            (MP3_SAFE_HW_VOLUME_MAX)
#define MP3_CODEC_VOLUME            (MP3_SAFE_HW_VOLUME_MAX)
#define MP3_AUTOKICK_DELAY_MS       (500U)
#define MP3_AUTOKICK_PAUSE_MS       (80U)
#define MP3_EXIT_HOLD_MS             (1200U)
#define MP3_EXIT_MOVE_LIMIT          (12)
#define MP3_STOP_SETTLE_MS          (80U)
#define MP3_RESTART_DELAY_MS        (100U)
#define MP3_REENTER_BLOCK_MS        (2000U)
#define MP3_EXIT_DELAY_MS           (300U)
#define MP3_EXIT_TASK_TIMEOUT_MS    (2000U)

extern const uint8_t g_demo_mp3_because_mp3_start[];
extern const uint8_t g_demo_mp3_because_mp3_end[];
extern const uint8_t g_demo_mp3_because_cover_start[];
extern const uint8_t g_demo_mp3_because_cover_end[];
extern void lvgl_init(void);

typedef struct {
    const char *title;
    const char *mp3_path;
    const char *cover_path;
    const uint8_t *mp3_start;
    const uint8_t *mp3_end;
    const uint8_t *cover_start;
    const uint8_t *cover_end;
    uint32_t bitrate;
} mp3_track_t;

static const mp3_track_t g_mp3_tracks[] = {
    {
        "Because Of You",
        "/flash/mp3/because_of_you.mp3",
        "/flash/mp3/because_of_you_cover_360x360_v2.rgb565",
        g_demo_mp3_because_mp3_start,
        g_demo_mp3_because_mp3_end,
        g_demo_mp3_because_cover_start,
        g_demo_mp3_because_cover_end,
        MP3_DEFAULT_BITRATE,
    },
};

#define MP3_TRACK_COUNT             (sizeof(g_mp3_tracks) / sizeof(g_mp3_tracks[0]))

static const liot_ext_flash_cfg_t g_mp3_flash_cfg = {
    .spi_port = MP3_FLASH_SPI_PORT,
    .base_addr = MP3_FLASH_BASE_ADDR,
    .total_size = MP3_FLASH_TOTAL_SIZE,
};

static const liot_ext_fs_cfg_t g_mp3_fs_cfg = {
    .base_addr = MP3_FS_BASE_ADDR,
    .total_size = MP3_FS_TOTAL_SIZE,
    .block_size = MP3_FS_BLOCK_SIZE,
    .read_size = MP3_FS_READ_SIZE,
    .prog_size = MP3_FS_PROG_SIZE,
};

typedef enum {
    MP3_STATE_STOPPED = 0,
    MP3_STATE_PLAYING,
    MP3_STATE_PAUSED,
    MP3_STATE_LOADING,
    MP3_STATE_ERROR,
} mp3_state_t;

typedef struct {
    bool fs_ready;
    bool audio_ready;
    bool mp3_cache_ready;
    bool stop_req;
    bool restart_req;
    bool ui_ready;
    bool play_finished;
    bool page_active;
    bool page_exiting;
    bool page_del;
    volatile bool load_done;
    volatile bool load_ok;
    mp3_state_t state;
    uint32_t last_exit_tick;
    uint32_t file_size;
    uint32_t duration_ms;
    uint32_t started_tick;
    uint32_t paused_tick;
    uint32_t paused_total_ms;
    uint32_t pause_started_ms;
    uint32_t current_track;
    uint32_t exit_press_tick;
    uint32_t exit_started_tick;
    lv_point_t exit_press_point;
    bool exit_press_candidate;
    uint8_t *track_data[MP3_TRACK_COUNT];
    uint32_t track_size[MP3_TRACK_COUNT];
    uint8_t *track_play_data[MP3_TRACK_COUNT];
    uint32_t track_play_size[MP3_TRACK_COUNT];
    liot_task_t play_task;
    liot_task_t load_task;
    lv_obj_t *screen;
    lv_obj_t *cover_img;
    lv_obj_t *title_label;
    lv_obj_t *state_label;
    lv_obj_t *time_label;
    lv_obj_t *play_btn_label;
    lv_obj_t *bar;
    lv_timer_t *ui_timer;
    lv_timer_t *exit_timer;
    uint8_t *cover_data;
    lv_img_dsc_t cover_dsc;
} demo_mp3_ctx_t;

static demo_mp3_ctx_t g_mp3;

static const char *mp3_state_name(mp3_state_t s);
static void mp3_set_state_from(mp3_state_t state, const char *reason);
#define mp3_set_state(s) mp3_set_state_from((s), __func__)
static void mp3_update_ui(void);
static void mp3_refresh_track_ui(void);
static void mp3_start_play(void);
static void mp3_start_load_task(void);
static void demo_mp3_handle_load_done(void);
static void demo_mp3_page_exit_timer_cb(lv_timer_t *timer);
static void demo_mp3_page_exit_request(void);

static void mp3_at_log(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len > 0) {
        liot_trace("%s\n", buf);
    }
}

#define MP3_LOG(fmt, ...) mp3_at_log(fmt, ##__VA_ARGS__)

static uint32_t mp3_seed_size(const uint8_t *start, const uint8_t *end)
{
    return (uint32_t)(end - start);
}

static const mp3_track_t *mp3_current_track(void)
{
    if (g_mp3.current_track >= MP3_TRACK_COUNT) {
        g_mp3.current_track = 0;
    }
    return &g_mp3_tracks[g_mp3.current_track];
}

static void mp3_set_track_info(const mp3_track_t *track, uint32_t file_size)
{
    uint32_t bitrate = (track->bitrate == 0U) ? MP3_DEFAULT_BITRATE : track->bitrate;

    g_mp3.file_size = file_size;
    g_mp3.duration_ms = (uint32_t)(((uint64_t)file_size * 8ULL * 1000ULL) / bitrate);
}

static bool mp3_set_embedded_cover_desc(void)
{
    const mp3_track_t *track = mp3_current_track();
    uint32_t cover_size = mp3_seed_size(track->cover_start, track->cover_end);

    if ((track->cover_start == NULL) ||
        (cover_size != (MP3_COVER_W * MP3_COVER_H * 2U))) {
        return false;
    }

    memset(&g_mp3.cover_dsc, 0, sizeof(g_mp3.cover_dsc));
    g_mp3.cover_dsc.header.always_zero = 0;
    g_mp3.cover_dsc.header.w = MP3_COVER_W;
    g_mp3.cover_dsc.header.h = MP3_COVER_H;
    g_mp3.cover_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    g_mp3.cover_dsc.data_size = cover_size;
    g_mp3.cover_dsc.data = track->cover_start;
    return true;
}

static uint32_t mp3_find_frame_offset(const uint8_t *data, uint32_t size)
{
    uint32_t i;

    if ((data == NULL) || (size < 4U)) {
        return 0;
    }

    for (i = 0; i + 3U < size; i++) {
        if ((data[i] == 0xFFU) &&
            ((data[i + 1U] & 0xE0U) == 0xE0U) &&
            ((data[i + 1U] & 0x18U) != 0x08U) &&
            ((data[i + 1U] & 0x06U) != 0x00U)) {
            return i;
        }
    }

    return 0;
}

static uint32_t mp3_now_ms(void)
{
    return liot_rtos_get_system_tick();
}

static uint32_t mp3_elapsed_ms(uint32_t start)
{
    return (uint32_t)(mp3_now_ms() - start);
}

static void mp3_format_time(char *buf, size_t len, uint32_t ms)
{
    uint32_t sec = ms / 1000U;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(sec / 60U), (unsigned long)(sec % 60U));
}

static uint32_t mp3_current_ms(void)
{
    uint32_t elapsed;

    if (g_mp3.state == MP3_STATE_STOPPED ||
        g_mp3.state == MP3_STATE_LOADING ||
        g_mp3.state == MP3_STATE_ERROR) {
        return 0;
    }
    if (g_mp3.state == MP3_STATE_PAUSED) {
        return g_mp3.pause_started_ms;
    }

    elapsed = mp3_elapsed_ms(g_mp3.started_tick);
    if (elapsed > g_mp3.paused_total_ms) {
        elapsed -= g_mp3.paused_total_ms;
    } else {
        elapsed = 0;
    }
    if ((g_mp3.duration_ms > 0U) && (elapsed > g_mp3.duration_ms)) {
        elapsed = g_mp3.duration_ms;
    }
    return elapsed;
}

static void mp3_audio_callback(Liot_AudEvent_e event, void *context)
{
    (void)context;

    switch (event) {
    case L_AUD_EVT_START:
        mp3_at_log("%s EVT_START state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        break;
    case L_AUD_EVT_FINISH: {
        uint32_t played = mp3_elapsed_ms(g_mp3.started_tick);
        mp3_at_log("%s EVT_FINISH state=%s played=%lums duration=%lums",
                   MP3_LOG_PREFIX, mp3_state_name(g_mp3.state),
                   (unsigned long)played, (unsigned long)g_mp3.duration_ms);
        if (g_mp3.state == MP3_STATE_PLAYING) {
            g_mp3.play_finished = true;
        }
        break;
    }
    case L_AUD_EVT_CLOSE:
        mp3_at_log("%s EVT_CLOSE state=%s", MP3_LOG_PREFIX, mp3_state_name(g_mp3.state));
        break;
    default:
        break;
    }
}

static int32_t mp3_power_gpio_enable(const char *name, int pad, liot_gpio_e gpio)
{
    liot_gpioerr_e ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (ret == L_GPIO_ERR_SUCCESS) {
        return 0;
    }

    liot_trace("%s %s direct gpio failed=%ld, try pad=%d", MP3_LOG_PREFIX, name, (long)ret, pad);
    ret = Liot_SetPinFunc(pad, L_PIN_FUNC_0);
    if (ret != L_GPIO_ERR_SUCCESS) {
        return (int32_t)ret;
    }
    ret = Liot_GpioInit(gpio, L_IO_OUTPUT, L_IO_HIGH, NULL);
    return (ret == L_GPIO_ERR_SUCCESS) ? 0 : (int32_t)ret;
}

static int32_t mp3_flash_hw_init(void)
{
    int32_t ret;

    ret = Liot_AonPowerCtl(true);
    if (ret != 0) return ret;
    ret = Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    if (ret != 0) return ret;

    ret = mp3_power_gpio_enable("LDO33", MP3_LDO33_EN_PAD, MP3_LDO33_EN_GPIO);
    if (ret != 0) return ret;
    liot_rtos_task_sleep_ms(10);

    ret = mp3_power_gpio_enable("VCC3V3", MP3_VCC3V3_EN_PAD, MP3_VCC3V3_EN_GPIO);
    if (ret != 0) return ret;
    liot_rtos_task_sleep_ms(20);

    if (Liot_SetPinFunc(MP3_SPI_MOSI_PAD, MP3_SPI_PIN_FUNC) != L_GPIO_ERR_SUCCESS) return -1;
    if (Liot_SetPinFunc(MP3_SPI_MISO_PAD, MP3_SPI_PIN_FUNC) != L_GPIO_ERR_SUCCESS) return -1;
    if (Liot_SetPinFunc(MP3_SPI_SCLK_PAD, MP3_SPI_PIN_FUNC) != L_GPIO_ERR_SUCCESS) return -1;
    if (Liot_SetPinFunc(MP3_SPI_CS_PAD, L_PIN_FUNC_0) != L_GPIO_ERR_SUCCESS) return -1;
    if (Liot_GpioInit(MP3_SPI_CS_GPIO, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS) return -1;

    return 0;
}

static int mp3_ensure_dir(const char *path)
{
    liot_stat_ext_s st;
    int ret = liot_mkdir_ext(path, 0);
    if (ret == LIOT_EXTFLASH_OK) return 0;
    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(path, &st);
    if ((ret == LIOT_EXTFLASH_OK) && (st.type == LIOT_EXTFLASH_TYPE_DIR)) return 0;
    return LIOT_EXTFLASH_MKDIR_FAIL;
}

static int mp3_write_seed_file(const char *path, const uint8_t *data, uint32_t size)
{
    LFILE_EXT fd;
    uint32_t offset = 0;
    int ret;

    fd = liot_fopen_ext(path, "w+");
    if (fd <= 0) {
        liot_trace("%s open for write failed path=%s fd=%d", MP3_LOG_PREFIX, path, (int)fd);
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    while (offset < size) {
        uint32_t chunk = size - offset;
        if (chunk > 4096U) chunk = 4096U;
        ret = liot_fwrite_ext((void *)(data + offset), chunk, 1, fd);
        if (ret != (int)chunk) {
            liot_trace("%s write failed path=%s off=%lu chunk=%lu ret=%d", MP3_LOG_PREFIX, path,
                       (unsigned long)offset, (unsigned long)chunk, ret);
            (void)liot_fclose_ext(fd);
            return LIOT_EXTFLASH_WRITE_FAIL;
        }
        offset += chunk;
    }

    (void)liot_fsync_ext(fd);
    ret = liot_fclose_ext(fd);
    return ret;
}

static int mp3_seed_file_if_needed(const char *path, const uint8_t *data, uint32_t size)
{
    liot_stat_ext_s st;
    int ret;

    memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(path, &st);
    if ((ret == LIOT_EXTFLASH_OK) && (st.type == LIOT_EXTFLASH_TYPE_FILE) && (st.size == size)) {
        liot_trace("%s file exists path=%s size=%lu", MP3_LOG_PREFIX, path, (unsigned long)size);
        return 0;
    }

    liot_trace("%s seed file path=%s size=%lu", MP3_LOG_PREFIX, path, (unsigned long)size);
    (void)liot_remove_ext(path);
    return mp3_write_seed_file(path, data, size);
}

static __attribute__((unused)) int mp3_storage_init(void)
{
    int ret;
    size_t i;

    if (g_mp3.fs_ready) return 0;

    mp3_at_log("%s storage flash hw init begin", MP3_LOG_PREFIX);
    ret = mp3_flash_hw_init();
    mp3_at_log("%s storage flash hw init ret=%d", MP3_LOG_PREFIX, ret);
    if (ret != 0) {
        liot_trace("%s flash hw init failed ret=%d", MP3_LOG_PREFIX, ret);
        return ret;
    }

    mp3_at_log("%s storage flash init begin", MP3_LOG_PREFIX);
    ret = liot_flash_init_ext(&g_mp3_flash_cfg);
    mp3_at_log("%s storage flash init ret=%d", MP3_LOG_PREFIX, ret);
    if (ret != 0) {
        liot_trace("%s liot_flash_init_ext failed ret=%d", MP3_LOG_PREFIX, ret);
        return ret;
    }

    mp3_at_log("%s storage fs mount begin base=0x%lx size=0x%lx", MP3_LOG_PREFIX,
               (unsigned long)MP3_FS_BASE_ADDR, (unsigned long)MP3_FS_TOTAL_SIZE);
    ret = liot_finit_ext(&g_mp3_fs_cfg);
    mp3_at_log("%s storage fs mount ret=%d", MP3_LOG_PREFIX, ret);
    if (ret != LIOT_EXTFLASH_OK) {
        liot_trace("%s mount failed ret=%d, format once", MP3_LOG_PREFIX, ret);
        ret = liot_fformat_ext();
        mp3_at_log("%s storage fs format ret=%d", MP3_LOG_PREFIX, ret);
        if (ret != LIOT_EXTFLASH_OK) return ret;
        (void)liot_fdeinit_ext();
        ret = liot_finit_ext(&g_mp3_fs_cfg);
        mp3_at_log("%s storage fs remount ret=%d", MP3_LOG_PREFIX, ret);
        if (ret != LIOT_EXTFLASH_OK) return ret;
    }

    ret = mp3_ensure_dir("/flash");
    mp3_at_log("%s storage ensure /flash ret=%d", MP3_LOG_PREFIX, ret);
    if (ret != 0) return ret;
    ret = mp3_ensure_dir(MP3_DIR_PATH);
    mp3_at_log("%s storage ensure %s ret=%d", MP3_LOG_PREFIX, MP3_DIR_PATH, ret);
    if (ret != 0) return ret;

    for (i = 0; i < MP3_TRACK_COUNT; i++) {
        uint32_t mp3_size = mp3_seed_size(g_mp3_tracks[i].mp3_start, g_mp3_tracks[i].mp3_end);
        uint32_t cover_size = mp3_seed_size(g_mp3_tracks[i].cover_start, g_mp3_tracks[i].cover_end);

        ret = mp3_seed_file_if_needed(g_mp3_tracks[i].mp3_path, g_mp3_tracks[i].mp3_start, mp3_size);
        if (ret != 0) return ret;
        ret = mp3_seed_file_if_needed(g_mp3_tracks[i].cover_path, g_mp3_tracks[i].cover_start, cover_size);
        if (ret != 0) return ret;
    }

    mp3_set_track_info(mp3_current_track(), mp3_seed_size(mp3_current_track()->mp3_start, mp3_current_track()->mp3_end));
    g_mp3.fs_ready = true;
    liot_trace("%s storage ready tracks=%lu free=%d", MP3_LOG_PREFIX, (unsigned long)MP3_TRACK_COUNT,
               liot_exflash_free_size_get());
    return 0;
}

static void mp3_audio_init_once(void)
{
    Liot_AudHwConfig_t cfg;
    Liot_AudErr_e ret;

    if (g_mp3.audio_ready) return;

    liot_rtos_task_sleep_ms(MP3_AUDIO_READY_DELAY_MS);
    Liot_AonPowerCtl(TRUE);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    memset(&cfg, 0, sizeof(cfg));
    cfg.i2cNum = 0;
    cfg.i2sNum = 0;
    cfg.paGpioNum = -1;
    cfg.codecType = L_AUD_ES8375;
    cfg.channel = L_AUD_MONO_RIGHT;
    cfg.role = L_AUD_ROLE_SLAVE;
    cfg.mode = L_AUD_MODE_I2S;
    cfg.frameSize = L_AUD_FRAMESIZE_16_16;
    cfg.samples = L_AUD_44K_SAMPLES;
    cfg.callback = mp3_audio_callback;

    ret = Liot_AudioInit(&cfg);
    liot_trace("%s AudioInit ret=%d", MP3_LOG_PREFIX, ret);
    if (ret == L_AUD_ERR_SUCCESS) {
        Liot_AudioSetVolume(MP3_AUDIO_VOLUME);
        Liot_AudioSetCodecVolume(MP3_CODEC_VOLUME);
        Liot_AudioSetMicVolume(8, 200);
        g_mp3.audio_ready = true;
    }
}

void demo_mp3_audio_preinit(void)
{
    mp3_audio_init_once();
}

bool demo_mp3_audio_is_ready(void)
{
    return g_mp3.audio_ready;
}

static __attribute__((unused)) uint8_t *mp3_load_ext_file(const char *path, uint32_t *out_size)
{
    LFILE_EXT fd;
    int size;
    int ret;
    uint8_t *buf;

    *out_size = 0;
    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        liot_trace("%s open read failed path=%s fd=%d", MP3_LOG_PREFIX, path, (int)fd);
        return NULL;
    }

    size = liot_fsize_ext(fd);
    if (size <= 0) {
        liot_trace("%s file size invalid path=%s size=%d", MP3_LOG_PREFIX, path, size);
        (void)liot_fclose_ext(fd);
        return NULL;
    }

    buf = (uint8_t *)malloc((size_t)size);
    if (buf == NULL) {
        liot_trace("%s malloc mp3 failed size=%d", MP3_LOG_PREFIX, size);
        (void)liot_fclose_ext(fd);
        return NULL;
    }

    ret = liot_fread_ext(buf, (size_t)size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (ret != size) {
        liot_trace("%s read failed path=%s expect=%d ret=%d", MP3_LOG_PREFIX, path, size, ret);
        free(buf);
        return NULL;
    }

    *out_size = (uint32_t)size;
    return buf;
}

static uint8_t *mp3_load_embedded_file(const mp3_track_t *track, uint32_t *out_size)
{
    uint32_t size;
    uint8_t *buf;

    *out_size = 0;
    size = mp3_seed_size(track->mp3_start, track->mp3_end);
    if ((track->mp3_start == NULL) || (size == 0U)) {
        liot_trace("%s embedded mp3 invalid title=%s size=%lu",
                   MP3_LOG_PREFIX, track->title, (unsigned long)size);
        return NULL;
    }

    buf = (uint8_t *)malloc((size_t)size);
    if (buf == NULL) {
        liot_trace("%s malloc embedded mp3 failed title=%s size=%lu",
                   MP3_LOG_PREFIX, track->title, (unsigned long)size);
        return NULL;
    }

    memcpy(buf, track->mp3_start, (size_t)size);
    *out_size = size;
    return buf;
}
static int mp3_cache_tracks_in_ram(void)
{
    size_t i;

    if (g_mp3.mp3_cache_ready) return 0;

    for (i = 0; i < MP3_TRACK_COUNT; i++) {
        if (g_mp3.track_data[i] != NULL) continue;

        g_mp3.track_data[i] = mp3_load_embedded_file(&g_mp3_tracks[i], &g_mp3.track_size[i]);
        if (g_mp3.track_data[i] == NULL) {
            liot_trace("%s cache failed title=%s", MP3_LOG_PREFIX, g_mp3_tracks[i].title);
            return -1;
        }
        {
            uint32_t frame_offset = mp3_find_frame_offset(g_mp3.track_data[i], g_mp3.track_size[i]);
            g_mp3.track_play_data[i] = g_mp3.track_data[i] + frame_offset;
            g_mp3.track_play_size[i] = g_mp3.track_size[i] - frame_offset;
            liot_trace("%s cached title=%s size=%lu frame_offset=%lu play_size=%lu",
                       MP3_LOG_PREFIX, g_mp3_tracks[i].title,
                       (unsigned long)g_mp3.track_size[i],
                       (unsigned long)frame_offset,
                       (unsigned long)g_mp3.track_play_size[i]);
        }
    }

    g_mp3.mp3_cache_ready = true;
    mp3_set_track_info(mp3_current_track(), g_mp3.track_play_size[g_mp3.current_track]);
    return 0;
}

static const char *mp3_state_name(mp3_state_t s)
{
    switch (s) {
    case MP3_STATE_STOPPED: return "STOPPED";
    case MP3_STATE_PLAYING: return "PLAYING";
    case MP3_STATE_PAUSED:  return "PAUSED";
    case MP3_STATE_LOADING: return "LOADING";
    case MP3_STATE_ERROR:   return "ERROR";
    default: return "?";
    }
}

static void mp3_set_state_from(mp3_state_t state, const char *reason)
{
    mp3_at_log("%s STATE %s -> %s (%s)", MP3_LOG_PREFIX,
               mp3_state_name(g_mp3.state), mp3_state_name(state), reason);
    g_mp3.state = state;
}

static void mp3_playback_task(void *argv)
{
    const mp3_track_t *track;
    uint8_t *mp3_data = NULL;
    uint32_t mp3_len = 0;
    uint32_t track_index;
    Liot_AudErr_e ret;
    bool restart_req;

    (void)argv;
    g_mp3.stop_req = false;
    g_mp3.paused_total_ms = 0;
    g_mp3.pause_started_ms = 0;

    if (!g_mp3.mp3_cache_ready || !g_mp3.audio_ready) {
        mp3_set_state(MP3_STATE_LOADING);
    }

    if (mp3_cache_tracks_in_ram() != 0) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }

    mp3_audio_init_once();
    if (!g_mp3.audio_ready) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }

    track_index = g_mp3.current_track;
    if (track_index >= MP3_TRACK_COUNT) {
        track_index = 0;
        g_mp3.current_track = 0;
    }
    track = &g_mp3_tracks[track_index];
    mp3_data = g_mp3.track_play_data[track_index];
    mp3_len = g_mp3.track_play_size[track_index];
    if (mp3_data == NULL) {
        mp3_set_state(MP3_STATE_ERROR);
        goto done;
    }
    mp3_set_track_info(track, mp3_len);

    g_mp3.started_tick = mp3_now_ms();
    mp3_set_state(MP3_STATE_PLAYING);

    (void)Liot_AudioStop();
    liot_rtos_task_sleep_ms(100);
    ret = Liot_AudioPlayMp3(mp3_data, (int)mp3_len);
    mp3_at_log("%s PlayMp3 title=%s ret=%d len=%lu", MP3_LOG_PREFIX, track->title, (int)ret, (unsigned long)mp3_len);
    if (ret == L_AUD_ERR_SUCCESS) {
        liot_rtos_task_sleep_ms(MP3_AUTOKICK_DELAY_MS);
        if (!g_mp3.stop_req) {
            (void)Liot_AudioPlayPause();
            liot_rtos_task_sleep_ms(MP3_AUTOKICK_PAUSE_MS);
            (void)Liot_AudioPlayResume();
        }
        g_mp3.play_finished = false;
        mp3_at_log("%s wait_start stop_req=%d", MP3_LOG_PREFIX, (int)g_mp3.stop_req);
        while (!g_mp3.stop_req && !g_mp3.play_finished) {
            liot_rtos_task_sleep_ms(100);
        }
        mp3_at_log("%s wait_exit stop_req=%d finished=%d elapsed=%lums",
                   MP3_LOG_PREFIX, (int)g_mp3.stop_req, (int)g_mp3.play_finished,
                   (unsigned long)mp3_elapsed_ms(g_mp3.started_tick));
        if (g_mp3.stop_req) {
            (void)Liot_AudioStop();
            liot_rtos_task_sleep_ms(200);
        }
    } else {
        mp3_set_state(MP3_STATE_ERROR);
    }

    if (!g_mp3.stop_req && g_mp3.state != MP3_STATE_ERROR) {
        mp3_set_state(MP3_STATE_STOPPED);
    }

done:
    restart_req = g_mp3.restart_req;
    g_mp3.restart_req = false;
    g_mp3.play_task = NULL;
    if (restart_req && g_mp3.page_active && !g_mp3.page_exiting) {
        mp3_set_state(MP3_STATE_STOPPED);
        liot_rtos_task_sleep_ms(MP3_RESTART_DELAY_MS);
        mp3_start_play();
    }
    liot_rtos_task_delete(NULL);
}

static void mp3_start_play(void)
{
    if (g_mp3.page_exiting) return;

    if (g_mp3.state == MP3_STATE_PAUSED) {
        g_mp3.paused_total_ms += mp3_elapsed_ms(g_mp3.paused_tick);
        (void)Liot_AudioPlayResume();
        mp3_set_state(MP3_STATE_PLAYING);
        return;
    }

    if ((g_mp3.state == MP3_STATE_PLAYING) || (g_mp3.state == MP3_STATE_LOADING)) return;
    if (g_mp3.play_task != NULL) {
        g_mp3.restart_req = true;
        return;
    }

    g_mp3.stop_req = false;
    liot_rtos_task_create(&g_mp3.play_task, 10 * 1024, LIOT_APP_TASK_PRIORITY,
                          "mp3_playback", mp3_playback_task, NULL);
}

static void mp3_pause_play(void)
{
    if (g_mp3.state != MP3_STATE_PLAYING) return;
    g_mp3.pause_started_ms = mp3_current_ms();
    g_mp3.paused_tick = mp3_now_ms();
    (void)Liot_AudioPlayPause();
    mp3_set_state(MP3_STATE_PAUSED);
}

static void mp3_request_restart_play(void)
{
    if (g_mp3.play_task != NULL) {
        g_mp3.stop_req = true;
        g_mp3.restart_req = true;
        mp3_set_state(MP3_STATE_LOADING);
        return;
    }

    mp3_start_play();
}

static void mp3_update_ui(void)
{
    char cur[12];
    char total[12];
    char text[32];
    uint32_t current_ms = mp3_current_ms();
    const char *state_text = "Stopped";

    if (!g_mp3.ui_ready) return;

    switch (g_mp3.state) {
    case MP3_STATE_PLAYING: state_text = "Playing"; break;
    case MP3_STATE_PAUSED: state_text = "Paused"; break;
    case MP3_STATE_LOADING: state_text = "Loading"; break;
    case MP3_STATE_ERROR: state_text = "Error"; break;
    default: break;
    }

    mp3_format_time(cur, sizeof(cur), current_ms);
    mp3_format_time(total, sizeof(total), g_mp3.duration_ms);
    snprintf(text, sizeof(text), "%s / %s", cur, total);
    lv_label_set_text(g_mp3.time_label, text);
    lv_label_set_text(g_mp3.state_label, state_text);
    lv_label_set_text(g_mp3.play_btn_label, (g_mp3.state == MP3_STATE_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (g_mp3.duration_ms > 0U) {
        int value = (int)((current_ms * 100U) / g_mp3.duration_ms);
        if (value > 100) value = 100;
        lv_bar_set_value(g_mp3.bar, value, LV_ANIM_OFF);
    }
}

static void mp3_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_mp3.load_done) {
        demo_mp3_handle_load_done();
    }
    mp3_update_ui();
}

static void mp3_switch_track(int delta)
{
    uint32_t next_track;

    if (delta < 0) {
        next_track = (g_mp3.current_track + MP3_TRACK_COUNT - 1U) % MP3_TRACK_COUNT;
    } else {
        next_track = (g_mp3.current_track + 1U) % MP3_TRACK_COUNT;
    }
    g_mp3.current_track = next_track;
    mp3_refresh_track_ui();
    mp3_request_restart_play();
}

static void mp3_prev_event_cb(lv_event_t *e)
{
    (void)e;
    mp3_switch_track(-1);
}

static void mp3_next_event_cb(lv_event_t *e)
{
    (void)e;
    mp3_switch_track(1);
}

static void mp3_play_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_mp3.state == MP3_STATE_PLAYING) {
        mp3_pause_play();
    } else {
        mp3_start_play();
    }
    mp3_update_ui();
}

static void mp3_delete_timer(lv_timer_t **timer)
{
    if ((timer != NULL) && (*timer != NULL)) {
        lv_timer_del(*timer);
        *timer = NULL;
    }
}

static void demo_mp3_page_exit(void)
{
    lv_obj_t *active_scr = lv_scr_act();

    if (!g_mp3.page_active && (g_mp3.screen == NULL)) {
        mp3_at_log("%s exit ignored inactive active=%p", MP3_LOG_PREFIX, active_scr);
        return;
    }

    g_mp3.stop_req = true;
    g_mp3.restart_req = false;
    g_mp3.load_done = false;
    g_mp3.load_ok = false;
    (void)Liot_AudioStop();

    if (g_mp3.play_task != NULL) {
        if ((g_mp3.exit_started_tick != 0U) &&
            (mp3_elapsed_ms(g_mp3.exit_started_tick) >= MP3_EXIT_TASK_TIMEOUT_MS)) {
            liot_task_t stale_task = g_mp3.play_task;
            g_mp3.play_task = NULL;
            mp3_at_log("%s exit force delete play_task=%p state=%s",
                       MP3_LOG_PREFIX, stale_task, mp3_state_name(g_mp3.state));
            liot_rtos_task_delete(stale_task);
        } else {
            mp3_at_log("%s exit wait play_task=%p state=%s",
                       MP3_LOG_PREFIX, g_mp3.play_task, mp3_state_name(g_mp3.state));
            if (g_mp3.exit_timer == NULL) {
                g_mp3.exit_timer = lv_timer_create(demo_mp3_page_exit_timer_cb, 200, NULL);
                lv_timer_set_repeat_count(g_mp3.exit_timer, 1);
            }
            return;
        }
    }
    mp3_at_log("%s exit begin active=%p screen=%p menu=%p",
               MP3_LOG_PREFIX, active_scr, g_mp3.screen, guider_ui.mp3);

    g_mp3.page_active = false;
    g_mp3.page_exiting = false;
    g_mp3.exit_started_tick = 0U;
    g_mp3.last_exit_tick = lv_tick_get();
    g_mp3.ui_ready = false;
    mp3_delete_timer(&g_mp3.ui_timer);
    mp3_delete_timer(&g_mp3.exit_timer);
    mp3_set_state(MP3_STATE_STOPPED);

    ui_load_scr_animation(&guider_ui,
                          &guider_ui.mp3,
                          guider_ui.mp3_del,
                          &g_mp3.page_del,
                          setup_scr_mp3,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          0,
                          false,
                          false);

    g_mp3.screen = NULL;
    g_mp3.cover_img = NULL;
    g_mp3.title_label = NULL;
    g_mp3.state_label = NULL;
    g_mp3.time_label = NULL;
    g_mp3.play_btn_label = NULL;
    g_mp3.bar = NULL;
    g_mp3.page_del = true;
    mp3_at_log("%s exit end menu=%p", MP3_LOG_PREFIX, guider_ui.mp3);
}

static void demo_mp3_page_exit_timer_cb(lv_timer_t *timer)
{
    if (g_mp3.exit_timer == timer) {
        g_mp3.exit_timer = NULL;
    }
    lv_timer_del(timer);
    demo_mp3_page_exit();
}

static void demo_mp3_page_exit_request(void)
{
    lv_indev_t *indev;

    if (g_mp3.page_exiting) {
        mp3_at_log("%s exit request ignored", MP3_LOG_PREFIX);
        return;
    }

    mp3_at_log("%s exit request active=%p screen=%p", MP3_LOG_PREFIX, lv_scr_act(), g_mp3.screen);
    g_mp3.page_exiting = true;
    g_mp3.exit_started_tick = mp3_now_ms();
    g_mp3.stop_req = true;
    g_mp3.restart_req = false;
    (void)Liot_AudioStop();

    indev = lv_indev_get_act();
    if (indev != NULL) {
        lv_indev_wait_release(indev);
    }

    if (g_mp3.exit_timer == NULL) {
        g_mp3.exit_timer = lv_timer_create(demo_mp3_page_exit_timer_cb, MP3_EXIT_DELAY_MS, NULL);
        lv_timer_set_repeat_count(g_mp3.exit_timer, 1);
    }
}

static void mp3_page_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev;
    lv_point_t point;
    int32_t dx;
    int32_t dy;

    if (code == LV_EVENT_PRESSED) {
        g_mp3.exit_press_candidate =
            (lv_event_get_target(event) == lv_event_get_current_target(event));
        if (!g_mp3.exit_press_candidate) return;
        indev = lv_indev_get_act();
        if (indev == NULL) {
            g_mp3.exit_press_candidate = false;
            return;
        }
        lv_indev_get_point(indev, &g_mp3.exit_press_point);
        g_mp3.exit_press_tick = lv_tick_get();
        return;
    }

    if (code == LV_EVENT_PRESSING && g_mp3.exit_press_candidate) {
        indev = lv_indev_get_act();
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        dx = point.x - g_mp3.exit_press_point.x;
        dy = point.y - g_mp3.exit_press_point.y;
        if (dx > MP3_EXIT_MOVE_LIMIT || dx < -MP3_EXIT_MOVE_LIMIT ||
            dy > MP3_EXIT_MOVE_LIMIT || dy < -MP3_EXIT_MOVE_LIMIT) {
            g_mp3.exit_press_candidate = false;
            return;
        }
        if (lv_tick_elaps(g_mp3.exit_press_tick) >= MP3_EXIT_HOLD_MS) {
            g_mp3.exit_press_candidate = false;
            lv_event_stop_bubbling(event);
            lv_event_stop_processing(event);
            demo_mp3_page_exit_request();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_mp3.exit_press_candidate = false;
    }
}

static lv_obj_t *mp3_create_btn(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 64, 44);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_center(label);
    return label;
}

static void mp3_refresh_track_ui(void)
{
    if (!g_mp3.ui_ready) return;

    lv_label_set_text(g_mp3.title_label, mp3_current_track()->title);
    if (mp3_set_embedded_cover_desc() && (g_mp3.cover_img != NULL)) {
        lv_img_set_src(g_mp3.cover_img, &g_mp3.cover_dsc);
    }
    mp3_set_track_info(mp3_current_track(), g_mp3.track_play_size[g_mp3.current_track]);
    g_mp3.paused_total_ms = 0;
    g_mp3.pause_started_ms = 0;
    lv_bar_set_value(g_mp3.bar, 0, LV_ANIM_OFF);
    mp3_update_ui();
}

static void mp3_create_page(void)
{
    lv_obj_t *cover;
    lv_obj_t *cover_title;
    bool cover_ok = mp3_set_embedded_cover_desc();

    g_mp3.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_mp3.screen, lv_color_hex(0x10141F), 0);
    lv_obj_set_style_bg_opa(g_mp3.screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_mp3.screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_mp3.screen, mp3_page_event_cb, LV_EVENT_ALL, NULL);

    g_mp3.cover_img = lv_img_create(g_mp3.screen);
    lv_obj_set_pos(g_mp3.cover_img, 0, 0);
    if (cover_ok) {
        lv_img_set_src(g_mp3.cover_img, &g_mp3.cover_dsc);
    }

    g_mp3.title_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.title_label, mp3_current_track()->title);
    lv_obj_set_style_text_color(g_mp3.title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_mp3.title_label, &lv_font_DAIMENG_28, 0);
    lv_obj_align(g_mp3.title_label, LV_ALIGN_TOP_MID, 0, 14);

    if (!cover_ok) {
        cover = lv_obj_create(g_mp3.screen);
        lv_obj_set_size(cover, 220, 170);
        lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 52);
        lv_obj_set_style_radius(cover, 18, 0);
        lv_obj_set_style_bg_color(cover, lv_color_hex(0xF7B267), 0);
        lv_obj_set_style_bg_grad_color(cover, lv_color_hex(0x522B5B), 0);
        lv_obj_set_style_bg_grad_dir(cover, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(cover, 0, 0);

        cover_title = lv_label_create(cover);
        lv_label_set_text_fmt(cover_title, "MP3\n%s", mp3_current_track()->title);
        lv_obj_set_style_text_align(cover_title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(cover_title, lv_color_white(), 0);
        lv_obj_set_style_text_font(cover_title, &lv_font_montserratMedium_23, 0);
        lv_obj_center(cover_title);
    }

    g_mp3.state_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.state_label, "Loading");
    lv_obj_set_style_text_color(g_mp3.state_label, lv_color_hex(0xD8DEE9), 0);
    lv_obj_align(g_mp3.state_label, LV_ALIGN_TOP_MID, 0, 202);

    g_mp3.bar = lv_bar_create(g_mp3.screen);
    lv_obj_set_size(g_mp3.bar, 240, 12);
    lv_obj_align(g_mp3.bar, LV_ALIGN_TOP_MID, 0, 230);
    lv_bar_set_range(g_mp3.bar, 0, 100);
    lv_bar_set_value(g_mp3.bar, 0, LV_ANIM_OFF);

    g_mp3.time_label = lv_label_create(g_mp3.screen);
    lv_label_set_text(g_mp3.time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(g_mp3.time_label, lv_color_hex(0xD8DEE9), 0);
    lv_obj_set_style_text_font(g_mp3.time_label, &lv_font_montserratMedium_18, 0);
    lv_obj_align(g_mp3.time_label, LV_ALIGN_TOP_MID, 0, 248);

    mp3_create_btn(g_mp3.screen, LV_SYMBOL_PREV, 68, 272, mp3_prev_event_cb);
    g_mp3.play_btn_label = mp3_create_btn(g_mp3.screen, LV_SYMBOL_PLAY, 148, 272, mp3_play_event_cb);
    mp3_create_btn(g_mp3.screen, LV_SYMBOL_NEXT, 228, 272, mp3_next_event_cb);

    lv_scr_load_anim(g_mp3.screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    guider_ui.mp3_del = true;
    g_mp3.page_del = false;
    if (g_mp3.ui_timer != NULL) lv_timer_del(g_mp3.ui_timer);
    g_mp3.ui_timer = lv_timer_create(mp3_ui_timer_cb, MP3_UI_UPDATE_MS, NULL);
    g_mp3.ui_ready = true;
    mp3_update_ui();
}

static void demo_mp3_handle_load_done(void)
{
    bool ok = g_mp3.load_ok;

    g_mp3.load_done = false;
    if (!g_mp3.page_active || g_mp3.page_exiting) {
        mp3_at_log("%s load done ignored active=%d exiting=%d",
                   MP3_LOG_PREFIX, g_mp3.page_active ? 1 : 0, g_mp3.page_exiting ? 1 : 0);
        return;
    }

    mp3_at_log("%s load done ok=%d", MP3_LOG_PREFIX, ok ? 1 : 0);
    if (ok) {
        if ((g_mp3.cover_data != NULL) && (g_mp3.cover_img != NULL)) {
            lv_img_set_src(g_mp3.cover_img, &g_mp3.cover_dsc);
        }
        mp3_set_state(MP3_STATE_STOPPED);
        mp3_update_ui();
        mp3_start_play();
    } else {
        mp3_set_state(MP3_STATE_ERROR);
        mp3_update_ui();
    }
}

static void mp3_load_task(void *argv)
{
    bool ok = false;

    (void)argv;
    mp3_at_log("%s load task begin", MP3_LOG_PREFIX);
    mp3_at_log("%s load step cache begin", MP3_LOG_PREFIX);
    if (mp3_cache_tracks_in_ram() == 0) {
        mp3_at_log("%s load step cache ok", MP3_LOG_PREFIX);
        mp3_at_log("%s load step audio begin", MP3_LOG_PREFIX);
        mp3_audio_init_once();
        ok = g_mp3.audio_ready;
        mp3_at_log("%s load step audio ok=%d", MP3_LOG_PREFIX, ok ? 1 : 0);
    } else {
        mp3_at_log("%s load step cache failed", MP3_LOG_PREFIX);
    }
    mp3_at_log("%s load task end ok=%d", MP3_LOG_PREFIX, ok ? 1 : 0);
    g_mp3.load_ok = ok;
    g_mp3.load_done = true;
    g_mp3.load_task = NULL;
    liot_rtos_task_delete(NULL);
}

static void mp3_start_load_task(void)
{
    LiotOSStatus_t ret;

    if (g_mp3.load_task != NULL) {
        mp3_at_log("%s load task already running", MP3_LOG_PREFIX);
        return;
    }

    ret = liot_rtos_task_create(&g_mp3.load_task, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                                "mp3_load", mp3_load_task, NULL);
    mp3_at_log("%s create load task ret=%d handle=%p", MP3_LOG_PREFIX, (int)ret, g_mp3.load_task);
    if (ret != LIOT_OSI_SUCCESS) {
        g_mp3.load_task = NULL;
        mp3_set_state(MP3_STATE_ERROR);
        mp3_update_ui();
    }
}

void demo_mp3_page_enter(void)
{
    if (g_mp3.page_active) {
        mp3_at_log("%s enter ignored active", MP3_LOG_PREFIX);
        return;
    }
    if ((g_mp3.last_exit_tick != 0U) && (lv_tick_elaps(g_mp3.last_exit_tick) < MP3_REENTER_BLOCK_MS)) {
        mp3_at_log("%s enter ignored guard", MP3_LOG_PREFIX);
        return;
    }

    mp3_at_log("%s enter begin", MP3_LOG_PREFIX);
    g_mp3.page_active = true;
    g_mp3.page_exiting = false;
    g_mp3.stop_req = false;
    g_mp3.restart_req = false;
    g_mp3.load_done = false;
    g_mp3.load_ok = false;
    mp3_delete_timer(&g_mp3.ui_timer);
    mp3_delete_timer(&g_mp3.exit_timer);

    mp3_create_page();
    mp3_set_state(MP3_STATE_LOADING);
    mp3_update_ui();
    mp3_start_load_task();
    mp3_at_log("%s enter end", MP3_LOG_PREFIX);
}

static void demo_mp3_page_enter_cb(void *arg)
{
    (void)arg;
    demo_mp3_page_enter();
}

void demo_mp3_page_enter_async(void)
{
    lv_async_call(demo_mp3_page_enter_cb, NULL);
}

void liot_mp3_demo_thread(void *argv)
{
    (void)argv;
    liot_trace("%s demo start", MP3_LOG_PREFIX);
    liot_rtos_task_sleep_ms(MP3_BOOT_DELAY_MS);
    liot_trace("[demo_mp3] === MP3 DEMO START ===");
    liot_rtos_task_sleep_ms(1000);
    liot_trace("[demo_mp3] === MP3 DEMO START 2 ===");
    liot_rtos_task_sleep_ms(1000);
    liot_trace("[demo_mp3] === MP3 DEMO START 3 ===");
    lvgl_init();
    demo_mp3_page_enter_async();
    while (1) {
        liot_rtos_task_sleep_s(10);
    }
}
