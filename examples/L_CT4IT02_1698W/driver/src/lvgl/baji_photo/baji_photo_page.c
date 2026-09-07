#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_external_flash_fs.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_rtc.h"
#include "app_features.h"
#include "app_boot_diag.h"
#include "gui_guider.h"
#include "baji_gif_player.h"
#include "baji_photo_bind_ui.h"
#include "baji_photo_config.h"
#include "baji_photo_diag.h"
#include "baji_photo_flash_img.h"
#include "baji_photo_page.h"
#include "baji_photo_http.h"
#include "baji_photo_mqtt.h"
#include "baji_photo_store.h"
#include "baji_photo_vpu_img.h"

#define BAJI_PHOTO_SCREEN_W         360
#define BAJI_PHOTO_SCREEN_H         360
#define BAJI_PHOTO_ANIM_TIME_MS     200
#define BAJI_PHOTO_REENTER_BLOCK_MS 600
#define BAJI_PHOTO_EXIT_HOLD_MS     1300u
#define BAJI_PHOTO_EXIT_MOVE_LIMIT  16
#define BAJI_PHOTO_SYNC_UI_POLL_MS  200
#define BAJI_PHOTO_SYNC_RESULT_MS   4000
#define BAJI_PHOTO_DOWNLOAD_UI_MIN_MS 1000u
#define BAJI_PHOTO_CTRL_PANEL_MIN_W 184
#define BAJI_PHOTO_CTRL_PANEL_H     62
#define BAJI_PHOTO_CTRL_BTN_W       44
#define BAJI_PHOTO_CTRL_BTN_H       44
#define BAJI_PHOTO_CTRL_BTN_GAP     12
#define BAJI_PHOTO_CTRL_MARGIN      22
#define BAJI_PHOTO_CTRL_RADIUS      31
#define BAJI_PHOTO_CTRL_PAD         9
#define BAJI_PHOTO_CTRL_EXT_HIT     5
#define BAJI_PHOTO_CTRL_MAX_MARGIN  56
#define BAJI_PHOTO_DELETE_MSGBOX_TITLE        "Delete"
#define BAJI_PHOTO_DELETE_MSGBOX_TEXT         "Delete photo?"
#define BAJI_PHOTO_DELETE_MSGBOX_CANCEL_TEXT  "Cancel"
#define BAJI_PHOTO_DELETE_MSGBOX_CONFIRM_TEXT "Delete"
#define BAJI_PHOTO_DELETE_MSGBOX_CONFIRM_BTN  1u
#define BAJI_PHOTO_PLAY_PERSIST_MIN_MS 30000u
#define BAJI_PHOTO_PLAY_PERSIST_MIN_SWITCHES 6u
#define BAJI_PHOTO_EMPTY_TITLE          "No photos yet"
#define BAJI_PHOTO_EMPTY_BODY_BOUND     "Wait for first photo"
#define BAJI_PHOTO_EMPTY_BODY_UNKNOWN   "Waiting for device status"
#define BAJI_PHOTO_EMPTY_BODY_DISABLED  "Device disabled"
#define BAJI_PHOTO_INDEX_LABEL_GAP_Y    10
#define BAJI_PHOTO_INDEX_LABEL_PAD_H    7
#define BAJI_PHOTO_INDEX_LABEL_PAD_V    3
#define BAJI_PHOTO_PAGE_TRACE(fmt, ...) liot_trace("[baji_page] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_KEY_TRACE(fmt, ...) liot_trace("\n[baji_page] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_DYN_CTX_MAGIC    0x44594E31u
#define BAJI_PHOTO_SCREEN_CTX_MAGIC 0x53435231u

typedef enum {
    BAJI_PHOTO_STATE_IDLE = 0,
    BAJI_PHOTO_STATE_TRANSITIONING,
    BAJI_PHOTO_STATE_ACTIVE,
} baji_photo_state_t;

typedef enum {
    BAJI_PHOTO_TRANSITION_NONE = 0,
    BAJI_PHOTO_TRANSITION_ENTER,
    BAJI_PHOTO_TRANSITION_SWITCH,
    BAJI_PHOTO_TRANSITION_EXIT,
} baji_photo_transition_t;

typedef enum {
    BAJI_PHOTO_JOB_SYNC = 0,
    BAJI_PHOTO_JOB_SEED,
} baji_photo_job_t;

typedef enum {
    BAJI_PHOTO_EXIT_TARGET_BAJI = 0,
    BAJI_PHOTO_EXIT_TARGET_TIME,
} baji_photo_exit_target_t;

#if BAJI_PHOTO_ENABLE_BUILTIN_DISPLAY
/* External image descriptors (defined in landscape_N_data.c, LVGL 8 format). */
extern const lv_img_dsc_t landscape_1_data;
extern const lv_img_dsc_t landscape_2_data;
extern const lv_img_dsc_t landscape_3_data;
extern const lv_img_dsc_t landscape_4_data;
extern const lv_img_dsc_t landscape_5_data;

static const lv_img_dsc_t *const g_baji_photos[] = {
    &landscape_1_data,
    &landscape_2_data,
    &landscape_3_data,
    &landscape_4_data,
    &landscape_5_data,
};
#define BAJI_PHOTO_CNT (sizeof(g_baji_photos) / sizeof(g_baji_photos[0]))
#else
#define BAJI_PHOTO_CNT 0u
#endif

typedef struct {
    bool is_downloaded;
    const lv_img_dsc_t *builtin;
    baji_photo_manifest_item_t item;
} baji_photo_list_item_t;

typedef struct {
    uint32_t magic;
    uint32_t session_id;
    uint32_t dyn_seq;
    int idx;
    bool is_downloaded;
    baji_photo_format_t format;
    char item_id[BAJI_PHOTO_ID_MAX_LEN + 1u];
    lv_img_dsc_t dsc;
    void *buf;
    lv_obj_t *img;
    bool release_queued;
} baji_photo_dynamic_src_t;

typedef struct {
    uint32_t magic;
    uint32_t session_id;
    int idx;
    bool is_downloaded;
    baji_photo_format_t format;
    char item_id[BAJI_PHOTO_ID_MAX_LEN + 1u];
    baji_photo_dynamic_src_t *dyn;
    uint32_t dyn_seq;
} baji_photo_screen_ctx_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *panel;
    lv_obj_t *index_label;
    lv_obj_t *delete_btn;
    lv_obj_t *play_btn;
    lv_obj_t *home_btn;
    lv_obj_t *play_label;
    lv_obj_t *msgbox;
    lv_timer_t *hide_timer;
    lv_timer_t *play_timer;
    lv_obj_t *press_target;
    bool gesture_consumed;
} baji_photo_controls_t;

typedef struct {
    unsigned int count;
    int current_index;
    uint32_t interval_ms;
    bool playing;
} baji_photo_player_t;

typedef struct {
    uint32_t switch_seq;
    uint32_t last_tick;
    uint32_t last_loaded_dyn_seq;
    uint32_t last_deleted_dyn_seq;
    uint32_t last_persist_switch_seq;
    uint32_t last_persist_tick;
    uint32_t timer_period_ms;
    lv_obj_t *last_loaded_screen;
    const char *last_trigger;
    int last_source_idx;
    int last_target_idx;
    lv_scr_load_anim_t last_anim;
    bool timer_active;
} baji_photo_play_diag_t;

typedef struct {
    bool valid;
    bool cleanup_pending;
    bool exit_home;
    bool start_after_delete;
    uint32_t session_id;
    uint32_t request_id;
    baji_photo_manifest_item_t item;
} baji_photo_delete_request_t;

typedef struct {
    bool valid;
    bool cleanup_pending;
    uint32_t session_id;
    uint32_t request_id;
    int ret;
} baji_photo_delete_result_t;

typedef enum {
    BAJI_PHOTO_PAGE_MAILBOX_SYNC_RESULT = 1u << 0,
    BAJI_PHOTO_PAGE_MAILBOX_SYNC_PROGRESS = 1u << 1,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_TASK = 1u << 2,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_DISPLAY_REQUEST = 1u << 3,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_RESULT = 1u << 4,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_STATUS = 1u << 5,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_TOKEN = 1u << 6,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_NOTICE = 1u << 7,
    BAJI_PHOTO_PAGE_MAILBOX_DELETE_RESULT = 1u << 8,
    BAJI_PHOTO_PAGE_MAILBOX_MQTT_IMAGE_PROGRESS = 1u << 9,
} baji_photo_page_mailbox_pending_t;

typedef struct {
    uint32_t pending;
    baji_photo_sync_result_t sync_result;
    baji_photo_sync_state_t sync_progress_state;
    uint16_t sync_progress_current;
    uint16_t sync_progress_total;
    baji_photo_image_task_t mqtt_task;
    baji_photo_mqtt_image_progress_t mqtt_image_progress;
    baji_photo_mqtt_display_request_t mqtt_display_request;
    baji_photo_mqtt_image_result_t mqtt_result;
    baji_photo_bind_status_t mqtt_bind_status;
    baji_photo_bind_token_info_t mqtt_bind_token;
    baji_photo_bind_notice_t mqtt_bind_notice;
    baji_photo_delete_result_t delete_result;
} baji_photo_page_mailbox_t;

typedef baji_photo_page_mailbox_t baji_photo_page_mailbox_snapshot_t;

static baji_photo_list_item_t g_baji_photo_list[BAJI_PHOTO_TOTAL_MAX];
static unsigned int g_baji_photo_count;
static int      g_baji_photo_idx            = 0;
static uint32_t g_baji_photo_session_id     = 0u;
static bool     g_baji_photo_del            = false;
static uint32_t g_baji_photo_last_exit_tick = 0;
static lv_obj_t *g_baji_photo_sync_label = NULL;
static lv_obj_t *g_baji_photo_empty_body_label = NULL;
static lv_timer_t *g_baji_photo_sync_timer = NULL;
static uint32_t g_baji_photo_sync_label_expire_tick = 0;
static bool g_baji_photo_download_progress_active;
static char g_baji_photo_download_task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
static char g_baji_photo_download_image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
static uint32_t g_baji_photo_downloaded_size;
static uint32_t g_baji_photo_download_total_size;
static uint32_t g_baji_photo_download_progress_bucket;
static uint32_t g_baji_photo_download_progress_last_ui_tick;
static bool g_baji_photo_download_progress_pending;
static baji_photo_page_mailbox_t s_baji_photo_page_mailbox;
static bool g_baji_photo_mqtt_display_pending = false;
static uint32_t g_baji_photo_mqtt_display_trace_id = 0u;
static char g_baji_photo_mqtt_display_task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
static char g_baji_photo_mqtt_display_image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
static baji_photo_job_t g_baji_photo_job = BAJI_PHOTO_JOB_SYNC;
static baji_photo_state_t      g_baji_photo_state      = BAJI_PHOTO_STATE_IDLE;
static baji_photo_transition_t g_baji_photo_transition = BAJI_PHOTO_TRANSITION_NONE;
static bool g_baji_photo_mqtt_started;
static bool g_baji_photo_mqtt_cb_registered;
static baji_photo_controls_t g_baji_photo_controls;
static uint32_t g_baji_photo_exit_press_tick;
static lv_point_t g_baji_photo_exit_press_point;
static bool g_baji_photo_exit_press_candidate;
static baji_photo_player_t g_baji_photo_player;
static baji_photo_play_diag_t g_baji_photo_play_diag;
static baji_photo_delete_request_t g_baji_photo_delete_request;
static liot_task_t g_baji_photo_delete_task = NULL;
static bool g_baji_photo_active_item_downloaded = false;
static char g_baji_photo_active_downloaded_id[BAJI_PHOTO_ID_MAX_LEN + 1u];

static void baji_photo_page_exit_to(baji_photo_exit_target_t target);
static void baji_photo_reload_list(void);
static void baji_photo_sync_ui_timer_cb(lv_timer_t *timer);
static void baji_photo_sync_label_show(const char *text, uint32_t visible_ms);
static uint32_t baji_photo_bytes_to_kbytes(uint32_t bytes);
static lv_obj_t *baji_photo_setup_scr(int idx);
static bool baji_photo_switch_to_index(int idx, lv_scr_load_anim_t anim, const char *trigger);
static void baji_photo_interaction_event_cb(lv_event_t *e);
static void baji_photo_controls_event_cb(lv_event_t *e);
static void baji_photo_delete_msgbox_event_cb(lv_event_t *e);
static void baji_photo_delete_task(void *arg);
static void baji_photo_mqtt_page_event_cb(baji_photo_mqtt_event_t event, const void *data, void *ctx);
static const char *baji_photo_item_id_get(int idx);
static const char *baji_photo_item_kind_get(int idx);
static int baji_photo_find_item_index_by_id(const char *id);
static void baji_photo_page_log_current_item(const char *phase, uint32_t session_id, int idx);
static void baji_photo_page_log_store_inventory(const char *phase,
                                                uint32_t session_id,
                                                const baji_photo_manifest_item_t *downloaded,
                                                unsigned int downloaded_count);
static int baji_photo_page_notify_mqtt_display_result(uint32_t session_id,
                                                      uint32_t trace_id,
                                                      const char *task_id,
                                                      const char *image_id,
                                                      int code,
                                                      uint64_t display_time_ms,
                                                      const char *reason);
static void baji_photo_page_track_mqtt_display(const baji_photo_mqtt_display_request_t *request);
static void baji_photo_page_clear_mqtt_display(void);
static void baji_photo_page_marker(const char *stage,
                                   uint32_t op,
                                   int idx,
                                   const char *item_id,
                                   const char *task_id,
                                   const char *image_id);
static void baji_photo_page_marker_display_request(const char *stage,
                                                   const baji_photo_mqtt_display_request_t *request,
                                                   int target_idx,
                                                   bool switched);
static void baji_photo_page_marker_display_result(const char *stage,
                                                  uint32_t trace_id,
                                                  const char *task_id,
                                                  const char *image_id,
                                                  int code,
                                                  uint64_t display_time_ms,
                                                  int ret);
static void baji_photo_page_marker_dyn(const char *stage,
                                       const baji_photo_dynamic_src_t *dyn,
                                       const char *reason);
static void baji_photo_controls_refresh_buttons(void);
static void baji_photo_controls_refresh_play_timer(void);
static void baji_photo_controls_cancel_hide_timer(void);
static void baji_photo_controls_refresh_index_label(void);
static void baji_photo_controls_show(void);
static void baji_photo_controls_hide(void);
static int baji_photo_controls_attach(lv_obj_t *screen);
static bool baji_photo_controls_handle_horizontal_gesture(lv_event_t *e);
static void baji_photo_controls_press_event_cb(lv_event_t *e);
static void baji_photo_delete_dialog_close(bool resume_controls);
static void baji_photo_delete_dialog_open(void);
static void baji_photo_delete_schedule_async(void *param);
static void baji_photo_page_log_boot_snapshot(const char *stage);
static int baji_photo_empty_state_attach(lv_obj_t *screen);
static void baji_photo_empty_state_refresh(void);
static void baji_photo_player_reset(void);
static void baji_photo_player_sync_count(unsigned int count, const char *reason);
static void baji_photo_player_sync_current_index(int idx, const char *reason);
static int baji_photo_player_get_current_index(void);
static bool baji_photo_player_is_playing(void);
static void baji_photo_player_set_playing(bool playing, const char *reason);
static bool baji_photo_player_resolve_step(lv_dir_t dir, int *out_index, const char *reason);
static bool baji_photo_player_resolve_advance(int *out_index, const char *reason);

static const baji_photo_bind_ui_ops_t g_baji_photo_bind_ui_ops = {
    .hide_controls = baji_photo_controls_hide,
    .cancel_hide_timer = baji_photo_controls_cancel_hide_timer,
    .refresh_buttons = baji_photo_controls_refresh_buttons,
    .refresh_play_timer = baji_photo_controls_refresh_play_timer,
};
static void baji_photo_play_diag_reset(void);
static void baji_photo_play_diag_mark_switch(const char *trigger,
                                             int source_idx,
                                             int target_idx,
                                             lv_scr_load_anim_t anim);
static void baji_photo_play_diag_note_timer(bool timer_active, uint32_t period_ms, const char *phase);
static void baji_photo_play_diag_note_loaded(const baji_photo_screen_ctx_t *screen_ctx,
                                             lv_obj_t *screen,
                                             const char *phase);
static void baji_photo_play_diag_note_dyn_delete(const baji_photo_dynamic_src_t *dyn,
                                                 const char *phase);
static void baji_photo_play_diag_log_persisted_snapshot(const char *phase);
static bool baji_photo_play_diag_should_persist(void);
static void baji_photo_play_diag_persist_if_needed(const baji_photo_screen_ctx_t *screen_ctx);
static void baji_photo_active_item_reset(void);
static void baji_photo_active_item_update(const baji_photo_screen_ctx_t *screen_ctx);
static void baji_photo_restore_active_index_after_reload(void);
static void baji_photo_dynamic_src_release_async(void *param);
static void baji_photo_dynamic_src_free_buf(baji_photo_dynamic_src_t *dyn);

static void baji_photo_page_mailbox_post_sync_result(const baji_photo_sync_result_t *result)
{
    if (result == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.sync_result = *result;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_SYNC_RESULT;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_sync_progress(baji_photo_sync_state_t state,
                                                       uint16_t current,
                                                       uint16_t total)
{
    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.sync_progress_state = state;
    s_baji_photo_page_mailbox.sync_progress_current = current;
    s_baji_photo_page_mailbox.sync_progress_total = total;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_SYNC_PROGRESS;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_task(const baji_photo_image_task_t *task)
{
    if (task == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_task = *task;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_TASK;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_image_progress(
    const baji_photo_mqtt_image_progress_t *progress)
{
    if (progress == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_image_progress = *progress;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_IMAGE_PROGRESS;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_display_request(const baji_photo_mqtt_display_request_t *request)
{
    if (request == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_display_request = *request;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_DISPLAY_REQUEST;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_result(const baji_photo_mqtt_image_result_t *result)
{
    if (result == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_result = *result;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_RESULT;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_bind_status(const baji_photo_bind_status_t *bind_status)
{
    if (bind_status == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_bind_status = *bind_status;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_STATUS;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_bind_token(const baji_photo_bind_token_info_t *bind_token)
{
    if (bind_token == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_bind_token = *bind_token;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_TOKEN;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_bind_notice(const baji_photo_bind_notice_t *bind_notice)
{
    if (bind_notice == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.mqtt_bind_notice = *bind_notice;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_NOTICE;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_delete_result(const baji_photo_delete_result_t *result)
{
    if (result == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    s_baji_photo_page_mailbox.delete_result = *result;
    s_baji_photo_page_mailbox.pending |= BAJI_PHOTO_PAGE_MAILBOX_DELETE_RESULT;
    liot_rtos_exit_critical();
}

static void baji_photo_page_mailbox_post_mqtt_event(baji_photo_mqtt_event_t event, const void *data)
{
    switch (event) {
    case BAJI_PHOTO_MQTT_EVT_IMAGE_TASK:
        baji_photo_page_mailbox_post_mqtt_task((const baji_photo_image_task_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_IMAGE_PROGRESS:
        baji_photo_page_mailbox_post_mqtt_image_progress(
            (const baji_photo_mqtt_image_progress_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_DISPLAY_REQUEST:
        baji_photo_page_mailbox_post_mqtt_display_request((const baji_photo_mqtt_display_request_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_IMAGE_RESULT:
        baji_photo_page_mailbox_post_mqtt_result((const baji_photo_mqtt_image_result_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_BIND_STATUS:
        baji_photo_page_mailbox_post_mqtt_bind_status((const baji_photo_bind_status_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_BIND_TOKEN:
        baji_photo_page_mailbox_post_mqtt_bind_token((const baji_photo_bind_token_info_t *)data);
        break;
    case BAJI_PHOTO_MQTT_EVT_BIND_NOTICE:
        baji_photo_page_mailbox_post_mqtt_bind_notice((const baji_photo_bind_notice_t *)data);
        break;
    default:
        break;
    }
}

static void baji_photo_page_mailbox_take_snapshot(baji_photo_page_mailbox_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    liot_rtos_enter_critical();
    *snapshot = s_baji_photo_page_mailbox;
    s_baji_photo_page_mailbox.pending = 0u;
    liot_rtos_exit_critical();
}

static bool baji_photo_page_mailbox_snapshot_has(const baji_photo_page_mailbox_snapshot_t *snapshot,
                                                 baji_photo_page_mailbox_pending_t pending)
{
    return (snapshot != NULL) &&
           ((snapshot->pending & (uint32_t)pending) != 0u);
}

static bool baji_photo_page_mailbox_has_pending(baji_photo_page_mailbox_pending_t pending)
{
    bool has_pending;

    liot_rtos_enter_critical();
    has_pending = (s_baji_photo_page_mailbox.pending & (uint32_t)pending) != 0u;
    liot_rtos_exit_critical();
    return has_pending;
}

static const char *baji_photo_ctx_kind(bool is_downloaded, baji_photo_format_t format)
{
    if (!is_downloaded) {
        return "builtin";
    }
    switch (format) {
    case BAJI_PHOTO_FORMAT_GIF:
        return "gif";
    case BAJI_PHOTO_FORMAT_JPEG:
        return "jpeg";
    case BAJI_PHOTO_FORMAT_PNG:
        return "png";
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        return "photo";
    }
}

static void baji_photo_dyn_ctx_init(baji_photo_dynamic_src_t *dyn,
                                    uint32_t session_id,
                                    int idx,
                                    const baji_photo_list_item_t *item)
{
    if ((dyn == NULL) || (item == NULL)) {
        return;
    }

    dyn->magic = BAJI_PHOTO_DYN_CTX_MAGIC;
    dyn->session_id = session_id;
    dyn->dyn_seq = baji_photo_diag_next_id();
    dyn->idx = idx;
    dyn->is_downloaded = item->is_downloaded;
    dyn->format = item->item.format;
    (void)snprintf(dyn->item_id, sizeof(dyn->item_id), "%s", baji_photo_item_id_get(idx));
}

static void baji_photo_screen_ctx_init(baji_photo_screen_ctx_t *ctx,
                                       uint32_t session_id,
                                       int idx,
                                       const baji_photo_list_item_t *item)
{
    if ((ctx == NULL) || (item == NULL)) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->magic = BAJI_PHOTO_SCREEN_CTX_MAGIC;
    ctx->session_id = session_id;
    ctx->idx = idx;
    ctx->is_downloaded = item->is_downloaded;
    ctx->format = item->item.format;
    ctx->dyn_seq = 0u;
    (void)snprintf(ctx->item_id, sizeof(ctx->item_id), "%s", baji_photo_item_id_get(idx));
}

static void baji_photo_page_log_runtime(const char *phase, uint32_t session_id)
{
    liot_task_t ui_task = NULL;
    liot_task_t mqtt_task = baji_photo_mqtt_get_task_ref();
    unsigned long heap_free = (unsigned long)liot_xPortGetFreeHeapSize();
    unsigned long heap_min = (unsigned long)liot_xPortGetMinimumEverFreeHeapSize();
    unsigned long heap_max = (unsigned long)liot_xPortGetMaximumFreeBlockSize();

    (void)liot_rtos_task_get_current_ref(&ui_task);

    BAJI_PHOTO_PAGE_TRACE("op=%lu runtime phase=%s ui_task=%p mqtt_task=%p heap_free=%lu heap_min=%lu heap_max=%lu idx=%d player_idx=%d player_count=%u playing=%d interval=%lu state=%d trans=%d",
                          (unsigned long)session_id,
                          phase,
                          ui_task,
                          mqtt_task,
                          heap_free,
                          heap_min,
                          heap_max,
                          g_baji_photo_idx,
                          g_baji_photo_player.current_index,
                          g_baji_photo_player.count,
                          g_baji_photo_player.playing ? 1 : 0,
                          (unsigned long)g_baji_photo_player.interval_ms,
                          (int)g_baji_photo_state,
                          (int)g_baji_photo_transition);
}

static void baji_photo_page_log_boot_snapshot(const char *stage)
{
    UINT8 powerup_reason = LIOT_PWRUP_UNKNOWN;
    liot_power_errcode_e power_ret = liot_get_powerup_reason(&powerup_reason);

    app_boot_diag_log_reason(stage, powerup_reason, power_ret);
    BAJI_PHOTO_PAGE_TRACE("op=%lu boot snapshot stage=%s reason=%u name=%s class=%s ret=%d",
                          (unsigned long)g_baji_photo_session_id,
                          (stage != NULL) ? stage : "-",
                          (unsigned int)powerup_reason,
                          app_boot_powerup_reason_name(powerup_reason),
                          app_boot_powerup_reason_class(powerup_reason),
                          (int)power_ret);
}

static void baji_photo_play_diag_log(const char *phase,
                                     int idx,
                                     uint32_t dyn_seq,
                                     lv_obj_t *screen)
{
    unsigned long heap_free = (unsigned long)liot_xPortGetFreeHeapSize();
    unsigned long heap_min = (unsigned long)liot_xPortGetMinimumEverFreeHeapSize();
    unsigned long heap_max = (unsigned long)liot_xPortGetMaximumFreeBlockSize();
    lv_obj_t *active_screen = lv_scr_act();

    BAJI_PHOTO_PAGE_TRACE("op=%lu playdiag phase=%s seq=%lu trigger=%s src=%d target=%d idx=%d dyn_seq=%lu count=%u playing=%d timer_active=%d timer_period=%lu timer=%p anim=%d active=%p loaded=%p screen=%p tick=%lu state=%d trans=%d heap_free=%lu heap_min=%lu heap_max=%lu",
                          (unsigned long)g_baji_photo_session_id,
                          (phase != NULL) ? phase : "-",
                          (unsigned long)g_baji_photo_play_diag.switch_seq,
                          (g_baji_photo_play_diag.last_trigger != NULL) ?
                              g_baji_photo_play_diag.last_trigger : "-",
                          g_baji_photo_play_diag.last_source_idx,
                          g_baji_photo_play_diag.last_target_idx,
                          idx,
                          (unsigned long)dyn_seq,
                          g_baji_photo_player.count,
                          baji_photo_player_is_playing() ? 1 : 0,
                          g_baji_photo_play_diag.timer_active ? 1 : 0,
                          (unsigned long)g_baji_photo_play_diag.timer_period_ms,
                          g_baji_photo_controls.play_timer,
                          (int)g_baji_photo_play_diag.last_anim,
                          active_screen,
                          g_baji_photo_play_diag.last_loaded_screen,
                          screen,
                          (unsigned long)g_baji_photo_play_diag.last_tick,
                          (int)g_baji_photo_state,
                          (int)g_baji_photo_transition,
                          heap_free,
                          heap_min,
                          heap_max);
}

static void baji_photo_page_log_current_item(const char *phase, uint32_t session_id, int idx)
{
    const baji_photo_list_item_t *item = NULL;
    const char *item_id = "invalid";
    const char *kind = "invalid";
    const char *name = "-";
    const char *path = "-";
    unsigned long file_size = 0u;
    unsigned int width = 0u;
    unsigned int height = 0u;
    int downloaded = 0;

    if ((idx >= 0) && ((unsigned int)idx < g_baji_photo_count)) {
        item = &g_baji_photo_list[idx];
        item_id = baji_photo_item_id_get(idx);
        kind = baji_photo_item_kind_get(idx);
        downloaded = item->is_downloaded ? 1 : 0;
        if (item->is_downloaded) {
            name = (item->item.name[0] != '\0') ? item->item.name : item->item.id;
            path = (item->item.local_path[0] != '\0') ? item->item.local_path : "-";
            file_size = (unsigned long)item->item.file_size;
            width = (unsigned int)item->item.width;
            height = (unsigned int)item->item.height;
        }
    }

    BAJI_PHOTO_PAGE_TRACE("op=%lu play phase=%s idx=%d id=%s kind=%s downloaded=%d name=%s size=%lu dims=%ux%u path=%s",
                          (unsigned long)session_id,
                          phase,
                          idx,
                          item_id,
                          kind,
                          downloaded,
                          name,
                          file_size,
                          width,
                          height,
                          path);
}

static void baji_photo_page_log_store_inventory(const char *phase,
                                                uint32_t session_id,
                                                const baji_photo_manifest_item_t *downloaded,
                                                unsigned int downloaded_count)
{
#if BAJI_PHOTO_ENABLE_STORE_INVENTORY_DIAG
    baji_photo_device_meta_t meta;
#endif
    const baji_photo_list_item_t *current_item = NULL;
    const char *meta_id = "-";
    const char *current_name = "-";
    const char *current_path = "-";
    const char *largest_id = "-";
    const char *largest_kind = "-";
    unsigned int photo_count = 0u;
    unsigned int gif_count = 0u;
    unsigned int builtin_count = 0u;
    unsigned int current_width = 0u;
    unsigned int current_height = 0u;
    unsigned int largest_width = 0u;
    unsigned int largest_height = 0u;
    unsigned long flash_total = (unsigned long)BAJI_FLASH_LFS_TOTAL;
    unsigned long flash_used = 0u;
    unsigned long flash_free = 0u;
    unsigned long flash_overhead = 0u;
    unsigned long store_bytes = 0u;
    unsigned long photo_bytes = 0u;
    unsigned long gif_bytes = 0u;
    unsigned long current_size = 0u;
    unsigned long largest_size = 0u;
    unsigned int flash_used_pct = 0u;
    int flash_free_ret;
    int meta_ret;
    unsigned int i;

    for (i = 0; i < downloaded_count; ++i) {
        unsigned long item_size = (unsigned long)downloaded[i].file_size;

        store_bytes += item_size;
        if (downloaded[i].format == BAJI_PHOTO_FORMAT_GIF) {
            ++gif_count;
            gif_bytes += item_size;
        } else {
            ++photo_count;
            photo_bytes += item_size;
        }
        if (item_size >= largest_size) {
            largest_size = item_size;
            largest_id = downloaded[i].id;
            largest_kind = (downloaded[i].format == BAJI_PHOTO_FORMAT_GIF) ? "gif" : "photo";
            largest_width = (unsigned int)downloaded[i].width;
            largest_height = (unsigned int)downloaded[i].height;
        }
    }

    if (g_baji_photo_count >= downloaded_count) {
        builtin_count = g_baji_photo_count - downloaded_count;
    }

#if BAJI_PHOTO_ENABLE_STORE_INVENTORY_DIAG
    flash_free_ret = baji_photo_store_free_size();
    if (flash_free_ret >= 0) {
        flash_free = (unsigned long)flash_free_ret;
        flash_used = (flash_total >= flash_free) ? (flash_total - flash_free) : 0u;
    }
    if (flash_total != 0u) {
        flash_used_pct = (unsigned int)((flash_used * 100u) / flash_total);
    }
    if (flash_used >= store_bytes) {
        flash_overhead = flash_used - store_bytes;
    }

    memset(&meta, 0, sizeof(meta));
    meta_ret = baji_photo_store_load_device_meta(&meta);
    if ((meta_ret == 0) && (meta.current_image_id[0] != '\0')) {
        meta_id = meta.current_image_id;
    }
#else
    flash_free_ret = LIOT_EXTFLASH_NOT_EXIST;
    meta_ret = LIOT_EXTFLASH_NOT_EXIST;
#endif
    if ((g_baji_photo_idx >= 0) && ((unsigned int)g_baji_photo_idx < g_baji_photo_count)) {
        current_item = &g_baji_photo_list[g_baji_photo_idx];
        if (current_item->is_downloaded) {
            current_name = (current_item->item.name[0] != '\0') ?
                               current_item->item.name : current_item->item.id;
            current_path = (current_item->item.local_path[0] != '\0') ?
                               current_item->item.local_path : "-";
            current_size = (unsigned long)current_item->item.file_size;
            current_width = (unsigned int)current_item->item.width;
            current_height = (unsigned int)current_item->item.height;
        }
    }

    BAJI_PHOTO_KEY_TRACE("op=%lu store summary phase=%s flash_used=%lu flash_free=%lu flash_total=%lu used_pct=%u list_total=%u builtin=%u store_total=%u photo=%u gif=%u current_idx=%d current_id=%s current_kind=%s",
                          (unsigned long)session_id,
                          phase,
                          flash_used,
                          flash_free,
                          flash_total,
                          flash_used_pct,
                          g_baji_photo_count,
                          builtin_count,
                          downloaded_count,
                          photo_count,
                          gif_count,
                          g_baji_photo_idx,
                          baji_photo_item_id_get(g_baji_photo_idx),
                          baji_photo_item_kind_get(g_baji_photo_idx));
    BAJI_PHOTO_KEY_TRACE("op=%lu store usage phase=%s store_bytes=%lu photo_bytes=%lu gif_bytes=%lu fs_overhead=%lu largest_id=%s largest_kind=%s largest_size=%lu largest_dims=%ux%u",
                          (unsigned long)session_id,
                          phase,
                          store_bytes,
                          photo_bytes,
                          gif_bytes,
                          flash_overhead,
                          largest_id,
                          largest_kind,
                          largest_size,
                          largest_width,
                          largest_height);
    BAJI_PHOTO_KEY_TRACE("op=%lu store detail phase=%s current_name=%s current_size=%lu current_dims=%ux%u current_path=%s meta_id=%s meta_ret=%d free_ret=%d",
                          (unsigned long)session_id,
                          phase,
                          current_name,
                          current_size,
                          current_width,
                          current_height,
                          current_path,
                          meta_id,
                          meta_ret,
                          flash_free_ret);
}

static int baji_photo_page_notify_mqtt_display_result(uint32_t session_id,
                                                      uint32_t trace_id,
                                                      const char *task_id,
                                                      const char *image_id,
                                                      int code,
                                                      uint64_t display_time_ms,
                                                      const char *reason)
{
    baji_photo_mqtt_display_result_t result;
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int ret;

    if ((task_id == NULL) || (image_id == NULL)) {
        return -1;
    }

    memset(&result, 0, sizeof(result));
    (void)snprintf(result.task_id, sizeof(result.task_id), "%s", task_id);
    (void)snprintf(result.image_id, sizeof(result.image_id), "%s", image_id);
    if (reason != NULL) {
        (void)snprintf(result.reason, sizeof(result.reason), "%s", reason);
    }
    result.code = code;
    result.trace_id = trace_id;
    result.display_time_ms = display_time_ms;
    ret = baji_photo_mqtt_notify_display_result(&result);
    BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display ack task_op=%lu task=%s image=%s code=%d display_ms=%s reason=%s ret=%d",
                          (unsigned long)session_id,
                          (unsigned long)trace_id,
                          task_id,
                          image_id,
                          code,
                          baji_photo_diag_u64_dec(display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)),
                          (reason != NULL) ? reason : "-",
                          ret);
    baji_photo_page_marker_display_result("UI_DISPLAY_ACK",
                                          trace_id,
                                          task_id,
                                          image_id,
                                          code,
                                          display_time_ms,
                                          ret);
    return ret;
}

static void baji_photo_page_track_mqtt_display(const baji_photo_mqtt_display_request_t *request)
{
    if (request == NULL) {
        return;
    }

    g_baji_photo_mqtt_display_pending = true;
    g_baji_photo_mqtt_display_trace_id = request->trace_id;
    (void)snprintf(g_baji_photo_mqtt_display_task_id,
                   sizeof(g_baji_photo_mqtt_display_task_id),
                   "%s",
                   request->task_id);
    (void)snprintf(g_baji_photo_mqtt_display_image_id,
                   sizeof(g_baji_photo_mqtt_display_image_id),
                   "%s",
                   request->image_id);
}

static void baji_photo_page_clear_mqtt_display(void)
{
    g_baji_photo_mqtt_display_pending = false;
    g_baji_photo_mqtt_display_trace_id = 0u;
    memset(g_baji_photo_mqtt_display_task_id, 0, sizeof(g_baji_photo_mqtt_display_task_id));
    memset(g_baji_photo_mqtt_display_image_id, 0, sizeof(g_baji_photo_mqtt_display_image_id));
}

static void baji_photo_page_marker(const char *stage,
                                   uint32_t op,
                                   int idx,
                                   const char *item_id,
                                   const char *task_id,
                                   const char *image_id)
{
    BAJI_PHOTO_MARK_TRACE("%s op=%lu idx=%d item=%s task=%s image=%s heap_min=%lu",
                          stage,
                          (unsigned long)op,
                          idx,
                          baji_photo_diag_id_tail(item_id),
                          baji_photo_diag_id_tail(task_id),
                          baji_photo_diag_id_tail(image_id),
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_page_marker_display_request(const char *stage,
                                                   const baji_photo_mqtt_display_request_t *request,
                                                   int target_idx,
                                                   bool switched)
{
    if ((stage == NULL) || (request == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu task=%s image=%s dl=%lu list=%u target=%d state=%d trans=%d switched=%d pending=%d heap_min=%lu",
                          stage,
                          (unsigned long)request->trace_id,
                          baji_photo_diag_id_tail(request->task_id),
                          baji_photo_diag_id_tail(request->image_id),
                          (unsigned long)request->downloaded_size,
                          g_baji_photo_count,
                          target_idx,
                          (int)g_baji_photo_state,
                          (int)g_baji_photo_transition,
                          switched ? 1 : 0,
                          g_baji_photo_mqtt_display_pending ? 1 : 0,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_page_marker_display_result(const char *stage,
                                                  uint32_t trace_id,
                                                  const char *task_id,
                                                  const char *image_id,
                                                  int code,
                                                  uint64_t display_time_ms,
                                                  int ret)
{
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];

    if (stage == NULL) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu task=%s image=%s code=%d display_ms=%s ret=%d heap_min=%lu",
                          stage,
                          (unsigned long)trace_id,
                          baji_photo_diag_id_tail(task_id),
                          baji_photo_diag_id_tail(image_id),
                          code,
                          baji_photo_diag_u64_dec(display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)),
                          ret,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_page_marker_dyn(const char *stage,
                                       const baji_photo_dynamic_src_t *dyn,
                                       const char *reason)
{
    if ((stage == NULL) || (dyn == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu dyn=%lu idx=%d item=%s kind=%s bytes=%lu img=%p buf=%p reason=%s heap_min=%lu",
                          stage,
                          (unsigned long)dyn->session_id,
                          (unsigned long)dyn->dyn_seq,
                          dyn->idx,
                          baji_photo_diag_id_tail(dyn->item_id),
                          baji_photo_ctx_kind(dyn->is_downloaded, dyn->format),
                          (unsigned long)dyn->dsc.data_size,
                          dyn->img,
                          dyn->buf,
                          (reason != NULL) ? reason : "-",
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static const char *baji_photo_item_id_get(int idx)
{
    static const char *const k_builtin_fallback = "builtin_display";

    if ((idx < 0) || ((unsigned int)idx >= g_baji_photo_count)) {
        return "invalid";
    }
    if (g_baji_photo_list[idx].is_downloaded) {
        return g_baji_photo_list[idx].item.id;
    }
    return k_builtin_fallback;
}

static const char *baji_photo_item_kind_get(int idx)
{
    if ((idx < 0) || ((unsigned int)idx >= g_baji_photo_count)) {
        return "invalid";
    }
    if (!g_baji_photo_list[idx].is_downloaded) {
        return "builtin";
    }
    return baji_photo_ctx_kind(true, g_baji_photo_list[idx].item.format);
}

static bool baji_photo_obj_valid(lv_obj_t *obj)
{
    return (obj != NULL) && lv_obj_is_valid(obj);
}

static bool baji_photo_delete_is_pending(void)
{
    return g_baji_photo_delete_request.valid || (g_baji_photo_delete_task != NULL);
}

static bool baji_photo_delete_result_matches_current(
    const baji_photo_delete_result_t *result)
{
    return (result != NULL) &&
           result->valid &&
           g_baji_photo_delete_request.valid &&
           (result->session_id == g_baji_photo_delete_request.session_id) &&
           (result->request_id == g_baji_photo_delete_request.request_id);
}

static void baji_photo_active_item_reset(void)
{
    g_baji_photo_active_item_downloaded = false;
    g_baji_photo_active_downloaded_id[0] = '\0';
}

static void baji_photo_play_diag_reset(void)
{
    memset(&g_baji_photo_play_diag, 0, sizeof(g_baji_photo_play_diag));
    g_baji_photo_play_diag.last_trigger = "idle";
    g_baji_photo_play_diag.last_source_idx = -1;
    g_baji_photo_play_diag.last_target_idx = -1;
    g_baji_photo_play_diag.timer_period_ms = BAJI_PHOTO_SLIDESHOW_INTERVAL_MS;
    g_baji_photo_play_diag.last_anim = (lv_scr_load_anim_t)0;
    g_baji_photo_play_diag.last_tick = lv_tick_get();
    baji_photo_play_diag_log("reset", g_baji_photo_idx, 0u, NULL);
}

static void baji_photo_play_diag_mark_switch(const char *trigger,
                                             int source_idx,
                                             int target_idx,
                                             lv_scr_load_anim_t anim)
{
    g_baji_photo_play_diag.switch_seq = baji_photo_diag_next_id();
    g_baji_photo_play_diag.last_trigger = (trigger != NULL) ? trigger : "-";
    g_baji_photo_play_diag.last_source_idx = source_idx;
    g_baji_photo_play_diag.last_target_idx = target_idx;
    g_baji_photo_play_diag.last_anim = anim;
    g_baji_photo_play_diag.last_tick = lv_tick_get();
    baji_photo_play_diag_log("switch_mark", target_idx, 0u, NULL);
}

static void baji_photo_play_diag_note_timer(bool timer_active, uint32_t period_ms, const char *phase)
{
    bool changed = (g_baji_photo_play_diag.timer_active != timer_active) ||
                   (g_baji_photo_play_diag.timer_period_ms != period_ms);

    g_baji_photo_play_diag.timer_active = timer_active;
    g_baji_photo_play_diag.timer_period_ms = period_ms;
    if (!changed) {
        return;
    }

    g_baji_photo_play_diag.last_tick = lv_tick_get();
    baji_photo_play_diag_log((phase != NULL) ? phase : "timer_state",
                             g_baji_photo_idx,
                             g_baji_photo_play_diag.last_loaded_dyn_seq,
                             g_baji_photo_play_diag.last_loaded_screen);
}

static void baji_photo_play_diag_note_loaded(const baji_photo_screen_ctx_t *screen_ctx,
                                             lv_obj_t *screen,
                                             const char *phase)
{
    g_baji_photo_play_diag.last_loaded_screen = screen;
    g_baji_photo_play_diag.last_loaded_dyn_seq = (screen_ctx != NULL) ? screen_ctx->dyn_seq : 0u;
    g_baji_photo_play_diag.last_tick = lv_tick_get();
    baji_photo_play_diag_log((phase != NULL) ? phase : "screen_loaded",
                             (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                             g_baji_photo_play_diag.last_loaded_dyn_seq,
                             screen);
}

static void baji_photo_play_diag_note_dyn_delete(const baji_photo_dynamic_src_t *dyn,
                                                 const char *phase)
{
    g_baji_photo_play_diag.last_deleted_dyn_seq = (dyn != NULL) ? dyn->dyn_seq : 0u;
    g_baji_photo_play_diag.last_tick = lv_tick_get();
    baji_photo_play_diag_log((phase != NULL) ? phase : "dyn_delete",
                             (dyn != NULL) ? dyn->idx : g_baji_photo_idx,
                             g_baji_photo_play_diag.last_deleted_dyn_seq,
                             NULL);
}

static void baji_photo_play_diag_log_persisted_snapshot(const char *phase)
{
    baji_photo_device_meta_t meta;
    char time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];

    memset(&meta, 0, sizeof(meta));
    if (baji_photo_store_load_device_meta(&meta) != 0) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu playdiag persisted phase=%s available=0",
                              (unsigned long)g_baji_photo_session_id,
                              (phase != NULL) ? phase : "-");
        return;
    }

    if (!meta.playback.valid) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu playdiag persisted phase=%s available=0 image=%s task=%s",
                              (unsigned long)g_baji_photo_session_id,
                              (phase != NULL) ? phase : "-",
                              meta.current_image_id,
                              meta.last_task_id);
        return;
    }

    BAJI_PHOTO_PAGE_TRACE("op=%lu playdiag persisted phase=%s available=1 seq=%lu trigger=%s image=%s src=%ld target=%ld dyn_seq=%lu timer_active=%d timer_period=%lu time_ms=%s task=%s current=%s",
                          (unsigned long)g_baji_photo_session_id,
                          (phase != NULL) ? phase : "-",
                          (unsigned long)meta.playback.seq,
                          meta.playback.trigger,
                          meta.playback.image_id,
                          (meta.playback.source_idx == UINT32_MAX) ? -1L :
                              (long)meta.playback.source_idx,
                          (meta.playback.target_idx == UINT32_MAX) ? -1L :
                              (long)meta.playback.target_idx,
                          (unsigned long)meta.playback.dyn_seq,
                          meta.playback.timer_active ? 1 : 0,
                          (unsigned long)meta.playback.timer_period_ms,
                          baji_photo_diag_u64_dec(meta.playback.time_ms,
                                                  time_buf,
                                                  sizeof(time_buf)),
                          meta.last_task_id,
                          meta.current_image_id);
}

static bool baji_photo_play_diag_should_persist(void)
{
    if ((g_baji_photo_play_diag.switch_seq == 0u) ||
        (g_baji_photo_play_diag.last_trigger == NULL)) {
        return false;
    }

    if (g_baji_photo_play_diag.last_persist_switch_seq == 0u) {
        return true;
    }
    if (g_baji_photo_play_diag.switch_seq <= g_baji_photo_play_diag.last_persist_switch_seq) {
        return false;
    }
    if ((g_baji_photo_play_diag.switch_seq - g_baji_photo_play_diag.last_persist_switch_seq) >=
        BAJI_PHOTO_PLAY_PERSIST_MIN_SWITCHES) {
        return true;
    }

    return lv_tick_elaps(g_baji_photo_play_diag.last_persist_tick) >=
           BAJI_PHOTO_PLAY_PERSIST_MIN_MS;
}

static void baji_photo_play_diag_persist_if_needed(const baji_photo_screen_ctx_t *screen_ctx)
{
    baji_photo_playback_meta_t playback;
    uint64_t now_ms = 0u;
    char ts_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int ret;

    if ((screen_ctx == NULL) || !screen_ctx->is_downloaded || !baji_photo_play_diag_should_persist()) {
        return;
    }

#if !BAJI_PHOTO_ENABLE_PLAYBACK_META_PERSIST
    g_baji_photo_play_diag.last_persist_switch_seq = g_baji_photo_play_diag.switch_seq;
    g_baji_photo_play_diag.last_persist_tick = lv_tick_get();
    return;
#endif

    memset(&playback, 0, sizeof(playback));
    (void)snprintf(playback.image_id, sizeof(playback.image_id), "%s", screen_ctx->item_id);
    (void)snprintf(playback.trigger,
                   sizeof(playback.trigger),
                   "%s",
                   (g_baji_photo_play_diag.last_trigger != NULL) ?
                       g_baji_photo_play_diag.last_trigger : "-");
    if (Liot_GetTimestamp(&now_ms) != 0) {
        now_ms = 0u;
    }
    playback.time_ms = now_ms;
    playback.seq = g_baji_photo_play_diag.switch_seq;
    playback.source_idx = (g_baji_photo_play_diag.last_source_idx >= 0) ?
                              (uint32_t)g_baji_photo_play_diag.last_source_idx : UINT32_MAX;
    playback.target_idx = (screen_ctx->idx >= 0) ? (uint32_t)screen_ctx->idx : UINT32_MAX;
    playback.dyn_seq = screen_ctx->dyn_seq;
    playback.timer_period_ms = g_baji_photo_play_diag.timer_period_ms;
    playback.timer_active = g_baji_photo_play_diag.timer_active;
    playback.valid = true;

    ret = baji_photo_store_update_device_meta_playback(&playback);
    BAJI_PHOTO_PAGE_TRACE("op=%lu playdiag persist seq=%lu trigger=%s image=%s src=%ld target=%ld dyn_seq=%lu timer_active=%d timer_period=%lu time_ms=%s ret=%d",
                          (unsigned long)g_baji_photo_session_id,
                          (unsigned long)playback.seq,
                          playback.trigger,
                          playback.image_id,
                          (playback.source_idx == UINT32_MAX) ? -1L :
                              (long)playback.source_idx,
                          (playback.target_idx == UINT32_MAX) ? -1L :
                              (long)playback.target_idx,
                          (unsigned long)playback.dyn_seq,
                          playback.timer_active ? 1 : 0,
                          (unsigned long)playback.timer_period_ms,
                          baji_photo_diag_u64_dec(playback.time_ms, ts_buf, sizeof(ts_buf)),
                          ret);
    if (ret == 0) {
        g_baji_photo_play_diag.last_persist_switch_seq = g_baji_photo_play_diag.switch_seq;
        g_baji_photo_play_diag.last_persist_tick = lv_tick_get();
    }
}

static void baji_photo_player_trace(const char *phase,
                                    const char *reason,
                                    int source_idx,
                                    int target_idx)
{
    BAJI_PHOTO_PAGE_TRACE("op=%lu player phase=%s reason=%s source=%d target=%d current=%d count=%u playing=%d interval=%lu state=%d trans=%d",
                          (unsigned long)g_baji_photo_session_id,
                          (phase != NULL) ? phase : "-",
                          (reason != NULL) ? reason : "-",
                          source_idx,
                          target_idx,
                          g_baji_photo_player.current_index,
                          g_baji_photo_player.count,
                          g_baji_photo_player.playing ? 1 : 0,
                          (unsigned long)g_baji_photo_player.interval_ms,
                          (int)g_baji_photo_state,
                          (int)g_baji_photo_transition);
}

static void baji_photo_player_reset(void)
{
    memset(&g_baji_photo_player, 0, sizeof(g_baji_photo_player));
    g_baji_photo_player.current_index = 0;
    g_baji_photo_player.interval_ms = BAJI_PHOTO_SLIDESHOW_INTERVAL_MS;
    baji_photo_player_trace("reset", "page_reset", -1, 0);
}

static void baji_photo_player_sync_count(unsigned int count, const char *reason)
{
    int source_idx = g_baji_photo_player.current_index;

    g_baji_photo_player.count = count;
    if (count == 0u) {
        g_baji_photo_player.current_index = 0;
        g_baji_photo_player.playing = false;
    } else if ((g_baji_photo_player.current_index < 0) ||
               ((unsigned int)g_baji_photo_player.current_index >= count)) {
        g_baji_photo_player.current_index = 0;
    }
    if (count < 2u) {
        g_baji_photo_player.playing = false;
    }

    baji_photo_player_trace("sync_count", reason, source_idx, g_baji_photo_player.current_index);
}

static void baji_photo_player_sync_current_index(int idx, const char *reason)
{
    int source_idx = g_baji_photo_player.current_index;

    if (g_baji_photo_player.count == 0u) {
        g_baji_photo_player.current_index = 0;
        baji_photo_player_trace("sync_index_skip", reason, source_idx, idx);
        return;
    }
    if ((idx < 0) || ((unsigned int)idx >= g_baji_photo_player.count)) {
        baji_photo_player_trace("sync_index_invalid", reason, source_idx, idx);
        return;
    }

    g_baji_photo_player.current_index = idx;
    baji_photo_player_trace("sync_index", reason, source_idx, idx);
}

static int baji_photo_player_get_current_index(void)
{
    if (g_baji_photo_player.count == 0u) {
        return -1;
    }
    if ((g_baji_photo_player.current_index < 0) ||
        ((unsigned int)g_baji_photo_player.current_index >= g_baji_photo_player.count)) {
        return 0;
    }

    return g_baji_photo_player.current_index;
}

static bool baji_photo_player_is_playing(void)
{
    return g_baji_photo_player.playing && (g_baji_photo_player.count > 1u);
}

static void baji_photo_player_set_playing(bool playing, const char *reason)
{
    int current_idx = baji_photo_player_get_current_index();

    if (g_baji_photo_player.count < 2u) {
        g_baji_photo_player.playing = false;
    } else {
        g_baji_photo_player.playing = playing;
    }

    baji_photo_player_trace("set_playing", reason, current_idx, current_idx);
}

static bool baji_photo_player_resolve_step(lv_dir_t dir, int *out_index, const char *reason)
{
    int current_idx;
    int target_idx;

    if ((out_index == NULL) || (g_baji_photo_player.count == 0u)) {
        return false;
    }

    current_idx = baji_photo_player_get_current_index();
    if (current_idx < 0) {
        return false;
    }

    if (dir == LV_DIR_LEFT) {
        target_idx = (current_idx + 1) % (int)g_baji_photo_player.count;
    } else if (dir == LV_DIR_RIGHT) {
        target_idx = (current_idx == 0) ? (int)(g_baji_photo_player.count - 1u) :
                                          (current_idx - 1);
    } else {
        return false;
    }

    *out_index = target_idx;
    baji_photo_player_trace("resolve_step", reason, current_idx, target_idx);
    return true;
}

static bool baji_photo_player_resolve_advance(int *out_index, const char *reason)
{
    if ((out_index == NULL) || (g_baji_photo_player.count == 0u)) {
        return false;
    }

    if (g_baji_photo_player.count == 1u) {
        *out_index = 0;
        baji_photo_player_trace("resolve_advance_single",
                                reason,
                                g_baji_photo_player.current_index,
                                0);
        return true;
    }

    return baji_photo_player_resolve_step(LV_DIR_LEFT, out_index, reason);
}

static void baji_photo_active_item_update(const baji_photo_screen_ctx_t *screen_ctx)
{
    baji_photo_active_item_reset();

    if ((screen_ctx != NULL) && screen_ctx->is_downloaded) {
        g_baji_photo_active_item_downloaded = true;
        (void)snprintf(g_baji_photo_active_downloaded_id,
                       sizeof(g_baji_photo_active_downloaded_id),
                       "%s",
                       screen_ctx->item_id);
        return;
    }

    if ((g_baji_photo_idx < 0) ||
        ((unsigned int)g_baji_photo_idx >= g_baji_photo_count) ||
        !g_baji_photo_list[g_baji_photo_idx].is_downloaded) {
        return;
    }

    g_baji_photo_active_item_downloaded = true;
    (void)snprintf(g_baji_photo_active_downloaded_id,
                   sizeof(g_baji_photo_active_downloaded_id),
                   "%s",
                   g_baji_photo_list[g_baji_photo_idx].item.id);
}

static void baji_photo_restore_active_index_after_reload(void)
{
    int idx;

    if ((g_baji_photo_state == BAJI_PHOTO_STATE_IDLE) ||
        !g_baji_photo_active_item_downloaded ||
        (g_baji_photo_active_downloaded_id[0] == '\0')) {
        return;
    }

    idx = baji_photo_find_item_index_by_id(g_baji_photo_active_downloaded_id);
    if (idx >= 0) {
        g_baji_photo_idx = idx;
        baji_photo_player_sync_current_index(idx, "restore_active");
    }
}

static void baji_photo_controls_consume_event(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }
    lv_event_stop_bubbling(e);
    lv_event_stop_processing(e);
}

static unsigned int baji_photo_controls_button_count_get(void)
{
    return 3u;
}

static lv_coord_t baji_photo_controls_resolve_panel_width(lv_obj_t *screen)
{
    lv_coord_t width;
    lv_coord_t screen_width;
    lv_coord_t max_width;

    screen_width = baji_photo_obj_valid(screen) ? lv_obj_get_width(screen) : BAJI_PHOTO_SCREEN_W;
    width = (lv_coord_t)(baji_photo_controls_button_count_get() * BAJI_PHOTO_CTRL_BTN_W);
    width += (lv_coord_t)((baji_photo_controls_button_count_get() - 1u) * BAJI_PHOTO_CTRL_BTN_GAP);
    width += (lv_coord_t)(BAJI_PHOTO_CTRL_PAD * 2);
    if (width < BAJI_PHOTO_CTRL_PANEL_MIN_W) {
        width = BAJI_PHOTO_CTRL_PANEL_MIN_W;
    }

    max_width = screen_width - BAJI_PHOTO_CTRL_MAX_MARGIN;
    if ((max_width > 0) && (width > max_width)) {
        width = max_width;
    }

    return width;
}

static void baji_photo_controls_log_layout(lv_obj_t *screen)
{
    lv_area_t panel_area = {0};
    lv_area_t delete_area = {0};
    lv_area_t play_area = {0};
    lv_area_t home_area = {0};

    if (!baji_photo_obj_valid(screen) || !baji_photo_obj_valid(g_baji_photo_controls.panel)) {
        return;
    }

    lv_obj_get_coords(g_baji_photo_controls.panel, &panel_area);
    if (baji_photo_obj_valid(g_baji_photo_controls.delete_btn)) {
        lv_obj_get_coords(g_baji_photo_controls.delete_btn, &delete_area);
    }
    if (baji_photo_obj_valid(g_baji_photo_controls.play_btn)) {
        lv_obj_get_coords(g_baji_photo_controls.play_btn, &play_area);
    }
    if (baji_photo_obj_valid(g_baji_photo_controls.home_btn)) {
        lv_obj_get_coords(g_baji_photo_controls.home_btn, &home_area);
    }

    BAJI_PHOTO_PAGE_TRACE("op=%lu controls layout screen=%dx%d panel=[%d,%d,%d,%d] delete=[%d,%d,%d,%d] play=[%d,%d,%d,%d] home=[%d,%d,%d,%d]",
                          (unsigned long)g_baji_photo_session_id,
                          (int)lv_obj_get_width(screen),
                          (int)lv_obj_get_height(screen),
                          panel_area.x1,
                          panel_area.y1,
                          panel_area.x2,
                          panel_area.y2,
                          delete_area.x1,
                          delete_area.y1,
                          delete_area.x2,
                          delete_area.y2,
                          play_area.x1,
                          play_area.y1,
                          play_area.x2,
                          play_area.y2,
                          home_area.x1,
                          home_area.y1,
                          home_area.x2,
                          home_area.y2);
}

static bool baji_photo_controls_handle_horizontal_gesture(lv_event_t *e)
{
    lv_indev_t *indev;
    lv_dir_t dir;
    int next;

    if ((e == NULL) ||
        (g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
        baji_photo_bind_ui_is_visible() ||
        (g_baji_photo_controls.msgbox != NULL) ||
        baji_photo_delete_is_pending() ||
        (g_baji_photo_count == 0u)) {
        return false;
    }

    indev = lv_indev_get_act();
    if (indev == NULL) {
        return false;
    }

    dir = lv_indev_get_gesture_dir(indev);
    if (!baji_photo_player_resolve_step(dir, &next, "gesture")) {
        return false;
    }

    g_baji_photo_controls.gesture_consumed = true;
    baji_photo_controls_consume_event(e);
    lv_indev_wait_release(indev);
    return baji_photo_switch_to_index(next,
                                      (dir == LV_DIR_LEFT) ? LV_SCR_LOAD_ANIM_MOVE_LEFT :
                                                             LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                                      "swipe");
}

static void baji_photo_controls_press_event_cb(lv_event_t *e)
{
    lv_event_code_t code;

    if (e == NULL) {
        return;
    }

    code = lv_event_get_code(e);
    if ((code != LV_EVENT_PRESSED) &&
        (code != LV_EVENT_RELEASED) &&
        (code != LV_EVENT_PRESS_LOST)) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        g_baji_photo_controls.press_target = lv_event_get_target(e);
        g_baji_photo_controls.gesture_consumed = false;
        baji_photo_controls_cancel_hide_timer();
        return;
    }

    if ((g_baji_photo_controls.msgbox == NULL) && !baji_photo_delete_is_pending()) {
        baji_photo_controls_show();
    }
}

static void baji_photo_controls_clear_refs_if_screen(lv_obj_t *screen)
{
    if ((screen == NULL) || (g_baji_photo_controls.screen != screen)) {
        return;
    }

    g_baji_photo_controls.screen = NULL;
    g_baji_photo_controls.panel = NULL;
    g_baji_photo_controls.index_label = NULL;
    g_baji_photo_controls.delete_btn = NULL;
    g_baji_photo_controls.play_btn = NULL;
    g_baji_photo_controls.home_btn = NULL;
    g_baji_photo_controls.play_label = NULL;
    g_baji_photo_controls.press_target = NULL;
    g_baji_photo_controls.gesture_consumed = false;
}

static void baji_photo_controls_cancel_hide_timer(void)
{
    if (g_baji_photo_controls.hide_timer == NULL) {
        return;
    }

    lv_timer_del(g_baji_photo_controls.hide_timer);
    g_baji_photo_controls.hide_timer = NULL;
}

static bool baji_photo_controls_can_auto_hide(void)
{
    return (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
           !baji_photo_bind_ui_is_visible() &&
           baji_photo_obj_valid(g_baji_photo_controls.panel) &&
           (g_baji_photo_controls.msgbox == NULL) &&
           !baji_photo_delete_is_pending();
}

static bool baji_photo_controls_should_preserve_switch_button_state(void)
{
    return (g_baji_photo_state == BAJI_PHOTO_STATE_TRANSITIONING) &&
           (g_baji_photo_transition == BAJI_PHOTO_TRANSITION_SWITCH) &&
           !baji_photo_bind_ui_is_visible() &&
           (g_baji_photo_controls.msgbox == NULL) &&
           !baji_photo_delete_is_pending();
}

static void baji_photo_controls_hide(void)
{
    if (!baji_photo_obj_valid(g_baji_photo_controls.panel)) {
        return;
    }

    baji_photo_controls_cancel_hide_timer();
    lv_obj_add_flag(g_baji_photo_controls.panel, LV_OBJ_FLAG_HIDDEN);
    if (baji_photo_obj_valid(g_baji_photo_controls.index_label)) {
        lv_obj_add_flag(g_baji_photo_controls.index_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void baji_photo_controls_hide_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    g_baji_photo_controls.hide_timer = NULL;
    if (!baji_photo_controls_can_auto_hide()) {
        return;
    }
    baji_photo_controls_hide();
}

static void baji_photo_controls_refresh_buttons(void)
{
    bool delete_enabled;
    bool play_enabled;
    bool preserve_switch_button_state =
        baji_photo_controls_should_preserve_switch_button_state();

    if (baji_photo_obj_valid(g_baji_photo_controls.play_label)) {
        lv_label_set_text(g_baji_photo_controls.play_label,
                          baji_photo_player_is_playing() ?
                              LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }

    delete_enabled = (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
                     !baji_photo_bind_ui_is_visible() &&
                     !baji_photo_delete_is_pending() &&
                     (g_baji_photo_controls.msgbox == NULL) &&
                     (g_baji_photo_idx >= 0) &&
                     ((unsigned int)g_baji_photo_idx < g_baji_photo_count) &&
                     g_baji_photo_list[g_baji_photo_idx].is_downloaded;
    play_enabled = (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
                   !baji_photo_bind_ui_is_visible() &&
                   (g_baji_photo_count > 1u) &&
                   (g_baji_photo_controls.msgbox == NULL) &&
                   !baji_photo_delete_is_pending();

    if (!preserve_switch_button_state &&
        baji_photo_obj_valid(g_baji_photo_controls.delete_btn)) {
        if (delete_enabled) {
            lv_obj_clear_state(g_baji_photo_controls.delete_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(g_baji_photo_controls.delete_btn, LV_STATE_DISABLED);
        }
    }
    if (!preserve_switch_button_state &&
        baji_photo_obj_valid(g_baji_photo_controls.play_btn)) {
        if (play_enabled) {
            lv_obj_clear_state(g_baji_photo_controls.play_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(g_baji_photo_controls.play_btn, LV_STATE_DISABLED);
        }
    }
    if (baji_photo_obj_valid(g_baji_photo_controls.home_btn)) {
        if (baji_photo_delete_is_pending()) {
            lv_obj_add_state(g_baji_photo_controls.home_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(g_baji_photo_controls.home_btn, LV_STATE_DISABLED);
        }
    }

    baji_photo_controls_refresh_index_label();
}

static void baji_photo_controls_refresh_index_label(void)
{
    if (!baji_photo_obj_valid(g_baji_photo_controls.index_label)) {
        return;
    }

    if (!baji_photo_obj_valid(g_baji_photo_controls.screen) ||
        !baji_photo_obj_valid(g_baji_photo_controls.panel) ||
        lv_obj_has_flag(g_baji_photo_controls.panel, LV_OBJ_FLAG_HIDDEN) ||
        (g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
        baji_photo_bind_ui_is_visible() ||
        (g_baji_photo_controls.msgbox != NULL) ||
        baji_photo_delete_is_pending() ||
        (g_baji_photo_count <= 1u) ||
        (g_baji_photo_idx < 0) ||
        ((unsigned int)g_baji_photo_idx >= g_baji_photo_count)) {
        lv_obj_add_flag(g_baji_photo_controls.index_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text_fmt(g_baji_photo_controls.index_label,
                          "%d/%u",
                          g_baji_photo_idx + 1,
                          (unsigned int)g_baji_photo_count);
    lv_obj_update_layout(g_baji_photo_controls.screen);
    lv_obj_align_to(g_baji_photo_controls.index_label,
                    g_baji_photo_controls.panel,
                    LV_ALIGN_OUT_TOP_MID,
                    0,
                    -BAJI_PHOTO_INDEX_LABEL_GAP_Y);
    lv_obj_clear_flag(g_baji_photo_controls.index_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_baji_photo_controls.index_label);
    lv_obj_move_foreground(g_baji_photo_controls.panel);
}

static void baji_photo_controls_show_async(void *param)
{
    lv_obj_t *screen = (lv_obj_t *)param;

    if ((screen != NULL) && (!baji_photo_obj_valid(screen) ||
                             (g_baji_photo_controls.screen != screen))) {
        return;
    }
    if (!baji_photo_controls_can_auto_hide()) {
        return;
    }

    baji_photo_controls_show();
}

static void baji_photo_controls_show(void)
{
    if (!baji_photo_obj_valid(g_baji_photo_controls.panel)) {
        return;
    }

    lv_obj_clear_flag(g_baji_photo_controls.panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_baji_photo_controls.panel);
    baji_photo_controls_refresh_buttons();
    baji_photo_controls_cancel_hide_timer();
    if (!baji_photo_controls_can_auto_hide()) {
        return;
    }
    g_baji_photo_controls.hide_timer = lv_timer_create(baji_photo_controls_hide_timer_cb,
                                                       BAJI_PHOTO_CONTROLS_HIDE_MS,
                                                       NULL);
    if (g_baji_photo_controls.hide_timer != NULL) {
        lv_timer_set_repeat_count(g_baji_photo_controls.hide_timer, 1);
    }
}

static void baji_photo_controls_play_timer_cb(lv_timer_t *timer)
{
    int next;

    (void)timer;

    if (!baji_photo_player_is_playing() ||
        (g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
        baji_photo_bind_ui_is_visible() ||
        (g_baji_photo_controls.msgbox != NULL) ||
        baji_photo_delete_is_pending()) {
        return;
    }

    if (!baji_photo_player_resolve_advance(&next, "timer")) {
        return;
    }

    (void)baji_photo_switch_to_index(next, LV_SCR_LOAD_ANIM_MOVE_LEFT, "timer");
}

static void baji_photo_controls_refresh_play_timer(void)
{
    bool timer_active = false;

    if (g_baji_photo_controls.play_timer == NULL) {
        g_baji_photo_controls.play_timer = lv_timer_create(baji_photo_controls_play_timer_cb,
                                                           BAJI_PHOTO_SLIDESHOW_INTERVAL_MS,
                                                           NULL);
    }

    if (g_baji_photo_controls.play_timer != NULL) {
        lv_timer_set_period(g_baji_photo_controls.play_timer,
                            g_baji_photo_player.interval_ms);
        if (baji_photo_player_is_playing() &&
            (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
            !baji_photo_bind_ui_is_visible() &&
            (g_baji_photo_controls.msgbox == NULL) &&
            !baji_photo_delete_is_pending() &&
            baji_photo_obj_valid(g_baji_photo_controls.screen)) {
            lv_timer_resume(g_baji_photo_controls.play_timer);
            timer_active = true;
        } else {
            lv_timer_pause(g_baji_photo_controls.play_timer);
        }
    }

    baji_photo_play_diag_note_timer(timer_active,
                                    g_baji_photo_player.interval_ms,
                                    "timer_refresh");
    baji_photo_controls_refresh_buttons();
}

static void baji_photo_controls_event_cb(lv_event_t *e)
{
    lv_event_code_t code;
    lv_obj_t *target;
    lv_obj_t *current_target;

    if (e == NULL) {
        return;
    }

    code = lv_event_get_code(e);
    target = lv_event_get_target(e);
    current_target = lv_event_get_current_target(e);
    if ((target == NULL) || (current_target == NULL)) {
        return;
    }

    if ((current_target != target) || (code != LV_EVENT_CLICKED)) {
        return;
    }

    if (g_baji_photo_controls.gesture_consumed &&
        (target == g_baji_photo_controls.press_target)) {
        g_baji_photo_controls.gesture_consumed = false;
        return;
    }

    g_baji_photo_controls.gesture_consumed = false;
    if (target == g_baji_photo_controls.panel) {
        return;
    }

    if (target == g_baji_photo_controls.delete_btn) {
        baji_photo_delete_dialog_open();
        return;
    }

    if (target == g_baji_photo_controls.play_btn) {
        if (g_baji_photo_player.count > 1u) {
            baji_photo_player_set_playing(!baji_photo_player_is_playing(), "play_button");
            baji_photo_controls_refresh_play_timer();
            baji_photo_controls_show();
        }
        return;
    }

    if (target == g_baji_photo_controls.home_btn) {
        if (!baji_photo_delete_is_pending()) {
            baji_photo_page_exit_to(BAJI_PHOTO_EXIT_TARGET_TIME);
        }
    }
}

static lv_obj_t *baji_photo_controls_create_button(lv_obj_t *parent,
                                                   const char *text,
                                                   lv_obj_t **out_label)
{
    lv_obj_t *btn;
    lv_obj_t *label;

    btn = lv_btn_create(parent);
    if (btn == NULL) {
        return NULL;
    }

    lv_obj_set_size(btn, BAJI_PHOTO_CTRL_BTN_W, BAJI_PHOTO_CTRL_BTN_H);
    lv_obj_set_style_radius(btn, BAJI_PHOTO_CTRL_BTN_W / 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1B232E), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_EVENT_BUBBLE |
                             LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(btn, BAJI_PHOTO_CTRL_EXT_HIT);
    lv_obj_add_event_cb(btn, baji_photo_controls_press_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn, baji_photo_controls_press_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, baji_photo_controls_press_event_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(btn, baji_photo_controls_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(btn);
    if (label == NULL) {
        return btn;
    }
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xEAF1F8), 0);
    lv_obj_center(label);
    if (out_label != NULL) {
        *out_label = label;
    }

    return btn;
}

static int baji_photo_controls_attach(lv_obj_t *screen)
{
    lv_obj_t *panel;
    lv_coord_t panel_width;

    if (!baji_photo_obj_valid(screen)) {
        return -1;
    }

    g_baji_photo_controls.screen = screen;
    g_baji_photo_controls.panel = NULL;
    g_baji_photo_controls.index_label = NULL;
    g_baji_photo_controls.delete_btn = NULL;
    g_baji_photo_controls.play_btn = NULL;
    g_baji_photo_controls.home_btn = NULL;
    g_baji_photo_controls.play_label = NULL;
    g_baji_photo_controls.press_target = NULL;
    g_baji_photo_controls.gesture_consumed = false;

    panel = lv_obj_create(screen);
    if (panel == NULL) {
        return -1;
    }

    panel_width = baji_photo_controls_resolve_panel_width(screen);
    lv_obj_set_size(panel, panel_width, BAJI_PHOTO_CTRL_PANEL_H);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -BAJI_PHOTO_CTRL_MARGIN);
    lv_obj_set_style_radius(panel, BAJI_PHOTO_CTRL_RADIUS, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x05090E), 0);
    lv_obj_set_style_bg_opa(panel, 180, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_top(panel, BAJI_PHOTO_CTRL_PAD, 0);
    lv_obj_set_style_pad_bottom(panel, BAJI_PHOTO_CTRL_PAD, 0);
    lv_obj_set_style_pad_left(panel, BAJI_PHOTO_CTRL_PAD, 0);
    lv_obj_set_style_pad_right(panel, BAJI_PHOTO_CTRL_PAD, 0);
    lv_obj_set_style_pad_row(panel, BAJI_PHOTO_CTRL_BTN_GAP, 0);
    lv_obj_set_style_pad_column(panel, BAJI_PHOTO_CTRL_BTN_GAP, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                        LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(panel, baji_photo_controls_press_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(panel, baji_photo_controls_press_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(panel, baji_photo_controls_press_event_cb, LV_EVENT_PRESS_LOST, NULL);

    g_baji_photo_controls.panel = panel;
    g_baji_photo_controls.delete_btn =
        baji_photo_controls_create_button(panel,
                                          LV_SYMBOL_TRASH,
                                          NULL);
    g_baji_photo_controls.play_btn =
        baji_photo_controls_create_button(panel,
                                          LV_SYMBOL_PLAY,
                                          &g_baji_photo_controls.play_label);
    g_baji_photo_controls.home_btn =
        baji_photo_controls_create_button(panel,
                                          LV_SYMBOL_HOME,
                                          NULL);
    if ((g_baji_photo_controls.delete_btn == NULL) ||
        (g_baji_photo_controls.play_btn == NULL) ||
        (g_baji_photo_controls.home_btn == NULL)) {
        return -1;
    }

    if (g_baji_photo_count > 1u) {
        g_baji_photo_controls.index_label = lv_label_create(screen);
        if (g_baji_photo_controls.index_label == NULL) {
            return -1;
        }
        lv_label_set_long_mode(g_baji_photo_controls.index_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(g_baji_photo_controls.index_label, LV_FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_baji_photo_controls.index_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(g_baji_photo_controls.index_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_baji_photo_controls.index_label, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_baji_photo_controls.index_label, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_radius(g_baji_photo_controls.index_label, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_baji_photo_controls.index_label, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(g_baji_photo_controls.index_label, BAJI_PHOTO_INDEX_LABEL_PAD_H, LV_PART_MAIN);
        lv_obj_set_style_pad_right(g_baji_photo_controls.index_label, BAJI_PHOTO_INDEX_LABEL_PAD_H, LV_PART_MAIN);
        lv_obj_set_style_pad_top(g_baji_photo_controls.index_label, BAJI_PHOTO_INDEX_LABEL_PAD_V, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(g_baji_photo_controls.index_label, BAJI_PHOTO_INDEX_LABEL_PAD_V, LV_PART_MAIN);
        lv_obj_add_flag(g_baji_photo_controls.index_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_move_foreground(panel);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    baji_photo_controls_refresh_buttons();
    baji_photo_controls_refresh_play_timer();
    lv_obj_update_layout(screen);
    baji_photo_controls_log_layout(screen);
    return 0;
}

static void baji_photo_delete_dialog_close(bool resume_controls)
{
    lv_obj_t *msgbox = g_baji_photo_controls.msgbox;

    g_baji_photo_controls.msgbox = NULL;
    if (baji_photo_obj_valid(msgbox)) {
        lv_msgbox_close_async(msgbox);
    }

    baji_photo_controls_refresh_buttons();
    baji_photo_controls_refresh_play_timer();
    if (resume_controls) {
        baji_photo_controls_show();
    } else {
        baji_photo_controls_cancel_hide_timer();
    }
}

static void baji_photo_delete_task(void *arg)
{
    baji_photo_delete_request_t *request = (baji_photo_delete_request_t *)arg;
    baji_photo_store_delete_result_t store_result;
    baji_photo_delete_result_t result;

    memset(&store_result, 0, sizeof(store_result));
    memset(&result, 0, sizeof(result));
    if (request != NULL) {
        result.valid = true;
        result.session_id = request->session_id;
        result.request_id = request->request_id;
        result.ret = baji_photo_store_delete_item_and_index(&request->item, &store_result);
        result.cleanup_pending = (result.ret == 0) && store_result.cleanup_pending;
        BAJI_PHOTO_PAGE_TRACE("op=%lu delete task done request=%lu id=%s ret=%d cleanup_pending=%d remaining=%u",
                              (unsigned long)request->session_id,
                              (unsigned long)request->request_id,
                              request->item.id,
                              result.ret,
                              result.cleanup_pending ? 1 : 0,
                              store_result.remaining_count);
    } else {
        result.valid = true;
        result.ret = LIOT_EXTFLASH_INVALID_PARAMETER;
        BAJI_PHOTO_PAGE_TRACE("op=%lu delete task invalid request", (unsigned long)g_baji_photo_session_id);
    }

    baji_photo_page_mailbox_post_delete_result(&result);

    liot_rtos_enter_critical();
    g_baji_photo_delete_task = NULL;
    liot_rtos_exit_critical();

    if (request != NULL) {
        liot_rtos_free(request);
    }
    liot_rtos_task_delete(NULL);
}

static void baji_photo_delete_schedule_async(void *param)
{
    baji_photo_delete_request_t *request = NULL;
    int ret;

    (void)param;

    if (!g_baji_photo_delete_request.valid ||
        !g_baji_photo_delete_request.start_after_delete ||
        (g_baji_photo_delete_task != NULL)) {
        return;
    }

    request = (baji_photo_delete_request_t *)liot_rtos_malloc(sizeof(*request));
    if (request == NULL) {
        baji_photo_delete_result_t result = {0};

        result.valid = true;
        result.session_id = g_baji_photo_delete_request.session_id;
        result.request_id = g_baji_photo_delete_request.request_id;
        result.ret = LIOT_EXTFLASH_ERROR_GENERAL;
        baji_photo_page_mailbox_post_delete_result(&result);
        g_baji_photo_delete_request.start_after_delete = false;
        return;
    }

    *request = g_baji_photo_delete_request;
    g_baji_photo_delete_request.start_after_delete = false;
    BAJI_PHOTO_PAGE_TRACE("op=%lu delete schedule request=%lu id=%s exit_home=%d",
                          (unsigned long)request->session_id,
                          (unsigned long)request->request_id,
                          request->item.id,
                          request->exit_home ? 1 : 0);
    ret = liot_rtos_task_create(&g_baji_photo_delete_task,
                                BAJI_PHOTO_DELETE_TASK_STACK,
                                BAJI_PHOTO_DELETE_TASK_PRIO,
                                "baji_photo_delete",
                                baji_photo_delete_task,
                                request);
    if (ret != 0) {
        baji_photo_delete_result_t result = {0};

        liot_rtos_free(request);
        g_baji_photo_delete_task = NULL;
        result.valid = true;
        result.session_id = g_baji_photo_delete_request.session_id;
        result.request_id = g_baji_photo_delete_request.request_id;
        result.ret = ret;
        baji_photo_page_mailbox_post_delete_result(&result);
        BAJI_PHOTO_PAGE_TRACE("op=%lu delete schedule failed request=%lu ret=%d",
                              (unsigned long)g_baji_photo_delete_request.session_id,
                              (unsigned long)g_baji_photo_delete_request.request_id,
                              ret);
    }
}

static int baji_photo_empty_state_attach(lv_obj_t *screen)
{
    lv_obj_t *title;
    lv_obj_t *body;

    if (!baji_photo_obj_valid(screen)) {
        return -1;
    }

    title = lv_label_create(screen);
    body = lv_label_create(screen);
    if ((title == NULL) || (body == NULL)) {
        return -1;
    }

    lv_label_set_text(title, BAJI_PHOTO_EMPTY_TITLE);
    lv_obj_set_width(title, 260);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -18);

    g_baji_photo_empty_body_label = body;
    baji_photo_empty_state_refresh();
    lv_obj_set_width(body, 260);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(body, lv_color_make(166, 176, 192), 0);
    lv_obj_align_to(body, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    return 0;
}

static void baji_photo_empty_state_refresh(void)
{
    const char *body_text = "";
    baji_photo_bind_status_t bind_status;

    if ((g_baji_photo_empty_body_label == NULL) ||
        !lv_obj_is_valid(g_baji_photo_empty_body_label)) {
        g_baji_photo_empty_body_label = NULL;
        return;
    }

    bind_status = baji_photo_mqtt_get_bind_status();
    switch (bind_status) {
    case BAJI_PHOTO_BIND_BOUND:
        body_text = BAJI_PHOTO_EMPTY_BODY_BOUND;
        break;
    case BAJI_PHOTO_BIND_DISABLED:
        body_text = BAJI_PHOTO_EMPTY_BODY_DISABLED;
        break;
    case BAJI_PHOTO_BIND_UNKNOWN:
        body_text = BAJI_PHOTO_EMPTY_BODY_UNKNOWN;
        break;
    case BAJI_PHOTO_BIND_UNBOUND:
    default:
        /* The bind overlay owns the unbound guidance and QR code. */
        body_text = "";
        break;
    }

    lv_label_set_text(g_baji_photo_empty_body_label, body_text);
}

static void baji_photo_delete_msgbox_event_cb(lv_event_t *e)
{
    lv_event_code_t code;
    lv_obj_t *msgbox;
    lv_obj_t *target;
    int active_btn;

    if (e == NULL) {
        return;
    }

    code = lv_event_get_code(e);
    target = lv_event_get_target(e);
    msgbox = lv_event_get_current_target(e);
    if (msgbox == NULL) {
        return;
    }

    if (code == LV_EVENT_DELETE) {
        if (g_baji_photo_controls.msgbox == msgbox) {
            g_baji_photo_controls.msgbox = NULL;
        }
        baji_photo_controls_refresh_buttons();
        baji_photo_controls_refresh_play_timer();
        return;
    }

    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    active_btn = lv_msgbox_get_active_btn(msgbox);
    BAJI_PHOTO_PAGE_TRACE("op=%lu delete dialog event active_btn=%d msgbox=%p target=%p idx=%d count=%u pending=%d",
                          (unsigned long)g_baji_photo_session_id,
                          active_btn,
                          msgbox,
                          target,
                          g_baji_photo_idx,
                          g_baji_photo_count,
                          baji_photo_delete_is_pending() ? 1 : 0);
    if (active_btn != BAJI_PHOTO_DELETE_MSGBOX_CONFIRM_BTN) {
        baji_photo_delete_dialog_close(true);
        return;
    }

    if ((g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
        (g_baji_photo_idx < 0) ||
        ((unsigned int)g_baji_photo_idx >= g_baji_photo_count) ||
        !g_baji_photo_list[g_baji_photo_idx].is_downloaded ||
        baji_photo_delete_is_pending()) {
        baji_photo_delete_dialog_close(true);
        return;
    }

    memset(&g_baji_photo_delete_request, 0, sizeof(g_baji_photo_delete_request));
    g_baji_photo_delete_request.valid = true;
    g_baji_photo_delete_request.start_after_delete = true;
    g_baji_photo_delete_request.exit_home = (g_baji_photo_count <= 1u);
    g_baji_photo_delete_request.session_id = g_baji_photo_session_id;
    g_baji_photo_delete_request.request_id = baji_photo_diag_next_id();
    g_baji_photo_delete_request.item = g_baji_photo_list[g_baji_photo_idx].item;
    BAJI_PHOTO_PAGE_TRACE("op=%lu delete confirm request=%lu idx=%d id=%s exit_home=%d count=%u",
                          (unsigned long)g_baji_photo_delete_request.session_id,
                          (unsigned long)g_baji_photo_delete_request.request_id,
                          g_baji_photo_idx,
                          g_baji_photo_delete_request.item.id,
                          g_baji_photo_delete_request.exit_home ? 1 : 0,
                          g_baji_photo_count);

    baji_photo_delete_dialog_close(false);
    baji_photo_controls_refresh_buttons();
    baji_photo_controls_refresh_play_timer();
    baji_photo_controls_cancel_hide_timer();

    if (g_baji_photo_delete_request.exit_home) {
        baji_photo_page_exit_to(BAJI_PHOTO_EXIT_TARGET_TIME);
        return;
    }

    {
        int next_idx;

        if (!baji_photo_player_resolve_advance(&next_idx, "delete_confirm") ||
            !baji_photo_switch_to_index(next_idx, LV_SCR_LOAD_ANIM_MOVE_LEFT, "delete_confirm")) {
            memset(&g_baji_photo_delete_request, 0, sizeof(g_baji_photo_delete_request));
            baji_photo_sync_label_show("delete failed", BAJI_PHOTO_SYNC_RESULT_MS);
            baji_photo_controls_refresh_buttons();
            baji_photo_controls_refresh_play_timer();
        }
    }
}

static void baji_photo_delete_dialog_open(void)
{
    static const char *btns[] = {
        BAJI_PHOTO_DELETE_MSGBOX_CANCEL_TEXT,
        BAJI_PHOTO_DELETE_MSGBOX_CONFIRM_TEXT,
        ""
    };
    lv_obj_t *msgbox;
    lv_obj_t *title;
    lv_obj_t *text;
    lv_obj_t *btns_obj;

    if ((g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
        baji_photo_bind_ui_is_visible() ||
        (g_baji_photo_idx < 0) ||
        ((unsigned int)g_baji_photo_idx >= g_baji_photo_count) ||
        !g_baji_photo_list[g_baji_photo_idx].is_downloaded ||
        (g_baji_photo_controls.msgbox != NULL) ||
        baji_photo_delete_is_pending()) {
        return;
    }

    baji_photo_controls_cancel_hide_timer();
    baji_photo_controls_refresh_play_timer();
    msgbox = lv_msgbox_create(NULL,
                              BAJI_PHOTO_DELETE_MSGBOX_TITLE,
                              BAJI_PHOTO_DELETE_MSGBOX_TEXT,
                              btns,
                              true);
    if (!baji_photo_obj_valid(msgbox)) {
        return;
    }

    g_baji_photo_controls.msgbox = msgbox;
    lv_obj_center(msgbox);
    lv_obj_set_style_text_font(msgbox, LV_FONT_DEFAULT, 0);
    title = lv_msgbox_get_title(msgbox);
    text = lv_msgbox_get_text(msgbox);
    btns_obj = lv_msgbox_get_btns(msgbox);
    if (baji_photo_obj_valid(title)) {
        lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    }
    if (baji_photo_obj_valid(text)) {
        lv_obj_set_style_text_font(text, LV_FONT_DEFAULT, 0);
    }
    if (baji_photo_obj_valid(btns_obj)) {
        lv_obj_set_style_text_font(btns_obj, LV_FONT_DEFAULT, 0);
    }
    lv_obj_add_event_cb(msgbox, baji_photo_delete_msgbox_event_cb, LV_EVENT_ALL, NULL);
    baji_photo_controls_refresh_buttons();
    baji_photo_controls_refresh_play_timer();
    baji_photo_controls_show();
}

static void baji_photo_interaction_event_cb(lv_event_t *e)
{
    lv_event_code_t code;
    lv_indev_t *indev;
    lv_point_t point;
    int32_t dx;
    int32_t dy;

    if (e == NULL) {
        return;
    }

    code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        if ((lv_event_get_target(e) == lv_event_get_current_target(e)) &&
            (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
            !baji_photo_delete_is_pending() &&
            !baji_photo_bind_ui_is_visible()) {
            g_baji_photo_exit_press_candidate = true;
            indev = lv_indev_get_act();
            if (indev == NULL) {
                g_baji_photo_exit_press_candidate = false;
            } else {
                lv_indev_get_point(indev, &g_baji_photo_exit_press_point);
                g_baji_photo_exit_press_tick = lv_tick_get();
            }
        } else {
            g_baji_photo_exit_press_candidate = false;
        }

        if ((g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
            !baji_photo_delete_is_pending() &&
            !baji_photo_bind_ui_is_visible()) {
            baji_photo_controls_show();
        }
        return;
    }

    if (code == LV_EVENT_PRESSING && g_baji_photo_exit_press_candidate) {
        indev = lv_indev_get_act();
        if (indev == NULL) {
            g_baji_photo_exit_press_candidate = false;
            return;
        }
        lv_indev_get_point(indev, &point);
        dx = point.x - g_baji_photo_exit_press_point.x;
        dy = point.y - g_baji_photo_exit_press_point.y;
        if (dx > BAJI_PHOTO_EXIT_MOVE_LIMIT || dx < -BAJI_PHOTO_EXIT_MOVE_LIMIT ||
            dy > BAJI_PHOTO_EXIT_MOVE_LIMIT || dy < -BAJI_PHOTO_EXIT_MOVE_LIMIT) {
            g_baji_photo_exit_press_candidate = false;
            return;
        }
        if (lv_tick_elaps(g_baji_photo_exit_press_tick) >= BAJI_PHOTO_EXIT_HOLD_MS) {
            g_baji_photo_exit_press_candidate = false;
            lv_event_stop_bubbling(e);
            lv_event_stop_processing(e);
            lv_indev_wait_release(indev);
            baji_photo_page_exit_to(BAJI_PHOTO_EXIT_TARGET_BAJI);
        }
        return;
    }

    if ((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST)) {
        g_baji_photo_exit_press_candidate = false;
        return;
    }

    if ((code == LV_EVENT_CLICKED) || (code == LV_EVENT_GESTURE)) {
        g_baji_photo_exit_press_candidate = false;
        return;
    }
}

static void baji_photo_dynamic_src_release(baji_photo_dynamic_src_t *dyn, const char *reason)
{
    void *buf = NULL;

    if (dyn == NULL) {
        return;
    }

    baji_photo_page_marker_dyn("UI_DYN_RELEASE_BEGIN", dyn, reason);
    if (dyn->buf != NULL) {
        lv_img_cache_invalidate_src(&dyn->dsc);
        baji_photo_page_marker_dyn("UI_DYN_CACHE_INVALIDATED", dyn, reason);
        buf = dyn->buf;
        baji_photo_dynamic_src_free_buf(dyn);
        dyn->buf = NULL;
        BAJI_PHOTO_MARK_TRACE("UI_DYN_BUF_FREED op=%lu dyn=%lu idx=%d item=%s bytes=%lu buf=%p reason=%s heap_min=%lu",
                              (unsigned long)dyn->session_id,
                              (unsigned long)dyn->dyn_seq,
                              dyn->idx,
                              baji_photo_diag_id_tail(dyn->item_id),
                              (unsigned long)dyn->dsc.data_size,
                              buf,
                              (reason != NULL) ? reason : "-",
                              (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    }
    dyn->img = NULL;
    baji_photo_page_marker_dyn("UI_DYN_RELEASE_DONE", dyn, reason);
    lv_mem_free(dyn);
}

static void baji_photo_dynamic_src_free_buf(baji_photo_dynamic_src_t *dyn)
{
    if ((dyn == NULL) || (dyn->buf == NULL)) {
        return;
    }

    switch (dyn->format) {
    case BAJI_PHOTO_FORMAT_JPEG:
    case BAJI_PHOTO_FORMAT_PNG:
        baji_photo_vpu_img_release(dyn->buf);
        break;
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        baji_photo_flash_img_release(dyn->buf);
        break;
    }
}

static void baji_photo_dynamic_src_release_async(void *param)
{
    baji_photo_dynamic_src_t *dyn = (baji_photo_dynamic_src_t *)param;

    if (dyn == NULL) {
        return;
    }

    BAJI_PHOTO_PAGE_TRACE("op=%lu dyn async release idx=%d id=%s dyn=%p dyn_seq=%lu",
                          (unsigned long)dyn->session_id,
                          dyn->idx,
                          dyn->item_id,
                          dyn,
                          (unsigned long)dyn->dyn_seq);
    baji_photo_dynamic_src_release(dyn, "img_delete_async");
}

static void baji_photo_dynamic_img_event_handler(lv_event_t *e)
{
    void *user_data;
    baji_photo_dynamic_src_t *dyn;
    uint32_t session_id;
    uint32_t magic = 0u;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    user_data = lv_event_get_user_data(e);
    dyn = NULL;
    if (user_data != NULL) {
        magic = *((const uint32_t *)user_data);
    }
    if (magic == BAJI_PHOTO_DYN_CTX_MAGIC) {
        dyn = (baji_photo_dynamic_src_t *)user_data;
    }
    session_id = (dyn != NULL) ? dyn->session_id : g_baji_photo_session_id;
    BAJI_PHOTO_PAGE_TRACE("op=%lu img delete idx=%d id=%s kind=%s dyn=%p dyn_seq=%lu img=%p bytes=%lu",
                          (unsigned long)session_id,
                          (dyn != NULL) ? dyn->idx : g_baji_photo_idx,
                          (dyn != NULL) ? dyn->item_id : baji_photo_item_id_get(g_baji_photo_idx),
                          (dyn != NULL) ? baji_photo_ctx_kind(dyn->is_downloaded, dyn->format)
                                        : baji_photo_item_kind_get(g_baji_photo_idx),
                          dyn,
                          (unsigned long)((dyn != NULL) ? dyn->dyn_seq : 0u),
                          (dyn != NULL) ? dyn->img : NULL,
                          (unsigned long)((dyn != NULL) ? dyn->dsc.data_size : 0u));
    baji_photo_page_marker("UI_IMAGE_DELETE",
                           session_id,
                           (dyn != NULL) ? dyn->idx : g_baji_photo_idx,
                           (dyn != NULL) ? dyn->item_id : baji_photo_item_id_get(g_baji_photo_idx),
                           NULL,
                           NULL);
    baji_photo_play_diag_note_dyn_delete(dyn, "img_delete");
    if (dyn != NULL) {
        lv_res_t async_ret;

        dyn->img = NULL;
        if (dyn->release_queued) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu img delete release already queued dyn=%p dyn_seq=%lu",
                                  (unsigned long)session_id,
                                  dyn,
                                  (unsigned long)dyn->dyn_seq);
        } else {
            dyn->release_queued = true;
            async_ret = lv_async_call(baji_photo_dynamic_src_release_async, dyn);
            BAJI_PHOTO_PAGE_TRACE("op=%lu img delete queue release dyn=%p dyn_seq=%lu ret=%d",
                                  (unsigned long)session_id,
                                  dyn,
                                  (unsigned long)dyn->dyn_seq,
                                  (int)async_ret);
            if (async_ret != LV_RES_OK) {
                BAJI_PHOTO_PAGE_TRACE("op=%lu img delete async queue failed dyn=%p dyn_seq=%lu",
                                      (unsigned long)session_id,
                                      dyn,
                                      (unsigned long)dyn->dyn_seq);
                dyn->release_queued = false;
                baji_photo_dynamic_src_release(dyn, "img_delete_sync_fallback");
            }
        }
    }
    baji_photo_page_log_runtime("img_delete", session_id);
}

static int baji_photo_find_item_index_by_id(const char *id)
{
    unsigned int i;

    if (id == NULL) {
        return -1;
    }

    for (i = 0; i < g_baji_photo_count; ++i) {
        if (g_baji_photo_list[i].is_downloaded &&
            (strcmp(g_baji_photo_list[i].item.id, id) == 0)) {
            return (int)i;
        }
    }

    return -1;
}

static void baji_photo_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    baji_photo_screen_ctx_t *screen_ctx =
        (baji_photo_screen_ctx_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_GESTURE) {
        (void)baji_photo_controls_handle_horizontal_gesture(e);
        return;
    }

    if (code == LV_EVENT_DELETE) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu screen delete idx=%d id=%s kind=%s dyn=%p dyn_seq=%lu",
                              (unsigned long)((screen_ctx != NULL) ? screen_ctx->session_id
                                                                   : g_baji_photo_session_id),
                              (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                              (screen_ctx != NULL) ? screen_ctx->item_id
                                                   : baji_photo_item_id_get(g_baji_photo_idx),
                              (screen_ctx != NULL) ? baji_photo_ctx_kind(screen_ctx->is_downloaded,
                                                                          screen_ctx->format)
                                                   : baji_photo_item_kind_get(g_baji_photo_idx),
                              (screen_ctx != NULL) ? screen_ctx->dyn : NULL,
                              (unsigned long)((screen_ctx != NULL) ? screen_ctx->dyn_seq : 0u));
        baji_photo_page_marker("UI_SCREEN_DELETE",
                               (screen_ctx != NULL) ? screen_ctx->session_id : g_baji_photo_session_id,
                               (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                               (screen_ctx != NULL) ? screen_ctx->item_id : baji_photo_item_id_get(g_baji_photo_idx),
                               NULL,
                               NULL);
        if (g_baji_photo_delete_request.valid &&
            g_baji_photo_delete_request.start_after_delete &&
            (screen_ctx != NULL) &&
            (screen_ctx->session_id == g_baji_photo_delete_request.session_id) &&
            (strcmp(screen_ctx->item_id, g_baji_photo_delete_request.item.id) == 0)) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu screen delete trigger delete request=%lu id=%s",
                                  (unsigned long)screen_ctx->session_id,
                                  (unsigned long)g_baji_photo_delete_request.request_id,
                                  screen_ctx->item_id);
            lv_async_call(baji_photo_delete_schedule_async, NULL);
        }
        baji_photo_controls_clear_refs_if_screen(target);
        baji_photo_bind_ui_clear_screen(target);
        g_baji_photo_empty_body_label = NULL;
        if ((screen_ctx != NULL) && (screen_ctx->dyn != NULL)) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu screen delete handoff dyn idx=%d id=%s dyn=%p dyn_seq=%lu owner=img_delete",
                                  (unsigned long)screen_ctx->session_id,
                                  screen_ctx->idx,
                                  screen_ctx->item_id,
                                  screen_ctx->dyn,
                                  (unsigned long)screen_ctx->dyn_seq);
            BAJI_PHOTO_MARK_TRACE("UI_DYN_OWNER_IMG_DELETE op=%lu dyn=%lu idx=%d item=%s heap_min=%lu",
                                  (unsigned long)screen_ctx->session_id,
                                  (unsigned long)screen_ctx->dyn_seq,
                                  screen_ctx->idx,
                                  baji_photo_diag_id_tail(screen_ctx->item_id),
                                  (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
            screen_ctx->dyn = NULL;
            screen_ctx->dyn_seq = 0u;
        }
        if (screen_ctx != NULL) {
            liot_rtos_free(screen_ctx);
        }
        return;
    }

    if (code == LV_EVENT_SCREEN_LOADED) {
        uint64_t display_time_ms = 0u;
        bool show_controls = (g_baji_photo_play_diag.last_trigger != NULL) &&
                             (strcmp(g_baji_photo_play_diag.last_trigger, "swipe") == 0);

        BAJI_PHOTO_PAGE_TRACE("op=%lu screen loaded idx=%d id=%s kind=%s state=%d trans=%d mqtt_task_op=%lu mqtt_task=%s mqtt_image=%s pending=%d",
                              (unsigned long)((screen_ctx != NULL) ? screen_ctx->session_id
                                                                   : g_baji_photo_session_id),
                              (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                              (screen_ctx != NULL) ? screen_ctx->item_id
                                                   : baji_photo_item_id_get(g_baji_photo_idx),
                              (screen_ctx != NULL) ? baji_photo_ctx_kind(screen_ctx->is_downloaded,
                                                                          screen_ctx->format)
                                                   : baji_photo_item_kind_get(g_baji_photo_idx),
                              (int)g_baji_photo_state,
                              (int)g_baji_photo_transition,
                              (unsigned long)g_baji_photo_mqtt_display_trace_id,
                              g_baji_photo_mqtt_display_task_id[0] != '\0' ?
                                  g_baji_photo_mqtt_display_task_id : "-",
                              g_baji_photo_mqtt_display_image_id[0] != '\0' ?
                                  g_baji_photo_mqtt_display_image_id : "-",
                              g_baji_photo_mqtt_display_pending ? 1 : 0);
        baji_photo_page_marker("UI_SCREEN_LOADED",
                               (screen_ctx != NULL) ? screen_ctx->session_id : g_baji_photo_session_id,
                               (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                               (screen_ctx != NULL) ? screen_ctx->item_id
                                                    : baji_photo_item_id_get(g_baji_photo_idx),
                               g_baji_photo_mqtt_display_pending ? g_baji_photo_mqtt_display_task_id : NULL,
                               g_baji_photo_mqtt_display_pending ? g_baji_photo_mqtt_display_image_id : NULL);
        if (g_baji_photo_transition == BAJI_PHOTO_TRANSITION_ENTER ||
            g_baji_photo_transition == BAJI_PHOTO_TRANSITION_SWITCH) {
            g_baji_photo_state = BAJI_PHOTO_STATE_ACTIVE;
            g_baji_photo_transition = BAJI_PHOTO_TRANSITION_NONE;
        }
        if (screen_ctx != NULL) {
            baji_photo_player_sync_current_index(screen_ctx->idx, "screen_loaded");
        }
        baji_photo_play_diag_note_loaded(screen_ctx, target, "screen_loaded");
        baji_photo_active_item_update(screen_ctx);
        baji_photo_controls_hide();
        baji_photo_controls_refresh_play_timer();
        baji_photo_bind_ui_set_screen(target);
        baji_photo_bind_ui_tick(true);
        if (show_controls) {
            lv_async_call(baji_photo_controls_show_async, target);
        }
        baji_photo_play_diag_persist_if_needed(screen_ctx);
        if (g_baji_photo_mqtt_display_pending &&
            (screen_ctx != NULL) &&
            (strcmp(screen_ctx->item_id, g_baji_photo_mqtt_display_image_id) == 0)) {
            if (Liot_GetTimestamp(&display_time_ms) != 0) {
                display_time_ms = 0u;
            }
            BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display loaded task_op=%lu task=%s image=%s idx=%d",
                                  (unsigned long)screen_ctx->session_id,
                                  (unsigned long)g_baji_photo_mqtt_display_trace_id,
                                  g_baji_photo_mqtt_display_task_id,
                                  g_baji_photo_mqtt_display_image_id,
                                  screen_ctx->idx);
            baji_photo_page_marker_display_result("UI_DISPLAY_LOADED",
                                                  g_baji_photo_mqtt_display_trace_id,
                                                  g_baji_photo_mqtt_display_task_id,
                                                  g_baji_photo_mqtt_display_image_id,
                                                  0,
                                                  display_time_ms,
                                                  0);
            if (baji_photo_page_notify_mqtt_display_result(screen_ctx->session_id,
                                                           g_baji_photo_mqtt_display_trace_id,
                                                           g_baji_photo_mqtt_display_task_id,
                                                           g_baji_photo_mqtt_display_image_id,
                                                           0,
                                                           display_time_ms,
                                                           NULL) == 0) {
                baji_photo_page_clear_mqtt_display();
            }
        }
        baji_photo_page_log_current_item("screen_loaded",
                                         (screen_ctx != NULL) ? screen_ctx->session_id
                                                              : g_baji_photo_session_id,
                                         (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx);
        return;
    }

    if (code == LV_EVENT_SCREEN_UNLOADED) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu screen unloaded idx=%d id=%s kind=%s state=%d trans=%d",
                              (unsigned long)((screen_ctx != NULL) ? screen_ctx->session_id
                                                                   : g_baji_photo_session_id),
                              (screen_ctx != NULL) ? screen_ctx->idx : g_baji_photo_idx,
                              (screen_ctx != NULL) ? screen_ctx->item_id
                                                   : baji_photo_item_id_get(g_baji_photo_idx),
                              (screen_ctx != NULL) ? baji_photo_ctx_kind(screen_ctx->is_downloaded,
                                                                          screen_ctx->format)
                                                   : baji_photo_item_kind_get(g_baji_photo_idx),
                              (int)g_baji_photo_state,
                              (int)g_baji_photo_transition);
        baji_photo_controls_cancel_hide_timer();
        if (g_baji_photo_transition == BAJI_PHOTO_TRANSITION_EXIT) {
            g_baji_photo_state = BAJI_PHOTO_STATE_IDLE;
            g_baji_photo_transition = BAJI_PHOTO_TRANSITION_NONE;
            baji_photo_active_item_reset();
        }
        baji_photo_controls_refresh_play_timer();
        return;
    }
}

static void baji_photo_reload_list(void)
{
    baji_photo_manifest_item_t downloaded[BAJI_PHOTO_DL_MAX];
    unsigned int downloaded_count = 0;
    int player_idx = baji_photo_player_get_current_index();
    unsigned int old_idx = (player_idx >= 0) ? (unsigned int)player_idx : 0u;
    unsigned int i;

    memset(g_baji_photo_list, 0, sizeof(g_baji_photo_list));
#if BAJI_PHOTO_ENABLE_BUILTIN_DISPLAY
    for (i = 0; i < BAJI_PHOTO_CNT; ++i) {
        g_baji_photo_list[i].builtin = g_baji_photos[i];
        g_baji_photo_list[i].is_downloaded = false;
    }
    g_baji_photo_count = BAJI_PHOTO_CNT;
#else
    g_baji_photo_count = 0u;
#endif

    if (baji_photo_store_load_index(downloaded, BAJI_PHOTO_DL_MAX, &downloaded_count) != 0) {
        baji_photo_player_sync_count(g_baji_photo_count, "reload_load_fail");
        if (g_baji_photo_count > 0u) {
            baji_photo_player_sync_current_index(g_baji_photo_idx, "reload_load_fail");
        }
        baji_photo_page_log_store_inventory("reload_load_fail",
                                            g_baji_photo_session_id,
                                            NULL,
                                            0u);
        return;
    }

    for (i = 0; (i < downloaded_count) && (g_baji_photo_count < BAJI_PHOTO_TOTAL_MAX); ++i) {
        g_baji_photo_list[g_baji_photo_count].is_downloaded = true;
        g_baji_photo_list[g_baji_photo_count].item = downloaded[i];
        ++g_baji_photo_count;
    }

    if (g_baji_photo_count == 0u) {
        g_baji_photo_idx = 0;
    } else if (old_idx >= g_baji_photo_count) {
        g_baji_photo_idx = (int)(g_baji_photo_count - 1u);
    } else {
        g_baji_photo_idx = (int)old_idx;
    }

    baji_photo_player_sync_count(g_baji_photo_count, "reload");
    if (g_baji_photo_count > 0u) {
        baji_photo_player_sync_current_index(g_baji_photo_idx, "reload_base");
    }

    baji_photo_page_log_store_inventory("reload",
                                        g_baji_photo_session_id,
                                        downloaded,
                                        downloaded_count);
}

static bool baji_photo_switch_to_index(int idx, lv_scr_load_anim_t anim, const char *trigger)
{
    lv_obj_t *scr;
    int old_idx = g_baji_photo_idx;
    bool delete_handoff = (trigger != NULL) &&
                          (strcmp(trigger, "delete_confirm") == 0);

    if ((idx < 0) || ((unsigned int)idx >= g_baji_photo_count)) {
        return false;
    }
    if (g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu switch blocked trigger=%s reason=state state=%d trans=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (trigger != NULL) ? trigger : "-",
                              (int)g_baji_photo_state,
                              (int)g_baji_photo_transition);
        return false;
    }
    if ((g_baji_photo_transition != BAJI_PHOTO_TRANSITION_NONE) ||
        (g_baji_photo_controls.msgbox != NULL) ||
        (baji_photo_delete_is_pending() && !delete_handoff)) {
        const char *reason = "transition";

        if (g_baji_photo_controls.msgbox != NULL) {
            reason = "dialog_open";
        } else if (baji_photo_delete_is_pending() && !delete_handoff) {
            reason = "delete_pending";
        }
        BAJI_PHOTO_PAGE_TRACE("op=%lu switch blocked trigger=%s reason=%s state=%d trans=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (trigger != NULL) ? trigger : "-",
                              reason,
                              (int)g_baji_photo_state,
                              (int)g_baji_photo_transition);
        return false;
    }

    baji_photo_play_diag_mark_switch(trigger, old_idx, idx, anim);
    g_baji_photo_state = BAJI_PHOTO_STATE_TRANSITIONING;
    g_baji_photo_transition = BAJI_PHOTO_TRANSITION_SWITCH;
    baji_photo_controls_hide();
    baji_photo_controls_cancel_hide_timer();
    baji_photo_controls_refresh_play_timer();
    BAJI_PHOTO_PAGE_TRACE("op=%lu switch seq=%lu trigger=%s idx=%d id=%s kind=%s anim=%d",
                          (unsigned long)g_baji_photo_session_id,
                          (unsigned long)g_baji_photo_play_diag.switch_seq,
                          (trigger != NULL) ? trigger : "-",
                          idx,
                          baji_photo_item_id_get(idx),
                          baji_photo_item_kind_get(idx),
                          (int)anim);
    baji_photo_page_log_runtime("switch_before", g_baji_photo_session_id);

    scr = baji_photo_setup_scr(idx);
    if (scr == NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu switch fail idx=%d id=%s kind=%s reason=setup_null",
                              (unsigned long)g_baji_photo_session_id,
                              idx,
                              baji_photo_item_id_get(idx),
                              baji_photo_item_kind_get(idx));
        g_baji_photo_state = BAJI_PHOTO_STATE_ACTIVE;
        g_baji_photo_transition = BAJI_PHOTO_TRANSITION_NONE;
        g_baji_photo_idx = old_idx;
        baji_photo_play_diag_log("switch_fail", idx, 0u, NULL);
        baji_photo_controls_refresh_play_timer();
        return false;
    }

    g_baji_photo_idx = idx;
    lv_scr_load_anim(scr, anim, BAJI_PHOTO_ANIM_TIME_MS, 0, true);
    return true;
}

static lv_obj_t *baji_photo_sync_label_parent_get(void)
{
    lv_obj_t *parent = lv_scr_act();

    if ((parent != NULL) && lv_obj_is_valid(parent)) {
        return parent;
    }

    parent = guider_ui.baji;
    if ((parent != NULL) && lv_obj_is_valid(parent)) {
        return parent;
    }

    return NULL;
}

static void baji_photo_sync_label_show(const char *text, uint32_t visible_ms)
{
    lv_obj_t *parent = baji_photo_sync_label_parent_get();

    if ((parent == NULL) || !lv_obj_is_valid(parent)) {
        g_baji_photo_sync_label = NULL;
        return;
    }

    if ((g_baji_photo_sync_label != NULL) &&
        (!lv_obj_is_valid(g_baji_photo_sync_label) ||
         (lv_obj_get_parent(g_baji_photo_sync_label) != parent))) {
        g_baji_photo_sync_label = NULL;
    }

    if ((g_baji_photo_sync_label == NULL) || !lv_obj_is_valid(g_baji_photo_sync_label)) {
        g_baji_photo_sync_label = lv_label_create(parent);
        if (g_baji_photo_sync_label == NULL) {
            return;
        }
        lv_obj_set_size(g_baji_photo_sync_label, 220, LV_SIZE_CONTENT);
        lv_label_set_long_mode(g_baji_photo_sync_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(g_baji_photo_sync_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(g_baji_photo_sync_label, lv_color_white(), 0);
        lv_obj_set_style_bg_color(g_baji_photo_sync_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(g_baji_photo_sync_label, LV_OPA_60, 0);
        lv_obj_set_style_radius(g_baji_photo_sync_label, 6, 0);
        lv_obj_set_style_pad_left(g_baji_photo_sync_label, 8, 0);
        lv_obj_set_style_pad_right(g_baji_photo_sync_label, 8, 0);
        lv_obj_set_style_pad_top(g_baji_photo_sync_label, 5, 0);
        lv_obj_set_style_pad_bottom(g_baji_photo_sync_label, 5, 0);
    }

    lv_label_set_text(g_baji_photo_sync_label, text);
    lv_obj_clear_flag(g_baji_photo_sync_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(g_baji_photo_sync_label, LV_ALIGN_BOTTOM_MID, 0, -22);
    g_baji_photo_sync_label_expire_tick = (visible_ms == 0u) ? 0u : (lv_tick_get() + visible_ms);
}

static uint32_t baji_photo_bytes_to_kbytes(uint32_t bytes)
{
    if (bytes == 0u) {
        return 0u;
    }

    return ((bytes - 1u) / 1024u) + 1u;
}

static void baji_photo_sync_ui_timer_ensure(void)
{
    if (g_baji_photo_sync_timer == NULL) {
        g_baji_photo_sync_timer = lv_timer_create(baji_photo_sync_ui_timer_cb,
                                                  BAJI_PHOTO_SYNC_UI_POLL_MS,
                                                  NULL);
    }
}

static void baji_photo_sync_done_cb(const baji_photo_sync_result_t *result, void *ctx)
{
    (void)ctx;

    if (result == NULL) {
        return;
    }

    baji_photo_page_mailbox_post_sync_result(result);
}

static void baji_photo_sync_progress_cb(baji_photo_sync_state_t state,
                                        uint16_t current,
                                        uint16_t total,
                                        void *ctx)
{
    (void)ctx;

    baji_photo_page_mailbox_post_sync_progress(state, current, total);
}

static void baji_photo_mqtt_page_event_cb(baji_photo_mqtt_event_t event, const void *data, void *ctx)
{
    const baji_photo_image_task_t *task = NULL;
    const baji_photo_mqtt_display_request_t *request = NULL;
    const baji_photo_mqtt_image_result_t *result = NULL;
    const baji_photo_mqtt_image_progress_t *image_progress = NULL;
    const baji_photo_bind_status_t *bind_status = NULL;
    const baji_photo_bind_token_info_t *bind_token = NULL;
    const baji_photo_bind_notice_t *bind_notice = NULL;

    (void)ctx;

    if ((event == BAJI_PHOTO_MQTT_EVT_IMAGE_TASK) && (data != NULL)) {
        task = (const baji_photo_image_task_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt task event enter task_op=%lu task=%s image=%s dims=%ux%u size=%lu",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned int)task->image_width,
                              (unsigned int)task->image_height,
                              (unsigned long)task->image_size);
    } else if ((event == BAJI_PHOTO_MQTT_EVT_DISPLAY_REQUEST) && (data != NULL)) {
        request = (const baji_photo_mqtt_display_request_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display req event enter task_op=%lu task=%s image=%s downloaded=%lu range=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)request->trace_id,
                              request->task_id,
                              request->image_id,
                              (unsigned long)request->downloaded_size,
                              request->range_resumed ? 1 : 0);
    } else if ((event == BAJI_PHOTO_MQTT_EVT_IMAGE_RESULT) && (data != NULL)) {
        result = (const baji_photo_mqtt_image_result_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt result event enter task_op=%lu task=%s image=%s code=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)result->trace_id,
                              result->task_id,
                              result->image_id,
                              result->code);
    } else if ((event == BAJI_PHOTO_MQTT_EVT_IMAGE_PROGRESS) && (data != NULL)) {
        image_progress = (const baji_photo_mqtt_image_progress_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt progress event enter task=%s image=%s bucket=%lu downloaded=%lu total=%lu",
                              (unsigned long)g_baji_photo_session_id,
                              image_progress->task_id,
                              image_progress->image_id,
                              (unsigned long)image_progress->percent_bucket,
                              (unsigned long)image_progress->downloaded_size,
                              (unsigned long)image_progress->total_size);
    } else if ((event == BAJI_PHOTO_MQTT_EVT_BIND_STATUS) && (data != NULL)) {
        bind_status = (const baji_photo_bind_status_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind status event enter status=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (int)*bind_status);
    } else if ((event == BAJI_PHOTO_MQTT_EVT_BIND_TOKEN) && (data != NULL)) {
        bind_token = (const baji_photo_bind_token_info_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind token event enter expire=%lu url_len=%u",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)bind_token->expire_seconds,
                              (unsigned int)strlen(bind_token->bind_url));
    } else if ((event == BAJI_PHOTO_MQTT_EVT_BIND_NOTICE) && (data != NULL)) {
        bind_notice = (const baji_photo_bind_notice_t *)data;
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind notice event enter level=%d msg=%s",
                              (unsigned long)g_baji_photo_session_id,
                              (int)bind_notice->level,
                              bind_notice->message);
    }

    baji_photo_page_mailbox_post_mqtt_event(event, data);

    if (task != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt task event copied task_op=%lu task=%s image=%s dims=%ux%u size=%lu",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned int)task->image_width,
                              (unsigned int)task->image_height,
                              (unsigned long)task->image_size);
    } else if (request != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display req event copied task_op=%lu task=%s image=%s downloaded=%lu range=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)request->trace_id,
                              request->task_id,
                              request->image_id,
                              (unsigned long)request->downloaded_size,
                              request->range_resumed ? 1 : 0);
    } else if (result != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt result event copied task_op=%lu task=%s image=%s code=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)result->trace_id,
                              result->task_id,
                              result->image_id,
                              result->code);
    } else if (image_progress != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt progress event copied task=%s image=%s bucket=%lu",
                              (unsigned long)g_baji_photo_session_id,
                              image_progress->task_id,
                              image_progress->image_id,
                              (unsigned long)image_progress->percent_bucket);
    } else if (bind_status != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind status event copied status=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (int)*bind_status);
    } else if (bind_token != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind token event copied expire=%lu url_len=%u",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)bind_token->expire_seconds,
                              (unsigned int)strlen(bind_token->bind_url));
    } else if (bind_notice != NULL) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt bind notice event copied level=%d msg=%s",
                              (unsigned long)g_baji_photo_session_id,
                              (int)bind_notice->level,
                              bind_notice->message);
    }
}

static void baji_photo_sync_ui_timer_cb(lv_timer_t *timer)
{
    baji_photo_page_mailbox_snapshot_t mailbox = {0};
    bool has_result;
    bool has_progress;
    bool has_mqtt_task;
    bool has_mqtt_image_progress;
    bool has_mqtt_display_request;
    bool has_mqtt_result;
    bool has_mqtt_bind_status;
    bool has_mqtt_bind_token;
    bool has_mqtt_bind_notice;
    bool has_delete_result;
    bool busy;
    const char *bind_hint;

    (void)timer;

    baji_photo_page_mailbox_take_snapshot(&mailbox);
    has_result = baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_SYNC_RESULT);
    has_progress = baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_SYNC_PROGRESS);
    has_mqtt_task = baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_TASK);
    has_mqtt_image_progress =
        baji_photo_page_mailbox_snapshot_has(&mailbox,
                                             BAJI_PHOTO_PAGE_MAILBOX_MQTT_IMAGE_PROGRESS);
    has_mqtt_display_request =
        baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_DISPLAY_REQUEST);
    has_mqtt_result = baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_RESULT);
    has_mqtt_bind_status =
        baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_STATUS);
    has_mqtt_bind_token =
        baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_TOKEN);
    has_mqtt_bind_notice =
        baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_MQTT_BIND_NOTICE);
    has_delete_result =
        baji_photo_page_mailbox_snapshot_has(&mailbox, BAJI_PHOTO_PAGE_MAILBOX_DELETE_RESULT);

    baji_photo_bind_ui_set_fatal_error(baji_photo_mqtt_get_fatal_error());

    if (has_mqtt_bind_status) {
        baji_photo_bind_ui_set_bind_status(mailbox.mqtt_bind_status);
        baji_photo_empty_state_refresh();
    }
    if (has_mqtt_bind_token) {
        baji_photo_bind_ui_set_bind_token(&mailbox.mqtt_bind_token);
    }
    if (has_mqtt_bind_notice) {
        baji_photo_bind_ui_set_notice(&mailbox.mqtt_bind_notice);
    }

    if (has_progress && !has_result) {
        char text[32];
        const char *verb = (g_baji_photo_job == BAJI_PHOTO_JOB_SEED) ? "seeding" : "syncing";

        if ((mailbox.sync_progress_state == BAJI_PHOTO_SYNC_STATE_DOWNLOAD_ONE) &&
            (mailbox.sync_progress_total != 0u)) {
            (void)snprintf(text,
                           sizeof(text),
                           "%s %u/%u",
                           verb,
                           (unsigned int)mailbox.sync_progress_current,
                           (unsigned int)mailbox.sync_progress_total);
        } else {
            (void)snprintf(text, sizeof(text), "%s", verb);
        }
        baji_photo_sync_label_show(text, 0u);
    }

    if (has_mqtt_task && !has_mqtt_result) {
        char text[64];

        g_baji_photo_download_progress_active = true;
        (void)snprintf(g_baji_photo_download_task_id,
                       sizeof(g_baji_photo_download_task_id),
                       "%s",
                       mailbox.mqtt_task.task_id);
        (void)snprintf(g_baji_photo_download_image_id,
                       sizeof(g_baji_photo_download_image_id),
                       "%s",
                       mailbox.mqtt_task.image_id);
        g_baji_photo_downloaded_size = 0u;
        g_baji_photo_download_total_size = mailbox.mqtt_task.image_size;
        g_baji_photo_download_progress_bucket = 0u;
        g_baji_photo_download_progress_last_ui_tick = lv_tick_get();
        g_baji_photo_download_progress_pending = false;
        (void)snprintf(text, sizeof(text), "Downloading...");
        baji_photo_sync_label_show(text, 0u);
    }

    if (has_mqtt_image_progress &&
        g_baji_photo_download_progress_active &&
        (strcmp(mailbox.mqtt_image_progress.task_id, g_baji_photo_download_task_id) == 0) &&
        (strcmp(mailbox.mqtt_image_progress.image_id, g_baji_photo_download_image_id) == 0)) {
        if ((g_baji_photo_downloaded_size == 0u) &&
            (mailbox.mqtt_image_progress.downloaded_size > 0u)) {
            g_baji_photo_download_progress_last_ui_tick = 0u;
        }
        g_baji_photo_downloaded_size = mailbox.mqtt_image_progress.downloaded_size;
        g_baji_photo_download_total_size = mailbox.mqtt_image_progress.total_size;
        g_baji_photo_download_progress_bucket = mailbox.mqtt_image_progress.percent_bucket;
        g_baji_photo_download_progress_pending =
            (mailbox.mqtt_image_progress.downloaded_size > 0u);
    }

    if (g_baji_photo_download_progress_active &&
        g_baji_photo_download_progress_pending &&
        ((g_baji_photo_download_progress_bucket == 100u) ||
         (g_baji_photo_download_progress_last_ui_tick == 0u) ||
         (lv_tick_elaps(g_baji_photo_download_progress_last_ui_tick) >=
          BAJI_PHOTO_DOWNLOAD_UI_MIN_MS))) {
        char text[64];
        uint32_t downloaded_size = g_baji_photo_downloaded_size;
        uint32_t total_size = g_baji_photo_download_total_size;

        if ((total_size != 0u) && (downloaded_size > total_size)) {
            downloaded_size = total_size;
        }

        (void)snprintf(text,
                       sizeof(text),
                       ((total_size != 0u) && (downloaded_size >= total_size))
                           ? "Downloaded %lu/%lu KB"
                           : "Downloading %lu/%lu KB",
                       (unsigned long)baji_photo_bytes_to_kbytes(downloaded_size),
                       (unsigned long)baji_photo_bytes_to_kbytes(total_size));
        baji_photo_sync_label_show(text, 0u);
        g_baji_photo_download_progress_last_ui_tick = lv_tick_get();
        g_baji_photo_download_progress_pending = false;
    }

    if (has_mqtt_display_request) {
        const baji_photo_mqtt_display_request_t mqtt_display_request = mailbox.mqtt_display_request;
        int target_idx;
        bool switched = false;
        bool requeue = false;

        baji_photo_reload_list();
        target_idx = baji_photo_find_item_index_by_id(mqtt_display_request.image_id);
        BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display req apply task_op=%lu task=%s image=%s downloaded=%lu list_total=%u target_idx=%d state=%d trans=%d",
                              (unsigned long)g_baji_photo_session_id,
                              (unsigned long)mqtt_display_request.trace_id,
                              mqtt_display_request.task_id,
                              mqtt_display_request.image_id,
                              (unsigned long)mqtt_display_request.downloaded_size,
                              g_baji_photo_count,
                              target_idx,
                              (int)g_baji_photo_state,
                              (int)g_baji_photo_transition);
        baji_photo_page_marker_display_request("UI_DISPLAY_APPLY",
                                               &mqtt_display_request,
                                               target_idx,
                                               false);
        if (target_idx < 0) {
            baji_photo_page_marker_display_request("UI_DISPLAY_FAIL",
                                                   &mqtt_display_request,
                                                   target_idx,
                                                   false);
            (void)baji_photo_page_notify_mqtt_display_result(g_baji_photo_session_id,
                                                             mqtt_display_request.trace_id,
                                                             mqtt_display_request.task_id,
                                                             mqtt_display_request.image_id,
                                                             1005,
                                                             0u,
                                                             "target_not_found");
            baji_photo_sync_label_show("mqtt failed 1005", BAJI_PHOTO_SYNC_RESULT_MS);
        } else if ((g_baji_photo_state != BAJI_PHOTO_STATE_ACTIVE) ||
                   (g_baji_photo_controls.msgbox != NULL) ||
                   baji_photo_delete_is_pending()) {
            const char *reason = "state_not_active";

            if (g_baji_photo_controls.msgbox != NULL) {
                reason = "dialog_open";
            } else if (baji_photo_delete_is_pending()) {
                reason = "delete_pending";
            }
            BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display req defer task_op=%lu task=%s image=%s reason=%s state=%d trans=%d",
                                  (unsigned long)g_baji_photo_session_id,
                                  (unsigned long)mqtt_display_request.trace_id,
                                  mqtt_display_request.task_id,
                                  mqtt_display_request.image_id,
                                  reason,
                                  (int)g_baji_photo_state,
                                  (int)g_baji_photo_transition);
            baji_photo_page_marker_display_request("UI_DISPLAY_DEFER",
                                                   &mqtt_display_request,
                                                   target_idx,
                                                   false);
            requeue = true;
        } else {
            baji_photo_page_track_mqtt_display(&mqtt_display_request);
            switched = baji_photo_switch_to_index(target_idx, LV_SCR_LOAD_ANIM_FADE_ON,
                                                  "mqtt_display");
            BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt display req switch task_op=%lu task=%s image=%s target_idx=%d switched=%d",
                                  (unsigned long)g_baji_photo_session_id,
                                  (unsigned long)mqtt_display_request.trace_id,
                                  mqtt_display_request.task_id,
                                  mqtt_display_request.image_id,
                                  target_idx,
                                  switched ? 1 : 0);
            baji_photo_page_marker_display_request("UI_DISPLAY_SWITCH",
                                                   &mqtt_display_request,
                                                   target_idx,
                                                   switched);
            if (!switched) {
                baji_photo_page_clear_mqtt_display();
                baji_photo_page_marker_display_request("UI_DISPLAY_FAIL",
                                                       &mqtt_display_request,
                                                       target_idx,
                                                       false);
                (void)baji_photo_page_notify_mqtt_display_result(g_baji_photo_session_id,
                                                                 mqtt_display_request.trace_id,
                                                                 mqtt_display_request.task_id,
                                                                 mqtt_display_request.image_id,
                                                                 1005,
                                                                 0u,
                                                                 "switch_setup_failed");
                baji_photo_sync_label_show("mqtt failed 1005", BAJI_PHOTO_SYNC_RESULT_MS);
            }
        }
        if (requeue) {
            baji_photo_page_mailbox_post_mqtt_display_request(&mqtt_display_request);
        }
    }

    if (has_delete_result) {
        const baji_photo_delete_result_t *delete_result = &mailbox.delete_result;
        bool result_matches = baji_photo_delete_result_matches_current(delete_result);

        BAJI_PHOTO_PAGE_TRACE("op=%lu delete result request=%lu ret=%d cleanup_pending=%d match=%d state=%d trans=%d",
                              (unsigned long)delete_result->session_id,
                              (unsigned long)delete_result->request_id,
                              delete_result->ret,
                              delete_result->cleanup_pending ? 1 : 0,
                              result_matches ? 1 : 0,
                              g_baji_photo_state,
                              g_baji_photo_transition);
        if (!result_matches) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu delete result ignored request=%lu reason=stale",
                                  (unsigned long)g_baji_photo_session_id,
                                  (unsigned long)delete_result->request_id);
        } else if (delete_result->session_id != g_baji_photo_session_id) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu delete result ignored request=%lu reason=session_changed old_session=%lu",
                                  (unsigned long)g_baji_photo_session_id,
                                  (unsigned long)delete_result->request_id,
                                  (unsigned long)delete_result->session_id);
            memset(&g_baji_photo_delete_request, 0, sizeof(g_baji_photo_delete_request));
        } else if (g_baji_photo_state == BAJI_PHOTO_STATE_TRANSITIONING) {
            baji_photo_page_mailbox_post_delete_result(delete_result);
        } else {
            bool session_match = (delete_result->session_id == g_baji_photo_session_id);

            if (delete_result->ret == 0) {
                baji_photo_reload_list();
                baji_photo_restore_active_index_after_reload();
                baji_photo_controls_refresh_buttons();
                baji_photo_controls_refresh_play_timer();
                if (session_match && (g_baji_photo_state != BAJI_PHOTO_STATE_IDLE)) {
                    baji_photo_sync_label_show(delete_result->cleanup_pending ?
                                                   "removed, cleanup retry" : "deleted",
                                               BAJI_PHOTO_SYNC_RESULT_MS);
                }
            } else if (session_match && (g_baji_photo_state != BAJI_PHOTO_STATE_IDLE)) {
                baji_photo_controls_refresh_buttons();
                baji_photo_controls_refresh_play_timer();
                baji_photo_sync_label_show("delete failed", BAJI_PHOTO_SYNC_RESULT_MS);
            }
            memset(&g_baji_photo_delete_request, 0, sizeof(g_baji_photo_delete_request));
            baji_photo_controls_refresh_buttons();
            baji_photo_controls_refresh_play_timer();
        }
    }

    if (has_result) {
        const baji_photo_sync_result_t result = mailbox.sync_result;
        char text[64];
        bool had_no_photos = (g_baji_photo_count == 0u);
        bool switched_to_first = false;

        baji_photo_reload_list();
        if (had_no_photos &&
            (g_baji_photo_count > 0u) &&
            (g_baji_photo_state == BAJI_PHOTO_STATE_ACTIVE) &&
            (g_baji_photo_transition == BAJI_PHOTO_TRANSITION_NONE)) {
            switched_to_first = baji_photo_switch_to_index(0,
                                                           LV_SCR_LOAD_ANIM_FADE_ON,
                                                           "sync_populated");
        }
        if ((result.downloaded == 0u) && (result.skipped == 0u) && (result.failed != 0u)) {
            (void)snprintf(text,
                           sizeof(text),
                           "%s failed",
                           (g_baji_photo_job == BAJI_PHOTO_JOB_SEED) ? "seed" : "sync");
        } else {
            (void)snprintf(text,
                           sizeof(text),
                           "%s %u skipped %u failed %u",
                           (g_baji_photo_job == BAJI_PHOTO_JOB_SEED) ? "seeded" : "updated",
                           (unsigned int)result.downloaded,
                           (unsigned int)result.skipped,
                           (unsigned int)result.failed);
        }
        if (!switched_to_first) {
            baji_photo_sync_label_show(text, BAJI_PHOTO_SYNC_RESULT_MS);
        }
    }

    if (has_mqtt_result) {
        const baji_photo_mqtt_image_result_t *mqtt_result = &mailbox.mqtt_result;
        bool download_matches_current =
            (strcmp(mqtt_result->task_id, g_baji_photo_download_task_id) == 0) &&
            (strcmp(mqtt_result->image_id, g_baji_photo_download_image_id) == 0);

        if (mqtt_result->code == 0) {
            char text[64];
            uint32_t downloaded_k = baji_photo_bytes_to_kbytes(mqtt_result->downloaded_size);
            uint32_t total_k = baji_photo_bytes_to_kbytes(g_baji_photo_download_total_size);

            BAJI_PHOTO_PAGE_TRACE("op=%lu mqtt result ignore success task_op=%lu task=%s image=%s code=0",
                                  (unsigned long)g_baji_photo_session_id,
                                  (unsigned long)mqtt_result->trace_id,
                                  mqtt_result->task_id,
                                  mqtt_result->image_id);
            if (download_matches_current) {
                if ((total_k != 0u) && (downloaded_k > total_k)) {
                    downloaded_k = total_k;
                }
                g_baji_photo_downloaded_size = mqtt_result->downloaded_size;
                if ((g_baji_photo_download_total_size != 0u) &&
                    (g_baji_photo_downloaded_size > g_baji_photo_download_total_size)) {
                    g_baji_photo_downloaded_size = g_baji_photo_download_total_size;
                }
                g_baji_photo_download_progress_bucket = 100u;
                g_baji_photo_download_progress_pending = false;
                if (total_k == 0u) {
                    (void)snprintf(text, sizeof(text), "Download complete");
                } else {
                    (void)snprintf(text,
                                   sizeof(text),
                                   "Downloaded %lu/%lu KB",
                                   (unsigned long)downloaded_k,
                                   (unsigned long)total_k);
                }
                baji_photo_sync_label_show(text, 0u);
            }
        } else {
            char text[48];

            (void)snprintf(text, sizeof(text), "mqtt failed %d", mqtt_result->code);
            baji_photo_sync_label_show(text, BAJI_PHOTO_SYNC_RESULT_MS);
        }
        if (download_matches_current) {
            g_baji_photo_download_progress_active = false;
            g_baji_photo_download_progress_pending = false;
        }
    }

    baji_photo_bind_ui_tick(g_baji_photo_state != BAJI_PHOTO_STATE_IDLE);

    busy = baji_photo_net_is_busy() ||
           baji_photo_delete_is_pending() ||
           baji_photo_page_mailbox_has_pending(BAJI_PHOTO_PAGE_MAILBOX_DELETE_RESULT);
    if ((g_baji_photo_sync_label != NULL) && !lv_obj_is_valid(g_baji_photo_sync_label)) {
        g_baji_photo_sync_label = NULL;
    }
    if ((g_baji_photo_sync_label != NULL) &&
        (g_baji_photo_sync_label_expire_tick != 0u) &&
        (lv_tick_elaps(g_baji_photo_sync_label_expire_tick) < 0x80000000u)) {
        lv_obj_add_flag(g_baji_photo_sync_label, LV_OBJ_FLAG_HIDDEN);
        g_baji_photo_sync_label_expire_tick = 0u;
    }
    bind_hint = baji_photo_bind_ui_get_hint();
    if ((bind_hint != NULL) &&
        (g_baji_photo_state != BAJI_PHOTO_STATE_IDLE) &&
        !baji_photo_bind_ui_is_visible() &&
        ((g_baji_photo_sync_label == NULL) || lv_obj_has_flag(g_baji_photo_sync_label, LV_OBJ_FLAG_HIDDEN))) {
        baji_photo_sync_label_show(bind_hint, 0u);
    }
    if ((g_baji_photo_state == BAJI_PHOTO_STATE_IDLE) &&
        !busy && !has_result && !has_progress &&
        !has_mqtt_task && !has_mqtt_display_request && !has_mqtt_result &&
        !has_mqtt_bind_status && !has_mqtt_bind_token && !has_mqtt_bind_notice &&
        !has_delete_result &&
        ((g_baji_photo_sync_label == NULL) || lv_obj_has_flag(g_baji_photo_sync_label, LV_OBJ_FLAG_HIDDEN))) {
        g_baji_photo_sync_timer = NULL;
        lv_timer_del(timer);
    }
}

static lv_obj_t *baji_photo_setup_scr(int idx)
{
    baji_photo_dynamic_src_t *dyn = NULL;
    baji_photo_screen_ctx_t *screen_ctx = NULL;
    const baji_photo_list_item_t *item = NULL;
    const lv_img_dsc_t *src = NULL;
    bool gif_item = false;
    lv_obj_t *interaction = NULL;
    lv_obj_t *scr;

    scr = lv_obj_create(NULL);
    if (scr == NULL) {
        return NULL;
    }

    lv_obj_set_size(scr, BAJI_PHOTO_SCREEN_W, BAJI_PHOTO_SCREEN_H);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    if (g_baji_photo_count == 0u) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu setup empty screen", (unsigned long)g_baji_photo_session_id);
        lv_obj_add_event_cb(scr, baji_photo_event_handler, LV_EVENT_ALL, NULL);

        interaction = lv_obj_create(scr);
        if (interaction == NULL) {
            lv_obj_del(scr);
            return NULL;
        }
        lv_obj_remove_style_all(interaction);
        lv_obj_set_size(interaction, BAJI_PHOTO_SCREEN_W, BAJI_PHOTO_SCREEN_H);
        lv_obj_center(interaction);
        lv_obj_clear_flag(interaction, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(interaction,
                        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                            LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(interaction, baji_photo_interaction_event_cb, LV_EVENT_ALL, NULL);

        if ((baji_photo_empty_state_attach(scr) != 0) ||
            (baji_photo_controls_attach(scr) != 0)) {
            lv_obj_del(scr);
            return NULL;
        }

        lv_obj_update_layout(scr);
        return scr;
    }

    if ((idx < 0) || ((unsigned int)idx >= g_baji_photo_count)) {
        idx = 0;
    }
    item = &g_baji_photo_list[idx];
    screen_ctx = (baji_photo_screen_ctx_t *)liot_rtos_malloc(sizeof(*screen_ctx));
    if (screen_ctx != NULL) {
        baji_photo_screen_ctx_init(screen_ctx, g_baji_photo_session_id, idx, item);
    } else {
        BAJI_PHOTO_PAGE_TRACE("op=%lu screen ctx alloc fail idx=%d id=%s",
                              (unsigned long)g_baji_photo_session_id,
                              idx,
                              baji_photo_item_id_get(idx));
    }

    if (item->is_downloaded &&
        (item->item.format == BAJI_PHOTO_FORMAT_GIF)) {
        gif_item = true;
    }
    BAJI_PHOTO_PAGE_TRACE("op=%lu setup idx=%d id=%s kind=%s downloaded=%d",
                          (unsigned long)g_baji_photo_session_id,
                          idx,
                          baji_photo_item_id_get(idx),
                          baji_photo_item_kind_get(idx),
                          item->is_downloaded ? 1 : 0);

    if (item->is_downloaded && !gif_item) {
        dyn = (baji_photo_dynamic_src_t *)lv_mem_alloc(sizeof(*dyn));
        if (dyn != NULL) {
            int load_ret = LIOT_EXTFLASH_INVALID_PARAMETER;

            memset(dyn, 0, sizeof(*dyn));
            baji_photo_dyn_ctx_init(dyn, g_baji_photo_session_id, idx, item);
            baji_photo_page_marker_dyn("UI_IMG_CTX_ALLOC", dyn, "setup_scr");
            BAJI_PHOTO_MARK_TRACE("UI_IMG_LOAD_BEGIN op=%lu dyn=%lu idx=%d item=%s kind=%s heap_min=%lu",
                                  (unsigned long)dyn->session_id,
                                  (unsigned long)dyn->dyn_seq,
                                  dyn->idx,
                                  baji_photo_diag_id_tail(dyn->item_id),
                                  baji_photo_ctx_kind(dyn->is_downloaded, dyn->format),
                                  (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());

            if (item->item.format == BAJI_PHOTO_FORMAT_BJP) {
                load_ret = baji_photo_flash_img_load(&item->item,
                                                     g_baji_photo_session_id,
                                                     dyn->dyn_seq,
                                                     &dyn->dsc,
                                                     &dyn->buf);
            } else if ((item->item.format == BAJI_PHOTO_FORMAT_JPEG) ||
                       (item->item.format == BAJI_PHOTO_FORMAT_PNG)) {
                load_ret = baji_photo_vpu_img_load(&item->item,
                                                   g_baji_photo_session_id,
                                                   dyn->dyn_seq,
                                                   &dyn->dsc,
                                                   &dyn->buf);
            }

            if (load_ret != 0) {
                lv_mem_free(dyn);
                dyn = NULL;
            }
        } else {
            BAJI_PHOTO_PAGE_TRACE("op=%lu dyn ctx alloc fail idx=%d id=%s",
                                  (unsigned long)g_baji_photo_session_id,
                                  idx,
                                  baji_photo_item_id_get(idx));
        }
    }

    src = (dyn != NULL) ? &dyn->dsc : NULL;
    if (!item->is_downloaded) {
        src = item->builtin;
    }
#if BAJI_PHOTO_ENABLE_BUILTIN_DISPLAY
    if ((src == NULL) && !gif_item) {
        src = g_baji_photo_list[0].builtin;
    }
#endif
    if ((src == NULL) && !gif_item) {
        if (screen_ctx != NULL) {
            liot_rtos_free(screen_ctx);
        }
        lv_obj_del(scr);
        return NULL;
    }

    lv_obj_add_event_cb(scr, baji_photo_event_handler, LV_EVENT_ALL, screen_ctx);

    if (gif_item) {
        BAJI_PHOTO_PAGE_TRACE("op=%lu setup gif idx=%d id=%s path=%s",
                              (unsigned long)g_baji_photo_session_id,
                              idx,
                              item->item.id,
                              item->item.local_path);
        if (baji_gif_player_create(scr,
                                   item->item.local_path,
                                   item->item.name) == NULL) {
            lv_obj_del(scr);
            return NULL;
        }
    } else {
        if ((dyn == NULL) &&
            (src->header.w == BAJI_PHOTO_SCREEN_W) &&
            (src->header.h == BAJI_PHOTO_SCREEN_H)) {
            BAJI_PHOTO_PAGE_TRACE("op=%lu setup bg idx=%d id=%s size=%ux%u",
                                  (unsigned long)g_baji_photo_session_id,
                                  idx,
                                  baji_photo_item_id_get(idx),
                                  (unsigned int)src->header.w,
                                  (unsigned int)src->header.h);
            lv_obj_set_style_bg_img_src(scr, src, LV_PART_MAIN);
            lv_obj_set_style_bg_img_opa(scr, 255, LV_PART_MAIN);
        } else {
            lv_obj_t *img = lv_img_create(scr);

            if (img == NULL) {
                baji_photo_dynamic_src_release(dyn, "img_create_fail");
                lv_obj_del(scr);
                return NULL;
            }

            lv_img_set_src(img, src);
            lv_obj_center(img);
            BAJI_PHOTO_PAGE_TRACE("op=%lu setup img idx=%d id=%s dyn_seq=%lu size=%ux%u",
                                  (unsigned long)g_baji_photo_session_id,
                                  idx,
                                  baji_photo_item_id_get(idx),
                                  (unsigned long)((dyn != NULL) ? dyn->dyn_seq : 0u),
                                  (unsigned int)src->header.w,
                                  (unsigned int)src->header.h);
            if (dyn != NULL) {
                dyn->img = img;
                if (screen_ctx != NULL) {
                    screen_ctx->dyn = dyn;
                    screen_ctx->dyn_seq = dyn->dyn_seq;
                }
                BAJI_PHOTO_PAGE_TRACE("op=%lu setup img bind idx=%d id=%s dyn=%p dyn_seq=%lu img=%p",
                                      (unsigned long)g_baji_photo_session_id,
                                      idx,
                                      baji_photo_item_id_get(idx),
                                      dyn,
                                      (unsigned long)dyn->dyn_seq,
                                      img);
                lv_obj_add_event_cb(img,
                                    baji_photo_dynamic_img_event_handler,
                                    LV_EVENT_DELETE,
                                    (void *)dyn);
            }
        }
    }

    interaction = lv_obj_create(scr);
    if (interaction == NULL) {
        lv_obj_del(scr);
        return NULL;
    }
    BAJI_PHOTO_PAGE_TRACE("op=%lu setup interaction idx=%d id=%s obj=%p",
                          (unsigned long)g_baji_photo_session_id,
                          idx,
                          baji_photo_item_id_get(idx),
                          interaction);
    lv_obj_remove_style_all(interaction);
    lv_obj_set_size(interaction, BAJI_PHOTO_SCREEN_W, BAJI_PHOTO_SCREEN_H);
    lv_obj_center(interaction);
    lv_obj_clear_flag(interaction, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(interaction,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                        LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(interaction, baji_photo_interaction_event_cb, LV_EVENT_ALL, NULL);

    if (baji_photo_controls_attach(scr) != 0) {
        lv_obj_del(scr);
        return NULL;
    }
    BAJI_PHOTO_PAGE_TRACE("op=%lu setup controls attached idx=%d id=%s screen=%p",
                          (unsigned long)g_baji_photo_session_id,
                          idx,
                          baji_photo_item_id_get(idx),
                          scr);

    lv_obj_update_layout(scr);
    BAJI_PHOTO_PAGE_TRACE("op=%lu setup ready idx=%d id=%s screen=%p",
                          (unsigned long)g_baji_photo_session_id,
                          idx,
                          baji_photo_item_id_get(idx),
                          scr);
    return scr;
}

static void baji_photo_page_exit_to(baji_photo_exit_target_t target)
{
    BAJI_PHOTO_PAGE_TRACE("op=%lu exit req idx=%d id=%s kind=%s target=%d",
                          (unsigned long)g_baji_photo_session_id,
                          g_baji_photo_idx,
                          baji_photo_item_id_get(g_baji_photo_idx),
                          baji_photo_item_kind_get(g_baji_photo_idx),
                          (int)target);
    baji_photo_page_log_runtime("exit", g_baji_photo_session_id);
    baji_photo_delete_dialog_close(false);
    baji_photo_player_set_playing(false, "exit");
    baji_photo_controls_cancel_hide_timer();
    baji_photo_controls_refresh_play_timer();
    g_baji_photo_last_exit_tick = lv_tick_get();
    g_baji_photo_state = BAJI_PHOTO_STATE_TRANSITIONING;
    g_baji_photo_transition = BAJI_PHOTO_TRANSITION_EXIT;
    baji_photo_bind_ui_tick(false);

    if (target == BAJI_PHOTO_EXIT_TARGET_TIME
#if APP_WATCHFACE_EN
        && true
#else
        && false
#endif
    ) {
        ui_load_scr_animation(&guider_ui,
                              &guider_ui.time,
                              guider_ui.time_del,
                              &g_baji_photo_del,
                              setup_scr_time,
                              LV_SCR_LOAD_ANIM_FADE_ON,
                              BAJI_PHOTO_ANIM_TIME_MS, 0,
                              false, true);
    } else {
        ui_load_scr_animation(&guider_ui,
                              &guider_ui.baji,
                              guider_ui.baji_del,
                              &g_baji_photo_del,
                              setup_scr_baji,
                              LV_SCR_LOAD_ANIM_FADE_ON,
                              BAJI_PHOTO_ANIM_TIME_MS, 0,
                              false, true);
    }
}

void baji_photo_page_enter(void)
{
    BAJI_PHOTO_PAGE_TRACE("op=%lu enter req state=%d trans=%d last_exit=%lu",
                          (unsigned long)g_baji_photo_session_id,
                          (int)g_baji_photo_state,
                          (int)g_baji_photo_transition,
                          (unsigned long)g_baji_photo_last_exit_tick);
    if (g_baji_photo_state != BAJI_PHOTO_STATE_IDLE) {
        return;
    }
    if (g_baji_photo_last_exit_tick != 0 &&
        lv_tick_elaps(g_baji_photo_last_exit_tick) < BAJI_PHOTO_REENTER_BLOCK_MS) {
        return;
    }

    g_baji_photo_session_id = baji_photo_diag_next_id();
    BAJI_PHOTO_PAGE_TRACE("op=%lu enter start", (unsigned long)g_baji_photo_session_id);
    baji_photo_page_log_boot_snapshot("baji_page_enter");
    baji_photo_play_diag_log_persisted_snapshot("enter");
    baji_photo_page_log_runtime("enter", g_baji_photo_session_id);
    baji_photo_player_reset();
    baji_photo_play_diag_reset();
    baji_photo_active_item_reset();
    baji_photo_bind_ui_set_ops(&g_baji_photo_bind_ui_ops);
    baji_photo_bind_ui_set_bind_status(baji_photo_mqtt_get_bind_status());
    baji_photo_bind_ui_set_fatal_error(baji_photo_mqtt_get_fatal_error());

#if BAJI_PHOTO_ENABLE_MQTT_CONTROL
    if (!g_baji_photo_mqtt_cb_registered) {
        if (baji_photo_mqtt_set_event_cb(baji_photo_mqtt_page_event_cb, NULL) == 0) {
            g_baji_photo_mqtt_cb_registered = true;
        }
    }
    if (!g_baji_photo_mqtt_started) {
        if (baji_photo_mqtt_start() == 0) {
            g_baji_photo_mqtt_started = true;
        }
    }
#endif

    baji_photo_sync_ui_timer_ensure();

    g_baji_photo_idx = 0;
    baji_photo_reload_list();
    BAJI_PHOTO_PAGE_TRACE("op=%lu enter list count=%u first_id=%s first_kind=%s",
                          (unsigned long)g_baji_photo_session_id,
                          g_baji_photo_count,
                          baji_photo_item_id_get(0),
                          baji_photo_item_kind_get(0));
    guider_ui.baji_del = true;
    g_baji_photo_del = false;
    baji_photo_play_diag_mark_switch((g_baji_photo_count == 0u) ? "enter_empty" : "enter",
                                     -1,
                                     (g_baji_photo_count == 0u) ? -1 : 0,
                                     LV_SCR_LOAD_ANIM_FADE_ON);
    lv_obj_t *scr = baji_photo_setup_scr(0);
    if (scr == NULL) {
        g_baji_photo_state = BAJI_PHOTO_STATE_IDLE;
        g_baji_photo_transition = BAJI_PHOTO_TRANSITION_NONE;
        baji_photo_play_diag_log("enter_setup_fail", 0, 0u, NULL);
        baji_photo_sync_label_show((g_baji_photo_count == 0u) ? "page load failed" :
                                                               "photo load failed",
                                   BAJI_PHOTO_SYNC_RESULT_MS);
        baji_photo_sync_ui_timer_ensure();
        return;
    }
    if (g_baji_photo_count > 0u) {
        baji_photo_player_sync_current_index(0, "enter_prepare");
    }
    g_baji_photo_state = BAJI_PHOTO_STATE_TRANSITIONING;
    g_baji_photo_transition = BAJI_PHOTO_TRANSITION_ENTER;

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON,
                     BAJI_PHOTO_ANIM_TIME_MS, 0, true);

}

baji_photo_sync_start_result_t baji_photo_request_sync(void)
{
    baji_photo_sync_start_result_t ret;

#if BAJI_PHOTO_ENABLE_MQTT_CONTROL
    if (baji_photo_mqtt_has_active_task()) {
        ret = BAJI_PHOTO_SYNC_START_BUSY;
    } else
#endif
#if BAJI_PHOTO_HTTP_RAW_RGB565_VERIFY
    {
        g_baji_photo_job = BAJI_PHOTO_JOB_SYNC;
        ret = baji_photo_net_request_sync(baji_photo_sync_done_cb, baji_photo_sync_progress_cb, NULL);
    }
#else
    {
        g_baji_photo_job = BAJI_PHOTO_JOB_SYNC;
        ret = baji_photo_net_request_sync(baji_photo_sync_done_cb, baji_photo_sync_progress_cb, NULL);
    }
#endif
    if (ret == BAJI_PHOTO_SYNC_START_OK) {
        baji_photo_sync_label_show((g_baji_photo_job == BAJI_PHOTO_JOB_SEED) ? "seeding" : "syncing", 0u);
        baji_photo_sync_ui_timer_ensure();
    } else if (ret == BAJI_PHOTO_SYNC_START_BUSY) {
        baji_photo_sync_label_show("busy", BAJI_PHOTO_SYNC_RESULT_MS);
        baji_photo_sync_ui_timer_ensure();
    } else {
        baji_photo_sync_label_show((g_baji_photo_job == BAJI_PHOTO_JOB_SEED) ? "seed start failed" :
                                                                              "sync start failed",
                                   BAJI_PHOTO_SYNC_RESULT_MS);
        baji_photo_sync_ui_timer_ensure();
    }

    return ret;
}
