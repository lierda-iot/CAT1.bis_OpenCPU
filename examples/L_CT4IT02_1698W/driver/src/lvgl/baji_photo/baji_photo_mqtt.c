#include "baji_photo_mqtt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lvgl.h"
#include "liot_dev.h"
#include "liot_external_flash_fs.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_mqtt_client.h"
#include "liot_os.h"
#include "liot_rtc.h"

#include "baji_photo_config.h"
#include "baji_photo_diag.h"
#include "baji_photo_http.h"
#include "baji_photo_store.h"

#define BAJI_PHOTO_MQTT_TRACE(fmt, ...) liot_trace("[baji_mqtt] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)
#define BAJI_PHOTO_MQTT_INDEX_READBACK_FAIL (-0x5A05)
#define BAJI_PHOTO_MQTT_FINAL_REPLY_ACK_TIMEOUT_S 30u

#if BAJI_PHOTO_ENABLE_MQTT_CONTROL

typedef struct {
    liot_task_t task;
    liot_sem_t conn_sem;
    liot_sem_t req_sem;
    liot_mqtt_client_t client;
    baji_photo_mqtt_state_t state;
    baji_photo_bind_status_t bind_status;
    char real_imei[16];
    char imei[16];
    char client_id[BAJI_PHOTO_MQTT_CLIENT_ID_MAX + 1u];
    char topic_down[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_cmd[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_status[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_event[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_reply[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_up[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char topic_bind[BAJI_PHOTO_MQTT_TOPIC_MAX];
    volatile bool running;
    volatile bool stop_requested;
    bool client_inited;
    bool connected;
    bool subscribed;
    uint32_t heartbeat_interval_s;
    uint32_t heartbeat_countdown_s;
    uint32_t bind_token_refresh_countdown_s;
    uint32_t reconnect_backoff_s;
    uint32_t image_max_size;
    uint32_t msg_seq;
    char unbind_request_id[40];
    bool msg_pending;
    bool has_current_task;
    bool current_task_waiting_sync;
    bool has_pending_reply;
    bool display_result_pending;
    bool pending_reply_waiting_ack;
    bool pending_reply_ack_pending;
    bool pending_reply_store_known_empty;
    int pending_reply_ack_err;
    int fatal_error;
    uint32_t pending_reply_ack_generation;
    uint32_t pending_reply_publish_at_s;
    char last_task_id[BAJI_PHOTO_TASK_ID_MAX_LEN + 1u];
    baji_photo_device_meta_t device_meta;
    baji_photo_bind_token_info_t bind_token_info;
    baji_photo_image_task_t current_task;
    baji_photo_task_state_t current_task_state;
    baji_photo_mqtt_pending_reply_t pending_reply;
    baji_photo_mqtt_display_result_t display_result;
    baji_photo_mqtt_event_cb_t event_cb;
    void *event_ctx;
    char rx_topic[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char rx_payload[BAJI_PHOTO_MQTT_RX_BUF_MAX];
} baji_photo_mqtt_ctx_t;

static void baji_photo_mqtt_marker_display_req(const baji_photo_mqtt_display_request_t *request)
{
    if (request == NULL) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("MQTT_DISPLAY_REQ op=%lu task=%s image=%s bytes=%lu heap_min=%lu",
                          (unsigned long)request->trace_id,
                          baji_photo_diag_id_tail(request->task_id),
                          baji_photo_diag_id_tail(request->image_id),
                          (unsigned long)request->downloaded_size,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_mqtt_marker_wait_network(const baji_photo_mqtt_ctx_t *ctx, int ret)
{
    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("MQTT_WAIT_NETWORK op=%lu task=%s image=%s ret=%d dl=%lu n=%u heap_min=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          baji_photo_diag_id_tail(ctx->current_task.task_id),
                          baji_photo_diag_id_tail(ctx->current_task.image_id),
                          ret,
                          (unsigned long)ctx->current_task_state.downloaded_size,
                          (unsigned int)ctx->current_task_state.network_wait_count,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_mqtt_marker_reply_ack(const baji_photo_mqtt_pending_reply_t *reply, int ack_err)
{
    if (reply == NULL) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("MQTT_REPLY_ACK gen=%lu task=%s image=%s code=%d err=%d heap_min=%lu",
                          (unsigned long)reply->generation,
                          baji_photo_diag_id_tail(reply->task_id),
                          baji_photo_diag_id_tail(reply->image_id),
                          reply->code,
                          ack_err,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_mqtt_marker_tail_result(const char *stage,
                                               const baji_photo_image_task_t *task,
                                               const baji_photo_http_image_result_t *result,
                                               int code,
                                               int ret)
{
    if ((stage == NULL) || (task == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu task=%s image=%s code=%d ret=%d http=%d dl=%lu range=%d heap_min=%lu",
                          stage,
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          code,
                          ret,
                          (result != NULL) ? result->http_status : -1,
                          (unsigned long)((result != NULL) ? result->downloaded_size : 0u),
                          ((result != NULL) && result->range_resumed) ? 1 : 0,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_mqtt_marker_reply_pub(const baji_photo_mqtt_pending_reply_t *reply,
                                             int ret,
                                             bool ack_wait)
{
    if (reply == NULL) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("MQTT_REPLY_PUB gen=%lu task=%s image=%s code=%d ret=%d ack_wait=%d attempt=%lu heap_min=%lu",
                          (unsigned long)reply->generation,
                          baji_photo_diag_id_tail(reply->task_id),
                          baji_photo_diag_id_tail(reply->image_id),
                          reply->code,
                          ret,
                          ack_wait ? 1 : 0,
                          (unsigned long)reply->attempt_count,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

typedef struct {
    baji_photo_mqtt_ctx_t *mqtt;
    bool range_resumed;
    bool half_sent;
    bool ui_nonzero_progress_sent;
    uint32_t last_ui_bucket;
    bool ui_progress_sent;
} baji_photo_mqtt_progress_ctx_t;

typedef struct {
    unsigned long total_bytes;
    unsigned long free_bytes;
} baji_photo_mqtt_storage_info_t;

typedef struct {
    baji_photo_mqtt_event_cb_t cb;
    void *ctx;
} baji_photo_mqtt_event_sink_t;

typedef struct {
    liot_task_t task;
    baji_photo_mqtt_state_t state;
    baji_photo_bind_status_t bind_status;
    bool running;
    bool connected;
    bool subscribed;
    bool has_current_task;
    bool current_task_waiting_sync;
    bool current_task_display_pending;
    bool current_task_displayed;
    bool has_pending_reply;
    bool display_result_pending;
    bool pending_reply_waiting_ack;
    uint32_t heartbeat_countdown_s;
    int fatal_error;
} baji_photo_mqtt_runtime_snapshot_t;

typedef struct {
    bool session_ready;
    bool needs_subscribe;
    bool has_display_result;
    bool has_pending_reply;
    bool waiting_display_ack;
    bool has_current_task;
    bool heartbeat_due;
} baji_photo_mqtt_loop_view_t;

static baji_photo_mqtt_ctx_t s_baji_photo_mqtt;

static baji_photo_mqtt_ctx_t *baji_photo_mqtt_default(void)
{
    return &s_baji_photo_mqtt;
}

#if BAJI_PHOTO_ENABLE_INDEX_READBACK_DIAG
static void baji_photo_mqtt_marker_index_readback(const baji_photo_mqtt_ctx_t *ctx,
                                                  const char *stage,
                                                  const baji_photo_manifest_item_t *item,
                                                  unsigned int saved_count,
                                                  unsigned int readback_count,
                                                  int found,
                                                  int ret)
{
    if ((stage == NULL) || (item == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu image=%s kind=%s saved=%u loaded=%u found=%d ret=%d size=%lu dims=%ux%u heap_min=%lu",
                          stage,
                          (unsigned long)(((ctx != NULL) && ctx->has_current_task) ?
                                              ctx->current_task.trace_id : 0u),
                          baji_photo_diag_id_tail(item->id),
                          (item->format == BAJI_PHOTO_FORMAT_GIF) ? "gif" : "photo",
                          saved_count,
                          readback_count,
                          (found >= 0) ? 1 : 0,
                          ret,
                          (unsigned long)item->file_size,
                          (unsigned int)item->width,
                          (unsigned int)item->height,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}
#endif

static int baji_photo_mqtt_upsert_downloaded_item(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_manifest_item_t *item);
#if BAJI_PHOTO_ENABLE_INDEX_READBACK_DIAG
static int baji_photo_mqtt_trace_index_readback(const baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_manifest_item_t *item,
                                                unsigned int saved_count);
#endif
static void baji_photo_mqtt_prepare_pending_reply(const baji_photo_image_task_t *task,
                                                  int code,
                                                  uint64_t display_time_ms,
                                                  const char *reason,
                                                  baji_photo_mqtt_pending_reply_t *reply);
static void baji_photo_mqtt_track_pending_reply(baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_mqtt_pending_reply_t *reply);
static uint32_t baji_photo_mqtt_next_msg_id(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_publish(baji_photo_mqtt_ctx_t *ctx, const char *topic,
                                   const char *payload, unsigned short payload_len);
static int baji_photo_mqtt_publish_unbind_request(baji_photo_mqtt_ctx_t *ctx)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    int written;

    if (ctx == NULL) return LIOT_MQTTCLIENT_INVALID_PARAM;
    (void)snprintf(ctx->unbind_request_id, sizeof(ctx->unbind_request_id),
                   "unbind_req_%lu",
                   (unsigned long)baji_photo_mqtt_next_msg_id(ctx));
    written = snprintf(payload, sizeof(payload),
                       "{\"msg_id\":\"%s\",\"type\":\"bind.unbind.request\","
                       "\"imei\":\"%s\",\"timestamp\":0,\"version\":\"1.0\","
                       "\"data\":{\"reason\":\"device_unbind\"}}",
                       ctx->unbind_request_id, ctx->imei);
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload)))
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    return baji_photo_mqtt_publish(ctx, ctx->topic_bind, payload,
                                   (unsigned short)written);
}
static int baji_photo_mqtt_request_bind_token_internal(baji_photo_mqtt_ctx_t *ctx, const char *reason);
static void baji_photo_mqtt_log_identity(const baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_emit_bind_notice(baji_photo_mqtt_ctx_t *ctx,
                                             baji_photo_bind_notice_level_t level,
                                             const char *message);
static void baji_photo_mqtt_emit_display_request(baji_photo_mqtt_ctx_t *ctx,
                                                 const baji_photo_http_image_result_t *download_result);
static bool baji_photo_mqtt_take_display_result(baji_photo_mqtt_ctx_t *ctx,
                                                baji_photo_mqtt_display_result_t *result);
static void baji_photo_mqtt_final_reply_req_cb(liot_mqtt_client_t *client, void *arg, int err);
static void baji_photo_mqtt_pending_reply_ack_reset(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_prepare_pending_reply_runtime(baji_photo_mqtt_ctx_t *ctx,
                                                          baji_photo_mqtt_pending_reply_t *reply);
static int baji_photo_mqtt_publish_pending_reply_start(baji_photo_mqtt_ctx_t *ctx,
                                                       baji_photo_mqtt_pending_reply_t *reply);
static void baji_photo_mqtt_process_pending_reply_ack(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_publish_image_result_fields_ex(baji_photo_mqtt_ctx_t *ctx,
                                                          const char *reply_to,
                                                          const char *task_id,
                                                          const char *image_id,
                                                          int code,
                                                          uint64_t timestamp_ms,
                                                          uint64_t display_time_ms,
                                                          const char *message,
                                                          const char *reason,
                                                          liot_mqtt_request_cb_t cb,
                                                          void *arg,
                                                          bool wait_req);
static void baji_photo_mqtt_process_display_result(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_finish_current_task(baji_photo_mqtt_ctx_t *ctx, bool success);
static int baji_photo_mqtt_save_current_task_state(baji_photo_mqtt_ctx_t *ctx, const char *phase);
static int baji_photo_mqtt_init_identity(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_init_once(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_reset_runtime(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_set_event_sink(baji_photo_mqtt_ctx_t *ctx,
                                           baji_photo_mqtt_event_cb_t cb,
                                           void *event_ctx);
static baji_photo_mqtt_event_sink_t baji_photo_mqtt_get_event_sink(const baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_activate_current_task(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_image_task_t *task,
                                                  const baji_photo_task_state_t *state,
                                                  bool refresh_trace_id);
static void baji_photo_mqtt_clear_current_task_runtime(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_set_runtime_state(baji_photo_mqtt_ctx_t *ctx,
                                              baji_photo_mqtt_state_t state);
static void baji_photo_mqtt_mark_connected(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_mark_subscribed(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_mark_disconnected(baji_photo_mqtt_ctx_t *ctx,
                                              baji_photo_mqtt_state_t state);
static void baji_photo_mqtt_set_fatal_error(baji_photo_mqtt_ctx_t *ctx, int fatal_error);
static baji_photo_mqtt_runtime_snapshot_t baji_photo_mqtt_get_runtime_snapshot(
    const baji_photo_mqtt_ctx_t *ctx);
static baji_photo_mqtt_loop_view_t baji_photo_mqtt_get_loop_view(const baji_photo_mqtt_ctx_t *ctx,
                                                                 int mqtt_state);
static bool baji_photo_mqtt_has_pending_reply_work(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_begin_connect_attempt(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_finish_connect_success(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_wait_reconnect_backoff(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_schedule_next_heartbeat(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_tick_heartbeat_countdown(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_process_idle_heartbeat(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_process_post_connect_setup(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_load_pending_reply(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_publish_online(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_publish_heartbeat(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_subscribe_all(baji_photo_mqtt_ctx_t *ctx);
static int baji_photo_mqtt_client_cleanup(baji_photo_mqtt_ctx_t *ctx);
static void baji_photo_mqtt_task_exit(baji_photo_mqtt_ctx_t *ctx);

static bool baji_photo_mqtt_is_valid_imei(const char *imei)
{
    size_t i;

    if (imei == NULL) {
        return false;
    }
    if (strlen(imei) != 15u) {
        return false;
    }
    for (i = 0; i < 15u; ++i) {
        if ((imei[i] < '0') || (imei[i] > '9')) {
            return false;
        }
    }
    return true;
}

static void baji_photo_mqtt_mask_identifier(const char *src, char *dst, size_t dst_len)
{
    size_t len;

    if ((dst == NULL) || (dst_len == 0u)) {
        return;
    }

    dst[0] = '\0';
    if ((src == NULL) || (src[0] == '\0')) {
        (void)snprintf(dst, dst_len, "%s", "n/a");
        return;
    }

    len = strlen(src);
    if (len <= 4u) {
        (void)snprintf(dst, dst_len, "%s", src);
        return;
    }

    (void)snprintf(dst, dst_len, "***%s", src + len - 4u);
}

static bool baji_photo_mqtt_has_text(const char *value)
{
    return (value != NULL) && (value[0] != '\0');
}

static void baji_photo_mqtt_copy_json_safe_string(char *dst,
                                                  size_t dst_len,
                                                  const char *src,
                                                  const char *fallback)
{
    size_t i = 0u;
    size_t j = 0u;
    const char *input = src;

    if ((dst == NULL) || (dst_len == 0u)) {
        return;
    }

    if (!baji_photo_mqtt_has_text(input)) {
        input = fallback;
    }
    if (!baji_photo_mqtt_has_text(input)) {
        input = "unknown";
    }

    while ((input[i] != '\0') && (j + 1u < dst_len)) {
        char ch = input[i++];

        if ((ch == '"') || (ch == '\\') || ((unsigned char)ch < 0x20u)) {
            dst[j++] = '_';
        } else {
            dst[j++] = ch;
        }
    }
    dst[j] = '\0';

    if (j == 0u) {
        (void)snprintf(dst, dst_len, "%s", "unknown");
    }
}

static void baji_photo_mqtt_get_firmware_version(char *buf, size_t buf_len)
{
#if BAJI_PHOTO_MQTT_ONLINE_DYNAMIC_DEVICE_INFO
    char version[64];
#endif

    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }

#if BAJI_PHOTO_MQTT_ONLINE_DYNAMIC_DEVICE_INFO
    memset(version, 0, sizeof(version));
    if (liot_dev_get_firmware_version(version, sizeof(version)) != 0) {
        version[0] = '\0';
    }
    baji_photo_mqtt_copy_json_safe_string(buf, buf_len, version, "unknown");
#else
    baji_photo_mqtt_copy_json_safe_string(buf,
                                          buf_len,
                                          BAJI_PHOTO_MQTT_ONLINE_FIRMWARE_VERSION,
                                          "dev-x");
#endif
}

static void baji_photo_mqtt_get_hardware_version(char *buf, size_t buf_len)
{
#if BAJI_PHOTO_MQTT_ONLINE_DYNAMIC_DEVICE_INFO
    char model[64];
    char product_id[64];
#endif

    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }

#if BAJI_PHOTO_MQTT_ONLINE_DYNAMIC_DEVICE_INFO
    memset(model, 0, sizeof(model));
    if ((liot_dev_get_model(model, sizeof(model)) == 0) && (model[0] != '\0')) {
        baji_photo_mqtt_copy_json_safe_string(buf, buf_len, model, "unknown");
        return;
    }

    memset(product_id, 0, sizeof(product_id));
    if ((liot_dev_get_product_id(product_id, sizeof(product_id)) == 0) &&
        (product_id[0] != '\0')) {
        baji_photo_mqtt_copy_json_safe_string(buf, buf_len, product_id, "unknown");
        return;
    }

    baji_photo_mqtt_copy_json_safe_string(buf, buf_len, NULL, "unknown");
#else
    baji_photo_mqtt_copy_json_safe_string(buf,
                                          buf_len,
                                          BAJI_PHOTO_MQTT_ONLINE_HARDWARE_VERSION,
                                          "A1");
#endif
}

static int baji_photo_mqtt_get_rssi_dbm(void)
{
    return 0;
}

static const char *baji_photo_mqtt_screen_status(const baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot;

    if (ctx == NULL) {
        return "error";
    }
    snapshot = baji_photo_mqtt_get_runtime_snapshot(ctx);
    if ((snapshot.fatal_error != 0) || (snapshot.bind_status == BAJI_PHOTO_BIND_DISABLED)) {
        return "error";
    }
    if (snapshot.bind_status != BAJI_PHOTO_BIND_BOUND) {
        return "binding";
    }
    if (snapshot.current_task_waiting_sync || snapshot.display_result_pending) {
        return "downloading";
    }
    if (snapshot.has_current_task) {
        if (snapshot.current_task_displayed) {
            return "displaying";
        }
        return "downloading";
    }
    if (baji_photo_mqtt_has_text(ctx->device_meta.current_image_id)) {
        return "displaying";
    }
    return "idle";
}

static void baji_photo_mqtt_request_fast_heartbeat(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->heartbeat_countdown_s = 0u;
}

static const char *baji_photo_mqtt_topic_role(const baji_photo_mqtt_ctx_t *ctx, const char *topic)
{
    if ((ctx == NULL) || (topic == NULL)) {
        return "n/a";
    }
    if (strcmp(topic, ctx->topic_down) == 0) {
        return "down";
    }
    if (strcmp(topic, ctx->topic_cmd) == 0) {
        return "cmd";
    }
    if (strcmp(topic, ctx->topic_status) == 0) {
        return "status";
    }
    if (strcmp(topic, ctx->topic_event) == 0) {
        return "event";
    }
    if (strcmp(topic, ctx->topic_reply) == 0) {
        return "reply";
    }
    if (strcmp(topic, ctx->topic_up) == 0) {
        return "up";
    }
    return "custom";
}

static uint32_t baji_photo_mqtt_now_s(void)
{
    int32_t rtc_now = (int32_t)liot_rtc_get_time_s();

    if (rtc_now > 0) {
        return (uint32_t)rtc_now;
    }
    return liot_rtos_get_running_time();
}

static uint64_t baji_photo_mqtt_now_ms(void)
{
    uint64_t timestamp = 0u;

    if (Liot_GetTimestamp(&timestamp) == 0) {
        return timestamp;
    }
    return (uint64_t)baji_photo_mqtt_now_s() * 1000u;
}

static uint64_t baji_photo_mqtt_status_timestamp_ms(void)
{
#if BAJI_PHOTO_MQTT_STATUS_DYNAMIC_TIMESTAMP
    return baji_photo_mqtt_now_ms();
#else
    return 0u;
#endif
}

static void baji_photo_mqtt_get_storage_info(baji_photo_mqtt_storage_info_t *info)
{
    int free_size;

    if (info == NULL) {
        return;
    }

    info->total_bytes = (unsigned long)BAJI_FLASH_LFS_TOTAL;
    info->free_bytes = 0u;
    if (!baji_photo_store_is_ready()) {
        return;
    }

    free_size = baji_photo_store_free_size();
    if (free_size > 0) {
        info->free_bytes = (unsigned long)free_size;
    }
}

static uint32_t baji_photo_mqtt_network_wait_window_s(const baji_photo_task_state_t *state)
{
    uint32_t wait_s = BAJI_PHOTO_NETWORK_WAIT_DEADLINE_S;

    if ((state != NULL) && (state->expire_seconds != 0u) && (state->expire_seconds < wait_s)) {
        wait_s = state->expire_seconds;
    }
    if (wait_s == 0u) {
        wait_s = 1u;
    }
    return wait_s;
}

static void baji_photo_mqtt_clear_network_wait_state(baji_photo_task_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->waiting_network = false;
    state->next_retry_at_s = 0u;
    state->network_wait_deadline_s = 0u;
    state->network_wait_count = 0u;
}

static bool baji_photo_mqtt_parse_task_format(const char *image_format,
                                              baji_photo_format_t *out_format)
{
    if ((image_format == NULL) || (out_format == NULL)) {
        return false;
    }

    if ((strcmp(image_format, "rgb565") == 0) ||
        (strcmp(image_format, "RGB565") == 0) ||
        (strcmp(image_format, "bjp") == 0) ||
        (strcmp(image_format, "BJP") == 0)) {
        *out_format = BAJI_PHOTO_FORMAT_BJP;
        return true;
    }
#if BAJI_PHOTO_ENABLE_GIF_SUPPORT
    if ((strcmp(image_format, "gif") == 0) ||
        (strcmp(image_format, "GIF") == 0)) {
        *out_format = BAJI_PHOTO_FORMAT_GIF;
        return true;
    }
#endif
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
    if ((strcmp(image_format, "jpeg") == 0) ||
        (strcmp(image_format, "JPEG") == 0) ||
        (strcmp(image_format, "jpg") == 0) ||
        (strcmp(image_format, "JPG") == 0)) {
        *out_format = BAJI_PHOTO_FORMAT_JPEG;
        return true;
    }
#endif

    return false;
}

static const char *baji_photo_mqtt_task_format_name(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_GIF:
        return "gif";
    case BAJI_PHOTO_FORMAT_JPEG:
        return "jpeg";
    case BAJI_PHOTO_FORMAT_BJP:
    default:
        return "bjp";
    }
}

static bool baji_photo_mqtt_normalize_task_format(const char *image_format,
                                                  char *out_format,
                                                  unsigned int out_len,
                                                  baji_photo_format_t *out_task_format)
{
    baji_photo_format_t task_format;
    const char *format_name;
    char normalized[16];
    int written;

    if ((image_format == NULL) || (out_format == NULL) || (out_len == 0u)) {
        return false;
    }
    if (!baji_photo_mqtt_parse_task_format(image_format, &task_format)) {
        return false;
    }

    format_name = baji_photo_mqtt_task_format_name(task_format);
    written = snprintf(normalized, sizeof(normalized), "%s", format_name);
    if ((written < 0) || ((unsigned int)written >= sizeof(normalized))) {
        return false;
    }
    written = snprintf(out_format, out_len, "%s", normalized);
    if ((written < 0) || ((unsigned int)written >= out_len)) {
        if (out_len > 0u) {
            out_format[0] = '\0';
        }
        return false;
    }
    if (out_task_format != NULL) {
        *out_task_format = task_format;
    }
    return true;
}

static bool baji_photo_mqtt_task_formats_match(const char *lhs, const char *rhs)
{
    baji_photo_format_t lhs_format;
    baji_photo_format_t rhs_format;

    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }
    if (baji_photo_mqtt_parse_task_format(lhs, &lhs_format) &&
        baji_photo_mqtt_parse_task_format(rhs, &rhs_format)) {
        return lhs_format == rhs_format;
    }
    return strcmp(lhs, rhs) == 0;
}

static uint32_t baji_photo_mqtt_task_local_max_size(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_JPEG:
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        return BAJI_PHOTO_JPEG_MAX_COMPRESSED_SIZE;
#else
        return 0u;
#endif
    default:
        return 0u;
    }
}

static uint32_t baji_photo_mqtt_image_format_local_max_size(const char *image_format)
{
    baji_photo_format_t format;

    if ((image_format == NULL) || !baji_photo_mqtt_parse_task_format(image_format, &format)) {
        return 0u;
    }

    return baji_photo_mqtt_task_local_max_size(format);
}

static void baji_photo_mqtt_set_validate_reason(const char **out_reason,
                                                const char *reason)
{
    if ((out_reason != NULL) && (reason != NULL)) {
        *out_reason = reason;
    }
}

static unsigned long baji_photo_mqtt_expected_image_size(const baji_photo_image_task_t *task)
{
    baji_photo_format_t format;

    if (task == NULL) {
        return 0ul;
    }

    if (baji_photo_mqtt_parse_task_format(task->image_format, &format) &&
        (format != BAJI_PHOTO_FORMAT_BJP)) {
        return (unsigned long)task->image_size;
    }

    return (unsigned long)task->image_width *
           (unsigned long)task->image_height *
           (unsigned long)BAJI_PHOTO_IMG_BPP;
}

static bool baji_photo_mqtt_retryable_network_error(int ret)
{
    return (ret == LIOT_HTTPC_ERR_NO_NETWORK) ||
           (ret == LIOT_HTTPC_ERR_SOCKET_FAILURE) ||
           (ret == LIOT_HTTPC_ERR_TIMEOUT) ||
           (ret == BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL);
}

static const char *baji_photo_mqtt_network_error_name(int ret)
{
    switch (ret) {
    case LIOT_HTTPC_ERR_NO_NETWORK:
        return "no_network";
    case LIOT_HTTPC_ERR_SOCKET_FAILURE:
        return "socket_failure";
    case LIOT_HTTPC_ERR_TIMEOUT:
        return "timeout";
    case BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL:
        return "legacy_pdp_active_fail";
    default:
        break;
    }

    return "network_like";
}

static void baji_photo_mqtt_build_network_wait_reason(char *buf,
                                                      unsigned int buf_len,
                                                      const baji_photo_task_state_t *state,
                                                      int ret)
{
    unsigned long downloaded = 0ul;
    unsigned int wait_count = 0u;

    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }
    if (state != NULL) {
        downloaded = (unsigned long)state->downloaded_size;
        wait_count = (unsigned int)state->network_wait_count;
    }
    (void)snprintf(buf,
                   buf_len,
                   "network_not_ready ret=%d downloaded=%lu wait=%u",
                   ret,
                   downloaded,
                   wait_count);
}

static int baji_photo_mqtt_save_device_meta(baji_photo_mqtt_ctx_t *ctx,
                                            const char *task_id,
                                            const char *image_id,
                                            const char *image_md5,
                                            uint64_t display_time_ms)
{
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int ret;

    if ((ctx == NULL) || (task_id == NULL) || (image_id == NULL) || (image_md5 == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    (void)snprintf(ctx->device_meta.last_task_id,
                   sizeof(ctx->device_meta.last_task_id),
                   "%s",
                   task_id);
    (void)snprintf(ctx->device_meta.current_image_id,
                   sizeof(ctx->device_meta.current_image_id),
                   "%s",
                   image_id);
    (void)snprintf(ctx->device_meta.current_image_md5,
                   sizeof(ctx->device_meta.current_image_md5),
                   "%s",
                   image_md5);
    ctx->device_meta.last_display_time_ms = display_time_ms;
    (void)snprintf(ctx->last_task_id, sizeof(ctx->last_task_id), "%s", task_id);

    ret = baji_photo_store_update_device_meta_mqtt(task_id,
                                                   image_id,
                                                   image_md5,
                                                   display_time_ms);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("save device meta fail ret=%d task=%s image=%s display_ms=%s",
                              ret,
                              task_id,
                              image_id,
                              baji_photo_diag_u64_dec(display_time_ms,
                                                      display_ms_buf,
                                                      sizeof(display_ms_buf)));
    }
    return ret;
}

static void baji_photo_mqtt_load_device_meta(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_device_meta_t meta;
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int ret;

    if (ctx == NULL) {
        return;
    }

    memset(&meta, 0, sizeof(meta));
    ret = baji_photo_store_load_device_meta(&meta);
    if (ret != 0) {
        return;
    }

    ctx->device_meta = meta;
    if (ctx->device_meta.last_task_id[0] != '\0') {
        (void)snprintf(ctx->last_task_id,
                       sizeof(ctx->last_task_id),
                       "%s",
                       ctx->device_meta.last_task_id);
    }
    BAJI_PHOTO_MQTT_TRACE("load device meta last_task=%s image=%s display_ms=%s",
                          ctx->device_meta.last_task_id,
                          ctx->device_meta.current_image_id,
                          baji_photo_diag_u64_dec(ctx->device_meta.last_display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)));
}

static void baji_photo_mqtt_reset_bind_token_info(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(&ctx->bind_token_info, 0, sizeof(ctx->bind_token_info));
    ctx->bind_token_refresh_countdown_s = 0u;
}

static void baji_photo_mqtt_schedule_bind_token_retry(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->bind_token_refresh_countdown_s = BAJI_PHOTO_BIND_TOKEN_RETRY_S;
}

static void baji_photo_mqtt_schedule_bind_token_refresh(baji_photo_mqtt_ctx_t *ctx,
                                                        uint32_t expire_seconds)
{
    if (ctx == NULL) {
        return;
    }

    if (expire_seconds > BAJI_PHOTO_BIND_TOKEN_REFRESH_LEAD_S) {
        ctx->bind_token_refresh_countdown_s =
            expire_seconds - BAJI_PHOTO_BIND_TOKEN_REFRESH_LEAD_S;
    } else {
        ctx->bind_token_refresh_countdown_s = 1u;
    }
}

static void baji_photo_mqtt_tick_bind_token_refresh(baji_photo_mqtt_ctx_t *ctx)
{
    const char *reason;
    int ret;

    if ((ctx == NULL) || !ctx->connected || !ctx->subscribed) {
        return;
    }
    if (ctx->bind_status != BAJI_PHOTO_BIND_UNBOUND) {
        ctx->bind_token_refresh_countdown_s = 0u;
        return;
    }
    if (ctx->bind_token_refresh_countdown_s > 0u) {
        ctx->bind_token_refresh_countdown_s -= 1u;
        if (ctx->bind_token_refresh_countdown_s > 0u) {
            return;
        }
    }

    reason = (ctx->bind_token_info.bind_url[0] != '\0') ? "token_refresh" : "device_unbound";
    ret = baji_photo_mqtt_request_bind_token_internal(ctx, reason);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("bind token refresh fail ret=%d reason=%s",
                              ret,
                              reason);
        baji_photo_mqtt_emit_bind_notice(ctx,
                                         BAJI_PHOTO_BIND_NOTICE_FAIL,
                                         "QR refresh failed, retrying");
    }
    baji_photo_mqtt_schedule_bind_token_retry(ctx);
}

static void baji_photo_mqtt_sem_signal(liot_sem_t sem)
{
    if (sem != NULL) {
        liot_rtos_semaphore_release(sem);
    }
}

static void baji_photo_mqtt_sem_drain(liot_sem_t sem)
{
    if (sem == NULL) {
        return;
    }
    while (liot_rtos_semaphore_wait(sem, LIOT_NO_WAIT) == 0) {
    }
}

static void baji_photo_mqtt_set_event_sink(baji_photo_mqtt_ctx_t *ctx,
                                           baji_photo_mqtt_event_cb_t cb,
                                           void *event_ctx)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->event_cb = cb;
    ctx->event_ctx = event_ctx;
    liot_rtos_exit_critical();
}

static baji_photo_mqtt_event_sink_t baji_photo_mqtt_get_event_sink(const baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_event_sink_t sink;

    memset(&sink, 0, sizeof(sink));
    if (ctx == NULL) {
        return sink;
    }

    liot_rtos_enter_critical();
    sink.cb = ctx->event_cb;
    sink.ctx = ctx->event_ctx;
    liot_rtos_exit_critical();
    return sink;
}

static void baji_photo_mqtt_emit_event(baji_photo_mqtt_ctx_t *ctx,
                                       baji_photo_mqtt_event_t event,
                                       const void *data)
{
    baji_photo_mqtt_event_sink_t sink = baji_photo_mqtt_get_event_sink(ctx);

    if (sink.cb != NULL) {
        sink.cb(event, data, sink.ctx);
    }
}

static void baji_photo_mqtt_emit_bind_notice(baji_photo_mqtt_ctx_t *ctx,
                                             baji_photo_bind_notice_level_t level,
                                             const char *message)
{
    baji_photo_bind_notice_t notice;

    if ((ctx == NULL) || !baji_photo_mqtt_has_text(message)) {
        return;
    }

    memset(&notice, 0, sizeof(notice));
    notice.level = level;
    (void)snprintf(notice.message, sizeof(notice.message), "%s", message);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_BIND_NOTICE, &notice);
}

static baji_photo_bind_status_t baji_photo_mqtt_parse_bind_status(const cJSON *item)
{
    const char *value;

    if (!cJSON_IsString(item) || (item->valuestring == NULL)) {
        return BAJI_PHOTO_BIND_UNKNOWN;
    }
    value = item->valuestring;
    if (strcmp(value, "unbound") == 0) {
        return BAJI_PHOTO_BIND_UNBOUND;
    }
    if (strcmp(value, "bound") == 0) {
        return BAJI_PHOTO_BIND_BOUND;
    }
    if (strcmp(value, "disabled") == 0) {
        return BAJI_PHOTO_BIND_DISABLED;
    }
    return BAJI_PHOTO_BIND_UNKNOWN;
}

static void baji_photo_mqtt_build_topics(baji_photo_mqtt_ctx_t *ctx)
{
#if BAJI_PHOTO_MQTT_USE_PLAIN_CLIENT_ID
    (void)snprintf(ctx->client_id,
                   sizeof(ctx->client_id),
                   "%s",
                   ctx->imei);
#else
    (void)snprintf(ctx->client_id,
                   sizeof(ctx->client_id),
                   "%s%s",
                   BAJI_PHOTO_MQTT_CLIENT_ID_PREFIX,
                   ctx->imei);
#endif
    (void)snprintf(ctx->topic_down, sizeof(ctx->topic_down), "/badge/%s/down", ctx->imei);
    (void)snprintf(ctx->topic_cmd, sizeof(ctx->topic_cmd), "/badge/%s/cmd", ctx->imei);
    (void)snprintf(ctx->topic_status, sizeof(ctx->topic_status), "/badge/%s/status", ctx->imei);
    (void)snprintf(ctx->topic_event, sizeof(ctx->topic_event), "/badge/%s/event", ctx->imei);
    (void)snprintf(ctx->topic_reply, sizeof(ctx->topic_reply), "/badge/%s/reply", ctx->imei);
    (void)snprintf(ctx->topic_up, sizeof(ctx->topic_up), "/badge/%s/up", ctx->imei);
    (void)snprintf(ctx->topic_bind, sizeof(ctx->topic_bind), "/badge/%s/bind", ctx->imei);
}

static void baji_photo_mqtt_log_identity(const baji_photo_mqtt_ctx_t *ctx)
{
    char real_id[16];
    char effective_id[16];
    char client_id[16];

    if (ctx == NULL) {
        return;
    }

    baji_photo_mqtt_mask_identifier(ctx->real_imei, real_id, sizeof(real_id));
    baji_photo_mqtt_mask_identifier(ctx->imei, effective_id, sizeof(effective_id));
    baji_photo_mqtt_mask_identifier(ctx->client_id, client_id, sizeof(client_id));

    BAJI_PHOTO_MQTT_TRACE("device identity real_imei=%s effective_id=%s client_id=%s fixed_id=%d plain_client_id=%d",
                          real_id,
                          effective_id,
                          client_id,
                          BAJI_PHOTO_MQTT_USE_FIXED_DEVICE_ID ? 1 : 0,
                          BAJI_PHOTO_MQTT_USE_PLAIN_CLIENT_ID ? 1 : 0);
    BAJI_PHOTO_MQTT_TRACE("mqtt topics configured down/cmd/up/reply");
}

static int baji_photo_mqtt_init_identity(baji_photo_mqtt_ctx_t *ctx)
{
    int ret = 0;
    bool real_imei_ok = false;

    if (ctx == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    memset(ctx->real_imei, 0, sizeof(ctx->real_imei));
    memset(ctx->imei, 0, sizeof(ctx->imei));

    ret = liot_dev_get_imei(ctx->real_imei, sizeof(ctx->real_imei), 0);
    if ((ret == 0) && baji_photo_mqtt_is_valid_imei(ctx->real_imei)) {
        real_imei_ok = true;
    } else {
        memset(ctx->real_imei, 0, sizeof(ctx->real_imei));
    }

#if BAJI_PHOTO_MQTT_USE_FIXED_DEVICE_ID
    if (!baji_photo_mqtt_is_valid_imei(BAJI_PHOTO_MQTT_FIXED_DEVICE_ID)) {
        BAJI_PHOTO_MQTT_TRACE("fixed device id invalid");
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }
    (void)snprintf(ctx->imei, sizeof(ctx->imei), "%s", BAJI_PHOTO_MQTT_FIXED_DEVICE_ID);
    return 0;
#else
    if (!real_imei_ok) {
        if (ret == 0) {
            ret = LIOT_DEV_IMEI_GET_ERR;
        }
        BAJI_PHOTO_MQTT_TRACE("real imei invalid ret=%d", ret);
        return ret;
    }
    (void)snprintf(ctx->imei, sizeof(ctx->imei), "%s", ctx->real_imei);
    return 0;
#endif
}

static void baji_photo_mqtt_task_exit(baji_photo_mqtt_ctx_t *ctx)
{
    (void)baji_photo_mqtt_client_cleanup(ctx);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_DISCONNECTED, NULL);
    baji_photo_mqtt_set_runtime_state(ctx, BAJI_PHOTO_MQTT_STATE_STOPPED);
    ctx->running = false;
    ctx->stop_requested = false;
    ctx->task = NULL;
    liot_rtos_task_delete(NULL);
}

static int baji_photo_mqtt_init_once(baji_photo_mqtt_ctx_t *ctx)
{
    LiotOSStatus_t ret;

    if (ctx == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    if (ctx->conn_sem == NULL) {
        ret = liot_rtos_semaphore_create(&ctx->conn_sem, 0u);
        if (ret != 0) {
            return ret;
        }
    }
    if (ctx->req_sem == NULL) {
        ret = liot_rtos_semaphore_create(&ctx->req_sem, 0u);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

static void baji_photo_mqtt_clear_current_task_runtime(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(&ctx->current_task, 0, sizeof(ctx->current_task));
    memset(&ctx->current_task_state, 0, sizeof(ctx->current_task_state));
    memset(&ctx->display_result, 0, sizeof(ctx->display_result));
    ctx->has_current_task = false;
    ctx->current_task_waiting_sync = false;
    ctx->display_result_pending = false;
}

static void baji_photo_mqtt_set_runtime_state(baji_photo_mqtt_ctx_t *ctx,
                                              baji_photo_mqtt_state_t state)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->state = state;
    liot_rtos_exit_critical();
}

static void baji_photo_mqtt_mark_connected(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->connected = true;
    ctx->state = BAJI_PHOTO_MQTT_STATE_CONNECTED;
    liot_rtos_exit_critical();
}

static void baji_photo_mqtt_mark_subscribed(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->connected = true;
    ctx->subscribed = true;
    ctx->state = BAJI_PHOTO_MQTT_STATE_SUBSCRIBED;
    liot_rtos_exit_critical();
}

static void baji_photo_mqtt_mark_disconnected(baji_photo_mqtt_ctx_t *ctx,
                                              baji_photo_mqtt_state_t state)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->connected = false;
    ctx->subscribed = false;
    ctx->state = state;
    liot_rtos_exit_critical();
}

static void baji_photo_mqtt_set_fatal_error(baji_photo_mqtt_ctx_t *ctx, int fatal_error)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->fatal_error = fatal_error;
    liot_rtos_exit_critical();
}

static baji_photo_mqtt_runtime_snapshot_t baji_photo_mqtt_get_runtime_snapshot(
    const baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    if (ctx == NULL) {
        return snapshot;
    }

    liot_rtos_enter_critical();
    snapshot.task = ctx->task;
    snapshot.state = ctx->state;
    snapshot.bind_status = ctx->bind_status;
    snapshot.running = ctx->running;
    snapshot.connected = ctx->connected;
    snapshot.subscribed = ctx->subscribed;
    snapshot.has_current_task = ctx->has_current_task;
    snapshot.current_task_waiting_sync = ctx->current_task_waiting_sync;
    snapshot.current_task_display_pending = ctx->current_task_state.display_pending;
    snapshot.current_task_displayed = ctx->current_task_state.displayed;
    snapshot.has_pending_reply = ctx->has_pending_reply;
    snapshot.display_result_pending = ctx->display_result_pending;
    snapshot.pending_reply_waiting_ack = ctx->pending_reply_waiting_ack;
    snapshot.heartbeat_countdown_s = ctx->heartbeat_countdown_s;
    snapshot.fatal_error = ctx->fatal_error;
    liot_rtos_exit_critical();
    return snapshot;
}

static baji_photo_mqtt_loop_view_t baji_photo_mqtt_get_loop_view(const baji_photo_mqtt_ctx_t *ctx,
                                                                 int mqtt_state)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot = baji_photo_mqtt_get_runtime_snapshot(ctx);
    baji_photo_mqtt_loop_view_t view;

    memset(&view, 0, sizeof(view));
    view.session_ready = snapshot.connected && (mqtt_state == MQTT_CONN_CONNECTED);
    view.needs_subscribe = view.session_ready && !snapshot.subscribed;
    view.has_display_result = snapshot.has_current_task && snapshot.display_result_pending;
    view.has_pending_reply = snapshot.has_pending_reply;
    view.waiting_display_ack = snapshot.has_current_task && snapshot.current_task_display_pending;
    view.has_current_task = snapshot.has_current_task;
    view.heartbeat_due = (snapshot.heartbeat_countdown_s == 0u);
    return view;
}

static bool baji_photo_mqtt_has_pending_reply_work(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    if (ctx->has_pending_reply) {
        return true;
    }
    return (baji_photo_mqtt_load_pending_reply(ctx) == 0) && ctx->has_pending_reply;
}

static void baji_photo_mqtt_begin_connect_attempt(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_mark_disconnected(ctx, BAJI_PHOTO_MQTT_STATE_CONNECTING);
}

static void baji_photo_mqtt_finish_connect_success(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    baji_photo_mqtt_set_runtime_state(ctx, BAJI_PHOTO_MQTT_STATE_CONNECTED);
    ctx->reconnect_backoff_s = BAJI_PHOTO_MQTT_RECONNECT_MIN_S;
    baji_photo_mqtt_request_fast_heartbeat(ctx);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_CONNECTED, NULL);
}

static void baji_photo_mqtt_wait_reconnect_backoff(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    liot_rtos_task_sleep_s(ctx->reconnect_backoff_s);
    if (ctx->reconnect_backoff_s < BAJI_PHOTO_MQTT_RECONNECT_MAX_S) {
        ctx->reconnect_backoff_s <<= 1;
        if (ctx->reconnect_backoff_s > BAJI_PHOTO_MQTT_RECONNECT_MAX_S) {
            ctx->reconnect_backoff_s = BAJI_PHOTO_MQTT_RECONNECT_MAX_S;
        }
    }
}

static void baji_photo_mqtt_schedule_next_heartbeat(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->heartbeat_countdown_s = ctx->heartbeat_interval_s;
}

static void baji_photo_mqtt_tick_heartbeat_countdown(baji_photo_mqtt_ctx_t *ctx)
{
    if ((ctx == NULL) || (ctx->heartbeat_countdown_s == 0u)) {
        return;
    }

    ctx->heartbeat_countdown_s -= 1u;
}

static void baji_photo_mqtt_process_idle_heartbeat(baji_photo_mqtt_ctx_t *ctx)
{
    int ret;

    if (ctx == NULL) {
        return;
    }

    ret = baji_photo_mqtt_publish_heartbeat(ctx);
    if (ret == 0) {
        baji_photo_mqtt_schedule_next_heartbeat(ctx);
    } else {
        BAJI_PHOTO_MQTT_TRACE("heartbeat publish fail ret=%d", ret);
    }
}

static int baji_photo_mqtt_process_post_connect_setup(baji_photo_mqtt_ctx_t *ctx)
{
    int ret;

    if (ctx == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    ret = baji_photo_mqtt_subscribe_all(ctx);
    if (ret == 0) {
        BAJI_PHOTO_MQTT_TRACE("post-connect online publish start");
        ret = baji_photo_mqtt_publish_online(ctx);
        BAJI_PHOTO_MQTT_TRACE("post-connect online publish done ret=%d", ret);
    }
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("post-connect setup fail ret=%d", ret);
        baji_photo_mqtt_mark_disconnected(ctx, BAJI_PHOTO_MQTT_STATE_BACKOFF);
    } else {
        baji_photo_mqtt_request_fast_heartbeat(ctx);
    }
    return ret;
}

static void baji_photo_mqtt_activate_current_task(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_image_task_t *task,
                                                  const baji_photo_task_state_t *state,
                                                  bool refresh_trace_id)
{
    if ((ctx == NULL) || (task == NULL)) {
        return;
    }

    ctx->current_task = *task;
    if (state != NULL) {
        ctx->current_task_state = *state;
    } else {
        memset(&ctx->current_task_state, 0, sizeof(ctx->current_task_state));
    }
    memset(&ctx->display_result, 0, sizeof(ctx->display_result));
    ctx->has_current_task = true;
    ctx->current_task_waiting_sync = false;
    ctx->display_result_pending = false;
    if (refresh_trace_id || (ctx->current_task.trace_id == 0u)) {
        ctx->current_task.trace_id = baji_photo_diag_next_id();
    }
}

static void baji_photo_mqtt_reset_runtime(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx->imei, 0, sizeof(ctx->imei));
    memset(ctx->real_imei, 0, sizeof(ctx->real_imei));
    memset(ctx->client_id, 0, sizeof(ctx->client_id));
    memset(ctx->topic_down, 0, sizeof(ctx->topic_down));
    memset(ctx->topic_cmd, 0, sizeof(ctx->topic_cmd));
    memset(ctx->topic_status, 0, sizeof(ctx->topic_status));
    memset(ctx->topic_event, 0, sizeof(ctx->topic_event));
    memset(ctx->topic_reply, 0, sizeof(ctx->topic_reply));
    memset(ctx->topic_up, 0, sizeof(ctx->topic_up));
    memset(ctx->topic_bind, 0, sizeof(ctx->topic_bind));
    memset(ctx->unbind_request_id, 0, sizeof(ctx->unbind_request_id));
    memset(ctx->rx_topic, 0, sizeof(ctx->rx_topic));
    memset(ctx->rx_payload, 0, sizeof(ctx->rx_payload));

    ctx->task = NULL;
    ctx->bind_status = BAJI_PHOTO_BIND_UNKNOWN;
    ctx->client = 0;
    ctx->running = false;
    ctx->stop_requested = false;
    ctx->client_inited = false;
    ctx->heartbeat_interval_s = BAJI_PHOTO_MQTT_HEARTBEAT_DEFAULT_S;
    ctx->heartbeat_countdown_s = 0u;
    ctx->bind_token_refresh_countdown_s = 0u;
    ctx->reconnect_backoff_s = BAJI_PHOTO_MQTT_RECONNECT_MIN_S;
    ctx->image_max_size = 0u;
    ctx->msg_seq = 0u;
    ctx->msg_pending = false;
    ctx->has_pending_reply = false;
    ctx->pending_reply_waiting_ack = false;
    ctx->pending_reply_ack_pending = false;
    ctx->pending_reply_store_known_empty = false;
    ctx->pending_reply_ack_err = 0;
    ctx->pending_reply_ack_generation = 0u;
    ctx->pending_reply_publish_at_s = 0u;

    memset(ctx->last_task_id, 0, sizeof(ctx->last_task_id));
    memset(&ctx->device_meta, 0, sizeof(ctx->device_meta));
    memset(&ctx->bind_token_info, 0, sizeof(ctx->bind_token_info));
    memset(&ctx->pending_reply, 0, sizeof(ctx->pending_reply));
    baji_photo_mqtt_clear_current_task_runtime(ctx);
    baji_photo_mqtt_mark_disconnected(ctx, BAJI_PHOTO_MQTT_STATE_IDLE);
    baji_photo_mqtt_set_fatal_error(ctx, 0);
}

static uint32_t baji_photo_mqtt_next_msg_id(baji_photo_mqtt_ctx_t *ctx)
{
    ctx->msg_seq += 1u;
    if (ctx->msg_seq == 0u) {
        ctx->msg_seq = 1u;
    }
    return ctx->msg_seq;
}

static int baji_photo_mqtt_wait_req(baji_photo_mqtt_ctx_t *ctx, uint32_t timeout_ms)
{
    if (liot_rtos_semaphore_wait(ctx->req_sem, timeout_ms) != 0) {
        return LIOT_MQTTCLIENT_TIMEOUT;
    }
    return LIOT_MQTTCLIENT_SUCCESS;
}

static bool baji_photo_mqtt_json_copy_string(const cJSON *obj,
                                             const char *key,
                                             char *out,
                                             unsigned int out_len)
{
    const cJSON *item;
    size_t len;

    if ((obj == NULL) || (key == NULL) || (out == NULL) || (out_len == 0u)) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (!cJSON_IsString(item) || (item->valuestring == NULL)) {
        return false;
    }

    len = strlen(item->valuestring);
    if (len >= out_len) {
        return false;
    }

    memcpy(out, item->valuestring, len + 1u);
    return true;
}

static bool baji_photo_mqtt_json_get_u32(const cJSON *obj,
                                         const char *key,
                                         uint32_t *out)
{
    const cJSON *item;

    if ((obj == NULL) || (key == NULL) || (out == NULL)) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (!cJSON_IsNumber(item) || (item->valuedouble < 0.0)) {
        return false;
    }

    *out = (uint32_t)item->valuedouble;
    return true;
}

static bool baji_photo_mqtt_json_get_bool(const cJSON *obj,
                                          const char *key,
                                          bool *out)
{
    const cJSON *item;

    if ((obj == NULL) || (key == NULL) || (out == NULL)) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (!cJSON_IsBool(item)) {
        return false;
    }

    *out = cJSON_IsTrue(item) ? true : false;
    return true;
}

static bool baji_photo_mqtt_parse_image_task(const cJSON *root,
                                             baji_photo_image_task_t *task)
{
    const cJSON *data;
    uint32_t image_width = 0u;
    uint32_t image_height = 0u;

    if ((root == NULL) || (task == NULL)) {
        return false;
    }

    memset(task, 0, sizeof(*task));
    data = cJSON_GetObjectItemCaseSensitive((cJSON *)root, "data");
    if (!cJSON_IsObject(data)) {
        return false;
    }

    if (!baji_photo_mqtt_json_copy_string(root, "msg_id", task->msg_id, sizeof(task->msg_id)) ||
        !baji_photo_mqtt_json_copy_string(data, "task_id", task->task_id, sizeof(task->task_id)) ||
        !baji_photo_mqtt_json_copy_string(data, "image_id", task->image_id, sizeof(task->image_id)) ||
        !baji_photo_mqtt_json_copy_string(data, "image_url", task->image_url, sizeof(task->image_url)) ||
        !baji_photo_mqtt_json_copy_string(data, "md5", task->md5, sizeof(task->md5)) ||
        !baji_photo_mqtt_json_copy_string(data, "image_format", task->image_format, sizeof(task->image_format))) {
        return false;
    }

    if (!baji_photo_mqtt_json_get_u32(data, "image_size", &task->image_size) ||
        !baji_photo_mqtt_json_get_u32(data, "chunk_size", &task->chunk_size) ||
        !baji_photo_mqtt_json_get_u32(data, "expire_seconds", &task->expire_seconds) ||
        !baji_photo_mqtt_json_get_u32(data, "image_width", &image_width) ||
        !baji_photo_mqtt_json_get_u32(data, "image_height", &image_height) ||
        !baji_photo_mqtt_json_get_bool(data, "support_range", &task->support_range)) {
        return false;
    }

    task->image_width = (uint16_t)image_width;
    task->image_height = (uint16_t)image_height;
    (void)baji_photo_mqtt_json_copy_string(data,
                                           "display_mode",
                                           task->display_mode,
                                           sizeof(task->display_mode));
    return true;
}

static int baji_photo_mqtt_validate_image_task(const baji_photo_mqtt_ctx_t *ctx,
                                               const baji_photo_image_task_t *task,
                                               const char **out_reason)
{
    baji_photo_format_t format;
    uint32_t local_max_size;

    baji_photo_mqtt_set_validate_reason(out_reason, "validate_failed");
    if ((ctx == NULL) || (task == NULL)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "validate_invalid_param");
        return 1001;
    }
    if (ctx->bind_status != BAJI_PHOTO_BIND_BOUND) {
        baji_photo_mqtt_set_validate_reason(out_reason, "device_not_bound");
        return 1009;
    }
    if (!baji_photo_mqtt_parse_task_format(task->image_format, &format)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "image_format_not_supported");
        return 1002;
    }
    if (!task->support_range) {
        baji_photo_mqtt_set_validate_reason(out_reason, "range_not_supported");
        return 1012;
    }
    if ((task->chunk_size == 0u) || (task->chunk_size > BAJI_PHOTO_RANGE_CHUNK_MAX)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "chunk_size_invalid");
        return 1012;
    }
    if ((task->image_width == 0u) || (task->image_height == 0u) ||
        (task->image_width > BAJI_PHOTO_IMG_W) || (task->image_height > BAJI_PHOTO_IMG_H)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "image_dims_invalid");
        return 1007;
    }
    if (format == BAJI_PHOTO_FORMAT_BJP) {
        if (task->image_size !=
            (task->image_width * task->image_height * BAJI_PHOTO_IMG_BPP)) {
            baji_photo_mqtt_set_validate_reason(out_reason, "image_size_mismatch");
            return 1007;
        }
    } else if (task->image_size == 0u) {
        baji_photo_mqtt_set_validate_reason(out_reason, "image_size_zero");
        return 1007;
    }
    local_max_size = baji_photo_mqtt_task_local_max_size(format);
    if ((local_max_size != 0u) && (task->image_size > local_max_size)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "local_size_limit");
        return 1007;
    }
    if ((ctx->image_max_size != 0u) && (task->image_size > ctx->image_max_size)) {
        baji_photo_mqtt_set_validate_reason(out_reason, "server_size_limit");
        return 1007;
    }
    return 0;
}

static int baji_photo_mqtt_prepare_task_state(const baji_photo_image_task_t *task,
                                              baji_photo_task_state_t *state)
{
    baji_photo_format_t format;
    char normalized_format[sizeof(state->image_format)];

    if ((task == NULL) || (state == NULL)) {
        return -1;
    }
    if (!baji_photo_mqtt_normalize_task_format(task->image_format,
                                               normalized_format,
                                               sizeof(normalized_format),
                                               &format)) {
        return -1;
    }

    memset(state, 0, sizeof(*state));
    snprintf(state->msg_id, sizeof(state->msg_id), "%s", task->msg_id);
    snprintf(state->task_id, sizeof(state->task_id), "%s", task->task_id);
    snprintf(state->image_id, sizeof(state->image_id), "%s", task->image_id);
    snprintf(state->image_url, sizeof(state->image_url), "%s", task->image_url);
    snprintf(state->md5, sizeof(state->md5), "%s", task->md5);
    snprintf(state->image_format, sizeof(state->image_format), "%s", normalized_format);
    snprintf(state->display_mode, sizeof(state->display_mode), "%s", task->display_mode);
    state->image_size = task->image_size;
    state->downloaded_size = 0u;
    state->chunk_size = task->chunk_size;
    state->expire_seconds = task->expire_seconds;
    state->image_width = task->image_width;
    state->image_height = task->image_height;
    state->retry_count = 0u;
    state->support_range = task->support_range;
    state->accepted_sent = false;
    state->completed = false;
    state->verified = false;
    baji_photo_mqtt_clear_network_wait_state(state);

    if ((baji_photo_store_build_tmp_path(task->image_id,
                                         state->tmp_path,
                                         sizeof(state->tmp_path)) != 0) ||
        (baji_photo_store_build_item_path(task->image_id,
                                          format,
                                          state->final_path,
                                          sizeof(state->final_path)) != 0)) {
        return -1;
    }
    return 0;
}

static bool baji_photo_mqtt_task_state_matches(const baji_photo_task_state_t *state,
                                               const baji_photo_image_task_t *task)
{
    return (state != NULL) &&
           (task != NULL) &&
           (strcmp(state->msg_id, task->msg_id) == 0) &&
           (strcmp(state->task_id, task->task_id) == 0) &&
           (strcmp(state->image_id, task->image_id) == 0) &&
           (strcmp(state->image_url, task->image_url) == 0) &&
           (strcmp(state->md5, task->md5) == 0) &&
           baji_photo_mqtt_task_formats_match(state->image_format, task->image_format) &&
           (strcmp(state->display_mode, task->display_mode) == 0) &&
           (state->image_size == task->image_size) &&
           (state->chunk_size == task->chunk_size) &&
           (state->expire_seconds == task->expire_seconds) &&
           (state->image_width == task->image_width) &&
           (state->image_height == task->image_height) &&
           (state->support_range == task->support_range) &&
           (state->tmp_path[0] != '\0') &&
           (state->final_path[0] != '\0');
}

static bool baji_photo_mqtt_task_state_is_resumable(const baji_photo_task_state_t *state)
{
    return (state != NULL) &&
           (state->msg_id[0] != '\0') &&
           (state->task_id[0] != '\0') &&
           (state->image_id[0] != '\0') &&
           (state->image_url[0] != '\0') &&
           (state->md5[0] != '\0') &&
           (state->image_format[0] != '\0') &&
           (state->tmp_path[0] != '\0') &&
           (state->final_path[0] != '\0') &&
           (state->image_size != 0u) &&
           (state->downloaded_size <= state->image_size) &&
           (state->chunk_size != 0u) &&
           (state->image_width != 0u) &&
           (state->image_height != 0u) &&
           state->support_range &&
           !state->verified;
}

static int baji_photo_mqtt_copy_task_from_state(const baji_photo_task_state_t *state,
                                                baji_photo_image_task_t *task)
{
    if ((state == NULL) || (task == NULL) ||
        (state->msg_id[0] == '\0') ||
        (state->task_id[0] == '\0') ||
        (state->image_id[0] == '\0') ||
        (state->image_url[0] == '\0') ||
        (state->md5[0] == '\0') ||
        (state->image_format[0] == '\0') ||
        (state->image_size == 0u) ||
        (state->chunk_size == 0u) ||
        (state->image_width == 0u) ||
        (state->image_height == 0u)) {
        return -1;
    }

    memset(task, 0, sizeof(*task));
    snprintf(task->msg_id, sizeof(task->msg_id), "%s", state->msg_id);
    snprintf(task->task_id, sizeof(task->task_id), "%s", state->task_id);
    snprintf(task->image_id, sizeof(task->image_id), "%s", state->image_id);
    snprintf(task->image_url, sizeof(task->image_url), "%s", state->image_url);
    snprintf(task->md5, sizeof(task->md5), "%s", state->md5);
    if (!baji_photo_mqtt_normalize_task_format(state->image_format,
                                               task->image_format,
                                               sizeof(task->image_format),
                                               NULL)) {
        return -1;
    }
    snprintf(task->display_mode, sizeof(task->display_mode), "%s", state->display_mode);
    task->image_size = state->image_size;
    task->image_width = state->image_width;
    task->image_height = state->image_height;
    task->chunk_size = state->chunk_size;
    task->expire_seconds = state->expire_seconds;
    task->support_range = state->support_range;
    return 0;
}

static int baji_photo_mqtt_restore_task_from_state(const baji_photo_task_state_t *state,
                                                   baji_photo_image_task_t *task)
{
    if (!baji_photo_mqtt_task_state_is_resumable(state)) {
        return -1;
    }
    return baji_photo_mqtt_copy_task_from_state(state, task);
}

static int baji_photo_mqtt_prepare_or_resume_task_state(const baji_photo_image_task_t *task,
                                                        baji_photo_task_state_t *state)
{
    baji_photo_task_state_t saved_state;

    if ((task == NULL) || (state == NULL)) {
        return -1;
    }

    memset(&saved_state, 0, sizeof(saved_state));
    if ((baji_photo_store_load_task_state(&saved_state) == 0) &&
        baji_photo_mqtt_task_state_matches(&saved_state, task) &&
        baji_photo_mqtt_task_state_is_resumable(&saved_state)) {
        *state = saved_state;
        (void)baji_photo_mqtt_normalize_task_format(state->image_format,
                                                    state->image_format,
                                                    sizeof(state->image_format),
                                                    NULL);
        return 1;
    }

    return baji_photo_mqtt_prepare_task_state(task, state);
}

static int baji_photo_mqtt_store_stat(const char *path, liot_stat_ext_s *out_st)
{
    int ret;

    if ((path == NULL) || (out_st == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_stat_ext(path, out_st);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_mqtt_store_remove(const char *path)
{
    int ret;

    if (path == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_remove_ext(path);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_mqtt_store_rename(const char *from_path, const char *to_path)
{
    int ret;

    if ((from_path == NULL) || (to_path == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_rename_ext(from_path, to_path);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_mqtt_store_exists(const char *path)
{
    int ret;

    if (path == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_file_exist_ext(path);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_mqtt_crc32_file(const char *path,
                                      uint32_t expected_size,
                                      uint32_t *out_crc)
{
    uint8_t buf[1024];
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t remain_size = expected_size;
    LFILE_EXT fd = 0;
    int ret;

    if ((path == NULL) || (out_crc == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    fd = liot_fopen_ext(path, "r");
    if (fd <= 0) {
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_OPEN_FAIL;
    }

    while (remain_size > 0u) {
        unsigned int read_size = sizeof(buf);
        int read_len;

        if (read_size > remain_size) {
            read_size = remain_size;
        }
        read_len = liot_fread_ext(buf, read_size, 1, fd);
        if (read_len != (int)read_size) {
            (void)liot_fclose_ext(fd);
            baji_photo_store_access_end();
            return LIOT_EXTFLASH_READ_FAIL;
        }
        crc = baji_photo_store_crc32_update(crc, buf, read_size);
        remain_size -= read_size;
    }

    *out_crc = baji_photo_store_crc32_finish(crc);
    (void)liot_fclose_ext(fd);
    baji_photo_store_access_end();
    return 0;
}

static void baji_photo_mqtt_trace_store_state(const char *stage,
                                              const baji_photo_task_state_t *state,
                                              const char *path,
                                              int stat_ret,
                                              const liot_stat_ext_s *st)
{
    BAJI_PHOTO_MQTT_TRACE("commit %s task=%s image=%s path=%s stat=%d type=%u size=%lu",
                          (stage != NULL) ? stage : "unknown",
                          ((state != NULL) && (state->task_id[0] != '\0')) ? state->task_id : "-",
                          ((state != NULL) && (state->image_id[0] != '\0')) ? state->image_id : "-",
                          (path != NULL) ? path : "(null)",
                          stat_ret,
                          (unsigned int)((st != NULL) ? st->type : 0u),
                          (unsigned long)((st != NULL) ? st->size : 0u));
}

static int baji_photo_mqtt_finalize_verified_image_file(const baji_photo_task_state_t *state)
{
    baji_photo_format_t format;
    liot_stat_ext_s tmp_st;
    liot_stat_ext_s final_st;
    uint32_t expected_size;
    int tmp_stat_ret;
    int final_exist_ret;
    int final_stat_ret;
    int remove_ret;
    int rename_ret;
    int final_exist_after_ret;
    int tmp_exist_after_ret;
    int tmp_after_ret;

    if ((state == NULL) || (state->final_path[0] == '\0')) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (!baji_photo_mqtt_parse_task_format(state->image_format, &format)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    expected_size = state->downloaded_size;
    if (format == BAJI_PHOTO_FORMAT_BJP) {
        expected_size += (uint32_t)sizeof(baji_photo_file_header_t);
    }

    memset(&final_st, 0, sizeof(final_st));
    final_exist_ret = baji_photo_mqtt_store_exists(state->final_path);
    final_stat_ret = baji_photo_mqtt_store_stat(state->final_path, &final_st);
    baji_photo_mqtt_trace_store_state("final-before", state, state->final_path, final_stat_ret, &final_st);
    BAJI_PHOTO_MQTT_TRACE("commit final exists task=%s image=%s path=%s exist=%d",
                          state->task_id,
                          state->image_id,
                          state->final_path,
                          final_exist_ret);
    if ((final_exist_ret == LIOT_EXTFLASH_OK) &&
        (final_stat_ret == LIOT_EXTFLASH_OK) &&
        (final_st.type == LIOT_EXTFLASH_TYPE_FILE) &&
        (final_st.size == expected_size)) {
        BAJI_PHOTO_MQTT_TRACE("commit final ready task=%s image=%s expect=%lu action=keep",
                              state->task_id,
                              state->image_id,
                              (unsigned long)expected_size);
        return 0;
    }

    if (state->tmp_path[0] == '\0') {
        BAJI_PHOTO_MQTT_TRACE("commit tmp missing task=%s image=%s expect=%lu reason=empty_path",
                              state->task_id,
                              state->image_id,
                              (unsigned long)expected_size);
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    memset(&tmp_st, 0, sizeof(tmp_st));
    tmp_stat_ret = baji_photo_mqtt_store_stat(state->tmp_path, &tmp_st);
    baji_photo_mqtt_trace_store_state("tmp-before", state, state->tmp_path, tmp_stat_ret, &tmp_st);
    if ((tmp_stat_ret != LIOT_EXTFLASH_OK) ||
        (tmp_st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (tmp_st.size != expected_size)) {
        BAJI_PHOTO_MQTT_TRACE("commit tmp invalid task=%s image=%s expect=%lu stat=%d type=%u size=%lu",
                              state->task_id,
                              state->image_id,
                              (unsigned long)expected_size,
                              tmp_stat_ret,
                              (unsigned int)tmp_st.type,
                              (unsigned long)tmp_st.size);
        if (tmp_stat_ret != LIOT_EXTFLASH_OK) {
            return tmp_stat_ret;
        }
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    if (final_exist_ret == LIOT_EXTFLASH_NOT_EXIST) {
        BAJI_PHOTO_MQTT_TRACE("commit remove skip task=%s image=%s path=%s reason=not_exist",
                              state->task_id,
                              state->image_id,
                              state->final_path);
    } else {
        remove_ret = baji_photo_mqtt_store_remove(state->final_path);
        BAJI_PHOTO_MQTT_TRACE("commit remove task=%s image=%s path=%s ret=%d",
                              state->task_id,
                              state->image_id,
                              state->final_path,
                              remove_ret);
        final_exist_ret = baji_photo_mqtt_store_exists(state->final_path);
        memset(&final_st, 0, sizeof(final_st));
        final_stat_ret = baji_photo_mqtt_store_stat(state->final_path, &final_st);
        baji_photo_mqtt_trace_store_state("final-after-remove",
                                          state,
                                          state->final_path,
                                          final_stat_ret,
                                          &final_st);
        BAJI_PHOTO_MQTT_TRACE("commit final exists after remove task=%s image=%s path=%s exist=%d",
                              state->task_id,
                              state->image_id,
                              state->final_path,
                              final_exist_ret);
        if ((remove_ret != LIOT_EXTFLASH_OK) && (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST)) {
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
        if (final_exist_ret == LIOT_EXTFLASH_OK) {
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
    }

    rename_ret = baji_photo_mqtt_store_rename(state->tmp_path, state->final_path);
    BAJI_PHOTO_MQTT_TRACE("commit rename task=%s image=%s from=%s to=%s ret=%d",
                          state->task_id,
                          state->image_id,
                          state->tmp_path,
                          state->final_path,
                          rename_ret);
    final_exist_after_ret = baji_photo_mqtt_store_exists(state->final_path);
    memset(&final_st, 0, sizeof(final_st));
    final_stat_ret = baji_photo_mqtt_store_stat(state->final_path, &final_st);
    baji_photo_mqtt_trace_store_state("final-after-rename",
                                      state,
                                      state->final_path,
                                      final_stat_ret,
                                      &final_st);
    BAJI_PHOTO_MQTT_TRACE("commit final exists after rename task=%s image=%s path=%s exist=%d",
                          state->task_id,
                          state->image_id,
                          state->final_path,
                          final_exist_after_ret);
    tmp_exist_after_ret = baji_photo_mqtt_store_exists(state->tmp_path);
    memset(&tmp_st, 0, sizeof(tmp_st));
    tmp_after_ret = baji_photo_mqtt_store_stat(state->tmp_path, &tmp_st);
    baji_photo_mqtt_trace_store_state("tmp-after-rename",
                                      state,
                                      state->tmp_path,
                                      tmp_after_ret,
                                      &tmp_st);
    BAJI_PHOTO_MQTT_TRACE("commit tmp exists after rename task=%s image=%s path=%s exist=%d",
                          state->task_id,
                          state->image_id,
                          state->tmp_path,
                          tmp_exist_after_ret);

    if (rename_ret == LIOT_EXTFLASH_OK) {
        return 0;
    }
    if ((final_exist_after_ret == LIOT_EXTFLASH_OK) &&
        (final_stat_ret == LIOT_EXTFLASH_OK) &&
        (final_st.type == LIOT_EXTFLASH_TYPE_FILE) &&
        (final_st.size == expected_size) &&
        (tmp_exist_after_ret == LIOT_EXTFLASH_NOT_EXIST)) {
        BAJI_PHOTO_MQTT_TRACE("commit rename tolerate task=%s image=%s expect=%lu",
                              state->task_id,
                              state->image_id,
                              (unsigned long)expected_size);
        return 0;
    }
    return LIOT_EXTFLASH_RENAME_FAIL;
}

static int baji_photo_mqtt_build_item_from_state(const baji_photo_task_state_t *state,
                                                 baji_photo_manifest_item_t *out_item)
{
    baji_photo_format_t format;
    baji_photo_file_header_t header;
    uint32_t crc32 = 0u;
    size_t url_len;
    int ret;

    if ((state == NULL) || (out_item == NULL)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (!baji_photo_mqtt_parse_task_format(state->image_format, &format)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    if (format == BAJI_PHOTO_FORMAT_BJP) {
        ret = baji_photo_store_read_photo_header(state->image_id, &header);
        if (ret != 0) {
            return ret;
        }
        if ((header.magic != BAJI_PHOTO_MAGIC) ||
            (header.cf != LV_IMG_CF_TRUE_COLOR) ||
            (header.data_size != state->image_size) ||
            (header.width != state->image_width) ||
            (header.height != state->image_height)) {
            return LIOT_EXTFLASH_READ_FAIL;
        }
        crc32 = header.crc32;
    } else {
        ret = baji_photo_mqtt_crc32_file(state->final_path, state->image_size, &crc32);
        if (ret != 0) {
            return ret;
        }
    }

    memset(out_item, 0, sizeof(*out_item));
    (void)snprintf(out_item->id, sizeof(out_item->id), "%s", state->image_id);
    (void)snprintf(out_item->name, sizeof(out_item->name), "%s", state->image_id);
    url_len = strlen(state->image_url);
    if (url_len < sizeof(out_item->remote_path)) {
        memcpy(out_item->remote_path, state->image_url, url_len + 1u);
    } else {
        (void)snprintf(out_item->remote_path,
                       sizeof(out_item->remote_path),
                       "mqtt/%s",
                       state->image_id);
    }
    if (baji_photo_store_build_item_path(state->image_id,
                                         format,
                                         out_item->local_path,
                                         sizeof(out_item->local_path)) != 0) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    out_item->file_size = state->image_size;
    if (format == BAJI_PHOTO_FORMAT_BJP) {
        out_item->file_size += (uint32_t)sizeof(baji_photo_file_header_t);
    }
    out_item->crc32 = crc32;
    out_item->width = state->image_width;
    out_item->height = state->image_height;
    out_item->cf = (format == BAJI_PHOTO_FORMAT_BJP) ? LV_IMG_CF_TRUE_COLOR : 0u;
    out_item->format = format;
    return 0;
}

static void baji_photo_mqtt_clear_saved_task_state_for_reply(const baji_photo_mqtt_pending_reply_t *reply)
{
    baji_photo_task_state_t saved_state;
    int ret;

    if ((reply == NULL) || (reply->task_id[0] == '\0')) {
        return;
    }

    memset(&saved_state, 0, sizeof(saved_state));
    ret = baji_photo_store_load_task_state(&saved_state);
    if (ret != 0) {
        return;
    }
    if (strcmp(saved_state.task_id, reply->task_id) != 0) {
        return;
    }

    ret = baji_photo_store_clear_task_state();
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("clear saved task state fail ret=%d task=%s code=%d",
                              ret,
                              reply->task_id,
                              reply->code);
    }
}

static int baji_photo_mqtt_restore_display_pending_task_state(baji_photo_mqtt_ctx_t *ctx,
                                                              const baji_photo_task_state_t *state)
{
    baji_photo_image_task_t task;
    baji_photo_manifest_item_t item;
    int ret;

    if ((ctx == NULL) || (state == NULL) || !state->completed || !state->verified ||
        !state->display_pending || state->displayed) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_mqtt_copy_task_from_state(state, &task);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_finalize_verified_image_file(state);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_build_item_from_state(state, &item);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_upsert_downloaded_item(ctx, &item);
    if (ret != 0) {
        return ret;
    }

    baji_photo_mqtt_activate_current_task(ctx, &task, state, true);
    BAJI_PHOTO_MQTT_TRACE("op=%lu restore display pending task=%s image=%s downloaded=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          ctx->current_task.image_id,
                          (unsigned long)ctx->current_task_state.downloaded_size);
    baji_photo_mqtt_emit_display_request(ctx, NULL);
    return 0;
}

static int baji_photo_mqtt_recover_completed_task_state(baji_photo_mqtt_ctx_t *ctx,
                                                        const baji_photo_task_state_t *state)
{
    baji_photo_image_task_t task;
    baji_photo_manifest_item_t item;
    baji_photo_mqtt_pending_reply_t reply;
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int ret;

    if ((ctx == NULL) || (state == NULL) || !state->completed || !state->verified) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_mqtt_copy_task_from_state(state, &task);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_finalize_verified_image_file(state);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_build_item_from_state(state, &item);
    if (ret != 0) {
        return ret;
    }
    ret = baji_photo_mqtt_upsert_downloaded_item(ctx, &item);
    if (ret != 0) {
        return ret;
    }
    (void)baji_photo_mqtt_save_device_meta(ctx,
                                           state->task_id,
                                           state->image_id,
                                           state->md5,
                                           state->display_time_ms);

    baji_photo_mqtt_prepare_pending_reply(&task, 0, state->display_time_ms, NULL, &reply);
    baji_photo_mqtt_prepare_pending_reply_runtime(ctx, &reply);
    baji_photo_mqtt_track_pending_reply(ctx, &reply);
    ret = baji_photo_store_save_pending_reply(&reply);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("save recovered reply fail ret=%d task=%s",
                              ret,
                              state->task_id);
        return ret;
    }

    ret = baji_photo_store_clear_task_state();
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("clear recovered task state fail ret=%d task=%s",
                              ret,
                              state->task_id);
    }

    BAJI_PHOTO_MQTT_TRACE("recover completed task state task=%s image=%s displayed=%d display_ms=%s",
                          state->task_id,
                          state->image_id,
                          state->displayed ? 1 : 0,
                          baji_photo_diag_u64_dec(state->display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)));
    return 0;
}

static void baji_photo_mqtt_restore_saved_task_state(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_image_task_t task;
    baji_photo_task_state_t saved_state;
    int ret;

    if ((ctx == NULL) || ctx->has_current_task) {
        return;
    }

    memset(&saved_state, 0, sizeof(saved_state));
    ret = baji_photo_store_load_task_state(&saved_state);
    if (ret == LIOT_EXTFLASH_NOT_EXIST) {
        return;
    }
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("load task state fail ret=%d", ret);
        return;
    }

    if (ctx->has_pending_reply) {
        memset(&task, 0, sizeof(task));
        if ((strcmp(ctx->pending_reply.task_id, saved_state.task_id) == 0) &&
            (baji_photo_mqtt_copy_task_from_state(&saved_state, &task) == 0)) {
            baji_photo_mqtt_activate_current_task(ctx, &task, &saved_state, true);
            BAJI_PHOTO_MQTT_TRACE("op=%lu restore pending reply task=%s image=%s code=%d gen=%lu attempt=%lu",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->pending_reply.task_id,
                                  ctx->pending_reply.image_id,
                                  ctx->pending_reply.code,
                                  (unsigned long)ctx->pending_reply.generation,
                                  (unsigned long)ctx->pending_reply.attempt_count);
        } else {
            BAJI_PHOTO_MQTT_TRACE("pending reply without matching task state reply_task=%s state_task=%s",
                                  ctx->pending_reply.task_id,
                                  saved_state.task_id);
        }
        return;
    }

    if (saved_state.completed && saved_state.verified &&
        saved_state.display_pending && !saved_state.displayed) {
        ret = baji_photo_mqtt_restore_display_pending_task_state(ctx, &saved_state);
        if (ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("restore display pending task state fail ret=%d task=%s",
                                  ret,
                                  saved_state.task_id);
        }
        return;
    }

    if (saved_state.completed && saved_state.verified) {
        ret = baji_photo_mqtt_recover_completed_task_state(ctx, &saved_state);
        if (ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("recover completed task state fail ret=%d task=%s",
                                  ret,
                                  saved_state.task_id);
        }
        return;
    }

    memset(&task, 0, sizeof(task));
    if (baji_photo_mqtt_restore_task_from_state(&saved_state, &task) != 0) {
        BAJI_PHOTO_MQTT_TRACE("skip non-resumable task state task=%s completed=%d verified=%d",
                              saved_state.task_id,
                              saved_state.completed ? 1 : 0,
                              saved_state.verified ? 1 : 0);
        return;
    }

    baji_photo_mqtt_activate_current_task(ctx, &task, &saved_state, true);
    BAJI_PHOTO_MARK_TRACE("MQTT_TASK_RESTORE op=%lu task=%s image=%s downloaded=%lu accepted=%d heap_min=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          baji_photo_diag_id_tail(ctx->current_task.task_id),
                          baji_photo_diag_id_tail(ctx->current_task.image_id),
                          (unsigned long)ctx->current_task_state.downloaded_size,
                          ctx->current_task_state.accepted_sent ? 1 : 0,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    BAJI_PHOTO_MQTT_TRACE("op=%lu restore task state task=%s downloaded=%lu accepted=%d",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          (unsigned long)ctx->current_task_state.downloaded_size,
                          ctx->current_task_state.accepted_sent ? 1 : 0);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_IMAGE_TASK, &ctx->current_task);
}

static int baji_photo_mqtt_publish_raw(baji_photo_mqtt_ctx_t *ctx,
                                       const char *topic,
                                       const char *payload,
                                       unsigned short payload_len,
                                       liot_mqtt_request_cb_t cb,
                                       void *arg,
                                       bool wait_req)
{
    int ret;

    if ((ctx == NULL) || (topic == NULL) || (payload == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    if (wait_req) {
        baji_photo_mqtt_sem_drain(ctx->req_sem);
    }

    ret = liot_mqtt_publish(&ctx->client,
                            topic,
                            payload,
                            payload_len,
                            BAJI_PHOTO_MQTT_QOS,
                            0,
                            cb,
                            arg);
    if (!wait_req && (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)) {
        return 0;
    }
    if (wait_req && (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)) {
        ret = baji_photo_mqtt_wait_req(ctx, BAJI_PHOTO_MQTT_CONNECT_WAIT_MS);
    }
    return ret;
}

static int baji_photo_mqtt_publish(baji_photo_mqtt_ctx_t *ctx,
                                   const char *topic,
                                   const char *payload,
                                   unsigned short payload_len)
{
    return baji_photo_mqtt_publish_raw(ctx, topic, payload, payload_len, NULL, NULL, true);
}

#if BAJI_PHOTO_ENABLE_INDEX_READBACK_DIAG
static int baji_photo_mqtt_find_item_by_id(const baji_photo_manifest_item_t *items,
                                           unsigned int count,
                                           const char *id)
{
    unsigned int i;

    if ((items == NULL) || (id == NULL)) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        if (strcmp(items[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int baji_photo_mqtt_trace_index_readback(const baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_manifest_item_t *item,
                                                unsigned int saved_count)
{
    baji_photo_manifest_item_t *readback = NULL;
    unsigned int readback_count = 0u;
    int ret;
    int found = -1;
    size_t items_bytes;

    if (item == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    items_bytes = sizeof(*readback) * BAJI_PHOTO_DL_MAX;
    readback = (baji_photo_manifest_item_t *)liot_rtos_malloc(items_bytes);
    if (readback == NULL) {
        BAJI_PHOTO_MQTT_TRACE("index readback alloc fail image=%s bytes=%lu",
                              item->id,
                              (unsigned long)items_bytes);
        baji_photo_mqtt_marker_index_readback(ctx,
                                              "MQTT_INDEX_READBACK_ALLOC_FAIL",
                                              item,
                                              saved_count,
                                              0u,
                                              -1,
                                              LIOT_EXTFLASH_ERROR_GENERAL);
        return LIOT_EXTFLASH_ERROR_GENERAL;
    }

    memset(readback, 0, items_bytes);
    ret = baji_photo_store_load_index(readback, BAJI_PHOTO_DL_MAX, &readback_count);
    if (ret == 0) {
        found = baji_photo_mqtt_find_item_by_id(readback, readback_count, item->id);
    }
    BAJI_PHOTO_MQTT_TRACE("index readback image=%s saved=%u loaded=%u found=%d ret=%d",
                          item->id,
                          saved_count,
                          readback_count,
                          (found >= 0) ? 1 : 0,
                          ret);
    baji_photo_mqtt_marker_index_readback(ctx,
                                          "MQTT_INDEX_READBACK",
                                          item,
                                          saved_count,
                                          readback_count,
                                          found,
                                          ret);
    liot_rtos_free(readback);

    if (ret != 0) {
        return ret;
    }
    if ((readback_count != saved_count) || (found < 0)) {
        return BAJI_PHOTO_MQTT_INDEX_READBACK_FAIL;
    }
    return 0;
}
#endif

static int baji_photo_mqtt_upsert_downloaded_item(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_manifest_item_t *item)
{
    unsigned int count = 0u;
    int ret = 0;

    if (item == NULL) {
        return -1;
    }

    ret = baji_photo_store_upsert_item(item, &count);
#if BAJI_PHOTO_ENABLE_INDEX_READBACK_DIAG
    if (ret == 0) {
        ret = baji_photo_mqtt_trace_index_readback(ctx, item, count);
    }
#else
    (void)ctx;
#endif
    return ret;
}

static const char *baji_photo_mqtt_result_message(int code)
{
    switch (code) {
    case 0:
        return "success";
    case 1005:
        return "screen refresh failed";
    case 1003:
        return "image parse failed";
    case 1002:
        return "image format not supported";
    case 1004:
        return "storage no space";
    case 1006:
        return "image md5 check failed";
    case 1007:
        return "image size mismatch";
    case 1008:
        return "duplicate task";
    case 1009:
        return "device not bound";
    case 1010:
        return "image url expired";
    case 1011:
        return "range resume failed";
    case 1012:
        return "range request not supported";
    default:
        return "image download failed";
    }
}

static int baji_photo_mqtt_publish_image_accepted(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_image_task_t *task)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    int written;

    if ((ctx == NULL) || (task == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    written = snprintf(payload,
                       sizeof(payload),
                       "{\"msg_id\":\"reply_%lu\",\"reply_to\":\"%s\","
                       "\"type\":\"image.display.accepted\",\"imei\":\"%s\","
                       "\"timestamp\":0,\"version\":\"1.0\",\"code\":0,"
                       "\"message\":\"accepted\",\"data\":{\"task_id\":\"%s\"}}",
                       (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                       task->msg_id,
                       ctx->imei,
                       task->task_id);
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish(ctx, ctx->topic_reply, payload, (unsigned short)written);
}

static int baji_photo_mqtt_publish_bind_token_request(baji_photo_mqtt_ctx_t *ctx,
                                                      const char *reason)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    int written;

    if ((ctx == NULL) || (reason == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    written = snprintf(payload,
                       sizeof(payload),
                       "{\"msg_id\":\"bind_req_%lu\",\"type\":\"bind.token.request\","
                       "\"imei\":\"%s\",\"timestamp\":0,\"version\":\"1.0\","
                       "\"data\":{\"reason\":\"%s\"}}",
                       (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                       ctx->imei,
                       reason);
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish(ctx, ctx->topic_up, payload, (unsigned short)written);
}

static int baji_photo_mqtt_request_bind_token_internal(baji_photo_mqtt_ctx_t *ctx, const char *reason)
{
    int ret;

    if ((ctx == NULL) || (reason == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }
    if (!ctx->connected || !ctx->subscribed) {
        return LIOT_MQTTCLIENT_NOT_CONNECT;
    }

    ret = baji_photo_mqtt_publish_bind_token_request(ctx, reason);
    if (ret == 0) {
        BAJI_PHOTO_MQTT_TRACE("bind token request reason=%s", reason);
    }
    return ret;
}

static bool baji_photo_mqtt_parse_bind_token_info(const cJSON *root,
                                                  baji_photo_bind_token_info_t *info)
{
    const cJSON *data;

    if ((root == NULL) || (info == NULL)) {
        return false;
    }

    memset(info, 0, sizeof(*info));
    data = cJSON_GetObjectItemCaseSensitive((cJSON *)root, "data");
    if (!cJSON_IsObject(data)) {
        return false;
    }

    if (!baji_photo_mqtt_json_copy_string(data,
                                          "bind_token",
                                          info->bind_token,
                                          sizeof(info->bind_token)) ||
        !baji_photo_mqtt_json_copy_string(data,
                                          "bind_url",
                                          info->bind_url,
                                          sizeof(info->bind_url)) ||
        !baji_photo_mqtt_json_get_u32(data, "expire_seconds", &info->expire_seconds)) {
        return false;
    }
    return (info->expire_seconds > 0u);
}

static int baji_photo_mqtt_publish_current_task_accepted(baji_photo_mqtt_ctx_t *ctx, bool force_resend)
{
    int ret;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }
    if (!force_resend && ctx->current_task_state.accepted_sent) {
        return 0;
    }

    ret = baji_photo_mqtt_publish_image_accepted(ctx, &ctx->current_task);
    if (ret != 0) {
        return ret;
    }
    if (!ctx->current_task_state.accepted_sent) {
        ctx->current_task_state.accepted_sent = true;
#if BAJI_PHOTO_ENABLE_ACCEPTED_SENT_PERSIST
        ret = baji_photo_store_save_task_state(&ctx->current_task_state);
        if (ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("save accepted state fail ret=%d task=%s",
                                  ret,
                                  ctx->current_task.task_id);
        }
#else
        BAJI_PHOTO_MQTT_TRACE("accepted state persist disabled task=%s", ctx->current_task.task_id);
#endif
    }
    return 0;
}

static uint32_t baji_photo_mqtt_progress_percent(uint32_t downloaded, uint32_t total)
{
    uint64_t pct;

    if (total == 0u) {
        return 0u;
    }
    if (downloaded >= total) {
        return 100u;
    }

    pct = ((uint64_t)downloaded * 100u) / total;
    if (pct > 100u) {
        pct = 100u;
    }
    return (uint32_t)pct;
}

static uint32_t baji_photo_mqtt_progress_bucket(uint32_t progress)
{
    if (progress >= 100u) {
        return 100u;
    }
    return (progress / 10u) * 10u;
}

static void baji_photo_mqtt_emit_image_progress(baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_image_task_t *task,
                                                uint32_t downloaded,
                                                uint32_t total,
                                                uint32_t percent_bucket,
                                                bool range_resumed)
{
    baji_photo_mqtt_image_progress_t progress;

    if ((ctx == NULL) || (task == NULL) || (total == 0u)) {
        return;
    }

    memset(&progress, 0, sizeof(progress));
    (void)snprintf(progress.task_id, sizeof(progress.task_id), "%s", task->task_id);
    (void)snprintf(progress.image_id, sizeof(progress.image_id), "%s", task->image_id);
    progress.downloaded_size = (downloaded > total) ? total : downloaded;
    progress.total_size = total;
    progress.percent_bucket = (percent_bucket > 100u) ? 100u : percent_bucket;
    progress.range_resumed = range_resumed;
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_IMAGE_PROGRESS, &progress);
}

static int baji_photo_mqtt_publish_image_progress(baji_photo_mqtt_ctx_t *ctx,
                                                  const baji_photo_image_task_t *task,
                                                  uint32_t downloaded,
                                                  uint32_t total,
                                                  bool range_resumed)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    uint32_t progress;
    int written;

    if ((ctx == NULL) || (task == NULL) || (total == 0u)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    if (downloaded > total) {
        downloaded = total;
    }
    progress = baji_photo_mqtt_progress_percent(downloaded, total);
    written = snprintf(payload,
                       sizeof(payload),
                       "{\"msg_id\":\"progress_%lu\",\"type\":\"image.download.progress\","
                       "\"imei\":\"%s\",\"timestamp\":0,\"version\":\"1.0\","
                       "\"data\":{\"task_id\":\"%s\",\"image_id\":\"%s\","
                       "\"downloaded\":%lu,\"total\":%lu,\"progress\":%lu,"
                       "\"range_resume\":%s}}",
                       (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                       ctx->imei,
                       task->task_id,
                       task->image_id,
                       (unsigned long)downloaded,
                       (unsigned long)total,
                       (unsigned long)progress,
                       range_resumed ? "true" : "false");
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish(ctx, ctx->topic_event, payload, (unsigned short)written);
}

static void baji_photo_mqtt_progress_cb(const baji_photo_image_task_t *task,
                                        uint32_t downloaded_size,
                                        uint32_t total_size,
                                        bool range_resumed,
                                        void *ctx)
{
    baji_photo_mqtt_progress_ctx_t *progress_ctx = (baji_photo_mqtt_progress_ctx_t *)ctx;
    uint32_t progress;
    uint32_t progress_bucket;
    int ret;

    if ((progress_ctx == NULL) || (progress_ctx->mqtt == NULL) || (task == NULL) || (total_size == 0u)) {
        return;
    }

    progress = baji_photo_mqtt_progress_percent(downloaded_size, total_size);
    progress_bucket = baji_photo_mqtt_progress_bucket(progress);
    if (!progress_ctx->ui_progress_sent ||
        ((downloaded_size > 0u) && !progress_ctx->ui_nonzero_progress_sent) ||
        (progress_bucket > progress_ctx->last_ui_bucket)) {
        baji_photo_mqtt_emit_image_progress(progress_ctx->mqtt,
                                            task,
                                            downloaded_size,
                                            total_size,
                                            progress_bucket,
                                            range_resumed || progress_ctx->range_resumed);
        progress_ctx->last_ui_bucket = progress_bucket;
        progress_ctx->ui_progress_sent = true;
        if (downloaded_size > 0u) {
            progress_ctx->ui_nonzero_progress_sent = true;
        }
    }
    if (!progress_ctx->half_sent && (progress >= 50u)) {
        ret = baji_photo_mqtt_publish_image_progress(progress_ctx->mqtt,
                                                     task,
                                                     downloaded_size,
                                                     total_size,
                                                     range_resumed || progress_ctx->range_resumed);
        if (ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("progress publish fail ret=%d task=%s progress=%lu",
                                  ret,
                                  task->task_id,
                                  (unsigned long)progress);
        } else {
            progress_ctx->half_sent = true;
        }
    }
}

static int baji_photo_mqtt_publish_image_result_fields(baji_photo_mqtt_ctx_t *ctx,
                                                       const char *reply_to,
                                                       const char *task_id,
                                                       const char *image_id,
                                                       int code,
                                                       uint64_t timestamp_ms,
                                                       uint64_t display_time_ms,
                                                       const char *message,
                                                       const char *reason)
{
    return baji_photo_mqtt_publish_image_result_fields_ex(ctx,
                                                          reply_to,
                                                          task_id,
                                                          image_id,
                                                          code,
                                                          timestamp_ms,
                                                          display_time_ms,
                                                          message,
                                                          reason,
                                                          NULL,
                                                          NULL,
                                                          true);
}

static int baji_photo_mqtt_publish_image_result_fields_ex(baji_photo_mqtt_ctx_t *ctx,
                                                          const char *reply_to,
                                                          const char *task_id,
                                                          const char *image_id,
                                                          int code,
                                                          uint64_t timestamp_ms,
                                                          uint64_t display_time_ms,
                                                          const char *message,
                                                          const char *reason,
                                                          liot_mqtt_request_cb_t cb,
                                                          void *arg,
                                                          bool wait_req)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    char timestamp_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    char display_time_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    int written;

    if ((ctx == NULL) || (reply_to == NULL) || (task_id == NULL) ||
        (image_id == NULL) || (message == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    if (code == 0) {
        written = snprintf(payload,
                           sizeof(payload),
                           "{\"msg_id\":\"reply_%lu\",\"reply_to\":\"%s\","
                           "\"type\":\"image.display.result\",\"imei\":\"%s\","
                           "\"timestamp\":%s,\"version\":\"1.0\",\"code\":0,"
                           "\"message\":\"%s\",\"data\":{\"task_id\":\"%s\","
                           "\"image_id\":\"%s\",\"display_time\":%s}}",
                           (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                           reply_to,
                           ctx->imei,
                           baji_photo_diag_u64_dec(timestamp_ms,
                                                   timestamp_buf,
                                                   sizeof(timestamp_buf)),
                           message,
                           task_id,
                           image_id,
                           baji_photo_diag_u64_dec(display_time_ms,
                                                   display_time_buf,
                                                   sizeof(display_time_buf)));
    } else {
        written = snprintf(payload,
                           sizeof(payload),
                           "{\"msg_id\":\"reply_%lu\",\"reply_to\":\"%s\","
                           "\"type\":\"image.display.result\",\"imei\":\"%s\","
                           "\"timestamp\":%s,\"version\":\"1.0\",\"code\":%d,"
                           "\"message\":\"%s\",\"data\":{\"task_id\":\"%s\","
                           "\"image_id\":\"%s\",\"reason\":\"%s\"}}",
                           (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                           reply_to,
                           ctx->imei,
                           baji_photo_diag_u64_dec(timestamp_ms,
                                                   timestamp_buf,
                                                   sizeof(timestamp_buf)),
                           code,
                           message,
                           task_id,
                           image_id,
                           (reason != NULL) ? reason : "");
    }
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish_raw(ctx,
                                       ctx->topic_reply,
                                       payload,
                                       (unsigned short)written,
                                       cb,
                                       arg,
                                       wait_req);
}

static int baji_photo_mqtt_publish_image_result(baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_image_task_t *task,
                                                int code,
                                                const char *message,
                                                const char *reason)
{
    if ((ctx == NULL) || (task == NULL) || (message == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    return baji_photo_mqtt_publish_image_result_fields(ctx,
                                                       task->msg_id,
                                                       task->task_id,
                                                       task->image_id,
                                                       code,
                                                       baji_photo_mqtt_now_ms(),
                                                       0u,
                                                       message,
                                                       reason);
}

static int baji_photo_mqtt_map_download_error(int ret,
                                              const baji_photo_http_image_result_t *result)
{
    if (ret == LIOT_EXTFLASH_NO_SPACE) {
        return 1004;
    }
    if (ret == BAJI_PHOTO_MQTT_INDEX_READBACK_FAIL) {
        return 1005;
    }
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_MD5_MISMATCH) {
        return 1006;
    }
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL) {
        return 1003;
    }
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH) {
        return 1007;
    }
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_RESUME) {
        return 1011;
    }
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_UNSUPPORTED) {
        return 1012;
    }
    if ((result != NULL) && (result->http_status == 403)) {
        return 1010;
    }
    return 1001;
}

static void baji_photo_mqtt_build_error_reason(char *buf,
                                               unsigned int buf_len,
                                               int ret,
                                               const baji_photo_http_image_result_t *result)
{
    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }

    if (ret == BAJI_PHOTO_MQTT_INDEX_READBACK_FAIL) {
        (void)snprintf(buf,
                       buf_len,
                       "index_readback_failed downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == LIOT_EXTFLASH_NO_SPACE) {
        (void)snprintf(buf,
                       buf_len,
                       "no_space downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_MD5_MISMATCH) {
        (void)snprintf(buf,
                       buf_len,
                       "md5_mismatch downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL) {
        (void)snprintf(buf,
                       buf_len,
                       "image_parse_failed downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_BODY_TRUNCATED) {
        (void)snprintf(buf,
                       buf_len,
                       "body_truncated status=%d downloaded=%lu",
                       (result != NULL) ? result->http_status : -1,
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH) {
        (void)snprintf(buf,
                       buf_len,
                       "image_size_mismatch downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_RESUME) {
        (void)snprintf(buf,
                       buf_len,
                       "range_resume_failed downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_UNSUPPORTED) {
        (void)snprintf(buf,
                       buf_len,
                       "range_not_supported downloaded=%lu",
                       (unsigned long)((result != NULL) ? result->downloaded_size : 0u));
    } else if ((result != NULL) && (result->http_status > 0)) {
        (void)snprintf(buf,
                       buf_len,
                       "http_status=%d ret=%d downloaded=%lu",
                       result->http_status,
                       ret,
                       (unsigned long)result->downloaded_size);
    } else if (result != NULL) {
        (void)snprintf(buf,
                       buf_len,
                       "ret=%d downloaded=%lu",
                       ret,
                       (unsigned long)result->downloaded_size);
    } else {
        (void)snprintf(buf, buf_len, "ret=%d", ret);
    }
}

static void baji_photo_mqtt_prepare_pending_reply(const baji_photo_image_task_t *task,
                                                  int code,
                                                  uint64_t display_time_ms,
                                                  const char *reason,
                                                  baji_photo_mqtt_pending_reply_t *reply)
{
    if ((task == NULL) || (reply == NULL)) {
        return;
    }

    memset(reply, 0, sizeof(*reply));
    (void)snprintf(reply->reply_to, sizeof(reply->reply_to), "%s", task->msg_id);
    (void)snprintf(reply->task_id, sizeof(reply->task_id), "%s", task->task_id);
    (void)snprintf(reply->image_id, sizeof(reply->image_id), "%s", task->image_id);
    if (reason != NULL) {
        (void)snprintf(reply->reason, sizeof(reply->reason), "%s", reason);
    }
    reply->code = code;
    reply->display_time_ms = display_time_ms;
}

static void baji_photo_mqtt_pending_reply_ack_reset(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->pending_reply_waiting_ack = false;
    ctx->pending_reply_ack_pending = false;
    ctx->pending_reply_ack_err = 0;
    ctx->pending_reply_ack_generation = 0u;
    ctx->pending_reply_publish_at_s = 0u;
}

static void baji_photo_mqtt_prepare_pending_reply_runtime(baji_photo_mqtt_ctx_t *ctx,
                                                          baji_photo_mqtt_pending_reply_t *reply)
{
    if ((ctx == NULL) || (reply == NULL)) {
        return;
    }

    if (reply->timestamp_ms == 0u) {
        reply->timestamp_ms = baji_photo_mqtt_now_ms();
    }
    if (reply->generation == 0u) {
        reply->generation = baji_photo_diag_next_id();
    }
}

static void baji_photo_mqtt_pending_reply_reset(baji_photo_mqtt_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    baji_photo_mqtt_pending_reply_ack_reset(ctx);
    memset(&ctx->pending_reply, 0, sizeof(ctx->pending_reply));
    ctx->has_pending_reply = false;
    ctx->pending_reply_store_known_empty = false;
}

static void baji_photo_mqtt_track_pending_reply(baji_photo_mqtt_ctx_t *ctx,
                                                const baji_photo_mqtt_pending_reply_t *reply)
{
    if ((ctx == NULL) || (reply == NULL)) {
        return;
    }

    ctx->pending_reply = *reply;
    ctx->has_pending_reply = true;
    ctx->pending_reply_store_known_empty = false;
    if ((reply->code == 0) && (reply->task_id[0] != '\0')) {
        (void)snprintf(ctx->last_task_id, sizeof(ctx->last_task_id), "%s", reply->task_id);
    }
}

static int baji_photo_mqtt_load_pending_reply(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_pending_reply_t reply;
    int ret;

    if (ctx == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }
    if (ctx->has_pending_reply) {
        return 0;
    }
    if (ctx->pending_reply_store_known_empty) {
        return LIOT_EXTFLASH_NOT_EXIST;
    }

    memset(&reply, 0, sizeof(reply));
    ret = baji_photo_store_load_pending_reply(&reply);
    if (ret == 0) {
        baji_photo_mqtt_track_pending_reply(ctx, &reply);
    } else if (ret == LIOT_EXTFLASH_NOT_EXIST) {
        ctx->pending_reply_store_known_empty = true;
    }
    return ret;
}

static void baji_photo_mqtt_final_reply_req_cb(liot_mqtt_client_t *client, void *arg, int err)
{
    baji_photo_mqtt_ctx_t *ctx = (baji_photo_mqtt_ctx_t *)arg;

    (void)client;
    if (ctx == NULL) {
        return;
    }

    liot_rtos_enter_critical();
    ctx->pending_reply_ack_err = err;
    ctx->pending_reply_ack_generation = ctx->pending_reply.generation;
    ctx->pending_reply_ack_pending = true;
    liot_rtos_exit_critical();
}

static int baji_photo_mqtt_publish_pending_reply_start(baji_photo_mqtt_ctx_t *ctx,
                                                       baji_photo_mqtt_pending_reply_t *reply)
{
    int ret;

    if ((ctx == NULL) || (reply == NULL)) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    baji_photo_mqtt_prepare_pending_reply_runtime(ctx, reply);
    if (reply->attempt_count < 0xFFFFFFFFu) {
        reply->attempt_count += 1u;
    }
    ctx->pending_reply = *reply;
    ctx->has_pending_reply = true;
    ctx->pending_reply_store_known_empty = false;
#if BAJI_PHOTO_ENABLE_PENDING_REPLY_ATTEMPT_PERSIST
    ret = baji_photo_store_save_pending_reply(reply);
#else
    BAJI_PHOTO_MQTT_TRACE("skip pending reply attempt persist task=%s code=%d gen=%lu attempt=%lu",
                          reply->task_id,
                          reply->code,
                          (unsigned long)reply->generation,
                          (unsigned long)reply->attempt_count);
    ret = 0;
#endif
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("save pending reply attempt fail ret=%d task=%s code=%d gen=%lu attempt=%lu",
                              ret,
                              reply->task_id,
                              reply->code,
                              (unsigned long)reply->generation,
                              (unsigned long)reply->attempt_count);
        return ret;
    }

    ctx->pending_reply_waiting_ack = true;
    ctx->pending_reply_ack_pending = false;
    ctx->pending_reply_ack_err = 0;
    ctx->pending_reply_ack_generation = reply->generation;
    ctx->pending_reply_publish_at_s = baji_photo_mqtt_now_s();

    ret = baji_photo_mqtt_publish_image_result_fields_ex(ctx,
                                                         reply->reply_to,
                                                         reply->task_id,
                                                         reply->image_id,
                                                         reply->code,
                                                         reply->timestamp_ms,
                                                         reply->display_time_ms,
                                                         baji_photo_mqtt_result_message(reply->code),
                                                         (reply->code == 0) ? NULL : reply->reason,
                                                         baji_photo_mqtt_final_reply_req_cb,
                                                         ctx,
                                                         false);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("reply publish start fail ret=%d task=%s code=%d gen=%lu attempt=%lu",
                              ret,
                              reply->task_id,
                              reply->code,
                              (unsigned long)reply->generation,
                              (unsigned long)reply->attempt_count);
        baji_photo_mqtt_pending_reply_ack_reset(ctx);
        return ret;
    }
    return 0;
}

static void baji_photo_mqtt_process_pending_reply_ack(baji_photo_mqtt_ctx_t *ctx)
{
    uint32_t ack_generation = 0u;
    int ack_err = 0;
    bool ack_pending = false;
    bool cleared = false;

    if ((ctx == NULL) || !ctx->pending_reply_waiting_ack) {
        return;
    }

    liot_rtos_enter_critical();
    if (ctx->pending_reply_ack_pending) {
        ack_pending = true;
        ack_err = ctx->pending_reply_ack_err;
        ack_generation = ctx->pending_reply_ack_generation;
        ctx->pending_reply_ack_pending = false;
    }
    liot_rtos_exit_critical();

    if (!ack_pending) {
        uint32_t now_s = baji_photo_mqtt_now_s();

        if ((ctx->pending_reply_publish_at_s != 0u) &&
            (now_s >= ctx->pending_reply_publish_at_s) &&
            ((now_s - ctx->pending_reply_publish_at_s) >= BAJI_PHOTO_MQTT_FINAL_REPLY_ACK_TIMEOUT_S)) {
            BAJI_PHOTO_MQTT_TRACE("reply ack timeout task=%s code=%d gen=%lu attempt=%lu wait=%lu",
                                  ctx->pending_reply.task_id,
                                  ctx->pending_reply.code,
                                  (unsigned long)ctx->pending_reply.generation,
                                  (unsigned long)ctx->pending_reply.attempt_count,
                                  (unsigned long)(now_s - ctx->pending_reply_publish_at_s));
            baji_photo_mqtt_pending_reply_ack_reset(ctx);
        }
        return;
    }

    if (ack_generation != ctx->pending_reply.generation) {
        BAJI_PHOTO_MQTT_TRACE("reply ack drop task=%s code=%d ack_gen=%lu current_gen=%lu err=%d",
                              ctx->pending_reply.task_id,
                              ctx->pending_reply.code,
                              (unsigned long)ack_generation,
                              (unsigned long)ctx->pending_reply.generation,
                              ack_err);
        return;
    }

    BAJI_PHOTO_MQTT_TRACE("reply ack task=%s image=%s code=%d gen=%lu attempt=%lu err=%d",
                          ctx->pending_reply.task_id,
                          ctx->pending_reply.image_id,
                          ctx->pending_reply.code,
                          (unsigned long)ctx->pending_reply.generation,
                          (unsigned long)ctx->pending_reply.attempt_count,
                          ack_err);
    baji_photo_mqtt_marker_reply_ack(&ctx->pending_reply, ack_err);
    baji_photo_mqtt_pending_reply_ack_reset(ctx);
    if (ack_err != 0) {
        return;
    }

    baji_photo_mqtt_clear_saved_task_state_for_reply(&ctx->pending_reply);
    ack_err = baji_photo_store_clear_pending_reply();
    if (ack_err != 0) {
        BAJI_PHOTO_MQTT_TRACE("clear pending reply fail ret=%d task=%s code=%d",
                              ack_err,
                              ctx->pending_reply.task_id,
                              ctx->pending_reply.code);
    } else {
        cleared = true;
    }
    if (ctx->has_current_task &&
        (strcmp(ctx->current_task.task_id, ctx->pending_reply.task_id) == 0)) {
        BAJI_PHOTO_MQTT_TRACE("tail finish after ack task=%s image=%s success=%d",
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              (ctx->pending_reply.code == 0) ? 1 : 0);
        baji_photo_mqtt_finish_current_task(ctx, ctx->pending_reply.code == 0);
    }
    baji_photo_mqtt_pending_reply_reset(ctx);
    ctx->pending_reply_store_known_empty = cleared;
}

static int baji_photo_mqtt_flush_pending_reply(baji_photo_mqtt_ctx_t *ctx)
{
    int ret;

    if (ctx == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }
    if (!ctx->has_pending_reply) {
        ret = baji_photo_mqtt_load_pending_reply(ctx);
        if (ret == LIOT_EXTFLASH_NOT_EXIST) {
            return 0;
        }
        if (ret != 0) {
            return ret;
        }
    }
    if (ctx->pending_reply_waiting_ack) {
        baji_photo_mqtt_process_pending_reply_ack(ctx);
        return 0;
    }

    ret = baji_photo_mqtt_publish_pending_reply_start(ctx, &ctx->pending_reply);
    return ret;
}

static void baji_photo_mqtt_finish_current_task(baji_photo_mqtt_ctx_t *ctx, bool success)
{
    if (ctx == NULL) {
        return;
    }

    if (success) {
        (void)snprintf(ctx->last_task_id, sizeof(ctx->last_task_id), "%s", ctx->current_task.task_id);
    }
    baji_photo_mqtt_clear_current_task_runtime(ctx);
}

static void baji_photo_mqtt_emit_image_result(baji_photo_mqtt_ctx_t *ctx,
                                              int code,
                                              const baji_photo_http_image_result_t *download_result)
{
    baji_photo_mqtt_image_result_t result;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }

    memset(&result, 0, sizeof(result));
    (void)snprintf(result.task_id, sizeof(result.task_id), "%s", ctx->current_task.task_id);
    (void)snprintf(result.image_id, sizeof(result.image_id), "%s", ctx->current_task.image_id);
    result.code = code;
    result.trace_id = ctx->current_task.trace_id;
    if (download_result != NULL) {
        result.downloaded_size = download_result->downloaded_size;
        result.range_resumed = download_result->range_resumed;
    }
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail ui enter task=%s image=%s code=%d downloaded=%lu range=%d",
                          (unsigned long)result.trace_id,
                          result.task_id,
                          result.image_id,
                          result.code,
                          (unsigned long)result.downloaded_size,
                          result.range_resumed ? 1 : 0);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_IMAGE_RESULT, &result);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail ui return task=%s image=%s code=%d",
                          (unsigned long)result.trace_id,
                          result.task_id,
                          result.image_id,
                          result.code);
}

static void baji_photo_mqtt_emit_display_request(baji_photo_mqtt_ctx_t *ctx,
                                                 const baji_photo_http_image_result_t *download_result)
{
    baji_photo_mqtt_display_request_t request;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }

    memset(&request, 0, sizeof(request));
    (void)snprintf(request.task_id, sizeof(request.task_id), "%s", ctx->current_task.task_id);
    (void)snprintf(request.image_id, sizeof(request.image_id), "%s", ctx->current_task.image_id);
    request.trace_id = ctx->current_task.trace_id;
    if (download_result != NULL) {
        request.downloaded_size = download_result->downloaded_size;
        request.range_resumed = download_result->range_resumed;
    }
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail display req enter task=%s image=%s downloaded=%lu range=%d",
                          (unsigned long)request.trace_id,
                          request.task_id,
                          request.image_id,
                          (unsigned long)request.downloaded_size,
                          request.range_resumed ? 1 : 0);
    baji_photo_mqtt_marker_display_req(&request);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_DISPLAY_REQUEST, &request);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail display req return task=%s image=%s",
                          (unsigned long)request.trace_id,
                          request.task_id,
                          request.image_id);
}

static bool baji_photo_mqtt_take_display_result(baji_photo_mqtt_ctx_t *ctx,
                                                baji_photo_mqtt_display_result_t *result)
{
    bool pending = false;

    if ((ctx == NULL) || (result == NULL)) {
        return false;
    }

    liot_rtos_enter_critical();
    if (ctx->display_result_pending) {
        *result = ctx->display_result;
        ctx->display_result_pending = false;
        pending = true;
    }
    liot_rtos_exit_critical();
    return pending;
}

static void baji_photo_mqtt_process_display_result(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_mqtt_display_result_t display_result;
    baji_photo_mqtt_pending_reply_t pending_reply;
    char display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    char reply_ts_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    char reply_display_ms_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    const char *reason = NULL;
    int save_ret;
    int publish_ret;
    int meta_ret = 0;
    int result_code;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }
    if (!baji_photo_mqtt_take_display_result(ctx, &display_result)) {
        return;
    }

    if ((strcmp(display_result.task_id, ctx->current_task.task_id) != 0) ||
        (strcmp(display_result.image_id, ctx->current_task.image_id) != 0)) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu drop display result task=%s image=%s current_task=%s current_image=%s code=%d",
                              (unsigned long)display_result.trace_id,
                              display_result.task_id,
                              display_result.image_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              display_result.code);
        return;
    }

    result_code = display_result.code;
    if ((result_code == 0) && (display_result.display_time_ms == 0u)) {
        display_result.display_time_ms = baji_photo_mqtt_now_ms();
    }
    reason = (display_result.reason[0] != '\0') ? display_result.reason : NULL;
    ctx->current_task_state.display_pending = false;
    ctx->current_task_state.displayed = (result_code == 0);
    ctx->current_task_state.display_time_ms =
        (result_code == 0) ? display_result.display_time_ms : 0u;
#if BAJI_PHOTO_ENABLE_DISPLAY_RESULT_STATE_PERSIST
    (void)baji_photo_mqtt_save_current_task_state(ctx,
                                                  (result_code == 0) ? "displayed" : "display_fail");
#else
    BAJI_PHOTO_MQTT_TRACE("op=%lu display result persist disabled task=%s image=%s code=%d display_ms=%s",
                          (unsigned long)display_result.trace_id,
                          display_result.task_id,
                          display_result.image_id,
                          result_code,
                          baji_photo_diag_u64_dec(display_result.display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)));
#endif

    BAJI_PHOTO_MQTT_TRACE("op=%lu display ack task=%s image=%s code=%d display_ms=%s reason=%s",
                          (unsigned long)display_result.trace_id,
                          display_result.task_id,
                          display_result.image_id,
                          result_code,
                          baji_photo_diag_u64_dec(display_result.display_time_ms,
                                                  display_ms_buf,
                                                  sizeof(display_ms_buf)),
                          (reason != NULL) ? reason : "-");

    if (result_code == 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail meta start task=%s image=%s display_ms=%s",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              baji_photo_diag_u64_dec(display_result.display_time_ms,
                                                      display_ms_buf,
                                                      sizeof(display_ms_buf)));
        meta_ret = baji_photo_mqtt_save_device_meta(ctx,
                                                    ctx->current_task.task_id,
                                                    ctx->current_task.image_id,
                                                    ctx->current_task.md5,
                                                    display_result.display_time_ms);
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail meta done task=%s image=%s ret=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              meta_ret);
    }
    baji_photo_mqtt_request_fast_heartbeat(ctx);

    baji_photo_mqtt_prepare_pending_reply(&ctx->current_task,
                                          result_code,
                                          (result_code == 0) ? display_result.display_time_ms : 0u,
                                          reason,
                                          &pending_reply);
    baji_photo_mqtt_prepare_pending_reply_runtime(ctx, &pending_reply);
    baji_photo_mqtt_track_pending_reply(ctx, &pending_reply);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply save start task=%s image=%s code=%d ts_ms=%s display_ms=%s gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          baji_photo_diag_u64_dec(pending_reply.timestamp_ms,
                                                  reply_ts_buf,
                                                  sizeof(reply_ts_buf)),
                          baji_photo_diag_u64_dec(pending_reply.display_time_ms,
                                                  reply_display_ms_buf,
                                                  sizeof(reply_display_ms_buf)));
    save_ret = baji_photo_store_save_pending_reply(&pending_reply);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply save done task=%s image=%s code=%d ret=%d gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          save_ret,
                          (unsigned long)pending_reply.generation);
    if (save_ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu save pending reply fail ret=%d task=%s code=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              save_ret,
                              pending_reply.task_id,
                              pending_reply.code);
    }

    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply pub start task=%s image=%s code=%d reply_to=%s gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          pending_reply.reply_to,
                          (unsigned long)pending_reply.generation);
    publish_ret = baji_photo_mqtt_flush_pending_reply(ctx);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply pub dispatch task=%s image=%s code=%d ret=%d gen=%lu ack_wait=%d attempt=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          publish_ret,
                          (unsigned long)ctx->pending_reply.generation,
                          ctx->pending_reply_waiting_ack ? 1 : 0,
                          (unsigned long)ctx->pending_reply.attempt_count);
    if (publish_ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu result publish fail ret=%d task=%s code=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              publish_ret,
                              ctx->current_task.task_id,
                              result_code);
    }
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply wait_ack task=%s image=%s code=%d gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          ctx->current_task.image_id,
                          result_code,
                          (unsigned long)ctx->pending_reply.generation);
}

static int baji_photo_mqtt_save_current_task_state(baji_photo_mqtt_ctx_t *ctx, const char *phase)
{
    int ret;

    if (ctx == NULL) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    ret = baji_photo_store_save_task_state(&ctx->current_task_state);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu save task state fail phase=%s ret=%d task=%s",
                              (unsigned long)ctx->current_task.trace_id,
                              (phase != NULL) ? phase : "unknown",
                              ret,
                              ctx->current_task.task_id);
    }
    return ret;
}

static void baji_photo_mqtt_defer_current_task_network_wait(baji_photo_mqtt_ctx_t *ctx,
                                                            int ret)
{
    uint32_t now_s;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }

    now_s = baji_photo_mqtt_now_s();
    if (ctx->current_task_state.network_wait_deadline_s == 0u) {
        ctx->current_task_state.network_wait_deadline_s =
            now_s + baji_photo_mqtt_network_wait_window_s(&ctx->current_task_state);
    }
    ctx->current_task_state.waiting_network = true;
    ctx->current_task_state.next_retry_at_s = now_s + BAJI_PHOTO_NETWORK_WAIT_RETRY_S;
    if (ctx->current_task_state.network_wait_count < 0xFFu) {
        ctx->current_task_state.network_wait_count += 1u;
    }
#if BAJI_PHOTO_ENABLE_NETWORK_WAIT_STATE_PERSIST
    (void)baji_photo_mqtt_save_current_task_state(ctx, "wait_network");
#else
    BAJI_PHOTO_MQTT_TRACE("op=%lu wait network persist disabled task=%s downloaded=%lu attempt=%u next=%lu deadline=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          (unsigned long)ctx->current_task_state.downloaded_size,
                          (unsigned int)ctx->current_task_state.network_wait_count,
                          (unsigned long)ctx->current_task_state.next_retry_at_s,
                          (unsigned long)ctx->current_task_state.network_wait_deadline_s);
#endif
    BAJI_PHOTO_MQTT_TRACE("op=%lu wait network task=%s class=%s ret=%d action=wait_network downloaded=%lu "
                          "attempt=%u next=%lu deadline=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          baji_photo_mqtt_network_error_name(ret),
                          ret,
                          (unsigned long)ctx->current_task_state.downloaded_size,
                          (unsigned int)ctx->current_task_state.network_wait_count,
                          (unsigned long)ctx->current_task_state.next_retry_at_s,
                          (unsigned long)ctx->current_task_state.network_wait_deadline_s);
    baji_photo_mqtt_marker_wait_network(ctx, ret);
}

static void baji_photo_mqtt_execute_current_task(baji_photo_mqtt_ctx_t *ctx)
{
    baji_photo_http_image_result_t download_result;
    baji_photo_mqtt_pending_reply_t pending_reply;
    baji_photo_mqtt_progress_ctx_t progress_ctx;
    baji_photo_manifest_item_t item;
    char reply_ts_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    char reason[96];
    uint32_t now_s;
    int result_code;
    int publish_ret;
    int save_ret;
    int ret;

    if ((ctx == NULL) || !ctx->has_current_task) {
        return;
    }

    ret = baji_photo_mqtt_publish_current_task_accepted(ctx, false);
    if (ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu accepted publish fail ret=%d task=%s",
                              (unsigned long)ctx->current_task.trace_id,
                              ret,
                              ctx->current_task.task_id);
        return;
    }
    if (baji_photo_net_is_busy()) {
        if (!ctx->current_task_waiting_sync) {
            BAJI_PHOTO_MQTT_TRACE("op=%lu defer task=%s while manual sync busy state=%d",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->current_task.task_id,
                                  (int)baji_photo_net_get_state());
            ctx->current_task_waiting_sync = true;
        }
        return;
    }
    ctx->current_task_waiting_sync = false;

    now_s = baji_photo_mqtt_now_s();
    if (ctx->current_task_state.waiting_network) {
        if ((ctx->current_task_state.network_wait_deadline_s != 0u) &&
            (now_s >= ctx->current_task_state.network_wait_deadline_s)) {
            ret = LIOT_HTTPC_ERR_NO_NETWORK;
            memset(&download_result, 0, sizeof(download_result));
            download_result.http_status = -1;
            download_result.downloaded_size = ctx->current_task_state.downloaded_size;
            download_result.range_resumed = (ctx->current_task_state.downloaded_size > 0u);
            BAJI_PHOTO_MQTT_TRACE("op=%lu network wait deadline reached task=%s class=%s downloaded=%lu "
                                  "attempt=%u deadline=%lu",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->current_task.task_id,
                                  baji_photo_mqtt_network_error_name(ret),
                                  (unsigned long)ctx->current_task_state.downloaded_size,
                                  (unsigned int)ctx->current_task_state.network_wait_count,
                                  (unsigned long)ctx->current_task_state.network_wait_deadline_s);
            baji_photo_mqtt_build_network_wait_reason(reason,
                                                      sizeof(reason),
                                                      &ctx->current_task_state,
                                                      ret);
            goto finalize_result;
        }
        if ((ctx->current_task_state.next_retry_at_s != 0u) &&
            (now_s < ctx->current_task_state.next_retry_at_s)) {
            return;
        }
        BAJI_PHOTO_MQTT_TRACE("op=%lu retry task after network wait task=%s downloaded=%lu attempt=%u",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              (unsigned long)ctx->current_task_state.downloaded_size,
                              (unsigned int)ctx->current_task_state.network_wait_count);
        baji_photo_mqtt_clear_network_wait_state(&ctx->current_task_state);
#if BAJI_PHOTO_ENABLE_NETWORK_WAIT_STATE_PERSIST
        (void)baji_photo_mqtt_save_current_task_state(ctx, "network_retry");
#else
        BAJI_PHOTO_MQTT_TRACE("op=%lu network retry persist disabled task=%s downloaded=%lu attempt=%u",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              (unsigned long)ctx->current_task_state.downloaded_size,
                              (unsigned int)ctx->current_task_state.network_wait_count);
#endif
    }

    memset(&progress_ctx, 0, sizeof(progress_ctx));
    progress_ctx.mqtt = ctx;
    progress_ctx.range_resumed = (ctx->current_task_state.downloaded_size > 0u);
    progress_ctx.ui_nonzero_progress_sent = progress_ctx.range_resumed;
    baji_photo_mqtt_emit_image_progress(ctx,
                                        &ctx->current_task,
                                        ctx->current_task_state.downloaded_size,
                                        ctx->current_task.image_size,
                                        baji_photo_mqtt_progress_bucket(
                                            baji_photo_mqtt_progress_percent(
                                                ctx->current_task_state.downloaded_size,
                                                ctx->current_task.image_size)),
                                        progress_ctx.range_resumed);
    progress_ctx.last_ui_bucket =
        baji_photo_mqtt_progress_bucket(
            baji_photo_mqtt_progress_percent(ctx->current_task_state.downloaded_size,
                                             ctx->current_task.image_size));
    progress_ctx.ui_progress_sent = true;
    publish_ret = baji_photo_mqtt_publish_image_progress(ctx,
                                                         &ctx->current_task,
                                                         ctx->current_task_state.downloaded_size,
                                                         ctx->current_task.image_size,
                                                         progress_ctx.range_resumed);
    if (publish_ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu progress publish fail ret=%d task=%s progress=start",
                              (unsigned long)ctx->current_task.trace_id,
                              publish_ret,
                              ctx->current_task.task_id);
    }
    if (baji_photo_mqtt_progress_percent(ctx->current_task_state.downloaded_size,
                                         ctx->current_task.image_size) >= 50u) {
        progress_ctx.half_sent = true;
    }

    memset(&item, 0, sizeof(item));
    memset(reason, 0, sizeof(reason));
    memset(&download_result, 0, sizeof(download_result));
    download_result.http_status = -1;

    ret = baji_photo_http_download_image_task(&ctx->current_task,
                                              &ctx->current_task_state,
                                              baji_photo_mqtt_progress_cb,
                                              &progress_ctx,
                                              &item,
                                              &download_result);
    if (ret == 0) {
        baji_photo_mqtt_clear_network_wait_state(&ctx->current_task_state);
    } else if (baji_photo_mqtt_retryable_network_error(ret)) {
        if (ret == BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL) {
            BAJI_PHOTO_MQTT_TRACE("op=%lu classify legacy pdp active fail as wait_network task=%s downloaded=%lu",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->current_task.task_id,
                                  (unsigned long)ctx->current_task_state.downloaded_size);
        }
        if ((ctx->current_task_state.network_wait_deadline_s != 0u) &&
            (baji_photo_mqtt_now_s() >= ctx->current_task_state.network_wait_deadline_s)) {
            baji_photo_mqtt_build_network_wait_reason(reason,
                                                      sizeof(reason),
                                                      &ctx->current_task_state,
                                                      ret);
            goto finalize_result;
        }
        baji_photo_mqtt_defer_current_task_network_wait(ctx, ret);
        return;
    }
    if (ret == 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail enter task=%s image=%s downloaded=%lu status=%d range=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              (unsigned long)download_result.downloaded_size,
                              download_result.http_status,
                              download_result.range_resumed ? 1 : 0);
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail index start task=%s image=%s",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id);
        ret = baji_photo_mqtt_upsert_downloaded_item(ctx, &item);
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail index done task=%s image=%s ret=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              ret);
        if (ret == 0) {
            ctx->current_task_state.display_pending = true;
            ctx->current_task_state.displayed = false;
            ctx->current_task_state.display_time_ms = 0u;
            (void)baji_photo_mqtt_save_current_task_state(ctx, "display_pending");
            BAJI_PHOTO_MQTT_TRACE("op=%lu tail display pending task=%s image=%s downloaded=%lu",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->current_task.task_id,
                                  ctx->current_task.image_id,
                                  (unsigned long)download_result.downloaded_size);
            baji_photo_mqtt_emit_display_request(ctx, &download_result);
        }
        publish_ret = baji_photo_mqtt_publish_image_progress(ctx,
                                                             &ctx->current_task,
                                                             ctx->current_task.image_size,
                                                             ctx->current_task.image_size,
                                                             download_result.range_resumed);
        baji_photo_mqtt_emit_image_progress(ctx,
                                            &ctx->current_task,
                                            ctx->current_task.image_size,
                                            ctx->current_task.image_size,
                                            100u,
                                            download_result.range_resumed);
        if (publish_ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("op=%lu progress publish fail ret=%d task=%s progress=complete",
                                  (unsigned long)ctx->current_task.trace_id,
                                  publish_ret,
                                  ctx->current_task.task_id);
        }
        if (ret == 0) {
            return;
        }
    }

finalize_result:
    if (ret == 0) {
        result_code = 0;
        BAJI_PHOTO_MQTT_TRACE("op=%lu tail finalize task=%s image=%s code=0 downloaded=%lu status=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              ctx->current_task.image_id,
                              (unsigned long)download_result.downloaded_size,
                              download_result.http_status);
        baji_photo_mqtt_marker_tail_result("MQTT_TAIL_OK",
                                           &ctx->current_task,
                                           &download_result,
                                           result_code,
                                           0);
    } else {
        result_code = baji_photo_mqtt_map_download_error(ret, &download_result);
        if (reason[0] == '\0') {
            baji_photo_mqtt_build_error_reason(reason, sizeof(reason), ret, &download_result);
        }
        BAJI_PHOTO_MQTT_TRACE("op=%lu finalize task=%s code=%d ret=%d class=%s downloaded=%lu status=%d reason=%s",
                              (unsigned long)ctx->current_task.trace_id,
                              ctx->current_task.task_id,
                              result_code,
                              ret,
                              baji_photo_mqtt_retryable_network_error(ret) ?
                                  baji_photo_mqtt_network_error_name(ret) : "non_network",
                              (unsigned long)download_result.downloaded_size,
                              download_result.http_status,
                              reason);
        baji_photo_mqtt_marker_tail_result("MQTT_TAIL_FAIL",
                                           &ctx->current_task,
                                           &download_result,
                                           result_code,
                                           ret);
        publish_ret = baji_photo_mqtt_publish_image_progress(ctx,
                                                             &ctx->current_task,
                                                             download_result.downloaded_size,
                                                             ctx->current_task.image_size,
                                                             download_result.range_resumed);
        if (publish_ret != 0) {
            BAJI_PHOTO_MQTT_TRACE("op=%lu progress publish fail ret=%d task=%s progress=fail",
                                  (unsigned long)ctx->current_task.trace_id,
                                  publish_ret,
                                  ctx->current_task.task_id);
        }
    }

    baji_photo_mqtt_emit_image_result(ctx, result_code, &download_result);
    baji_photo_mqtt_prepare_pending_reply(&ctx->current_task,
                                          result_code,
                                          0u,
                                          (result_code == 0) ? NULL : reason,
                                          &pending_reply);
    baji_photo_mqtt_prepare_pending_reply_runtime(ctx, &pending_reply);
    baji_photo_mqtt_track_pending_reply(ctx, &pending_reply);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply save start task=%s image=%s code=%d ts_ms=%s gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          baji_photo_diag_u64_dec(pending_reply.timestamp_ms,
                                                  reply_ts_buf,
                                                  sizeof(reply_ts_buf)),
                          (unsigned long)pending_reply.generation);
    save_ret = baji_photo_store_save_pending_reply(&pending_reply);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply save done task=%s image=%s code=%d ret=%d gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          save_ret,
                          (unsigned long)pending_reply.generation);
    if (save_ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu save pending reply fail ret=%d task=%s code=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              save_ret,
                              pending_reply.task_id,
                              pending_reply.code);
    }

    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply pub start task=%s image=%s code=%d reply_to=%s gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          pending_reply.reply_to,
                          (unsigned long)pending_reply.generation);
    publish_ret = baji_photo_mqtt_flush_pending_reply(ctx);
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply pub dispatch task=%s image=%s code=%d ret=%d gen=%lu ack_wait=%d attempt=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          pending_reply.task_id,
                          pending_reply.image_id,
                          pending_reply.code,
                          publish_ret,
                          (unsigned long)ctx->pending_reply.generation,
                          ctx->pending_reply_waiting_ack ? 1 : 0,
                          (unsigned long)ctx->pending_reply.attempt_count);
    baji_photo_mqtt_marker_reply_pub(&ctx->pending_reply,
                                     publish_ret,
                                     ctx->pending_reply_waiting_ack);
    if (publish_ret != 0) {
        BAJI_PHOTO_MQTT_TRACE("op=%lu result publish fail ret=%d task=%s code=%d",
                              (unsigned long)ctx->current_task.trace_id,
                              publish_ret,
                              ctx->current_task.task_id,
                              result_code);
    }
    BAJI_PHOTO_MQTT_TRACE("op=%lu tail reply wait_ack task=%s image=%s code=%d gen=%lu",
                          (unsigned long)ctx->current_task.trace_id,
                          ctx->current_task.task_id,
                          ctx->current_task.image_id,
                          result_code,
                          (unsigned long)ctx->pending_reply.generation);
}

static int baji_photo_mqtt_publish_online(baji_photo_mqtt_ctx_t *ctx)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    char firmware_version[64];
    char hardware_version[64];
    char timestamp_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    baji_photo_mqtt_storage_info_t storage;
    int written;
    uint64_t timestamp_ms;
    const char *timestamp_text;

    baji_photo_mqtt_get_firmware_version(firmware_version, sizeof(firmware_version));
    baji_photo_mqtt_get_hardware_version(hardware_version, sizeof(hardware_version));
    baji_photo_mqtt_get_storage_info(&storage);
    timestamp_ms = baji_photo_mqtt_status_timestamp_ms();
    timestamp_text = baji_photo_diag_u64_dec(timestamp_ms,
                                             timestamp_buf,
                                             sizeof(timestamp_buf));
    BAJI_PHOTO_MQTT_TRACE("online payload meta ts=%s fw=%s hw=%s total=%lu free=%lu dyn_info=%u dyn_ts=%u",
                          timestamp_text,
                          firmware_version,
                          hardware_version,
                          storage.total_bytes,
                          storage.free_bytes,
                          (unsigned int)BAJI_PHOTO_MQTT_ONLINE_DYNAMIC_DEVICE_INFO,
                          (unsigned int)BAJI_PHOTO_MQTT_STATUS_DYNAMIC_TIMESTAMP);

    written = snprintf(payload,
                       sizeof(payload),
                       "{\"msg_id\":\"online_%lu\",\"type\":\"device.online\","
                       "\"imei\":\"%s\",\"timestamp\":%s,\"version\":\"1.0\","
                       "\"data\":{\"firmware_version\":\"%s\",\"hardware_version\":\"%s\","
                       "\"battery\":0,\"screen_width\":%u,\"screen_height\":%u,"
                       "\"screen_color\":\"rgb\",\"storage_total\":%lu,\"storage_free\":%lu,"
                       "\"network\":\"4g\"}}",
                       (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                       ctx->imei,
                       timestamp_text,
                       firmware_version,
                       hardware_version,
                       BAJI_PHOTO_IMG_W,
                       BAJI_PHOTO_IMG_H,
                       storage.total_bytes,
                       storage.free_bytes);
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish(ctx, ctx->topic_status, payload, (unsigned short)written);
}

static int baji_photo_mqtt_publish_heartbeat(baji_photo_mqtt_ctx_t *ctx)
{
    char payload[BAJI_PHOTO_MQTT_TX_BUF_MAX];
    char current_image_id[BAJI_PHOTO_IMAGE_ID_MAX_LEN + 1u];
    char timestamp_buf[BAJI_PHOTO_DIAG_U64_DEC_BUF_LEN];
    baji_photo_mqtt_storage_info_t storage;
    int written;
    int rssi;
    uint64_t timestamp_ms;
    const char *screen_status;
    const char *timestamp_text;

    current_image_id[0] = '\0';
    if (baji_photo_mqtt_has_text(ctx->device_meta.current_image_id)) {
        baji_photo_mqtt_copy_json_safe_string(current_image_id,
                                              sizeof(current_image_id),
                                              ctx->device_meta.current_image_id,
                                              NULL);
    }
    rssi = baji_photo_mqtt_get_rssi_dbm();
    baji_photo_mqtt_get_storage_info(&storage);
    timestamp_ms = baji_photo_mqtt_status_timestamp_ms();
    screen_status = baji_photo_mqtt_screen_status(ctx);
    timestamp_text = baji_photo_diag_u64_dec(timestamp_ms,
                                             timestamp_buf,
                                             sizeof(timestamp_buf));

    if (current_image_id[0] != '\0') {
        written = snprintf(payload,
                           sizeof(payload),
                           "{\"msg_id\":\"heartbeat_%lu\",\"type\":\"device.heartbeat\","
                           "\"imei\":\"%s\",\"timestamp\":%s,\"version\":\"1.0\","
                           "\"data\":{\"battery\":0,\"charging\":false,\"rssi\":%d,"
                           "\"free_memory\":%lu,\"storage_total\":%lu,\"storage_free\":%lu,"
                           "\"screen_status\":\"%s\","
                           "\"current_image_id\":\"%s\"}}",
                           (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                           ctx->imei,
                           timestamp_text,
                           rssi,
                           (unsigned long)liot_xPortGetFreeHeapSize(),
                           storage.total_bytes,
                           storage.free_bytes,
                           screen_status,
                           current_image_id);
    } else {
        written = snprintf(payload,
                           sizeof(payload),
                           "{\"msg_id\":\"heartbeat_%lu\",\"type\":\"device.heartbeat\","
                           "\"imei\":\"%s\",\"timestamp\":%s,\"version\":\"1.0\","
                           "\"data\":{\"battery\":0,\"charging\":false,\"rssi\":%d,"
                           "\"free_memory\":%lu,\"storage_total\":%lu,\"storage_free\":%lu,"
                           "\"screen_status\":\"%s\"}}",
                           (unsigned long)baji_photo_mqtt_next_msg_id(ctx),
                           ctx->imei,
                           timestamp_text,
                           rssi,
                           (unsigned long)liot_xPortGetFreeHeapSize(),
                           storage.total_bytes,
                           storage.free_bytes,
                           screen_status);
    }
    if ((written <= 0) || ((unsigned int)written >= sizeof(payload))) {
        return LIOT_MQTTCLIENT_OUT_OF_MEM;
    }

    return baji_photo_mqtt_publish(ctx, ctx->topic_status, payload, (unsigned short)written);
}

static void baji_photo_mqtt_handle_device_init(baji_photo_mqtt_ctx_t *ctx, const cJSON *root)
{
    const cJSON *data;
    const cJSON *bind_status;
    const cJSON *heartbeat_interval;
    baji_photo_mqtt_init_info_t info = {
        .bind_status = BAJI_PHOTO_BIND_UNKNOWN,
        .heartbeat_interval_s = 0u,
        .image_max_size = 0u,
    };

    data = cJSON_GetObjectItemCaseSensitive((cJSON *)root, "data");
    if (!cJSON_IsObject(data)) {
        return;
    }

    bind_status = cJSON_GetObjectItemCaseSensitive((cJSON *)data, "bind_status");
    ctx->bind_status = baji_photo_mqtt_parse_bind_status(bind_status);
    info.bind_status = ctx->bind_status;
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_BIND_STATUS, &ctx->bind_status);
    baji_photo_mqtt_request_fast_heartbeat(ctx);

    heartbeat_interval = cJSON_GetObjectItemCaseSensitive((cJSON *)data, "heartbeat_interval");
    if (cJSON_IsNumber(heartbeat_interval) && (heartbeat_interval->valuedouble > 0.0)) {
        uint32_t interval = (uint32_t)heartbeat_interval->valuedouble;

        if (interval > 300u) {
            interval = 300u;
        }
        ctx->heartbeat_interval_s = interval;
    }
    info.heartbeat_interval_s = ctx->heartbeat_interval_s;
    (void)baji_photo_mqtt_json_get_u32(data, "image_max_size", &info.image_max_size);
    ctx->image_max_size = info.image_max_size;

    BAJI_PHOTO_MARK_TRACE("MQTT_DEVICE_INIT bind=%d heartbeat=%lu image_max=%lu heap_min=%lu",
                          (int)ctx->bind_status,
                          (unsigned long)ctx->heartbeat_interval_s,
                          (unsigned long)ctx->image_max_size,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    BAJI_PHOTO_MQTT_TRACE("device.init bind=%d heartbeat=%lu",
                          (int)ctx->bind_status,
                          (unsigned long)ctx->heartbeat_interval_s);
    baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_INIT_INFO, &info);
    if (ctx->bind_status == BAJI_PHOTO_BIND_UNBOUND) {
        if (baji_photo_mqtt_request_bind_token_internal(ctx, "device_unbound") != 0) {
            BAJI_PHOTO_MQTT_TRACE("auto bind token request fail bind=%d",
                                  (int)ctx->bind_status);
            baji_photo_mqtt_emit_bind_notice(ctx,
                                             BAJI_PHOTO_BIND_NOTICE_FAIL,
                                             "QR request failed, retrying");
        }
        baji_photo_mqtt_schedule_bind_token_retry(ctx);
    } else if (ctx->bind_status != BAJI_PHOTO_BIND_BOUND) {
        baji_photo_mqtt_reset_bind_token_info(ctx);
    }
}

static void baji_photo_mqtt_handle_message(baji_photo_mqtt_ctx_t *ctx,
                                           const char *topic,
                                           const char *payload)
{
    cJSON *root;
    cJSON *type;

    root = cJSON_Parse(payload);
    if (root == NULL) {
        BAJI_PHOTO_MQTT_TRACE("json parse fail topic=%s", topic);
        return;
    }

    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || (type->valuestring == NULL)) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "device.init") == 0) {
        baji_photo_mqtt_handle_device_init(ctx, root);
    } else if (strcmp(type->valuestring, "bind.token.response") == 0) {
        baji_photo_bind_token_info_t info;

        if (!baji_photo_mqtt_parse_bind_token_info(root, &info)) {
            BAJI_PHOTO_MQTT_TRACE("bind.token.response parse fail");
            baji_photo_mqtt_emit_bind_notice(ctx,
                                             BAJI_PHOTO_BIND_NOTICE_FAIL,
                                             "QR response invalid");
            baji_photo_mqtt_schedule_bind_token_retry(ctx);
        } else {
            ctx->bind_token_info = info;
            baji_photo_mqtt_schedule_bind_token_refresh(ctx,
                                                        ctx->bind_token_info.expire_seconds);
            BAJI_PHOTO_MQTT_TRACE("bind token ready expire=%lu url_len=%u",
                                  (unsigned long)ctx->bind_token_info.expire_seconds,
                                  (unsigned int)strlen(ctx->bind_token_info.bind_url));
            baji_photo_mqtt_emit_event(ctx,
                                       BAJI_PHOTO_MQTT_EVT_BIND_TOKEN,
                                       &ctx->bind_token_info);
        }
    } else if (strcmp(type->valuestring, "bind.success") == 0) {
        ctx->bind_status = BAJI_PHOTO_BIND_BOUND;
        baji_photo_mqtt_reset_bind_token_info(ctx);
        baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_BIND_STATUS, &ctx->bind_status);
        baji_photo_mqtt_emit_bind_notice(ctx,
                                         BAJI_PHOTO_BIND_NOTICE_SUCCESS,
                                         "Device linked");
        baji_photo_mqtt_request_fast_heartbeat(ctx);
    } else if (strcmp(type->valuestring, "bind.unbind.response") == 0) {
        const cJSON *reply_to = cJSON_GetObjectItemCaseSensitive(root, "reply_to");
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
        const cJSON *status = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive((cJSON *)data, "bind_status") : NULL;
        bool matched = cJSON_IsString(reply_to) && strcmp(reply_to->valuestring, ctx->unbind_request_id) == 0;
        bool success = matched && cJSON_IsNumber(code) && code->valueint == 0 && cJSON_IsString(status) && strcmp(status->valuestring, "unbound") == 0;
        BAJI_PHOTO_MQTT_TRACE("unbind response matched=%d success=%d", matched ? 1 : 0, success ? 1 : 0);
        if (success) {
            ctx->unbind_request_id[0] = '\0';
            ctx->bind_status = BAJI_PHOTO_BIND_UNBOUND;
            baji_photo_mqtt_clear_current_task_runtime(ctx);
            (void)baji_photo_store_clear_task_state();
            baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_BIND_STATUS, &ctx->bind_status);
            baji_photo_mqtt_emit_bind_notice(ctx, BAJI_PHOTO_BIND_NOTICE_SUCCESS, "Device unlinked");
            if (baji_photo_mqtt_request_bind_token_internal(ctx, "device_unbind") != 0)
                baji_photo_mqtt_schedule_bind_token_retry(ctx);
        } else if (matched) {
            ctx->unbind_request_id[0] = '\0';
            baji_photo_mqtt_emit_bind_notice(ctx, BAJI_PHOTO_BIND_NOTICE_FAIL, "Unbind failed");
        }    } else if (strcmp(type->valuestring, "bind.unbind") == 0) {
        ctx->bind_status = BAJI_PHOTO_BIND_UNBOUND;
        baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_BIND_STATUS, &ctx->bind_status);
        baji_photo_mqtt_request_fast_heartbeat(ctx);
        if (baji_photo_mqtt_request_bind_token_internal(ctx, "user_unbind") != 0) {
            BAJI_PHOTO_MQTT_TRACE("rebind token request fail");
            baji_photo_mqtt_emit_bind_notice(ctx,
                                             BAJI_PHOTO_BIND_NOTICE_FAIL,
                                             "QR request failed, retrying");
        }
        baji_photo_mqtt_schedule_bind_token_retry(ctx);
    } else if (strcmp(type->valuestring, "image.display") == 0) {
        baji_photo_image_task_t task;
        const char *task_reason = "validate_failed";
        int task_code;
        int ret;
        uint32_t local_max_size = 0u;

        if (!baji_photo_mqtt_parse_image_task(root, &task)) {
            BAJI_PHOTO_MQTT_TRACE("image.display parse fail");
        } else if ((task_code = baji_photo_mqtt_validate_image_task(ctx,
                                                                    &task,
                                                                    &task_reason)) != 0) {
            local_max_size = baji_photo_mqtt_image_format_local_max_size(task.image_format);
            BAJI_PHOTO_MARK_TRACE("MQTT_TASK_REJECT code=%d reason=%s task=%s image=%s format=%s size=%lu local_max=%lu server_max=%lu heap_min=%lu",
                                  task_code,
                                  task_reason,
                                  baji_photo_diag_id_tail(task.task_id),
                                  baji_photo_diag_id_tail(task.image_id),
                                  task.image_format,
                                  (unsigned long)task.image_size,
                                  (unsigned long)local_max_size,
                                  (unsigned long)ctx->image_max_size,
                                  (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
            BAJI_PHOTO_MQTT_TRACE("image.display invalid code=%d reason=%s task=%s image=%s format=%s dims=%ux%u size=%lu expect=%lu chunk=%lu range=%d local_max=%lu server_max=%lu",
                                  task_code,
                                  task_reason,
                                  task.task_id,
                                  task.image_id,
                                  task.image_format,
                                  (unsigned int)task.image_width,
                                  (unsigned int)task.image_height,
                                  (unsigned long)task.image_size,
                                  baji_photo_mqtt_expected_image_size(&task),
                                  (unsigned long)task.chunk_size,
                                  task.support_range ? 1 : 0,
                                  (unsigned long)local_max_size,
                                  (unsigned long)ctx->image_max_size);
            (void)baji_photo_mqtt_publish_image_result(ctx,
                                                       &task,
                                                       task_code,
                                                       baji_photo_mqtt_result_message(task_code),
                                                       task_reason);
        } else if (ctx->has_pending_reply &&
                   (strcmp(ctx->pending_reply.task_id, task.task_id) == 0)) {
            ret = baji_photo_mqtt_flush_pending_reply(ctx);
            if (ret != 0) {
                BAJI_PHOTO_MQTT_TRACE("flush duplicate pending reply fail ret=%d task=%s code=%d",
                                      ret,
                                      ctx->pending_reply.task_id,
                                      ctx->pending_reply.code);
            }
        } else if (ctx->has_pending_reply) {
            BAJI_PHOTO_MQTT_TRACE("image.display busy pending_reply current=%s next=%s code=%d gen=%lu ack_wait=%d",
                                  ctx->pending_reply.task_id,
                                  task.task_id,
                                  ctx->pending_reply.code,
                                  (unsigned long)ctx->pending_reply.generation,
                                  ctx->pending_reply_waiting_ack ? 1 : 0);
            (void)baji_photo_mqtt_publish_image_result(ctx,
                                                       &task,
                                                       1001,
                                                       "task busy",
                                                       "pending_reply_in_progress");
        } else if ((ctx->has_current_task &&
                    (strcmp(ctx->current_task.task_id, task.task_id) == 0))) {
            ret = baji_photo_mqtt_publish_current_task_accepted(ctx, true);
            if (ret != 0) {
                BAJI_PHOTO_MQTT_TRACE("duplicate accepted publish fail ret=%d task=%s",
                                      ret,
                                      task.task_id);
            } else {
                BAJI_PHOTO_MQTT_TRACE("image.display duplicate active task=%s", task.task_id);
            }
        } else if (ctx->has_current_task) {
            BAJI_PHOTO_MQTT_TRACE("image.display busy current=%s next=%s",
                                  ctx->current_task.task_id,
                                  task.task_id);
            (void)baji_photo_mqtt_publish_image_result(ctx,
                                                       &task,
                                                       1001,
                                                       "task busy",
                                                       "task_in_progress");
        } else if (ctx->last_task_id[0] != '\0' &&
                   (strcmp(ctx->last_task_id, task.task_id) == 0)) {
            BAJI_PHOTO_MQTT_TRACE("image.display duplicate task=%s", task.task_id);
            (void)baji_photo_mqtt_publish_image_result(ctx,
                                                       &task,
                                                       1008,
                                                       baji_photo_mqtt_result_message(1008),
                                                       "duplicate_task");
        } else {
            baji_photo_task_state_t task_state;

            BAJI_PHOTO_MARK_TRACE("MQTT_IMAGE_TASK_RX op=%lu task=%s image=%s format=%s size=%lu range=%d heap_min=%lu",
                                  (unsigned long)task.trace_id,
                                  task.task_id,
                                  task.image_id,
                                  task.image_format,
                                  (unsigned long)task.image_size,
                                  task.support_range ? 1 : 0,
                                  (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
            if (!baji_photo_mqtt_normalize_task_format(task.image_format,
                                                       task.image_format,
                                                       sizeof(task.image_format),
                                                       NULL)) {
                BAJI_PHOTO_MQTT_TRACE("image.display normalize fail task=%s image=%s format=%s",
                                      task.task_id,
                                      task.image_id,
                                      task.image_format);
                (void)baji_photo_mqtt_publish_image_result(ctx,
                                                           &task,
                                                           1002,
                                                           baji_photo_mqtt_result_message(1002),
                                                           "normalize_failed");
                cJSON_Delete(root);
                return;
            }
            memset(&task_state, 0, sizeof(task_state));
            baji_photo_mqtt_activate_current_task(ctx, &task, &task_state, true);
            baji_photo_mqtt_request_fast_heartbeat(ctx);
            local_max_size = baji_photo_mqtt_image_format_local_max_size(ctx->current_task.image_format);
            BAJI_PHOTO_MQTT_TRACE("op=%lu task rx task=%s image=%s format=%s dims=%ux%u size=%lu expect=%lu chunk=%lu range=%d local_max=%lu server_max=%lu",
                                  (unsigned long)ctx->current_task.trace_id,
                                  ctx->current_task.task_id,
                                  ctx->current_task.image_id,
                                  ctx->current_task.image_format,
                                  (unsigned int)ctx->current_task.image_width,
                                  (unsigned int)ctx->current_task.image_height,
                                  (unsigned long)ctx->current_task.image_size,
                                  baji_photo_mqtt_expected_image_size(&ctx->current_task),
                                  (unsigned long)ctx->current_task.chunk_size,
                                  ctx->current_task.support_range ? 1 : 0,
                                  (unsigned long)local_max_size,
                                  (unsigned long)ctx->image_max_size);
            ret = baji_photo_mqtt_prepare_or_resume_task_state(&ctx->current_task,
                                                               &ctx->current_task_state);
            if (ret >= 0) {
#if BAJI_PHOTO_ENABLE_NON_RANGE_TASK_READY_PERSIST
                if (baji_photo_store_save_task_state(&ctx->current_task_state) == 0) {
                    BAJI_PHOTO_MQTT_TRACE("op=%lu state ready task=%s image=%s downloaded=%lu dims=%ux%u tmp=%s",
                                          (unsigned long)ctx->current_task.trace_id,
                                          ctx->current_task_state.task_id,
                                          ctx->current_task_state.image_id,
                                          (unsigned long)ctx->current_task_state.downloaded_size,
                                          (unsigned int)ctx->current_task_state.image_width,
                                          (unsigned int)ctx->current_task_state.image_height,
                                          ctx->current_task_state.tmp_path);
                }
#else
                if ((ret > 0) || ctx->current_task.support_range) {
                    if (baji_photo_store_save_task_state(&ctx->current_task_state) == 0) {
                        BAJI_PHOTO_MQTT_TRACE("op=%lu state ready task=%s image=%s downloaded=%lu dims=%ux%u tmp=%s",
                                              (unsigned long)ctx->current_task.trace_id,
                                              ctx->current_task_state.task_id,
                                              ctx->current_task_state.image_id,
                                              (unsigned long)ctx->current_task_state.downloaded_size,
                                              (unsigned int)ctx->current_task_state.image_width,
                                              (unsigned int)ctx->current_task_state.image_height,
                                              ctx->current_task_state.tmp_path);
                    }
                } else {
                    BAJI_PHOTO_MQTT_TRACE("op=%lu state ready persist disabled task=%s image=%s range=%d downloaded=%lu",
                                          (unsigned long)ctx->current_task.trace_id,
                                          ctx->current_task_state.task_id,
                                          ctx->current_task_state.image_id,
                                          ctx->current_task.support_range ? 1 : 0,
                                          (unsigned long)ctx->current_task_state.downloaded_size);
                }
#endif
                baji_photo_mqtt_emit_event(ctx, BAJI_PHOTO_MQTT_EVT_IMAGE_TASK, &ctx->current_task);
            } else {
                BAJI_PHOTO_MQTT_TRACE("op=%lu image.display state prepare fail task=%s",
                                      (unsigned long)ctx->current_task.trace_id,
                                      task.task_id);
                baji_photo_mqtt_finish_current_task(ctx, false);
                (void)baji_photo_mqtt_publish_image_result(ctx,
                                                           &task,
                                                           1001,
                                                           baji_photo_mqtt_result_message(1001),
                                                           "task_state_prepare_failed");
            }
        }
    } else {
        BAJI_PHOTO_MQTT_TRACE("ignore type=%s", type->valuestring);
    }

    cJSON_Delete(root);
}

static void baji_photo_mqtt_consume_message(baji_photo_mqtt_ctx_t *ctx)
{
    char topic[BAJI_PHOTO_MQTT_TOPIC_MAX];
    char payload[BAJI_PHOTO_MQTT_RX_BUF_MAX];
    bool pending = false;

    liot_rtos_enter_critical();
    if (ctx->msg_pending) {
        memcpy(topic, ctx->rx_topic, sizeof(topic));
        memcpy(payload, ctx->rx_payload, sizeof(payload));
        ctx->msg_pending = false;
        pending = true;
    }
    liot_rtos_exit_critical();

    if (pending) {
        baji_photo_mqtt_handle_message(ctx, topic, payload);
    }
}

static void baji_photo_mqtt_inpub_cb(liot_mqtt_client_t *client,
                                     void *arg,
                                     int pkt_id,
                                     const char *topic,
                                     const unsigned char *payload,
                                     unsigned short payload_len)
{
    baji_photo_mqtt_ctx_t *ctx = (baji_photo_mqtt_ctx_t *)arg;
    unsigned short copy_len;

    (void)client;
    (void)pkt_id;

    if ((ctx == NULL) || (topic == NULL) || (payload == NULL)) {
        return;
    }

    copy_len = payload_len;
    if (copy_len >= (unsigned short)sizeof(ctx->rx_payload)) {
        copy_len = (unsigned short)(sizeof(ctx->rx_payload) - 1u);
    }

    liot_rtos_enter_critical();
    if (!ctx->msg_pending) {
        (void)snprintf(ctx->rx_topic, sizeof(ctx->rx_topic), "%s", topic);
        memcpy(ctx->rx_payload, payload, copy_len);
        ctx->rx_payload[copy_len] = '\0';
        ctx->msg_pending = true;
    }
    liot_rtos_exit_critical();
}

static void baji_photo_mqtt_event_cb(liot_mqtt_client_t *client, int event, void *arg, void *data)
{
    baji_photo_mqtt_ctx_t *ctx = (baji_photo_mqtt_ctx_t *)arg;
    int status = -1;

    (void)client;

    if ((data != NULL) &&
        ((event == LIOT_MQTT_OPEN_EVENT) || (event == LIOT_MQTT_CONNECT_EVENT))) {
        status = *(int *)data;
    }

    switch (event) {
    case LIOT_MQTT_OPEN_EVENT:
    case LIOT_MQTT_CONNECT_EVENT:
        if (status == 0) {
            baji_photo_mqtt_mark_connected(ctx);
        }
        baji_photo_mqtt_sem_signal(ctx->conn_sem);
        break;
    case LIOT_MQTT_CLOSE_EVENT:
        if (ctx->running) {
            baji_photo_mqtt_mark_disconnected(ctx, BAJI_PHOTO_MQTT_STATE_BACKOFF);
        } else {
            baji_photo_mqtt_mark_disconnected(ctx, ctx->state);
        }
        if (ctx->pending_reply_waiting_ack) {
            BAJI_PHOTO_MQTT_TRACE("reply inflight reset on close task=%s code=%d gen=%lu",
                                  ctx->pending_reply.task_id,
                                  ctx->pending_reply.code,
                                  (unsigned long)ctx->pending_reply.generation);
            baji_photo_mqtt_pending_reply_ack_reset(ctx);
        }
        break;
    case LIOT_MQTT_SUB_EVENT:
    case LIOT_MQTT_PUB_EVENT:
    case LIOT_MQTT_UNSUB_EVENT:
    case LIOT_MQTT_DISCONNECT_EVENT:
        baji_photo_mqtt_sem_signal(ctx->req_sem);
        break;
    case LIOT_MQTT_RECONNECT_EVENT:
        if (ctx->pending_reply_waiting_ack) {
            BAJI_PHOTO_MQTT_TRACE("reply inflight reset on reconnect task=%s code=%d gen=%lu",
                                  ctx->pending_reply.task_id,
                                  ctx->pending_reply.code,
                                  (unsigned long)ctx->pending_reply.generation);
            baji_photo_mqtt_pending_reply_ack_reset(ctx);
        }
        BAJI_PHOTO_MQTT_TRACE("sdk reconnect event");
        break;
    default:
        break;
    }
}

static int baji_photo_mqtt_client_cleanup(baji_photo_mqtt_ctx_t *ctx)
{
    if ((ctx == NULL) || !ctx->client_inited) {
        return 0;
    }

    if (ctx->connected) {
        int ret = liot_mqtt_disconnect(&ctx->client, NULL, NULL);

        if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK) {
            (void)baji_photo_mqtt_wait_req(ctx, BAJI_PHOTO_MQTT_CONNECT_WAIT_MS);
        }
    }

    liot_mqtt_client_deinit(&ctx->client);
    ctx->client = 0;
    ctx->client_inited = false;
    baji_photo_mqtt_mark_disconnected(ctx, ctx->state);
    return 0;
}

static int baji_photo_mqtt_connect_client(baji_photo_mqtt_ctx_t *ctx)
{
    liot_mqtt_client_option opt = {0};
    char client_id[16];
    char username[16];
    int ret;

    baji_photo_mqtt_sem_drain(ctx->conn_sem);
    baji_photo_mqtt_sem_drain(ctx->req_sem);
    (void)baji_photo_mqtt_client_cleanup(ctx);

    ret = liot_mqtt_client_init_ex(&ctx->client,
                                   BAJI_PHOTO_HTTP_PDP_CID,
                                   baji_photo_mqtt_event_cb,
                                   ctx);
    if (ret != LIOT_MQTTCLIENT_SUCCESS) {
        BAJI_PHOTO_MQTT_TRACE("client init fail cid=%d ret=%d",
                              BAJI_PHOTO_HTTP_PDP_CID,
                              ret);
        return ret;
    }
    ctx->client_inited = true;

    ret = liot_mqtt_set_inpub_callback(&ctx->client, baji_photo_mqtt_inpub_cb, ctx);
    if (ret != LIOT_MQTTCLIENT_SUCCESS) {
        BAJI_PHOTO_MQTT_TRACE("set inpub callback fail ret=%d", ret);
        return ret;
    }

    opt.version = LIOT_MQTT_VERSION_4;
    opt.pdp_cid = BAJI_PHOTO_HTTP_PDP_CID;
    opt.client_id = ctx->client_id;
    opt.client_user = ctx->imei;
    opt.client_pass = BAJI_PHOTO_MQTT_PASSWORD;
    opt.clean_session = BAJI_PHOTO_MQTT_CLEAN_SESSION;
    opt.kalive_time = BAJI_PHOTO_MQTT_KEEPALIVE_S;
    opt.delivery_time = BAJI_PHOTO_MQTT_DELIVERY_TIME_S;
    opt.delivery_cnt = BAJI_PHOTO_MQTT_DELIVERY_CNT;
    opt.ping_timeout = BAJI_PHOTO_MQTT_PING_TIMEOUT_S;
    opt.ssl_enable = 0;
    opt.will_flag = 0;

    baji_photo_mqtt_mask_identifier(ctx->client_id, client_id, sizeof(client_id));
    baji_photo_mqtt_mask_identifier(ctx->imei, username, sizeof(username));
    BAJI_PHOTO_MARK_TRACE("MQTT_CONNECT_BEGIN broker=%s pdp=%d client_id=%s heap_min=%lu",
                          BAJI_PHOTO_MQTT_BROKER_URL,
                          BAJI_PHOTO_HTTP_PDP_CID,
                          client_id,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    BAJI_PHOTO_MQTT_TRACE("connect start broker=%s pdp=%d client_id=%s username=%s keepalive=%u clean=%u",
                          BAJI_PHOTO_MQTT_BROKER_URL,
                          BAJI_PHOTO_HTTP_PDP_CID,
                          client_id,
                          username,
                          (unsigned int)opt.kalive_time,
                          (unsigned int)opt.clean_session);

    ret = liot_mqtt_connect(&ctx->client,
                            BAJI_PHOTO_MQTT_BROKER_URL,
                            NULL,
                            NULL,
                            &opt,
                            NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK) {
        BAJI_PHOTO_MQTT_TRACE("connect wait async broker=%s", BAJI_PHOTO_MQTT_BROKER_URL);
        if (liot_rtos_semaphore_wait(ctx->conn_sem, BAJI_PHOTO_MQTT_CONNECT_WAIT_MS) != 0) {
            BAJI_PHOTO_MQTT_TRACE("connect wait timeout broker=%s wait_ms=%u",
                                  BAJI_PHOTO_MQTT_BROKER_URL,
                                  BAJI_PHOTO_MQTT_CONNECT_WAIT_MS);
            return LIOT_MQTTCLIENT_TIMEOUT;
        }
        if (!ctx->connected) {
            BAJI_PHOTO_MQTT_TRACE("connect wait done but sdk not connected broker=%s",
                                  BAJI_PHOTO_MQTT_BROKER_URL);
            return LIOT_MQTTCLIENT_TIMEOUT;
        }
        BAJI_PHOTO_MQTT_TRACE("connect ok broker=%s client_id=%s", BAJI_PHOTO_MQTT_BROKER_URL, client_id);
        return LIOT_MQTTCLIENT_SUCCESS;
    }

    if (ret == LIOT_MQTTCLIENT_SUCCESS) {
        baji_photo_mqtt_mark_connected(ctx);
    }
    BAJI_PHOTO_MQTT_TRACE("connect done ret=%d connected=%d broker=%s client_id=%s",
                          ret,
                          (ret == LIOT_MQTTCLIENT_SUCCESS) ? 1 : 0,
                          BAJI_PHOTO_MQTT_BROKER_URL,
                          client_id);
    return ret;
}

static int baji_photo_mqtt_subscribe_topic(baji_photo_mqtt_ctx_t *ctx, const char *topic)
{
    int ret;

    BAJI_PHOTO_MQTT_TRACE("subscribe start topic=%s qos=%u",
                          baji_photo_mqtt_topic_role(ctx, topic),
                          BAJI_PHOTO_MQTT_QOS);
    baji_photo_mqtt_sem_drain(ctx->req_sem);
    ret = liot_mqtt_sub_unsub(&ctx->client,
                              topic,
                              BAJI_PHOTO_MQTT_QOS,
                              NULL,
                              NULL,
                              1);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK) {
        ret = baji_photo_mqtt_wait_req(ctx, BAJI_PHOTO_MQTT_CONNECT_WAIT_MS);
    }
    BAJI_PHOTO_MQTT_TRACE("subscribe done topic=%s ret=%d",
                          baji_photo_mqtt_topic_role(ctx, topic),
                          ret);
    return ret;
}

static int baji_photo_mqtt_subscribe_all(baji_photo_mqtt_ctx_t *ctx)
{
    int ret;

    ret = baji_photo_mqtt_subscribe_topic(ctx, ctx->topic_down);
    if (ret != LIOT_MQTTCLIENT_SUCCESS) {
        return ret;
    }

    ret = baji_photo_mqtt_subscribe_topic(ctx, ctx->topic_cmd);
    if (ret != LIOT_MQTTCLIENT_SUCCESS) {
        return ret;
    }

    baji_photo_mqtt_mark_subscribed(ctx);
    return 0;
}

static void baji_photo_mqtt_task(void *arg)
{
    baji_photo_mqtt_ctx_t *ctx = (baji_photo_mqtt_ctx_t *)arg;
    baji_photo_mqtt_loop_view_t loop_view;
    int ret;

#if BAJI_PHOTO_MQTT_START_DELAY_MS > 0u
    BAJI_PHOTO_MQTT_TRACE("startup delay ms=%u", BAJI_PHOTO_MQTT_START_DELAY_MS);
    liot_rtos_task_sleep_ms(BAJI_PHOTO_MQTT_START_DELAY_MS);
    if (ctx->stop_requested) {
        baji_photo_mqtt_task_exit(ctx);
        return;
    }
#endif

    ret = baji_photo_mqtt_init_identity(ctx);
    if (ret != 0) {
        baji_photo_mqtt_set_fatal_error(ctx, ret);
        BAJI_PHOTO_MQTT_TRACE("identity init fatal ret=%d", ret);
        baji_photo_mqtt_task_exit(ctx);
        return;
    }
    baji_photo_mqtt_build_topics(ctx);
    baji_photo_mqtt_log_identity(ctx);
    baji_photo_mqtt_load_device_meta(ctx);
    (void)baji_photo_mqtt_load_pending_reply(ctx);
    baji_photo_mqtt_restore_saved_task_state(ctx);
    ctx->heartbeat_interval_s = BAJI_PHOTO_MQTT_HEARTBEAT_DEFAULT_S;
    ctx->heartbeat_countdown_s = 0u;
    ctx->reconnect_backoff_s = BAJI_PHOTO_MQTT_RECONNECT_MIN_S;

    while (!ctx->stop_requested) {
        int mqtt_state = ctx->client_inited ? liot_mqtt_client_state(&ctx->client) : MQTT_CONN_DEFAULT;

        baji_photo_mqtt_consume_message(ctx);
        loop_view = baji_photo_mqtt_get_loop_view(ctx, mqtt_state);

        if (loop_view.session_ready) {
            if (loop_view.needs_subscribe) {
                ret = baji_photo_mqtt_process_post_connect_setup(ctx);
            } else if (loop_view.has_display_result) {
                baji_photo_mqtt_process_display_result(ctx);
            } else if (loop_view.has_pending_reply || baji_photo_mqtt_has_pending_reply_work(ctx)) {
                ret = baji_photo_mqtt_flush_pending_reply(ctx);
                if (ret != 0) {
                    BAJI_PHOTO_MQTT_TRACE("flush pending reply fail ret=%d task=%s code=%d",
                                          ret,
                                          ctx->pending_reply.task_id,
                                          ctx->pending_reply.code);
                }
            } else if (loop_view.waiting_display_ack) {
                /* Wait for the LVGL screen-loaded confirmation before final reply. */
            } else if (loop_view.has_current_task) {
                baji_photo_mqtt_execute_current_task(ctx);
            } else if (loop_view.heartbeat_due) {
                baji_photo_mqtt_process_idle_heartbeat(ctx);
            } else {
                baji_photo_mqtt_tick_heartbeat_countdown(ctx);
            }

            baji_photo_mqtt_tick_bind_token_refresh(ctx);

            liot_rtos_task_sleep_s(1u);
            continue;
        }

        baji_photo_mqtt_begin_connect_attempt(ctx);

        ret = baji_photo_mqtt_connect_client(ctx);
        if (ret == 0) {
            baji_photo_mqtt_finish_connect_success(ctx);
            liot_rtos_task_sleep_s(1u);
            continue;
        }

        baji_photo_mqtt_set_runtime_state(ctx, BAJI_PHOTO_MQTT_STATE_BACKOFF);
        BAJI_PHOTO_MQTT_TRACE("direct connect fail ret=%d observe network before retry", ret);

        (void)baji_photo_network_prepare();
        baji_photo_mqtt_begin_connect_attempt(ctx);
        ret = baji_photo_mqtt_connect_client(ctx);
        if (ret == 0) {
            baji_photo_mqtt_finish_connect_success(ctx);
            liot_rtos_task_sleep_s(1u);
            continue;
        }
        BAJI_PHOTO_MQTT_TRACE("connect fail ret=%d backoff=%lu",
                              ret,
                              (unsigned long)ctx->reconnect_backoff_s);

        baji_photo_mqtt_wait_reconnect_backoff(ctx);
    }

    baji_photo_mqtt_task_exit(ctx);
}

int baji_photo_mqtt_start(void)
{
    LiotOSStatus_t ret;
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();

    if (ctx->running) {
        return 0;
    }

    ret = baji_photo_mqtt_init_once(ctx);
    if (ret != 0) {
        return ret;
    }

    baji_photo_mqtt_reset_runtime(ctx);
    ctx->running = true;

    ret = liot_rtos_task_create(&ctx->task,
                                BAJI_PHOTO_MQTT_TASK_STACK,
                                BAJI_PHOTO_MQTT_TASK_PRIO,
                                "baji_mqtt",
                                baji_photo_mqtt_task,
                                ctx);
    if (ret != 0) {
        ctx->task = NULL;
        ctx->running = false;
        ctx->stop_requested = false;
        return ret;
    }

    return 0;
}

int baji_photo_mqtt_stop(void)
{
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();

    if (!ctx->running) {
        return 0;
    }
    ctx->stop_requested = true;
    return 0;
}

int baji_photo_mqtt_request_bind_token(void)
{
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();
    int ret = baji_photo_mqtt_request_bind_token_internal(ctx, "manual_request");

    if (ret != 0) {
        baji_photo_mqtt_emit_bind_notice(ctx,
                                         BAJI_PHOTO_BIND_NOTICE_FAIL,
                                         "QR request failed, retrying");
    }
    return ret;
}

int baji_photo_mqtt_request_unbind(void)
{
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();
    int ret;

    if (!ctx->connected || !ctx->subscribed)
        return LIOT_MQTTCLIENT_NOT_CONNECT;
    if (ctx->bind_status != BAJI_PHOTO_BIND_BOUND)
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    ret = baji_photo_mqtt_publish_unbind_request(ctx);
    BAJI_PHOTO_MQTT_TRACE("unbind request ret=%d", ret);
    return ret;
}
int baji_photo_mqtt_set_event_cb(baji_photo_mqtt_event_cb_t cb, void *event_ctx)
{
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();

    baji_photo_mqtt_set_event_sink(ctx, cb, event_ctx);
    return 0;
}

int baji_photo_mqtt_notify_display_result(const baji_photo_mqtt_display_result_t *result)
{
    baji_photo_mqtt_ctx_t *ctx = baji_photo_mqtt_default();

    if (result == NULL) {
        return LIOT_MQTTCLIENT_INVALID_PARAM;
    }

    liot_rtos_enter_critical();
    ctx->display_result = *result;
    ctx->display_result_pending = true;
    liot_rtos_exit_critical();
    return 0;
}

bool baji_photo_mqtt_is_running(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.running;
}

bool baji_photo_mqtt_is_connected(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.connected && snapshot.subscribed;
}

bool baji_photo_mqtt_has_active_task(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.has_current_task ||
           snapshot.has_pending_reply ||
           snapshot.pending_reply_waiting_ack;
}

liot_task_t baji_photo_mqtt_get_task_ref(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.task;
}

baji_photo_mqtt_state_t baji_photo_mqtt_get_state(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.state;
}

baji_photo_bind_status_t baji_photo_mqtt_get_bind_status(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.bind_status;
}

int baji_photo_mqtt_get_fatal_error(void)
{
    baji_photo_mqtt_runtime_snapshot_t snapshot =
        baji_photo_mqtt_get_runtime_snapshot(baji_photo_mqtt_default());

    return snapshot.fatal_error;
}

#else

int baji_photo_mqtt_start(void)
{
    return 0;
}

int baji_photo_mqtt_stop(void)
{
    return 0;
}

int baji_photo_mqtt_request_bind_token(void)
{
    return 0;
}

int baji_photo_mqtt_request_unbind(void)
{
    return -1;
}
int baji_photo_mqtt_set_event_cb(baji_photo_mqtt_event_cb_t cb, void *ctx)
{
    (void)cb;
    (void)ctx;
    return 0;
}

int baji_photo_mqtt_notify_display_result(const baji_photo_mqtt_display_result_t *result)
{
    (void)result;
    return 0;
}

bool baji_photo_mqtt_is_running(void)
{
    return false;
}

bool baji_photo_mqtt_is_connected(void)
{
    return false;
}

bool baji_photo_mqtt_has_active_task(void)
{
    return false;
}

liot_task_t baji_photo_mqtt_get_task_ref(void)
{
    return NULL;
}

baji_photo_mqtt_state_t baji_photo_mqtt_get_state(void)
{
    return BAJI_PHOTO_MQTT_STATE_STOPPED;
}

baji_photo_bind_status_t baji_photo_mqtt_get_bind_status(void)
{
    return BAJI_PHOTO_BIND_UNKNOWN;
}

int baji_photo_mqtt_get_fatal_error(void)
{
    return 0;
}

#endif
