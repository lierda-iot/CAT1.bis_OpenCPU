#include "app_display_port_lvgl_internal.h"

#include <stdint.h>
#include <string.h>

#include "app_osal.h"

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#define DRAWING_TIMER_MS 45U
#define DRAWING_CELL_COUNT 9U
#define DRAWING_CELL_COLS 3U
#define DRAWING_CELL_ROWS 3U
#define DRAWING_MARGIN 8
#define DRAWING_GAP 4

typedef enum {
    DRAWING_SHAPE_LINE,
    DRAWING_SHAPE_CROSS,
    DRAWING_SHAPE_TRIANGLE,
    DRAWING_SHAPE_SQUARE,
    DRAWING_SHAPE_PENTAGON,
    DRAWING_SHAPE_HEXAGON,
    DRAWING_SHAPE_HEPTAGON,
    DRAWING_SHAPE_OCTAGON,
    DRAWING_SHAPE_CIRCLE,
} drawing_shape_t;

typedef struct {
    drawing_shape_t shape;
    lv_color_t color;
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
} drawing_item_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *canvas;
    lv_timer_t *timer;
    lv_color_t *buf;
    lv_coord_t w;
    lv_coord_t h;
    lv_color_t round_color;
    drawing_item_t item[DRAWING_CELL_COUNT];
    uint8_t index;
    uint16_t progress;
    uint8_t round;
} drawing_ctx_t;

static drawing_ctx_t s_drawing;

static const drawing_shape_t s_drawing_sequence[DRAWING_CELL_COUNT] = {
    DRAWING_SHAPE_LINE,
    DRAWING_SHAPE_CROSS,
    DRAWING_SHAPE_TRIANGLE,
    DRAWING_SHAPE_SQUARE,
    DRAWING_SHAPE_PENTAGON,
    DRAWING_SHAPE_HEXAGON,
    DRAWING_SHAPE_HEPTAGON,
    DRAWING_SHAPE_OCTAGON,
    DRAWING_SHAPE_CIRCLE,
};

static const lv_color_t s_drawing_round_palette[] = {
    LV_COLOR_MAKE(0x1D, 0x4E, 0xD8),
    LV_COLOR_MAKE(0xDC, 0x26, 0x26),
    LV_COLOR_MAKE(0x16, 0xA3, 0x4A),
    LV_COLOR_MAKE(0xF9, 0x73, 0x16),
    LV_COLOR_MAKE(0x7C, 0x3A, 0xED),
    LV_COLOR_MAKE(0x0F, 0x76, 0x6E),
    LV_COLOR_MAKE(0xDB, 0x27, 0x77),
    LV_COLOR_MAKE(0xD9, 0x77, 0x06),
};

static void drawing_release_canvas(void)
{
    if (s_drawing.canvas != NULL) {
        lv_obj_del(s_drawing.canvas);
        s_drawing.canvas = NULL;
    }
    if (s_drawing.buf != NULL) {
        app_os_free(s_drawing.buf);
        s_drawing.buf = NULL;
    }
}

static void drawing_prepare_item(uint8_t index)
{
    drawing_item_t *it = &s_drawing.item[index];
    lv_coord_t cell_w;
    lv_coord_t cell_h;
    lv_coord_t base_x;
    lv_coord_t base_y;

    cell_w = (s_drawing.w - (2 * DRAWING_MARGIN) - ((DRAWING_CELL_COLS - 1U) * DRAWING_GAP)) / DRAWING_CELL_COLS;
    cell_h = (s_drawing.h - (2 * DRAWING_MARGIN) - ((DRAWING_CELL_ROWS - 1U) * DRAWING_GAP)) / DRAWING_CELL_ROWS;
    base_x = DRAWING_MARGIN + (lv_coord_t)(index % DRAWING_CELL_COLS) * (cell_w + DRAWING_GAP);
    base_y = DRAWING_MARGIN + (lv_coord_t)(index / DRAWING_CELL_COLS) * (cell_h + DRAWING_GAP);

    it->shape = s_drawing_sequence[index % DRAWING_CELL_COUNT];
    it->color = s_drawing.round_color;
    it->x = base_x + 8;
    it->y = base_y + 8;
    it->w = (cell_w > 16) ? (cell_w - 16) : cell_w;
    it->h = (cell_h > 16) ? (cell_h - 16) : cell_h;
    if (it->w < 20) {
        it->w = 20;
    }
    if (it->h < 20) {
        it->h = 20;
    }
}

static void drawing_prepare_round(void)
{
    uint8_t i;

    s_drawing.round++;
    s_drawing.index = 0U;
    s_drawing.progress = 0U;
    s_drawing.round_color = s_drawing_round_palette[(s_drawing.round - 1U) %
                                                   (sizeof(s_drawing_round_palette) / sizeof(s_drawing_round_palette[0]))];
    for (i = 0U; i < DRAWING_CELL_COUNT; i++) {
        drawing_prepare_item(i);
    }
    app_log("display drawing round: %u", (unsigned int)s_drawing.round);
}

static void drawing_draw_line(const drawing_item_t *it, uint16_t progress)
{
    lv_point_t pts[2];
    lv_coord_t x2 = it->x + (lv_coord_t)(((it->w - 1U) * progress) / 100U);
    lv_draw_line_dsc_t dsc;

    lv_draw_line_dsc_init(&dsc);
    dsc.color = it->color;
    dsc.width = 4;
    dsc.round_start = 1;
    dsc.round_end = 1;
    pts[0].x = it->x;
    pts[0].y = it->y + it->h / 2;
    pts[1].x = x2;
    pts[1].y = it->y + it->h / 2;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
}

static void drawing_draw_square(const drawing_item_t *it, uint16_t progress)
{
    lv_draw_line_dsc_t dsc;
    lv_point_t pts[2];
    lv_coord_t side = (it->w < it->h) ? it->w : it->h;
    lv_coord_t left = it->x + (it->w - side) / 2;
    lv_coord_t top = it->y + (it->h - side) / 2;
    lv_coord_t right = left + side - 1;
    lv_coord_t bottom = top + side - 1;
    lv_coord_t span;

    lv_draw_line_dsc_init(&dsc);
    dsc.color = it->color;
    dsc.width = 3;
    dsc.round_start = 1;
    dsc.round_end = 1;

    if (progress < 25U) {
        span = (lv_coord_t)(((side - 1U) * progress) / 25U);
        pts[0].x = left;
        pts[0].y = top;
        pts[1].x = left + span;
        pts[1].y = top;
        lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
        return;
    }

    pts[0].x = left;
    pts[0].y = top;
    pts[1].x = right;
    pts[1].y = top;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);

    if (progress < 50U) {
        span = (lv_coord_t)(((side - 1U) * (progress - 25U)) / 25U);
        pts[0].x = right;
        pts[0].y = top;
        pts[1].x = right;
        pts[1].y = top + span;
        lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
        return;
    }

    pts[0].x = right;
    pts[0].y = top;
    pts[1].x = right;
    pts[1].y = bottom;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);

    if (progress < 75U) {
        span = (lv_coord_t)(((side - 1U) * (progress - 50U)) / 25U);
        pts[0].x = right;
        pts[0].y = bottom;
        pts[1].x = right - span;
        pts[1].y = bottom;
        lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
        return;
    }

    pts[0].x = right;
    pts[0].y = bottom;
    pts[1].x = left;
    pts[1].y = bottom;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);

    span = (lv_coord_t)(((side - 1U) * (progress - 75U)) / 25U);
    pts[0].x = left;
    pts[0].y = bottom;
    pts[1].x = left;
    pts[1].y = bottom - span;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
}

static void drawing_draw_cross(const drawing_item_t *it, uint16_t progress)
{
    lv_draw_line_dsc_t dsc;
    lv_point_t pts[2];
    lv_coord_t left = it->x;
    lv_coord_t top = it->y;
    lv_coord_t right = it->x + it->w - 1;
    lv_coord_t mid_x = it->x + it->w / 2;
    lv_coord_t mid_y = it->y + it->h / 2;
    lv_coord_t horiz_end;
    lv_coord_t vert_end;

    lv_draw_line_dsc_init(&dsc);
    dsc.color = it->color;
    dsc.width = 4;
    dsc.round_start = 1;
    dsc.round_end = 1;

    if (progress < 50U) {
        horiz_end = left + (lv_coord_t)(((it->w - 1U) * progress) / 50U);
        pts[0].x = left;
        pts[0].y = top + it->h / 2;
        pts[1].x = horiz_end;
        pts[1].y = top + it->h / 2;
        lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
        return;
    }

    pts[0].x = left;
    pts[0].y = mid_y;
    pts[1].x = right;
    pts[1].y = mid_y;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);

    vert_end = top + (lv_coord_t)(((it->h - 1U) * (progress - 50U)) / 50U);
    pts[0].x = mid_x;
    pts[0].y = top;
    pts[1].x = mid_x;
    pts[1].y = vert_end;
    lv_canvas_draw_line(s_drawing.canvas, pts, 2, &dsc);
}

static void drawing_draw_circle(const drawing_item_t *it, uint16_t progress)
{
    lv_draw_arc_dsc_t dsc;
    lv_point_t center;
    uint16_t end = (uint16_t)((360U * progress) / 100U);

    lv_draw_arc_dsc_init(&dsc);
    dsc.color = it->color;
    dsc.width = 4;
    dsc.opa = LV_OPA_COVER;
    dsc.rounded = 1;
    center.x = it->x + it->w / 2;
    center.y = it->y + it->h / 2;
    lv_canvas_draw_arc(s_drawing.canvas,
                       center.x,
                       center.y,
                       ((it->w < it->h ? it->w : it->h) - 1U) / 2,
                       0U,
                       end,
                       &dsc);
}

static void drawing_draw_polygon(const drawing_item_t *it, uint16_t progress, uint8_t sides)
{
    lv_point_t pts[9];
    uint32_t i;
    uint32_t cnt;
    lv_coord_t cx = it->x + it->w / 2;
    lv_coord_t cy = it->y + it->h / 2;
    lv_coord_t rx = (it->w - 1U) / 2;
    lv_coord_t ry = (it->h - 1U) / 2;
    lv_draw_line_dsc_t dsc;

    if (sides < 3U) {
        return;
    }
    if (sides > 8U) {
        sides = 8U;
    }

    for (i = 0U; i < sides; i++) {
        int16_t angle = (int16_t)(90 + (360 / sides) * i);
        pts[i].x = cx + (lv_coord_t)((rx * lv_trigo_cos(angle)) / LV_TRIGO_SIN_MAX);
        pts[i].y = cy - (lv_coord_t)((ry * lv_trigo_sin(angle)) / LV_TRIGO_SIN_MAX);
    }
    pts[sides] = pts[0];
    cnt = 2U + ((sides - 1U) * progress) / 100U;
    if (cnt > (sides + 1U)) {
        cnt = sides + 1U;
    }

    lv_draw_line_dsc_init(&dsc);
    dsc.color = it->color;
    dsc.width = 3;
    dsc.round_start = 1;
    dsc.round_end = 1;
    if (cnt >= 2U) {
        lv_canvas_draw_line(s_drawing.canvas, pts, cnt, &dsc);
    }
}

static void drawing_draw_current(void)
{
    drawing_item_t *it;

    if (s_drawing.index >= DRAWING_CELL_COUNT || s_drawing.canvas == NULL) {
        return;
    }

    it = &s_drawing.item[s_drawing.index];
    switch (it->shape) {
    case DRAWING_SHAPE_LINE:
        drawing_draw_line(it, s_drawing.progress);
        break;
    case DRAWING_SHAPE_CROSS:
        drawing_draw_cross(it, s_drawing.progress);
        break;
    case DRAWING_SHAPE_TRIANGLE:
        drawing_draw_polygon(it, s_drawing.progress, 3U);
        break;
    case DRAWING_SHAPE_SQUARE:
        drawing_draw_square(it, s_drawing.progress);
        break;
    case DRAWING_SHAPE_PENTAGON:
        drawing_draw_polygon(it, s_drawing.progress, 5U);
        break;
    case DRAWING_SHAPE_HEXAGON:
        drawing_draw_polygon(it, s_drawing.progress, 6U);
        break;
    case DRAWING_SHAPE_HEPTAGON:
        drawing_draw_polygon(it, s_drawing.progress, 7U);
        break;
    case DRAWING_SHAPE_OCTAGON:
        drawing_draw_polygon(it, s_drawing.progress, 8U);
        break;
    case DRAWING_SHAPE_CIRCLE:
        drawing_draw_circle(it, s_drawing.progress);
        break;
    default:
        drawing_draw_line(it, s_drawing.progress);
        break;
    }
}

static void drawing_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_drawing.canvas == NULL) {
        return;
    }
    if (s_drawing.progress < 100U) {
        s_drawing.progress += 8U;
        if (s_drawing.progress > 100U) {
            s_drawing.progress = 100U;
        }
    }
    drawing_draw_current();
    if (s_drawing.progress >= 100U) {
        s_drawing.progress = 0U;
        s_drawing.index++;
        if (s_drawing.index >= DRAWING_CELL_COUNT) {
            drawing_prepare_round();
        }
    }
}

int display_lvgl_drawing_create(lv_obj_t *root,
                                lv_coord_t screen_width,
                                lv_coord_t screen_height)
{
    lv_coord_t content_h;

    if (root == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    content_h = screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT;
    if (content_h <= 0) {
        return APP_ERR_INVALID_ARG;
    }

    s_drawing.container = lv_obj_create(root);
    if (s_drawing.container == NULL) {
        return APP_ERR_FAIL;
    }
    lv_obj_set_size(s_drawing.container, screen_width, content_h);
    lv_obj_set_pos(s_drawing.container, 0, APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    lv_obj_clear_flag(s_drawing.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_drawing.container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_drawing.container, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_drawing.container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_drawing.container, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_drawing.container, LV_OBJ_FLAG_HIDDEN);

    s_drawing.w = screen_width;
    s_drawing.h = content_h;
    app_log("display drawing create: %dx%d", (int)screen_width, (int)content_h);
    return APP_OK;
}

void display_lvgl_drawing_set_visible(bool visible)
{
    if (s_drawing.container == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_drawing.container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_drawing.container, LV_OBJ_FLAG_HIDDEN);
    }
}

int display_lvgl_drawing_start(void)
{
    uint32_t buf_pixels;

    if (s_drawing.container == NULL) {
        return APP_ERR_NOT_READY;
    }
    if (s_drawing.canvas == NULL) {
        s_drawing.canvas = lv_canvas_create(s_drawing.container);
        if (s_drawing.canvas == NULL) {
            return APP_ERR_FAIL;
        }
        buf_pixels = (uint32_t)s_drawing.w * (uint32_t)s_drawing.h;
        s_drawing.buf = (lv_color_t *)app_os_malloc(buf_pixels * sizeof(lv_color_t));
        if (s_drawing.buf == NULL) {
            drawing_release_canvas();
            app_log("display drawing buffer alloc failed: %lu bytes",
                    (unsigned long)(buf_pixels * sizeof(lv_color_t)));
            return APP_ERR_NO_MEMORY;
        }
        memset(s_drawing.buf, 0, buf_pixels * sizeof(lv_color_t));
        lv_canvas_set_buffer(s_drawing.canvas, s_drawing.buf, s_drawing.w, s_drawing.h, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(s_drawing.canvas, 0, 0);
        lv_canvas_fill_bg(s_drawing.canvas, lv_color_white(), LV_OPA_COVER);
    }
    if (s_drawing.timer == NULL) {
        s_drawing.timer = lv_timer_create(drawing_timer_cb, DRAWING_TIMER_MS, NULL);
        if (s_drawing.timer == NULL) {
            drawing_release_canvas();
            return APP_ERR_NO_MEMORY;
        }
    } else {
        lv_timer_resume(s_drawing.timer);
    }
    drawing_prepare_round();
    display_lvgl_drawing_set_visible(true);
    return APP_OK;
}

void display_lvgl_drawing_stop(void)
{
    if (s_drawing.timer != NULL) {
        lv_timer_del(s_drawing.timer);
        s_drawing.timer = NULL;
    }
    if (s_drawing.canvas != NULL) {
        lv_obj_del(s_drawing.canvas);
        s_drawing.canvas = NULL;
    }
    if (s_drawing.buf != NULL) {
        app_os_free(s_drawing.buf);
        s_drawing.buf = NULL;
    }
    display_lvgl_drawing_set_visible(false);
}
#else
int display_lvgl_drawing_create(lv_obj_t *root,
                                lv_coord_t screen_width,
                                lv_coord_t screen_height)
{
    (void)root;
    (void)screen_width;
    (void)screen_height;
    return APP_ERR_NOT_SUPPORTED;
}

void display_lvgl_drawing_set_visible(bool visible)
{
    (void)visible;
}

int display_lvgl_drawing_start(void)
{
    return APP_ERR_NOT_SUPPORTED;
}

void display_lvgl_drawing_stop(void)
{
}
#endif
