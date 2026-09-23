#include "app_display_port_lvgl_gif_player.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD && APP_DISPLAY_LVGL_ENABLE_GIF

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_display_port_lvgl_gif_mm.h"
#include "app_osal.h"

#define DISPLAY_LVGL_GIF_MIN_PERIOD_MS 10U
#define DISPLAY_LVGL_GIF_DEFAULT_PERIOD_MS 40U

struct display_lvgl_gif_player {
    void *decoder;
    lv_obj_t *img;
    lv_timer_t *timer;
    lv_img_dsc_t frame_dsc;
    VIDEO_IMAGE_BUF canvas;
    GIF_INFO info;
    uint16_t *rgb565;
    const uint8_t *src_data;
    uint32_t src_size;
    const char *name;
    bool restart_pending;
};

static uint32_t display_lvgl_gif_period_get(uint32_t duration)
{
    if (duration == 0U) {
        return DISPLAY_LVGL_GIF_DEFAULT_PERIOD_MS;
    }
    if (duration < DISPLAY_LVGL_GIF_MIN_PERIOD_MS) {
        return DISPLAY_LVGL_GIF_MIN_PERIOD_MS;
    }
    return duration;
}

static void display_lvgl_gif_decoder_close(display_lvgl_gif_player_t *player)
{
    if (player->decoder != NULL) {
        GifD_Destroy(player->decoder);
        player->decoder = NULL;
    }
}

static bool display_lvgl_gif_decoder_open(display_lvgl_gif_player_t *player)
{
    GIF_INFO info;
    int ret;

    player->decoder = GifD_Create();
    if (player->decoder == NULL) {
        app_log("display gif create decoder failed: %s", player->name);
        return false;
    }

    ret = GifD_DecodeInfo(player->decoder,
                          (unsigned char *)player->src_data,
                          player->src_size,
                          &info);
    if (ret != 0) {
        app_log("display gif decode info failed: %s ret=%d", player->name, ret);
        display_lvgl_gif_decoder_close(player);
        return false;
    }
    if (info.uWidth == 0U || info.uHeight == 0U) {
        app_log("display gif invalid size: %s %lux%lu",
                player->name,
                (unsigned long)info.uWidth,
                (unsigned long)info.uHeight);
        display_lvgl_gif_decoder_close(player);
        return false;
    }
    if (player->rgb565 != NULL &&
        (info.uWidth != player->info.uWidth ||
         info.uHeight != player->info.uHeight)) {
        app_log("display gif restart size changed: %s %lux%lu",
                player->name,
                (unsigned long)info.uWidth,
                (unsigned long)info.uHeight);
        display_lvgl_gif_decoder_close(player);
        return false;
    }

    player->info = info;
    return true;
}

static bool display_lvgl_gif_canvas_set(display_lvgl_gif_player_t *player)
{
    int ret;

    player->canvas.eFmt = VIDEO_COLOR_FMT_RGB565;
    player->canvas.uWidth = (unsigned short)player->info.uWidth;
    player->canvas.uHeight = (unsigned short)player->info.uHeight;
    player->canvas.pData[0] = player->rgb565;
    player->canvas.pData[1] = NULL;
    player->canvas.pData[2] = NULL;

    ret = GifD_SetCanvas(player->decoder, &player->canvas);
    if (ret != 0) {
        app_log("display gif set canvas failed: %s ret=%d", player->name, ret);
        return false;
    }
    return true;
}

static bool display_lvgl_gif_decode_next(display_lvgl_gif_player_t *player,
                                         uint32_t *duration,
                                         uint32_t *eos)
{
    unsigned int frame_duration = 0U;
    unsigned int frame_eos = 0U;
    int ret;

    ret = GifD_DecodeImage(player->decoder, NULL, &frame_duration, &frame_eos);
    if (ret != 0) {
        app_log("display gif decode frame failed: %s ret=%d", player->name, ret);
        return false;
    }

    *duration = frame_duration;
    *eos = frame_eos;
    return true;
}

static bool display_lvgl_gif_restart(display_lvgl_gif_player_t *player,
                                     uint32_t *duration,
                                     uint32_t *eos)
{
    display_lvgl_gif_decoder_close(player);
    memset(player->rgb565, 0, player->frame_dsc.data_size);

    if (!display_lvgl_gif_decoder_open(player)) {
        return false;
    }
    if (!display_lvgl_gif_canvas_set(player)) {
        return false;
    }

    player->restart_pending = false;
    return display_lvgl_gif_decode_next(player, duration, eos);
}

static void display_lvgl_gif_timer_cb(lv_timer_t *timer)
{
    display_lvgl_gif_player_t *player = (display_lvgl_gif_player_t *)timer->user_data;
    uint32_t duration = 0U;
    uint32_t eos = 0U;

    if (player == NULL || player->decoder == NULL || player->img == NULL) {
        return;
    }

    if (player->restart_pending) {
        if (!display_lvgl_gif_restart(player, &duration, &eos)) {
            lv_timer_pause(timer);
            return;
        }
    } else if (!display_lvgl_gif_decode_next(player, &duration, &eos)) {
        lv_timer_pause(timer);
        return;
    }

    lv_obj_invalidate(player->img);
    lv_timer_set_period(timer, display_lvgl_gif_period_get(duration));
    if (eos != 0U) {
        player->restart_pending = true;
    }
}

static bool display_lvgl_gif_player_init(display_lvgl_gif_player_t *player,
                                         lv_obj_t *parent,
                                         const lv_img_dsc_t *gif_src,
                                         const char *name)
{
    uint32_t duration = 0U;
    uint32_t eos = 0U;
    uint32_t pixel_count;
    uint32_t frame_bytes;

    memset(player, 0, sizeof(*player));
    player->name = (name != NULL) ? name : "unknown";
    if (parent == NULL || gif_src == NULL || gif_src->data == NULL ||
        gif_src->data_size == 0U) {
        app_log("display gif invalid source: %s", player->name);
        return false;
    }

    player->src_data = gif_src->data;
    player->src_size = (uint32_t)gif_src->data_size;
    if (!display_lvgl_gif_decoder_open(player)) {
        return false;
    }

    pixel_count = player->info.uWidth * player->info.uHeight;
    frame_bytes = pixel_count * (uint32_t)sizeof(uint16_t);
    player->rgb565 = (uint16_t *)app_os_malloc(frame_bytes);
    if (player->rgb565 == NULL) {
        app_log("display gif frame alloc failed: %s bytes=%lu",
                player->name,
                (unsigned long)frame_bytes);
        return false;
    }
    memset(player->rgb565, 0, frame_bytes);

    if (!display_lvgl_gif_canvas_set(player)) {
        return false;
    }

    player->frame_dsc.header.always_zero = 0;
    player->frame_dsc.header.w = player->info.uWidth;
    player->frame_dsc.header.h = player->info.uHeight;
    player->frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    player->frame_dsc.data_size = frame_bytes;
    player->frame_dsc.data = (const uint8_t *)player->rgb565;

    if (!display_lvgl_gif_decode_next(player, &duration, &eos)) {
        return false;
    }

    player->img = lv_img_create(parent);
    if (player->img == NULL) {
        app_log("display gif image create failed: %s", player->name);
        return false;
    }
    lv_img_set_src(player->img, &player->frame_dsc);
    lv_obj_center(player->img);

    player->timer = lv_timer_create(display_lvgl_gif_timer_cb,
                                    display_lvgl_gif_period_get(duration),
                                    player);
    if (player->timer == NULL) {
        app_log("display gif timer create failed: %s", player->name);
        return false;
    }
    if (eos != 0U) {
        player->restart_pending = true;
    }

    app_log("display gif ready: %s %lux%lu frame=%lu src=%lu",
            player->name,
            (unsigned long)player->info.uWidth,
            (unsigned long)player->info.uHeight,
            (unsigned long)frame_bytes,
            (unsigned long)gif_src->data_size);
    return true;
}

display_lvgl_gif_player_t *display_lvgl_gif_player_create(lv_obj_t *parent,
                                                          const lv_img_dsc_t *gif_src,
                                                          const char *name)
{
    display_lvgl_gif_player_t *player;

    player = (display_lvgl_gif_player_t *)app_os_malloc(sizeof(*player));
    if (player == NULL) {
        app_log("display gif player alloc failed: %s",
                (name != NULL) ? name : "unknown");
        return NULL;
    }

    if (!display_lvgl_gif_player_init(player, parent, gif_src, name)) {
        display_lvgl_gif_player_destroy(player);
        return NULL;
    }
    return player;
}

void display_lvgl_gif_player_destroy(display_lvgl_gif_player_t *player)
{
    if (player == NULL) {
        return;
    }
    if (player->timer != NULL) {
        lv_timer_del(player->timer);
        player->timer = NULL;
    }
    if (player->img != NULL) {
        lv_obj_del(player->img);
        player->img = NULL;
    }
    display_lvgl_gif_decoder_close(player);
    if (player->rgb565 != NULL) {
        app_os_free(player->rgb565);
        player->rgb565 = NULL;
    }
    app_log("display gif destroy: %s", player->name);
    app_os_free(player);
}

#endif
