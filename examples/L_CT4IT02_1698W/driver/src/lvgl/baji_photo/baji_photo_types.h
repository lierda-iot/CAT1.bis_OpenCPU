#ifndef BAJI_PHOTO_TYPES_H
#define BAJI_PHOTO_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "baji_photo_config.h"

typedef enum {
    BAJI_PHOTO_SYNC_START_OK = 0,
    BAJI_PHOTO_SYNC_START_BUSY,
    BAJI_PHOTO_SYNC_START_NOT_READY,
    BAJI_PHOTO_SYNC_START_ERROR,
} baji_photo_sync_start_result_t;

typedef enum {
    BAJI_PHOTO_SYNC_STATE_IDLE = 0,
    BAJI_PHOTO_SYNC_STATE_PREPARE_NET,
    BAJI_PHOTO_SYNC_STATE_MOUNT_STORE,
    BAJI_PHOTO_SYNC_STATE_FETCH_INDEX,
    BAJI_PHOTO_SYNC_STATE_PLAN_DIFF,
    BAJI_PHOTO_SYNC_STATE_DOWNLOAD_ONE,
    BAJI_PHOTO_SYNC_STATE_VERIFY_ONE,
    BAJI_PHOTO_SYNC_STATE_COMMIT_ONE,
    BAJI_PHOTO_SYNC_STATE_CLEAN_STALE,
    BAJI_PHOTO_SYNC_STATE_RELOAD_LIST,
    BAJI_PHOTO_SYNC_STATE_DONE,
    BAJI_PHOTO_SYNC_STATE_FAIL,
} baji_photo_sync_state_t;

typedef enum {
    BAJI_PHOTO_FORMAT_BJP = 0,
    BAJI_PHOTO_FORMAT_GIF = 1,
    BAJI_PHOTO_FORMAT_JPEG = 2,
    BAJI_PHOTO_FORMAT_PNG = 3,
} baji_photo_format_t;

typedef enum {
    BAJI_PHOTO_BIND_UNKNOWN = 0,
    BAJI_PHOTO_BIND_UNBOUND,
    BAJI_PHOTO_BIND_BOUND,
    BAJI_PHOTO_BIND_DISABLED,
} baji_photo_bind_status_t;

typedef enum {
    BAJI_PHOTO_MQTT_STATE_IDLE = 0,
    BAJI_PHOTO_MQTT_STATE_CONNECTING,
    BAJI_PHOTO_MQTT_STATE_CONNECTED,
    BAJI_PHOTO_MQTT_STATE_SUBSCRIBED,
    BAJI_PHOTO_MQTT_STATE_BACKOFF,
    BAJI_PHOTO_MQTT_STATE_STOPPED,
} baji_photo_mqtt_state_t;

typedef struct {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint16_t cf;
    uint16_t reserved;
    uint32_t data_size;
    uint32_t crc32;
} baji_photo_file_header_t;

typedef struct {
    char id[BAJI_PHOTO_ID_MAX_LEN + 1u];
    char name[BAJI_PHOTO_NAME_MAX_LEN + 1u];
    char remote_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char local_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    uint32_t file_size;
    uint32_t crc32;
    uint16_t width;
    uint16_t height;
    uint16_t cf;
    baji_photo_format_t format;
} baji_photo_manifest_item_t;

typedef struct {
    uint16_t total;
    uint16_t downloaded;
    uint16_t skipped;
    uint16_t failed;
} baji_photo_sync_result_t;

typedef struct {
    char msg_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char image_url[BAJI_PHOTO_IMAGE_URL_MAX_LEN + 1u];
    char md5[BAJI_PHOTO_IMAGE_MD5_HEX_LEN + 1u];
    char image_format[16];
    char display_mode[16];
    uint32_t image_size;
    uint16_t image_width;
    uint16_t image_height;
    uint32_t chunk_size;
    uint32_t expire_seconds;
    uint32_t trace_id;
    bool support_range;
} baji_photo_image_task_t;

typedef struct {
    baji_photo_bind_status_t bind_status;
    uint32_t heartbeat_interval_s;
    uint32_t image_max_size;
} baji_photo_mqtt_init_info_t;

typedef struct {
    char bind_token[BAJI_PHOTO_BIND_TOKEN_MAX_LEN + 1u];
    char bind_url[BAJI_PHOTO_BIND_URL_MAX_LEN + 1u];
    uint32_t expire_seconds;
} baji_photo_bind_token_info_t;

typedef enum {
    BAJI_PHOTO_BIND_NOTICE_INFO = 0,
    BAJI_PHOTO_BIND_NOTICE_SUCCESS,
    BAJI_PHOTO_BIND_NOTICE_FAIL,
} baji_photo_bind_notice_level_t;

typedef struct {
    baji_photo_bind_notice_level_t level;
    char message[BAJI_PHOTO_MQTT_REASON_MAX_LEN + 1u];
} baji_photo_bind_notice_t;

typedef struct {
    char msg_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char image_url[BAJI_PHOTO_IMAGE_URL_MAX_LEN + 1u];
    char md5[BAJI_PHOTO_IMAGE_MD5_HEX_LEN + 1u];
    char image_format[16];
    char display_mode[16];
    char tmp_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    char final_path[BAJI_PHOTO_PATH_MAX_LEN + 1u];
    uint32_t image_size;
    uint32_t downloaded_size;
    uint32_t chunk_size;
    uint32_t expire_seconds;
    uint16_t image_width;
    uint16_t image_height;
    uint32_t next_retry_at_s;
    uint32_t network_wait_deadline_s;
    uint64_t display_time_ms;
    uint8_t retry_count;
    uint8_t network_wait_count;
    bool support_range;
    bool accepted_sent;
    bool completed;
    bool verified;
    bool waiting_network;
    bool display_pending;
    bool displayed;
} baji_photo_task_state_t;

typedef struct {
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    uint32_t downloaded_size;
    uint32_t trace_id;
    bool range_resumed;
} baji_photo_mqtt_display_request_t;

typedef struct {
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char reason[BAJI_PHOTO_MQTT_REASON_MAX_LEN + 1u];
    int code;
    uint32_t trace_id;
    uint64_t display_time_ms;
} baji_photo_mqtt_display_result_t;

typedef struct {
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    int code;
    uint32_t downloaded_size;
    uint32_t trace_id;
    bool range_resumed;
} baji_photo_mqtt_image_result_t;

typedef struct {
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    uint32_t downloaded_size;
    uint32_t total_size;
    uint32_t percent_bucket;
    bool range_resumed;
} baji_photo_mqtt_image_progress_t;

typedef struct {
    char reply_to[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char reason[BAJI_PHOTO_MQTT_REASON_MAX_LEN + 1u];
    int code;
    uint64_t timestamp_ms;
    uint64_t display_time_ms;
    uint32_t generation;
    uint32_t attempt_count;
} baji_photo_mqtt_pending_reply_t;

typedef struct {
    char image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char trigger[BAJI_PHOTO_PLAY_TRIGGER_MAX_LEN + 1u];
    uint64_t time_ms;
    uint32_t seq;
    uint32_t source_idx;
    uint32_t target_idx;
    uint32_t dyn_seq;
    uint32_t timer_period_ms;
    bool timer_active;
    bool valid;
} baji_photo_playback_meta_t;

typedef struct {
    char last_task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    char current_image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char current_image_md5[BAJI_PHOTO_IMAGE_MD5_HEX_LEN + 1u];
    uint64_t last_display_time_ms;
    baji_photo_playback_meta_t playback;
} baji_photo_device_meta_t;

typedef enum {
    BAJI_PHOTO_MQTT_EVT_CONNECTED = 0,
    BAJI_PHOTO_MQTT_EVT_DISCONNECTED,
    BAJI_PHOTO_MQTT_EVT_INIT_INFO,
    BAJI_PHOTO_MQTT_EVT_BIND_STATUS,
    BAJI_PHOTO_MQTT_EVT_IMAGE_TASK,
    BAJI_PHOTO_MQTT_EVT_IMAGE_PROGRESS,
    BAJI_PHOTO_MQTT_EVT_DISPLAY_REQUEST,
    BAJI_PHOTO_MQTT_EVT_IMAGE_RESULT,
    BAJI_PHOTO_MQTT_EVT_BIND_TOKEN,
    BAJI_PHOTO_MQTT_EVT_BIND_NOTICE,
} baji_photo_mqtt_event_t;

typedef void (*baji_photo_mqtt_event_cb_t)(baji_photo_mqtt_event_t event,
                                           const void *data,
                                           void *ctx);

typedef void (*baji_photo_sync_done_cb_t)(const baji_photo_sync_result_t *result, void *ctx);
typedef void (*baji_photo_sync_progress_cb_t)(baji_photo_sync_state_t state,
                                              uint16_t current,
                                              uint16_t total,
                                              void *ctx);

#endif
