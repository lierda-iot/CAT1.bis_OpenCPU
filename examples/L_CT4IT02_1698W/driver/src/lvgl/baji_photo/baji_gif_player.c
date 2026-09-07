#include "baji_gif_player.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "baji_photo_config.h"
#include "baji_photo_store.h"
#include "liot_external_flash_fs.h"
#include "liot_log.h"
#include "liot_os.h"
#include "mm_video_if.h"

#define BAJI_GIF_MIN_PERIOD_MS        10U
#define BAJI_GIF_DEFAULT_PERIOD_MS    40U
#define BAJI_GIF_TRACE(fmt, ...) liot_trace("[baji_gif] " fmt "\n", ##__VA_ARGS__)

typedef struct {
    void *decoder;
    lv_obj_t *img;
    lv_obj_t *screen;
    lv_timer_t *timer;
    lv_img_dsc_t frame_dsc;
    VIDEO_IMAGE_BUF canvas;
    GIF_INFO info;
    uint16_t *rgb565;
    uint8_t *src_buf;
    unsigned int src_size;
    const char *name;
    bool restart_pending;
    bool timer_paused;
    bool cleanup_direct_only;
    bool cleanup_queued;
    bool cleanup_started;
} baji_gif_player_t;

static void baji_gif_screen_event_cb(lv_event_t *e);
static void baji_gif_cleanup_async(void *user_data);

static void *baji_gif_alloc(unsigned int size)
{
    return liot_rtos_malloc(size);
}

static void baji_gif_free(void *buf)
{
    if (buf != NULL) {
        liot_rtos_free(buf);
    }
}

static unsigned int baji_gif_period_get(unsigned int duration)
{
    if (duration == 0U) {
        return BAJI_GIF_DEFAULT_PERIOD_MS;
    }
    if (duration < BAJI_GIF_MIN_PERIOD_MS) {
        return BAJI_GIF_MIN_PERIOD_MS;
    }
    return duration;
}

static bool baji_gif_decode_next(baji_gif_player_t *player,
                                 unsigned int *duration,
                                 unsigned int *eos)
{
    int ret;

    ret = GifD_DecodeImage(player->decoder, NULL, duration, eos);
    return (ret == 0);
}

static void baji_gif_timer_pause(baji_gif_player_t *player, const char *reason)
{
    if ((player == NULL) || (player->timer == NULL) || player->timer_paused) {
        return;
    }

    lv_timer_pause(player->timer);
    player->timer_paused = true;
    BAJI_GIF_TRACE("pause name=%s reason=%s", player->name, reason);
}

static void baji_gif_decoder_close(baji_gif_player_t *player)
{
    if (player->decoder != NULL) {
        GifD_Destroy(player->decoder);
        player->decoder = NULL;
    }
}

static bool baji_gif_canvas_set(baji_gif_player_t *player)
{
    player->canvas.eFmt = VIDEO_COLOR_FMT_RGB565;
    player->canvas.uWidth = (unsigned short)player->info.uWidth;
    player->canvas.uHeight = (unsigned short)player->info.uHeight;
    player->canvas.pData[0] = player->rgb565;
    player->canvas.pData[1] = NULL;
    player->canvas.pData[2] = NULL;

    return (GifD_SetCanvas(player->decoder, &player->canvas) == 0);
}

static bool baji_gif_dims_valid(uint32_t width, uint32_t height, uint32_t *out_frame_bytes)
{
    uint32_t pixels;

    if ((width == 0u) || (height == 0u) ||
        (width > BAJI_PHOTO_IMG_W) || (height > BAJI_PHOTO_IMG_H)) {
        return false;
    }
    if (width > (UINT32_MAX / height)) {
        return false;
    }

    pixels = width * height;
    if (pixels > (UINT32_MAX / (uint32_t)sizeof(uint16_t))) {
        return false;
    }

    if (out_frame_bytes != NULL) {
        *out_frame_bytes = pixels * (uint32_t)sizeof(uint16_t);
    }
    return true;
}

static bool baji_gif_decoder_open(baji_gif_player_t *player)
{
    GIF_INFO info;

    player->decoder = GifD_Create();
    if (player->decoder == NULL) {
        return false;
    }

    if (GifD_DecodeInfo(player->decoder, player->src_buf, player->src_size, &info) != 0) {
        baji_gif_decoder_close(player);
        return false;
    }
    if ((info.uWidth == 0U) || (info.uHeight == 0U)) {
        baji_gif_decoder_close(player);
        return false;
    }
    if (!baji_gif_dims_valid(info.uWidth, info.uHeight, NULL)) {
        baji_gif_decoder_close(player);
        return false;
    }

    if ((player->info.uWidth != 0U) &&
        ((info.uWidth != player->info.uWidth) || (info.uHeight != player->info.uHeight))) {
        baji_gif_decoder_close(player);
        return false;
    }

    player->info = info;
    return true;
}

static bool baji_gif_restart(baji_gif_player_t *player,
                             unsigned int *duration,
                             unsigned int *eos)
{
    baji_gif_decoder_close(player);
    memset(player->rgb565, 0, player->frame_dsc.data_size);

    if (!baji_gif_decoder_open(player)) {
        return false;
    }
    if (!baji_gif_canvas_set(player)) {
        return false;
    }

    player->restart_pending = false;
    return baji_gif_decode_next(player, duration, eos);
}

static void baji_gif_timer_cb(lv_timer_t *timer)
{
    baji_gif_player_t *player = (baji_gif_player_t *)timer->user_data;
    unsigned int duration = 0;
    unsigned int eos = 0;

    if ((player == NULL) || (player->decoder == NULL) || (player->img == NULL)) {
        return;
    }

    if (player->restart_pending) {
        if (!baji_gif_restart(player, &duration, &eos)) {
            BAJI_GIF_TRACE("restart failed name=%s", player->name);
            player->timer_paused = true;
            lv_timer_pause(timer);
            return;
        }
    } else if (!baji_gif_decode_next(player, &duration, &eos)) {
        BAJI_GIF_TRACE("decode failed name=%s", player->name);
        player->timer_paused = true;
        lv_timer_pause(timer);
        return;
    }

    lv_obj_invalidate(player->img);
    lv_timer_set_period(timer, baji_gif_period_get(duration));
    if (eos != 0U) {
        player->restart_pending = true;
    }
}

static void baji_gif_cleanup(baji_gif_player_t *player)
{
    if (player == NULL) {
        return;
    }
    if (player->cleanup_started) {
        return;
    }
    player->cleanup_started = true;

    baji_gif_timer_pause(player, "cleanup");

    if (player->timer != NULL) {
        BAJI_GIF_TRACE("timer delete name=%s", player->name);
        lv_timer_del(player->timer);
        player->timer = NULL;
    }
    if ((player->screen != NULL) && lv_obj_is_valid(player->screen)) {
        (void)lv_obj_remove_event_cb_with_user_data(player->screen,
                                                    baji_gif_screen_event_cb,
                                                    player);
        BAJI_GIF_TRACE("screen cb removed name=%s", player->name);
        player->screen = NULL;
    }
    baji_gif_decoder_close(player);
    baji_gif_free(player->rgb565);
    player->rgb565 = NULL;
    baji_gif_free(player->src_buf);
    player->src_buf = NULL;
    player->src_size = 0u;
    BAJI_GIF_TRACE("cleanup done name=%s", player->name);
    liot_rtos_free(player);
}

static void baji_gif_cleanup_async(void *user_data)
{
    baji_gif_player_t *player = (baji_gif_player_t *)user_data;

    if (player == NULL) {
        return;
    }

    baji_gif_cleanup(player);
}

static void baji_gif_event_cb(lv_event_t *e)
{
    baji_gif_player_t *player;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    player = (baji_gif_player_t *)lv_event_get_user_data(e);
    if (player != NULL) {
        BAJI_GIF_TRACE("img delete name=%s", player->name);
        baji_gif_timer_pause(player, "img_delete");
        baji_gif_decoder_close(player);
        player->img = NULL;
        if (player->cleanup_direct_only) {
            return;
        }
        if (!player->cleanup_queued) {
            lv_res_t ret;

            player->cleanup_queued = true;
            ret = lv_async_call(baji_gif_cleanup_async, player);
            if (ret != LV_RES_OK) {
                player->cleanup_queued = false;
                baji_gif_cleanup(player);
            }
        }
    }
}

static void baji_gif_screen_event_cb(lv_event_t *e)
{
    baji_gif_player_t *player;
    lv_event_code_t code;

    code = lv_event_get_code(e);
    if ((code != LV_EVENT_SCREEN_UNLOAD_START) &&
        (code != LV_EVENT_SCREEN_UNLOADED)) {
        return;
    }

    player = (baji_gif_player_t *)lv_event_get_user_data(e);
    if (player == NULL) {
        return;
    }

    if (code == LV_EVENT_SCREEN_UNLOAD_START) {
        baji_gif_timer_pause(player, "screen_unload_start");
        baji_gif_decoder_close(player);
        return;
    }

    baji_gif_timer_pause(player, "screen_unloaded");
}

static bool baji_gif_file_load(baji_gif_player_t *player, const char *gif_path)
{
    liot_stat_ext_s st;
    LFILE_EXT fd;
    int read_len;
    int ret;
    bool locked = false;
    bool ok = false;

    memset(&st, 0, sizeof(st));
    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return false;
    }
    locked = true;
    ret = liot_stat_ext(gif_path, &st);
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) || (st.size == 0u)) {
        goto out;
    }

    player->src_buf = (uint8_t *)baji_gif_alloc((unsigned int)st.size);
    if (player->src_buf == NULL) {
        goto out;
    }
    player->src_size = (unsigned int)st.size;

    fd = liot_fopen_ext(gif_path, "r");
    if (fd <= 0) {
        goto out;
    }

    read_len = liot_fread_ext(player->src_buf, player->src_size, 1, fd);
    (void)liot_fclose_ext(fd);
    if (read_len != (int)player->src_size) {
        goto out;
    }

    ok = true;

out:
    if (locked) {
        baji_photo_store_access_end();
    }
    if (!ok) {
        baji_gif_free(player->src_buf);
        player->src_buf = NULL;
        player->src_size = 0u;
    }
    return ok;
}

static bool baji_gif_player_init(baji_gif_player_t *player,
                                 lv_obj_t *parent,
                                 const char *gif_path,
                                 const char *name)
{
    unsigned int duration = 0;
    unsigned int eos = 0;
    uint32_t frame_bytes;

    memset(player, 0, sizeof(*player));
    player->name = (name != NULL) ? name : "unknown";
    player->screen = parent;

    if ((parent == NULL) || (gif_path == NULL) || (gif_path[0] == '\0')) {
        return false;
    }
    if (baji_photo_store_mount() != 0) {
        return false;
    }
    BAJI_GIF_TRACE("create start name=%s path=%s", player->name, gif_path);
    if (!baji_gif_file_load(player, gif_path)) {
        BAJI_GIF_TRACE("file load failed name=%s path=%s", player->name, gif_path);
        return false;
    }
    if (!baji_gif_decoder_open(player)) {
        BAJI_GIF_TRACE("decoder open failed name=%s path=%s", player->name, gif_path);
        return false;
    }
    BAJI_GIF_TRACE("decoder info name=%s size=%ux%u bytes=%u",
                   player->name,
                   (unsigned int)player->info.uWidth,
                   (unsigned int)player->info.uHeight,
                   player->src_size);

    if (!baji_gif_dims_valid(player->info.uWidth, player->info.uHeight, &frame_bytes)) {
        BAJI_GIF_TRACE("decoder info invalid name=%s size=%ux%u",
                       player->name,
                       (unsigned int)player->info.uWidth,
                       (unsigned int)player->info.uHeight);
        return false;
    }
    player->rgb565 = (uint16_t *)baji_gif_alloc(frame_bytes);
    if (player->rgb565 == NULL) {
        return false;
    }
    memset(player->rgb565, 0, frame_bytes);

    if (!baji_gif_canvas_set(player)) {
        return false;
    }

    player->frame_dsc.header.always_zero = 0;
    player->frame_dsc.header.w = player->info.uWidth;
    player->frame_dsc.header.h = player->info.uHeight;
    player->frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    player->frame_dsc.data_size = frame_bytes;
    player->frame_dsc.data = (const uint8_t *)player->rgb565;

    if (!baji_gif_decode_next(player, &duration, &eos)) {
        return false;
    }

    player->img = lv_img_create(parent);
    if (player->img == NULL) {
        return false;
    }
    lv_img_set_src(player->img, &player->frame_dsc);
    lv_obj_center(player->img);

    player->timer = lv_timer_create(baji_gif_timer_cb, baji_gif_period_get(duration), player);
    if (player->timer == NULL) {
        player->cleanup_direct_only = true;
        lv_obj_del(player->img);
        player->img = NULL;
        return false;
    }
    player->timer_paused = false;
    lv_obj_add_event_cb(player->img, baji_gif_event_cb, LV_EVENT_DELETE, player);
    lv_obj_add_event_cb(parent, baji_gif_screen_event_cb, LV_EVENT_SCREEN_UNLOAD_START, player);
    lv_obj_add_event_cb(parent, baji_gif_screen_event_cb, LV_EVENT_SCREEN_UNLOADED, player);
    BAJI_GIF_TRACE("create ok name=%s period=%u", player->name, baji_gif_period_get(duration));
    if (eos != 0U) {
        player->restart_pending = true;
    }
    return true;
}

lv_obj_t *baji_gif_player_create(lv_obj_t *parent,
                                 const char *gif_path,
                                 const char *name)
{
#if BAJI_PHOTO_ENABLE_GIF_SUPPORT && BAJI_PHOTO_GIF_PLAYER_GIFD
    baji_gif_player_t *player;

    player = (baji_gif_player_t *)liot_rtos_malloc(sizeof(*player));
    if (player == NULL) {
        return NULL;
    }

    if (!baji_gif_player_init(player, parent, gif_path, name)) {
        baji_gif_cleanup(player);
        return NULL;
    }

    return player->img;
#else
    (void)parent;
    (void)gif_path;
    (void)name;
    return NULL;
#endif
}
