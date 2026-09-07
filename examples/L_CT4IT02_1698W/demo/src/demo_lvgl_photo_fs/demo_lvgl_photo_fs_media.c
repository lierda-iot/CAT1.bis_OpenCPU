#include "demo_lvgl_photo_fs_media.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "liot_external_flash_fs.h"
#include "liot_log.h"
#include "liot_os.h"
#include "mm_jpeg_if.h"
#include "mm_video_if.h"

#include "demo_lvgl_photo_fs_store.h"

#define DEMO_LVGL_PHOTO_FS_MEDIA_LOG_PREFIX "[demo_lvgl_photo_fs_media]"

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
} demo_lvgl_photo_fs_gif_player_t;

typedef struct {
    lv_img_dsc_t dsc;
    void *buf;
} demo_lvgl_photo_fs_static_img_t;

static void demo_lvgl_photo_fs_gif_screen_event_cb(lv_event_t *e);

static void demo_lvgl_photo_fs_media_buf_free(void *buf)
{
    if (buf != NULL) {
        liot_rtos_free(buf);
    }
}

static void demo_lvgl_photo_fs_media_img_delete_cb(lv_event_t *e)
{
    demo_lvgl_photo_fs_static_img_t *img_ctx;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    img_ctx = (demo_lvgl_photo_fs_static_img_t *)lv_event_get_user_data(e);
    if (img_ctx != NULL) {
        demo_lvgl_photo_fs_media_buf_free(img_ctx->buf);
        liot_rtos_free(img_ctx);
    }
}

static int demo_lvgl_photo_fs_media_resolve_item_path(const demo_lvgl_photo_fs_item_t *item,
                                                      char *out_path,
                                                      unsigned int out_len)
{
    int written;

    if ((item == NULL) || (out_path == NULL) || (out_len == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (item->local_path[0] != '\0') {
        written = snprintf(out_path, out_len, "%s", item->local_path);
        if ((written < 0) || ((unsigned int)written >= out_len)) {
            return LIOT_EXTFLASH_INVALID_PARAMETER;
        }
        return 0;
    }

    return demo_lvgl_photo_fs_store_build_path(item->id,
                                               item->format,
                                               out_path,
                                               out_len);
}

static int demo_lvgl_photo_fs_media_rgb565_size(uint16_t width,
                                                uint16_t height,
                                                uint32_t *out_size)
{
    uint32_t pixels;

    if ((out_size == NULL) || (width == 0u) || (height == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if ((uint32_t)width > (UINT32_MAX / (uint32_t)height)) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    pixels = (uint32_t)width * (uint32_t)height;
    if (pixels > (UINT32_MAX / DEMO_LVGL_PHOTO_FS_IMG_BPP)) {
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    *out_size = pixels * DEMO_LVGL_PHOTO_FS_IMG_BPP;
    return 0;
}

static int demo_lvgl_photo_fs_media_load_raw_item(const demo_lvgl_photo_fs_item_t *item,
                                                  uint8_t **out_src_buf,
                                                  unsigned int *out_src_size)
{
    char path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    liot_stat_ext_s st;
    LFILE_EXT fd;
    uint8_t *src_buf;
    int read_len;
    int ret;

    if ((item == NULL) || (out_src_buf == NULL) || (out_src_size == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    *out_src_buf = NULL;
    *out_src_size = 0u;

    ret = demo_lvgl_photo_fs_media_resolve_item_path(item, path, sizeof(path));
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    (void)memset(&st, 0, sizeof(st));
    ret = liot_stat_ext(path, &st);
    if ((ret != LIOT_EXTFLASH_OK) ||
        (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (st.size == 0u) ||
        (st.size != item->file_size)) {
        demo_lvgl_photo_fs_store_access_end();
        return (ret == LIOT_EXTFLASH_OK) ? LIOT_EXTFLASH_SIZE_FAIL : ret;
    }

    src_buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
    if (src_buf == NULL) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        liot_rtos_free(src_buf);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    read_len = liot_fread_ext(src_buf, (unsigned int)st.size, 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();
    if (read_len != (int)st.size) {
        liot_rtos_free(src_buf);
        return LIOT_EXTFLASH_READ_FAIL;
    }

    *out_src_buf = src_buf;
    *out_src_size = (unsigned int)st.size;
    return 0;
}

static int demo_lvgl_photo_fs_media_load_bjp(const demo_lvgl_photo_fs_item_t *item,
                                             lv_img_dsc_t *out_dsc,
                                             void **out_buf)
{
    char path[DEMO_LVGL_PHOTO_FS_PATH_MAX_LEN + 1u];
    demo_lvgl_photo_fs_file_header_t header;
    LFILE_EXT fd;
    uint8_t *pixels;
    int ret;

    if ((item == NULL) || (out_dsc == NULL) || (out_buf == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    *out_buf = NULL;

    ret = demo_lvgl_photo_fs_media_resolve_item_path(item, path, sizeof(path));
    if (ret != 0) {
        return ret;
    }

    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    memset(&header, 0, sizeof(header));
    ret = liot_fread_ext(&header, sizeof(header), 1, fd);
    if (ret != (int)sizeof(header)) {
        (void)liot_fclose_ext(fd);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_READ_FAIL;
    }

    if ((header.magic != DEMO_LVGL_PHOTO_FS_MAGIC) ||
        (header.width == 0u) ||
        (header.height == 0u) ||
        (header.width > DEMO_LVGL_PHOTO_FS_IMG_W) ||
        (header.height > DEMO_LVGL_PHOTO_FS_IMG_H) ||
        (header.data_size != ((uint32_t)header.width * (uint32_t)header.height *
                              DEMO_LVGL_PHOTO_FS_IMG_BPP)) ||
        (header.crc32 != item->crc32)) {
        (void)liot_fclose_ext(fd);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    pixels = (uint8_t *)liot_rtos_malloc(header.data_size);
    if (pixels == NULL) {
        (void)liot_fclose_ext(fd);
        demo_lvgl_photo_fs_store_access_end();
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    ret = liot_fread_ext(pixels, header.data_size, 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();
    if (ret != (int)header.data_size) {
        liot_rtos_free(pixels);
        return LIOT_EXTFLASH_READ_FAIL;
    }

    memset(out_dsc, 0, sizeof(*out_dsc));
    out_dsc->header.always_zero = 0;
    out_dsc->header.w = header.width;
    out_dsc->header.h = header.height;
    out_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    out_dsc->data_size = header.data_size;
    out_dsc->data = pixels;
    *out_buf = pixels;
    return 0;
}

#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
static int demo_lvgl_photo_fs_media_load_jpeg(const demo_lvgl_photo_fs_item_t *item,
                                              lv_img_dsc_t *out_dsc,
                                              void **out_buf)
{
    JPEG_INFO info;
    JPEG_IMAGE_BUF out;
    uint8_t *src_buf = NULL;
    uint8_t *pixels = NULL;
    uint32_t pixel_bytes = 0u;
    void *decoder = NULL;
    unsigned int src_size = 0u;
    int ret;

    if ((item == NULL) || (out_dsc == NULL) || (out_buf == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if ((item->cf != LV_IMG_CF_RAW) || (item->width == 0u) || (item->height == 0u)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    *out_buf = NULL;

    ret = demo_lvgl_photo_fs_media_load_raw_item(item, &src_buf, &src_size);
    if (ret != 0) {
        return ret;
    }
    if ((src_size < 4u) || (src_buf[0] != 0xFFu) || (src_buf[1] != 0xD8u)) {
        ret = LIOT_EXTFLASH_READ_FAIL;
        goto cleanup;
    }

    ret = demo_lvgl_photo_fs_media_rgb565_size(item->width, item->height, &pixel_bytes);
    if (ret != 0) {
        goto cleanup;
    }

    decoder = JpegD_Create();
    if (decoder == NULL) {
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }

    (void)memset(&info, 0, sizeof(info));
    ret = JpegD_DecodeInfo(decoder, (unsigned char *)src_buf, src_size, &info);
    if ((ret != 0) || (info.uWidth != item->width) || (info.uHeight != item->height)) {
        ret = LIOT_EXTFLASH_SIZE_FAIL;
        goto cleanup;
    }

    pixels = (uint8_t *)liot_rtos_malloc(pixel_bytes);
    if (pixels == NULL) {
        ret = LIOT_EXTFLASH_ERROR_GENERAL;
        goto cleanup;
    }
    (void)memset(pixels, 0, pixel_bytes);

    (void)memset(&out, 0, sizeof(out));
    out.eFmt = JPEG_COLOR_FMT_RGB565;
    out.uWidth = info.uWidth;
    out.uHeight = info.uHeight;
    out.pData[0] = pixels;

    ret = JpegD_DecodeImage(decoder, &out);
    if (ret != 0) {
        ret = LIOT_EXTFLASH_READ_FAIL;
        goto cleanup;
    }

    (void)memset(out_dsc, 0, sizeof(*out_dsc));
    out_dsc->header.always_zero = 0;
    out_dsc->header.w = item->width;
    out_dsc->header.h = item->height;
    out_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    out_dsc->data_size = pixel_bytes;
    out_dsc->data = pixels;
    *out_buf = pixels;
    ret = 0;
    pixels = NULL;

cleanup:
    if (decoder != NULL) {
        JpegD_Destroy(decoder);
    }
    if (src_buf != NULL) {
        liot_rtos_free(src_buf);
    }
    if (pixels != NULL) {
        liot_rtos_free(pixels);
    }
    return ret;
}
#endif

static void demo_lvgl_photo_fs_gif_free(void *buf)
{
    if (buf != NULL) {
        liot_rtos_free(buf);
    }
}

static unsigned int demo_lvgl_photo_fs_gif_period_get(unsigned int duration)
{
    if (duration == 0u) {
        return 40u;
    }
    if (duration < 10u) {
        return 10u;
    }
    return duration;
}

static bool demo_lvgl_photo_fs_gif_decode_next(demo_lvgl_photo_fs_gif_player_t *player,
                                               unsigned int *duration,
                                               unsigned int *eos)
{
    return (GifD_DecodeImage(player->decoder, NULL, duration, eos) == 0);
}

static void demo_lvgl_photo_fs_gif_timer_pause(demo_lvgl_photo_fs_gif_player_t *player)
{
    if ((player == NULL) || (player->timer == NULL) || player->timer_paused) {
        return;
    }

    lv_timer_pause(player->timer);
    player->timer_paused = true;
}

static void demo_lvgl_photo_fs_gif_decoder_close(demo_lvgl_photo_fs_gif_player_t *player)
{
    if (player->decoder != NULL) {
        GifD_Destroy(player->decoder);
        player->decoder = NULL;
    }
}

static bool demo_lvgl_photo_fs_gif_canvas_set(demo_lvgl_photo_fs_gif_player_t *player)
{
    player->canvas.eFmt = VIDEO_COLOR_FMT_RGB565;
    player->canvas.uWidth = (unsigned short)player->info.uWidth;
    player->canvas.uHeight = (unsigned short)player->info.uHeight;
    player->canvas.pData[0] = player->rgb565;
    player->canvas.pData[1] = NULL;
    player->canvas.pData[2] = NULL;

    return (GifD_SetCanvas(player->decoder, &player->canvas) == 0);
}

static bool demo_lvgl_photo_fs_gif_decoder_open(demo_lvgl_photo_fs_gif_player_t *player)
{
    GIF_INFO info;

    player->decoder = GifD_Create();
    if (player->decoder == NULL) {
        return false;
    }

    if (GifD_DecodeInfo(player->decoder, player->src_buf, player->src_size, &info) != 0) {
        demo_lvgl_photo_fs_gif_decoder_close(player);
        return false;
    }
    if ((info.uWidth == 0u) || (info.uHeight == 0u)) {
        demo_lvgl_photo_fs_gif_decoder_close(player);
        return false;
    }

    if ((player->info.uWidth != 0u) &&
        ((info.uWidth != player->info.uWidth) || (info.uHeight != player->info.uHeight))) {
        demo_lvgl_photo_fs_gif_decoder_close(player);
        return false;
    }

    player->info = info;
    return true;
}

static bool demo_lvgl_photo_fs_gif_restart(demo_lvgl_photo_fs_gif_player_t *player,
                                           unsigned int *duration,
                                           unsigned int *eos)
{
    demo_lvgl_photo_fs_gif_decoder_close(player);
    memset(player->rgb565, 0, player->frame_dsc.data_size);

    if (!demo_lvgl_photo_fs_gif_decoder_open(player)) {
        return false;
    }
    if (!demo_lvgl_photo_fs_gif_canvas_set(player)) {
        return false;
    }

    player->restart_pending = false;
    return demo_lvgl_photo_fs_gif_decode_next(player, duration, eos);
}

static void demo_lvgl_photo_fs_gif_timer_cb(lv_timer_t *timer)
{
    demo_lvgl_photo_fs_gif_player_t *player;
    unsigned int duration = 0u;
    unsigned int eos = 0u;

    player = (demo_lvgl_photo_fs_gif_player_t *)timer->user_data;
    if ((player == NULL) || (player->decoder == NULL) || (player->img == NULL)) {
        return;
    }

    if (player->restart_pending) {
        if (!demo_lvgl_photo_fs_gif_restart(player, &duration, &eos)) {
            demo_lvgl_photo_fs_gif_timer_pause(player);
            return;
        }
    } else if (!demo_lvgl_photo_fs_gif_decode_next(player, &duration, &eos)) {
        demo_lvgl_photo_fs_gif_timer_pause(player);
        return;
    }

    lv_obj_invalidate(player->img);
    lv_timer_set_period(timer, demo_lvgl_photo_fs_gif_period_get(duration));
    if (eos != 0u) {
        player->restart_pending = true;
    }
}

static void demo_lvgl_photo_fs_gif_cleanup(demo_lvgl_photo_fs_gif_player_t *player)
{
    if (player == NULL) {
        return;
    }

    if (player->timer != NULL) {
        lv_timer_del(player->timer);
        player->timer = NULL;
    }
    if ((player->screen != NULL) && lv_obj_is_valid(player->screen)) {
        (void)lv_obj_remove_event_cb_with_user_data(player->screen,
                                                    demo_lvgl_photo_fs_gif_screen_event_cb,
                                                    player);
        player->screen = NULL;
    }
    demo_lvgl_photo_fs_gif_decoder_close(player);
    demo_lvgl_photo_fs_gif_free(player->rgb565);
    demo_lvgl_photo_fs_gif_free(player->src_buf);
    liot_rtos_free(player);
}

static void demo_lvgl_photo_fs_gif_event_cb(lv_event_t *e)
{
    demo_lvgl_photo_fs_gif_player_t *player;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    player = (demo_lvgl_photo_fs_gif_player_t *)lv_event_get_user_data(e);
    if (player != NULL) {
        player->img = NULL;
        demo_lvgl_photo_fs_gif_cleanup(player);
    }
}

static void demo_lvgl_photo_fs_gif_screen_event_cb(lv_event_t *e)
{
    demo_lvgl_photo_fs_gif_player_t *player;

    if (lv_event_get_code(e) != LV_EVENT_SCREEN_UNLOADED) {
        return;
    }

    player = (demo_lvgl_photo_fs_gif_player_t *)lv_event_get_user_data(e);
    if (player == NULL) {
        return;
    }

    demo_lvgl_photo_fs_gif_timer_pause(player);
}

static bool demo_lvgl_photo_fs_gif_file_load(demo_lvgl_photo_fs_gif_player_t *player,
                                             const char *gif_path)
{
    liot_stat_ext_s st;
    LFILE_EXT fd;
    int read_len;
    int ret;

    memset(&st, 0, sizeof(st));
    ret = demo_lvgl_photo_fs_store_access_begin();
    if (ret != 0) {
        return false;
    }

    ret = liot_stat_ext(gif_path, &st);
    if ((ret != LIOT_EXTFLASH_OK) || (st.type != LIOT_EXTFLASH_TYPE_FILE) || (st.size == 0u)) {
        demo_lvgl_photo_fs_store_access_end();
        return false;
    }

    player->src_buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
    if (player->src_buf == NULL) {
        demo_lvgl_photo_fs_store_access_end();
        return false;
    }
    player->src_size = (unsigned int)st.size;

    fd = liot_fopen_ext(gif_path, "r");
    if (fd <= 0) {
        demo_lvgl_photo_fs_store_access_end();
        return false;
    }

    read_len = liot_fread_ext(player->src_buf, player->src_size, 1, fd);
    (void)liot_fclose_ext(fd);
    demo_lvgl_photo_fs_store_access_end();
    if (read_len != (int)player->src_size) {
        return false;
    }

    return true;
}

static lv_obj_t *demo_lvgl_photo_fs_gif_player_create(lv_obj_t *parent,
                                                      const char *gif_path,
                                                      const char *name)
{
#if DEMO_LVGL_PHOTO_FS_ENABLE_GIF
    demo_lvgl_photo_fs_gif_player_t *player;
    unsigned int duration = 0u;
    unsigned int eos = 0u;
    uint32_t frame_bytes;

    if ((parent == NULL) || (gif_path == NULL) || (gif_path[0] == '\0')) {
        return NULL;
    }

    player = (demo_lvgl_photo_fs_gif_player_t *)liot_rtos_malloc(sizeof(*player));
    if (player == NULL) {
        return NULL;
    }
    memset(player, 0, sizeof(*player));
    player->name = (name != NULL) ? name : "gif";
    player->screen = parent;

    if (demo_lvgl_photo_fs_store_mount() != 0) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }

    if (!demo_lvgl_photo_fs_gif_file_load(player, gif_path)) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }
    if (!demo_lvgl_photo_fs_gif_decoder_open(player)) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }

    frame_bytes = player->info.uWidth * player->info.uHeight * (uint32_t)sizeof(uint16_t);
    player->rgb565 = (uint16_t *)liot_rtos_malloc(frame_bytes);
    if (player->rgb565 == NULL) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }
    memset(player->rgb565, 0, frame_bytes);

    if (!demo_lvgl_photo_fs_gif_canvas_set(player)) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }

    player->frame_dsc.header.always_zero = 0;
    player->frame_dsc.header.w = player->info.uWidth;
    player->frame_dsc.header.h = player->info.uHeight;
    player->frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    player->frame_dsc.data_size = frame_bytes;
    player->frame_dsc.data = (const uint8_t *)player->rgb565;

    if (!demo_lvgl_photo_fs_gif_decode_next(player, &duration, &eos)) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }

    player->img = lv_img_create(parent);
    if (player->img == NULL) {
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }
    lv_img_set_src(player->img, &player->frame_dsc);
    lv_obj_center(player->img);

    player->timer = lv_timer_create(demo_lvgl_photo_fs_gif_timer_cb,
                                    demo_lvgl_photo_fs_gif_period_get(duration),
                                    player);
    if (player->timer == NULL) {
        lv_obj_del(player->img);
        player->img = NULL;
        demo_lvgl_photo_fs_gif_cleanup(player);
        return NULL;
    }
    player->timer_paused = false;
    lv_obj_add_event_cb(player->img, demo_lvgl_photo_fs_gif_event_cb, LV_EVENT_DELETE, player);
    lv_obj_add_event_cb(parent, demo_lvgl_photo_fs_gif_screen_event_cb, LV_EVENT_SCREEN_UNLOADED, player);
    if (eos != 0u) {
        player->restart_pending = true;
    }
    return player->img;
#else
    (void)parent;
    (void)gif_path;
    (void)name;
    return NULL;
#endif
}

int demo_lvgl_photo_fs_media_attach(lv_obj_t *parent,
                                    const demo_lvgl_photo_fs_item_t *item)
{
    demo_lvgl_photo_fs_static_img_t *img_ctx;
    lv_obj_t *img;
    lv_obj_t *player_img;
    int ret;

    if ((parent == NULL) || (item == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (item->format == DEMO_LVGL_PHOTO_FS_FORMAT_GIF) {
        player_img = demo_lvgl_photo_fs_gif_player_create(parent, item->local_path, item->name);
        if (player_img == NULL) {
            liot_trace("%s gif attach failed id=%s path=%s",
                       DEMO_LVGL_PHOTO_FS_MEDIA_LOG_PREFIX,
                       item->id,
                       item->local_path);
            return LIOT_EXTFLASH_READ_FAIL;
        }
        return 0;
    }

    if ((item->format != DEMO_LVGL_PHOTO_FS_FORMAT_BJP) &&
        (item->format != DEMO_LVGL_PHOTO_FS_FORMAT_JPEG)) {
        liot_trace("%s unsupported format=%d id=%s",
                   DEMO_LVGL_PHOTO_FS_MEDIA_LOG_PREFIX,
                   (int)item->format,
                   item->id);
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    img_ctx = (demo_lvgl_photo_fs_static_img_t *)liot_rtos_malloc(sizeof(*img_ctx));
    if (img_ctx == NULL) {
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }
    memset(img_ctx, 0, sizeof(*img_ctx));

    if (item->format == DEMO_LVGL_PHOTO_FS_FORMAT_BJP) {
        ret = demo_lvgl_photo_fs_media_load_bjp(item, &img_ctx->dsc, &img_ctx->buf);
    } else {
#if DEMO_LVGL_PHOTO_FS_ENABLE_JPEG
        ret = demo_lvgl_photo_fs_media_load_jpeg(item, &img_ctx->dsc, &img_ctx->buf);
#else
        ret = LIOT_EXTFLASH_INVALID_PARAMETER;
#endif
    }
    if (ret != 0) {
        liot_rtos_free(img_ctx);
        liot_trace("%s static attach failed ret=%d id=%s fmt=%d",
                   DEMO_LVGL_PHOTO_FS_MEDIA_LOG_PREFIX,
                   ret,
                   item->id,
                   (int)item->format);
        return ret;
    }

    img = lv_img_create(parent);
    if (img == NULL) {
        demo_lvgl_photo_fs_media_buf_free(img_ctx->buf);
        liot_rtos_free(img_ctx);
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    lv_img_set_src(img, &img_ctx->dsc);
    lv_obj_center(img);
    lv_obj_add_event_cb(img, demo_lvgl_photo_fs_media_img_delete_cb, LV_EVENT_DELETE, img_ctx);
    return 0;
}
