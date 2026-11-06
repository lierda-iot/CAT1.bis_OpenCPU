/* Modern product UI based on starter_product (f7186630), not legacy main/ui.
 * One LVGL owner, fixed copy-in mailboxes; hardware remains in ../port. */
#include "ui_internal.h"
#include "font/tirtc_font_digits_48.h"
#include "ui_face.h"
#include "tirtc_video_draw.h"
#include "assets/modern_icons.h"
#include "../port/tirtc_port.h"
#ifdef TIRTC_EXTERNAL_UI_ASSETS
#include "../resource_store/assets.h"
#include "../resource_store/asset_manifest.h"
#include "assets/ui_face_background.h"
static bool external_ui_attached;
static lv_obj_t *asset_boot_label;
#endif
#include <stdio.h>
#include <string.h>

#define FRAME_W 208U
#define FRAME_H 144U
tirtc_ui_state_t ui_state;
tirtc_ui_resources_t ui_resources;
tirtc_ui_system_t ui_system;
bool ui_system_live;
tirtc_ui_platform_t ui_platform;
bool ui_platform_live;
lv_obj_t *ui_screen;
tirtc_ui_page_t ui_page = TIRTC_PAGE_HOME;
static tirtc_ui_state_t pending_state;
static tirtc_ui_ai_t pending_ai;
static tirtc_ui_contacts_t pending_contacts;
static tirtc_ui_call_t pending_call;
static tirtc_ui_remote_t pending_remote;
static tirtc_ui_room_t pending_room;
static bool room_live;
static bool remote_live;
static uint32_t remote_end_pending_until, remote_end_generation, home_phone_pressed_generation;
static bool home_phone_pressed_remote;
static bool contacts_live, contacts_available, call_live;
static bool ai_live;
static bool audio_settings_live;
static uint8_t settings_volume, settings_mic_gain;
static bool settings_speaker_enabled, settings_mic_enabled;
static int pending_volume_delta;
static tirtc_ui_network_t pending_network;
static tirtc_ui_resources_t pending_resources;
static tirtc_ui_system_t pending_system;
static bool system_dirty;
static tirtc_ui_platform_t pending_platform;
static bool platform_dirty, setup_active, setup_complete, binding_retry_pending;
static uint32_t platform_code_tick, pending_platform_tick, binding_retry_tick;
static bool resources_dirty;
static bool state_dirty, network_live, ready, needs_render;
/* Single GUI writer, aligned 32-bit publication on Cortex-M3. Zero means no
 * usable page; page+1 publishes readiness and page together without a mutex
 * in SDK admission callbacks. No LVGL object is exposed to other tasks. */
static volatile uint32_t rendered_page_word;
static bool page_waiting_flush, page_refresh_failed, page_retry_refresh;
static int pending_page = -1;
static tirtc_ui_action_cb_t backend;
static void *backend_context;
static uint16_t frame_storage[2][FRAME_W * FRAME_H];
static uint16_t *frame = frame_storage[0], *pending_frame = frame_storage[1];
static uint16_t frame_w, frame_h, pending_w, pending_h;
static bool frame_dirty, notice_dirty;
static tirtc_ui_video_stats_t video_stats;
static uint32_t pending_frame_seq, frame_seq, drawn_frame_seq, lcd_frame_seq;
static bool video_refresh_failed;
static char pending_notice[128], selected_id[96];
static bool selected_wechat;
static uint8_t preview_expression;
static lv_obj_t *clock_label, *state_label, *caption_panel, *caption_label, *wake_hint;
static lv_obj_t *home_phone, *home_phone_back, *home_phone_icon, *home_phone_end;
static int home_phone_remote = -1;
static lv_obj_t *date_label, *clock_face, *signal_bars[4], *signal_label, *toast, *qr_layer, *sleep_layer;
static lv_obj_t *contact_panel, *contact_name, *contact_status, *contact_id, *voice_button, *video_button;
static lv_obj_t *call_title, *call_peer, *call_status, *accept_button, *reject_button;
static lv_obj_t *mute_button, *camera_button, *hangup_button, *preview_image, *preview_empty;
static lv_obj_t *binding_code, *binding_status, *binding_network, *binding_retry;
static lv_img_dsc_t preview_descriptor;
static uint32_t toast_until, ai_pending_until, phone_pending_until, last_interaction, call_tick;
static bool asleep, wake_requested;
static bool contacts_changed;
static bool return_to_room_after_call;
static int last_signal_bars = -1, last_signal_ready = -1, last_caption_role = -1;
static char caption_cache[TIRTC_UI_CAPTION_MAX + 8];
static const char *const page_names[TIRTC_PAGE_COUNT] = {
 "表情首页", "时钟首页", "更多功能", "通讯录", "联系人", "待机表情",
 "表情预览", "设备设置", "网络信息", "绑定设备", "通话", "通话结束",
 "运行状态", "多人对讲", "房间设置", "远程监控"
};
static const char *const expression_names[] = {
 "中性", "开心", "大笑", "逗趣", "难过", "生气", "哭泣", "喜爱",
 "害羞", "惊讶", "震惊", "思考", "眨眼", "酷", "放松", "美味",
 "亲亲", "自信", "困倦", "搞怪", "困惑", "聆听", "悠闲"
};

void ui_copy(char *dst, size_t capacity, const char *src)
{
    size_t n = 0;
    if (!dst || !capacity) return;
    if (!src) src = "";
    while (n + 1 < capacity && src[n]) { dst[n] = src[n]; n++; }
    dst[n] = 0;
    if (src[n] && n) {
        size_t first = n;
        while (first && ((uint8_t)dst[first - 1] & 0xC0) == 0x80) first--;
        if (first) {
            uint8_t lead = (uint8_t)dst[first - 1];
            unsigned bytes = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            if (n - first + 1 < bytes) dst[first - 1] = 0;
        }
    }
}
void tirtc_ui_state_defaults(tirtc_ui_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->volume = 8; s->mic_gain = 10; s->speaker_enabled = s->mic_enabled = s->video_enabled = true;
    s->sleep_minutes = 5; s->expression_variant = UI_FACE_VARIANT_AUTO;
    ui_copy(s->version, sizeof(s->version), "UI 14 / Starter AI");
}
static void sanitize(tirtc_ui_state_t *s)
{
#define END(f) s->f[sizeof(s->f) - 1] = 0
    END(operator_name); END(network_type); END(ip_address); END(apn); END(network_message);
    END(device_id); END(version); END(protocol_version); END(status_text); END(clock_text); END(date_text);
    END(ai_status); END(ai_emotion); END(ai_caption); END(ai_utterance_id); END(contacts_message);
    END(peer_name); END(call_message); END(binding_code); END(binding_message);
    END(diagnostic_system); END(diagnostic_audio); END(diagnostic_events); END(room.code); END(room.message);
    END(remote.peer_name); END(remote.message);
#undef END
    if (s->volume > 10) s->volume = 10;
    if (s->mic_gain > 10) s->mic_gain = 10;
    if (s->audio_level > 3) s->audio_level = 3;
    if (s->expression_index >= 23) s->expression_index = 0;
    if (s->expression_variant > 1) s->expression_variant = UI_FACE_VARIANT_AUTO;
    if ((unsigned)s->ai_state > TIRTC_AI_ERROR) s->ai_state = TIRTC_AI_ERROR;
    if ((unsigned)s->call_state > TIRTC_CALL_BUSY) s->call_state = TIRTC_CALL_ERROR;
    if ((unsigned)s->call_type > TIRTC_CALL_VIDEO) s->call_type = TIRTC_CALL_AUDIO;
    if ((unsigned)s->remote.state > TIRTC_REMOTE_ERROR) s->remote.state = TIRTC_REMOTE_ERROR;
    if (s->contact_count > TIRTC_UI_CONTACT_MAX) s->contact_count = TIRTC_UI_CONTACT_MAX;
    if (s->room.member_count > TIRTC_UI_ROOM_MEMBER_MAX) s->room.member_count = TIRTC_UI_ROOM_MEMBER_MAX;
    if (s->sleep_minutes != 0 && s->sleep_minutes != 1 && s->sleep_minutes != 5 &&
        s->sleep_minutes != 10 && s->sleep_minutes != 30) s->sleep_minutes = 5;
    for (unsigned i = 0; i < TIRTC_UI_CONTACT_MAX; i++) {
        s->contacts[i].id[95] = 0; s->contacts[i].name[63] = 0;
    }
    for (unsigned i = 0; i < TIRTC_UI_ROOM_MEMBER_MAX; i++) {
        s->room.members[i].id[63] = 0; s->room.members[i].name[63] = 0;
    }
}
void tirtc_ui_set_backend(tirtc_ui_action_cb_t cb, void *context) { backend = cb; backend_context = context; }
int tirtc_ui_publish_audio_settings(uint8_t volume, uint8_t mic_gain,
                                  bool speaker_enabled, bool mic_enabled)
{
    if (volume > 10 || mic_gain > 10) return -2;
    tirtc_ui_platform_lock();
    settings_volume = volume; settings_mic_gain = mic_gain;
    settings_speaker_enabled = speaker_enabled; settings_mic_enabled = mic_enabled;
    audio_settings_live = true;
    tirtc_ui_platform_unlock();
    return 0;
}
static bool apply_audio_settings(void)
{
    bool changed;
    if (!audio_settings_live) return false;
    changed = ui_state.volume != settings_volume || ui_state.mic_gain != settings_mic_gain ||
              ui_state.speaker_enabled != settings_speaker_enabled || ui_state.mic_enabled != settings_mic_enabled;
    ui_state.volume = settings_volume; ui_state.mic_gain = settings_mic_gain;
    ui_state.speaker_enabled = settings_speaker_enabled; ui_state.mic_enabled = settings_mic_enabled;
    return changed;
}
int tirtc_ui_request_volume_delta(int delta)
{
    if (delta != -1 && delta != 1) return -2;
    tirtc_ui_platform_lock();
    pending_volume_delta += delta;
    if (pending_volume_delta > 10) pending_volume_delta = 10;
    if (pending_volume_delta < -10) pending_volume_delta = -10;
    tirtc_ui_platform_unlock();
    return 0;
}
int tirtc_ui_publish_state(const tirtc_ui_state_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock(); pending_state = *s; sanitize(&pending_state); state_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_ai(const tirtc_ui_ai_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock(); pending_ai = *s;
#define END(f) pending_ai.f[sizeof(pending_ai.f) - 1] = 0
    END(ai_status); END(ai_emotion); END(ai_caption); END(ai_utterance_id); END(diagnostic_audio);
#undef END
    if ((unsigned)pending_ai.ai_state > TIRTC_AI_ERROR) pending_ai.ai_state = TIRTC_AI_ERROR;
    if (pending_ai.volume > 10) pending_ai.volume = 10;
    if (pending_ai.mic_gain > 10) pending_ai.mic_gain = 10;
    if (pending_ai.audio_level > 3) pending_ai.audio_level = 3;
    ai_live = true; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_contacts(const tirtc_ui_contacts_t *s)
{
    if (!s || s->count > TIRTC_UI_CONTACT_MAX) return -2;
    for (unsigned i=0; i<s->count; i++) {
        if (!s->contacts[i].id[0] || !memchr(s->contacts[i].id,0,sizeof(s->contacts[i].id))) return -2;
    }
    tirtc_ui_platform_lock(); pending_contacts = *s;
    pending_contacts.message[sizeof(pending_contacts.message)-1] = 0;
    for (unsigned i=0; i<TIRTC_UI_CONTACT_MAX; i++) {
        pending_contacts.contacts[i].id[95]=0;
        pending_contacts.contacts[i].name[63]=0;
    }
    contacts_live=true; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_call(const tirtc_ui_call_t *s)
{
    if (!s || (unsigned)s->state>TIRTC_CALL_BUSY || (unsigned)s->type>TIRTC_CALL_VIDEO) return -2;
    tirtc_ui_platform_lock(); pending_call=*s;
    pending_call.peer_id[95]=0; pending_call.peer_name[95]=0; pending_call.message[127]=0;
    call_live=true; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_remote(const tirtc_ui_remote_t *s)
{
    if (!s || (unsigned)s->state > TIRTC_REMOTE_ERROR) return -2;
    tirtc_ui_platform_lock(); pending_remote = *s;
    pending_remote.peer_name[sizeof(pending_remote.peer_name) - 1U] = 0;
    pending_remote.message[sizeof(pending_remote.message) - 1U] = 0;
    remote_live = true; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_room(const tirtc_ui_room_t *s)
{
    if (!s || s->member_count > TIRTC_UI_ROOM_MEMBER_MAX) return -2;
    tirtc_ui_platform_lock(); pending_room = *s;
    pending_room.code[sizeof(pending_room.code) - 1U] = 0;
    pending_room.message[sizeof(pending_room.message) - 1U] = 0;
    for (unsigned i = 0; i < TIRTC_UI_ROOM_MEMBER_MAX; ++i) {
        pending_room.members[i].id[63] = 0;
        pending_room.members[i].name[63] = 0;
    }
    room_live = true; tirtc_ui_platform_unlock(); return 0;
}
static bool apply_room(void)
{
    if (!room_live) return false;
    bool changed = memcmp(&ui_state.room, &pending_room, sizeof(pending_room)) != 0;
    ui_state.room = pending_room;
    return changed;
}
bool tirtc_ui_get_rendered_page(tirtc_ui_page_t *out)
{
    uint32_t page = rendered_page_word;
    if (!out || !page || page > TIRTC_PAGE_COUNT) return false;
    *out = (tirtc_ui_page_t)(page - 1U);
    return true;
}
static bool remote_session_active(void)
{
    return ui_state.remote.state >= TIRTC_REMOTE_CONNECTING &&
           ui_state.remote.state <= TIRTC_REMOTE_CLOSING;
}
static bool apply_remote(void)
{
    bool changed;
    if (!remote_live) return false;
    changed = memcmp(&ui_state.remote, &pending_remote, sizeof(pending_remote)) != 0;
    ui_state.remote = pending_remote;
    if (remote_session_active()) wake_requested = asleep;
    else remote_end_pending_until = 0;
    return changed;
}
static bool apply_contacts(void)
{
    if (!contacts_live) return false;
    bool different=ui_state.contact_count!=pending_contacts.count ||
        contacts_available!=(pending_contacts.known && !pending_contacts.loading && !pending_contacts.stale) ||
        ui_state.contacts_loading!=pending_contacts.loading ||
        memcmp(ui_state.contacts,pending_contacts.contacts,sizeof(ui_state.contacts)) ||
        strncmp(ui_state.contacts_message,pending_contacts.message,sizeof(ui_state.contacts_message)-1);
    ui_state.contact_count=pending_contacts.count;
    ui_state.contacts_loading=pending_contacts.loading;
    contacts_available=pending_contacts.known && !pending_contacts.loading && !pending_contacts.stale;
    memcpy(ui_state.contacts,pending_contacts.contacts,sizeof(ui_state.contacts));
    ui_copy(ui_state.contacts_message,sizeof(ui_state.contacts_message),pending_contacts.message);
    if (different) contacts_changed=true;
    return different;
}
static bool apply_ai(void)
{
    bool changed = false;
    if (!ai_live) return false;
#define FIELD(f) do { if (memcmp(&ui_state.f, &pending_ai.f, sizeof(ui_state.f))) { memcpy(&ui_state.f, &pending_ai.f, sizeof(ui_state.f)); changed = true; } } while (0)
    FIELD(ai_active); FIELD(ai_state); FIELD(audio_level);
    if (!audio_settings_live) { FIELD(volume); FIELD(mic_gain); FIELD(speaker_enabled); FIELD(mic_enabled); }
    FIELD(ai_status); FIELD(ai_emotion);
    FIELD(ai_caption); FIELD(ai_utterance_id); FIELD(ai_revision); FIELD(ai_paragraph);
    FIELD(ai_caption_is_ai); FIELD(ai_caption_final); FIELD(ai_caption_truncated);
#undef FIELD
    if (strcmp(ui_state.diagnostic_audio, pending_ai.diagnostic_audio)) {
        ui_copy(ui_state.diagnostic_audio, sizeof(ui_state.diagnostic_audio), pending_ai.diagnostic_audio);
        changed = true;
    }
    if (ui_state.ai_active) wake_requested = asleep;
    return changed;
}
int tirtc_ui_publish_network(const tirtc_ui_network_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock(); pending_network = *s;
#define END(f) pending_network.f[sizeof(pending_network.f) - 1] = 0
    END(operator_name); END(network_type); END(ip_address); END(apn); END(network_message);
#undef END
    network_live = true; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_resources(const tirtc_ui_resources_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock();
    pending_resources = *s;
    if (!s->flash_total_bytes || s->flash_used_bytes > s->flash_total_bytes)
        pending_resources.flash_valid = false;
    if (!s->static_ram_total_bytes || s->static_ram_used_bytes > s->static_ram_total_bytes)
        pending_resources.static_ram_valid = false;
    if (!s->heap_total_bytes || s->heap_free_bytes > s->heap_total_bytes)
        pending_resources.heap_valid = false;
    if (!pending_resources.heap_valid || s->heap_min_free_bytes > s->heap_total_bytes)
        pending_resources.heap_min_valid = false;
    if (!pending_resources.heap_valid || s->heap_max_free_block_bytes > s->heap_total_bytes)
        pending_resources.heap_max_valid = false;
    if (!s->filesystem_total_bytes || s->filesystem_free_bytes > s->filesystem_total_bytes)
        pending_resources.filesystem_valid = false;
    if (!s->external_flash_total_bytes)
        pending_resources.external_flash_valid = false;
    if ((unsigned)s->external_state > TIRTC_STORAGE_READ_ONLY) {
        pending_resources.external_state = TIRTC_STORAGE_ERROR;
        pending_resources.external_filesystem_valid = false;
    }
    if (!pending_resources.external_flash_valid ||
        (pending_resources.external_state != TIRTC_STORAGE_READY &&
         pending_resources.external_state != TIRTC_STORAGE_READ_ONLY) ||
        !s->external_filesystem_total_bytes ||
        s->external_filesystem_total_bytes > s->external_flash_total_bytes ||
        s->external_filesystem_free_bytes > s->external_filesystem_total_bytes)
        pending_resources.external_filesystem_valid = false;
    resources_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_system(const tirtc_ui_system_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock();
    pending_system = *s;
#define END(f) pending_system.f[sizeof(pending_system.f) - 1] = 0
    END(clock_text); END(date_text); END(modem_imei); END(modem_version);
    END(diagnostic_audio); END(diagnostic_events);
#undef END
    if (!pending_system.clock_valid) {
        pending_system.clock_text[0] = 0; pending_system.date_text[0] = 0;
    }
    if (!pending_system.modem_imei_valid) pending_system.modem_imei[0] = 0;
    if (!pending_system.modem_version_valid) pending_system.modem_version[0] = 0;
    system_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
static bool six_digits(const char *code)
{
    for (unsigned i = 0; i < 6; ++i)
        if (code[i] < '0' || code[i] > '9') return false;
    return code[6] == 0;
}
int tirtc_ui_publish_platform(const tirtc_ui_platform_t *s)
{
    if (!s) return -2;
    tirtc_ui_platform_lock(); pending_platform = *s;
    /* TTL starts when published, not when a held finger finally releases. */
    pending_platform_tick = lv_tick_get();
    pending_platform.code[sizeof(pending_platform.code) - 1] = 0;
    pending_platform.message[sizeof(pending_platform.message) - 1] = 0;
    pending_platform.device_id[sizeof(pending_platform.device_id) - 1] = 0;
    if ((unsigned)pending_platform.binding_state > TIRTC_BINDING_ERROR)
        pending_platform.binding_state = TIRTC_BINDING_ERROR;
    if (!pending_platform.known || !pending_platform.enabled) {
        pending_platform.api_ready = pending_platform.mqtt_connected = false;
        pending_platform.rtc_ready = pending_platform.busy = false;
        pending_platform.binding_required = false;
        pending_platform.binding_state = TIRTC_BINDING_DISABLED;
    }
    if (!pending_platform.rtc_enabled) pending_platform.rtc_ready = false;
    if (pending_platform.binding_state != TIRTC_BINDING_WAITING_USER ||
        !pending_platform.seconds_left || !six_digits(pending_platform.code)) {
        pending_platform.code[0] = 0; pending_platform.seconds_left = 0;
    }
    platform_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
bool ui_platform_setup_active(void) { return setup_active; }
static bool apply_platform(void)
{
    bool changed = false;
    if (!ui_platform_live) return false;
#define ASSIGN(dst, value) do { if ((dst) != (value)) { (dst) = (value); changed = true; } } while (0)
    ASSIGN(ui_state.bound, ui_platform.known && ui_platform.enabled && !ui_platform.binding_required && ui_platform.binding_state == TIRTC_BINDING_BOUND);
    ASSIGN(ui_state.binding_state, ui_platform.binding_state);
    ASSIGN(ui_state.mqtt_connected, ui_platform.mqtt_connected);
    ASSIGN(ui_state.rtc_connected, ui_platform.rtc_ready);
#undef ASSIGN
#define COPY(dst, src) do { if (strcmp((dst), (src))) { ui_copy((dst), sizeof(dst), (src)); changed = true; } } while (0)
    COPY(ui_state.binding_code, ui_platform.code); COPY(ui_state.binding_message, ui_platform.message);
    COPY(ui_state.device_id, ui_platform.device_id);
#undef COPY
    return changed;
}
static int setup_target(int requested)
{
    if (ui_platform_live && ui_platform.known && ui_platform.enabled && ui_platform.binding_required)
        setup_complete = false;
    if (!ui_platform_live || !ui_platform.known || !ui_platform.enabled || setup_complete) {
        if (setup_active) { setup_active = false; needs_render = true; }
        return requested;
    }
    bool complete = !ui_platform.binding_required && ui_platform.binding_state == TIRTC_BINDING_BOUND &&
                    ui_platform.api_ready && ui_platform.mqtt_connected;
    if (complete) {
        setup_complete = true;
        bool return_home = setup_active;
        setup_active = false;
        return return_home ? TIRTC_PAGE_HOME : requested;
    }
    if (!setup_active) { setup_active = true; needs_render = true; wake_requested = asleep; }
    return ui_state.network_status_valid && ui_state.network_connected ? TIRTC_PAGE_BINDING : TIRTC_PAGE_NETWORK;
}
static const char *display_clock(void)
{
    const char *text = ui_system_live ? ui_system.clock_text : ui_state.clock_text;
    return text[0] ? text : "--:--";
}
static const char *display_date(void)
{
    return ui_system_live ? ui_system.date_text : ui_state.date_text;
}
static bool apply_network(void)
{
    bool changed = false;
    if (!network_live) return false;
#define FIELD(f) do { if (memcmp(&ui_state.f, &pending_network.f, sizeof(ui_state.f))) { memcpy(&ui_state.f, &pending_network.f, sizeof(ui_state.f)); changed = true; } } while (0)
    FIELD(network_status_valid); FIELD(network_connected); FIELD(network_connecting); FIELD(network_roaming);
    FIELD(signal_valid); FIELD(sim_state); FIELD(registration); FIELD(signal_dbm);
    FIELD(signal_rsrq_valid); FIELD(signal_snr_valid); FIELD(signal_rssi_valid);
    FIELD(signal_rsrq_x2); FIELD(signal_snr_db); FIELD(signal_rssi_dbm); FIELD(signal_bars);
    FIELD(operator_name); FIELD(network_type); FIELD(ip_address); FIELD(apn); FIELD(network_message);
#undef FIELD
    return changed;
}
int tirtc_ui_request_page(tirtc_ui_page_t page)
{
    if ((unsigned)page >= TIRTC_PAGE_COUNT) return -2;
    tirtc_ui_platform_lock(); pending_page = page; tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_notify(const char *message)
{
    if (!message) return -2;
    tirtc_ui_platform_lock(); ui_copy(pending_notice, sizeof(pending_notice), message); notice_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
int tirtc_ui_publish_frame(const uint16_t *pixels, uint16_t w, uint16_t h)
{
    if ((!pixels && (w || h)) || (pixels && (!w || !h)) || w > FRAME_W || h > FRAME_H) return -2;
    tirtc_ui_platform_lock();
    if (pixels) memcpy(pending_frame, pixels, (size_t)w * h * sizeof(*pixels));
    if (frame_dirty && pending_w && pending_h) ++video_stats.replaced;
    if (pixels) {
        ++video_stats.published;
        if (++pending_frame_seq == 0U) ++pending_frame_seq;
    }
    pending_w = w; pending_h = h; frame_dirty = true;
    tirtc_ui_platform_unlock(); return 0;
}
void tirtc_ui_video_stats(tirtc_ui_video_stats_t *stats)
{
    if (!stats) return;
    tirtc_ui_platform_lock();
    *stats = video_stats;
    tirtc_ui_platform_unlock();
    tirtc_video_draw_stats_t drawing;
    tirtc_video_draw_get_stats(&drawing);
    stats->draw_fast = drawing.fast_calls;
    stats->draw_fallback = drawing.fallback_calls;
    stats->video_visible = ui_page == TIRTC_PAGE_CALL &&
        ui_state.call_type == TIRTC_CALL_VIDEO && preview_image != NULL;
}
void tirtc_ui_video_flush_done(bool success, bool last)
{
    if (page_waiting_flush && !success) page_refresh_failed = true;
    if (last && page_waiting_flush) {
        if (!page_refresh_failed) {
            rendered_page_word = (uint32_t)ui_page + 1U;
            page_waiting_flush = false;
        } else page_retry_refresh = true;
        page_refresh_failed = false;
    }
    if (!success) video_refresh_failed = true;
    if (!last) return;
    if (!video_refresh_failed && drawn_frame_seq && drawn_frame_seq != lcd_frame_seq) {
        lcd_frame_seq = drawn_frame_seq;
        ++video_stats.lcd_frames;
    }
    drawn_frame_seq = 0U;
    video_refresh_failed = false;
}
static void preview_drawn(lv_event_t *event)
{
    /* Only the GUI task touches frame/descriptor/objects. A publication can
     * replace pending_frame during drawing, but cannot replace this frame. */
    if (lv_event_get_target(event) == preview_image && frame_w && frame_h)
        drawn_frame_seq = frame_seq;
}
tirtc_ui_page_t tirtc_ui_current_page(void) { return ui_page; }
const char *tirtc_ui_page_name(tirtc_ui_page_t page) { return (unsigned)page < TIRTC_PAGE_COUNT ? page_names[page] : "未知页面"; }
bool ui_pointer_busy(void)
{
    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)))
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER &&
            (indev->proc.state == LV_INDEV_STATE_PRESSED || lv_indev_get_scroll_obj(indev))) return true;
    return false;
}
void ui_note_interaction(void) { last_interaction = lv_tick_get(); }
void ui_navigate(tirtc_ui_page_t page) { ui_note_interaction(); (void)tirtc_ui_request_page(page); }
void ui_redraw(void) { needs_render = true; }
void ui_enabled(lv_obj_t *obj, bool enabled)
{
    if (!obj) return;
    if (lv_obj_has_state(obj, LV_STATE_DISABLED) == !enabled) return;
    if (enabled) lv_obj_clear_state(obj, LV_STATE_DISABLED); else lv_obj_add_state(obj, LV_STATE_DISABLED);
    if (lv_obj_check_type(obj, &lv_btn_class)) {
        lv_obj_t *label = lv_obj_get_child(obj, 0);
        if (label && lv_obj_check_type(label, &lv_label_class))
            lv_obj_set_style_text_color(label, lv_color_hex(enabled ? UI_TEXT : 0x81898F), 0);
    }
}
static void visible(lv_obj_t *obj, bool show)
{
    if (!obj) return;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == !show) return;
    if (show) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static void text_if_changed(lv_obj_t *label, const char *text)
{ if (label && strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text); }
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, int x, int y, int w, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y); lv_obj_set_width(label, w);
    lv_obj_set_style_text_font(label, &tirtc_font_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(label, 3, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_label_set_text(label, text ? text : "");
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE); return label;
}
lv_obj_t *ui_panel(lv_obj_t *parent, int x, int y, int w, int h, bool scroll)
{
    lv_obj_t *panel = lv_obj_create(parent); lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, x, y); lv_obj_set_size(panel, w, h);
    if (scroll) {
        lv_obj_set_scroll_dir(panel, LV_DIR_VER); lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(panel, 3, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_color(panel, lv_color_hex(UI_MUTED), LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(panel, LV_OPA_70, LV_PART_SCROLLBAR);
    } else lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, lv_event_cb_t cb, uintptr_t value)
{
    lv_obj_t *button = lv_btn_create(parent); lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y); lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, lv_color_hex(UI_BUTTON), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0); lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x465057), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x222629), LV_STATE_DISABLED);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = ui_label(button, text, 0, 0, w - 4, UI_TEXT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(label);
    if (cb) lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)value);
    return button;
}
void ui_notice(const char *message)
{
    if (!toast) {
        toast = ui_label(lv_layer_top(), "", 8, 42, 304, UI_TEXT);
        lv_obj_set_style_bg_color(toast, lv_color_hex(0x3C4348), 0);
        lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0); lv_obj_set_style_pad_all(toast, 8, 0);
        lv_obj_set_style_radius(toast, 6, 0); lv_obj_set_style_text_align(toast, LV_TEXT_ALIGN_CENTER, 0);
    }
    text_if_changed(toast, message); visible(toast, true); lv_obj_move_foreground(toast);
    toast_until = lv_tick_get() + 2400;
}
int ui_action(tirtc_ui_action_type_t type, uint8_t index, int32_t value, const char *text, const char *extra)
{
    tirtc_ui_action_t action = {0}; action.type = type; action.page = ui_page;
    action.index = index; action.value = value;
    ui_copy(action.text, sizeof(action.text), text); ui_copy(action.extra, sizeof(action.extra), extra);
    int result = backend ? backend(&action, backend_context) : -1;
    /* These choices are local presentation preferences; no media success is implied. */
    if (type == TIRTC_ACTION_SET_IDLE_EXPRESSION || type == TIRTC_ACTION_SET_SLEEP_MINUTES ||
        type == TIRTC_ACTION_SET_ACK_VOICE) result = 0;
    bool advisory = type == TIRTC_ACTION_ENTER_PAGE || type == TIRTC_ACTION_RETURN_HOME ||
                    type == TIRTC_ACTION_ROOM_OPEN || (type == TIRTC_ACTION_ROOM_PTT && value == 0);
    if (result < 0 && !advisory) ui_notice("此功能暂不可用，请稍后再试");
    return result;
}
static void route_cb(lv_event_t *e) { ui_navigate((tirtc_ui_page_t)(uintptr_t)lv_event_get_user_data(e)); }
static lv_obj_t *route(const char *text, int x, int y, int w, int h, tirtc_ui_page_t page)
{ return ui_button(ui_screen, text, x, y, w, h, route_cb, page); }
static lv_obj_t *symbol_button(const char *symbol, int x, int y, int w, int h, lv_event_cb_t cb, uintptr_t value)
{
    lv_obj_t *button = ui_button(ui_screen, symbol, x, y, w, h, cb, value);
    lv_obj_set_style_text_font(lv_obj_get_child(button, 0), &lv_font_montserrat_14, 0);
    return button;
}
static void header_status(int clock_x, int signal_x)
{
    clock_label = ui_label(ui_screen, "--:--", clock_x, 11, 47, UI_ACCENT);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_14, 0);
    for (unsigned i = 0; i < 4; i++) {
        signal_bars[i] = ui_panel(ui_screen, signal_x + (int)i * 4, 25 - (4 + (int)i * 3), 3, 4 + (int)i * 3, false);
        lv_obj_set_style_bg_opa(signal_bars[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(signal_bars[i], LV_OBJ_FLAG_CLICKABLE);
    }
    signal_label = ui_label(ui_screen, "4G", signal_x + 18, 11, 22, UI_MUTED);
}
void ui_header(const char *title, tirtc_ui_page_t back)
{
    (void)symbol_button(LV_SYMBOL_LEFT, 4, 0, 44, 40, route_cb, back);
    lv_obj_t *label = ui_label(ui_screen, title, 56, 12, 150, UI_TEXT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT); lv_obj_set_height(label, 20);
    header_status(222, 277);
}
static void brand(void)
{
    lv_obj_t *mark = ui_panel(ui_screen, 14, 7, 20, 20, false);
    lv_obj_set_style_bg_color(mark, lv_color_hex(0x72DEF8), 0); lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mark, 6, 0); lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *eye = ui_panel(mark, 4 + i * 9, 7, 3, 5, false);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0x07151C), 0); lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0); lv_obj_clear_flag(eye, LV_OBJ_FLAG_CLICKABLE);
    }
    (void)ui_label(ui_screen, "小钛", 40, 11, 36, 0xBFE9F3);
}
static void close_qr_cb(lv_event_t *event)
{
    (void)event; ui_note_interaction();
    if (qr_layer) { lv_obj_del(qr_layer); qr_layer = NULL; ui_face_pause(asleep); }
}
static void home_phone_press_cb(lv_event_t *event)
{
    (void)event;
    home_phone_pressed_remote = remote_session_active();
    home_phone_pressed_generation = home_phone_pressed_remote ? ui_state.remote.generation : 0;
}
static void qr_cb(lv_event_t *event)
{
    (void)event; ui_note_interaction(); if (qr_layer) return;
    if (remote_session_active()) {
        char generation[16];
        if (!home_phone_pressed_remote || !home_phone_pressed_generation ||
            home_phone_pressed_generation != ui_state.remote.generation ||
            ui_state.remote.state == TIRTC_REMOTE_CLOSING ||
            (remote_end_generation == ui_state.remote.generation &&
             (int32_t)(lv_tick_get() - remote_end_pending_until) < 0)) return;
        snprintf(generation, sizeof(generation), "%lu", (unsigned long)ui_state.remote.generation);
        if (ui_action(TIRTC_ACTION_REMOTE_END, 0, 0, NULL, generation) == 0) {
            remote_end_generation = ui_state.remote.generation;
            remote_end_pending_until = lv_tick_get() + 1500U;
        }
        return;
    }
    /* A remote end arriving during a held press must not turn that release
     * into an unrelated outgoing phone call. */
    if (home_phone_pressed_remote) return;
    if ((int32_t)(lv_tick_get() - phone_pending_until) < 0) return;
    for (unsigned i = 0; i < ui_state.contact_count; i++) {
        const tirtc_ui_contact_t *contact = &ui_state.contacts[i];
        if (!contact->wechat) continue;
        if (ui_action(TIRTC_ACTION_CALL_AUDIO, (uint8_t)i, 1, contact->id, contact->name) == 0) {
            phone_pending_until = lv_tick_get() + 1500;
            ui_notice("正在发起呼叫");
        }
        return;
    }
    qr_layer = ui_panel(lv_layer_top(), 0, 0, 320, 240, false);
    lv_obj_set_style_bg_color(qr_layer, lv_color_hex(UI_BG), 0); lv_obj_set_style_bg_opa(qr_layer, LV_OPA_COVER, 0);
    lv_obj_t *label = ui_label(qr_layer, "微信扫码添加联系人", 12, 10, 296, UI_TEXT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *qr = lv_img_create(qr_layer); lv_img_set_src(qr, &ui_wechat_qr_image);
    lv_obj_set_pos(qr, 67, 45); lv_obj_clear_flag(qr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(qr_layer, close_qr_cb, LV_EVENT_CLICKED, NULL);
    ui_face_pause(true); visible(toast, false);
}
static void ai_cb(lv_event_t *e)
{
    (void)e; ui_note_interaction();
    if (remote_session_active()) return;
    if (ui_state.ai_active || (int32_t)(lv_tick_get() - ai_pending_until) < 0) return;
    if (ui_action(TIRTC_ACTION_AI_START, 0, 0, NULL, NULL) == 0) ai_pending_until = lv_tick_get() + 1500;
}
static void home_clock_cb(lv_event_t *e)
{ (void)e; ui_navigate(ui_page == TIRTC_PAGE_CLOCK ? TIRTC_PAGE_HOME : TIRTC_PAGE_CLOCK); }
static void render_home(void)
{
    (void)ui_face_background(ui_screen); brand(); header_status(194, 242);
    state_label = ui_label(ui_screen, "点击开始", 80, 11, 110, UI_ACCENT);
    lv_label_set_long_mode(state_label, LV_LABEL_LONG_DOT); lv_obj_set_height(state_label, 20);
    if (ui_page == TIRTC_PAGE_CLOCK) {
        clock_face = ui_label(ui_screen, "--:--", 24, 70, 272, UI_TEXT);
        lv_obj_set_style_text_font(clock_face, &tirtc_font_digits_48, 0);
        lv_obj_set_style_text_align(clock_face, LV_TEXT_ALIGN_CENTER, 0);
        date_label = ui_label(ui_screen, "", 40, 132, 240, UI_MUTED);
        lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
    } else if (!ui_face_create(ui_screen, 50, 66)) ui_notice("表情初始化失败");
    int caption_height = 2 * lv_font_get_line_height(&tirtc_font_14) + 4;
    caption_panel = ui_panel(ui_screen, 8, 234 - caption_height, 304, caption_height, false);
    lv_obj_clear_flag(caption_panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    caption_label = ui_label(caption_panel, "", 8, 0, 288, 0xB5DFFF);
    lv_obj_set_style_text_line_space(caption_label, 4, 0);
    lv_label_set_long_mode(caption_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(caption_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    wake_hint = ui_label(ui_screen, "点击表情开始对话\n连接后直接说话", 16, 195, 288, UI_MUTED);
    lv_obj_set_style_text_align(wake_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(wake_hint, 4, 0);
    lv_obj_t *tap = ui_panel(ui_screen, 48, 66, 224, 108, false);
    lv_obj_add_event_cb(tap, ai_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *left = symbol_button(LV_SYMBOL_LEFT, 0, 94, 44, 52, home_clock_cb, 0);
    lv_obj_t *right = symbol_button(LV_SYMBOL_RIGHT, 276, 94, 44, 52, home_clock_cb, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0); lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_t *more = symbol_button(LV_SYMBOL_BULLET " " LV_SYMBOL_BULLET " " LV_SYMBOL_BULLET,
                                    283, 6, 32, 26, route_cb, TIRTC_PAGE_MENU);
    lv_obj_set_style_bg_opa(more, LV_OPA_20, 0); lv_obj_set_ext_click_area(more, 10);
    lv_obj_t *phone = ui_button(ui_screen, "", 282, 156, 32, 32, qr_cb, 0);
    home_phone = phone;
    lv_obj_add_event_cb(phone, home_phone_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_set_style_bg_opa(phone, LV_OPA_TRANSP, 0); lv_obj_set_ext_click_area(phone, 8);
    lv_obj_t *back = ui_panel(phone, 3, 3, 26, 26, false);
    home_phone_back = back;
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(back, lv_color_white(), 0); lv_obj_clear_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *icon = lv_img_create(phone); lv_img_set_src(icon, &ui_phone_image);
    home_phone_icon = icon;
    lv_img_set_zoom(icon, 195); lv_obj_center(icon); lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    home_phone_end = lv_label_create(phone); lv_label_set_text(home_phone_end, LV_SYMBOL_CALL);
    lv_obj_set_style_text_font(home_phone_end, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_phone_end, lv_color_white(), 0);
    lv_obj_center(home_phone_end); lv_obj_clear_flag(home_phone_end, LV_OBJ_FLAG_CLICKABLE);
    visible(home_phone_end, false);
    caption_cache[0] = 0;
}
static void render_menu(void)
{
    ui_header("更多功能", TIRTC_PAGE_HOME);
    (void)route("通讯录", 8, 46, 304, 37, TIRTC_PAGE_CONTACTS);
    lv_obj_t *room = route("多人对讲", 8, 89, 304, 37, TIRTC_PAGE_ROOM);
    lv_obj_set_style_bg_color(room, lv_color_hex(0x233D37), 0);
    (void)ui_label(ui_screen, "设备", 12, 132, 280, UI_MUTED);
    (void)route("设备设置", 8, 155, 148, 36, TIRTC_PAGE_SETTINGS);
    (void)route("待机表情", 164, 155, 148, 36, TIRTC_PAGE_EXPRESSIONS);
    (void)route("网络信息", 8, 197, 148, 36, TIRTC_PAGE_NETWORK);
    (void)route("运行状态", 164, 197, 148, 36, TIRTC_PAGE_DIAGNOSTICS);
}
static int selected_contact(void)
{
    for (unsigned i = 0; i < ui_state.contact_count; i++)
        if (selected_wechat==ui_state.contacts[i].wechat && !strcmp(selected_id, ui_state.contacts[i].id)) return (int)i;
    return -1;
}
static void contact_cb(lv_event_t *e)
{
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (index >= ui_state.contact_count) return;
    ui_copy(selected_id, sizeof(selected_id), ui_state.contacts[index].id);
    selected_wechat=ui_state.contacts[index].wechat;
    ui_navigate(TIRTC_PAGE_CONTACT_DETAIL);
}
static void refresh_contacts_cb(lv_event_t *e)
{ (void)e; ui_note_interaction(); (void)ui_action(TIRTC_ACTION_REFRESH_CONTACTS, 0, 0, NULL, NULL); }
static void fill_contacts(void)
{
    int scroll = lv_obj_get_scroll_y(contact_panel); lv_obj_clean(contact_panel);
    int offset=0;
    if (ui_state.contact_count && (ui_state.contacts_loading || ui_state.contacts_message[0])) {
        lv_obj_t *hint=ui_label(contact_panel,ui_state.contacts_loading ? "正在同步联系人…" : ui_state.contacts_message,8,2,280,UI_MUTED);
        lv_label_set_long_mode(hint,LV_LABEL_LONG_DOT); lv_obj_set_height(hint,22); offset=28;
    }
    if (!ui_state.contact_count) {
        (void)ui_label(contact_panel, ui_state.contacts_loading ? "正在同步联系人…" : "暂无联系人", 12, 16, 280, UI_TEXT);
        (void)ui_label(contact_panel, ui_state.contacts_message[0] ? ui_state.contacts_message : "绑定设备或完成微信授权后同步", 12, 48, 280, UI_MUTED);
    }
    for (unsigned i = 0; i < ui_state.contact_count; i++) {
        const tirtc_ui_contact_t *c = &ui_state.contacts[i];
        lv_obj_t *row = ui_button(contact_panel, "", 0, offset+i * 66, 296, 60, contact_cb, i);
        lv_obj_t *name = ui_label(row, c->name[0] ? c->name : c->id, 12, 8, 232, UI_TEXT);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT); lv_obj_set_height(name, 20);
        (void)ui_label(row, c->wechat ? "微信联系人" : c->online ? "设备 / 在线" : "设备 / 离线", 12, 34, 232, c->online ? UI_ACCENT : UI_MUTED);
        lv_obj_t *arrow = ui_label(row, LV_SYMBOL_RIGHT, 268, 22, 16, UI_MUTED);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    }
    lv_obj_update_layout(contact_panel); lv_obj_scroll_to_y(contact_panel, scroll, LV_ANIM_OFF);
    contacts_changed=false;
}
static void render_contacts(void)
{
    ui_header("通讯录", TIRTC_PAGE_MENU); visible(clock_label, false);
    /* Refresh replaces the generic clock area, leaving the 4G status intact. */
    (void)symbol_button(LV_SYMBOL_REFRESH, 222, 0, 44, 40, refresh_contacts_cb, 0);
    contact_panel = ui_panel(ui_screen, 8, 48, 304, 188, true); fill_contacts();
}
static void dial_cb(lv_event_t *e)
{
    ui_note_interaction(); int index = selected_contact();
    if (contacts_live && !contacts_available) { ui_notice("联系人尚未同步，请重试"); return; }
    if (index < 0) { ui_notice("联系人已更新，请返回重试"); return; }
    const tirtc_ui_contact_t *c = &ui_state.contacts[index];
    if (!c->online && !c->wechat) { ui_notice("设备离线，暂时无法呼叫"); return; }
    bool video = (uintptr_t)lv_event_get_user_data(e) != 0;
    if (ui_action(video ? TIRTC_ACTION_CALL_VIDEO : TIRTC_ACTION_CALL_AUDIO, (uint8_t)index,
                  c->wechat ? 1 : 0, c->id, c->name) == 0) ui_notice("正在发起呼叫");
}
static void render_contact_detail(void)
{
    ui_header("联系人", TIRTC_PAGE_CONTACTS);
    contact_name = ui_label(ui_screen, "", 16, 60, 288, UI_TEXT);
    lv_label_set_long_mode(contact_name, LV_LABEL_LONG_DOT); lv_obj_set_height(contact_name, 44);
    contact_status = ui_label(ui_screen, "", 16, 116, 288, UI_ACCENT);
    contact_id = ui_label(ui_screen, "", 16, 144, 288, UI_MUTED);
    lv_label_set_long_mode(contact_id, LV_LABEL_LONG_DOT); lv_obj_set_height(contact_id, 36);
    voice_button = ui_button(ui_screen, "语音呼叫", 8, 192, 148, 44, dial_cb, 0);
    video_button = ui_button(ui_screen, "视频呼叫", 164, 192, 148, 44, dial_cb, 1);
    lv_obj_set_style_bg_color(voice_button, lv_color_hex(UI_ACCEPT), 0);
}
static void expression_cb(lv_event_t *e)
{ preview_expression = (uint8_t)(uintptr_t)lv_event_get_user_data(e); ui_navigate(TIRTC_PAGE_EXPRESSION_PREVIEW); }
static void apply_expression_cb(lv_event_t *e)
{
    (void)e;
    if (ui_action(TIRTC_ACTION_SET_IDLE_EXPRESSION, preview_expression, UI_FACE_VARIANT_AUTO,
                  ui_face_key_at(preview_expression), NULL) == 0) {
        ui_state.expression_index = preview_expression; ui_state.expression_variant = UI_FACE_VARIANT_AUTO;
        ui_navigate(TIRTC_PAGE_HOME);
    }
}
static void render_expressions(void)
{
    ui_header("待机表情", TIRTC_PAGE_MENU);
    lv_obj_t *panel = ui_panel(ui_screen, 8, 48, 304, 188, true);
    for (unsigned i = 0; i < 23; i++) {
        lv_obj_t *b = ui_button(panel, expression_names[i], (i % 3) * 100, (i / 3) * 52, 94, 46, expression_cb, i);
        if (i == ui_state.expression_index) {
            lv_obj_set_style_border_width(b, 2, 0); lv_obj_set_style_border_color(b, lv_color_hex(UI_ACCENT), 0);
        }
    }
}
static void call_action_cb(lv_event_t *e)
{
    ui_note_interaction(); tirtc_ui_action_type_t type = (tirtc_ui_action_type_t)(uintptr_t)lv_event_get_user_data(e);
    int32_t value = type == TIRTC_ACTION_SET_MIC_ENABLED ? !ui_state.mic_enabled :
                    type == TIRTC_ACTION_SET_VIDEO_ENABLED ? !ui_state.video_enabled : (int32_t)ui_state.call_session_id;
    char generation[16]; snprintf(generation, sizeof(generation), "%lu", (unsigned long)ui_state.call_session_id);
    if (ui_action(type, 0, value, ui_state.peer_name, generation) == 0) {
        if (type == TIRTC_ACTION_SET_MIC_ENABLED) ui_state.mic_enabled = value != 0;
        if (type == TIRTC_ACTION_SET_VIDEO_ENABLED) ui_state.video_enabled = value != 0;
    }
}
static void apply_frame(void)
{
    if (!preview_image) return;
    bool has_frame = frame_w && frame_h && ui_state.call_type == TIRTC_CALL_VIDEO;
    visible(preview_image, has_frame); visible(preview_empty, !has_frame);
    if (!has_frame) return;
    bool geometry_changed = lv_img_get_src(preview_image) != &preview_descriptor ||
        preview_descriptor.header.w != frame_w || preview_descriptor.header.h != frame_h;
    /* The fallback LVGL decoder may retain a pointer into the previous front
     * buffer. Invalidate that cache before changing the descriptor's data. */
    lv_img_cache_invalidate_src(&preview_descriptor);
    preview_descriptor.header.cf = LV_IMG_CF_TRUE_COLOR;
    preview_descriptor.header.w = frame_w; preview_descriptor.header.h = frame_h;
    preview_descriptor.data = (const uint8_t *)frame;
    preview_descriptor.data_size = (uint32_t)frame_w * frame_h * 2;
    if (geometry_changed) {
        lv_img_set_src(preview_image, &preview_descriptor);
        unsigned zoom = 320U * 256 / frame_w, zy = 240U * 256 / frame_h;
        if (zoom > zy) zoom = zy;
        if (zoom > 8192) zoom = 8192;
        lv_img_set_zoom(preview_image, (uint16_t)zoom); lv_obj_center(preview_image);
    }
    lv_obj_invalidate(preview_image);
}
static void render_call(void)
{
    bool video = ui_state.call_type == TIRTC_CALL_VIDEO;
    if (video) {
        lv_obj_t *canvas = ui_panel(ui_screen, 0, 0, 320, 240, false);
        lv_obj_set_style_bg_color(canvas, lv_color_black(), 0); lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
        preview_image = tirtc_video_draw_create(canvas); lv_obj_clear_flag(preview_image, LV_OBJ_FLAG_CLICKABLE);
        /* Remote video alone uses nearest-neighbour scaling. Avoid per-pixel
         * interpolation at 192x144 -> 320x240; UI icons/fonts stay unchanged. */
        lv_img_set_antialias(preview_image, false);
        lv_obj_add_event_cb(preview_image, preview_drawn, LV_EVENT_DRAW_MAIN_END, NULL);
        preview_empty = ui_label(canvas, "等待对方画面", 16, 100, 288, UI_MUTED);
        lv_obj_set_style_text_align(preview_empty, LV_TEXT_ALIGN_CENTER, 0); apply_frame();
    }
    call_title = ui_label(ui_screen, video ? "视频通话" : "语音通话", 14, 10, 190, UI_TEXT);
    header_status(222, 277);
    call_peer = ui_label(ui_screen, "", 16, video ? 42 : 58, 288, UI_TEXT);
    lv_obj_set_style_text_align(call_peer, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(call_peer, LV_LABEL_LONG_DOT); lv_obj_set_height(call_peer, 36);
    call_status = ui_label(ui_screen, "", 16, video ? 70 : 116, 288, UI_ACCENT);
    lv_obj_set_style_text_align(call_status, LV_TEXT_ALIGN_CENTER, 0);
    accept_button = ui_button(ui_screen, "接听", 164, 180, 148, 56, call_action_cb, TIRTC_ACTION_ACCEPT_CALL);
    reject_button = ui_button(ui_screen, "拒绝", 8, 180, 148, 56, call_action_cb, TIRTC_ACTION_REJECT_CALL);
    mute_button = ui_button(ui_screen, "关闭麦克风", 8, 180, video ? 88 : 148, 56, call_action_cb, TIRTC_ACTION_SET_MIC_ENABLED);
    camera_button = ui_button(ui_screen, "关摄像头", 116, 184, 88, 42, call_action_cb, TIRTC_ACTION_SET_VIDEO_ENABLED);
    hangup_button = ui_button(ui_screen, "挂断", video ? 220 : 164, video ? 184 : 180, video ? 88 : 148, video ? 42 : 56, call_action_cb, TIRTC_ACTION_HANGUP);
    if (video) { lv_obj_set_pos(mute_button, 12, 184); lv_obj_set_size(mute_button, 88, 42); }
    lv_obj_set_style_bg_color(accept_button, lv_color_hex(UI_ACCEPT), 0);
    lv_obj_set_style_bg_color(reject_button, lv_color_hex(UI_DANGER), 0);
    lv_obj_set_style_bg_color(hangup_button, lv_color_hex(UI_DANGER), 0);
}
static void bind_cb(lv_event_t *e)
{
    (void)e; ui_note_interaction();
    if (binding_retry_pending || (ui_platform_live && ui_platform.busy)) return;
    if (ui_action(TIRTC_ACTION_BIND, 0, 0, NULL, NULL) < 0) ui_notice("绑定重试未受理，请稍后重试");
    else if (ui_platform_live) { binding_retry_pending = true; binding_retry_tick = lv_tick_get(); }
}
static void render_binding(void)
{
    if (setup_active) (void)ui_label(ui_screen, "绑定设备", 16, 12, 288, UI_TEXT);
    else ui_header("绑定设备", TIRTC_PAGE_MENU);
    binding_network = ui_label(ui_screen, "", 16, 50, 240, UI_ACCENT);
    binding_code = ui_label(ui_screen, "------", 16, 90, 288, UI_TEXT);
    lv_obj_set_style_text_font(binding_code, &tirtc_font_digits_48, 0);
    lv_obj_set_style_text_align(binding_code, LV_TEXT_ALIGN_CENTER, 0);
    (void)ui_label(ui_screen, "在体验平台输入上方 6 位验证码", 16, 166, 288, UI_TEXT);
    binding_status = ui_label(ui_screen, "", 16, 202, 288, UI_MUTED);
    binding_retry = symbol_button(LV_SYMBOL_REFRESH, 268, 44, 44, 40, bind_cb, 0);
}
static void clear_page_pointers(void)
{
    home_phone = home_phone_back = home_phone_icon = home_phone_end = NULL;
    home_phone_remote = -1;
    home_phone_pressed_remote = false; home_phone_pressed_generation = 0;
    clock_label = state_label = caption_panel = caption_label = wake_hint = date_label = clock_face = NULL;
    contact_panel = contact_name = contact_status = contact_id = voice_button = video_button = NULL;
    call_title = call_peer = call_status = accept_button = reject_button = NULL;
    mute_button = camera_button = hangup_button = preview_image = preview_empty = NULL;
    drawn_frame_seq = 0U;
    binding_code = binding_status = binding_network = binding_retry = NULL;
    signal_label = NULL;
    last_signal_bars = last_signal_ready = last_caption_role = -1;
    memset(signal_bars, 0, sizeof(signal_bars));
}
static void render_page(void)
{
    rendered_page_word = 0U;
    page_waiting_flush = page_refresh_failed = page_retry_refresh = false;
    ui_pages_reset(); ui_face_detach();
    if (qr_layer) { lv_obj_del(qr_layer); qr_layer = NULL; }
    lv_obj_clean(ui_screen); clear_page_pointers();
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(UI_BG), 0);
    switch (ui_page) {
    case TIRTC_PAGE_HOME: case TIRTC_PAGE_CLOCK: render_home(); break;
    case TIRTC_PAGE_MENU: render_menu(); break;
    case TIRTC_PAGE_CONTACTS: render_contacts(); break;
    case TIRTC_PAGE_CONTACT_DETAIL: render_contact_detail(); break;
    case TIRTC_PAGE_EXPRESSIONS: render_expressions(); break;
    case TIRTC_PAGE_EXPRESSION_PREVIEW:
        (void)ui_face_background(ui_screen);
        ui_header(expression_names[preview_expression], TIRTC_PAGE_EXPRESSIONS);
        (void)ui_face_create(ui_screen, 50, 48);
        ui_face_set(ui_face_key_at(preview_expression), UI_FACE_IDLE, 1, UI_FACE_VARIANT_AUTO);
        (void)ui_button(ui_screen, "应用到主页", 8, 192, 304, 44, apply_expression_cb, 0); break;
    case TIRTC_PAGE_CALL: render_call(); break;
    case TIRTC_PAGE_CALL_RESULT:
        ui_header("通话结束", TIRTC_PAGE_HOME);
        (void)ui_label(ui_screen, ui_state.call_message[0] ? ui_state.call_message : "通话已结束", 16, 88, 288, UI_TEXT);
        (void)route("返回首页", 8, 192, 304, 44, TIRTC_PAGE_HOME); break;
    case TIRTC_PAGE_BINDING: render_binding(); break;
    default: (void)ui_pages_render(ui_page); break;
    }
    ui_face_pause(asleep || qr_layer != NULL); needs_render = false;
    page_waiting_flush = true;
}
static const char *home_state_text(bool pending)
{
    if (remote_session_active()) return ui_state.remote.state == TIRTC_REMOTE_CLOSING ? "正在结束" :
        ui_state.remote.state == TIRTC_REMOTE_CONNECTING ? "远程连接中" : "远程查看中";
    if (ui_state.ai_status[0]) return ui_state.ai_status;
    if (pending) return "正在连接 AI";
    if (!ui_state.ai_active && ui_state.ai_state == TIRTC_AI_IDLE) return ui_state.mic_enabled ? "点击开始" : "麦克风关闭";
    static const char *const names[] = {"点击开始", "正在聆听", "正在连接 AI", "AI 回复中", "休息中", "暂时不可用"};
    return names[ui_state.ai_state];
}
static const char *home_wake_text(void)
{
    if (!ui_state.mic_enabled && !ui_state.speaker_enabled)
        return "麦克风/扬声器已关闭\n请在设备设置中开启";
    if (!ui_state.mic_enabled)
        return ui_state.volume ? "麦克风已关闭\n请在设备设置中开启" :
                                 "麦克风已关闭，扬声器静音\n请开启麦克风并调高音量";
    if (!ui_state.speaker_enabled)
        return "扬声器已关闭\n可继续对话，回复仅显示字幕";
    if (!ui_state.volume) return "扬声器静音（音量0）\n请调高音量";
    return "点击表情开始对话\n连接后直接说话";
}
static void refresh_home(uint32_t now)
{
    bool remote = remote_session_active();
    bool end_pending = remote && remote_end_generation == ui_state.remote.generation &&
                       (int32_t)(now - remote_end_pending_until) < 0;
    bool pending = !ui_state.ai_active && (int32_t)(now - ai_pending_until) < 0;
    bool idle = !remote && !ui_state.ai_active && ui_state.ai_state == TIRTC_AI_IDLE && !pending;
    text_if_changed(state_label, home_state_text(pending));
    visible(wake_hint, remote || idle); visible(caption_panel, !remote && !idle);
    text_if_changed(wake_hint, remote ?
        (ui_state.remote.state == TIRTC_REMOTE_CLOSING ? "正在结束远程查看\n请稍候" :
         end_pending ? "结束请求已提交\n等待设备确认" :
         ui_state.remote.state == TIRTC_REMOTE_CONNECTING ? "远程连接中\n点击电话按钮结束" :
         "远程查看中\n点击电话按钮结束") : home_wake_text());
    if (home_phone_back && home_phone_remote != (int)remote) {
        lv_obj_set_style_bg_color(home_phone_back, lv_color_hex(remote ? UI_DANGER : 0xFFFFFF), 0);
        visible(home_phone_icon, !remote); visible(home_phone_end, remote);
        home_phone_remote = (int)remote;
    }
    if (home_phone)
        ui_enabled(home_phone, !remote || (ui_state.remote.generation &&
                   ui_state.remote.state != TIRTC_REMOTE_CLOSING && !end_pending));
    if (caption_label && strcmp(caption_cache, ui_state.ai_caption)) {
        ui_copy(caption_cache, sizeof(caption_cache), ui_state.ai_caption);
        lv_label_set_text(caption_label, caption_cache);
        lv_obj_update_layout(caption_label); lv_obj_align(caption_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    if (caption_label && last_caption_role != (int)ui_state.ai_caption_is_ai) {
        last_caption_role = ui_state.ai_caption_is_ai;
        lv_obj_set_style_text_color(caption_label, lv_color_hex(ui_state.ai_caption_is_ai ? 0xB5DFFF : 0xBCE8CF), 0);
    }
    if (ui_page == TIRTC_PAGE_HOME) {
        const char *key = (!remote && !idle && ui_state.ai_emotion[0]) ? ui_state.ai_emotion : ui_face_key_at(ui_state.expression_index);
        ui_face_set(key, remote ? UI_FACE_IDLE : pending ? UI_FACE_THINKING : (uint8_t)ui_state.ai_state,
                    remote ? 0 : ui_state.audio_level, ui_state.expression_variant);
    }
    text_if_changed(clock_face, display_clock());
    text_if_changed(date_label, display_date());
}
static void refresh_dynamic(uint32_t now, bool changed)
{
    text_if_changed(clock_label, display_clock());
    bool signal_ready = ui_state.network_status_valid && ui_state.network_connected &&
        ui_state.sim_state == TIRTC_SIM_READY &&
        (ui_state.registration == TIRTC_REG_HOME || ui_state.registration == TIRTC_REG_ROAMING) &&
        ui_state.signal_valid;
    unsigned bars = signal_ready ? (ui_state.signal_bars > 4U ? 4U : ui_state.signal_bars) : 0U;
    if (last_signal_bars != (int)bars) {
        last_signal_bars = (int)bars;
        for (unsigned i = 0; i < 4; i++) if (signal_bars[i])
            lv_obj_set_style_bg_color(signal_bars[i], lv_color_hex(i < bars ? UI_ACCENT : 0x29424C), 0);
    }
    if (last_signal_ready != (int)signal_ready) {
        last_signal_ready = (int)signal_ready;
        if (signal_label) lv_obj_set_style_text_color(signal_label,
            lv_color_hex(signal_ready ? UI_ACCENT : UI_MUTED), 0);
    }
    if (ui_page == TIRTC_PAGE_HOME || ui_page == TIRTC_PAGE_CLOCK) refresh_home(now);
    if (ui_page == TIRTC_PAGE_CONTACTS && contacts_changed && !ui_pointer_busy()) fill_contacts();
    if (ui_page == TIRTC_PAGE_CONTACT_DETAIL) {
        int i = selected_contact(); const tirtc_ui_contact_t *c = i >= 0 ? &ui_state.contacts[i] : NULL;
        text_if_changed(contact_name, c ? (c->name[0] ? c->name : c->id) : "联系人已更新，请返回重试");
        text_if_changed(contact_status, c ? (c->wechat ? "微信联系人" : c->online ? "设备在线" : "设备离线") : "");
        text_if_changed(contact_id, c ? (c->wechat ? "微信联系人" : c->id) : "");
        bool callable=(!contacts_live || contacts_available) && c && (c->online || c->wechat);
        ui_enabled(voice_button, callable); ui_enabled(video_button, callable);
    }
    if (ui_page == TIRTC_PAGE_CALL) {
        bool incoming = ui_state.call_state == TIRTC_CALL_INCOMING;
        bool active = ui_state.call_state == TIRTC_CALL_CONNECTED;
        bool live = ui_state.call_state == TIRTC_CALL_OUTGOING || ui_state.call_state == TIRTC_CALL_CONNECTING || active || incoming;
        bool video = ui_state.call_type == TIRTC_CALL_VIDEO;
        char status[160];
        if (ui_state.call_message[0]) ui_copy(status, sizeof(status), ui_state.call_message);
        else if (active) snprintf(status, sizeof(status), "通话中  %02lu:%02lu", (unsigned long)ui_state.call_seconds / 60, (unsigned long)ui_state.call_seconds % 60);
        else ui_copy(status, sizeof(status), incoming ? "来电" : live ? "正在建立通话" : "当前没有通话");
        text_if_changed(call_peer, ui_state.peer_name[0] ? ui_state.peer_name : "未知联系人"); text_if_changed(call_status, status);
        visible(accept_button, incoming); visible(reject_button, incoming);
        visible(mute_button, active); visible(camera_button, active && video); visible(hangup_button, !incoming);
        ui_enabled(hangup_button, live);
        text_if_changed(lv_obj_get_child(mute_button, 0), ui_state.mic_enabled ? (video ? "关麦" : "关闭麦克风") : (video ? "开麦" : "开启麦克风"));
        text_if_changed(lv_obj_get_child(camera_button, 0), ui_state.video_enabled ? "关摄像头" : "开摄像头");
        text_if_changed(lv_obj_get_child(hangup_button, 0), active ? "挂断" : "取消");
    }
    if (ui_page == TIRTC_PAGE_BINDING) {
        text_if_changed(binding_code, ui_state.binding_code[0] ? ui_state.binding_code : "------");
        text_if_changed(binding_network, ui_state.network_status_valid && ui_state.network_connected ? "移动网络已连接" : "请先连接移动网络");
        const char *message = ui_state.binding_message;
        if (ui_platform_live && ui_platform.binding_state == TIRTC_BINDING_WAITING_USER && !ui_platform.code[0])
            message = "验证码已过期，请重试";
        else if (binding_retry_pending) message = "重试已提交，等待处理";
        else if (!message[0]) message = ui_state.bound ? "设备已绑定" :
            ui_state.binding_state == TIRTC_BINDING_CHECKING ? "正在检查设备配置…" :
            ui_state.binding_state == TIRTC_BINDING_ERROR ? "绑定未完成，请点击右上角重试" :
            ui_state.binding_code[0] ? "等待绑定，完成后进入主页" : "等待绑定服务提供验证码";
        text_if_changed(binding_status, message);
        bool retry = !ui_platform_live || (ui_platform.known && ui_platform.enabled &&
            (ui_platform.binding_state == TIRTC_BINDING_ERROR ||
             (ui_platform.binding_state == TIRTC_BINDING_WAITING_USER && !ui_platform.code[0])));
        visible(binding_retry, retry);
        ui_enabled(binding_retry, !binding_retry_pending && (!ui_platform_live || !ui_platform.busy));
    }
    ui_pages_process(changed, now); contacts_changed = false;
}
static void wake_cb(lv_event_t *e) { (void)e; wake_requested = true; }
static void update_sleep(void)
{
    /* Page construction and callbacks can record a newer last_interaction.
     * Sample after that work: an earlier loop timestamp would underflow the
     * unsigned elapsed time and turn a single tap into an immediate sleep.
     * Both timestamps belong to this UI task; subtraction remains wrap-safe. */
    uint32_t now = lv_tick_get();
    if (wake_requested) {
        wake_requested = false; asleep = false; last_interaction = now;
        if (sleep_layer) { lv_obj_del(sleep_layer); sleep_layer = NULL; }
        tirtc_port_set_brightness(100); ui_face_pause(qr_layer != NULL);
    }
    bool active = setup_active || ui_state.ai_active || ui_state.room.connected || remote_session_active() ||
        (ui_state.call_state >= TIRTC_CALL_OUTGOING && ui_state.call_state <= TIRTC_CALL_CONNECTED);
    if (active || qr_layer || ui_pointer_busy()) last_interaction = now;
    if (!asleep && !active && !qr_layer && ui_state.sleep_minutes &&
        now - last_interaction >= (uint32_t)ui_state.sleep_minutes * 60000U && !ui_pointer_busy()) {
        sleep_layer = ui_panel(lv_layer_top(), 0, 0, 320, 240, false);
        lv_obj_set_style_bg_color(sleep_layer, lv_color_black(), 0); lv_obj_set_style_bg_opa(sleep_layer, LV_OPA_COVER, 0);
        lv_obj_add_event_cb(sleep_layer, wake_cb, LV_EVENT_CLICKED, NULL);
        asleep = true; ui_face_pause(true); tirtc_port_set_brightness(0);
    }
}
void tirtc_ui_init(void)
{
    if (ready) return;
    tirtc_ui_state_defaults(&ui_state); call_tick = last_interaction = lv_tick_get();
    tirtc_ui_platform_lock(); (void)apply_audio_settings(); tirtc_ui_platform_unlock();
    ui_screen = lv_obj_create(NULL); lv_obj_remove_style_all(ui_screen);
    lv_obj_set_size(ui_screen, 320, 240); lv_obj_set_style_bg_opa(ui_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE); lv_scr_load(ui_screen);
    ready = true;
#ifdef TIRTC_EXTERNAL_UI_ASSETS
    /* Only built-in Latin glyphs are used until verified external resources
     * are resident. SPI/file I/O belongs to the resource worker, never LVGL. */
    asset_boot_label = lv_label_create(ui_screen);
    lv_obj_set_style_text_font(asset_boot_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(asset_boot_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0x1C1F22), 0);
    lv_label_set_text(asset_boot_label, "Loading UI resources...");
    lv_obj_align(asset_boot_label, LV_ALIGN_CENTER, 0, 0);
#else
    render_page(); refresh_dynamic(lv_tick_get(), true);
#endif
}
void tirtc_ui_process(void)
{
    if (!ready) return;
#ifdef TIRTC_EXTERNAL_UI_ASSETS
    if (!external_ui_attached) {
        if (!tirtc_assets_ready()) {
            tirtc_assets_snapshot_t assets;
            tirtc_assets_get_snapshot(&assets);
            if (assets.state == TIRTC_ASSETS_RETRY_WAIT)
                text_if_changed(asset_boot_label, "UI resource read failed.\nRetrying automatically...");
            else if (assets.state == TIRTC_ASSETS_ERROR)
                text_if_changed(asset_boot_label, "UI resources unavailable.\nCheck COM9 log / asset installer.");
            else text_if_changed(asset_boot_label, "Loading UI resources...");
            return;
        }
        if (tirtc_font_set_reader(tirtc_assets_glyph, TIRTC_FONT_BYTES) != 0 ||
            ui_face_set_background(tirtc_assets_background(), TIRTC_BACKGROUND_BYTES) != 0) return;
        external_ui_attached = true; asset_boot_label = NULL;
        render_page(); refresh_dynamic(lv_tick_get(), true);
    }
    /* The sole LVGL owner commits completed glyphs before lv_timer_handler.
     * Even a static page redraws once per completed batch; misses alone never
     * invalidate, so a slow/failed Flash read cannot cause a busy redraw loop. */
    if (tirtc_assets_font_process()) lv_obj_invalidate(ui_screen);
#endif
    /* A failed first-page flush is never ready. Retry the whole page from
     * the GUI loop, since invalidating during LVGL's flush is not allowed. */
    if (page_retry_refresh) {
        page_retry_refresh = false;
        lv_obj_invalidate(ui_screen);
    }
    bool busy = ui_pointer_busy(), changed = false, frame_changed = false, new_session = false;
    bool call_cleanup_pending = false;
    bool call_changed = false, call_type_changed = false, cancel_ptt = false; int page = -1, volume_delta = 0;
    char notice[128] = ""; uint32_t now = lv_tick_get();
    tirtc_ui_platform_lock();
    /* Safety releases must not wait for a held finger to release. Widget and
     * ordinary state replacement remain deferred until the pointer is idle. */
    if (busy && !room_live && state_dirty && ui_page == TIRTC_PAGE_ROOM &&
        (!pending_state.room.known || !pending_state.room.connected || !pending_state.room.assigned || pending_state.room.busy ||
         !pending_state.mic_enabled || pending_state.mic_gain == 0 || pending_state.room.generation != ui_state.room.generation ||
         strcmp(pending_state.room.code, ui_state.room.code))) cancel_ptt = true;
    if (busy && room_live && ui_page == TIRTC_PAGE_ROOM &&
        (!pending_room.known || !pending_room.assigned || !pending_room.connected || pending_room.busy ||
         (audio_settings_live && (!settings_mic_enabled || settings_mic_gain == 0)) || pending_room.generation != ui_state.room.generation ||
         strcmp(pending_room.code, ui_state.room.code))) cancel_ptt = true;
    if (!busy && state_dirty) {
        call_changed = ui_state.call_state != pending_state.call_state;
        call_type_changed = ui_state.call_type != pending_state.call_type;
        new_session = call_type_changed || ui_state.call_session_id != pending_state.call_session_id || strcmp(ui_state.peer_name, pending_state.peer_name);
        contacts_changed = ui_state.contact_count != pending_state.contact_count ||
            memcmp(ui_state.contacts, pending_state.contacts, sizeof(ui_state.contacts)) ||
            ui_state.contacts_loading != pending_state.contacts_loading || strcmp(ui_state.contacts_message, pending_state.contacts_message);
        uint32_t seconds = ui_state.call_seconds;
        bool clock_reset = call_changed || new_session || pending_state.call_seconds > seconds;
        tirtc_ui_remote_t remote_snapshot = ui_state.remote;
        ui_state = pending_state;
        if (remote_live) ui_state.remote = remote_snapshot;
        state_dirty = false; changed = true;
        if (clock_reset) call_tick = now;
        else if (ui_state.call_state == TIRTC_CALL_CONNECTED) ui_state.call_seconds = seconds;
        if (ui_state.ai_active || ui_state.call_state == TIRTC_CALL_INCOMING || ui_state.audio_level) wake_requested = asleep;
        if (new_session || (call_changed && (ui_state.call_state == TIRTC_CALL_IDLE || ui_state.call_state >= TIRTC_CALL_ENDED))) {
            frame_w = frame_h = 0; frame_dirty = false; frame_changed = true;
        }
    }
    if (!busy) {
        if (apply_contacts()) changed=true;
        if (call_live) {
            call_cleanup_pending = pending_call.state == TIRTC_CALL_ERROR &&
                pending_call.error == TIRTC_CALL_CLEANUP_PENDING;
            bool state_diff=ui_state.call_state!=pending_call.state;
            bool type_diff=ui_state.call_type!=pending_call.type;
            bool session_diff=type_diff || ui_state.call_session_id!=pending_call.session_id;
            bool text_diff=strcmp(ui_state.peer_name,pending_call.peer_name) || strcmp(ui_state.call_message,pending_call.message);
            call_changed=call_changed || state_diff;
            call_type_changed=call_type_changed || type_diff;
            new_session=new_session || session_diff;
            if (session_diff || state_diff || pending_call.seconds>ui_state.call_seconds) {
                ui_state.call_seconds=pending_call.seconds; call_tick=now;
            }
            ui_state.call_state=pending_call.state; ui_state.call_type=pending_call.type;
            ui_state.call_session_id=pending_call.session_id;
            ui_state.video_enabled=pending_call.video_enabled;
            ui_copy(ui_state.peer_name,sizeof(ui_state.peer_name),pending_call.peer_name);
            ui_copy(ui_state.call_message,sizeof(ui_state.call_message),pending_call.message);
            if (state_diff || session_diff || text_diff) changed=true;
            if (session_diff || (state_diff && (pending_call.state==TIRTC_CALL_IDLE || pending_call.state>=TIRTC_CALL_ENDED))) {
                frame_w=frame_h=0; frame_dirty=false; frame_changed=true;
                }
            if (pending_call.state==TIRTC_CALL_INCOMING) wake_requested=asleep;
        }
        if (apply_ai()) changed = true;
        if (apply_audio_settings()) changed = true;
        volume_delta = pending_volume_delta; pending_volume_delta = 0;
        if (apply_network()) changed = true;
        if (resources_dirty) {
            ui_resources = pending_resources; resources_dirty = false; changed = true;
        }
        if (system_dirty) {
            bool different = !ui_system_live || memcmp(&ui_system, &pending_system, sizeof(ui_system)) != 0;
            ui_system = pending_system; system_dirty = false; ui_system_live = true;
            if (different) changed = true;
        }
        if (platform_dirty) {
            bool different = !ui_platform_live || memcmp(&ui_platform, &pending_platform, sizeof(ui_platform)) != 0;
            ui_platform = pending_platform; platform_dirty = false; ui_platform_live = true;
            platform_code_tick = pending_platform_tick; binding_retry_pending = false;
            if (different) changed = true;
        }
        if (ui_platform_live && ui_platform.code[0] &&
            lv_tick_get() - platform_code_tick >= (uint32_t)ui_platform.seconds_left * 1000U) {
            ui_platform.code[0] = 0; ui_platform.seconds_left = 0; changed = true;
        }
        if (binding_retry_pending && now - binding_retry_tick >= 1500U) binding_retry_pending = false;
        if (apply_platform()) changed = true;
        page = pending_page; pending_page = -1;
    }
    /* Privacy status and generation must be current even under a held touch.
     * Remote widget rebuilds/refreshes still wait for pointer release. */
    if (apply_remote()) changed = true;
    /* Replace only room data while held, never widgets: disconnect and room
     * changes must revoke PTT before another touch heartbeat can be accepted. */
    if (apply_room()) changed = true;
    /* Pixel replacement does not delete objects. Keep a stable video call
     * live under a held touch, while deferring clear/page/session changes
     * until LVGL has finished dispatching that pointer interaction. */
    bool stable_video_touch = pending_w && pending_h && !state_dirty && pending_page < 0 &&
        !needs_render && ui_page == TIRTC_PAGE_CALL &&
        ui_state.call_state == TIRTC_CALL_CONNECTED && ui_state.call_type == TIRTC_CALL_VIDEO &&
        (!call_live || (pending_call.state == ui_state.call_state && pending_call.type == ui_state.call_type &&
                       pending_call.session_id == ui_state.call_session_id));
    if (frame_dirty && (!busy || stable_video_touch)) {
        frame_w = pending_w; frame_h = pending_h;
        if (frame_w && frame_h) {
            /* Only this GUI task reads frame. Producers copy into pending_frame
             * under the same lock, so neither can overwrite a displayed frame. */
            uint16_t *old_frame = frame;
            frame = pending_frame;
            pending_frame = old_frame;
            frame_seq = pending_frame_seq;
            ++video_stats.consumed;
        }
        frame_dirty = false; frame_changed = true;
    }
    if (notice_dirty) { ui_copy(notice, sizeof(notice), pending_notice); notice_dirty = false; }
    tirtc_ui_platform_unlock();
    if (volume_delta) {
        int volume = (int)ui_state.volume + volume_delta;
        char volume_text[64];
        if (volume < 0) volume = 0;
        if (volume > 10) volume = 10;
        ui_note_interaction(); wake_requested = asleep;
        if (volume == ui_state.volume || ui_action(TIRTC_ACTION_SET_SPEAKER_VOLUME, 0, volume, NULL, NULL) == 0) {
            ui_state.volume = (uint8_t)volume; changed = true;
            (void)snprintf(volume_text, sizeof(volume_text), ui_state.speaker_enabled ? "音量  %u / 10" : "音量  %u / 10（扬声器已关闭）", (unsigned)volume);
            ui_notice(volume_text);
        } else ui_notice("音量设置未受理，请重试");
    }
    if (cancel_ptt) ui_pages_cancel_ptt();
    /* Remote VIEW stays on HOME/CLOCK. A previous QR overlay must not hide
     * the privacy status; defer deletion until the pointer is released. */
    if (!busy && remote_session_active() && qr_layer) close_qr_cb(NULL);
    if (call_changed && ui_state.call_state >= TIRTC_CALL_OUTGOING && ui_state.call_state <= TIRTC_CALL_CONNECTED) {
        if (ui_page != TIRTC_PAGE_CALL)
            return_to_room_after_call = ui_page == TIRTC_PAGE_ROOM || ui_page == TIRTC_PAGE_ROOM_FORM;
        page = TIRTC_PAGE_CALL;
    }
    if (call_changed && (ui_state.call_state == TIRTC_CALL_ENDED || ui_state.call_state == TIRTC_CALL_BUSY || ui_state.call_state == TIRTC_CALL_ERROR) && ui_page == TIRTC_PAGE_CALL) {
        page = call_cleanup_pending ? TIRTC_PAGE_HOME :
            (return_to_room_after_call ? TIRTC_PAGE_ROOM : TIRTC_PAGE_CALL_RESULT);
        if (call_cleanup_pending) ui_notice(ui_state.call_message);
        return_to_room_after_call = false;
    }
    if (!busy) page = setup_target(page);
    if (page >= 0 && page != (int)ui_page && !busy && ai_live &&
        (ui_state.ai_active || (int32_t)(now - ai_pending_until) < 0) &&
        page != TIRTC_PAGE_HOME && page != TIRTC_PAGE_CLOCK &&
        page != TIRTC_PAGE_MENU && page != TIRTC_PAGE_DIAGNOSTICS) {
        if (ui_action(TIRTC_ACTION_AI_STOP, 0, 0, NULL, NULL) < 0) {
            page = -1; ui_notice("对讲正在停止，请稍后重试");
        }
    }
    if (page >= 0 && page != (int)ui_page && !busy) {
        tirtc_ui_page_t previous = ui_page;
        ui_pages_navigation(previous, (tirtc_ui_page_t)page);
        ui_page = (tirtc_ui_page_t)page;
        if (previous == TIRTC_PAGE_CALL && ui_page != TIRTC_PAGE_CALL) {
            return_to_room_after_call = false;
            frame_w=frame_h=0;
        }
        render_page(); ui_note_interaction();
        (void)ui_action(ui_page == TIRTC_PAGE_HOME ? TIRTC_ACTION_RETURN_HOME : TIRTC_ACTION_ENTER_PAGE, 0, ui_page, NULL, NULL);
    } else if ((needs_render || (call_type_changed && ui_page == TIRTC_PAGE_CALL)) && !busy) render_page();
    /* Rendering may advance the tick and initialise page-local timestamps. */
    now = lv_tick_get();
    if (ui_state.call_state == TIRTC_CALL_CONNECTED) {
        uint32_t seconds = (now - call_tick) / 1000;
        ui_state.call_seconds += seconds; call_tick += seconds * 1000;
    } else call_tick = now;
    if (frame_changed) apply_frame();
    refresh_dynamic(now, changed);
    if (notice[0]) ui_notice(notice);
    if (toast && (int32_t)(now - toast_until) >= 0) visible(toast, false);
    update_sleep();
}
