#include "demo_lvgl_photo_fs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_log.h"
#include "liot_os.h"

#include "demo_lvgl_photo_fs_media.h"
#include "demo_lvgl_photo_fs_seed.h"
#include "demo_lvgl_photo_fs_store.h"
#include "demo_lvgl_photo_fs_types.h"
#include "demo_lvgl_photo_fs_lvgl_port.h"
#include "demo_lvgl_photo_fs_boot_diag.h"

#define DEMO_LVGL_PHOTO_FS_LOG_PREFIX   "[demo_lvgl_photo_fs]"
#define DEMO_LVGL_PHOTO_FS_BOOT_DELAY_MS 2000u
#define DEMO_LVGL_PHOTO_FS_RESEED_STACK (12u * 1024u)

typedef struct {
    demo_lvgl_photo_fs_item_t items[DEMO_LVGL_PHOTO_FS_MAX_ITEMS];
    unsigned int item_count;
    int current_index;
    bool seed_busy;
    bool page_active;
    bool refresh_pending;
    int last_seed_ret;
    uint32_t long_press_tick;
    demo_lvgl_photo_fs_seed_result_t last_seed_result;
    lv_obj_t *active_screen;
    lv_obj_t *title_label;
    lv_obj_t *status_label;
    lv_obj_t *hint_label;
    lv_timer_t *poll_timer;
    liot_task_t reseed_task;
} demo_lvgl_photo_fs_app_t;

static demo_lvgl_photo_fs_app_t g_demo_lvgl_photo_fs_app;

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

static void demo_lvgl_photo_fs_build_title_text(char *buf, unsigned int buf_len)
{
    const demo_lvgl_photo_fs_item_t *item;

    if (g_demo_lvgl_photo_fs_app.item_count == 0u) {
        (void)snprintf(buf, buf_len, "demo_lvgl_photo_fs empty");
        return;
    }

    if ((g_demo_lvgl_photo_fs_app.current_index < 0) ||
        ((unsigned int)g_demo_lvgl_photo_fs_app.current_index >= g_demo_lvgl_photo_fs_app.item_count)) {
        g_demo_lvgl_photo_fs_app.current_index = 0;
    }

    item = &g_demo_lvgl_photo_fs_app.items[g_demo_lvgl_photo_fs_app.current_index];
    (void)snprintf(buf,
                   buf_len,
                   "%u/%u %s %s",
                   (unsigned int)(g_demo_lvgl_photo_fs_app.current_index + 1),
                   g_demo_lvgl_photo_fs_app.item_count,
                   demo_lvgl_photo_fs_format_name(item->format),
                   item->name);
}

static void demo_lvgl_photo_fs_build_status_text(char *buf, unsigned int buf_len)
{
    if (g_demo_lvgl_photo_fs_app.seed_busy) {
        (void)snprintf(buf, buf_len, "reseeding...");
        return;
    }

    if (g_demo_lvgl_photo_fs_app.last_seed_ret != 0) {
        (void)snprintf(buf,
                       buf_len,
                       "seed ret=%d w=%u s=%u f=%u",
                       g_demo_lvgl_photo_fs_app.last_seed_ret,
                       (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.written,
                       (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.skipped,
                       (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.failed);
        return;
    }

    (void)snprintf(buf,
                   buf_len,
                   "seed ok w=%u s=%u f=%u",
                   (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.written,
                   (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.skipped,
                   (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.failed);
}

static void demo_lvgl_photo_fs_build_hint_text(char *buf, unsigned int buf_len)
{
    if (g_demo_lvgl_photo_fs_app.item_count == 0u) {
        (void)snprintf(buf, buf_len, "long press to reseed");
        return;
    }

    (void)snprintf(buf, buf_len, "tap next  swipe prev/next  long press reseed");
}

static void demo_lvgl_photo_fs_update_active_labels(void)
{
    char text[96];

    if ((g_demo_lvgl_photo_fs_app.title_label != NULL) &&
        lv_obj_is_valid(g_demo_lvgl_photo_fs_app.title_label)) {
        demo_lvgl_photo_fs_build_title_text(text, sizeof(text));
        lv_label_set_text(g_demo_lvgl_photo_fs_app.title_label, text);
    }

    if ((g_demo_lvgl_photo_fs_app.status_label != NULL) &&
        lv_obj_is_valid(g_demo_lvgl_photo_fs_app.status_label)) {
        demo_lvgl_photo_fs_build_status_text(text, sizeof(text));
        lv_label_set_text(g_demo_lvgl_photo_fs_app.status_label, text);
    }

    if ((g_demo_lvgl_photo_fs_app.hint_label != NULL) &&
        lv_obj_is_valid(g_demo_lvgl_photo_fs_app.hint_label)) {
        demo_lvgl_photo_fs_build_hint_text(text, sizeof(text));
        lv_label_set_text(g_demo_lvgl_photo_fs_app.hint_label, text);
    }
}

static int demo_lvgl_photo_fs_load_items(void)
{
    unsigned int count = 0u;
    int ret;

    memset(g_demo_lvgl_photo_fs_app.items, 0, sizeof(g_demo_lvgl_photo_fs_app.items));
    ret = demo_lvgl_photo_fs_store_load_index(g_demo_lvgl_photo_fs_app.items,
                                              DEMO_LVGL_PHOTO_FS_MAX_ITEMS,
                                              &count);
    if (ret != 0) {
        g_demo_lvgl_photo_fs_app.item_count = 0u;
        return ret;
    }

    g_demo_lvgl_photo_fs_app.item_count = count;
    if ((count == 0u) || (g_demo_lvgl_photo_fs_app.current_index < 0) ||
        ((unsigned int)g_demo_lvgl_photo_fs_app.current_index >= count)) {
        g_demo_lvgl_photo_fs_app.current_index = 0;
    }

    return 0;
}

static void demo_lvgl_photo_fs_screen_event_cb(lv_event_t *e)
{
    lv_obj_t *screen;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    screen = lv_event_get_target(e);
    if (screen != g_demo_lvgl_photo_fs_app.active_screen) {
        return;
    }

    g_demo_lvgl_photo_fs_app.active_screen = NULL;
    g_demo_lvgl_photo_fs_app.title_label = NULL;
    g_demo_lvgl_photo_fs_app.status_label = NULL;
    g_demo_lvgl_photo_fs_app.hint_label = NULL;
    g_demo_lvgl_photo_fs_app.page_active = false;
}

static void demo_lvgl_photo_fs_reseed_task(void *arg)
{
    demo_lvgl_photo_fs_seed_result_t result;
    int ret;

    (void)arg;
    memset(&result, 0, sizeof(result));
    ret = demo_lvgl_photo_fs_seed_run(true, &result);

    liot_rtos_enter_critical();
    g_demo_lvgl_photo_fs_app.last_seed_ret = ret;
    g_demo_lvgl_photo_fs_app.last_seed_result = result;
    g_demo_lvgl_photo_fs_app.refresh_pending = true;
    g_demo_lvgl_photo_fs_app.reseed_task = NULL;
    liot_rtos_exit_critical();

    liot_rtos_task_delete(NULL);
}

static void demo_lvgl_photo_fs_open_at(int idx, lv_scr_load_anim_t anim);

static void demo_lvgl_photo_fs_poll_timer_cb(lv_timer_t *timer)
{
    bool refresh_pending;
    int ret;

    (void)timer;

    if (!g_demo_lvgl_photo_fs_app.page_active) {
        return;
    }

    liot_rtos_enter_critical();
    refresh_pending = g_demo_lvgl_photo_fs_app.refresh_pending;
    if (refresh_pending) {
        g_demo_lvgl_photo_fs_app.refresh_pending = false;
        g_demo_lvgl_photo_fs_app.seed_busy = false;
    }
    liot_rtos_exit_critical();

    if (!refresh_pending) {
        return;
    }

    ret = demo_lvgl_photo_fs_load_items();
    if (ret != 0) {
        g_demo_lvgl_photo_fs_app.item_count = 0u;
    }
    demo_lvgl_photo_fs_open_at(0, LV_SCR_LOAD_ANIM_FADE_ON);
}

static void demo_lvgl_photo_fs_poll_timer_ensure(void)
{
    if (g_demo_lvgl_photo_fs_app.poll_timer == NULL) {
        g_demo_lvgl_photo_fs_app.poll_timer =
            lv_timer_create(demo_lvgl_photo_fs_poll_timer_cb, 200u, NULL);
    }
}

static void demo_lvgl_photo_fs_start_reseed(void)
{
    LiotOSStatus_t ret;

    if (g_demo_lvgl_photo_fs_app.seed_busy) {
        return;
    }

    g_demo_lvgl_photo_fs_app.seed_busy = true;
    demo_lvgl_photo_fs_update_active_labels();

    ret = liot_rtos_task_create(&g_demo_lvgl_photo_fs_app.reseed_task,
                                DEMO_LVGL_PHOTO_FS_RESEED_STACK,
                                LIOT_APP_TASK_PRIORITY,
                                "demo_lvgl_photo_fs_seed",
                                demo_lvgl_photo_fs_reseed_task,
                                NULL);
    if (ret != 0) {
        g_demo_lvgl_photo_fs_app.seed_busy = false;
        g_demo_lvgl_photo_fs_app.last_seed_ret = (int)ret;
        demo_lvgl_photo_fs_update_active_labels();
    }
}

static void demo_lvgl_photo_fs_interaction_event_cb(lv_event_t *event)
{
    lv_event_code_t code;
    lv_indev_t *indev;
    lv_obj_t *target;
    int next_idx;

    code = lv_event_get_code(event);
    switch (code) {
    case LV_EVENT_CLICKED:
        if ((g_demo_lvgl_photo_fs_app.long_press_tick != 0u) &&
            (lv_tick_elaps(g_demo_lvgl_photo_fs_app.long_press_tick) < 1000u)) {
            g_demo_lvgl_photo_fs_app.long_press_tick = 0u;
            return;
        }
        if (g_demo_lvgl_photo_fs_app.seed_busy || (g_demo_lvgl_photo_fs_app.item_count == 0u)) {
            return;
        }
        next_idx = g_demo_lvgl_photo_fs_app.current_index + 1;
        if ((unsigned int)next_idx >= g_demo_lvgl_photo_fs_app.item_count) {
            next_idx = 0;
        }
        demo_lvgl_photo_fs_open_at(next_idx, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        break;
    case LV_EVENT_LONG_PRESSED:
        indev = lv_indev_get_act();
        target = lv_event_get_target(event);
        g_demo_lvgl_photo_fs_app.long_press_tick = lv_tick_get();
        if (indev != NULL) {
            lv_indev_reset(indev, target);
        } else {
            lv_indev_reset(NULL, target);
        }
        lv_event_stop_bubbling(event);
        lv_event_stop_processing(event);
        demo_lvgl_photo_fs_start_reseed();
        break;
    case LV_EVENT_GESTURE:
        if (g_demo_lvgl_photo_fs_app.seed_busy || (g_demo_lvgl_photo_fs_app.item_count == 0u)) {
            return;
        }
        indev = lv_indev_get_act();
        if (indev == NULL) {
            return;
        }
        switch (lv_indev_get_gesture_dir(indev)) {
        case LV_DIR_LEFT:
            next_idx = g_demo_lvgl_photo_fs_app.current_index + 1;
            if ((unsigned int)next_idx >= g_demo_lvgl_photo_fs_app.item_count) {
                next_idx = 0;
            }
            demo_lvgl_photo_fs_open_at(next_idx, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            break;
        case LV_DIR_RIGHT:
            next_idx = g_demo_lvgl_photo_fs_app.current_index - 1;
            if (next_idx < 0) {
                next_idx = (int)g_demo_lvgl_photo_fs_app.item_count - 1;
            }
            demo_lvgl_photo_fs_open_at(next_idx, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        default:
            break;
        }
        break;
    case LV_EVENT_RELEASED:
        if ((g_demo_lvgl_photo_fs_app.long_press_tick != 0u) &&
            (lv_tick_elaps(g_demo_lvgl_photo_fs_app.long_press_tick) >= 1000u)) {
            g_demo_lvgl_photo_fs_app.long_press_tick = 0u;
        }
        break;
    default:
        break;
    }
}

static lv_obj_t *demo_lvgl_photo_fs_setup_scr(int idx)
{
    lv_obj_t *scr;
    lv_obj_t *interaction;
    lv_obj_t *label;
    char text[128];
    const demo_lvgl_photo_fs_item_t *item = NULL;
    int ret;

    scr = lv_obj_create(NULL);
    if (scr == NULL) {
        return NULL;
    }

    lv_obj_set_size(scr, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, demo_lvgl_photo_fs_screen_event_cb, LV_EVENT_DELETE, NULL);

    g_demo_lvgl_photo_fs_app.active_screen = scr;
    g_demo_lvgl_photo_fs_app.page_active = true;

    g_demo_lvgl_photo_fs_app.title_label = lv_label_create(scr);
    if (g_demo_lvgl_photo_fs_app.title_label != NULL) {
        lv_obj_set_style_text_color(g_demo_lvgl_photo_fs_app.title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(g_demo_lvgl_photo_fs_app.title_label, &lv_font_montserrat_16, 0);
        lv_obj_align(g_demo_lvgl_photo_fs_app.title_label, LV_ALIGN_TOP_LEFT, 8, 8);
    }

    g_demo_lvgl_photo_fs_app.status_label = lv_label_create(scr);
    if (g_demo_lvgl_photo_fs_app.status_label != NULL) {
        lv_obj_set_style_text_color(g_demo_lvgl_photo_fs_app.status_label, lv_color_hex(0x7FDBFF), 0);
        lv_obj_set_style_text_font(g_demo_lvgl_photo_fs_app.status_label, &lv_font_montserrat_14, 0);
        lv_obj_align(g_demo_lvgl_photo_fs_app.status_label, LV_ALIGN_TOP_LEFT, 8, 30);
    }

    g_demo_lvgl_photo_fs_app.hint_label = lv_label_create(scr);
    if (g_demo_lvgl_photo_fs_app.hint_label != NULL) {
        lv_obj_set_style_text_color(g_demo_lvgl_photo_fs_app.hint_label, lv_color_hex(0xDADADA), 0);
        lv_obj_set_style_text_font(g_demo_lvgl_photo_fs_app.hint_label, &lv_font_montserrat_12, 0);
        lv_obj_align(g_demo_lvgl_photo_fs_app.hint_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
    demo_lvgl_photo_fs_update_active_labels();

    if (g_demo_lvgl_photo_fs_app.item_count == 0u) {
        label = lv_label_create(scr);
        if (label != NULL) {
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            (void)snprintf(text, sizeof(text), "No media in %s", DEMO_LVGL_PHOTO_FS_ROOT_DIR);
            lv_label_set_text(label, text);
            lv_obj_center(label);
        }
    } else {
        if ((idx < 0) || ((unsigned int)idx >= g_demo_lvgl_photo_fs_app.item_count)) {
            idx = 0;
        }
        g_demo_lvgl_photo_fs_app.current_index = idx;
        item = &g_demo_lvgl_photo_fs_app.items[idx];
        demo_lvgl_photo_fs_update_active_labels();

        ret = demo_lvgl_photo_fs_media_attach(scr, item);
        if (ret != 0) {
            label = lv_label_create(scr);
            if (label != NULL) {
                lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0xFF6B6B), 0);
                (void)snprintf(text,
                               sizeof(text),
                               "Load failed\nid=%s\nformat=%s\nret=%d",
                               item->id,
                               demo_lvgl_photo_fs_format_name(item->format),
                               ret);
                lv_label_set_text(label, text);
                lv_obj_center(label);
            }
        }
    }

    interaction = lv_obj_create(scr);
    if (interaction == NULL) {
        lv_obj_del(scr);
        return NULL;
    }
    lv_obj_remove_style_all(interaction);
    lv_obj_set_size(interaction, DEMO_LVGL_PHOTO_FS_IMG_W, DEMO_LVGL_PHOTO_FS_IMG_H);
    lv_obj_center(interaction);
    lv_obj_clear_flag(interaction, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(interaction,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                        LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(interaction, demo_lvgl_photo_fs_interaction_event_cb, LV_EVENT_ALL, NULL);

    return scr;
}

static void demo_lvgl_photo_fs_open_at(int idx, lv_scr_load_anim_t anim)
{
    lv_obj_t *scr;

    scr = demo_lvgl_photo_fs_setup_scr(idx);
    if (scr == NULL) {
        liot_trace("%s setup screen failed idx=%d", DEMO_LVGL_PHOTO_FS_LOG_PREFIX, idx);
        return;
    }

    lv_scr_load_anim(scr, anim, 200, 0, true);
}

static void demo_lvgl_photo_fs_page_enter(void)
{
    demo_lvgl_photo_fs_poll_timer_ensure();
    demo_lvgl_photo_fs_open_at(g_demo_lvgl_photo_fs_app.current_index, LV_SCR_LOAD_ANIM_FADE_ON);
}

static void demo_lvgl_photo_fs_page_enter_async_cb(void *arg)
{
    (void)arg;
    demo_lvgl_photo_fs_page_enter();
}

void liot_lvgl_photo_fs_demo_thread(void *argv)
{
    int ret;

    (void)argv;

    memset(&g_demo_lvgl_photo_fs_app, 0, sizeof(g_demo_lvgl_photo_fs_app));
    liot_rtos_task_sleep_ms(DEMO_LVGL_PHOTO_FS_BOOT_DELAY_MS);
    demo_lvgl_photo_fs_boot_diag_log_config(DEMO_LVGL_PHOTO_FS_LOG_PREFIX);

    ret = demo_lvgl_photo_fs_seed_run(DEMO_LVGL_PHOTO_FS_FORCE_RESEED != 0,
                                      &g_demo_lvgl_photo_fs_app.last_seed_result);
    g_demo_lvgl_photo_fs_app.last_seed_ret = ret;
    ret = demo_lvgl_photo_fs_load_items();
    if (ret != 0) {
        g_demo_lvgl_photo_fs_app.item_count = 0u;
    }

    liot_trace("%s start items=%u seed_ret=%d written=%u skipped=%u failed=%u",
               DEMO_LVGL_PHOTO_FS_LOG_PREFIX,
               g_demo_lvgl_photo_fs_app.item_count,
               g_demo_lvgl_photo_fs_app.last_seed_ret,
               (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.written,
               (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.skipped,
               (unsigned int)g_demo_lvgl_photo_fs_app.last_seed_result.failed);

    lvgl_init();
    lv_async_call(demo_lvgl_photo_fs_page_enter_async_cb, NULL);

    while (1) {
        liot_rtos_task_sleep_s(10u);
    }
}
