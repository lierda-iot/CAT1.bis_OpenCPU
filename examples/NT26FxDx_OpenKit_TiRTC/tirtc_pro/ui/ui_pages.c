/* Current starter_product_s3_ui / starter_product_room presentation.
 * Business actions are queued by the host; accepting a request is not success.
 */
#include "ui_internal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define ROOM_PAGE_SIZE 3U
#define ROOM_KEY_BACKSPACE 10U
#define ROOM_KEY_CONFIRM 11U

/* Object references are cleared before the core deletes the previous screen.
 * Drafts and navigation/session bookkeeping deliberately live separately. */
static struct {
    lv_obj_t *volume, *down, *up, *speaker, *microphone, *mic_gain, *sleep, *ack;
    lv_obj_t *network, *refresh, *reconnect;
    lv_obj_t *diagnostics, *diagnostic_panel, *tabs[3];
    lv_obj_t *code, *status, *count, *ptt, *previous, *next, *leave, *confirm;
    lv_obj_t *names[ROOM_PAGE_SIZE], *self[ROOM_PAGE_SIZE], *speaking[ROOM_PAGE_SIZE];
    lv_obj_t *code_input, *password_input, *active_input, *keyboard;
    lv_obj_t *create_button, *join_button;
    lv_obj_t *remote_status, *remote_details, *remote_message;
    lv_obj_t *remote_mic, *remote_speaker, *remote_camera, *remote_end;
} view;
static struct {
    bool create, layout_assigned, opened, held, submitting, room_ticked, diagnostics_ticked;
    bool ptt_cancelled_until_release;
    uint8_t diagnostic_tab, styled_diagnostic_tab;
    unsigned offset;
    uint32_t held_generation, held_at, renewed_at, submit_at, confirm_generation, list_generation;
    uint32_t room_refreshed_at, diagnostics_refreshed_at;
    char held_code[8], confirm_code[8], list_code[8], code[8], password[5];
    bool remote_submitting;
    uint32_t remote_generation, remote_revision, remote_submit_at;
} local;

static void settings_refresh(void);
static void network_refresh(void);
static void diagnostics_refresh(void);
static void room_refresh(uint32_t now);
static void room_event(lv_event_t *event);
static void remote_refresh(uint32_t now);

static void text_if_changed(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void append_text(char *text, size_t capacity, const char *format, ...)
{
    size_t used = strlen(text);
    if (used + 1 >= capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(text + used, capacity - used, format, args);
    va_end(args);
}

static void button_text(lv_obj_t *button, const char *text)
{
    if (button) text_if_changed(lv_obj_get_child(button, 0), text);
}

static void visible(lv_obj_t *obj, bool show)
{
    if (!obj) return;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == !show) return;
    if (show) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *scroller(int y, int height)
{
    lv_obj_t *panel = ui_panel(ui_screen, 8, y, 304, height, true);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(panel, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_MUTED), LV_PART_SCROLLBAR);
    return panel;
}

static lv_obj_t *icon(lv_obj_t *parent, const char *symbol, int x, int y,
                      int width, lv_event_cb_t cb, uintptr_t action)
{
    lv_obj_t *button = ui_button(parent, symbol, x, y, width, 40, cb, action);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(button, 0), &lv_font_montserrat_14, 0);
    return button;
}

static void settings_event(lv_event_t *event)
{
    tirtc_ui_action_type_t type = (tirtc_ui_action_type_t)(uintptr_t)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    int32_t value;
    static const uint16_t minutes[] = {1, 5, 10, 30, 0};
    ui_note_interaction();
    if (type == TIRTC_ACTION_SET_SPEAKER_VOLUME) {
        value = (int32_t)ui_state.volume + (target == view.up ? 1 : -1);
        if (value < 0 || value > 10) return;
    } else if (type == TIRTC_ACTION_SET_MIC_GAIN) {
        uint16_t selected = lv_dropdown_get_selected(target);
        if (selected > 10) return;
        value = selected;
    } else if (type == TIRTC_ACTION_SET_SLEEP_MINUTES) {
        uint16_t selected = lv_dropdown_get_selected(target);
        if (selected >= sizeof(minutes) / sizeof(minutes[0])) return;
        value = minutes[selected];
    } else if (type == TIRTC_ACTION_SET_ACK_VOICE) {
        uint16_t selected = lv_dropdown_get_selected(target);
        if (selected > 1) return;
        value = selected;
    } else value = lv_obj_has_state(target, LV_STATE_CHECKED) ? 1 : 0;
    if (ui_action(type, 0, value, NULL, NULL) == 0) {
        switch (type) {
        case TIRTC_ACTION_SET_SPEAKER_VOLUME: ui_state.volume = (uint8_t)value; break;
        case TIRTC_ACTION_SET_SPEAKER_ENABLED: ui_state.speaker_enabled = value != 0; break;
        case TIRTC_ACTION_SET_MIC_ENABLED: ui_state.mic_enabled = value != 0; break;
        case TIRTC_ACTION_SET_MIC_GAIN: ui_state.mic_gain = (uint8_t)value; break;
        case TIRTC_ACTION_SET_SLEEP_MINUTES: ui_state.sleep_minutes = (uint16_t)value; break;
        case TIRTC_ACTION_SET_ACK_VOICE: ui_state.acknowledgement_male = value != 0; break;
        default: break;
        }
    } else ui_notice("设置未受理，请重试");
    /* Programmatic state changes do not emit LV_EVENT_VALUE_CHANGED. */
    settings_refresh();
}

static lv_obj_t *settings_row(lv_obj_t *parent, const char *title, int y)
{
    lv_obj_t *row = ui_panel(parent, 0, y, 296, 48, false);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0x343A3F), 0);
    ui_label(row, title, 4, 14, 140, UI_TEXT);
    return row;
}

static lv_obj_t *settings_switch(lv_obj_t *row, tirtc_ui_action_type_t type)
{
    lv_obj_t *control = lv_switch_create(row);
    lv_obj_set_pos(control, 234, 10);
    lv_obj_set_size(control, 56, 28);
    lv_obj_set_ext_click_area(control, 8);
    lv_obj_set_style_bg_color(control, lv_color_hex(UI_ACCEPT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(control, settings_event, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)type);
    return control;
}

static lv_obj_t *settings_choice(lv_obj_t *row, const char *options, tirtc_ui_action_type_t type)
{
    lv_obj_t *choice = lv_dropdown_create(row);
    lv_obj_set_pos(choice, 156, 2);
    lv_obj_set_size(choice, 138, 44);
    lv_dropdown_set_options(choice, options);
    lv_dropdown_set_symbol(choice, NULL);
    lv_obj_set_style_text_font(choice, &tirtc_font_14, 0);
    lv_obj_set_style_text_color(choice, lv_color_hex(UI_TEXT), 0);
    lv_obj_set_style_bg_color(choice, lv_color_hex(UI_SURFACE), 0);
    lv_obj_set_style_bg_opa(choice, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(choice, 6, 0);
    lv_obj_t *list = lv_dropdown_get_list(choice);
    lv_obj_set_style_text_font(list, &tirtc_font_14, 0);
    lv_obj_set_style_text_color(list, lv_color_hex(UI_TEXT), 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(UI_SURFACE), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(list, 10, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, lv_color_hex(UI_ACCEPT), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_t *arrow = ui_label(choice, LV_SYMBOL_DOWN, 108, 0, 16, UI_MUTED);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(choice, settings_event, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)type);
    return choice;
}

static void settings_refresh(void)
{
    if (!view.volume) return;
    char text[40];
    snprintf(text, sizeof(text), ui_state.volume ? "音量  %u / 10" : "音量  %u / 10  静音", (unsigned)ui_state.volume);
    text_if_changed(view.volume, text);
    ui_enabled(view.down, ui_state.volume > 0);
    ui_enabled(view.up, ui_state.volume < 10);
    if (ui_state.speaker_enabled) lv_obj_add_state(view.speaker, LV_STATE_CHECKED);
    else lv_obj_clear_state(view.speaker, LV_STATE_CHECKED);
    if (ui_state.mic_enabled) lv_obj_add_state(view.microphone, LV_STATE_CHECKED);
    else lv_obj_clear_state(view.microphone, LV_STATE_CHECKED);
    uint16_t selection = 4;
    switch (ui_state.sleep_minutes) {
    case 1: selection = 0; break;
    case 5: selection = 1; break;
    case 10: selection = 2; break;
    case 30: selection = 3; break;
    default: break;
    }
    /* A background network snapshot must not move the open dropdown's cursor. */
    if (!lv_dropdown_is_open(view.mic_gain)) lv_dropdown_set_selected(view.mic_gain, ui_state.mic_gain);
    if (!lv_dropdown_is_open(view.sleep)) lv_dropdown_set_selected(view.sleep, selection);
    if (!lv_dropdown_is_open(view.ack)) lv_dropdown_set_selected(view.ack, ui_state.acknowledgement_male ? 1 : 0);
}

static void settings_render(void)
{
    ui_header("设置", TIRTC_PAGE_MENU);
    lv_obj_t *panel = scroller(46, 190);
    view.down = icon(panel, LV_SYMBOL_MINUS, 0, 0, 48, settings_event, TIRTC_ACTION_SET_SPEAKER_VOLUME);
    view.up = icon(panel, LV_SYMBOL_PLUS, 248, 0, 48, settings_event, TIRTC_ACTION_SET_SPEAKER_VOLUME);
    view.volume = ui_label(panel, "", 56, 13, 184, UI_TEXT);
    lv_obj_set_style_text_align(view.volume, LV_TEXT_ALIGN_CENTER, 0);
    view.speaker = settings_switch(settings_row(panel, "扬声器", 52), TIRTC_ACTION_SET_SPEAKER_ENABLED);
    view.microphone = settings_switch(settings_row(panel, "麦克风", 104), TIRTC_ACTION_SET_MIC_ENABLED);
    view.mic_gain = settings_choice(settings_row(panel, "麦克风音量", 156), "L0 静音\nL1\nL2\nL3\nL4\nL5\nL6\nL7\nL8\nL9\nL10", TIRTC_ACTION_SET_MIC_GAIN);
    view.sleep = settings_choice(settings_row(panel, "自动息屏", 208), "1 分钟\n5 分钟\n10 分钟\n30 分钟\n永不", TIRTC_ACTION_SET_SLEEP_MINUTES);
    view.ack = settings_choice(settings_row(panel, "回应声", 260), "女声\n男声", TIRTC_ACTION_SET_ACK_VOICE);
    settings_refresh();
}

static const char *sim_text(tirtc_ui_sim_state_t state)
{
    switch (state) {
    case TIRTC_SIM_READY: return "就绪";
    case TIRTC_SIM_ABSENT: return "未插入";
    case TIRTC_SIM_PIN_REQUIRED: return "需要 PIN";
    case TIRTC_SIM_PUK_REQUIRED: return "需要 PUK";
    case TIRTC_SIM_ERROR: return "异常";
    default: return "未知";
    }
}

static const char *registration_text(tirtc_ui_registration_t state)
{
    switch (state) {
    case TIRTC_REG_NOT_REGISTERED: return "未注册";
    case TIRTC_REG_SEARCHING: return "搜索中";
    case TIRTC_REG_HOME: return "已注册";
    case TIRTC_REG_ROAMING: return "漫游";
    case TIRTC_REG_DENIED: return "注册被拒绝";
    default: return "未知";
    }
}

static void network_event(lv_event_t *event)
{
    tirtc_ui_action_type_t type = (tirtc_ui_action_type_t)(uintptr_t)lv_event_get_user_data(event);
    ui_note_interaction();
    if (ui_action(type, 0, 0, NULL, NULL) < 0) ui_notice("网络请求未受理，请重试");
    /* The worker's next real snapshot owns all connection state. */
}

/* LTE metrics retain their source and units. RSRQ is stored in half dB;
 * boundary indices describe an open-ended range, not an exact measurement. */
static void radio_text(char *text, size_t capacity)
{
    if (!ui_state.network_status_valid) { ui_copy(text, capacity, "未知"); return; }
    if (!ui_state.signal_valid) ui_copy(text, capacity, "RSRP 未知");
    else if (ui_state.signal_dbm == -141) ui_copy(text, capacity, "RSRP < -140 dBm");
    else if (ui_state.signal_dbm == -44) ui_copy(text, capacity, "RSRP >= -44 dBm");
    else snprintf(text, capacity, "RSRP 约 %d dBm", (int)ui_state.signal_dbm);
    if (ui_state.signal_rsrq_valid) {
        int value = (int)ui_state.signal_rsrq_x2;
        if (value == -40) append_text(text, capacity, "  RSRQ < -19.5 dB");
        else if (value == -6) append_text(text, capacity, "  RSRQ >= -3 dB");
        else {
            unsigned magnitude = value < 0 ? (unsigned)-value : (unsigned)value;
            append_text(text, capacity, "  RSRQ %s%u.%u dB", value < 0 ? "-" : "",
                        magnitude / 2U, (magnitude % 2U) * 5U);
        }
    }
    if (ui_state.signal_snr_valid) append_text(text, capacity, "  SNR %d dB", (int)ui_state.signal_snr_db);
    if (ui_state.signal_rssi_valid) append_text(text, capacity, "  RSSI %d dBm", (int)ui_state.signal_rssi_dbm);
}

static void network_refresh(void)
{
    if (!view.network) return;
    char signal[160], text[1152];
    bool known = ui_state.network_status_valid;
    radio_text(signal, sizeof(signal));
    const char *data = !known ? "未获取" : ui_state.network_connected ? "已连接" :
                       ui_state.network_connecting ? "连接中" : "未连接";
    snprintf(text, sizeof(text),
        "移动网络  %s\nSIM 卡  %s\n网络注册  %s%s\n运营商  %s\n网络制式  %s\n信号强度  %s\nIP  %s\nAPN  %s",
        data, known ? sim_text(ui_state.sim_state) : "未知",
        known ? registration_text(ui_state.registration) : "未知",
        known && ui_state.network_roaming && ui_state.registration != TIRTC_REG_ROAMING ? "（漫游）" : "",
        known && ui_state.operator_name[0] ? ui_state.operator_name : "未知",
        known && ui_state.network_type[0] ? ui_state.network_type : "未知", signal,
        known && ui_state.network_connected && ui_state.ip_address[0] ? ui_state.ip_address : "未获取",
        known && ui_state.apn[0] ? ui_state.apn : "未获取");
    if (ui_system.modem_imei_valid && ui_system.modem_imei[0])
        append_text(text, sizeof(text), "\n模组 IMEI  %s", ui_system.modem_imei);
    if (ui_system.modem_version_valid && ui_system.modem_version[0])
        append_text(text, sizeof(text), "\n模组版本  %s", ui_system.modem_version);
    if (ui_state.device_id[0]) append_text(text, sizeof(text), "\n云设备 ID  %s", ui_state.device_id);
    if (ui_platform_live && ui_platform.known) {
        if (!ui_platform.enabled) append_text(text, sizeof(text), "\n平台服务  暂未启用");
        else {
            append_text(text, sizeof(text), "\n平台 API  %s\n消息服务  %s\nTiRTC  %s",
                ui_platform.api_ready ? "已连接" : "未连接",
                ui_platform.mqtt_connected ? "已连接" : "未连接",
                !ui_platform.rtc_enabled ? "暂未接入" : ui_platform.rtc_ready ? "已连接" : "未连接");
            if (ui_platform.message[0]) append_text(text, sizeof(text), "\n%s", ui_platform.message);
            if (ui_platform.error) append_text(text, sizeof(text), "\n平台错误  %ld", (long)ui_platform.error);
        }
    } else if (!ui_platform_live) {
        if (ui_state.rtc_connected) append_text(text, sizeof(text), "\nTiRTC  已连接");
        if (ui_state.mqtt_connected) append_text(text, sizeof(text), "\n消息服务  已连接");
    }
    if (ui_state.network_message[0]) append_text(text, sizeof(text), "\n%s", ui_state.network_message);
    text_if_changed(view.network, text);
    ui_enabled(view.reconnect, !ui_state.network_connecting);
    button_text(view.reconnect, ui_state.network_connecting ? "正在恢复" : "恢复连接");
}

static void network_render(void)
{
    if (ui_platform_setup_active()) (void)ui_label(ui_screen, "4G 联网", 16, 12, 288, UI_TEXT);
    else ui_header("网络信息", TIRTC_PAGE_MENU);
    view.network = ui_label(scroller(48, 132), "", 8, 0, 284, UI_TEXT);
    lv_obj_set_style_text_line_space(view.network, 5, 0);
    view.refresh = ui_button(ui_screen, "刷新", 8, 188, 146, 48, network_event, TIRTC_ACTION_NETWORK_REFRESH);
    view.reconnect = ui_button(ui_screen, "恢复连接", 162, 188, 150, 48, network_event, TIRTC_ACTION_NETWORK_RECONNECT);
    network_refresh();
}

static void diagnostic_event(lv_event_t *event)
{
    unsigned tab = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if (tab > 2) return;
    ui_note_interaction();
    if (local.diagnostic_tab == tab) return;
    local.diagnostic_tab = (uint8_t)tab;
    diagnostics_refresh();
    lv_obj_scroll_to_y(view.diagnostic_panel, 0, LV_ANIM_OFF);
}

/* KiB always means 1024 bytes. Integer rounding avoids printf floating-point
 * support and remains safe for the entire uint32_t byte range. */
static void kib_text(char *text, size_t size, uint32_t bytes)
{
    uint32_t tenths = (bytes / 1024U) * 10U + ((bytes % 1024U) * 10U + 512U) / 1024U;
    snprintf(text, size, "%lu.%lu", (unsigned long)(tenths / 10U), (unsigned long)(tenths % 10U));
}

static const char *storage_state_text(tirtc_ui_storage_state_t state)
{
    switch (state) {
    case TIRTC_STORAGE_INITIALIZING: return "正在初始化";
    case TIRTC_STORAGE_READY: return "已就绪";
    case TIRTC_STORAGE_UNRECOGNIZED: return "未识别，保留原数据";
    case TIRTC_STORAGE_UNSUPPORTED: return "芯片不支持";
    case TIRTC_STORAGE_ERROR: return "读取失败";
    case TIRTC_STORAGE_READ_ONLY: return "已挂载（只读）";
    default: return "未获取";
    }
}

/* Match the active starter_product diagnostics: one quiet scrolling label.
 * Only board-backed values and concise state/error lines are presented. */
static void resource_text(char *text, size_t capacity)
{
    const tirtc_ui_resources_t *r = &ui_resources;
    char first[24], second[24];
    text[0] = 0;
    append_text(text, capacity, "版本 %s", ui_state.version[0] ? ui_state.version : "UI 09 / Starter");
    if (r->uptime_valid) append_text(text, capacity, "\n运行 %lu s", (unsigned long)r->uptime_seconds);
    if (r->heap_valid) {
        kib_text(first, sizeof(first), r->heap_free_bytes);
        kib_text(second, sizeof(second), r->heap_total_bytes);
        append_text(text, capacity, "\nRAM 可用 %s / %s KiB", first, second);
        if (r->heap_min_valid) {
            kib_text(first, sizeof(first), r->heap_min_free_bytes);
            append_text(text, capacity, "\n最低 %s KiB", first);
        }
        if (r->heap_max_valid) {
            kib_text(first, sizeof(first), r->heap_max_free_block_bytes);
            append_text(text, capacity, "%s最大块 %s KiB", r->heap_min_valid ? "  " : "\n", first);
        }
    } else append_text(text, capacity, "\nRAM 等待采样");
    if (r->external_filesystem_valid) {
        kib_text(first, sizeof(first), r->external_filesystem_free_bytes);
        kib_text(second, sizeof(second), r->external_filesystem_total_bytes);
        append_text(text, capacity, "\n%s %s / %s KiB",
            r->external_state == TIRTC_STORAGE_READ_ONLY ? "外置只读 剩余" : "外置存储 剩余", first, second);
    } else {
        const char *status = storage_state_text(r->external_state);
        if (r->external_state == TIRTC_STORAGE_UNKNOWN) status = "等待检测";
        if (r->external_state == TIRTC_STORAGE_READY || r->external_state == TIRTC_STORAGE_READ_ONLY)
            status = r->external_error ? "读取失败" : "等待采样";
        append_text(text, capacity, "\n外置存储 %s", status);
        if (r->external_error) append_text(text, capacity, " (%ld)", (long)r->external_error);
    }
    if (r->flash_valid) {
        kib_text(first, sizeof(first), r->flash_total_bytes - r->flash_used_bytes);
        append_text(text, capacity, "\nAPP 程序区 剩余 %s KiB", first);
    }
    if (r->filesystem_valid) {
        kib_text(first, sizeof(first), r->filesystem_free_bytes);
        append_text(text, capacity, "\n内部文件 剩余 %s KiB", first);
    } else if (r->filesystem_error) append_text(text, capacity, "\n内部文件 读取失败 (%ld)", (long)r->filesystem_error);
    else if (r->filesystem_pending) append_text(text, capacity, "\n内部文件 正在读取");
    if (ui_state.network_status_valid) {
        append_text(text, capacity, "\n4G %s", ui_state.network_connected ? "在线" :
            ui_state.network_connecting ? "连接中" : "离线");
        char signal[160];
        radio_text(signal, sizeof(signal));
        append_text(text, capacity, "  %s", signal);
    } else append_text(text, capacity, "\n4G 检测中");
    if (ui_state.diagnostic_system[0]) append_text(text, capacity, "\n%s", ui_state.diagnostic_system);
}

static void diagnostics_refresh(void)
{
    if (!view.diagnostics) return;
    local.diagnostics_ticked = true;
    local.diagnostics_refreshed_at = lv_tick_get();
    const char *text;
    char generated[2048];
    if (local.diagnostic_tab == 0) {
        resource_text(generated, sizeof(generated));
        text = generated;
    } else if (local.diagnostic_tab == 1) {
        text = ui_state.diagnostic_audio[0] ? ui_state.diagnostic_audio :
            ui_system_live ? ui_system.diagnostic_audio : "";
        if (!text[0]) text = "音频业务尚未接入";
    } else {
        if (ui_state.diagnostic_events[0] && ui_system_live && ui_system.diagnostic_events[0]) {
            snprintf(generated, sizeof(generated), "系统事件\n%s\n\n业务事件\n%s",
                ui_system.diagnostic_events, ui_state.diagnostic_events);
            text = generated;
        } else text = ui_state.diagnostic_events[0] ? ui_state.diagnostic_events :
            ui_system_live ? ui_system.diagnostic_events : "";
        if (!text[0]) text = "最近事件（最多 8 条）\n暂无事件";
    }
    text_if_changed(view.diagnostics, text);
    if (local.styled_diagnostic_tab != local.diagnostic_tab) {
        for (unsigned i = 0; i < 3; ++i)
            lv_obj_set_style_bg_color(view.tabs[i], lv_color_hex(i == local.diagnostic_tab ? UI_ACCEPT : UI_BUTTON), 0);
        local.styled_diagnostic_tab = local.diagnostic_tab;
    }
}

static void diagnostics_render(void)
{
    ui_header("运行状态", TIRTC_PAGE_MENU);
    static const char *titles[] = {"资源", "音频", "事件"};
    for (unsigned i = 0; i < 3; ++i) view.tabs[i] = ui_button(ui_screen, titles[i], 8 + i * 104, 48, 96, 44, diagnostic_event, i);
    view.diagnostic_panel = scroller(100, 136);
    view.diagnostics = ui_label(view.diagnostic_panel, "", 4, 0, 284, UI_TEXT);
    lv_obj_set_style_text_line_space(view.diagnostics, 4, 0);
    local.styled_diagnostic_tab = 255;
    diagnostics_refresh();
}

enum { ROOM_BACK = 1, ROOM_CREATE, ROOM_JOIN, ROOM_REFRESH, ROOM_LEAVE,
       ROOM_LEAVE_YES, ROOM_CANCEL, ROOM_PREVIOUS, ROOM_NEXT };

/* Every PTT intent is addressed to the session captured at press time. A
 * release of an old session must never silence a newer connection. */
static int room_intent(tirtc_ui_action_type_t type, int32_t value,
                       const char *code, uint32_t generation)
{
    char extra[16];
    snprintf(extra, sizeof(extra), "%lu", (unsigned long)generation);
    return ui_action(type, 0, value, code, extra);
}

static void room_release(void)
{
    if (!local.held) return;
    uint32_t generation = local.held_generation;
    char code[8];
    ui_copy(code, sizeof(code), local.held_code);
    local.held = false;
    local.held_generation = 0;
    local.held_code[0] = '\0';
    if (room_intent(TIRTC_ACTION_ROOM_PTT, 0, code, generation) < 0)
        ui_notice("结束讲话请求未受理");
}

static bool room_can_talk(void)
{
    return ui_page == TIRTC_PAGE_ROOM && local.opened && ui_state.room.known &&
        ui_state.room.assigned && ui_state.room.connected && !ui_state.room.busy &&
        ui_state.room.generation != 0 && ui_state.mic_enabled && ui_state.mic_gain != 0 && !view.confirm &&
        !local.ptt_cancelled_until_release;
}

static void ptt_refresh(void)
{
    if (!view.ptt) return;
    bool ready = room_can_talk();
    ui_enabled(view.ptt, ready);
    button_text(view.ptt, local.held ? "松开结束讲话" : !ui_state.mic_enabled ?
        "麦克风已关闭" : !ui_state.mic_gain ? "麦克风静音（L0）" : ready ? "按下讲话" : "等待房间连接");
    lv_obj_set_style_bg_color(view.ptt, lv_color_hex(local.held ? UI_ACCEPT : UI_BUTTON), 0);
    lv_obj_set_style_bg_color(view.ptt, lv_color_hex(local.held ? UI_ACCEPT : 0x465057), LV_STATE_PRESSED);
}

void ui_pages_cancel_ptt(void)
{
    /* The core may defer replacing ui_state while a pointer is down. A newer
     * unsafe snapshot still revokes this press immediately. Latch cancellation
     * so moving out and back cannot restart PTT against the deferred old state. */
    if (local.ptt_cancelled_until_release) return;
    local.ptt_cancelled_until_release = true;
    room_release();
    ptt_refresh();
}

static void ptt_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    uint32_t now = lv_tick_get();
    if (code == LV_EVENT_DELETE || code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        room_release();
        if (code == LV_EVENT_RELEASED) local.ptt_cancelled_until_release = false;
        if (code == LV_EVENT_DELETE) { view.ptt = NULL; return; }
    } else if (code == LV_EVENT_PRESSED) {
        ui_note_interaction();
        if (!room_can_talk()) return;
        room_release();
        uint32_t generation = ui_state.room.generation;
        char room_code[8];
        ui_copy(room_code, sizeof(room_code), ui_state.room.code);
        if (room_intent(TIRTC_ACTION_ROOM_PTT, 1, room_code, generation) == 0) {
            local.held = true;
            local.held_generation = generation;
            ui_copy(local.held_code, sizeof(local.held_code), room_code);
            local.held_at = local.renewed_at = now;
        } else ui_notice("讲话请求未受理，请重试");
    } else if (code == LV_EVENT_PRESSING && local.held) {
        ui_note_interaction();
        if (!room_can_talk() || ui_state.room.generation != local.held_generation ||
            strcmp(ui_state.room.code, local.held_code) != 0 || (uint32_t)(now - local.held_at) > 750U) {
            room_release();
        } else if ((uint32_t)(now - local.renewed_at) >= 200U) {
            if (room_intent(TIRTC_ACTION_ROOM_PTT, 1, local.held_code, local.held_generation) < 0) {
                room_release();
                ui_notice("讲话已停止，请重试");
            }
            local.renewed_at = now;
        }
        local.held_at = now;
    } else return;
    ptt_refresh();
}

static void room_header(const char *title)
{
    icon(ui_screen, LV_SYMBOL_LEFT, 4, 0, 40, room_event, ROOM_BACK);
    lv_obj_t *label = ui_label(ui_screen, title, 56, 12, 124, UI_TEXT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_height(label, 20);
}

static void close_confirm(void)
{
    if (view.confirm) {
        lv_obj_t *obj = view.confirm;
        view.confirm = NULL;
        lv_obj_del(obj);
    }
    local.confirm_generation = 0;
    local.confirm_code[0] = '\0';
}

static void room_event(lv_event_t *event)
{
    unsigned action = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    ui_note_interaction();
    room_release();
    if (action == ROOM_BACK) {
        ui_navigate(ui_page == TIRTC_PAGE_ROOM_FORM ? TIRTC_PAGE_ROOM : TIRTC_PAGE_MENU);
    } else if (action == ROOM_CREATE || action == ROOM_JOIN) {
        if (ui_state.room.assigned || ui_state.room.busy) return;
        local.create = action == ROOM_CREATE;
        local.code[0] = local.password[0] = '\0';
        local.submitting = false;
        ui_navigate(TIRTC_PAGE_ROOM_FORM);
    } else if (action == ROOM_REFRESH) {
        if (ui_action(TIRTC_ACTION_ROOM_REFRESH, 0, 0, NULL, NULL) < 0) ui_notice("刷新未提交，请重试");
    } else if (action == ROOM_PREVIOUS || action == ROOM_NEXT) {
        if (action == ROOM_PREVIOUS) local.offset = local.offset >= ROOM_PAGE_SIZE ? local.offset - ROOM_PAGE_SIZE : 0;
        else if (local.offset + ROOM_PAGE_SIZE < ui_state.room.member_count &&
                 local.offset + ROOM_PAGE_SIZE < TIRTC_UI_ROOM_MEMBER_MAX) local.offset += ROOM_PAGE_SIZE;
    } else if (action == ROOM_LEAVE) {
        if (view.confirm || !ui_state.room.assigned || ui_state.room.busy) return;
        local.confirm_generation = ui_state.room.generation;
        ui_copy(local.confirm_code, sizeof(local.confirm_code), ui_state.room.code);
        view.confirm = ui_panel(ui_screen, 4, 44, 312, 192, false);
        lv_obj_set_style_bg_color(view.confirm, lv_color_hex(UI_SURFACE), 0);
        lv_obj_set_style_bg_opa(view.confirm, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(view.confirm, 0, 0);
        lv_obj_set_style_radius(view.confirm, 8, 0);
        ui_label(view.confirm, "退出当前对讲房间？", 16, 30, 280, UI_TEXT);
        ui_label(view.confirm, "退出后需要重新加入", 16, 62, 280, UI_MUTED);
        ui_button(view.confirm, "取消", 8, 128, 140, 48, room_event, ROOM_CANCEL);
        lv_obj_t *leave = ui_button(view.confirm, "退出房间", 160, 128, 140, 48, room_event, ROOM_LEAVE_YES);
        lv_obj_set_style_bg_color(leave, lv_color_hex(UI_DANGER), 0);
    } else if (action == ROOM_CANCEL || action == ROOM_LEAVE_YES) {
        if (action == ROOM_LEAVE_YES) {
            if (!view.confirm) return;
            if (!ui_state.room.assigned || ui_state.room.generation != local.confirm_generation ||
                strcmp(ui_state.room.code, local.confirm_code) != 0) ui_notice("房间已变化，请重新确认");
            else if (room_intent(TIRTC_ACTION_ROOM_LEAVE, 0, local.confirm_code, local.confirm_generation) < 0)
                ui_notice("退出请求未提交，请重试");
            else { local.submitting = true; local.submit_at = lv_tick_get(); }
        }
        close_confirm();
    }
    room_refresh(lv_tick_get());
}

static void ignore_click(lv_event_t *event) { (void)event; }

static void room_render(void)
{
    local.layout_assigned = ui_state.room.assigned;
    room_header("多人对讲");
    icon(ui_screen, LV_SYMBOL_REFRESH, local.layout_assigned ? 184 : 276, 0, 40, room_event, ROOM_REFRESH);
    if (!local.layout_assigned) {
        ui_label(ui_screen, "对讲房间", 16, 56, 288, UI_TEXT);
        view.create_button = ui_button(ui_screen, "创建房间", 8, 94, 304, 44, room_event, ROOM_CREATE);
        view.join_button = ui_button(ui_screen, "加入房间", 8, 146, 304, 44, room_event, ROOM_JOIN);
        view.status = ui_label(ui_screen, "", 12, 202, 296, UI_MUTED);
    } else {
        view.leave = ui_button(ui_screen, "退出房间", 228, 0, 84, 40, room_event, ROOM_LEAVE);
        lv_obj_set_style_bg_opa(view.leave, LV_OPA_TRANSP, 0);
        view.code = ui_label(ui_screen, "", 12, 46, 192, UI_TEXT);
        view.count = ui_label(ui_screen, "", 12, 69, 200, UI_MUTED);
        view.previous = icon(ui_screen, LV_SYMBOL_LEFT, 228, 44, 40, room_event, ROOM_PREVIOUS);
        view.next = icon(ui_screen, LV_SYMBOL_RIGHT, 276, 44, 40, room_event, ROOM_NEXT);
        for (unsigned i = 0; i < ROOM_PAGE_SIZE; ++i) {
            view.names[i] = ui_label(ui_screen, "", 16, 98 + i * 27, 198, UI_TEXT);
            view.self[i] = ui_label(ui_screen, "（本机）", 144, 98 + i * 27, 70, UI_MUTED);
            visible(view.self[i], false);
            view.speaking[i] = ui_label(ui_screen, "", 220, 98 + i * 27, 88, UI_MUTED);
            lv_label_set_long_mode(view.names[i], LV_LABEL_LONG_DOT);
            lv_obj_set_height(view.names[i], 22);
        }
        view.status = ui_label(ui_screen, "", 12, 178, 296, UI_MUTED);
        view.ptt = ui_button(ui_screen, "按下讲话", 8, 201, 304, 35, ignore_click, 0);
        lv_obj_remove_event_cb(view.ptt, ignore_click);
        lv_obj_clear_flag(view.ptt, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(view.ptt, ptt_event, LV_EVENT_ALL, NULL);
    }
    lv_label_set_long_mode(view.status, LV_LABEL_LONG_DOT);
    lv_obj_set_height(view.status, 20);
    room_refresh(lv_tick_get());
}

static void select_input(lv_obj_t *input)
{
    if (view.active_input) lv_obj_clear_state(view.active_input, LV_STATE_FOCUSED);
    view.active_input = input;
    if (input) lv_obj_add_state(input, LV_STATE_FOCUSED);
}

static void input_event(lv_event_t *event)
{
    lv_obj_t *input = lv_event_get_target(event);
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_note_interaction();
        select_input(input);
    } else if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        if (input == view.code_input) ui_copy(local.code, sizeof(local.code), lv_textarea_get_text(input));
        else if (input == view.password_input) ui_copy(local.password, sizeof(local.password), lv_textarea_get_text(input));
    }
}

static bool digits(const char *text, unsigned count)
{
    if (strlen(text) != count) return false;
    for (unsigned i = 0; i < count; ++i) if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

static void room_submit(void)
{
    if (ui_page != TIRTC_PAGE_ROOM_FORM || ui_state.room.assigned || ui_state.room.busy || local.submitting) return;
    ui_note_interaction();
    if ((!local.create && !digits(local.code, 6)) || (local.password[0] && !digits(local.password, 4))) {
        ui_notice("房间号为6位，密码为4位或留空");
        return;
    }
    if (ui_action(local.create ? TIRTC_ACTION_ROOM_CREATE : TIRTC_ACTION_ROOM_JOIN,
                  0, 0, local.create ? "" : local.code, local.password) < 0) {
        ui_notice("请求未提交，请重试");
        return;
    }
    local.submitting = true;
    local.submit_at = lv_tick_get();
    /* A queued intent does not change assignment, members, or room code. */
    lv_textarea_set_text(view.password_input, "");
    local.password[0] = '\0';
    room_refresh(lv_tick_get());
}

static void keyboard_event(lv_event_t *event)
{
    if (ui_page != TIRTC_PAGE_ROOM_FORM || !view.active_input) return;
    ui_note_interaction();
    unsigned key = lv_btnmatrix_get_selected_btn(lv_event_get_target(event));
    if (key == ROOM_KEY_CONFIRM) room_submit();
    else if (key == ROOM_KEY_BACKSPACE) lv_textarea_del_char(view.active_input);
    else if (key < ROOM_KEY_BACKSPACE) {
        const char *text = lv_btnmatrix_get_btn_text(lv_event_get_target(event), key);
        if (text) lv_textarea_add_text(view.active_input, text);
    }
}

static void keyboard_draw(lv_event_t *event)
{
    lv_obj_draw_part_dsc_t *part = lv_event_get_draw_part_dsc(event);
    if (part && part->part == LV_PART_ITEMS && part->id == ROOM_KEY_BACKSPACE && part->label_dsc)
        part->label_dsc->font = &lv_font_montserrat_14;
}

static lv_obj_t *room_input(const char *placeholder, int x, int width, int max, bool password)
{
    lv_obj_t *input = lv_textarea_create(ui_screen);
    lv_obj_set_pos(input, x, 44);
    lv_obj_set_size(input, width, 36);
    lv_obj_set_style_bg_color(input, lv_color_hex(UI_SURFACE), 0);
    lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(input, &tirtc_font_14, 0);
    lv_obj_set_style_text_color(input, lv_color_hex(UI_TEXT), 0);
    lv_obj_set_style_radius(input, 6, 0);
    lv_obj_set_style_pad_ver(input, 8, 0);
    lv_textarea_set_one_line(input, true);
    lv_obj_set_height(input, 36);
    lv_textarea_set_max_length(input, max);
    lv_textarea_set_accepted_chars(input, "0123456789");
    lv_textarea_set_placeholder_text(input, placeholder);
    lv_textarea_set_password_mode(input, password);
    lv_textarea_set_text(input, "");
    lv_obj_add_event_cb(input, input_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(input, input_event, LV_EVENT_VALUE_CHANGED, NULL);
    return input;
}

static void room_form_render(void)
{
    static const char *const keys[] = {"1", "2", "3", "4", "\n", "5", "6", "7", "8", "\n", "9", "0", LV_SYMBOL_BACKSPACE, "确认", ""};
    static const lv_btnmatrix_ctrl_t controls[] = {
        1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1 | LV_BTNMATRIX_CTRL_NO_REPEAT,
        1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1 | LV_BTNMATRIX_CTRL_NO_REPEAT,
        1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1 | LV_BTNMATRIX_CTRL_NO_REPEAT,
        1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1 | LV_BTNMATRIX_CTRL_NO_REPEAT,
        1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1 | LV_BTNMATRIX_CTRL_NO_REPEAT, 1,
        1 | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CHECKED};
    room_header(local.create ? "创建房间" : "加入房间");
    if (!local.create) {
        view.code_input = room_input("6位房间号", 8, 146, 6, false);
        lv_textarea_set_text(view.code_input, local.code);
    }
    view.password_input = room_input("4位密码（可选）", local.create ? 8 : 162, local.create ? 304 : 150, 4, true);
    lv_textarea_set_text(view.password_input, local.password);
    view.status = ui_label(ui_screen, "", 184, 12, 128, UI_MUTED);
    lv_label_set_long_mode(view.status, LV_LABEL_LONG_DOT);
    lv_obj_set_height(view.status, 20);
    view.keyboard = lv_btnmatrix_create(ui_screen);
    lv_btnmatrix_set_map(view.keyboard, (const char **)keys);
    lv_btnmatrix_set_ctrl_map(view.keyboard, controls);
    lv_obj_set_pos(view.keyboard, 8, 84);
    lv_obj_set_size(view.keyboard, 304, 152);
    lv_obj_set_style_bg_opa(view.keyboard, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view.keyboard, 0, 0);
    lv_obj_set_style_pad_all(view.keyboard, 0, 0);
    lv_obj_set_style_pad_row(view.keyboard, 4, 0);
    lv_obj_set_style_pad_column(view.keyboard, 6, 0);
    lv_obj_set_style_bg_color(view.keyboard, lv_color_hex(UI_BUTTON), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(view.keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(view.keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(view.keyboard, lv_color_hex(UI_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(view.keyboard, &tirtc_font_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(view.keyboard, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(view.keyboard, lv_color_hex(UI_ACCEPT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(view.keyboard, lv_color_hex(0x465057), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(view.keyboard, keyboard_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(view.keyboard, keyboard_draw, LV_EVENT_DRAW_PART_BEGIN, NULL);
    select_input(local.create ? view.password_input : view.code_input);
    room_refresh(lv_tick_get());
}

static void room_refresh(uint32_t now)
{
    if (ui_page != TIRTC_PAGE_ROOM && ui_page != TIRTC_PAGE_ROOM_FORM) return;
    local.room_ticked = true;
    local.room_refreshed_at = now;
    if (local.list_generation != ui_state.room.generation || strcmp(local.list_code, ui_state.room.code) != 0) {
        local.offset = 0;
        local.list_generation = ui_state.room.generation;
        ui_copy(local.list_code, sizeof(local.list_code), ui_state.room.code);
    }
    if (local.submitting && (uint32_t)(now - local.submit_at) >= 1000U) local.submitting = false;
    if (local.held && (!room_can_talk() || ui_state.room.generation != local.held_generation ||
        strcmp(ui_state.room.code, local.held_code) != 0 || (uint32_t)(now - local.held_at) > 750U)) room_release();
    if (view.confirm && (!ui_state.room.assigned || ui_state.room.generation != local.confirm_generation ||
        strcmp(ui_state.room.code, local.confirm_code) != 0)) {
        /* Keep the modal until release so its old button cannot click through
         * into the new room. Confirmation always rechecks its captured tag. */
        if (!ui_pointer_busy()) close_confirm();
    }
    if ((ui_page == TIRTC_PAGE_ROOM && local.layout_assigned != ui_state.room.assigned) ||
        (ui_page == TIRTC_PAGE_ROOM_FORM && ui_state.room.assigned)) {
        if (!ui_pointer_busy()) {
            local.offset = 0;
            local.submitting = false;
            if (ui_page == TIRTC_PAGE_ROOM_FORM) ui_navigate(TIRTC_PAGE_ROOM);
            else ui_redraw();
        }
        return;
    }
    /* Offline navigation/form editing are local; only submitting needs a
     * backend. An unknown snapshot must not lock the user out of the form. */
    if (view.create_button) ui_enabled(view.create_button, !ui_state.room.busy && !local.submitting);
    if (view.join_button) ui_enabled(view.join_button, !ui_state.room.busy && !local.submitting);
    if (view.keyboard) {
        if (ui_state.room.busy || local.submitting) lv_btnmatrix_set_btn_ctrl(view.keyboard, ROOM_KEY_CONFIRM, LV_BTNMATRIX_CTRL_DISABLED);
        else lv_btnmatrix_clear_btn_ctrl(view.keyboard, ROOM_KEY_CONFIRM, LV_BTNMATRIX_CTRL_DISABLED);
    }
    const char *status = local.submitting ? "提交中" : ui_state.room.message[0] ? ui_state.room.message :
        !ui_state.room.known ? "房间状态未获取" : ui_state.room.busy ? "正在处理" :
        ui_state.room.connected ? "房间已连接" : ui_state.room.assigned ? "等待房间连接" : "未加入房间";
    text_if_changed(view.status, status);
    if (!view.code) return;
    unsigned count = ui_state.room.member_count;
    if (count > TIRTC_UI_ROOM_MEMBER_MAX) count = TIRTC_UI_ROOM_MEMBER_MAX;
    if (local.offset >= count) local.offset = 0;
    unsigned pages = (count + ROOM_PAGE_SIZE - 1) / ROOM_PAGE_SIZE;
    char text[64];
    snprintf(text, sizeof(text), "房间  %s", ui_state.room.code[0] ? ui_state.room.code : "------");
    text_if_changed(view.code, text);
    /* The adapter supplies a visible member list, not a separate total online
     * counter. Do not label that count as an independently measured total. */
    snprintf(text, sizeof(text), ui_state.room.members_limited ? "前 %u 位成员  %u/%u" : "%u 位成员  %u/%u", count, local.offset / ROOM_PAGE_SIZE + 1, pages ? pages : 1);
    text_if_changed(view.count, text);
    ui_enabled(view.previous, local.offset > 0);
    ui_enabled(view.next, local.offset + ROOM_PAGE_SIZE < count);
    ui_enabled(view.leave, !ui_state.room.busy && !local.submitting);
    for (unsigned i = 0; i < ROOM_PAGE_SIZE; ++i) {
        bool exists = local.offset + i < count;
        const tirtc_ui_room_member_t *member = exists ? &ui_state.room.members[local.offset + i] : NULL;
        bool self = member && member->self;
        lv_obj_set_width(view.names[i], self ? 124 : 198);
        visible(view.self[i], self);
        text_if_changed(view.names[i], member ? (member->name[0] ? member->name : member->id) : "");
        text_if_changed(view.speaking[i], member ? (member->speaking ? "正在讲话" : "") : "");
        lv_obj_set_style_text_color(view.speaking[i], lv_color_hex(member && member->speaking ? UI_ACCENT : UI_MUTED), 0);
    }
    ptt_refresh();
}

static void remote_event(lv_event_t *event)
{
    const tirtc_ui_remote_t *remote = &ui_state.remote;
    tirtc_ui_action_type_t action = (tirtc_ui_action_type_t)(uintptr_t)lv_event_get_user_data(event);
    char generation[16];
    int32_t value = 0;
    ui_note_interaction();
    /* A new session under a held finger must not receive an old button tap. */
    if (!remote->generation || remote->generation != local.remote_generation || local.remote_submitting) return;
    if (action == TIRTC_ACTION_REMOTE_END) {
        if (remote->state != TIRTC_REMOTE_CONNECTING && remote->state != TIRTC_REMOTE_ACTIVE) return;
    } else {
        if (remote->state != TIRTC_REMOTE_ACTIVE) return;
        if (action == TIRTC_ACTION_REMOTE_SET_MIC) value = !remote->mic_enabled;
        else if (action == TIRTC_ACTION_REMOTE_SET_SPEAKER) value = !remote->speaker_enabled;
        else if (action == TIRTC_ACTION_REMOTE_SET_CAMERA) value = !remote->camera_enabled;
        else return;
    }
    snprintf(generation, sizeof(generation), "%lu", (unsigned long)remote->generation);
    if (ui_action(action, 0, value, NULL, generation) == 0) {
        local.remote_submitting = true; local.remote_submit_at = lv_tick_get();
        local.remote_revision = remote->revision;
        /* Accepted means queued. Effective flags and session state only change
         * when the owner publishes its next authoritative snapshot. */
    }
}

static void remote_refresh(uint32_t now)
{
    const tirtc_ui_remote_t *remote = &ui_state.remote;
    const char *status = "等待远程连接";
    char details[160];
    bool active = remote->state == TIRTC_REMOTE_ACTIVE && remote->generation != 0;
    bool can_end = (active || remote->state == TIRTC_REMOTE_CONNECTING) && remote->generation != 0;
    if (!view.remote_status || ui_pointer_busy()) return;
    if (local.remote_generation != remote->generation || local.remote_revision != remote->revision ||
        !can_end || (uint32_t)(now - local.remote_submit_at) >= 1500U)
        local.remote_submitting = false;
    local.remote_generation = remote->generation; local.remote_revision = remote->revision;
    switch (remote->state) {
    case TIRTC_REMOTE_CONNECTING: status = "远程连接中"; break;
    case TIRTC_REMOTE_ACTIVE: status = "远程查看中"; break;
    case TIRTC_REMOTE_CLOSING: status = "正在结束远程查看"; break;
    case TIRTC_REMOTE_ENDED: status = "远程查看已结束"; break;
    case TIRTC_REMOTE_ERROR: status = "远程连接失败"; break;
    default: break;
    }
    text_if_changed(view.remote_status, status);
    snprintf(details, sizeof(details), "视频请求：%s  声音请求：%s\n远程讲话：%s  %02lu:%02lu",
             remote->video_subscribed ? "有" : "无", remote->audio_subscribed ? "有" : "无",
             remote->talkback_active ? "有" : "无", (unsigned long)(remote->seconds / 60U),
             (unsigned long)(remote->seconds % 60U));
    text_if_changed(view.remote_details, details);
    text_if_changed(view.remote_message, local.remote_submitting ? "请求已提交，等待设备确认" :
                    remote->message[0] ? remote->message :
                    remote->state == TIRTC_REMOTE_IDLE ? "首页空闲时可远程连接" : remote->peer_name);
    button_text(view.remote_mic, remote->mic_enabled ? "关闭麦克风" : "开启麦克风");
    button_text(view.remote_speaker, remote->speaker_enabled ? "关闭扬声器" : "开启扬声器");
    button_text(view.remote_camera, remote->camera_enabled ? "关闭摄像头" : "开启摄像头");
    ui_enabled(view.remote_mic, active && !local.remote_submitting);
    ui_enabled(view.remote_speaker, active && !local.remote_submitting);
    ui_enabled(view.remote_camera, active && !local.remote_submitting);
    ui_enabled(view.remote_end, can_end && !local.remote_submitting);
}

static void remote_render(void)
{
    ui_header("远程监控", TIRTC_PAGE_MENU);
    view.remote_status = ui_label(ui_screen, "", 16, 48, 288, UI_ACCENT);
    view.remote_details = ui_label(ui_screen, "", 16, 74, 288, UI_TEXT);
    lv_obj_set_style_text_line_space(view.remote_details, 4, 0);
    view.remote_message = ui_label(ui_screen, "", 16, 117, 288, UI_MUTED);
    lv_label_set_long_mode(view.remote_message, LV_LABEL_LONG_DOT);
    lv_obj_set_height(view.remote_message, 20);
    view.remote_mic = ui_button(ui_screen, "", 8, 143, 148, 38, remote_event, TIRTC_ACTION_REMOTE_SET_MIC);
    view.remote_speaker = ui_button(ui_screen, "", 164, 143, 148, 38, remote_event, TIRTC_ACTION_REMOTE_SET_SPEAKER);
    view.remote_camera = ui_button(ui_screen, "", 8, 189, 148, 43, remote_event, TIRTC_ACTION_REMOTE_SET_CAMERA);
    view.remote_end = ui_button(ui_screen, "结束查看", 164, 189, 148, 43, remote_event, TIRTC_ACTION_REMOTE_END);
    lv_obj_set_style_bg_color(view.remote_end, lv_color_hex(UI_DANGER), 0);
    remote_refresh(lv_tick_get());
}

bool ui_pages_render(tirtc_ui_page_t page)
{
    switch (page) {
    case TIRTC_PAGE_SETTINGS: settings_render(); return true;
    case TIRTC_PAGE_NETWORK: network_render(); return true;
    case TIRTC_PAGE_DIAGNOSTICS: diagnostics_render(); return true;
    case TIRTC_PAGE_ROOM: room_render(); return true;
    case TIRTC_PAGE_ROOM_FORM: room_form_render(); return true;
    case TIRTC_PAGE_REMOTE: remote_render(); return true;
    default: return false;
    }
}

void ui_pages_reset(void)
{
    room_release();
    /* Do not retain a pointer into an object about to be deleted. Drafts are
     * copied on every edit, so this never has to read an old textarea. */
    memset(&view, 0, sizeof(view));
    local.room_ticked = local.diagnostics_ticked = false;
    local.confirm_generation = 0;
    local.confirm_code[0] = '\0';
    local.remote_submitting = false;
}

void ui_pages_process(bool changed, uint32_t now)
{
    /* A disabled/deleted button may receive PRESS_LOST rather than RELEASED;
     * use the physical pointer state to finish cancellation in that case. */
    if (local.ptt_cancelled_until_release && !ui_pointer_busy()) {
        local.ptt_cancelled_until_release = false;
        ptt_refresh();
    }
    if (ui_page == TIRTC_PAGE_REMOTE) remote_refresh(now);
    else if (ui_page == TIRTC_PAGE_ROOM || ui_page == TIRTC_PAGE_ROOM_FORM) {
        if (changed || !local.room_ticked || (uint32_t)(now - local.room_refreshed_at) >= 150U) room_refresh(now);
        else if (local.held && (uint32_t)(now - local.held_at) > 750U) { room_release(); ptt_refresh(); }
    }
    else if (changed && ui_page == TIRTC_PAGE_SETTINGS) settings_refresh();
    else if (changed && ui_page == TIRTC_PAGE_NETWORK) network_refresh();
    else if (ui_page == TIRTC_PAGE_DIAGNOSTICS && !ui_pointer_busy()) {
        if (!local.diagnostics_ticked || (uint32_t)(now - local.diagnostics_refreshed_at) >= 5000U) diagnostics_refresh();
    }
}

void ui_pages_navigation(tirtc_ui_page_t from, tirtc_ui_page_t to)
{
    bool was_room = from == TIRTC_PAGE_ROOM || from == TIRTC_PAGE_ROOM_FORM;
    bool is_room = to == TIRTC_PAGE_ROOM || to == TIRTC_PAGE_ROOM_FORM;
    if (from != to) room_release();
    if (from == TIRTC_PAGE_ROOM_FORM && to != TIRTC_PAGE_ROOM_FORM) {
        local.password[0] = local.code[0] = '\0';
        local.submitting = false;
    }
    if (was_room && !is_room) { local.offset = 0; local.submitting = false; }
    if (local.opened != is_room) {
        local.opened = is_room;
        if (ui_action(TIRTC_ACTION_ROOM_OPEN, 0, is_room ? 1 : 0, NULL, NULL) < 0 && is_room)
            ui_notice("对讲服务未就绪，可先填写房间信息");
    }
}
