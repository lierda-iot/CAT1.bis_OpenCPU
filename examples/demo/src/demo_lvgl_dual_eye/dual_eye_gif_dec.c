/**
 * @file dual_eye_gif_dec.c
 * @brief Platform GIF decoder adapter for the dual eye LVGL demo.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "dual_eye_gif_dec.h"
#include "liot_log.h"
#include "liot_os.h"
#include "mm_video_if.h"

#define DUAL_EYE_GIF_MIN_PERIOD_MS 10U
#define DUAL_EYE_GIF_DEFAULT_PERIOD_MS 40U

typedef struct
{
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
} dual_eye_gif_player_t;

static unsigned int dual_eye_gif_period_get(unsigned int duration)
{
    if (duration == 0U) {
        return DUAL_EYE_GIF_DEFAULT_PERIOD_MS;
    }

    if (duration < DUAL_EYE_GIF_MIN_PERIOD_MS) {
        return DUAL_EYE_GIF_MIN_PERIOD_MS;
    }

    return duration;
}

static bool dual_eye_gif_decode_next(dual_eye_gif_player_t *player,
                                     unsigned int *duration,
                                     unsigned int *eos)
{
    int ret;

    ret = GifD_DecodeImage(player->decoder, NULL, duration, eos);
    if (ret == 0) {
        return true;
    }

    liot_trace("dual eye gif %s decode frame failed:%d",
               player->name, ret);
    return false;
}

static void dual_eye_gif_decoder_close(dual_eye_gif_player_t *player)
{
    if (player->decoder != NULL) {
        GifD_Destroy(player->decoder);
        player->decoder = NULL;
    }
}

static bool dual_eye_gif_canvas_set(dual_eye_gif_player_t *player)
{
    player->canvas.eFmt = VIDEO_COLOR_FMT_RGB565;
    player->canvas.uWidth = (unsigned short)player->info.uWidth;
    player->canvas.uHeight = (unsigned short)player->info.uHeight;
    player->canvas.pData[0] = player->rgb565;
    player->canvas.pData[1] = NULL;
    player->canvas.pData[2] = NULL;

    if (GifD_SetCanvas(player->decoder, &player->canvas) != 0) {
        liot_trace("dual eye gif %s set canvas failed", player->name);
        return false;
    }

    return true;
}

static bool dual_eye_gif_decoder_open(dual_eye_gif_player_t *player)
{
    GIF_INFO info;

    player->decoder = GifD_Create();
    if (player->decoder == NULL) {
        liot_trace("dual eye gif %s create decoder failed", player->name);
        return false;
    }

    if (GifD_DecodeInfo(player->decoder,
                        (unsigned char *)player->src_data,
                        player->src_size,
                        &info) != 0) {
        liot_trace("dual eye gif %s decode info failed", player->name);
        dual_eye_gif_decoder_close(player);
        return false;
    }

    if (info.uWidth == 0U || info.uHeight == 0U) {
        liot_trace("dual eye gif %s invalid size:%lux%lu",
                   player->name,
                   (unsigned long)info.uWidth,
                   (unsigned long)info.uHeight);
        dual_eye_gif_decoder_close(player);
        return false;
    }

    if (player->rgb565 != NULL &&
        (info.uWidth != player->info.uWidth ||
         info.uHeight != player->info.uHeight)) {
        liot_trace("dual eye gif %s restart size changed:%lux%lu",
                   player->name,
                   (unsigned long)info.uWidth,
                   (unsigned long)info.uHeight);
        dual_eye_gif_decoder_close(player);
        return false;
    }

    player->info = info;
    return true;
}

static bool dual_eye_gif_restart(dual_eye_gif_player_t *player,
                                 unsigned int *duration,
                                 unsigned int *eos)
{
    dual_eye_gif_decoder_close(player);
    memset(player->rgb565, 0, player->frame_dsc.data_size);

    if (!dual_eye_gif_decoder_open(player)) {
        return false;
    }

    if (!dual_eye_gif_canvas_set(player)) {
        return false;
    }

    player->restart_pending = false;
    return dual_eye_gif_decode_next(player, duration, eos);
}

static void dual_eye_gif_timer_cb(lv_timer_t *timer)
{
    dual_eye_gif_player_t *player = (dual_eye_gif_player_t *)timer->user_data;
    unsigned int duration = 0;
    unsigned int eos = 0;

    if (player == NULL || player->decoder == NULL || player->img == NULL) {
        return;
    }

    if (player->restart_pending) {
        if (!dual_eye_gif_restart(player, &duration, &eos)) {
            lv_timer_pause(timer);
            return;
        }
    } else if (!dual_eye_gif_decode_next(player, &duration, &eos)) {
        lv_timer_pause(timer);
        return;
    }

    lv_obj_invalidate(player->img);
    lv_timer_set_period(timer, dual_eye_gif_period_get(duration));

    if (eos != 0U) {
        player->restart_pending = true;
    }
}

static bool dual_eye_gif_player_init(dual_eye_gif_player_t *player,
                                     lv_obj_t *parent,
                                     const lv_img_dsc_t *gif_src,
                                     const char *name)
{
    unsigned int duration = 0;
    unsigned int eos = 0;
    uint32_t pixel_count;
    uint32_t frame_bytes;

    memset(player, 0, sizeof(*player));
    player->name = (name != NULL) ? name : "unknown";

    if (parent == NULL || gif_src == NULL || gif_src->data == NULL ||
        gif_src->data_size == 0U) {
        liot_trace("dual eye gif %s invalid source", player->name);
        return false;
    }

    player->src_data = gif_src->data;
    player->src_size = (uint32_t)gif_src->data_size;

    if (!dual_eye_gif_decoder_open(player)) {
        return false;
    }

    pixel_count = player->info.uWidth * player->info.uHeight;
    frame_bytes = pixel_count * (uint32_t)sizeof(uint16_t);
    player->rgb565 = (uint16_t *)liot_rtos_malloc(frame_bytes);
    if (player->rgb565 == NULL) {
        liot_trace("dual eye gif %s frame alloc failed:%lu",
                   player->name, (unsigned long)frame_bytes);
        return false;
    }

    memset(player->rgb565, 0, frame_bytes);

    if (!dual_eye_gif_canvas_set(player)) {
        return false;
    }

    player->frame_dsc.header.always_zero = 0;
    player->frame_dsc.header.w = player->info.uWidth;
    player->frame_dsc.header.h = player->info.uHeight;
    player->frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    player->frame_dsc.data_size = frame_bytes;
    player->frame_dsc.data = (const uint8_t *)player->rgb565;

    if (!dual_eye_gif_decode_next(player, &duration, &eos)) {
        return false;
    }

    player->img = lv_img_create(parent);
    if (player->img == NULL) {
        liot_trace("dual eye gif %s create image failed", player->name);
        return false;
    }

    lv_img_set_src(player->img, &player->frame_dsc);
    lv_obj_center(player->img);

    player->timer = lv_timer_create(dual_eye_gif_timer_cb,
                                    dual_eye_gif_period_get(duration),
                                    player);
    if (player->timer == NULL) {
        liot_trace("dual eye gif %s create timer failed", player->name);
        return false;
    }

    if (eos != 0U) {
        player->restart_pending = true;
    }

    liot_trace("dual eye gif %s ready:%lux%lu bytes:%lu",
               player->name,
               (unsigned long)player->info.uWidth,
               (unsigned long)player->info.uHeight,
               (unsigned long)gif_src->data_size);
    return true;
}

lv_obj_t *dual_eye_gif_dec_create(lv_obj_t *parent,
                                  const lv_img_dsc_t *gif_src,
                                  const char *name)
{
    dual_eye_gif_player_t *player;

    player = (dual_eye_gif_player_t *)liot_rtos_malloc(sizeof(*player));
    if (player == NULL) {
        liot_trace("dual eye gif %s player alloc failed",
                   (name != NULL) ? name : "unknown");
        return NULL;
    }

    if (!dual_eye_gif_player_init(player, parent, gif_src, name)) {
        if (player->timer != NULL) {
            lv_timer_del(player->timer);
        }
        dual_eye_gif_decoder_close(player);
        if (player->rgb565 != NULL) {
            liot_rtos_free(player->rgb565);
        }
        liot_rtos_free(player);
        return NULL;
    }

    return player->img;
}
