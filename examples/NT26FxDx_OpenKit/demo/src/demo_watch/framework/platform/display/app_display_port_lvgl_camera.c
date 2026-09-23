#include "app_display_port_lvgl_internal.h"

#include <stdint.h>
#include <string.h>

#include "app_osal.h"

#define APP_DISPLAY_LVGL_CAMERA_RGB565_BYTES_PER_PIXEL 2U
#define APP_DISPLAY_LVGL_CAMERA_GRAY_BYTES_PER_PIXEL 1U

#ifndef APP_DISPLAY_LVGL_CAMERA_RGB565_MSB_FIRST
#define APP_DISPLAY_LVGL_CAMERA_RGB565_MSB_FIRST 1
#endif

#ifndef APP_DISPLAY_LVGL_CAMERA_RGB565_BGR
#define APP_DISPLAY_LVGL_CAMERA_RGB565_BGR 1
#endif

#ifndef APP_DISPLAY_LVGL_CAMERA_YUYV_BGR
#define APP_DISPLAY_LVGL_CAMERA_YUYV_BGR 1
#endif

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
static lv_color_t *s_camera_preview_buf;
static lv_img_dsc_t s_camera_preview_dsc;
static lv_obj_t *s_camera_container;
static lv_obj_t *s_camera_image;
static lv_obj_t *s_camera_message_label;
static uint16_t s_camera_preview_width;
static uint16_t s_camera_preview_height;

static void display_lvgl_camera_gesture_event_cb(lv_event_t *event)
{
    lv_indev_t *indev;
    lv_dir_t dir;

    if (event == NULL || lv_event_get_code(event) != LV_EVENT_GESTURE ||
        s_display_lvgl.screen != APP_DISPLAY_SCREEN_CAMERA) {
        return;
    }

    indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }
    dir = lv_indev_get_gesture_dir(indev);
    display_lvgl_input_emit_back_swipe_from_gesture((int)dir);
}

static uint16_t display_lvgl_camera_preview_size(void)
{
    uint16_t width = s_display_lvgl.active_caps.width;
    uint16_t height = s_display_lvgl.active_caps.height;
    uint16_t content_height;

    if (height > APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT) {
        content_height = (uint16_t)(height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    } else {
        content_height = height;
    }
    if (width == 0U || content_height == 0U) {
        return 0U;
    }
    return (width < content_height) ? width : content_height;
}

static void display_lvgl_camera_layout_message(lv_coord_t screen_width,
                                               lv_coord_t content_height)
{
    lv_coord_t label_width;
    lv_coord_t label_y;

    if (s_camera_message_label == NULL) {
        return;
    }

    label_width = (screen_width > 16) ? (screen_width - 16) : screen_width;
    label_y = (content_height > 38) ? (content_height - 36) : 4;
    lv_obj_set_width(s_camera_message_label, label_width);
    lv_obj_set_pos(s_camera_message_label,
                   (screen_width - label_width) / 2,
                   label_y);
}

void display_lvgl_camera_set_message(app_display_role_t role, const char *text)
{
    (void)role;
    if (s_camera_message_label == NULL) {
        return;
    }
    lv_label_set_text(s_camera_message_label, (text != NULL) ? text : "");
    if (text == NULL || text[0] == '\0') {
        lv_obj_add_flag(s_camera_message_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_camera_message_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void display_lvgl_camera_release_preview_buffer(void)
{
    if (s_camera_preview_buf == NULL) {
        return;
    }
    if (s_camera_image != NULL) {
        lv_img_set_src(s_camera_image, NULL);
    }
    app_os_free(s_camera_preview_buf);
    s_camera_preview_buf = NULL;
    memset(&s_camera_preview_dsc, 0, sizeof(s_camera_preview_dsc));
    s_camera_preview_width = 0U;
    s_camera_preview_height = 0U;
    app_log("display camera preview buffer released");
}

void display_lvgl_camera_reset_preview(void)
{
    if (s_camera_preview_buf == NULL || s_camera_preview_width == 0U ||
        s_camera_preview_height == 0U) {
        return;
    }
    memset(s_camera_preview_buf,
           0,
           (uint32_t)s_camera_preview_width *
           (uint32_t)s_camera_preview_height *
           sizeof(lv_color_t));
    lv_img_cache_invalidate_src(&s_camera_preview_dsc);
    if (s_camera_image != NULL) {
        lv_obj_invalidate(s_camera_image);
    }
}

static int display_lvgl_camera_ensure_preview_buffer(void)
{
    uint16_t preview_size;
    uint32_t preview_pixel_count;
    lv_coord_t screen_width = (lv_coord_t)s_display_lvgl.active_caps.width;

    if (s_camera_preview_buf != NULL) {
        return APP_OK;
    }

    preview_size = display_lvgl_camera_preview_size();
    preview_pixel_count = (uint32_t)preview_size * (uint32_t)preview_size;
    if (preview_pixel_count == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    s_camera_preview_buf = (lv_color_t *)app_os_malloc(preview_pixel_count * sizeof(lv_color_t));
    if (s_camera_preview_buf == NULL) {
        app_log("display camera preview buffer alloc failed: %lu bytes",
                (unsigned long)(preview_pixel_count * sizeof(lv_color_t)));
        return APP_ERR_NO_MEMORY;
    }
    memset(s_camera_preview_buf, 0, preview_pixel_count * sizeof(lv_color_t));
    s_camera_preview_width = preview_size;
    s_camera_preview_height = preview_size;
    memset(&s_camera_preview_dsc, 0, sizeof(s_camera_preview_dsc));
    s_camera_preview_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_camera_preview_dsc.header.w = s_camera_preview_width;
    s_camera_preview_dsc.header.h = s_camera_preview_height;
    s_camera_preview_dsc.data_size = preview_pixel_count * sizeof(lv_color_t);
    s_camera_preview_dsc.data = (const uint8_t *)s_camera_preview_buf;
    if (s_camera_image != NULL) {
        lv_img_set_src(s_camera_image, &s_camera_preview_dsc);
        lv_obj_set_pos(s_camera_image,
                       (screen_width - (lv_coord_t)s_camera_preview_width) / 2,
                       0);
    }
    app_log("display camera preview buffer ready: %ux%u bytes=%lu",
            (unsigned int)s_camera_preview_width,
            (unsigned int)s_camera_preview_height,
            (unsigned long)s_camera_preview_dsc.data_size);
    return APP_OK;
}

static uint8_t display_lvgl_camera_format_bytes_per_pixel(app_display_frame_format_t format)
{
    switch (format) {
    case APP_DISPLAY_FRAME_FORMAT_RGB565:
    case APP_DISPLAY_FRAME_FORMAT_YUYV:
        return APP_DISPLAY_LVGL_CAMERA_RGB565_BYTES_PER_PIXEL;
    case APP_DISPLAY_FRAME_FORMAT_GRAY:
        return APP_DISPLAY_LVGL_CAMERA_GRAY_BYTES_PER_PIXEL;
    default:
        return 0U;
    }
}

bool display_lvgl_camera_frame_is_valid(const app_display_camera_frame_t *frame)
{
    uint32_t pixel_count;
    uint32_t expected_len;
    uint8_t expected_bytes_per_pixel;

    if (frame == NULL || frame->data == NULL || frame->width == 0U ||
        frame->height == 0U || frame->bytes_per_pixel == 0U) {
        return false;
    }
    expected_bytes_per_pixel = display_lvgl_camera_format_bytes_per_pixel(frame->format);
    if (expected_bytes_per_pixel == 0U ||
        frame->bytes_per_pixel != expected_bytes_per_pixel) {
        return false;
    }
    if (frame->format == APP_DISPLAY_FRAME_FORMAT_YUYV &&
        (frame->width & 1U) != 0U) {
        return false;
    }
    pixel_count = (uint32_t)frame->width * (uint32_t)frame->height;
    if (pixel_count == 0U ||
        pixel_count > (UINT32_MAX / frame->bytes_per_pixel)) {
        return false;
    }
    expected_len = pixel_count * frame->bytes_per_pixel;
    return frame->len >= expected_len;
}

static uint16_t display_lvgl_camera_load_rgb565(const uint8_t *pixel)
{
    uint16_t value;

    if (pixel == NULL) {
        return 0U;
    }

#if APP_DISPLAY_LVGL_CAMERA_RGB565_MSB_FIRST
    value = ((uint16_t)pixel[0] << 8) | (uint16_t)pixel[1];
#else
    value = ((uint16_t)pixel[1] << 8) | (uint16_t)pixel[0];
#endif

#if APP_DISPLAY_LVGL_CAMERA_RGB565_BGR
    value = (uint16_t)((value & 0x07E0U) |
                       ((value & 0x001FU) << 11) |
                       ((value & 0xF800U) >> 11));
#endif

    return value;
}

static lv_color_t display_lvgl_camera_load_gray8(const uint8_t *pixel)
{
    uint8_t gray;

    if (pixel == NULL) {
        gray = 0U;
    } else {
        gray = pixel[0];
    }
    return lv_color_make(gray, gray, gray);
}

static uint8_t display_lvgl_camera_clamp_u8(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > 255) {
        return 255U;
    }
    return (uint8_t)value;
}

static lv_color_t display_lvgl_camera_load_yuyv(const uint8_t *pair,
                                                bool second_pixel)
{
    int32_t y;
    int32_t u;
    int32_t v;
    int32_t c;
    int32_t d;
    int32_t e;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (pair == NULL) {
        return lv_color_black();
    }

    y = second_pixel ? pair[2] : pair[0];
    u = pair[1];
    v = pair[3];
    c = y - 16;
    d = u - 128;
    e = v - 128;
    if (c < 0) {
        c = 0;
    }

    r = display_lvgl_camera_clamp_u8((298 * c + 409 * e + 128) >> 8);
    g = display_lvgl_camera_clamp_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
    b = display_lvgl_camera_clamp_u8((298 * c + 516 * d + 128) >> 8);
#if APP_DISPLAY_LVGL_CAMERA_YUYV_BGR
    return lv_color_make(b, g, r);
#else
    return lv_color_make(r, g, b);
#endif
}

int display_lvgl_camera_present_frame(const app_display_camera_frame_t *frame)
{
    const uint8_t *src;
    uint32_t dst_w;
    uint32_t dst_h;
    uint32_t src_w;
    uint32_t src_h;
    uint32_t crop_w;
    uint32_t crop_h;
    uint32_t src_x0;
    uint32_t src_y0;
    uint32_t y;
    uint8_t src_bytes_per_pixel;
    int ret;

    if (s_display_lvgl.screen != APP_DISPLAY_SCREEN_CAMERA) {
        return APP_OK;
    }
    if (s_camera_image == NULL) {
        return APP_ERR_NOT_READY;
    }
    if (!display_lvgl_camera_frame_is_valid(frame)) {
        app_log("display camera frame rejected: id=%lu %ux%u len=%lu bpp=%u fmt=%d",
                (unsigned long)((frame != NULL) ? frame->frame_id : 0U),
                (unsigned int)((frame != NULL) ? frame->width : 0U),
                (unsigned int)((frame != NULL) ? frame->height : 0U),
                (unsigned long)((frame != NULL) ? frame->len : 0U),
                (unsigned int)((frame != NULL) ? frame->bytes_per_pixel : 0U),
                (int)((frame != NULL) ? frame->format : APP_DISPLAY_FRAME_FORMAT_RGB565));
        return APP_ERR_INVALID_ARG;
    }
    ret = display_lvgl_camera_ensure_preview_buffer();
    if (ret != APP_OK) {
        return ret;
    }
    dst_w = s_camera_preview_width;
    dst_h = s_camera_preview_height;
    if (dst_w == 0U || dst_h == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    src = frame->data;
    src_w = frame->width;
    src_h = frame->height;
    src_bytes_per_pixel = display_lvgl_camera_format_bytes_per_pixel(frame->format);
    crop_w = src_w;
    crop_h = src_h;

    if ((src_w * dst_h) > (src_h * dst_w)) {
        crop_w = (src_h * dst_w) / dst_h;
    } else {
        crop_h = (src_w * dst_h) / dst_w;
    }
    if (crop_w == 0U || crop_h == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    src_x0 = (src_w - crop_w) / 2U;
    src_y0 = (src_h - crop_h) / 2U;

    for (y = 0U; y < dst_h; y++) {
        uint32_t x;
        uint32_t src_y = src_y0 + ((y * crop_h) / dst_h);
        lv_color_t *dst_row = &s_camera_preview_buf[y * dst_w];
        const uint8_t *src_row = &src[(src_y * src_w) * src_bytes_per_pixel];

        for (x = 0U; x < dst_w; x++) {
            uint32_t src_x = src_x0 + ((x * crop_w) / dst_w);
            const uint8_t *src_pixel = &src_row[src_x * src_bytes_per_pixel];

            if (frame->format == APP_DISPLAY_FRAME_FORMAT_RGB565) {
                dst_row[x].full = display_lvgl_camera_load_rgb565(src_pixel);
            } else if (frame->format == APP_DISPLAY_FRAME_FORMAT_YUYV) {
                uint32_t pair_x = src_x & ~1U;
                const uint8_t *src_pair = &src_row[pair_x * src_bytes_per_pixel];

                dst_row[x] = display_lvgl_camera_load_yuyv(src_pair,
                                                           (src_x & 1U) != 0U);
            } else {
                dst_row[x] = display_lvgl_camera_load_gray8(src_pixel);
            }
        }
    }

    s_display_lvgl.camera_frame_count++;
    if (s_display_lvgl.camera_frame_count <= 3U ||
        (s_display_lvgl.camera_frame_count % 30U) == 0U) {
        app_log("display camera frame: id=%lu src=%ux%u crop=%lux%lu dst=%ux%u count=%lu fmt=%d bpp=%u msb=%d bgr=%d yuyv_bgr=%d",
                (unsigned long)frame->frame_id,
                (unsigned int)frame->width,
                (unsigned int)frame->height,
                (unsigned long)crop_w,
                (unsigned long)crop_h,
                (unsigned int)s_camera_preview_width,
                (unsigned int)s_camera_preview_height,
                (unsigned long)s_display_lvgl.camera_frame_count,
                (int)frame->format,
                (unsigned int)src_bytes_per_pixel,
                APP_DISPLAY_LVGL_CAMERA_RGB565_MSB_FIRST ? 1 : 0,
                APP_DISPLAY_LVGL_CAMERA_RGB565_BGR ? 1 : 0,
                APP_DISPLAY_LVGL_CAMERA_YUYV_BGR ? 1 : 0);
    }

    lv_img_cache_invalidate_src(&s_camera_preview_dsc);
    lv_obj_invalidate(s_camera_image);
    return APP_OK;
}

void display_lvgl_camera_set_visible(bool visible)
{
    if (s_camera_container == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_camera_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_camera_container, LV_OBJ_FLAG_HIDDEN);
    }
}

int display_lvgl_camera_create(lv_obj_t *root,
                               lv_coord_t screen_width,
                               lv_coord_t screen_height)
{
    lv_coord_t content_height;

    if (root == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    content_height = (screen_height > APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT) ?
                     (screen_height - APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT) : 0;

    s_camera_container = lv_obj_create(root);
    if (s_camera_container == NULL) {
        app_log("display lvgl UI camera container create failed");
        return APP_ERR_FAIL;
    }

    lv_obj_set_size(s_camera_container,
                    screen_width,
                    content_height);
    lv_obj_set_pos(s_camera_container, 0, APP_DISPLAY_LVGL_STATUS_BAR_HEIGHT);
    lv_obj_clear_flag(s_camera_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_camera_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(s_camera_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_camera_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_camera_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_camera_container, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_camera_container,
                        display_lvgl_camera_gesture_event_cb,
                        LV_EVENT_GESTURE,
                        NULL);

    s_camera_image = lv_img_create(s_camera_container);
    if (s_camera_image == NULL) {
        app_log("display lvgl UI camera image create failed");
        return APP_ERR_FAIL;
    }
    lv_obj_add_flag(s_camera_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_camera_image,
                        display_lvgl_camera_gesture_event_cb,
                        LV_EVENT_GESTURE,
                        NULL);
    if (s_camera_preview_buf != NULL && s_camera_preview_width != 0U &&
        s_camera_preview_height != 0U) {
        lv_img_set_src(s_camera_image, &s_camera_preview_dsc);
        lv_obj_set_pos(s_camera_image,
                       (screen_width - (lv_coord_t)s_camera_preview_width) / 2,
                       0);
    }

    s_camera_message_label = lv_label_create(s_camera_container);
    if (s_camera_message_label == NULL) {
        app_log("display lvgl UI camera message create failed");
        return APP_ERR_FAIL;
    }
    lv_label_set_long_mode(s_camera_message_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_camera_message_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_camera_message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_camera_message_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_camera_message_label, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(s_camera_message_label, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_camera_message_label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_camera_message_label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_camera_message_label, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_camera_message_label, 4, LV_PART_MAIN);
    display_lvgl_camera_layout_message(screen_width, content_height);
    display_lvgl_camera_set_message(s_display_lvgl.chat_role, s_display_lvgl.chat_text);

    display_lvgl_camera_set_visible(false);
    return APP_OK;
}
#else
bool display_lvgl_camera_frame_is_valid(const app_display_camera_frame_t *frame)
{
    (void)frame;
    return false;
}

int display_lvgl_camera_present_frame(const app_display_camera_frame_t *frame)
{
    (void)frame;
    return APP_ERR_NOT_SUPPORTED;
}

void display_lvgl_camera_release_preview_buffer(void)
{
}
#endif
