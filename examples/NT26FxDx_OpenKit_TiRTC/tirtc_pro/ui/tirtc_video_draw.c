#include "tirtc_video_draw.h"
#include "src/draw/sw/lv_draw_sw.h"
#include <stdint.h>
#include <string.h>

/* Application-local optimization for the pinned LVGL 8.3 RGB565 software
 * renderer. It is deliberately an lv_img subclass: ordinary geometry,
 * invalidation, cover checking, layering and unsupported styles stay in LVGL.
 * The object has exactly the same allocation size as an ordinary lv_img. */
#define VIDEO_DRAW_MAX_WIDTH 320
#define VIDEO_DRAW_MAX_SOURCE 320

static void video_draw_event(const lv_obj_class_t *class_p, lv_event_t *event);

static const lv_obj_class_t video_draw_class = {
    .base_class = &lv_img_class,
    .event_cb = video_draw_event,
    .instance_size = sizeof(lv_img_t)
};
static tirtc_video_draw_stats_t draw_stats;

#if LV_COLOR_DEPTH == 16 && LV_DRAW_COMPLEX
static bool draw_rgb565(lv_obj_t *obj, lv_draw_ctx_t *ctx)
{
    const lv_img_t *img = (const lv_img_t *)obj;
    const lv_img_dsc_t *src;
    lv_disp_t *disp = lv_obj_get_disp(obj);
    lv_draw_img_dsc_t dsc;
    lv_area_t area;
    uint16_t xmap[VIDEO_DRAW_MAX_WIDTH];
    int32_t inverse_zoom, xf, yf, x, y, first_x, last_x, width, stride;
    int32_t previous_sy = -1;
    lv_color_t *previous_row = NULL;

    /* Only the normal CPU RGB565 buffer has this known tightly packed layout.
     * In particular a transparent layer stores RGB565+alpha, not uint16_t[]. */
    if (!ctx || !ctx->buf || !ctx->buf_area || !ctx->clip_area || !disp ||
        !disp->driver || disp->driver->screen_transp || disp->driver->set_px_cb ||
        ctx->draw_img_decoded != lv_draw_sw_img_decoded ||
        ctx->draw_transform != lv_draw_sw_transform ||
        ((uintptr_t)ctx->buf & 1U) ||
        img->src_type != LV_IMG_SRC_VARIABLE || img->cf != LV_IMG_CF_TRUE_COLOR ||
        !img->src || img->angle || img->antialias ||
        img->zoom < LV_IMG_ZOOM_NONE || img->zoom > 8192U ||
        img->offset.x || img->offset.y || img->obj_size_mode != LV_IMG_SIZE_MODE_VIRTUAL ||
        img->w <= 0 || img->h <= 0 || img->w > VIDEO_DRAW_MAX_SOURCE || img->h > VIDEO_DRAW_MAX_SOURCE ||
        lv_obj_get_width(obj) != img->w || lv_obj_get_height(obj) != img->h)
        return false;

    /* Drawing the base object's decorations is normally part of lv_img.
     * Never silently discard one if this widget is restyled later. */
    if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != LV_OPA_TRANSP ||
        lv_obj_get_style_bg_img_src(obj, LV_PART_MAIN) ||
        lv_obj_get_style_border_width(obj, LV_PART_MAIN) ||
        lv_obj_get_style_shadow_width(obj, LV_PART_MAIN) ||
        lv_obj_get_style_outline_width(obj, LV_PART_MAIN) ||
        lv_obj_get_style_pad_left(obj, LV_PART_MAIN) ||
        lv_obj_get_style_pad_right(obj, LV_PART_MAIN) ||
        lv_obj_get_style_pad_top(obj, LV_PART_MAIN) ||
        lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN) ||
        lv_obj_get_style_clip_corner(obj, LV_PART_MAIN) ||
        lv_obj_get_style_transform_angle(obj, LV_PART_MAIN) ||
        lv_obj_get_style_transform_zoom_safe(obj, LV_PART_MAIN) != LV_IMG_ZOOM_NONE ||
        lv_obj_get_style_blend_mode(obj, LV_PART_MAIN) != LV_BLEND_MODE_NORMAL)
        return false;

    lv_draw_img_dsc_init(&dsc);
    lv_obj_init_draw_img_dsc(obj, LV_PART_MAIN, &dsc);
    if (dsc.opa != LV_OPA_COVER || dsc.recolor_opa != LV_OPA_TRANSP)
        return false;

    src = (const lv_img_dsc_t *)img->src;
    if (!src->data || ((uintptr_t)src->data & 1U) ||
        src->header.cf != LV_IMG_CF_TRUE_COLOR ||
        src->header.w != (uint32_t)img->w || src->header.h != (uint32_t)img->h ||
        src->data_size < (uint32_t)img->w * (uint32_t)img->h * sizeof(lv_color_t))
        return false;

    _lv_img_buf_get_transformed_area(&area, img->w, img->h, 0, img->zoom, &img->pivot);
    lv_area_move(&area, obj->coords.x1, obj->coords.y1);
    if (!_lv_area_intersect(&area, &area, ctx->clip_area) ||
        !_lv_area_intersect(&area, &area, ctx->buf_area))
        return true;
    width = (int32_t)area.x2 - area.x1 + 1;
    stride = (int32_t)ctx->buf_area->x2 - ctx->buf_area->x1 + 1;
    if (width <= 0 || width > VIDEO_DRAW_MAX_WIDTH || stride <= 0 ||
        stride > VIDEO_DRAW_MAX_WIDTH || ctx->buf_area->y2 < ctx->buf_area->y1 ||
        (int32_t)ctx->buf_area->y2 - ctx->buf_area->y1 + 1 > VIDEO_DRAW_MAX_SOURCE ||
        lv_draw_mask_is_any(&area))
        return false;

    /* Exact angle=0, antialias=0 mapping from LVGL 8.3.10's
     * lv_draw_sw_transform(), including its inverse-zoom truncation and +128
     * nearest-neighbour rounding. No per-pixel division or alpha blending. */
    inverse_zoom = 65536L / img->zoom;
    xf = ((int32_t)area.x1 - obj->coords.x1 - img->pivot.x) * inverse_zoom +
         (int32_t)img->pivot.x * 256 + 128;
    first_x = 0;
    while (first_x < width && xf < 0) { ++first_x; xf += inverse_zoom; }
    last_x = first_x;
    while (last_x < width && xf < (int32_t)img->w * 256) {
        xmap[last_x] = (uint16_t)(xf >> 8);
        ++last_x;
        xf += inverse_zoom;
    }
    if (last_x <= first_x) return true;
    yf = ((int32_t)area.y1 - obj->coords.y1 - img->pivot.y) * inverse_zoom +
         (int32_t)img->pivot.y * 256 + 128;

    /* Preserve ordering with any pending renderer work before CPU stores.
     * Subsequent siblings (labels/buttons/overlays) still render afterwards. */
    lv_draw_wait_for_finish(ctx);
    for (y = area.y1; y <= area.y2; ++y, yf += inverse_zoom) {
        int32_t sy;
        lv_color_t *dst;
        const lv_color_t *row;
        if (yf < 0 || yf >= (int32_t)img->h * 256) continue;
        sy = yf >> 8;
        dst = (lv_color_t *)ctx->buf + (y - ctx->buf_area->y1) * stride +
              area.x1 - ctx->buf_area->x1;
        if (sy == previous_sy) {
            memcpy(dst + first_x, previous_row + first_x,
                   (size_t)(last_x - first_x) * sizeof(lv_color_t));
        } else {
            row = (const lv_color_t *)src->data + sy * img->w;
            for (x = first_x; x < last_x; ++x) dst[x] = row[xmap[x]];
            previous_sy = sy;
        }
        previous_row = dst;
        draw_stats.pixels += (uint32_t)(last_x - first_x);
    }
    return true;
}
#endif

static void video_draw_event(const lv_obj_class_t *class_p, lv_event_t *event)
{
    (void)class_p;
    if (lv_event_get_code(event) == LV_EVENT_DRAW_MAIN) {
#if LV_COLOR_DEPTH == 16 && LV_DRAW_COMPLEX
        if (draw_rgb565(lv_event_get_target(event), lv_event_get_draw_ctx(event))) {
            ++draw_stats.fast_calls;
            return;
        }
#endif
        ++draw_stats.fallback_calls;
    }
    (void)lv_obj_event_base(&video_draw_class, event);
}

lv_obj_t *tirtc_video_draw_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&video_draw_class, parent);
    if (obj) lv_obj_class_init_obj(obj);
    return obj;
}

void tirtc_video_draw_get_stats(tirtc_video_draw_stats_t *stats)
{
    if (stats) *stats = draw_stats;
}
