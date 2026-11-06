/* LVGL task adapter for the tangeai/xiaotai-esp32 product procedural face.
 * Original tables/rasterizer: assets/ui_face_engine.inc (source attribution).
 * CAT.1 adaptations: no ESP task, no PSRAM allocation, no image zoom/rotation;
 * one static 220x108 RGB565 canvas, 40 ms LVGL timer, and flash P4 background.
 */
#include "ui_face.h"
#include "assets/ui_face_background.h"
#include <stdlib.h>
#include <string.h>

#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
#error "The face backdrop requires native RGB565 (LV_COLOR_DEPTH=16, SWAP=0)"
#endif

#define FACE_CANVAS_W UI_FACE_WIDTH
#define FACE_CANVAS_H UI_FACE_HEIGHT
static lv_obj_t *s_face;
static bool s_face_paused;
static lv_color_t s_face_canvas_buffer[FACE_CANVAS_W * FACE_CANVAS_H];
static int32_t s_face_bg_x, s_face_bg_y;

/* Extend the existing LVGL 1 ms tick, including its uint32_t wrap. This is
 * called on the LVGL task only; no second clock or interrupt is introduced. */
static int64_t monotonic_ms(void)
{
    static uint32_t previous;
    static uint64_t elapsed;
    uint32_t now = lv_tick_get();
    elapsed += (uint32_t)(now - previous);
    previous = now;
    return (int64_t)elapsed;
}

lv_color_t ui_face_background_color(void)
{
    return lv_color_hex(0x1C1F22);
}

static lv_color_t product_face_background_color(void)
{
    return ui_face_background_color();
}

static bool ui_face_update_backdrop_position(void)
{
    lv_area_t area;
    bool moved;
    if (s_face == NULL) return false;
    lv_obj_get_coords(s_face, &area);
    moved = s_face_bg_x != area.x1 || s_face_bg_y != area.y1;
    if (moved) {
        /* Moving an existing face requires restoring the complete crop. */
        s_face_bg_x = area.x1;
        s_face_bg_y = area.y1;
    }
    return moved;
}

static lv_color_t ui_face_backdrop_pixel(int x, int y)
{
    lv_color_t color;
    x += s_face_bg_x;
    y += s_face_bg_y;
    if (x < 0 || x >= 320 || y < 0 || y >= 240)
        return product_face_background_color();
    color.full = ui_face_background_pixels[y * 320 + x];
    return color;
}

#include "assets/ui_face_engine.inc"

static bool face_room_available(void)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    /* Object creation invokes LVGL's allocation assertion on OOM. Reserve a
     * conservative small-object allowance before entering that path. No face
     * pixels are allocated from this heap. The LVGL task is the only caller. */
    return m.free_biggest_size >= 2048U;
}

static void face_stop_timer(void)
{
    lv_timer_t *timer = s3_face.timer;
    s3_face.timer = NULL;
    s3_face.attached = false;
    if (timer != NULL) lv_timer_del(timer);
}

static void face_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != s_face) return;
    s_face = NULL;
    face_stop_timer();
}

void ui_face_detach(void)
{
    lv_obj_t *object = s_face;
    s_face = NULL;
    face_stop_timer();
    s_face_paused = false;
    if (object != NULL) lv_obj_del(object);
}

lv_obj_t *ui_face_background(lv_obj_t *parent)
{
    lv_obj_t *image;
    if (parent == NULL || !face_room_available()) return NULL;
    image = lv_img_create(parent);
    if (image == NULL) return NULL;
    lv_obj_remove_style_all(image);
    lv_img_set_src(image, &ui_face_background_image);
    lv_obj_set_pos(image, 0, 0);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return image;
}

lv_obj_t *ui_face_create(lv_obj_t *parent, int32_t x, int32_t y)
{
    int64_t now;
#ifdef TIRTC_EXTERNAL_UI_ASSETS
    if (!s3_face_resources_ready) return NULL;
#endif
    if (parent == NULL || !face_room_available()) return NULL;
    ui_face_detach();
    memset(&s3_face, 0, sizeof(s3_face));
    s3_face.timer = lv_timer_create(s3_face_frame, S3_FACE_PERIOD_MS, NULL);
    if (s3_face.timer == NULL) return NULL;
    lv_timer_pause(s3_face.timer);
    s_face = lv_canvas_create(parent);
    if (s_face == NULL) {
        face_stop_timer();
        return NULL;
    }
    lv_obj_remove_style_all(s_face);
    lv_canvas_set_buffer(s_face, s_face_canvas_buffer,
                         FACE_CANVAS_W, FACE_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_face, x, y);
    lv_obj_clear_flag(s_face, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_face, face_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_update_layout(s_face);
    now = monotonic_ms();
    s3_face.attached = true;
    s3_face.clear_all = true;
    s3_face.random = (uint32_t)now | 1U;
    s3_face.gaze_at = now;
    s3_face.gaze_next = now + 6000;
    s3_face.blink_at = now + 2600;
    s_face_bg_x = x;
    s_face_bg_y = y;
    s3_face_set_target("neutral", UI_FACE_IDLE, 0, UI_FACE_VARIANT_AUTO);
    s3_face_frame(NULL);
    lv_timer_resume(s3_face.timer);
    lv_timer_ready(s3_face.timer);
    return s_face;
}

void ui_face_set(const char *key, uint8_t phase, uint8_t audio_level, uint8_t variant)
{
    if (s_face == NULL || !s3_face.attached) return;
    if (phase > UI_FACE_ERROR) phase = UI_FACE_IDLE;
    s3_face_set_target(key, (ui_face_phase_t)phase, audio_level, variant);
}

void ui_face_pause(bool paused)
{
    s_face_paused = paused;
    if (s3_face.timer == NULL) return;
    if (paused) lv_timer_pause(s3_face.timer);
    else {
        s3_face.last_frame_at = 0;
        lv_timer_resume(s3_face.timer);
        lv_timer_ready(s3_face.timer);
    }
}

uint8_t ui_face_key_count(void)
{
    return (uint8_t)(sizeof(s3_face_poses) / sizeof(s3_face_poses[0]));
}

const char *ui_face_key_at(uint8_t index)
{
#ifdef TIRTC_EXTERNAL_UI_ASSETS
    return index < ui_face_key_count() ? s3_face_resource_keys[index] : NULL;
#else
    return index < ui_face_key_count() ? s3_face_poses[index].key : NULL;
#endif
}

uint8_t ui_face_variant_count(const char *key)
{
    return (uint8_t)s3_face_variant_count(s3_face_pose_index(key == NULL ? "neutral" : key));
}
