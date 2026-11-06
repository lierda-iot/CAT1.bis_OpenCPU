/* Real settings widgets, font and LVGL events; fake only the backend/display. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ui_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static lv_color_t pixels[320 * 40];
static bool pointer_down, reject_action;
static int pointer_x, pointer_y;
static unsigned action_count;
static tirtc_ui_action_type_t last_type;
static int32_t last_value;
static uint8_t saved_volume = 8, saved_mic = 10;
static bool saved_speaker = true, saved_microphone = true;

void tirtc_ui_platform_lock(void) {}
void tirtc_ui_platform_unlock(void) {}
void tirtc_port_set_brightness(uint8_t value) { (void)value; }

static int backend(const tirtc_ui_action_t *action, void *context)
{
    (void)context;
    ++action_count; last_type = action->type; last_value = action->value;
    if (reject_action) return -3;
    switch (action->type) {
    case TIRTC_ACTION_SET_MIC_GAIN: saved_mic = (uint8_t)action->value; break;
    case TIRTC_ACTION_SET_MIC_ENABLED: saved_microphone = action->value != 0; break;
    case TIRTC_ACTION_SET_SPEAKER_ENABLED: saved_speaker = action->value != 0; break;
    case TIRTC_ACTION_SET_SPEAKER_VOLUME: saved_volume = (uint8_t)action->value; break;
    default: return 0;
    }
    /* Mirror the existing preferences -> UI publication after acceptance. */
    assert(!tirtc_ui_publish_audio_settings(saved_volume, saved_mic,
                                           saved_speaker, saved_microphone));
    return 0;
}

static void flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *data)
{
    (void)area; (void)data;
    tirtc_ui_video_flush_done(true, lv_disp_flush_is_last(display));
    lv_disp_flush_ready(display);
}

static void read_pointer(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    data->point.x = pointer_x; data->point.y = pointer_y;
    data->state = pointer_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void pump(unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i += 5) {
        lv_tick_inc(5); tirtc_ui_process(); lv_timer_handler();
    }
}

static lv_obj_t *label(lv_obj_t *object, const char *text)
{
    if (lv_obj_check_type(object, &lv_label_class) &&
        (!text || !strcmp(lv_label_get_text(object), text))) return object;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); ++i) {
        lv_obj_t *found = label(lv_obj_get_child(object, i), text);
        if (found) return found;
    }
    return NULL;
}

static lv_obj_t *control(const char *title, const lv_obj_class_t *type)
{
    lv_obj_t *caption = label(ui_screen, title); assert(caption);
    lv_obj_t *row = lv_obj_get_parent(caption);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(row); ++i) {
        lv_obj_t *child = lv_obj_get_child(row, i);
        if (lv_obj_check_type(child, type)) return child;
    }
    assert(!"missing settings control"); return NULL;
}

static void click_at(int x, int y)
{
    assert(x >= 0 && x < 320 && y >= 0 && y < 240);
    pointer_x = x; pointer_y = y;
    pointer_down = true; pump(80); pointer_down = false; pump(80);
}

static void choose_by_pointer(lv_obj_t *choice, unsigned selected)
{
    lv_obj_scroll_to_view(choice, LV_ANIM_OFF); pump(60);
    lv_area_t bounds; lv_obj_get_coords(choice, &bounds);
    click_at((bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2);
    assert(lv_dropdown_is_open(choice));
    lv_obj_t *list = lv_dropdown_get_list(choice);
    lv_obj_t *options = label(list, NULL); assert(options);
    const lv_font_t *font = lv_obj_get_style_text_font(options, LV_PART_MAIN);
    int line = lv_font_get_line_height(font) + lv_obj_get_style_text_line_space(options, LV_PART_MAIN);
    lv_obj_get_coords(list, &bounds);
    lv_area_t text; lv_obj_get_coords(options, &text);
    int y = text.y1 + (int)selected * line + lv_font_get_line_height(font) / 2;
    if (y <= bounds.y1 || y >= bounds.y2) {
        lv_obj_scroll_by(list, 0, (bounds.y1 + bounds.y2) / 2 - y, LV_ANIM_OFF);
        pump(40); lv_obj_get_coords(options, &text);
        y = text.y1 + (int)selected * line + lv_font_get_line_height(font) / 2;
    }
    click_at((bounds.x1 + bounds.x2) / 2, y);
    assert(!lv_dropdown_is_open(choice));
}

static void choose(lv_obj_t *choice, unsigned selected)
{
    lv_dropdown_set_selected(choice, (uint16_t)selected);
    assert(lv_event_send(choice, LV_EVENT_VALUE_CHANGED, NULL) == LV_RES_OK);
}

int main(void)
{
    lv_init();
    lv_disp_draw_buf_t buffer; lv_disp_draw_buf_init(&buffer, pixels, NULL, 320 * 40);
    lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res = 320; display.ver_res = 240; display.draw_buf = &buffer; display.flush_cb = flush;
    assert(lv_disp_drv_register(&display));
    lv_indev_drv_t pointer; lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER; pointer.read_cb = read_pointer; assert(lv_indev_drv_register(&pointer));
    tirtc_ui_set_backend(backend, NULL); tirtc_ui_init(); pump(100);
    assert(!tirtc_ui_publish_audio_settings(8, 10, true, true));
    tirtc_ui_request_page(TIRTC_PAGE_SETTINGS); pump(120); assert(ui_page == TIRTC_PAGE_SETTINGS);
    const uint32_t characters[] = {0x9ea6, 0x514b, 0x98ce, 0x97f3, 0x91cf, 0x9759, 'L', '0', '7'};
    for (unsigned i = 0; i < sizeof(characters) / sizeof(characters[0]); ++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(&tirtc_font_14, &glyph, characters[i], 0));
        assert(!glyph.is_placeholder);
    }
    lv_obj_t *gain = control("麦克风音量", &lv_dropdown_class);
    lv_obj_t *microphone = control("麦克风", &lv_switch_class);
    lv_obj_t *speaker = control("扬声器", &lv_switch_class);
    lv_obj_t *sleep = control("自动息屏", &lv_dropdown_class);
    lv_obj_t *ack = control("回应声", &lv_dropdown_class);
    assert(lv_dropdown_get_option_cnt(gain) == 11 && lv_dropdown_get_selected(gain) == 10);
    unsigned actions = action_count;
    choose_by_pointer(gain, 7);
    assert(action_count == actions + 1 && last_type == TIRTC_ACTION_SET_MIC_GAIN && last_value == 7);
    assert(ui_state.mic_gain == 7 && saved_mic == 7 && lv_dropdown_get_selected(gain) == 7);
    assert(ui_state.mic_enabled && ui_state.volume == 8);
    /* Background state must not replace the selection while its list is open. */
    lv_dropdown_open(gain); lv_dropdown_set_selected(gain, 5);
    assert(!tirtc_ui_publish_audio_settings(9, 7, true, true)); pump(50);
    assert(lv_dropdown_is_open(gain) && lv_dropdown_get_selected(gain) == 5);
    lv_dropdown_close(gain); ui_pages_process(true, lv_tick_get());
    assert(lv_dropdown_get_selected(gain) == 7);
    reject_action = true; choose(gain, 5); reject_action = false;
    assert(ui_state.mic_gain == 7 && lv_dropdown_get_selected(gain) == 7 && saved_mic == 7);
    choose(gain, 0); assert(last_value == 0 && ui_state.mic_gain == 0 && ui_state.mic_enabled);
    choose(gain, 10); assert(last_value == 10 && ui_state.mic_gain == 10);
    choose(gain, 7);
    lv_obj_clear_state(microphone, LV_STATE_CHECKED); lv_event_send(microphone, LV_EVENT_VALUE_CHANGED, NULL);
    assert(last_type == TIRTC_ACTION_SET_MIC_ENABLED && last_value == 0 && !ui_state.mic_enabled && ui_state.mic_gain == 7);
    lv_obj_add_state(microphone, LV_STATE_CHECKED); lv_event_send(microphone, LV_EVENT_VALUE_CHANGED, NULL);
    assert(ui_state.mic_enabled && ui_state.mic_gain == 7);
    lv_obj_clear_state(speaker, LV_STATE_CHECKED); lv_event_send(speaker, LV_EVENT_VALUE_CHANGED, NULL);
    assert(last_type == TIRTC_ACTION_SET_SPEAKER_ENABLED && !ui_state.speaker_enabled && ui_state.mic_gain == 7);
    choose(sleep, 3); assert(last_type == TIRTC_ACTION_SET_SLEEP_MINUTES && last_value == 30 && ui_state.sleep_minutes == 30);
    choose(ack, 1); assert(last_type == TIRTC_ACTION_SET_ACK_VOICE && last_value == 1 && ui_state.acknowledgement_male);
    lv_obj_t *plus = lv_obj_get_parent(label(ui_screen, LV_SYMBOL_PLUS));
    unsigned volume = ui_state.volume; lv_event_send(plus, LV_EVENT_CLICKED, NULL);
    assert(last_type == TIRTC_ACTION_SET_SPEAKER_VOLUME && ui_state.volume == volume + 1 && ui_state.mic_gain == 7);
    tirtc_ui_request_page(TIRTC_PAGE_MENU); pump(100);
    tirtc_ui_request_page(TIRTC_PAGE_SETTINGS); pump(100);
    assert(lv_dropdown_get_selected(control("麦克风音量", &lv_dropdown_class)) == 7);
    puts("PASS real LVGL settings: pointer selects L7, existing glyphs, accepted/rejected state, open-list refresh, L0/L10, mic/speaker switches, sleep/voice/volume and page recreation.");
    return 0;
}
