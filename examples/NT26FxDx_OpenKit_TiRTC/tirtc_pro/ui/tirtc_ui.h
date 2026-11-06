/* Modern XiaoTai product presentation, rebuilt for Lierda V04. */
#ifndef TIRTC_UI_H
#define TIRTC_UI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TIRTC_UI_WIDTH 320
#define TIRTC_UI_HEIGHT 240
#define TIRTC_UI_CONTACT_MAX 12
#define TIRTC_UI_ROOM_MEMBER_MAX 12
#define TIRTC_UI_CAPTION_MAX 4096
/* Recoverable call teardown warning; media remains reserved until drained. */
#define TIRTC_CALL_CLEANUP_PENDING (-6304)
typedef enum {
 TIRTC_PAGE_HOME, TIRTC_PAGE_CLOCK, TIRTC_PAGE_MENU, TIRTC_PAGE_CONTACTS,
 TIRTC_PAGE_CONTACT_DETAIL, TIRTC_PAGE_EXPRESSIONS, TIRTC_PAGE_EXPRESSION_PREVIEW,
 TIRTC_PAGE_SETTINGS, TIRTC_PAGE_NETWORK, TIRTC_PAGE_BINDING, TIRTC_PAGE_CALL,
 TIRTC_PAGE_CALL_RESULT, TIRTC_PAGE_DIAGNOSTICS, TIRTC_PAGE_ROOM,
 TIRTC_PAGE_ROOM_FORM, TIRTC_PAGE_REMOTE, TIRTC_PAGE_COUNT
} tirtc_ui_page_t;
typedef enum {
 TIRTC_ACTION_NONE, TIRTC_ACTION_ENTER_PAGE, TIRTC_ACTION_RETURN_HOME,
 TIRTC_ACTION_NETWORK_REFRESH, TIRTC_ACTION_NETWORK_RECONNECT,
 TIRTC_ACTION_AI_START, TIRTC_ACTION_AI_STOP,
 TIRTC_ACTION_REFRESH_CONTACTS, TIRTC_ACTION_CALL_AUDIO, TIRTC_ACTION_CALL_VIDEO,
 TIRTC_ACTION_ACCEPT_CALL, TIRTC_ACTION_REJECT_CALL, TIRTC_ACTION_HANGUP,
 TIRTC_ACTION_SET_SPEAKER_VOLUME, TIRTC_ACTION_SET_SPEAKER_ENABLED,
 TIRTC_ACTION_SET_MIC_ENABLED, TIRTC_ACTION_SET_VIDEO_ENABLED,
 TIRTC_ACTION_SET_SLEEP_MINUTES, TIRTC_ACTION_SET_ACK_VOICE,
 TIRTC_ACTION_SET_IDLE_EXPRESSION, TIRTC_ACTION_BIND,
 TIRTC_ACTION_ROOM_OPEN, TIRTC_ACTION_ROOM_REFRESH, TIRTC_ACTION_ROOM_CREATE,
 TIRTC_ACTION_ROOM_JOIN, TIRTC_ACTION_ROOM_LEAVE, TIRTC_ACTION_ROOM_PTT,
 TIRTC_ACTION_SET_MIC_GAIN,
 TIRTC_ACTION_REMOTE_END, TIRTC_ACTION_REMOTE_SET_MIC,
 TIRTC_ACTION_REMOTE_SET_SPEAKER, TIRTC_ACTION_REMOTE_SET_CAMERA
} tirtc_ui_action_type_t;
typedef enum { TIRTC_CALL_IDLE, TIRTC_CALL_OUTGOING, TIRTC_CALL_INCOMING,
 TIRTC_CALL_CONNECTING, TIRTC_CALL_CONNECTED, TIRTC_CALL_ENDED,
 TIRTC_CALL_ERROR, TIRTC_CALL_CLOSING, TIRTC_CALL_BUSY } tirtc_ui_call_state_t;
typedef enum { TIRTC_CALL_AUDIO, TIRTC_CALL_VIDEO } tirtc_ui_call_type_t;
typedef enum { TIRTC_SIM_UNKNOWN, TIRTC_SIM_READY, TIRTC_SIM_ABSENT,
 TIRTC_SIM_PIN_REQUIRED, TIRTC_SIM_PUK_REQUIRED, TIRTC_SIM_ERROR } tirtc_ui_sim_state_t;
typedef enum { TIRTC_REG_UNKNOWN, TIRTC_REG_NOT_REGISTERED, TIRTC_REG_SEARCHING,
 TIRTC_REG_HOME, TIRTC_REG_ROAMING, TIRTC_REG_DENIED } tirtc_ui_registration_t;
typedef enum { TIRTC_AI_IDLE, TIRTC_AI_LISTENING, TIRTC_AI_THINKING,
 TIRTC_AI_SPEAKING, TIRTC_AI_RESTING, TIRTC_AI_ERROR } tirtc_ui_ai_state_t;

/* Media owner publishes only its fields; network, contacts and local
 * presentation preferences remain owned by their existing services. */
typedef struct {
 bool ai_active, speaker_enabled, mic_enabled, audio_valid;
 tirtc_ui_ai_state_t ai_state;
 uint8_t volume, mic_gain, audio_level;
 char ai_status[96], ai_emotion[24];
 char ai_caption[TIRTC_UI_CAPTION_MAX], ai_utterance_id[64];
 uint32_t ai_revision, ai_paragraph;
 bool ai_caption_is_ai, ai_caption_final, ai_caption_truncated;
 char diagnostic_audio[256];
} tirtc_ui_ai_t;
typedef enum { TIRTC_BINDING_DISABLED, TIRTC_BINDING_CHECKING,
 TIRTC_BINDING_WAITING_USER, TIRTC_BINDING_BOUND, TIRTC_BINDING_ERROR } tirtc_ui_binding_state_t;
/* Kept independent from business snapshots; the cellular worker owns these fields. */
typedef struct {
 bool network_status_valid, network_connected, network_connecting, network_roaming, signal_valid;
 tirtc_ui_sim_state_t sim_state;
 tirtc_ui_registration_t registration;
 int16_t signal_dbm; /* LTE RSRP, encoded modem index minus 141. */
 char operator_name[48], network_type[16], ip_address[40], apn[64], network_message[96];
 bool signal_rsrq_valid, signal_snr_valid, signal_rssi_valid;
 int16_t signal_rsrq_x2, signal_snr_db, signal_rssi_dbm;
 uint8_t signal_bars; /* Cellular owner quality, 0..4; never inferred from RSSI. */
} tirtc_ui_network_t;

/* Independent platform owner. No credentials or tokens belong in this snapshot.
 * seconds_left is the code's remaining lifetime at publication; zero is expired. */
typedef struct {
 bool known, enabled, api_ready, mqtt_connected, rtc_enabled, rtc_ready, busy;
 bool binding_required; /* Authoritative unbound/first setup, not a transient reconnect. */
 tirtc_ui_binding_state_t binding_state;
 char code[16], message[128], device_id[64];
 int32_t error;
 uint16_t seconds_left;
} tirtc_ui_platform_t;

typedef enum {
 TIRTC_STORAGE_UNKNOWN, TIRTC_STORAGE_INITIALIZING, TIRTC_STORAGE_READY,
 TIRTC_STORAGE_UNRECOGNIZED, TIRTC_STORAGE_UNSUPPORTED, TIRTC_STORAGE_ERROR,
 TIRTC_STORAGE_READ_ONLY
} tirtc_ui_storage_state_t;

/* Bytes, not KiB. Independent of business snapshots; sampled by the app worker.
 * Flash is the linked APP region, heap is the same pool as liot_rtos_malloc.
 * Static RAM and heap describe different allocations and must not be summed. */
typedef struct {
 bool flash_valid, static_ram_valid, heap_valid, heap_min_valid, heap_max_valid;
 bool filesystem_valid, uptime_valid;
 uint32_t flash_used_bytes, flash_total_bytes;
 uint32_t static_ram_used_bytes, static_ram_total_bytes;
 uint32_t heap_total_bytes, heap_free_bytes, heap_min_free_bytes, heap_max_free_block_bytes;
 uint32_t filesystem_free_bytes, filesystem_total_bytes, uptime_seconds;
 /* Slow storage workers publish cached snapshots; RAM never waits for I/O. */
 bool filesystem_pending, external_flash_valid, external_filesystem_valid;
 uint32_t external_flash_total_bytes, external_filesystem_total_bytes;
 uint32_t external_filesystem_free_bytes, external_jedec_id;
 int32_t filesystem_error, external_error;
 tirtc_ui_storage_state_t external_state;
 uint32_t sample_sequence, filesystem_sample_uptime_seconds, external_sample_uptime_seconds;
} tirtc_ui_resources_t;
/* Board-owned facts, independent from cloud/business state and resource bytes.
 * Strings are complete UTF-8 values. Publish one complete copy per update. */
typedef struct {
 bool clock_valid, modem_imei_valid, modem_version_valid, audio_valid;
 char clock_text[16], date_text[48], modem_imei[24], modem_version[64];
 char diagnostic_audio[256], diagnostic_events[768];
} tirtc_ui_system_t;
typedef struct { char id[96], name[64]; bool online, wechat; } tirtc_ui_contact_t;

/* Separate copy-in owners: contacts/calls must never overwrite AI, cellular,
 * preferences or another service's snapshot with a stale whole-page copy. */
typedef struct {
 bool known, loading, stale;
 uint8_t count;
 tirtc_ui_contact_t contacts[TIRTC_UI_CONTACT_MAX];
 char message[128];
} tirtc_ui_contacts_t;
typedef struct {
 tirtc_ui_call_state_t state;
 tirtc_ui_call_type_t type;
 uint32_t session_id, seconds, revision;
 bool incoming, wechat, video_enabled;
 int error;
 char peer_id[96], peer_name[96], message[128];
} tirtc_ui_call_t;

typedef enum {
 TIRTC_REMOTE_IDLE, TIRTC_REMOTE_CONNECTING, TIRTC_REMOTE_ACTIVE,
 TIRTC_REMOTE_CLOSING, TIRTC_REMOTE_ENDED, TIRTC_REMOTE_ERROR
} tirtc_ui_remote_state_t;
/* Independent remote-session owner. Flags describe effective session controls,
 * not persisted global audio preferences. Actions carry generation in extra. */
typedef struct {
 tirtc_ui_remote_state_t state;
 uint32_t generation, seconds, revision;
 bool mic_enabled, speaker_enabled, camera_enabled;
 bool video_subscribed, audio_subscribed, talkback_active;
 int error;
 char peer_name[96], message[128];
} tirtc_ui_remote_t;
typedef struct { char id[64], name[64]; bool self, speaking; } tirtc_ui_room_member_t;
typedef struct {
 bool known, assigned, connected, busy, mic_on, members_limited;
 char code[8], message[128];
 uint32_t generation;
 uint8_t member_count;
 tirtc_ui_room_member_t members[TIRTC_UI_ROOM_MEMBER_MAX];
} tirtc_ui_room_t;
typedef struct {
 bool network_status_valid, network_connected, network_connecting, network_roaming, signal_valid;
 tirtc_ui_sim_state_t sim_state;
 tirtc_ui_registration_t registration;
 int16_t signal_dbm; /* LTE RSRP, encoded modem index minus 141. */
 char operator_name[48], network_type[16], ip_address[40], apn[64], network_message[96];
 bool rtc_connected, mqtt_connected, bound, contacts_loading;
 char device_id[64], version[32], protocol_version[32], status_text[128];
 char clock_text[16], date_text[48];
 uint32_t uptime_seconds;
 bool memory_valid;
 uint32_t memory_free_kb;
 uint8_t volume, mic_gain; /* Independent 0..10 levels; zero is muted. */
 bool speaker_enabled, mic_enabled, video_enabled, acknowledgement_male;
 uint16_t sleep_minutes; /* 0 = never, otherwise 1/5/10/30. Local preference. */
 uint8_t expression_index, expression_variant;
 tirtc_ui_ai_state_t ai_state;
 bool ai_active;
 uint8_t audio_level;
 char ai_status[96], ai_emotion[24];
 /* Latest complete caption, not a fixed 192-byte transcript fragment. */
 char ai_caption[TIRTC_UI_CAPTION_MAX], ai_utterance_id[64];
 uint32_t ai_revision, ai_paragraph;
 bool ai_caption_is_ai, ai_caption_final, ai_caption_truncated;
 uint8_t contact_count;
 tirtc_ui_contact_t contacts[TIRTC_UI_CONTACT_MAX];
 char contacts_message[96];
 tirtc_ui_call_state_t call_state;
 tirtc_ui_call_type_t call_type;
 uint32_t call_session_id, call_seconds;
 char peer_name[96], call_message[128];
 bool video_stats_valid;
 uint32_t tx_fps, rx_fps, tx_kbps, rx_kbps;
 tirtc_ui_binding_state_t binding_state;
 char binding_code[16], binding_message[128];
 char diagnostic_system[512], diagnostic_audio[512], diagnostic_events[1024];
 tirtc_ui_room_t room;
 bool signal_rsrq_valid, signal_snr_valid, signal_rssi_valid;
 int16_t signal_rsrq_x2, signal_snr_db, signal_rssi_dbm;
 uint8_t signal_bars;
 tirtc_ui_remote_t remote;
} tirtc_ui_state_t;
typedef struct {
 tirtc_ui_action_type_t type;
 tirtc_ui_page_t page;
 uint8_t index;
 int32_t value;
 char text[257], extra[128];
} tirtc_ui_action_t;
/* UI-task callback: copy the request to a queue and return immediately.
 * 0 = accepted, negative = unavailable. No I/O or LVGL calls from worker tasks. */
typedef int (*tirtc_ui_action_cb_t)(const tirtc_ui_action_t *, void *);
void tirtc_ui_set_backend(tirtc_ui_action_cb_t callback, void *context);
void tirtc_ui_state_defaults(tirtc_ui_state_t *state);
int tirtc_ui_publish_state(const tirtc_ui_state_t *state);
int tirtc_ui_publish_ai(const tirtc_ui_ai_t *state);
int tirtc_ui_publish_contacts(const tirtc_ui_contacts_t *state);
int tirtc_ui_publish_call(const tirtc_ui_call_t *state);
int tirtc_ui_publish_remote(const tirtc_ui_remote_t *state);
/* Group owner only; copies bounded, secret-free room state into its own mailbox. */
int tirtc_ui_publish_room(const tirtc_ui_room_t *state);
/* Nonblocking RAM-only admission snapshot; never calls LVGL or waits on a
 * mutex. False until a real page has completed a successful LCD refresh,
 * including resource loading/failure and page rebuild. NULL returns false. */
bool tirtc_ui_get_rendered_page(tirtc_ui_page_t *out);
/* Independent preference owner: avoids stale media snapshots undoing a tap. */
int tirtc_ui_publish_audio_settings(uint8_t volume, uint8_t mic_gain,
                                  bool speaker_enabled, bool mic_enabled);
/* Physical keys enqueue deltas; the UI task serializes them with touch edits. */
int tirtc_ui_request_volume_delta(int delta);
int tirtc_ui_publish_network(const tirtc_ui_network_t *state);
int tirtc_ui_publish_resources(const tirtc_ui_resources_t *resources);
int tirtc_ui_publish_system(const tirtc_ui_system_t *system);
int tirtc_ui_publish_platform(const tirtc_ui_platform_t *platform);
int tirtc_ui_request_page(tirtc_ui_page_t page);
int tirtc_ui_notify(const char *message);
/* Copy-in, little-endian RGB565; maximum 208x144. NULL,0,0 clears the frame. */
int tirtc_ui_publish_frame(const uint16_t *pixels, uint16_t width, uint16_t height);
/* GUI-task diagnostics. Counters wrap naturally; subtract snapshots unsigned.
 * lcd_frames counts distinct video frames drawn in a refresh whose synchronous
 * LCD transfers all succeeded, not publications or physical panel scanouts. */
typedef struct {
 uint32_t published, replaced, consumed, lcd_frames;
 /* Drawing regions, not frames: one refresh may be split by clipping. */
 uint32_t draw_fast, draw_fallback;
 bool video_visible;
} tirtc_ui_video_stats_t;
void tirtc_ui_video_stats(tirtc_ui_video_stats_t *stats);
/* Call for every flush, before flush_ready clears LVGL's last-flush flag. */
void tirtc_ui_video_flush_done(bool success, bool last);
void tirtc_ui_init(void);
void tirtc_ui_process(void);
tirtc_ui_page_t tirtc_ui_current_page(void);
const char *tirtc_ui_page_name(tirtc_ui_page_t page);
#ifdef __cplusplus
}
#endif
#endif
