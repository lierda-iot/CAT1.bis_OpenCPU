#include "baji_photo_http.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "liot_datacall.h"
#include "liot_external_flash_fs.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_rtc.h"
#include "mbedtls/md5.h"
#include "mm_video_if.h"

#include "baji_photo_diag.h"
#include "baji_photo_store.h"
#include "baji_photo_vpu_img.h"

#define BAJI_PHOTO_HTTP_URL_MAX            256u
#define BAJI_PHOTO_HTTP_RESP_WAIT_GRACE_MS 1000u
#define BAJI_PHOTO_HTTP_WAIT_STEP_MS       1000u
#define BAJI_PHOTO_HTTP_RELEASE_GRACE_MS   100u
#define BAJI_PHOTO_HTTP_RELEASE_SETTLE_MS  1000u
#define BAJI_PHOTO_HTTP_CTX_REUSE_WAIT_MS  1500u
#define BAJI_PHOTO_HTTP_RANGE_HEADER_MAX   64u
#define BAJI_PHOTO_HTTP_RTC_READY_EPOCH_S  1704067200u
#define BAJI_PHOTO_MARK_TRACE(fmt, ...) liot_trace("\n[baji_mark] " fmt "\n", ##__VA_ARGS__)

typedef struct {
    liot_sem_t sem_resp;
    liot_sem_t sem_close;
    char *buf;
    unsigned int cap;
    unsigned int len;
    LFILE_EXT file_fd;
    bool write_to_file;
    int status_code;
    int content_len;
    int content_range;
    int chunk_encode;
    int err;
    bool resp_done;
    bool saw_status;
    bool saw_complete;
    bool saw_close;
    bool stop_requested;
    bool close_wait_required;
} baji_photo_http_ctx_t;

typedef struct {
    unsigned int len;
    int status_code;
    bool saw_status;
    bool saw_complete;
} baji_photo_http_ctx_read_view_t;

typedef struct {
    unsigned int chunk_len;
    int http_status;
    uint32_t start_offset;
    uint32_t request_size;
    bool range_request;
    bool partial_accepted;
} baji_photo_http_chunk_request_result_t;

typedef enum {
    BAJI_PHOTO_HTTP_PLAIN_STEP_RANGE_FALLBACK = 0,
    BAJI_PHOTO_HTTP_PLAIN_STEP_CHUNK_READY,
    BAJI_PHOTO_HTTP_PLAIN_STEP_CHUNK_ERROR,
} baji_photo_http_plain_chunk_step_t;

typedef enum {
    BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CLEANUP = 0,
    BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CONTINUE,
    BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_RESTART,
} baji_photo_http_chunk_error_step_t;

typedef enum {
    BAJI_PHOTO_HTTP_FINALIZE_STEP_CLEANUP = 0,
    BAJI_PHOTO_HTTP_FINALIZE_STEP_RESTART,
    BAJI_PHOTO_HTTP_FINALIZE_STEP_SUCCESS,
} baji_photo_http_finalize_step_t;

typedef enum {
    BAJI_PHOTO_NET_OBS_NOT_READY = 0,
    BAJI_PHOTO_NET_OBS_PROBE_ALLOWED,
    BAJI_PHOTO_NET_OBS_ACTIVE_READY,
} baji_photo_net_obs_state_t;

typedef struct {
    bool profile_active;
    bool rtc_ready;
    bool cached_http_success;
    bool last_http_ok;
    int info_ret;
    int cid;
    int ip_version;
    int v4_state;
    int last_http_ret;
    int last_http_status;
    baji_photo_net_obs_state_t state;
    const char *reason;
} baji_photo_net_observation_t;

typedef struct {
    bool busy;
    bool ready;
    bool last_http_ok;
    int last_http_ret;
    int last_http_status;
    liot_task_t sync_task;
    baji_photo_sync_state_t state;
    baji_photo_sync_done_cb_t done_cb;
    baji_photo_sync_progress_cb_t progress_cb;
    void *done_ctx;
    baji_photo_http_ctx_t http_ctx;
    bool http_sem_ready;
} baji_photo_net_service_t;

static baji_photo_net_service_t s_baji_photo_net = {
    .last_http_ret = LIOT_HTTPC_SUCCESS,
    .last_http_status = -1,
    .state = BAJI_PHOTO_SYNC_STATE_IDLE,
};

static baji_photo_net_service_t *baji_photo_net_default(void);
static baji_photo_http_ctx_t *baji_photo_http_ctx_default(void);
static int baji_photo_http_perform_url(const char *url_str,
                                       const char *request_header,
                                       bool text_response,
                                       unsigned int *out_len);
static int baji_photo_http_get_url(const char *url_str,
                                   char *buf,
                                   unsigned int cap,
                                   bool text_response,
                                   unsigned int *out_len);
static int baji_photo_http_get_url_retry(const char *url_str,
                                         char *buf,
                                         unsigned int cap,
                                         bool text_response,
                                         unsigned int *out_len,
                                         unsigned int max_attempts);
static int baji_photo_find_item_by_id(const baji_photo_manifest_item_t *items,
                                      unsigned int count,
                                      const char *id);
static bool baji_photo_local_matches_remote(const baji_photo_manifest_item_t *local,
                                            const baji_photo_manifest_item_t *remote);
static bool baji_photo_index_item_equals(const baji_photo_manifest_item_t *lhs,
                                         const baji_photo_manifest_item_t *rhs);
static bool baji_photo_index_items_equal(const baji_photo_manifest_item_t *lhs,
                                         const baji_photo_manifest_item_t *rhs,
                                         unsigned int count);
static bool baji_photo_true_color_dims_valid(uint32_t width, uint32_t height);
static uint32_t baji_photo_true_color_data_size(uint32_t width, uint32_t height);
static bool baji_photo_http_parse_task_format(const baji_photo_image_task_t *task,
                                              baji_photo_format_t *out_format);
static uint32_t baji_photo_http_task_data_offset(baji_photo_format_t format,
                                                 uint32_t payload_offset);
static uint32_t baji_photo_http_raw_format_max_size(baji_photo_format_t format);
static uint32_t baji_photo_http_task_stored_size(baji_photo_format_t format,
                                                 uint32_t payload_size);
static int baji_photo_http_validate_raw_file_payload(LFILE_EXT fd,
                                                     const baji_photo_manifest_item_t *item);
static void baji_photo_http_result_reset(baji_photo_http_image_result_t *out_result);
static void baji_photo_network_observe(const baji_photo_net_service_t *service,
                                       baji_photo_net_observation_t *obs);
static int baji_photo_network_prepare_trace(uint32_t trace_id,
                                            uint32_t retry_deadline_ms,
                                            uint32_t next_retry_ms);
static bool baji_photo_http_transport_error_is_network_like(int ret);
static int baji_photo_http_perform_url_once(const char *url_str,
                                            const char *request_header,
                                            bool text_response,
                                            unsigned int *out_len,
                                            int pdp_cid,
                                            const char **out_stage);
#define BAJI_PHOTO_HTTP_TRACE(fmt, ...) liot_trace("[baji_http] " fmt "\n", ##__VA_ARGS__)
static void baji_photo_http_marker(const char *stage,
                                   const baji_photo_image_task_t *task,
                                   unsigned int downloaded_size);
static void baji_photo_http_marker_result(const char *stage,
                                          const baji_photo_image_task_t *task,
                                          const baji_photo_task_state_t *state,
                                          const baji_photo_http_image_result_t *result,
                                          int ret);
static int baji_photo_http_wait_until(liot_sem_t sem, const bool *flag, unsigned int timeout_ms);
static int baji_photo_validate_gif_payload(const baji_photo_manifest_item_t *item,
                                           const uint8_t *data,
                                           unsigned int len);
static int baji_photo_http_store_fsync(LFILE_EXT fd);
static int baji_photo_http_store_close(LFILE_EXT fd);
static int baji_photo_http_store_remove(const char *path);
static int baji_photo_http_commit_tmp_file(const baji_photo_image_task_t *task,
                                           const baji_photo_task_state_t *state);
static void baji_photo_http_chunk_request_result_reset(
    baji_photo_http_chunk_request_result_t *result);
static void baji_photo_http_reset_download_state(baji_photo_task_state_t *state);
static void baji_photo_http_note_chunk_error(baji_photo_task_state_t *state);
static uint32_t baji_photo_http_progress_percent(uint32_t downloaded, uint32_t total);
static void baji_photo_http_trace_download_progress(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    bool range_resumed);
static int baji_photo_http_commit_chunk_progress(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT fd,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    bool range_resumed,
    baji_photo_http_image_progress_cb_t progress_cb,
    void *progress_ctx,
    baji_photo_http_image_result_t *out_result);
static void baji_photo_http_prepare_restart_download(baji_photo_task_state_t *state,
                                                     LFILE_EXT *fd,
                                                     bool bump_retry_count);
static int baji_photo_http_finalize_download_success(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    const baji_photo_manifest_item_t *out_item,
    baji_photo_http_image_result_t *out_result);
static int baji_photo_http_finalize_download_cleanup(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    baji_photo_http_image_result_t *out_result);
static int baji_photo_http_prepare_tmp_file(const baji_photo_image_task_t *task,
                                            baji_photo_task_state_t *state,
                                            LFILE_EXT *out_fd,
                                            bool *out_range_resumed);
static int baji_photo_http_request_plain_chunk(const baji_photo_image_task_t *task,
                                               uint32_t start_offset,
                                               LFILE_EXT fd,
                                               baji_photo_http_chunk_request_result_t *result);
static int baji_photo_http_request_range_chunk(const baji_photo_image_task_t *task,
                                               uint32_t start_offset,
                                               LFILE_EXT fd,
                                               baji_photo_http_chunk_request_result_t *result);
static int baji_photo_http_verify_tmp_file(LFILE_EXT fd,
                                           const baji_photo_image_task_t *task,
                                           baji_photo_manifest_item_t *out_item);
static int baji_photo_http_request_download_chunk(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    const baji_photo_http_ctx_t *http_ctx,
    LFILE_EXT fd,
    baji_photo_http_chunk_request_result_t *result);
static int baji_photo_http_prepare_download_preflight(const baji_photo_image_task_t *task,
                                                      const baji_photo_task_state_t *state,
                                                      baji_photo_format_t format);
static int baji_photo_http_prepare_download_attempt(const baji_photo_image_task_t *task,
                                                    baji_photo_task_state_t *state,
                                                    LFILE_EXT *fd,
                                                    baji_photo_http_image_result_t *out_result);
static int baji_photo_http_process_ready_chunk(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT fd,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    unsigned int *failure_count,
    bool range_resumed,
    baji_photo_http_image_progress_cb_t progress_cb,
    void *progress_ctx,
    baji_photo_http_image_result_t *out_result);
static baji_photo_http_chunk_error_step_t baji_photo_http_process_chunk_error_step(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    int http_status,
    bool *allow_full_restart,
    unsigned int *failure_count);
static baji_photo_http_finalize_step_t baji_photo_http_finalize_verified_download(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    bool *allow_full_restart,
    baji_photo_manifest_item_t *out_item,
    baji_photo_http_image_result_t *out_result,
    int *out_ret);
static baji_photo_http_plain_chunk_step_t baji_photo_http_process_plain_chunk_result(
    const baji_photo_image_task_t *task,
    const baji_photo_http_ctx_t *http_ctx,
    int ret,
    const baji_photo_http_chunk_request_result_t *chunk_result);
static bool baji_photo_http_try_restart_after_verify_failure(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    bool *allow_full_restart);
static bool baji_photo_http_error_retryable(int ret, int status_code);
static bool baji_photo_http_error_should_wait_network(int ret);
static bool baji_photo_http_process_range_error_step(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    int ret,
    int http_status,
    unsigned int *failure_count);
static void baji_photo_http_ctx_get_read_view(const baji_photo_http_ctx_t *ctx,
                                              baji_photo_http_ctx_read_view_t *view);
static bool baji_photo_http_ctx_has_no_response(const baji_photo_http_ctx_t *ctx);
static void baji_photo_network_record_http_result(baji_photo_net_service_t *service,
                                                  int ret,
                                                  int status_code,
                                                  bool ok);
static void baji_photo_sync_report_progress(baji_photo_net_service_t *service,
                                            baji_photo_sync_state_t state,
                                            unsigned int current,
                                            unsigned int total);
static void baji_photo_sync_service_reset_callbacks(baji_photo_net_service_t *service);
static void baji_photo_sync_service_set_idle(baji_photo_net_service_t *service);

static baji_photo_net_service_t *baji_photo_net_default(void)
{
    return &s_baji_photo_net;
}

static baji_photo_http_ctx_t *baji_photo_http_ctx_default(void)
{
    return &baji_photo_net_default()->http_ctx;
}

static void baji_photo_http_signal(liot_sem_t sem)
{
    if (sem != NULL) {
        liot_rtos_semaphore_release(sem);
    }
}

static void baji_photo_http_drain_sem(liot_sem_t sem)
{
    if (sem == NULL) {
        return;
    }

    while (liot_rtos_semaphore_wait(sem, LIOT_NO_WAIT) == 0) {
    }
}

static int baji_photo_http_ctx_wait_reusable(baji_photo_http_ctx_t *ctx)
{
    int ret;

    if ((ctx == NULL) || (ctx->sem_close == NULL) ||
        !ctx->close_wait_required || ctx->saw_close) {
        return LIOT_HTTPC_SUCCESS;
    }

    BAJI_PHOTO_HTTP_TRACE("ctx reuse wait start done=%d close=%d stop=%d status=%d recv=%u",
                          ctx->resp_done ? 1 : 0,
                          ctx->saw_close ? 1 : 0,
                          ctx->stop_requested ? 1 : 0,
                          ctx->status_code,
                          ctx->len);
    BAJI_PHOTO_MARK_TRACE("HTTP_CTX_REUSE_WAIT done=%d close=%d stop=%d status=%d recv=%u heap_min=%lu",
                          ctx->resp_done ? 1 : 0,
                          ctx->saw_close ? 1 : 0,
                          ctx->stop_requested ? 1 : 0,
                          ctx->status_code,
                          ctx->len,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    ret = baji_photo_http_wait_until(ctx->sem_close,
                                     &ctx->saw_close,
                                     BAJI_PHOTO_HTTP_CTX_REUSE_WAIT_MS);
    BAJI_PHOTO_HTTP_TRACE("ctx reuse wait done ret=%d close=%d status=%d recv=%u",
                          ret,
                          ctx->saw_close ? 1 : 0,
                          ctx->status_code,
                          ctx->len);
    if ((ret == LIOT_HTTPC_SUCCESS) && ctx->saw_close) {
        ctx->close_wait_required = false;
        return LIOT_HTTPC_SUCCESS;
    }

    BAJI_PHOTO_MARK_TRACE("HTTP_CTX_REUSE_BLOCK ret=%d done=%d close=%d stop=%d status=%d recv=%u heap_min=%lu",
                          ret,
                          ctx->resp_done ? 1 : 0,
                          ctx->saw_close ? 1 : 0,
                          ctx->stop_requested ? 1 : 0,
                          ctx->status_code,
                          ctx->len,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    return ret;
}

static void baji_photo_http_chunk_request_result_reset(
    baji_photo_http_chunk_request_result_t *result)
{
    if (result == NULL) {
        return;
    }

    result->chunk_len = 0u;
    result->http_status = -1;
    result->start_offset = 0u;
    result->request_size = 0u;
    result->range_request = false;
    result->partial_accepted = false;
}

static void baji_photo_http_reset_download_state(baji_photo_task_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->downloaded_size = 0u;
    state->completed = false;
    state->verified = false;
}

static void baji_photo_http_note_chunk_error(baji_photo_task_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->retry_count < 0xFFu) {
        state->retry_count += 1u;
    }
#if BAJI_PHOTO_ENABLE_HTTP_RETRY_COUNT_PERSIST
    (void)baji_photo_store_save_task_state(state);
#endif
}

static uint32_t baji_photo_http_progress_percent(uint32_t downloaded, uint32_t total)
{
    if (total == 0u) {
        return 0u;
    }
    if (downloaded >= total) {
        return 100u;
    }
    return (uint32_t)((((uint64_t)downloaded) * 100u) / (uint64_t)total);
}

static const char *baji_photo_http_task_state_checkpoint_reason(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    uint32_t previous_downloaded_size)
{
    uint32_t prev_progress;
    uint32_t curr_progress;
    uint32_t bytes_threshold = BAJI_PHOTO_TASK_STATE_CHECKPOINT_BYTES;
    uint32_t percent_threshold = BAJI_PHOTO_TASK_STATE_CHECKPOINT_PERCENT;

    if ((task == NULL) || (state == NULL)) {
        return "invalid";
    }
    if (state->completed) {
        return "final";
    }

#if !BAJI_PHOTO_ENABLE_NON_RANGE_TASK_CHECKPOINT_PERSIST
    if (!task->support_range) {
        return NULL;
    }
#endif

    if (bytes_threshold == 0u) {
        return "bytes";
    }
    if ((previous_downloaded_size / bytes_threshold) !=
        (state->downloaded_size / bytes_threshold)) {
        return "bytes";
    }

    if (percent_threshold == 0u) {
        return "percent";
    }
    prev_progress = baji_photo_http_progress_percent(previous_downloaded_size, task->image_size);
    curr_progress = baji_photo_http_progress_percent(state->downloaded_size, task->image_size);
    if ((prev_progress / percent_threshold) != (curr_progress / percent_threshold)) {
        return "percent";
    }

    return NULL;
}

static void baji_photo_http_trace_download_progress(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    bool range_resumed)
{
    uint32_t downloaded;
    uint32_t total;
    uint32_t progress;
    uint32_t remain;
    uint32_t recv_end;
    uint32_t req_end;

    if ((task == NULL) || (state == NULL) || (chunk_result == NULL)) {
        return;
    }

    downloaded = state->downloaded_size;
    total = task->image_size;
    progress = baji_photo_http_progress_percent(downloaded, total);
    remain = (downloaded < total) ? (total - downloaded) : 0u;
    recv_end = (chunk_result->chunk_len > 0u) ?
                   (chunk_result->start_offset + chunk_result->chunk_len - 1u) :
                   chunk_result->start_offset;
    req_end = (chunk_result->request_size > 0u) ?
                  (chunk_result->start_offset + chunk_result->request_size - 1u) :
                  recv_end;
    BAJI_PHOTO_HTTP_TRACE(
        "op=%lu download progress task=%s mode=%s partial=%d write=%u recv_range=%lu-%lu req_range=%lu-%lu total=%lu/%lu progress=%lu%% remain=%lu status=%d resumed=%d",
        (unsigned long)task->trace_id,
        task->task_id,
        chunk_result->range_request ? "range" : "plain",
        chunk_result->partial_accepted ? 1 : 0,
        chunk_result->chunk_len,
        (unsigned long)chunk_result->start_offset,
        (unsigned long)recv_end,
        (unsigned long)chunk_result->start_offset,
        (unsigned long)req_end,
        (unsigned long)downloaded,
        (unsigned long)total,
        (unsigned long)progress,
        (unsigned long)remain,
        chunk_result->http_status,
        range_resumed ? 1 : 0);
}

static int baji_photo_http_commit_chunk_progress(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT fd,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    bool range_resumed,
    baji_photo_http_image_progress_cb_t progress_cb,
    void *progress_ctx,
    baji_photo_http_image_result_t *out_result)
{
    const char *checkpoint_reason;
    uint32_t previous_downloaded_size;
    int ret;

    if ((task == NULL) || (state == NULL) || (chunk_result == NULL) || (out_result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    previous_downloaded_size = state->downloaded_size;
    state->downloaded_size += chunk_result->chunk_len;
    state->completed = (state->downloaded_size == task->image_size);
    state->verified = false;
    state->retry_count = 0u;
    out_result->downloaded_size = state->downloaded_size;
    if (progress_cb != NULL) {
        progress_cb(task,
                    state->downloaded_size,
                    task->image_size,
                    range_resumed,
                    progress_ctx);
    }

    checkpoint_reason = baji_photo_http_task_state_checkpoint_reason(task,
                                                                     state,
                                                                     previous_downloaded_size);
    if (checkpoint_reason != NULL) {
        ret = baji_photo_http_store_fsync(fd);
        if (ret != LIOT_EXTFLASH_OK) {
            return LIOT_EXTFLASH_SYNC_FAIL;
        }
        ret = baji_photo_store_save_task_state(state);
        if (ret != 0) {
            return ret;
        }
        BAJI_PHOTO_HTTP_TRACE("task checkpoint op=%lu task=%s image=%s reason=%s downloaded=%lu total=%lu",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              checkpoint_reason,
                              (unsigned long)state->downloaded_size,
                              (unsigned long)task->image_size);
    }
    baji_photo_http_trace_download_progress(task, state, chunk_result, range_resumed);
    return 0;
}

static void baji_photo_http_prepare_restart_download(baji_photo_task_state_t *state,
                                                     LFILE_EXT *fd,
                                                     bool bump_retry_count)
{
    bool should_persist = true;

    if ((state == NULL) || (fd == NULL)) {
        return;
    }

    baji_photo_http_reset_download_state(state);
    if (bump_retry_count && (state->retry_count < 0xFFu)) {
        state->retry_count += 1u;
    }
    (void)baji_photo_http_store_close(*fd);
    *fd = 0;
    (void)baji_photo_http_store_remove(state->tmp_path);
#if !BAJI_PHOTO_ENABLE_NON_RANGE_TASK_RESTART_PERSIST
    should_persist = state->support_range;
#endif
    if (should_persist) {
        (void)baji_photo_store_save_task_state(state);
    } else {
        BAJI_PHOTO_HTTP_TRACE("skip restart state persist task=%s image=%s range=%d retry=%u",
                              state->task_id,
                              state->image_id,
                              state->support_range ? 1 : 0,
                              (unsigned int)state->retry_count);
    }
}

static int baji_photo_http_finalize_download_success(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    const baji_photo_manifest_item_t *out_item,
    baji_photo_http_image_result_t *out_result)
{
    int ret;

    if ((task == NULL) || (state == NULL) || (fd == NULL) ||
        (out_item == NULL) || (out_result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    state->completed = true;
    state->verified = true;
    ret = baji_photo_store_save_task_state(state);
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_http_store_close(*fd);
    *fd = 0;
    if (ret != LIOT_EXTFLASH_OK) {
        return LIOT_EXTFLASH_CLOSE_FAIL;
    }

    ret = baji_photo_http_commit_tmp_file(task, state);
    if (ret != 0) {
        return ret;
    }
    baji_photo_http_marker("HTTP_COMMIT_OK", task, task->image_size);

    out_result->downloaded_size = task->image_size;
    BAJI_PHOTO_HTTP_TRACE("op=%lu task success task=%s image=%s status=%d downloaded=%lu progress=%lu%% remain=0 hdr=%ux%u file=%lu crc=%08lX",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          out_result->http_status,
                          (unsigned long)out_result->downloaded_size,
                          (unsigned long)baji_photo_http_progress_percent(out_result->downloaded_size,
                                                                          task->image_size),
                          (unsigned int)out_item->width,
                          (unsigned int)out_item->height,
                          (unsigned long)out_item->file_size,
                          (unsigned long)out_item->crc32);
    baji_photo_http_marker("HTTP_RETURN_OK", task, task->image_size);
    baji_photo_http_marker_result("HTTP_TASK_OK", task, state, out_result, 0);
    return 0;
}

static int baji_photo_http_finalize_download_cleanup(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    baji_photo_http_image_result_t *out_result)
{
    if ((task == NULL) || (state == NULL) || (fd == NULL) || (out_result == NULL)) {
        return ret;
    }

    out_result->downloaded_size = state->downloaded_size;
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu image task fail task=%s image=%s ret=%d status=%d downloaded=%lu progress=%lu%% remain=%lu retry=%u",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              ret,
                              out_result->http_status,
                              (unsigned long)state->downloaded_size,
                              (unsigned long)baji_photo_http_progress_percent(state->downloaded_size,
                                                                              task->image_size),
                              (unsigned long)((state->downloaded_size < task->image_size) ?
                                                  (task->image_size - state->downloaded_size) :
                                                  0u),
                              (unsigned int)state->retry_count);
        baji_photo_http_marker_result("HTTP_TASK_FAIL", task, state, out_result, ret);
    }
    if (*fd > 0) {
        int close_ret = baji_photo_http_store_close(*fd);

        *fd = 0;
        if ((close_ret != LIOT_EXTFLASH_OK) && (ret == 0)) {
            ret = LIOT_EXTFLASH_CLOSE_FAIL;
        }
    }
    return ret;
}

static baji_photo_http_plain_chunk_step_t baji_photo_http_process_plain_chunk_result(
    const baji_photo_image_task_t *task,
    const baji_photo_http_ctx_t *http_ctx,
    int ret,
    const baji_photo_http_chunk_request_result_t *chunk_result)
{
    baji_photo_http_ctx_read_view_t http_view;

    if ((task == NULL) || (chunk_result == NULL)) {
        return BAJI_PHOTO_HTTP_PLAIN_STEP_CHUNK_ERROR;
    }

    if (ret == 0) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu plain accepted task=%s status=%d bytes=%u",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              chunk_result->http_status,
                              chunk_result->chunk_len);
        if (chunk_result->chunk_len > 0u) {
            return BAJI_PHOTO_HTTP_PLAIN_STEP_CHUNK_READY;
        }
        return BAJI_PHOTO_HTTP_PLAIN_STEP_RANGE_FALLBACK;
    }

    baji_photo_http_ctx_get_read_view(http_ctx, &http_view);
    if (baji_photo_http_ctx_has_no_response(http_ctx)) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu plain fallback range task=%s ret=%d status=%d recv=%u",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              ret,
                              chunk_result->http_status,
                              http_view.len);
        return BAJI_PHOTO_HTTP_PLAIN_STEP_RANGE_FALLBACK;
    }

    BAJI_PHOTO_HTTP_TRACE("op=%lu plain fail task=%s ret=%d status=%d recv=%u",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          ret,
                          chunk_result->http_status,
                          http_view.len);
    return BAJI_PHOTO_HTTP_PLAIN_STEP_CHUNK_ERROR;
}

static int baji_photo_http_request_download_chunk(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    const baji_photo_http_ctx_t *http_ctx,
    LFILE_EXT fd,
    baji_photo_http_chunk_request_result_t *result)
{
    int ret;

    if ((task == NULL) || (state == NULL) || (fd <= 0) || (result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    if (state->downloaded_size != 0u) {
        return baji_photo_http_request_range_chunk(task,
                                                   state->downloaded_size,
                                                   fd,
                                                   result);
    }

    ret = baji_photo_http_request_plain_chunk(task, 0u, fd, result);
    if (baji_photo_http_process_plain_chunk_result(task,
                                                   http_ctx,
                                                   ret,
                                                   result) != BAJI_PHOTO_HTTP_PLAIN_STEP_RANGE_FALLBACK) {
        return ret;
    }

    return baji_photo_http_request_range_chunk(task,
                                               state->downloaded_size,
                                               fd,
                                               result);
}

static int baji_photo_http_prepare_download_preflight(const baji_photo_image_task_t *task,
                                                      const baji_photo_task_state_t *state,
                                                      baji_photo_format_t format)
{
    int free_size;
    int ret;

    if ((task == NULL) || (state == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_network_prepare_trace(task->trace_id, 0u, 0u);
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu network prepare fail task=%s ret=%d",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              ret);
        return ret;
    }
    ret = baji_photo_store_mount();
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu store mount fail task=%s ret=%d",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              ret);
        return ret;
    }

    free_size = baji_photo_store_free_size();
    if ((state->downloaded_size == 0u) &&
        (free_size >= 0) &&
        ((uint32_t)free_size < (baji_photo_http_task_stored_size(format, task->image_size) +
                                16u * 1024u))) {
        return LIOT_EXTFLASH_NO_SPACE;
    }
    return 0;
}

static int baji_photo_http_prepare_download_attempt(const baji_photo_image_task_t *task,
                                                    baji_photo_task_state_t *state,
                                                    LFILE_EXT *fd,
                                                    baji_photo_http_image_result_t *out_result)
{
    int ret;

    if ((task == NULL) || (state == NULL) || (fd == NULL) || (out_result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_http_prepare_tmp_file(task, state, fd, &out_result->range_resumed);
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu tmp prepare fail task=%s ret=%d downloaded=%lu tmp=%s",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              ret,
                              (unsigned long)state->downloaded_size,
                              state->tmp_path);
        return ret;
    }
    out_result->range_resumed = out_result->range_resumed || (state->downloaded_size > 0u);
    return 0;
}

static int baji_photo_http_process_ready_chunk(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT fd,
    const baji_photo_http_chunk_request_result_t *chunk_result,
    unsigned int *failure_count,
    bool range_resumed,
    baji_photo_http_image_progress_cb_t progress_cb,
    void *progress_ctx,
    baji_photo_http_image_result_t *out_result)
{
    if ((task == NULL) || (state == NULL) || (fd <= 0) || (chunk_result == NULL) ||
        (failure_count == NULL) || (out_result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (chunk_result->chunk_len == 0u) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }

    *failure_count = 0u;
    return baji_photo_http_commit_chunk_progress(task,
                                                 state,
                                                 fd,
                                                 chunk_result,
                                                 range_resumed,
                                                 progress_cb,
                                                 progress_ctx,
                                                 out_result);
}

static baji_photo_http_chunk_error_step_t baji_photo_http_process_chunk_error_step(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    int http_status,
    bool *allow_full_restart,
    unsigned int *failure_count)
{
    if ((task == NULL) || (state == NULL) || (fd == NULL) || (allow_full_restart == NULL) ||
        (failure_count == NULL)) {
        return BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CLEANUP;
    }

    baji_photo_http_note_chunk_error(state);
    if ((ret == BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_RESUME) && *allow_full_restart) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu range resume reset task=%s image=%s downloaded=%lu",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned long)state->downloaded_size);
        *allow_full_restart = false;
        baji_photo_http_prepare_restart_download(state, fd, false);
        return BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_RESTART;
    }
    if (baji_photo_http_process_range_error_step(task,
                                                 state,
                                                 ret,
                                                 http_status,
                                                 failure_count)) {
        return BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CONTINUE;
    }
    return BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CLEANUP;
}

static baji_photo_http_finalize_step_t baji_photo_http_finalize_verified_download(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    bool *allow_full_restart,
    baji_photo_manifest_item_t *out_item,
    baji_photo_http_image_result_t *out_result,
    int *out_ret)
{
    int ret = LIOT_HTTPC_ERR_INVALID_PARAM;

    if (out_ret != NULL) {
        *out_ret = ret;
    }
    if ((task == NULL) || (state == NULL) || (fd == NULL) || (allow_full_restart == NULL) ||
        (out_item == NULL) || (out_result == NULL) || (out_ret == NULL)) {
        return BAJI_PHOTO_HTTP_FINALIZE_STEP_CLEANUP;
    }

    ret = baji_photo_http_verify_tmp_file(*fd, task, out_item);
    if (baji_photo_http_try_restart_after_verify_failure(task,
                                                         state,
                                                         fd,
                                                         ret,
                                                         allow_full_restart)) {
        *out_ret = ret;
        return BAJI_PHOTO_HTTP_FINALIZE_STEP_RESTART;
    }
    if (ret != 0) {
        *out_ret = ret;
        return BAJI_PHOTO_HTTP_FINALIZE_STEP_CLEANUP;
    }

    ret = baji_photo_http_finalize_download_success(task, state, fd, out_item, out_result);
    *out_ret = ret;
    if (ret != 0) {
        return BAJI_PHOTO_HTTP_FINALIZE_STEP_CLEANUP;
    }
    return BAJI_PHOTO_HTTP_FINALIZE_STEP_SUCCESS;
}

static bool baji_photo_http_try_restart_after_verify_failure(
    const baji_photo_image_task_t *task,
    baji_photo_task_state_t *state,
    LFILE_EXT *fd,
    int ret,
    bool *allow_full_restart)
{
    if ((task == NULL) || (state == NULL) || (fd == NULL) || (allow_full_restart == NULL)) {
        return false;
    }
    if (!*allow_full_restart) {
        return false;
    }
    if ((ret != BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH) &&
        (ret != BAJI_PHOTO_HTTP_IMAGE_ERR_MD5_MISMATCH)) {
        return false;
    }

    BAJI_PHOTO_HTTP_TRACE("verify reset task=%s image=%s ret=%d",
                          task->task_id,
                          task->image_id,
                          ret);
    *allow_full_restart = false;
    baji_photo_http_prepare_restart_download(state, fd, true);
    return true;
}

static bool baji_photo_http_process_range_error_step(
    const baji_photo_image_task_t *task,
    const baji_photo_task_state_t *state,
    int ret,
    int http_status,
    unsigned int *failure_count)
{
    if ((task == NULL) || (state == NULL) || (failure_count == NULL)) {
        return false;
    }

    if (baji_photo_http_error_should_wait_network(ret)) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu range defer wait_network task=%s ret=%d status=%d downloaded=%lu "
                              "retry=%u next_wait_retry_s=%u",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              ret,
                              http_status,
                              (unsigned long)state->downloaded_size,
                              (unsigned int)state->retry_count,
                              (unsigned int)BAJI_PHOTO_NETWORK_WAIT_RETRY_S);
        (void)baji_photo_network_prepare_trace(task->trace_id,
                                               0u,
                                               BAJI_PHOTO_NETWORK_WAIT_RETRY_S * 1000u);
        return false;
    }

    if (baji_photo_http_error_retryable(ret, http_status) &&
        ((*failure_count + 1u) < BAJI_PHOTO_RANGE_FAIL_MAX)) {
        *failure_count += 1u;
        BAJI_PHOTO_HTTP_TRACE("op=%lu range retry task=%s attempt=%u/%u ret=%d status=%d downloaded=%lu next_retry_ms=%u",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              *failure_count + 1u,
                              BAJI_PHOTO_RANGE_FAIL_MAX,
                              ret,
                              http_status,
                              (unsigned long)state->downloaded_size,
                              BAJI_PHOTO_HTTP_RETRY_BACKOFF_MS);
        liot_rtos_task_sleep_ms(BAJI_PHOTO_HTTP_RETRY_BACKOFF_MS);
        return true;
    }

    return false;
}

static void baji_photo_http_ctx_get_read_view(const baji_photo_http_ctx_t *ctx,
                                              baji_photo_http_ctx_read_view_t *view)
{
    if (view == NULL) {
        return;
    }

    memset(view, 0, sizeof(*view));
    view->status_code = -1;
    if (ctx == NULL) {
        return;
    }

    view->len = ctx->len;
    view->status_code = ctx->status_code;
    view->saw_status = ctx->saw_status;
    view->saw_complete = ctx->saw_complete;
}

static bool baji_photo_http_ctx_has_no_response(const baji_photo_http_ctx_t *ctx)
{
    baji_photo_http_ctx_read_view_t view;

    baji_photo_http_ctx_get_read_view(ctx, &view);
    return (view.len == 0u) &&
           (view.status_code < 0) &&
           !view.saw_status &&
           !view.saw_complete;
}

static void baji_photo_http_marker(const char *stage,
                                   const baji_photo_image_task_t *task,
                                   unsigned int downloaded_size)
{
    if ((stage == NULL) || (task == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu task=%s image=%s bytes=%u heap_min=%lu",
                          stage,
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          downloaded_size,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_http_marker_result(const char *stage,
                                          const baji_photo_image_task_t *task,
                                          const baji_photo_task_state_t *state,
                                          const baji_photo_http_image_result_t *result,
                                          int ret)
{
    if ((stage == NULL) || (task == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("%s op=%lu task=%s image=%s ret=%d http=%d dl=%lu retry=%u range=%d heap_min=%lu",
                          stage,
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          ret,
                          (result != NULL) ? result->http_status : -1,
                          (unsigned long)((state != NULL) ? state->downloaded_size : 0u),
                          (unsigned int)((state != NULL) ? state->retry_count : 0u),
                          ((result != NULL) && result->range_resumed) ? 1 : 0,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_http_marker_verify_ok(const baji_photo_image_task_t *task,
                                             const baji_photo_file_header_t *header,
                                             const baji_photo_manifest_item_t *item)
{
    if ((task == NULL) || (header == NULL) || (item == NULL)) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("HTTP_VERIFY_OK op=%lu task=%s image=%s hdr=%ux%u data=%lu file=%lu crc=%08lX heap_min=%lu",
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          (unsigned int)header->width,
                          (unsigned int)header->height,
                          (unsigned long)header->data_size,
                          (unsigned long)item->file_size,
                          (unsigned long)header->crc32,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static void baji_photo_http_marker_commit_result(const baji_photo_image_task_t *task,
                                                 uint32_t expected_size,
                                                 int rename_ret,
                                                 int final_exist_ret,
                                                 const liot_stat_ext_s *final_st,
                                                 int tmp_exist_ret)
{
    if (task == NULL) {
        return;
    }

    BAJI_PHOTO_MARK_TRACE("HTTP_COMMIT_RESULT op=%lu task=%s image=%s expect=%lu rename=%d final_exist=%d final_size=%lu tmp_exist=%d heap_min=%lu",
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          (unsigned long)expected_size,
                          rename_ret,
                          final_exist_ret,
                          (unsigned long)((final_st != NULL) ? final_st->size : 0u),
                          tmp_exist_ret,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
}

static int baji_photo_http_wait_until(liot_sem_t sem, const bool *flag, unsigned int timeout_ms)
{
    unsigned int waited_ms = 0u;

    if ((sem == NULL) || (flag == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    while (!(*flag) && (waited_ms < timeout_ms)) {
        unsigned int wait_ms = BAJI_PHOTO_HTTP_WAIT_STEP_MS;

        if (wait_ms > (timeout_ms - waited_ms)) {
            wait_ms = timeout_ms - waited_ms;
        }
        if (wait_ms == 0u) {
            break;
        }
        (void)liot_rtos_semaphore_wait(sem, wait_ms);
        waited_ms += wait_ms;
    }

    return (*flag) ? LIOT_HTTPC_SUCCESS : LIOT_HTTPC_ERR_TIMEOUT;
}

static void baji_photo_http_stop_client(liot_http_client_t *client,
                                        baji_photo_http_ctx_t *ctx,
                                        const char *reason)
{
    int stop_ret;
    int running;

    if ((client == NULL) || (ctx == NULL)) {
        return;
    }
    if (ctx->stop_requested) {
        return;
    }

    ctx->stop_requested = true;
    running = liot_httpc_is_running(client) ? 1 : 0;
    BAJI_PHOTO_HTTP_TRACE("body stop reason=%s running=%d err=%d recv=%u",
                          (reason != NULL) ? reason : "unknown",
                          running,
                          ctx->err,
                          ctx->len);
    stop_ret = liot_httpc_stop(client);
    BAJI_PHOTO_HTTP_TRACE("body stop done reason=%s ret=%d recv=%u",
                          (reason != NULL) ? reason : "unknown",
                          stop_ret,
                          ctx->len);
}

static int baji_photo_http_store_stat(const char *path, liot_stat_ext_s *out_st)
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

static int baji_photo_http_store_seek(LFILE_EXT fd, long offset, int whence)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_fseek_ext(fd, offset, whence);
    baji_photo_store_access_end();
    if (ret < 0) {
        return ret;
    }
    return LIOT_EXTFLASH_OK;
}

static int baji_photo_http_store_fsync(LFILE_EXT fd)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_fsync_ext(fd);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_http_store_close(LFILE_EXT fd)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_fclose_ext(fd);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_http_store_remove(const char *path)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_remove_ext(path);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_http_store_rename(const char *from_path, const char *to_path)
{
    int ret;

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }
    ret = liot_rename_ext(from_path, to_path);
    baji_photo_store_access_end();
    return ret;
}

static int baji_photo_http_store_exists(const char *path)
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

static void baji_photo_http_trace_store_state(const char *stage,
                                              const baji_photo_image_task_t *task,
                                              const char *path,
                                              int stat_ret,
                                              const liot_stat_ext_s *st)
{
    BAJI_PHOTO_HTTP_TRACE("commit %s op=%lu task=%s image=%s path=%s stat=%d type=%u size=%lu",
                          (stage != NULL) ? stage : "unknown",
                          (task != NULL) ? (unsigned long)task->trace_id : 0ul,
                          ((task != NULL) && (task->task_id[0] != '\0')) ? task->task_id : "-",
                          ((task != NULL) && (task->image_id[0] != '\0')) ? task->image_id : "-",
                          (path != NULL) ? path : "(null)",
                          stat_ret,
                          (unsigned int)((st != NULL) ? st->type : 0u),
                          (unsigned long)((st != NULL) ? st->size : 0u));
}

static int baji_photo_http_commit_tmp_file(const baji_photo_image_task_t *task,
                                           const baji_photo_task_state_t *state)
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

    if ((task == NULL) || (state == NULL) ||
        (state->tmp_path[0] == '\0') || (state->final_path[0] == '\0')) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_EXTFLASH_INVALID_PARAMETER;
    }

    expected_size = baji_photo_http_task_stored_size(format, state->downloaded_size);

    memset(&tmp_st, 0, sizeof(tmp_st));
    tmp_stat_ret = baji_photo_http_store_stat(state->tmp_path, &tmp_st);
    baji_photo_http_trace_store_state("tmp-before", task, state->tmp_path, tmp_stat_ret, &tmp_st);
    if ((tmp_stat_ret != LIOT_EXTFLASH_OK) ||
        (tmp_st.type != LIOT_EXTFLASH_TYPE_FILE) ||
        (tmp_st.size != expected_size)) {
        BAJI_PHOTO_HTTP_TRACE("commit tmp invalid op=%lu task=%s image=%s expect=%lu stat=%d type=%u size=%lu",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned long)expected_size,
                              tmp_stat_ret,
                              (unsigned int)tmp_st.type,
                              (unsigned long)tmp_st.size);
        if (tmp_stat_ret != LIOT_EXTFLASH_OK) {
            return tmp_stat_ret;
        }
        return LIOT_EXTFLASH_SIZE_FAIL;
    }

    memset(&final_st, 0, sizeof(final_st));
    final_exist_ret = baji_photo_http_store_exists(state->final_path);
    final_stat_ret = baji_photo_http_store_stat(state->final_path, &final_st);
    baji_photo_http_trace_store_state("final-before", task, state->final_path, final_stat_ret, &final_st);
    BAJI_PHOTO_HTTP_TRACE("commit final exists op=%lu task=%s image=%s path=%s exist=%d",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          state->final_path,
                          final_exist_ret);
    if (final_exist_ret == LIOT_EXTFLASH_NOT_EXIST) {
        BAJI_PHOTO_HTTP_TRACE("commit remove skip op=%lu task=%s image=%s path=%s reason=not_exist",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              state->final_path);
    } else {
        remove_ret = baji_photo_http_store_remove(state->final_path);
        BAJI_PHOTO_HTTP_TRACE("commit remove op=%lu task=%s image=%s path=%s ret=%d",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              state->final_path,
                              remove_ret);
        final_exist_ret = baji_photo_http_store_exists(state->final_path);
        memset(&final_st, 0, sizeof(final_st));
        final_stat_ret = baji_photo_http_store_stat(state->final_path, &final_st);
        baji_photo_http_trace_store_state("final-after-remove",
                                          task,
                                          state->final_path,
                                          final_stat_ret,
                                          &final_st);
        BAJI_PHOTO_HTTP_TRACE("commit final exists after remove op=%lu task=%s image=%s path=%s exist=%d",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              state->final_path,
                              final_exist_ret);
        if ((remove_ret != LIOT_EXTFLASH_OK) && (final_exist_ret != LIOT_EXTFLASH_NOT_EXIST)) {
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
        if (final_exist_ret == LIOT_EXTFLASH_OK) {
            return LIOT_EXTFLASH_REMOVE_FAIL;
        }
    }

    rename_ret = baji_photo_http_store_rename(state->tmp_path, state->final_path);
    BAJI_PHOTO_HTTP_TRACE("commit rename op=%lu task=%s image=%s from=%s to=%s ret=%d",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          state->tmp_path,
                          state->final_path,
                          rename_ret);

    final_exist_after_ret = baji_photo_http_store_exists(state->final_path);
    memset(&final_st, 0, sizeof(final_st));
    final_stat_ret = baji_photo_http_store_stat(state->final_path, &final_st);
    baji_photo_http_trace_store_state("final-after-rename",
                                      task,
                                      state->final_path,
                                      final_stat_ret,
                                      &final_st);
    BAJI_PHOTO_HTTP_TRACE("commit final exists after rename op=%lu task=%s image=%s path=%s exist=%d",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          state->final_path,
                          final_exist_after_ret);
    tmp_exist_after_ret = baji_photo_http_store_exists(state->tmp_path);
    memset(&tmp_st, 0, sizeof(tmp_st));
    tmp_after_ret = baji_photo_http_store_stat(state->tmp_path, &tmp_st);
    baji_photo_http_trace_store_state("tmp-after-rename",
                                      task,
                                      state->tmp_path,
                                      tmp_after_ret,
                                      &tmp_st);
    BAJI_PHOTO_HTTP_TRACE("commit tmp exists after rename op=%lu task=%s image=%s path=%s exist=%d",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          state->tmp_path,
                          tmp_exist_after_ret);

    if (rename_ret == LIOT_EXTFLASH_OK) {
        baji_photo_http_marker_commit_result(task,
                                             expected_size,
                                             rename_ret,
                                             final_exist_after_ret,
                                             &final_st,
                                             tmp_exist_after_ret);
        return 0;
    }
    if ((final_exist_after_ret == LIOT_EXTFLASH_OK) &&
        (final_stat_ret == LIOT_EXTFLASH_OK) &&
        (final_st.type == LIOT_EXTFLASH_TYPE_FILE) &&
        (final_st.size == expected_size) &&
        (tmp_exist_after_ret == LIOT_EXTFLASH_NOT_EXIST)) {
        BAJI_PHOTO_HTTP_TRACE("commit rename tolerate op=%lu task=%s image=%s expect=%lu",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned long)expected_size);
        baji_photo_http_marker_commit_result(task,
                                             expected_size,
                                             rename_ret,
                                             final_exist_after_ret,
                                             &final_st,
                                             tmp_exist_after_ret);
        return 0;
    }
    return LIOT_EXTFLASH_RENAME_FAIL;
}

static int baji_photo_http_ctx_prepare(char *buf,
                                       unsigned int cap,
                                       LFILE_EXT file_fd,
                                       bool write_to_file)
{
    baji_photo_net_service_t *service = baji_photo_net_default();
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    LiotOSStatus_t ret;
    liot_sem_t sem_resp = NULL;
    liot_sem_t sem_close = NULL;

    if (cap == 0u) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if ((!write_to_file && (buf == NULL)) ||
        (write_to_file && (file_fd <= 0))) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    if (!service->http_sem_ready) {
        ret = liot_rtos_semaphore_create(&ctx->sem_resp, 0);
        if (ret != 0) {
            return ret;
        }
        ret = liot_rtos_semaphore_create(&ctx->sem_close, 0);
        if (ret != 0) {
            liot_rtos_semaphore_delete(ctx->sem_resp);
            ctx->sem_resp = NULL;
            return ret;
        }
        service->http_sem_ready = true;
    }

    ret = baji_photo_http_ctx_wait_reusable(ctx);
    if (ret != 0) {
        return ret;
    }

    sem_resp = ctx->sem_resp;
    sem_close = ctx->sem_close;
    baji_photo_http_drain_sem(sem_resp);
    baji_photo_http_drain_sem(sem_close);

    memset(ctx, 0, sizeof(*ctx));
    ctx->sem_resp = sem_resp;
    ctx->sem_close = sem_close;
    ctx->buf = buf;
    ctx->cap = cap;
    ctx->file_fd = file_fd;
    ctx->write_to_file = write_to_file;
    ctx->status_code = -1;
    ctx->content_len = -1;
    ctx->content_range = -1;
    ctx->chunk_encode = -1;
    return 0;
}

static bool baji_photo_true_color_dims_valid(uint32_t width, uint32_t height)
{
    return (width != 0u) &&
           (height != 0u) &&
           (width <= BAJI_PHOTO_IMG_W) &&
           (height <= BAJI_PHOTO_IMG_H);
}

static uint32_t baji_photo_true_color_data_size(uint32_t width, uint32_t height)
{
    return width * height * BAJI_PHOTO_IMG_BPP;
}

static void baji_photo_http_result_reset(baji_photo_http_image_result_t *out_result)
{
    if (out_result == NULL) {
        return;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->http_status = -1;
}

static void baji_photo_http_url_free(liot_httpc_url_s *url)
{
    if (url == NULL) {
        return;
    }

    if (url->host != NULL) {
        liot_rtos_free(url->host);
        url->host = NULL;
    }
    if (url->uri != NULL) {
        liot_rtos_free(url->uri);
        url->uri = NULL;
    }
}

static const char *baji_photo_http_client_opt_name(int opt_tag)
{
    switch (opt_tag) {
    case LIOT_HTTP_CLIENT_OPT_PDPCID:
        return "pdp_cid";
    case LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER:
        return "request_header";
    case LIOT_HTTP_CLIENT_OPT_METHOD:
        return "method";
    case LIOT_HTTP_CLIENT_OPT_WRITE_FUNC:
        return "write_func";
    case LIOT_HTTP_CLIENT_OPT_WRITE_DATA:
        return "write_data";
    case LIOT_HTTP_CLIENT_OPT_URL:
        return "url";
    case LIOT_HTTP_CLIENT_OPT_SIM_ID:
        return "sim_id";
    case LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT:
        return "send_timeout";
    case LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT:
        return "recv_timeout";
    default:
        break;
    }

    return "unknown_opt";
}

static void baji_photo_http_build_url_summary(const char *url_str, char *buf, unsigned int buf_len)
{
    const char *end;
    unsigned int copy_len;

    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }

    buf[0] = '\0';
    if (url_str == NULL) {
        (void)snprintf(buf, buf_len, "null");
        return;
    }

    end = strchr(url_str, '?');
    if (end == NULL) {
        end = url_str + strlen(url_str);
    }
    copy_len = (unsigned int)(end - url_str);
    if (copy_len >= buf_len) {
        if (buf_len > 4u) {
            copy_len = buf_len - 4u;
            memcpy(buf, url_str, copy_len);
            memcpy(buf + copy_len, "...", 4u);
        }
        return;
    }
    memcpy(buf, url_str, copy_len);
    buf[copy_len] = '\0';
}

static void baji_photo_http_build_header_summary(const char *request_header, char *buf, unsigned int buf_len)
{
    unsigned int copy_len;

    if ((buf == NULL) || (buf_len == 0u)) {
        return;
    }

    buf[0] = '\0';
    if ((request_header == NULL) || (request_header[0] == '\0')) {
        (void)snprintf(buf, buf_len, "none");
        return;
    }

    copy_len = (unsigned int)strlen(request_header);
    if (copy_len >= buf_len) {
        if (buf_len > 4u) {
            copy_len = buf_len - 4u;
            memcpy(buf, request_header, copy_len);
            memcpy(buf + copy_len, "...", 4u);
        }
        return;
    }
    memcpy(buf, request_header, copy_len);
    buf[copy_len] = '\0';
}

static void baji_photo_http_setopt_diag(liot_http_client_t *client, int opt_tag, int ret)
{
    const char *opt_name;

    (void)client;

    if (ret == LIOT_HTTPC_SUCCESS) {
        return;
    }

    opt_name = baji_photo_http_client_opt_name(opt_tag);
    if ((opt_tag == LIOT_HTTP_CLIENT_OPT_SIM_ID) && (ret == LIOT_HTTPC_ERR_NOT_SUPPORT)) {
        BAJI_PHOTO_HTTP_TRACE("GET setopt opt=%s ret=%d action=diag_only", opt_name, ret);
        return;
    }
    if (opt_tag == LIOT_HTTP_CLIENT_OPT_PDPCID) {
        BAJI_PHOTO_HTTP_TRACE("GET setopt opt=%s ret=%d action=blocking", opt_name, ret);
        return;
    }

    BAJI_PHOTO_HTTP_TRACE("GET setopt opt=%s ret=%d action=warn", opt_name, ret);
}

static bool baji_photo_http_setopt_can_continue(int opt_tag, int ret)
{
    if (ret == LIOT_HTTPC_SUCCESS) {
        return true;
    }
    if ((opt_tag == LIOT_HTTP_CLIENT_OPT_SIM_ID) && (ret == LIOT_HTTPC_ERR_NOT_SUPPORT)) {
        return true;
    }
    return false;
}

static bool baji_photo_http_should_retry_alt_pdp(int ret,
                                                 const baji_photo_http_ctx_t *ctx,
                                                 int primary_pdp_cid,
                                                 int alt_pdp_cid)
{
    if (primary_pdp_cid == alt_pdp_cid) {
        return false;
    }
    if (ctx == NULL) {
        return false;
    }
    if (!baji_photo_http_ctx_has_no_response(ctx)) {
        return false;
    }
    if (ret == BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL) {
        return true;
    }
    return baji_photo_http_transport_error_is_network_like(ret);
}

static bool baji_photo_http_status_ok(int status_code)
{
    return status_code == 200;
}

static bool baji_photo_http_status_payload_ok(int status_code)
{
    return (status_code == 200) || (status_code == 206);
}

static bool baji_photo_http_status_retryable(int status_code)
{
    return (status_code >= 500) && (status_code < 600);
}

static bool baji_photo_http_error_retryable(int ret, int status_code)
{
    switch (ret) {
    case LIOT_HTTPC_ERR_TIMEOUT:
    case LIOT_HTTPC_ERR_SOCKET_FAILURE:
    case LIOT_HTTPC_ERR_NO_NETWORK:
        return true;

    case LIOT_DATACALL_EXECUTE_ERR:
    case LIOT_DATACALL_NW_REGISTER_TIMEOUT_ERR:
    case LIOT_DATACALL_NOT_REGISTERED_ERR:
    case LIOT_DATACALL_SEMAPHORE_TIMEOUT_ERR:
    case LIOT_DATACALL_CFW_ACTIVE_REQUEST_ERR:
    case LIOT_DATACALL_ACTIVE_FAIL_ERR:
        return true;

    case LIOT_HTTPC_ERR_INVALID_PARAM:
    case LIOT_HTTPC_EMPTY_URL:
    case LIOT_HTTPC_ERR_OUT_OF_MEM:
    case LIOT_EXTFLASH_NO_SPACE:
        return false;

    default:
        break;
    }

    return baji_photo_http_status_retryable(status_code);
}

static bool baji_photo_network_rtc_ready(void)
{
    int32_t rtc_now = (int32_t)liot_rtc_get_time_s();

    return (rtc_now >= (int32_t)BAJI_PHOTO_HTTP_RTC_READY_EPOCH_S);
}

static const char *baji_photo_network_obs_state_name(baji_photo_net_obs_state_t state)
{
    switch (state) {
    case BAJI_PHOTO_NET_OBS_ACTIVE_READY:
        return "active_ready";
    case BAJI_PHOTO_NET_OBS_PROBE_ALLOWED:
        return "probe_allowed";
    default:
        break;
    }

    return "not_ready";
}

static bool baji_photo_http_transport_error_is_network_like(int ret)
{
    return (ret == LIOT_HTTPC_ERR_NO_NETWORK) ||
           (ret == LIOT_HTTPC_ERR_SOCKET_FAILURE) ||
           (ret == LIOT_HTTPC_ERR_TIMEOUT) ||
           (ret == BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL);
}

static bool baji_photo_http_error_should_wait_network(int ret)
{
    return baji_photo_http_transport_error_is_network_like(ret);
}

static const char *baji_photo_http_error_class_name(int ret)
{
    if (ret == BAJI_PHOTO_HTTP_IMAGE_ERR_BODY_TRUNCATED) {
        return "body_truncated";
    }
    return baji_photo_http_transport_error_is_network_like(ret) ? "network" : "non_network";
}

static const char *baji_photo_network_obs_reason(const baji_photo_net_observation_t *obs)
{
    if (obs == NULL) {
        return "obs_null";
    }
    if ((obs->info_ret == LIOT_DATACALL_SUCCESS) && (obs->v4_state == LIOT_DATACALL_STATE_ACTIVED)) {
        return "pdp_actived";
    }
    if (obs->cached_http_success) {
        return "cached_http_success";
    }
    if (obs->profile_active && (obs->info_ret != LIOT_DATACALL_SUCCESS)) {
        return "profile_active_info_unknown";
    }
    if (obs->profile_active) {
        return "profile_active_snapshot_only";
    }
    if (obs->info_ret == LIOT_DATACALL_SUCCESS) {
        return "pdp_snapshot_only";
    }
    return "no_profile";
}

static void baji_photo_network_record_http_result(baji_photo_net_service_t *service,
                                                  int ret,
                                                  int status_code,
                                                  bool ok)
{
    if (service == NULL) {
        return;
    }

    service->last_http_ok = ok;
    service->last_http_ret = ret;
    service->last_http_status = status_code;
    if (ok) {
        service->ready = true;
    } else if (baji_photo_http_transport_error_is_network_like(ret)) {
        service->ready = false;
    }
}

static void baji_photo_network_observe(const baji_photo_net_service_t *service,
                                       baji_photo_net_observation_t *obs)
{
    liot_data_call_info_t info = {0};

    if ((service == NULL) || (obs == NULL)) {
        return;
    }

    memset(obs, 0, sizeof(*obs));
    obs->info_ret = LIOT_DATACALL_EXECUTE_ERR;
    obs->cid = BAJI_PHOTO_HTTP_PDP_CID;
    obs->ip_version = -1;
    obs->v4_state = -1;
    obs->last_http_ok = service->last_http_ok;
    obs->last_http_ret = service->last_http_ret;
    obs->last_http_status = service->last_http_status;
    obs->cached_http_success = service->ready;
    obs->rtc_ready = baji_photo_network_rtc_ready();
    obs->profile_active = liot_datacall_get_sim_profile_is_active(BAJI_PHOTO_HTTP_SIM_ID,
                                                                  BAJI_PHOTO_HTTP_PDP_CID);
    obs->info_ret = liot_get_data_call_info(BAJI_PHOTO_HTTP_SIM_ID, BAJI_PHOTO_HTTP_PDP_CID, &info);
    if (obs->info_ret == LIOT_DATACALL_SUCCESS) {
        obs->cid = info.cid;
        obs->ip_version = info.ip_version;
        obs->v4_state = info.v4.state;
    }
    if ((obs->info_ret == LIOT_DATACALL_SUCCESS) &&
        (obs->v4_state == LIOT_DATACALL_STATE_ACTIVED)) {
        obs->state = BAJI_PHOTO_NET_OBS_ACTIVE_READY;
    } else if (obs->cached_http_success || obs->profile_active) {
        obs->state = BAJI_PHOTO_NET_OBS_PROBE_ALLOWED;
    } else {
        obs->state = BAJI_PHOTO_NET_OBS_NOT_READY;
    }
    obs->reason = baji_photo_network_obs_reason(obs);
}

static void baji_photo_http_log_network_state(const char *phase,
                                              uint32_t trace_id,
                                              const baji_photo_net_observation_t *obs,
                                              uint32_t retry_deadline_ms,
                                              uint32_t next_retry_ms)
{
    if (obs == NULL) {
        return;
    }

    BAJI_PHOTO_HTTP_TRACE("op=%lu net %s net_state=%s reason=%s cached_http_success=%d profile_active=%d rtc_ready=%d "
                          "info_ret=%d cid=%d ip_ver=%d v4_state=%d last_http_ok=%d last_http_ret=%d last_http_status=%d "
                          "retry_deadline_ms=%lu next_retry_ms=%lu",
                          (unsigned long)trace_id,
                          phase,
                          baji_photo_network_obs_state_name(obs->state),
                          obs->reason,
                          obs->cached_http_success ? 1 : 0,
                          obs->profile_active ? 1 : 0,
                          obs->rtc_ready ? 1 : 0,
                          obs->info_ret,
                          obs->cid,
                          obs->ip_version,
                          obs->v4_state,
                          obs->last_http_ok ? 1 : 0,
                          obs->last_http_ret,
                          obs->last_http_status,
                          (unsigned long)retry_deadline_ms,
                          (unsigned long)next_retry_ms);
}

static bool baji_photo_http_body_complete(const baji_photo_http_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    if (ctx->err != LIOT_HTTPC_SUCCESS) {
        return false;
    }
    if (ctx->chunk_encode == 1) {
        return ctx->saw_complete;
    }
    if ((ctx->chunk_encode == 0) && (ctx->content_len >= 0) &&
        (ctx->len == (unsigned int)ctx->content_len)) {
        return true;
    }
    if (ctx->saw_complete && ((ctx->chunk_encode < 0) || (ctx->content_len < 0))) {
        BAJI_PHOTO_HTTP_TRACE("download fallback status=%d chunk=%d content_len=%d recv=%u",
                              ctx->status_code,
                              ctx->chunk_encode,
                              ctx->content_len,
                              ctx->len);
        return true;
    }
    return false;
}

static bool baji_photo_http_body_truncated(const baji_photo_http_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    if (!baji_photo_http_status_payload_ok(ctx->status_code)) {
        return false;
    }
    if ((ctx->content_len <= 0) || (ctx->len == 0u)) {
        return false;
    }
    if (!ctx->saw_close || ctx->saw_complete) {
        return false;
    }
    return ctx->len < (unsigned int)ctx->content_len;
}

#if BAJI_PHOTO_HTTP_RAW_RGB565_VERIFY
static bool baji_photo_raw_rgb565_size_detect(unsigned int raw_len,
                                              uint16_t *out_w,
                                              uint16_t *out_h)
{
    if ((out_w == NULL) || (out_h == NULL)) {
        return false;
    }

    if (raw_len == baji_photo_true_color_data_size(BAJI_PHOTO_HTTP_RAW_RGB565_W,
                                                   BAJI_PHOTO_HTTP_RAW_RGB565_H)) {
        *out_w = BAJI_PHOTO_HTTP_RAW_RGB565_W;
        *out_h = BAJI_PHOTO_HTTP_RAW_RGB565_H;
        return true;
    }

    if (raw_len == baji_photo_true_color_data_size(240u, 240u)) {
        *out_w = 240u;
        *out_h = 240u;
        return true;
    }

    if (raw_len == baji_photo_true_color_data_size(360u, 360u)) {
        *out_w = 360u;
        *out_h = 360u;
        return true;
    }

    return false;
}

static int baji_photo_raw_rgb565_fetch(baji_photo_manifest_item_t *item,
                                       uint8_t **out_photo_buf,
                                       unsigned int *out_photo_len)
{
    uint8_t *raw_buf = NULL;
    uint8_t *photo_buf = NULL;
    uint8_t *photo_pixels = NULL;
    baji_photo_file_header_t *header;
    uint16_t raw_w = 0u;
    uint16_t raw_h = 0u;
    uint16_t out_w = 0u;
    uint16_t out_h = 0u;
    unsigned int raw_len = 0;
    uint32_t crc;
    uint32_t photo_data_size;
    int free_size;
    int ret = 0;

    if ((item == NULL) || (out_photo_buf == NULL) || (out_photo_len == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    *out_photo_buf = NULL;
    *out_photo_len = 0u;

    free_size = baji_photo_store_free_size();
    if ((free_size >= 0) &&
        ((uint32_t)free_size < ((uint32_t)sizeof(baji_photo_file_header_t) + BAJI_PHOTO_IMG_DATA_SIZE + 16u * 1024u))) {
        return LIOT_EXTFLASH_NO_SPACE;
    }

    raw_buf = (uint8_t *)liot_rtos_malloc(BAJI_PHOTO_IMG_DATA_SIZE);
    if (raw_buf == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    ret = baji_photo_http_get_url_retry(BAJI_PHOTO_HTTP_RAW_RGB565_URL,
                                        (char *)raw_buf,
                                        BAJI_PHOTO_IMG_DATA_SIZE,
                                        false,
                                        &raw_len,
                                        BAJI_PHOTO_HTTP_RETRY_MAX);
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("raw verify GET failed ret=%d", ret);
        goto cleanup;
    }
    if (!baji_photo_raw_rgb565_size_detect(raw_len, &raw_w, &raw_h)) {
        BAJI_PHOTO_HTTP_TRACE("raw verify unexpected size bytes=%u expect=%lu/%lu",
                              raw_len,
                              (unsigned long)baji_photo_true_color_data_size(240u, 240u),
                              (unsigned long)baji_photo_true_color_data_size(360u, 360u));
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto cleanup;
    }
    photo_data_size = baji_photo_true_color_data_size(raw_w, raw_h);
    BAJI_PHOTO_HTTP_TRACE("raw verify download ok url=%s bytes=%u",
                          BAJI_PHOTO_HTTP_RAW_RGB565_URL,
                          raw_len);
    BAJI_PHOTO_HTTP_TRACE("raw verify detect size=%ux%u",
                          (unsigned int)raw_w,
                          (unsigned int)raw_h);
    out_w = raw_w;
    out_h = raw_h;
    if ((raw_w == 240u) && (raw_h == 240u)) {
        out_w = BAJI_PHOTO_IMG_W;
        out_h = BAJI_PHOTO_IMG_H;
        BAJI_PHOTO_HTTP_TRACE("raw verify center pad %ux%u -> %ux%u",
                              (unsigned int)raw_w,
                              (unsigned int)raw_h,
                              (unsigned int)out_w,
                              (unsigned int)out_h);
    }

    photo_data_size = baji_photo_true_color_data_size(out_w, out_h);
    photo_buf = (uint8_t *)liot_rtos_malloc(sizeof(baji_photo_file_header_t) + photo_data_size);
    if (photo_buf == NULL) {
        ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        goto cleanup;
    }

    header = (baji_photo_file_header_t *)photo_buf;
    photo_pixels = photo_buf + sizeof(*header);
    memset(photo_pixels, 0, photo_data_size);
    if ((out_w == raw_w) && (out_h == raw_h)) {
        memcpy(photo_pixels, raw_buf, photo_data_size);
    } else {
        unsigned int row;
        unsigned int src_stride = (unsigned int)raw_w * BAJI_PHOTO_IMG_BPP;
        unsigned int dst_stride = (unsigned int)out_w * BAJI_PHOTO_IMG_BPP;
        unsigned int x_off = ((unsigned int)out_w - (unsigned int)raw_w) / 2u;
        unsigned int y_off = ((unsigned int)out_h - (unsigned int)raw_h) / 2u;

        for (row = 0; row < (unsigned int)raw_h; ++row) {
            memcpy(photo_pixels + ((y_off + row) * dst_stride) + (x_off * BAJI_PHOTO_IMG_BPP),
                   raw_buf + (row * src_stride),
                   src_stride);
        }
    }

    crc = baji_photo_store_crc32_update(0xFFFFFFFFu,
                                        photo_pixels,
                                        photo_data_size);
    crc = baji_photo_store_crc32_finish(crc);

    memset(header, 0, sizeof(*header));
    header->magic = BAJI_PHOTO_MAGIC;
    header->width = out_w;
    header->height = out_h;
    header->cf = LV_IMG_CF_TRUE_COLOR;
    header->data_size = photo_data_size;
    header->crc32 = crc;

    memset(item, 0, sizeof(*item));
    snprintf(item->id, sizeof(item->id), "%s", BAJI_PHOTO_HTTP_RAW_RGB565_ID);
    snprintf(item->name, sizeof(item->name), "%s", BAJI_PHOTO_HTTP_RAW_RGB565_NAME);
    snprintf(item->remote_path, sizeof(item->remote_path), "%s", BAJI_PHOTO_HTTP_RAW_RGB565_PATH);
    baji_photo_store_build_photo_path(item->id, item->local_path, sizeof(item->local_path));
    item->file_size = (uint32_t)(sizeof(*header) + photo_data_size);
    item->crc32 = crc;
    item->width = out_w;
    item->height = out_h;
    item->cf = LV_IMG_CF_TRUE_COLOR;
    item->format = BAJI_PHOTO_FORMAT_BJP;
    BAJI_PHOTO_HTTP_TRACE("raw verify wrap bjp %ux%u crc=%08lX size=%lu",
                          (unsigned int)item->width,
                          (unsigned int)item->height,
                          (unsigned long)item->crc32,
                          (unsigned long)item->file_size);
    *out_photo_buf = photo_buf;
    *out_photo_len = item->file_size;
    photo_buf = NULL;

cleanup:
    if (raw_buf != NULL) {
        liot_rtos_free(raw_buf);
    }
    if (photo_buf != NULL) {
        liot_rtos_free(photo_buf);
    }
    return ret;
}

static int baji_photo_raw_rgb565_commit(const baji_photo_manifest_item_t *item,
                                        const baji_photo_manifest_item_t *local_items,
                                        unsigned int local_count)
{
    baji_photo_manifest_item_t *result_items = NULL;
    unsigned int i;
    unsigned int result_count = 0;
    int ret = 0;

    if (item == NULL) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    result_items = (baji_photo_manifest_item_t *)liot_rtos_malloc(sizeof(*result_items) * BAJI_PHOTO_DL_MAX);
    if (result_items == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    result_items[result_count++] = *item;
    for (i = 0; i < local_count; ++i) {
        if (strcmp(local_items[i].id, item->id) == 0) {
            continue;
        }
        if (result_count >= BAJI_PHOTO_DL_MAX) {
            ret = LIOT_HTTPC_ERR_INVALID_PARAM;
            goto cleanup;
        }
        result_items[result_count++] = local_items[i];
    }
    ret = baji_photo_store_save_index(result_items, result_count);

cleanup:
    liot_rtos_free(result_items);
    return ret;
}

static int baji_photo_raw_rgb565_write(const baji_photo_manifest_item_t *item,
                                       const uint8_t *photo_buf,
                                       unsigned int photo_len)
{
    if ((item == NULL) || (photo_buf == NULL) || (photo_len != item->file_size)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    BAJI_PHOTO_HTTP_TRACE("write raw verify photo id=%s path=%s size=%u",
                          item->id,
                          item->local_path,
                          photo_len);
    return baji_photo_store_write_photo_file(item->id, photo_buf, photo_len);
}
#endif

static const char *baji_photo_skip_ws(const char *p, const char *end)
{
    while ((p < end) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))) {
        ++p;
    }
    return p;
}

static const char *baji_photo_find_json_key(const char *start, const char *end, const char *key)
{
    char pattern[48];
    unsigned int key_len;
    int written;
    const char *p;

    key_len = (unsigned int)strlen(key);
    if ((key_len + 3u) > sizeof(pattern)) {
        return NULL;
    }

    written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0) {
        return NULL;
    }

    p = start;
    while ((p < end) && ((end - p) >= written)) {
        if ((memcmp(p, pattern, (unsigned int)written) == 0)) {
            p += written;
            p = baji_photo_skip_ws(p, end);
            if ((p < end) && (*p == ':')) {
                return baji_photo_skip_ws(p + 1, end);
            }
        }
        ++p;
    }

    return NULL;
}

static int baji_photo_parse_json_string(const char *obj_start,
                                        const char *obj_end,
                                        const char *key,
                                        char *out,
                                        unsigned int out_len)
{
    const char *p;
    unsigned int len = 0;

    if ((out == NULL) || (out_len == 0u)) {
        return -1;
    }
    out[0] = '\0';

    p = baji_photo_find_json_key(obj_start, obj_end, key);
    if ((p == NULL) || (p >= obj_end) || (*p != '"')) {
        return -1;
    }
    ++p;

    while (p < obj_end) {
        char c = *p++;

        if (c == '"') {
            out[len] = '\0';
            return 0;
        }
        if (c == '\\') {
            if (p >= obj_end) {
                return -1;
            }
            c = *p++;
        }
        if ((len + 1u) >= out_len) {
            return -1;
        }
        out[len++] = c;
    }

    return -1;
}

static int baji_photo_parse_json_u32(const char *obj_start,
                                     const char *obj_end,
                                     const char *key,
                                     uint32_t *out)
{
    const char *p;
    uint32_t value = 0;
    bool any = false;

    if (out == NULL) {
        return -1;
    }

    p = baji_photo_find_json_key(obj_start, obj_end, key);
    if (p == NULL) {
        return -1;
    }
    p = baji_photo_skip_ws(p, obj_end);

    while ((p < obj_end) && (*p >= '0') && (*p <= '9')) {
        value = (value * 10u) + (uint32_t)(*p - '0');
        any = true;
        ++p;
    }

    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

static int baji_photo_parse_json_hex_u32(const char *obj_start,
                                         const char *obj_end,
                                         const char *key,
                                         uint32_t *out)
{
    char hex[16];
    const char *p;
    const char *parse_end;
    uint32_t value = 0;
    bool any = false;

    if (out == NULL) {
        return -1;
    }

    if (baji_photo_parse_json_string(obj_start, obj_end, key, hex, sizeof(hex)) == 0) {
        p = hex;
        parse_end = hex + strlen(hex);
    } else {
        p = baji_photo_find_json_key(obj_start, obj_end, key);
        if (p == NULL) {
            return -1;
        }
        parse_end = obj_end;
    }

    if (((p + 2) < parse_end) && (p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X'))) {
        p += 2;
    }

    while (p < parse_end) {
        char c = *p;
        uint32_t digit;

        if ((c >= '0') && (c <= '9')) {
            digit = (uint32_t)(c - '0');
        } else if ((c >= 'a') && (c <= 'f')) {
            digit = (uint32_t)(c - 'a' + 10);
        } else if ((c >= 'A') && (c <= 'F')) {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            break;
        }

        value = (value << 4) | digit;
        any = true;
        ++p;
    }

    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

static int baji_photo_parse_manifest_format(const char *obj_start,
                                            const char *obj_end,
                                            baji_photo_format_t *out)
{
    char format[16];

    if (out == NULL) {
        return -1;
    }

    if (baji_photo_parse_json_string(obj_start, obj_end, "format", format, sizeof(format)) != 0) {
        *out = BAJI_PHOTO_FORMAT_BJP;
        return 0;
    }

    if ((strcmp(format, "gif") == 0) || (strcmp(format, "GIF") == 0)) {
#if BAJI_PHOTO_ENABLE_GIF_SUPPORT
        *out = BAJI_PHOTO_FORMAT_GIF;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "jpeg") == 0) || (strcmp(format, "jpg") == 0) ||
        (strcmp(format, "JPEG") == 0) || (strcmp(format, "JPG") == 0)) {
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        *out = BAJI_PHOTO_FORMAT_JPEG;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "png") == 0) || (strcmp(format, "PNG") == 0)) {
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        *out = BAJI_PHOTO_FORMAT_PNG;
        return 0;
#else
        return -1;
#endif
    }

    if ((strcmp(format, "bjp") == 0) ||
        (strcmp(format, "BJP") == 0) ||
        (strcmp(format, "rgb565") == 0) ||
        (strcmp(format, "RGB565") == 0)) {
        *out = BAJI_PHOTO_FORMAT_BJP;
        return 0;
    }

    return -1;
}

static int baji_photo_manifest_id_valid(const char *id)
{
    unsigned int i;

    if ((id == NULL) || (id[0] == '\0')) {
        return -1;
    }

    for (i = 0; id[i] != '\0'; ++i) {
        char c = id[i];

        if (i >= BAJI_PHOTO_ID_MAX_LEN) {
            return -1;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') ||
            (c == '-')) {
            continue;
        }
        return -1;
    }

    return 0;
}

static int baji_photo_manifest_remote_path_valid(const char *path)
{
    unsigned int i;

    if ((path == NULL) || (path[0] == '\0') || (path[0] == '/')) {
        return -1;
    }
    if (strstr(path, "..") != NULL) {
        return -1;
    }

    for (i = 0; path[i] != '\0'; ++i) {
        char c = path[i];

        if (i >= BAJI_PHOTO_PATH_MAX_LEN) {
            return -1;
        }
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '0') && (c <= '9')) ||
            (c == '_') ||
            (c == '-') ||
            (c == '.') ||
            (c == '/')) {
            continue;
        }
        return -1;
    }

    return 0;
}

static int baji_photo_parse_manifest_item(const char *obj_start,
                                          const char *obj_end,
                                          baji_photo_manifest_item_t *item)
{
    uint32_t width;
    uint32_t height;

    if (item == NULL) {
        return -1;
    }

    memset(item, 0, sizeof(*item));
    if (baji_photo_parse_json_string(obj_start, obj_end, "id", item->id, sizeof(item->id)) != 0) {
        return -1;
    }
    if (baji_photo_manifest_id_valid(item->id) != 0) {
        return -1;
    }
    (void)baji_photo_parse_json_string(obj_start, obj_end, "name", item->name, sizeof(item->name));
    if ((baji_photo_parse_json_string(obj_start,
                                      obj_end,
                                      "full_path",
                                      item->remote_path,
                                      sizeof(item->remote_path)) != 0) &&
        (baji_photo_parse_json_string(obj_start,
                                      obj_end,
                                      "remote_path",
                                      item->remote_path,
                                      sizeof(item->remote_path)) != 0)) {
        return -1;
    }
    if (baji_photo_manifest_remote_path_valid(item->remote_path) != 0) {
        return -1;
    }
    if (baji_photo_parse_json_u32(obj_start, obj_end, "file_size", &item->file_size) != 0) {
        return -1;
    }
    if (baji_photo_parse_json_hex_u32(obj_start, obj_end, "crc32", &item->crc32) != 0) {
        return -1;
    }
    if (baji_photo_parse_json_u32(obj_start, obj_end, "width", &width) != 0) {
        return -1;
    }
    if (baji_photo_parse_json_u32(obj_start, obj_end, "height", &height) != 0) {
        return -1;
    }
    if (baji_photo_parse_manifest_format(obj_start, obj_end, &item->format) != 0) {
        return -1;
    }

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        if ((width == 0u) || (height == 0u) || (item->file_size < 6u)) {
            return -1;
        }
        item->cf = LV_IMG_CF_RAW;
        if (baji_photo_store_build_gif_path(item->id, item->local_path, sizeof(item->local_path)) != 0) {
            return -1;
        }
    } else if (item->format == BAJI_PHOTO_FORMAT_BJP) {
        uint32_t expect_size;

        if (!baji_photo_true_color_dims_valid(width, height)) {
            return -1;
        }
        expect_size = (uint32_t)sizeof(baji_photo_file_header_t) +
                      baji_photo_true_color_data_size(width, height);
        if (item->file_size != expect_size) {
            return -1;
        }
        item->cf = LV_IMG_CF_TRUE_COLOR;
        if (baji_photo_store_build_photo_path(item->id, item->local_path, sizeof(item->local_path)) != 0) {
            return -1;
        }
    } else {
        uint32_t max_size = baji_photo_http_raw_format_max_size(item->format);

        if (!baji_photo_true_color_dims_valid(width, height)) {
            return -1;
        }
        if ((item->file_size == 0u) || (max_size == 0u) || (item->file_size > max_size)) {
            return -1;
        }
        item->cf = LV_IMG_CF_TRUE_COLOR;
        if (baji_photo_store_build_item_path(item->id,
                                             item->format,
                                             item->local_path,
                                             sizeof(item->local_path)) != 0) {
            return -1;
        }
    }

    item->width = (uint16_t)width;
    item->height = (uint16_t)height;
    return 0;
}

static int baji_photo_find_json_object_end(const char *obj_start, const char *end, const char **out_end)
{
    const char *p = obj_start;
    bool in_string = false;
    bool escaped = false;
    int depth = 0;

    if ((obj_start == NULL) || (out_end == NULL) || (obj_start >= end) || (*obj_start != '{')) {
        return -1;
    }

    while (p < end) {
        char c = *p++;

        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                *out_end = p;
                return 0;
            }
        }
    }

    return -1;
}

static int baji_photo_parse_manifest(const char *json,
                                     unsigned int len,
                                     baji_photo_manifest_item_t *items,
                                     unsigned int max_items,
                                     unsigned int *out_count)
{
    const char *start;
    const char *end;
    const char *array;
    const char *p;
    unsigned int count = 0;

    if ((json == NULL) || (items == NULL) || (out_count == NULL)) {
        return -1;
    }

    *out_count = 0;
    start = json;
    end = json + len;
    array = baji_photo_find_json_key(start, end, "result");
    if (array == NULL) {
        return -1;
    }

    array = baji_photo_skip_ws(array, end);
    if ((array >= end) || (*array != '[')) {
        return -1;
    }
    p = array + 1;

    while ((p < end) && (count < max_items)) {
        const char *obj_end;

        p = baji_photo_skip_ws(p, end);
        if ((p >= end) || (*p == ']')) {
            break;
        }
        if (*p != '{') {
            ++p;
            continue;
        }
        if (baji_photo_find_json_object_end(p, end, &obj_end) != 0) {
            return -1;
        }
        if (baji_photo_parse_manifest_item(p, obj_end, &items[count]) == 0) {
            ++count;
        }
        p = obj_end;
    }

    *out_count = count;
    return 0;
}

static void baji_photo_http_event_cb(liot_http_client_t *client, int evt, int evt_code, void *arg)
{
    baji_photo_http_ctx_t *ctx = (baji_photo_http_ctx_t *)arg;

    if (ctx == NULL) {
        return;
    }

    switch (evt) {
    case LIOT_HTTPC_SESSION_OPEN:
        if (evt_code != LIOT_HTTPC_SUCCESS) {
            BAJI_PHOTO_HTTP_TRACE("evt open fail err=%d", evt_code);
            if (ctx->err == LIOT_HTTPC_SUCCESS) {
                ctx->err = evt_code;
            }
            ctx->resp_done = true;
            baji_photo_http_signal(ctx->sem_resp);
        }
        break;

    case LIOT_HTTPC_RESPONSE_STATUS:
        if (evt_code == LIOT_HTTPC_SUCCESS) {
            char *date = NULL;
            char *location = NULL;

            (void)liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &ctx->status_code);
            (void)liot_httpc_getinfo(client, LIOT_HTTPC_CHUNK_ENCODE, &ctx->chunk_encode);
            (void)liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_LEN, &ctx->content_len);
            (void)liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_RANGE, &ctx->content_range);
            (void)liot_httpc_getinfo(client, LIOT_HTTPC_DATE, &date);
            ctx->saw_status = true;
            BAJI_PHOTO_HTTP_TRACE("evt status http=%d chunk=%d content_len=%d content_range=%d recv=%u",
                                  ctx->status_code,
                                  ctx->chunk_encode,
                                  ctx->content_len,
                                  ctx->content_range,
                                  ctx->len);
            if (date != NULL) {
                BAJI_PHOTO_HTTP_TRACE("evt date %s", date);
                liot_rtos_free(date);
            }
            if ((ctx->status_code >= 300) && (ctx->status_code < 400)) {
                (void)liot_httpc_getinfo(client, LIOT_HTTPC_LOCATION, &location);
                if (location != NULL) {
                    BAJI_PHOTO_HTTP_TRACE("evt redirect location=%s", location);
                    liot_rtos_free(location);
                }
            }
        } else {
            BAJI_PHOTO_HTTP_TRACE("evt status fail err=%d recv=%u",
                                  evt_code,
                                  ctx->len);
            if (ctx->err == LIOT_HTTPC_SUCCESS) {
                ctx->err = evt_code;
            }
            ctx->resp_done = true;
            baji_photo_http_signal(ctx->sem_resp);
        }
        break;

    case LIOT_HTTPC_RESPONSE_COMPLETE:
        ctx->saw_complete = true;
        BAJI_PHOTO_HTTP_TRACE("evt complete err=%d status=%d recv=%u",
                              evt_code,
                              ctx->status_code,
                              ctx->len);
        if (ctx->err == LIOT_HTTPC_SUCCESS) {
            ctx->err = evt_code;
        }
        ctx->resp_done = true;
        baji_photo_http_signal(ctx->sem_resp);
        break;

    case LIOT_HTTPC_RESPONSE_TIMEOUT:
        BAJI_PHOTO_HTTP_TRACE("evt timeout status=%d recv=%u",
                              ctx->status_code,
                              ctx->len);
        if (ctx->err == LIOT_HTTPC_SUCCESS) {
            ctx->err = LIOT_HTTPC_ERR_TIMEOUT;
        }
        ctx->resp_done = true;
        baji_photo_http_signal(ctx->sem_resp);
        break;

    case LIOT_HTTPC_SESSION_CLOSE:
        ctx->saw_close = true;
        ctx->close_wait_required = false;
        BAJI_PHOTO_HTTP_TRACE("evt close status=%d recv=%u err=%d complete=%d",
                              ctx->status_code,
                              ctx->len,
                              ctx->err,
                              ctx->saw_complete ? 1 : 0);
        baji_photo_http_signal(ctx->sem_close);
        break;

    default:
        break;
    }
}

static int baji_photo_http_write_cb(liot_http_client_t *client,
                                    void *arg,
                                    char *data,
                                    int size,
                                    unsigned char end)
{
    baji_photo_http_ctx_t *ctx = (baji_photo_http_ctx_t *)arg;

    (void)client;

    if ((ctx == NULL) || (data == NULL) || (size <= 0)) {
        return 0;
    }

    if ((ctx->len + (unsigned int)size) > ctx->cap) {
        if (ctx->err == LIOT_HTTPC_SUCCESS) {
            ctx->err = ctx->write_to_file ? LIOT_EXTFLASH_NO_SPACE : LIOT_HTTPC_ERR_OUT_OF_MEM;
        }
        BAJI_PHOTO_HTTP_TRACE("body overflow recv=%u chunk=%d cap=%u",
                              ctx->len,
                              size,
                              ctx->cap);
        baji_photo_http_stop_client(client, ctx, "overflow");
        return 0;
    }

    if ((ctx->len == 0u) ||
        ((ctx->content_len > 0) && ((ctx->len + (unsigned int)size) >= (unsigned int)ctx->content_len)) ||
        (end != 0u)) {
        BAJI_PHOTO_HTTP_TRACE("body chunk size=%d recv=%u/%d end=%u",
                              size,
                              ctx->len + (unsigned int)size,
                              ctx->content_len,
                              (unsigned int)end);
    }

    if (ctx->write_to_file) {
        int write_ret;
        int lock_ret;

        lock_ret = baji_photo_store_access_begin();
        if (lock_ret != 0) {
            if (ctx->err == LIOT_HTTPC_SUCCESS) {
                ctx->err = lock_ret;
            }
            BAJI_PHOTO_HTTP_TRACE("body file lock fail ret=%d recv=%u",
                                  lock_ret,
                                  ctx->len);
            baji_photo_http_stop_client(client, ctx, "file_lock_fail");
            return 0;
        }
        write_ret = liot_fwrite_ext(data, (size_t)size, 1, ctx->file_fd);
        baji_photo_store_access_end();
        if (write_ret != size) {
            if (ctx->err == LIOT_HTTPC_SUCCESS) {
                ctx->err = LIOT_EXTFLASH_WRITE_FAIL;
            }
            BAJI_PHOTO_HTTP_TRACE("body file write fail ret=%d size=%d recv=%u",
                                  write_ret,
                                  size,
                                  ctx->len);
            baji_photo_http_stop_client(client, ctx, "file_write_fail");
            return 0;
        }
    } else {
        memcpy(ctx->buf + ctx->len, data, (unsigned int)size);
    }
    ctx->len += (unsigned int)size;
    return size;
}

bool baji_photo_network_is_ready(void)
{
    baji_photo_net_observation_t obs;

    baji_photo_network_observe(baji_photo_net_default(), &obs);
    return (obs.state != BAJI_PHOTO_NET_OBS_NOT_READY);
}

static int baji_photo_network_prepare_trace(uint32_t trace_id,
                                            uint32_t retry_deadline_ms,
                                            uint32_t next_retry_ms)
{
    baji_photo_net_observation_t obs;

    baji_photo_network_observe(baji_photo_net_default(), &obs);
    baji_photo_http_log_network_state("prepare",
                                      trace_id,
                                      &obs,
                                      retry_deadline_ms,
                                      next_retry_ms);
    return 0;
}

int baji_photo_network_prepare(void)
{
    return baji_photo_network_prepare_trace(0u, 0u, 0u);
}

static int baji_photo_http_pick_primary_pdp_cid(int *out_alt_pdp_cid,
                                                const char **out_net_state,
                                                const char **out_reason)
{
    baji_photo_net_observation_t obs;

    if (out_alt_pdp_cid != NULL) {
        *out_alt_pdp_cid = BAJI_PHOTO_HTTP_PDP_CID;
    }
    if (out_net_state != NULL) {
        *out_net_state = "cfg_only";
    }
    if (out_reason != NULL) {
        *out_reason = "configured";
    }
    if (BAJI_PHOTO_HTTP_PDP_CID == 0) {
        return -1;
    }

    memset(&obs, 0, sizeof(obs));
    baji_photo_network_observe(baji_photo_net_default(), &obs);
    if (out_net_state != NULL) {
        *out_net_state = baji_photo_network_obs_state_name(obs.state);
    }
    if (out_reason != NULL) {
        *out_reason = obs.reason;
    }
    if (out_alt_pdp_cid != NULL) {
        *out_alt_pdp_cid = BAJI_PHOTO_HTTP_PDP_CID;
    }
    return -1;
}

static int baji_photo_http_perform_url_once(const char *url_str,
                                            const char *request_header,
                                            bool text_response,
                                            unsigned int *out_len,
                                            int pdp_cid,
                                            const char **out_stage)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    liot_http_client_t client = 0;
    liot_httpc_url_s url;
    char url_summary[96];
    char header_summary[80];
    int ret = LIOT_HTTPC_SUCCESS;
    int wait_ret = LIOT_HTTPC_SUCCESS;
    int close_wait_ret = LIOT_HTTPC_SUCCESS;
    int release_ret = LIOT_HTTPC_SUCCESS;
    int stop_ret = LIOT_HTTPC_SUCCESS;
    int was_running = 0;
    bool body_complete = false;
    bool body_truncated = false;
    bool payload_accepted = false;
    bool url_owned_by_client = false;
    bool stop_issued = false;
    const char *stage = "url";
    unsigned int final_len = 0u;
    int final_status = -1;
    int final_content_len = -1;

    if ((url_str == NULL) || (out_len == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (text_response && ((ctx->buf == NULL) || (ctx->cap == 0u))) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    *out_len = 0;
    memset(&url, 0, sizeof(url));
    baji_photo_http_build_url_summary(url_str, url_summary, sizeof(url_summary));
    baji_photo_http_build_header_summary(request_header, header_summary, sizeof(header_summary));

    if (!liot_httpc_url_parse((char *)url_str, &url)) {
        ret = LIOT_HTTPC_EMPTY_URL;
        goto exit;
    }

    stage = "new";
    ret = liot_httpc_new(&client, baji_photo_http_event_cb, ctx);
    if (ret != LIOT_HTTPC_SUCCESS) {
        goto exit;
    }

    stage = "sim";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SIM_ID, BAJI_PHOTO_HTTP_SIM_ID);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_SIM_ID, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_SIM_ID, ret)) {
        goto exit;
    }
    if (pdp_cid >= 0) {
        stage = "cid";
        ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_PDPCID, pdp_cid);
        baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_PDPCID, ret);
        if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_PDPCID, ret)) {
            goto exit;
        }
    } else {
        BAJI_PHOTO_HTTP_TRACE("bind auto target=%s", url_summary);
    }
    stage = "method";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_GET);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_METHOD, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_METHOD, ret)) {
        goto exit;
    }
    if ((request_header != NULL) && (request_header[0] != '\0')) {
        stage = "header";
        ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, (void *)request_header);
        baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, ret);
        if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, ret)) {
            goto exit;
        }
    }
    /*
     * After LIOT_HTTP_CLIENT_OPT_URL succeeds, the SDK owns url.host/url.uri and
     * releases them during client cleanup. Before that point, failure paths must free them.
     */
    stage = "urlopt";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_URL, &url);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_URL, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_URL, ret)) {
        goto exit;
    }
    url_owned_by_client = true;
    stage = "wfunc";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, baji_photo_http_write_cb);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, ret)) {
        goto exit;
    }
    stage = "wdata";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ctx);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_WRITE_DATA, ret)) {
        goto exit;
    }
    stage = "rtmo";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT, BAJI_PHOTO_HTTP_TIMEOUT_MS);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT, ret)) {
        goto exit;
    }
    stage = "stmo";
    ret = liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT, BAJI_PHOTO_HTTP_TIMEOUT_MS);
    baji_photo_http_setopt_diag(&client, LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT, ret);
    if (!baji_photo_http_setopt_can_continue(LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT, ret)) {
        goto exit;
    }

    BAJI_PHOTO_HTTP_TRACE("GET start target=%s header=%s cap=%u pdp_cid=%d",
                          url_summary,
                          header_summary,
                          ctx->cap,
                          pdp_cid);
    stage = "perform";
    ret = liot_httpc_perform(&client);
    if (ret == LIOT_HTTPC_SUCCESS) {
        ctx->close_wait_required = true;
        wait_ret = baji_photo_http_wait_until(ctx->sem_resp,
                                              &ctx->resp_done,
                                              BAJI_PHOTO_HTTP_TIMEOUT_MS + BAJI_PHOTO_HTTP_RESP_WAIT_GRACE_MS);
        BAJI_PHOTO_HTTP_TRACE("GET first wait target=%s ret=%d done=%d complete=%d close=%d status=%d recv=%u",
                              url_summary,
                              wait_ret,
                              ctx->resp_done ? 1 : 0,
                              ctx->saw_complete ? 1 : 0,
                              ctx->saw_close ? 1 : 0,
                              ctx->status_code,
                              ctx->len);
        body_complete = baji_photo_http_body_complete(ctx);
        body_truncated = baji_photo_http_body_truncated(ctx);
        if (body_truncated) {
            BAJI_PHOTO_HTTP_TRACE("GET body truncated target=%s status=%d content_len=%d recv=%u close=%d complete=%d",
                                  url_summary,
                                  ctx->status_code,
                                  ctx->content_len,
                                  ctx->len,
                                  ctx->saw_close ? 1 : 0,
                                  ctx->saw_complete ? 1 : 0);
        }
        if (wait_ret != 0) {
            BAJI_PHOTO_HTTP_TRACE("GET wait fail target=%s ret=%d status=%d recv=%u",
                                  url_summary,
                                  wait_ret,
                                  ctx->status_code,
                                  ctx->len);
        } else if (body_complete) {
            BAJI_PHOTO_HTTP_TRACE("GET payload ready target=%s status=%d content_len=%d recv=%u",
                                  url_summary,
                                  ctx->status_code,
                                  ctx->content_len,
                                  ctx->len);
            if (ctx->err == LIOT_HTTPC_SUCCESS) {
                final_len = ctx->len;
                final_status = ctx->status_code;
                final_content_len = ctx->content_len;
                payload_accepted = true;
                if (text_response) {
                    ctx->buf[final_len] = '\0';
                }
                *out_len = final_len;
                BAJI_PHOTO_HTTP_TRACE("GET payload accepted target=%s status=%d content_len=%d recv=%u",
                                      url_summary,
                                      final_status,
                                      final_content_len,
                                      final_len);
            }
        } else if ((ctx->status_code >= 300) && (ctx->status_code < 400)) {
            BAJI_PHOTO_HTTP_TRACE("GET redirect not followed target=%s status=%d recv=%u",
                                  url_summary,
                                  ctx->status_code,
                                  ctx->len);
        }
    } else {
        BAJI_PHOTO_HTTP_TRACE("GET submit fail target=%s ret=%d pdp_cid=%d",
                              url_summary,
                              ret,
                              pdp_cid);
    }

    BAJI_PHOTO_HTTP_TRACE("GET cleanup target=%s stage=%s ret=%d wait=%d ctx_err=%d done=%d complete=%d close=%d status=%d recv=%u",
                          url_summary,
                          stage,
                          ret,
                          wait_ret,
                          ctx->err,
                          ctx->resp_done ? 1 : 0,
                          ctx->saw_complete ? 1 : 0,
                          ctx->saw_close ? 1 : 0,
                          ctx->status_code,
                          ctx->len);
    if ((client != 0) && !ctx->saw_close) {
        if (!ctx->stop_requested) {
            was_running = liot_httpc_is_running(&client) ? 1 : 0;
            BAJI_PHOTO_HTTP_TRACE("GET stop target=%s running=%d", url_summary, was_running);
            stop_ret = liot_httpc_stop(&client);
            if (stop_ret != LIOT_HTTPC_SUCCESS) {
                BAJI_PHOTO_HTTP_TRACE("GET stop fail target=%s ret=%d", url_summary, stop_ret);
            }
        } else {
            BAJI_PHOTO_HTTP_TRACE("GET stop skip target=%s reason=already_requested", url_summary);
        }
        stop_issued = true;
        if (!ctx->saw_close) {
            close_wait_ret = baji_photo_http_wait_until(ctx->sem_close,
                                                        &ctx->saw_close,
                                                        BAJI_PHOTO_HTTP_CLOSE_WAIT_MS);
        }
    }
    BAJI_PHOTO_HTTP_TRACE("GET close wait ret=%d done=%d complete=%d close=%d status=%d recv=%u",
                          close_wait_ret,
                          ctx->resp_done ? 1 : 0,
                          ctx->saw_complete ? 1 : 0,
                          ctx->saw_close ? 1 : 0,
                          ctx->status_code,
                          ctx->len);
    if (client != 0) {
        unsigned int release_delay_ms = 0u;
        const char *release_delay_reason = "none";

        if (ctx->saw_close) {
            release_delay_ms = BAJI_PHOTO_HTTP_RELEASE_SETTLE_MS;
            release_delay_reason = "close_seen";
        } else if (stop_issued) {
            release_delay_ms = BAJI_PHOTO_HTTP_RELEASE_GRACE_MS;
            release_delay_reason = "stop_pending_close";
        }
        if (release_delay_ms != 0u) {
            BAJI_PHOTO_HTTP_TRACE("GET release settle target=%s reason=%s ms=%u close=%d stop=%d recv=%u",
                                  url_summary,
                                  release_delay_reason,
                                  release_delay_ms,
                                  ctx->saw_close ? 1 : 0,
                                  stop_issued ? 1 : 0,
                                  ctx->len);
            liot_rtos_task_sleep_ms(release_delay_ms);
        }
        BAJI_PHOTO_HTTP_TRACE("GET release start target=%s close=%d", url_summary, ctx->saw_close ? 1 : 0);
        release_ret = liot_httpc_release(&client);
        client = 0;
        BAJI_PHOTO_HTTP_TRACE("GET release done target=%s ret=%d", url_summary, release_ret);
    }

    if (payload_accepted) {
        baji_photo_network_record_http_result(baji_photo_net_default(), 0, final_status, true);
        if (close_wait_ret != 0) {
            BAJI_PHOTO_HTTP_TRACE("GET close wait ignored target=%s ret=%d after payload ready",
                                  url_summary,
                                  close_wait_ret);
        }
        if (release_ret != LIOT_HTTPC_SUCCESS) {
            BAJI_PHOTO_HTTP_TRACE("GET release ignored target=%s ret=%d after payload accepted",
                                  url_summary,
                                  release_ret);
        }
        BAJI_PHOTO_HTTP_TRACE("GET ok target=%s status=%d content_len=%d recv=%u",
                              url_summary,
                              final_status,
                              final_content_len,
                              final_len);
        ret = 0;
        goto exit;
    }

    if (ret != LIOT_HTTPC_SUCCESS) {
        goto exit;
    }
    if (body_truncated) {
        ret = BAJI_PHOTO_HTTP_IMAGE_ERR_BODY_TRUNCATED;
        goto exit;
    }
    if (wait_ret != LIOT_HTTPC_SUCCESS) {
        ret = wait_ret;
        goto exit;
    }
    if (ctx->err != LIOT_HTTPC_SUCCESS) {
        BAJI_PHOTO_HTTP_TRACE("GET failed target=%s err=%d status=%d recv=%u",
                              url_summary,
                              ctx->err,
                              ctx->status_code,
                              ctx->len);
        ret = ctx->err;
        goto exit;
    }
    if ((ctx->status_code >= 300) && (ctx->status_code < 400)) {
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto exit;
    }
    if (!body_complete) {
        if (baji_photo_http_body_truncated(ctx)) {
            BAJI_PHOTO_HTTP_TRACE("GET incomplete body_truncated target=%s status=%d chunk=%d content_len=%d recv=%u close=%d complete=%d",
                                  url_summary,
                                  ctx->status_code,
                                  ctx->chunk_encode,
                                  ctx->content_len,
                                  ctx->len,
                                  ctx->saw_close ? 1 : 0,
                                  ctx->saw_complete ? 1 : 0);
            ret = BAJI_PHOTO_HTTP_IMAGE_ERR_BODY_TRUNCATED;
        } else {
            BAJI_PHOTO_HTTP_TRACE("GET incomplete target=%s status=%d chunk=%d content_len=%d recv=%u complete=%d",
                                  url_summary,
                                  ctx->status_code,
                                  ctx->chunk_encode,
                                  ctx->content_len,
                                  ctx->len,
                                  ctx->saw_complete ? 1 : 0);
            ret = LIOT_HTTPC_ERR_UNKNOWN;
        }
        goto exit;
    }
    if (close_wait_ret != LIOT_HTTPC_SUCCESS) {
        ret = close_wait_ret;
        goto exit;
    }
    if (release_ret != LIOT_HTTPC_SUCCESS) {
        BAJI_PHOTO_HTTP_TRACE("GET release fail target=%s ret=%d status=%d recv=%u",
                              url_summary,
                              release_ret,
                              ctx->status_code,
                              ctx->len);
        ret = release_ret;
        goto exit;
    }

    baji_photo_network_record_http_result(baji_photo_net_default(), 0, ctx->status_code, true);

exit:
    if (out_stage != NULL) {
        *out_stage = stage;
    }
    if (ret != 0) {
        int log_status = final_status;

        if (log_status < 0) {
            log_status = ctx->status_code;
        }
        baji_photo_network_record_http_result(baji_photo_net_default(), ret, log_status, false);
        BAJI_PHOTO_HTTP_TRACE("GET done stage=%s target=%s ret=%d http_status=%d recv=%u error_class=%s",
                              stage,
                              url_summary,
                              ret,
                              log_status,
                              ctx->len,
                              baji_photo_http_error_class_name(ret));
        BAJI_PHOTO_HTTP_TRACE("GET ctx stage=%s resp_done=%d payload_complete=%d close_seen=%d ctx_err=%d "
                              "http_status=%d content_range=%d recv=%u header=%s",
                              stage,
                              ctx->resp_done ? 1 : 0,
                              ctx->saw_complete ? 1 : 0,
                              ctx->saw_close ? 1 : 0,
                              ctx->err,
                              ctx->status_code,
                              ctx->content_range,
                              ctx->len,
                              header_summary);
    }
    if (client != 0) {
        int cleanup_ret = liot_httpc_release(&client);

        BAJI_PHOTO_HTTP_TRACE("GET release fallback target=%s ret=%d", url_summary, cleanup_ret);
    }
    if (!url_owned_by_client) {
        baji_photo_http_url_free(&url);
    }
    return ret;
}

static int baji_photo_http_perform_url(const char *url_str,
                                       const char *request_header,
                                       bool text_response,
                                       unsigned int *out_len)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    const char *route_net_state = "cfg_only";
    const char *route_reason = "configured";
    int alt_pdp_cid = BAJI_PHOTO_HTTP_PDP_CID;
    int primary_pdp_cid;
    char *buf = ctx->buf;
    unsigned int cap = ctx->cap;
    LFILE_EXT file_fd = ctx->file_fd;
    bool write_to_file = ctx->write_to_file;
    const char *fail_stage = "init";
    int ret;

    primary_pdp_cid = baji_photo_http_pick_primary_pdp_cid(&alt_pdp_cid,
                                                           &route_net_state,
                                                           &route_reason);
    BAJI_PHOTO_HTTP_TRACE("route primary_pdp=%d alt_pdp=%d net_state=%s reason=%s cfg_pdp=%d",
                          primary_pdp_cid,
                          alt_pdp_cid,
                          route_net_state,
                          route_reason,
                          BAJI_PHOTO_HTTP_PDP_CID);
    ret = baji_photo_http_perform_url_once(url_str,
                                           request_header,
                                           text_response,
                                           out_len,
                                           primary_pdp_cid,
                                           &fail_stage);
    if (!baji_photo_http_should_retry_alt_pdp(ret, ctx, primary_pdp_cid, alt_pdp_cid)) {
        return ret;
    }

    BAJI_PHOTO_HTTP_TRACE("GET retry alt_pdp stage=%s from_cid=%d to_cid=%d net_state=%s reason=%s recv=%u status=%d",
                          fail_stage,
                          primary_pdp_cid,
                          alt_pdp_cid,
                          route_net_state,
                          route_reason,
                          ctx->len,
                          ctx->status_code);
    ret = baji_photo_http_ctx_prepare(buf, cap, file_fd, write_to_file);
    if (ret != 0) {
        return ret;
    }
    return baji_photo_http_perform_url_once(url_str,
                                            request_header,
                                            text_response,
                                            out_len,
                                            alt_pdp_cid,
                                            NULL);
}

static int baji_photo_http_get_url(const char *url_str,
                                   char *buf,
                                   unsigned int cap,
                                   bool text_response,
                                   unsigned int *out_len)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    baji_photo_http_ctx_read_view_t view;
    unsigned int body_cap;
    int ret;

    if ((url_str == NULL) || (buf == NULL) || (cap == 0u) || (out_len == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (text_response && (cap < 2u)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    *out_len = 0u;
    body_cap = text_response ? (cap - 1u) : cap;
    ret = baji_photo_http_ctx_prepare(buf, body_cap, 0, false);
    if (ret != 0) {
        return ret;
    }

    ret = baji_photo_http_perform_url(url_str, NULL, text_response, out_len);
    if (ret != 0) {
        return ret;
    }
    baji_photo_http_ctx_get_read_view(ctx, &view);
    if (!baji_photo_http_status_ok(view.status_code)) {
        BAJI_PHOTO_HTTP_TRACE("GET bad status url=%s status=%d recv=%u",
                              url_str,
                              view.status_code,
                              view.len);
        *out_len = 0u;
        return LIOT_HTTPC_ERR_UNKNOWN;
    }
    return 0;
}

static int baji_photo_http_get_url_retry(const char *url_str,
                                         char *buf,
                                         unsigned int cap,
                                         bool text_response,
                                         unsigned int *out_len,
                                         unsigned int max_attempts)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    baji_photo_http_ctx_read_view_t view;
    unsigned int attempt_count = (max_attempts == 0u) ? 1u : max_attempts;
    unsigned int attempt;
    int ret = LIOT_HTTPC_ERR_UNKNOWN;
    int status_code = -1;

    for (attempt = 0; attempt < attempt_count; ++attempt) {
        ret = baji_photo_http_get_url(url_str, buf, cap, text_response, out_len);
        if (ret == 0) {
            return 0;
        }

        baji_photo_http_ctx_get_read_view(ctx, &view);
        status_code = view.status_code;
        if (!baji_photo_http_error_retryable(ret, status_code) || ((attempt + 1u) >= attempt_count)) {
            break;
        }

        BAJI_PHOTO_HTTP_TRACE("GET retry url=%s attempt=%u/%u ret=%d status=%d",
                              url_str,
                              attempt + 1u,
                              attempt_count,
                              ret,
                              status_code);
        liot_rtos_task_sleep_ms(BAJI_PHOTO_HTTP_RETRY_BACKOFF_MS);
    }

    return ret;
}

static char baji_photo_http_hex_lower(char ch)
{
    if ((ch >= 'A') && (ch <= 'F')) {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static bool baji_photo_http_md5_match(const char *expected, const char *actual)
{
    unsigned int i;

    if ((expected == NULL) || (actual == NULL)) {
        return false;
    }

    for (i = 0; i < BAJI_PHOTO_IMAGE_MD5_HEX_LEN; ++i) {
        if ((expected[i] == '\0') || (actual[i] == '\0')) {
            return false;
        }
        if (baji_photo_http_hex_lower(expected[i]) != baji_photo_http_hex_lower(actual[i])) {
            return false;
        }
    }

    return (expected[BAJI_PHOTO_IMAGE_MD5_HEX_LEN] == '\0') &&
           (actual[BAJI_PHOTO_IMAGE_MD5_HEX_LEN] == '\0');
}

static bool baji_photo_http_parse_task_format(const baji_photo_image_task_t *task,
                                              baji_photo_format_t *out_format)
{
    const char *image_format;

    if ((task == NULL) || (out_format == NULL)) {
        return false;
    }

    image_format = task->image_format;
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
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
    if ((strcmp(image_format, "png") == 0) ||
        (strcmp(image_format, "PNG") == 0)) {
        *out_format = BAJI_PHOTO_FORMAT_PNG;
        return true;
    }
#endif

    return false;
}

static uint32_t baji_photo_http_task_data_offset(baji_photo_format_t format,
                                                 uint32_t payload_offset)
{
    if (format == BAJI_PHOTO_FORMAT_BJP) {
        return (uint32_t)sizeof(baji_photo_file_header_t) + payload_offset;
    }
    return payload_offset;
}

static uint32_t baji_photo_http_raw_format_max_size(baji_photo_format_t format)
{
    switch (format) {
    case BAJI_PHOTO_FORMAT_JPEG:
#if BAJI_PHOTO_ENABLE_JPEG_SUPPORT
        return BAJI_PHOTO_JPEG_MAX_COMPRESSED_SIZE;
#else
        return 0u;
#endif
    case BAJI_PHOTO_FORMAT_PNG:
#if BAJI_PHOTO_ENABLE_PNG_SUPPORT
        return BAJI_PHOTO_PNG_MAX_COMPRESSED_SIZE;
#else
        return 0u;
#endif
    default:
        return 0u;
    }
}

static uint32_t baji_photo_http_task_stored_size(baji_photo_format_t format,
                                                 uint32_t payload_size)
{
    return baji_photo_http_task_data_offset(format, payload_size);
}

static int baji_photo_http_validate_raw_file_payload(LFILE_EXT fd,
                                                     const baji_photo_manifest_item_t *item)
{
    uint8_t *data = NULL;
    int read_len;
    int ret = 0;

    if ((fd <= 0) || (item == NULL) || (item->file_size == 0u)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    data = (uint8_t *)liot_rtos_malloc(item->file_size);
    if (data == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    ret = liot_fseek_ext(fd, 0, LIOT_EXTFLASH_SEEK_SET);
    if (ret < 0) {
        ret = LIOT_EXTFLASH_SEEK_FAIL;
        goto cleanup;
    }

    read_len = liot_fread_ext(data, item->file_size, 1, fd);
    if (read_len != (int)item->file_size) {
        ret = BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
        goto cleanup;
    }

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        ret = baji_photo_validate_gif_payload(item, data, item->file_size);
    } else if (baji_photo_vpu_img_format_is_supported(item->format)) {
        ret = baji_photo_vpu_img_validate_payload(item, data, item->file_size);
    } else {
        ret = LIOT_HTTPC_ERR_INVALID_PARAM;
    }

cleanup:
    if (data != NULL) {
        liot_rtos_free(data);
    }
    return ret;
}

static int baji_photo_http_build_image_item(const baji_photo_image_task_t *task,
                                            uint32_t crc32,
                                            baji_photo_manifest_item_t *out_item)
{
    baji_photo_format_t format;
    size_t url_len;

    if ((task == NULL) || (out_item == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    memset(out_item, 0, sizeof(*out_item));
    (void)snprintf(out_item->id, sizeof(out_item->id), "%s", task->image_id);
    (void)snprintf(out_item->name, sizeof(out_item->name), "%s", task->image_id);
    url_len = strlen(task->image_url);
    if (url_len < sizeof(out_item->remote_path)) {
        memcpy(out_item->remote_path, task->image_url, url_len + 1u);
    } else {
        (void)snprintf(out_item->remote_path, sizeof(out_item->remote_path), "mqtt/%s", task->image_id);
    }
    if (baji_photo_store_build_item_path(task->image_id,
                                         format,
                                         out_item->local_path,
                                         sizeof(out_item->local_path)) != 0) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    out_item->file_size = baji_photo_http_task_stored_size(format, task->image_size);
    out_item->crc32 = crc32;
    out_item->width = task->image_width;
    out_item->height = task->image_height;
    out_item->cf = (format == BAJI_PHOTO_FORMAT_BJP) ? LV_IMG_CF_TRUE_COLOR : 0u;
    out_item->format = format;
    return 0;
}

static int baji_photo_http_prepare_tmp_file(const baji_photo_image_task_t *task,
                                            baji_photo_task_state_t *state,
                                            LFILE_EXT *out_fd,
                                            bool *out_range_resumed)
{
    baji_photo_format_t format;
    baji_photo_file_header_t header;
    liot_stat_ext_s st;
    LFILE_EXT fd = 0;
    bool range_resumed = false;
    const char *open_mode = "w+";
    int ret;

    if ((task == NULL) || (state == NULL) || (out_fd == NULL) || (out_range_resumed == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    *out_fd = 0;
    *out_range_resumed = false;
    memset(&st, 0, sizeof(st));
    memset(&header, 0, sizeof(header));
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    if (state->downloaded_size > task->image_size) {
        baji_photo_http_reset_download_state(state);
    }

    if (state->downloaded_size > 0u) {
        ret = liot_stat_ext(state->tmp_path, &st);
        if ((ret == LIOT_EXTFLASH_OK) &&
            (st.type == LIOT_EXTFLASH_TYPE_FILE) &&
            (st.size == baji_photo_http_task_stored_size(format, state->downloaded_size))) {
            range_resumed = true;
        } else {
            BAJI_PHOTO_HTTP_TRACE("tmp state mismatch reset task=%s size=%lu stat=%d tmp=%s",
                                  state->task_id,
                                  (unsigned long)state->downloaded_size,
                                  ret,
                                  state->tmp_path);
            baji_photo_http_reset_download_state(state);
        }
    }

    if (state->downloaded_size == 0u) {
        fd = liot_fopen_ext(state->tmp_path, "w+");
        if (fd <= 0) {
            baji_photo_store_access_end();
            return LIOT_EXTFLASH_OPEN_FAIL;
        }
        if (format == BAJI_PHOTO_FORMAT_BJP) {
            ret = liot_fwrite_ext(&header, sizeof(header), 1, fd);
            if (ret != (int)sizeof(header)) {
                (void)liot_fclose_ext(fd);
                baji_photo_store_access_end();
                return LIOT_EXTFLASH_WRITE_FAIL;
            }
            ret = liot_fsync_ext(fd);
            if (ret != LIOT_EXTFLASH_OK) {
                (void)liot_fclose_ext(fd);
                baji_photo_store_access_end();
                return LIOT_EXTFLASH_SYNC_FAIL;
            }
        }
    } else {
        open_mode = "r+";
        fd = liot_fopen_ext(state->tmp_path, "r+");
        if (fd <= 0) {
            baji_photo_store_access_end();
            return LIOT_EXTFLASH_OPEN_FAIL;
        }
    }

    ret = liot_fseek_ext(fd,
                         (long)baji_photo_http_task_data_offset(format, state->downloaded_size),
                         LIOT_EXTFLASH_SEEK_SET);
    if (ret < 0) {
        BAJI_PHOTO_HTTP_TRACE("tmp seek fail task=%s mode=%s downloaded=%lu offset=%lu fs_ret=%d path=%s",
                              task->task_id,
                              open_mode,
                              (unsigned long)state->downloaded_size,
                              (unsigned long)baji_photo_http_task_data_offset(format,
                                                                             state->downloaded_size),
                              ret,
                              state->tmp_path);
        (void)liot_fclose_ext(fd);
        baji_photo_store_access_end();
        return LIOT_EXTFLASH_SEEK_FAIL;
    }

    baji_photo_store_access_end();
    BAJI_PHOTO_HTTP_TRACE("tmp open task=%s mode=%s downloaded=%lu range_resumed=%d seek_offset=%lu path=%s",
                          task->task_id,
                          open_mode,
                          (unsigned long)state->downloaded_size,
                          range_resumed ? 1 : 0,
                          (unsigned long)baji_photo_http_task_data_offset(format,
                                                                         state->downloaded_size),
                          state->tmp_path);
    *out_fd = fd;
    *out_range_resumed = range_resumed;
    return 0;
}

static bool baji_photo_http_try_accept_partial_chunk(const baji_photo_image_task_t *task,
                                                     const baji_photo_http_ctx_t *ctx,
                                                     uint32_t start_offset,
                                                     uint32_t request_size,
                                                     bool allow_http_200,
                                                     baji_photo_http_chunk_request_result_t *result)
{
    if ((task == NULL) || (ctx == NULL) || (result == NULL)) {
        return false;
    }
    if (!baji_photo_http_body_truncated(ctx)) {
        return false;
    }
    if ((ctx->len == 0u) || (ctx->len > request_size)) {
        return false;
    }
    if (allow_http_200) {
        if (!baji_photo_http_status_payload_ok(ctx->status_code)) {
            return false;
        }
    } else if (ctx->status_code != 206) {
        return false;
    }

    result->http_status = ctx->status_code;
    result->chunk_len = ctx->len;
    result->partial_accepted = true;
    BAJI_PHOTO_HTTP_TRACE("op=%lu partial chunk accepted task=%s start=%lu recv=%u req=%lu status=%d content_len=%d",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          (unsigned long)start_offset,
                          result->chunk_len,
                          (unsigned long)request_size,
                          result->http_status,
                          ctx->content_len);
    return true;
}

static int baji_photo_http_request_plain_chunk(const baji_photo_image_task_t *task,
                                               uint32_t start_offset,
                                               LFILE_EXT fd,
                                               baji_photo_http_chunk_request_result_t *result)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    baji_photo_format_t format;
    uint32_t remain_size;
    int ret;

    if ((task == NULL) || (fd <= 0) || (result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    baji_photo_http_chunk_request_result_reset(result);
    result->start_offset = start_offset;
    result->range_request = false;
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (start_offset >= task->image_size) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }

    remain_size = task->image_size - start_offset;
    result->request_size = remain_size;
    ret = baji_photo_http_store_seek(fd,
                                     (long)baji_photo_http_task_data_offset(format, start_offset),
                                     LIOT_EXTFLASH_SEEK_SET);
    if (ret != LIOT_EXTFLASH_OK) {
        BAJI_PHOTO_HTTP_TRACE("plain seek fail task=%s offset=%lu ret=%d",
                              task->task_id,
                              (unsigned long)start_offset,
                              ret);
        return LIOT_EXTFLASH_SEEK_FAIL;
    }

    ret = baji_photo_http_ctx_prepare(NULL, remain_size, fd, true);
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("plain ctx prepare fail task=%s offset=%lu ret=%d cap=%lu",
                              task->task_id,
                              (unsigned long)start_offset,
                              ret,
                              (unsigned long)remain_size);
        return ret;
    }

    ret = baji_photo_http_perform_url(task->image_url, NULL, false, &result->chunk_len);
    result->http_status = ctx->status_code;
    if (ret != 0) {
        if (baji_photo_http_try_accept_partial_chunk(task,
                                                     ctx,
                                                     start_offset,
                                                     remain_size,
                                                     true,
                                                     result)) {
            return 0;
        }
        return ret;
    }
    if ((ctx->status_code == 200) ||
        (ctx->status_code == 206)) {
        return 0;
    }
    return LIOT_HTTPC_ERR_UNKNOWN;
}

static int baji_photo_http_request_range_chunk(const baji_photo_image_task_t *task,
                                               uint32_t start_offset,
                                               LFILE_EXT fd,
                                               baji_photo_http_chunk_request_result_t *result)
{
    baji_photo_http_ctx_t *ctx = baji_photo_http_ctx_default();
    baji_photo_format_t format;
    char range_header[BAJI_PHOTO_HTTP_RANGE_HEADER_MAX];
    uint32_t remain_size;
    uint32_t req_end_offset;
    int written;
    int ret;

    if ((task == NULL) || (fd <= 0) || (result == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    baji_photo_http_chunk_request_result_reset(result);
    result->start_offset = start_offset;
    result->range_request = true;
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (start_offset >= task->image_size) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }

    remain_size = task->image_size - start_offset;
    req_end_offset = start_offset + task->chunk_size;
    if ((req_end_offset == 0u) || (req_end_offset > task->image_size)) {
        req_end_offset = task->image_size;
    }
    req_end_offset -= 1u;
    result->request_size = req_end_offset - start_offset + 1u;

    written = snprintf(range_header,
                       sizeof(range_header),
                       "Range: bytes=%lu-%lu",
                       (unsigned long)start_offset,
                       (unsigned long)req_end_offset);
    if ((written <= 0) || ((unsigned int)written >= sizeof(range_header))) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_http_store_seek(fd,
                                     (long)baji_photo_http_task_data_offset(format, start_offset),
                                     LIOT_EXTFLASH_SEEK_SET);
    if (ret != LIOT_EXTFLASH_OK) {
        BAJI_PHOTO_HTTP_TRACE("range seek fail task=%s offset=%lu ret=%d",
                              task->task_id,
                              (unsigned long)start_offset,
                              ret);
        return LIOT_EXTFLASH_SEEK_FAIL;
    }

    ret = baji_photo_http_ctx_prepare(NULL, remain_size, fd, true);
    if (ret != 0) {
        BAJI_PHOTO_HTTP_TRACE("range ctx prepare fail task=%s offset=%lu ret=%d cap=%lu",
                              task->task_id,
                              (unsigned long)start_offset,
                              ret,
                              (unsigned long)remain_size);
        return ret;
    }

    ret = baji_photo_http_perform_url(task->image_url, range_header, false, &result->chunk_len);
    result->http_status = ctx->status_code;
    if (ret != 0) {
        if (baji_photo_http_try_accept_partial_chunk(task,
                                                     ctx,
                                                     start_offset,
                                                     req_end_offset - start_offset + 1u,
                                                     start_offset == 0u,
                                                     result)) {
            return 0;
        }
        return ret;
    }
    if ((start_offset == 0u) && (ctx->status_code == 200)) {
        return 0;
    }
    if (ctx->status_code == 206) {
        return 0;
    }
    if (ctx->status_code == 416) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_RESUME;
    }
    if (ctx->status_code == 200) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_UNSUPPORTED;
    }
    return LIOT_HTTPC_ERR_UNKNOWN;
}

static int baji_photo_http_verify_tmp_file(LFILE_EXT fd,
                                           const baji_photo_image_task_t *task,
                                           baji_photo_manifest_item_t *out_item)
{
    static const char *const k_hex = "0123456789abcdef";
    baji_photo_format_t format;
    baji_photo_file_header_t header;
    mbedtls_md5_context md5_ctx;
    uint8_t digest[16];
    uint8_t *buf = NULL;
    char md5_hex[BAJI_PHOTO_IMAGE_MD5_HEX_LEN + 1u];
    uint32_t remain_size;
    uint32_t crc32 = 0xFFFFFFFFu;
    unsigned int i;
    int ret = 0;

    if ((fd <= 0) || (task == NULL) || (out_item == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_store_access_begin();
    if (ret != 0) {
        return ret;
    }

    memset(&header, 0, sizeof(header));
    memset(digest, 0, sizeof(digest));
    memset(md5_hex, 0, sizeof(md5_hex));
    mbedtls_md5_init(&md5_ctx);

    buf = (uint8_t *)liot_rtos_malloc(BAJI_PHOTO_RANGE_VERIFY_BUF_SIZE);
    if (buf == NULL) {
        ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        goto cleanup;
    }
    if (mbedtls_md5_starts_ret(&md5_ctx) != 0) {
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto cleanup;
    }
    ret = liot_fseek_ext(fd,
                         (long)baji_photo_http_task_data_offset(format, 0u),
                         LIOT_EXTFLASH_SEEK_SET);
    if (ret < 0) {
        ret = LIOT_EXTFLASH_SEEK_FAIL;
        goto cleanup;
    }

    remain_size = task->image_size;
    while (remain_size > 0u) {
        unsigned int read_size = BAJI_PHOTO_RANGE_VERIFY_BUF_SIZE;
        int read_len;

        if (read_size > remain_size) {
            read_size = remain_size;
        }
        read_len = liot_fread_ext(buf, read_size, 1, fd);
        if (read_len != (int)read_size) {
            ret = BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
            goto cleanup;
        }
        crc32 = baji_photo_store_crc32_update(crc32, buf, read_size);
        if (mbedtls_md5_update_ret(&md5_ctx, buf, read_size) != 0) {
            ret = LIOT_HTTPC_ERR_UNKNOWN;
            goto cleanup;
        }
        remain_size -= read_size;
    }

    crc32 = baji_photo_store_crc32_finish(crc32);
    if (mbedtls_md5_finish_ret(&md5_ctx, digest) != 0) {
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto cleanup;
    }
    for (i = 0; i < sizeof(digest); ++i) {
        md5_hex[i * 2u] = k_hex[(digest[i] >> 4) & 0x0Fu];
        md5_hex[i * 2u + 1u] = k_hex[digest[i] & 0x0Fu];
    }
    md5_hex[BAJI_PHOTO_IMAGE_MD5_HEX_LEN] = '\0';
    if (!baji_photo_http_md5_match(task->md5, md5_hex)) {
        BAJI_PHOTO_HTTP_TRACE("md5 mismatch task=%s expect=%.8s actual=%.8s",
                              task->task_id,
                              task->md5,
                              md5_hex);
        ret = BAJI_PHOTO_HTTP_IMAGE_ERR_MD5_MISMATCH;
        goto cleanup;
    }

    header.magic = (format == BAJI_PHOTO_FORMAT_BJP) ? BAJI_PHOTO_MAGIC : 0u;
    header.width = task->image_width;
    header.height = task->image_height;
    header.cf = (format == BAJI_PHOTO_FORMAT_BJP) ? LV_IMG_CF_TRUE_COLOR : 0u;
    header.data_size = task->image_size;
    header.crc32 = crc32;

    if (format == BAJI_PHOTO_FORMAT_BJP) {
        ret = liot_fseek_ext(fd, 0, LIOT_EXTFLASH_SEEK_SET);
        if (ret < 0) {
            ret = LIOT_EXTFLASH_SEEK_FAIL;
            goto cleanup;
        }
        ret = liot_fwrite_ext(&header, sizeof(header), 1, fd);
        if (ret != (int)sizeof(header)) {
            ret = LIOT_EXTFLASH_WRITE_FAIL;
            goto cleanup;
        }
        ret = liot_fsync_ext(fd);
        if (ret != LIOT_EXTFLASH_OK) {
            ret = LIOT_EXTFLASH_SYNC_FAIL;
            goto cleanup;
        }
    }

    ret = baji_photo_http_build_image_item(task, crc32, out_item);
    if ((ret == 0) && (format != BAJI_PHOTO_FORMAT_BJP)) {
        ret = baji_photo_http_validate_raw_file_payload(fd, out_item);
    }
    if (ret == 0) {
        BAJI_PHOTO_HTTP_TRACE("verify ok op=%lu task=%s image=%s hdr=%ux%u data=%lu file=%lu crc=%08lX",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              (unsigned int)header.width,
                              (unsigned int)header.height,
                              (unsigned long)header.data_size,
                              (unsigned long)out_item->file_size,
                              (unsigned long)header.crc32);
        baji_photo_http_marker_verify_ok(task, &header, out_item);
    }

cleanup:
    baji_photo_store_access_end();
    if (buf != NULL) {
        liot_rtos_free(buf);
    }
    mbedtls_md5_free(&md5_ctx);
    return ret;
}

int baji_photo_http_download_image_task(const baji_photo_image_task_t *task,
                                        baji_photo_task_state_t *state,
                                        baji_photo_http_image_progress_cb_t progress_cb,
                                        void *progress_ctx,
                                        baji_photo_manifest_item_t *out_item,
                                        baji_photo_http_image_result_t *out_result)
{
    baji_photo_http_ctx_t *http_ctx = baji_photo_http_ctx_default();
    baji_photo_format_t format;
    baji_photo_http_image_result_t local_result;
    char url_summary[96];
    uint32_t expected_size;
    uint32_t local_max_size;
    LFILE_EXT fd = 0;
    int ret = 0;
    bool allow_full_restart = true;
    unsigned int failure_count = 0u;

    if ((task == NULL) || (state == NULL) || (out_item == NULL)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }
    if (!baji_photo_http_parse_task_format(task, &format)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    if (out_result == NULL) {
        out_result = &local_result;
    }
    baji_photo_http_result_reset(out_result);
    out_result->downloaded_size = state->downloaded_size;
    expected_size = (format == BAJI_PHOTO_FORMAT_BJP) ?
                        baji_photo_true_color_data_size(task->image_width, task->image_height) :
                        task->image_size;
    local_max_size = baji_photo_http_raw_format_max_size(format);
    baji_photo_http_build_url_summary(task->image_url, url_summary, sizeof(url_summary));
    BAJI_PHOTO_MARK_TRACE("HTTP_DOWNLOAD_BEGIN op=%lu task=%s image=%s format=%s size=%lu downloaded=%lu range=%d heap_min=%lu",
                          (unsigned long)task->trace_id,
                          baji_photo_diag_id_tail(task->task_id),
                          baji_photo_diag_id_tail(task->image_id),
                          task->image_format,
                          (unsigned long)task->image_size,
                          (unsigned long)state->downloaded_size,
                          task->support_range ? 1 : 0,
                          (unsigned long)liot_xPortGetMinimumEverFreeHeapSize());
    BAJI_PHOTO_HTTP_TRACE("op=%lu task start task=%s image=%s format=%s dims=%ux%u size=%lu expect=%lu downloaded=%lu progress=%lu%% remain=%lu range=%d local_max=%lu",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          task->image_id,
                          task->image_format,
                          (unsigned int)task->image_width,
                          (unsigned int)task->image_height,
                          (unsigned long)task->image_size,
                          (unsigned long)expected_size,
                          (unsigned long)state->downloaded_size,
                          (unsigned long)baji_photo_http_progress_percent(state->downloaded_size,
                                                                          task->image_size),
                          (unsigned long)((state->downloaded_size < task->image_size) ?
                                              (task->image_size - state->downloaded_size) :
                                              0u),
                          task->support_range ? 1 : 0,
                          (unsigned long)local_max_size);
    BAJI_PHOTO_HTTP_TRACE("op=%lu task source task=%s chunk=%lu url=%s",
                          (unsigned long)task->trace_id,
                          task->task_id,
                          (unsigned long)task->chunk_size,
                          url_summary);

    if ((task->image_width == 0u) || (task->image_height == 0u) ||
        (task->image_width > BAJI_PHOTO_IMG_W) || (task->image_height > BAJI_PHOTO_IMG_H)) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }
    if (format == BAJI_PHOTO_FORMAT_BJP) {
        if (!baji_photo_true_color_dims_valid(task->image_width, task->image_height) ||
            (task->image_size != expected_size)) {
            return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
        }
    } else if (task->image_size == 0u) {
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }
    if ((local_max_size != 0u) && (task->image_size > local_max_size)) {
        BAJI_PHOTO_HTTP_TRACE("op=%lu task reject local_size_limit task=%s image=%s format=%s size=%lu local_max=%lu downloaded=%lu",
                              (unsigned long)task->trace_id,
                              task->task_id,
                              task->image_id,
                              task->image_format,
                              (unsigned long)task->image_size,
                              (unsigned long)local_max_size,
                              (unsigned long)state->downloaded_size);
        return BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH;
    }
    if ((state->tmp_path[0] == '\0') || (state->final_path[0] == '\0')) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    ret = baji_photo_http_prepare_download_preflight(task, state, format);
    if (ret != 0) {
        return ret;
    }

restart_download:
    ret = baji_photo_http_prepare_download_attempt(task, state, &fd, out_result);
    if (ret != 0) {
        goto cleanup;
    }
    failure_count = 0u;

    while (state->downloaded_size < task->image_size) {
        baji_photo_http_chunk_request_result_t chunk_result;

        out_result->range_resumed = out_result->range_resumed || (state->downloaded_size > 0u);
        ret = baji_photo_http_request_download_chunk(task,
                                                     state,
                                                     http_ctx,
                                                     fd,
                                                     &chunk_result);
        out_result->http_status = chunk_result.http_status;
        if (ret != 0) {
            switch (baji_photo_http_process_chunk_error_step(task,
                                                             state,
                                                             &fd,
                                                             ret,
                                                             chunk_result.http_status,
                                                             &allow_full_restart,
                                                             &failure_count)) {
            case BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_RESTART:
                goto restart_download;
            case BAJI_PHOTO_HTTP_CHUNK_ERROR_STEP_CONTINUE:
                continue;
            default:
                break;
            }
            goto cleanup;
        }
        ret = baji_photo_http_process_ready_chunk(task,
                                                  state,
                                                  fd,
                                                  &chunk_result,
                                                  &failure_count,
                                                  out_result->range_resumed,
                                                  progress_cb,
                                                  progress_ctx,
                                                  out_result);
        if (ret != 0) {
            goto cleanup;
        }
    }

    switch (baji_photo_http_finalize_verified_download(task,
                                                       state,
                                                       &fd,
                                                       &allow_full_restart,
                                                       out_item,
                                                       out_result,
                                                       &ret)) {
    case BAJI_PHOTO_HTTP_FINALIZE_STEP_RESTART:
        goto restart_download;
    case BAJI_PHOTO_HTTP_FINALIZE_STEP_SUCCESS:
        return 0;
    default:
        break;
    }
    if (ret != 0) {
        goto cleanup;
    }
    return 0;

cleanup:
    return baji_photo_http_finalize_download_cleanup(task, state, &fd, ret, out_result);
}

static int baji_photo_http_get_manifest(char *buf, unsigned int cap, unsigned int *out_len)
{
    char url_buf[BAJI_PHOTO_HTTP_URL_MAX];
    int written;

    written = snprintf(url_buf,
                       sizeof(url_buf),
                       "http://%s:%u%s%s",
                       BAJI_PHOTO_HTTP_HOST,
                       BAJI_PHOTO_HTTP_PORT,
                       BAJI_PHOTO_HTTP_INDEX_URI,
                       BAJI_PHOTO_HTTP_IOT_ID);
    if ((written < 0) || ((unsigned int)written >= sizeof(url_buf))) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    BAJI_PHOTO_HTTP_TRACE("manifest fetch url=%s iot_id=%s cap=%u retry_max=%u",
                          url_buf,
                          BAJI_PHOTO_HTTP_IOT_ID,
                          cap,
                          BAJI_PHOTO_HTTP_MANIFEST_RETRY_MAX);

    return baji_photo_http_get_url_retry(url_buf,
                                         buf,
                                         cap,
                                         true,
                                         out_len,
                                         BAJI_PHOTO_HTTP_MANIFEST_RETRY_MAX);
}

static int baji_photo_validate_gif_payload(const baji_photo_manifest_item_t *item,
                                           const uint8_t *data,
                                           unsigned int len)
{
    GIF_INFO info;
    void *decoder;
    uint32_t crc;
    int ret;

    if ((item == NULL) || (data == NULL) || (len < 6u)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    crc = baji_photo_store_crc32_update(0xFFFFFFFFu, data, len);
    crc = baji_photo_store_crc32_finish(crc);
    if (crc != item->crc32) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }

    decoder = GifD_Create();
    if (decoder == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    ret = GifD_DecodeInfo(decoder, (unsigned char *)data, len, &info);
    GifD_Destroy(decoder);
    if (ret != 0) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }
    if ((info.uWidth != item->width) || (info.uHeight != item->height)) {
        return LIOT_HTTPC_ERR_UNKNOWN;
    }

    return 0;
}

static int baji_photo_download_one(const baji_photo_manifest_item_t *item)
{
    char url_buf[BAJI_PHOTO_HTTP_URL_MAX];
    char *photo_buf = NULL;
    unsigned int photo_len = 0;
    int free_size;
    int written;
    int ret;

    if (item == NULL) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    BAJI_PHOTO_HTTP_TRACE("download item start id=%s format=%u remote=%s local=%s size=%lu crc=%08lX",
                          item->id,
                          (unsigned int)item->format,
                          item->remote_path,
                          item->local_path,
                          (unsigned long)item->file_size,
                          (unsigned long)item->crc32);

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        if (item->file_size < 6u) {
            return LIOT_HTTPC_ERR_INVALID_PARAM;
        }
    } else if (item->format == BAJI_PHOTO_FORMAT_BJP) {
        if (item->file_size <= sizeof(baji_photo_file_header_t)) {
            return LIOT_HTTPC_ERR_INVALID_PARAM;
        }
    } else if (!baji_photo_vpu_img_format_is_supported(item->format) ||
               (item->file_size == 0u)) {
        return LIOT_HTTPC_ERR_INVALID_PARAM;
    }

    free_size = baji_photo_store_free_size();
    if ((free_size >= 0) && ((uint32_t)free_size < (item->file_size + 16u * 1024u))) {
        return LIOT_EXTFLASH_NO_SPACE;
    }

    photo_buf = (char *)liot_rtos_malloc(item->file_size);
    if (photo_buf == NULL) {
        return LIOT_HTTPC_ERR_OUT_OF_MEM;
    }

    written = snprintf(url_buf,
                       sizeof(url_buf),
                       "http://%s:%u%s%s",
                       BAJI_PHOTO_HTTP_HOST,
                       BAJI_PHOTO_HTTP_PORT,
                       BAJI_PHOTO_HTTP_DOWNLOAD_URI,
                       item->remote_path);
    if ((written < 0) || ((unsigned int)written >= sizeof(url_buf))) {
        ret = LIOT_HTTPC_ERR_INVALID_PARAM;
        goto cleanup;
    }

    ret = baji_photo_http_get_url_retry(url_buf,
                                        photo_buf,
                                        item->file_size,
                                        false,
                                        &photo_len,
                                        BAJI_PHOTO_HTTP_RETRY_MAX);
    if (ret != 0) {
        goto cleanup;
    }
    if (photo_len != item->file_size) {
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto cleanup;
    }
    BAJI_PHOTO_HTTP_TRACE("download item ok id=%s format=%u bytes=%u",
                          item->id,
                          (unsigned int)item->format,
                          photo_len);

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        ret = baji_photo_validate_gif_payload(item, (const uint8_t *)photo_buf, photo_len);
        if (ret != 0) {
            goto cleanup;
        }
        ret = baji_photo_store_write_gif_file(item->id, photo_buf, photo_len);
        goto cleanup;
    }

    if ((item->format == BAJI_PHOTO_FORMAT_JPEG) || (item->format == BAJI_PHOTO_FORMAT_PNG)) {
        ret = baji_photo_vpu_img_validate_payload(item, (const uint8_t *)photo_buf, photo_len);
        if (ret != 0) {
            goto cleanup;
        }
        ret = baji_photo_store_write_raw_file(item->id, item->format, photo_buf, photo_len);
        goto cleanup;
    }

    if (photo_len >= sizeof(baji_photo_file_header_t)) {
        const baji_photo_file_header_t *header = (const baji_photo_file_header_t *)photo_buf;
        const uint8_t *pixels = (const uint8_t *)photo_buf + sizeof(*header);
        uint32_t crc;
        uint32_t expect_data_size;

        if (!baji_photo_true_color_dims_valid(header->width, header->height)) {
            ret = LIOT_HTTPC_ERR_UNKNOWN;
            goto cleanup;
        }
        expect_data_size = baji_photo_true_color_data_size(header->width, header->height);

        if ((header->magic != BAJI_PHOTO_MAGIC) ||
            (header->data_size != expect_data_size) ||
            (photo_len != (sizeof(*header) + header->data_size)) ||
            (header->crc32 != item->crc32)) {
            ret = LIOT_HTTPC_ERR_UNKNOWN;
            goto cleanup;
        }

        crc = baji_photo_store_crc32_update(0xFFFFFFFFu, pixels, header->data_size);
        crc = baji_photo_store_crc32_finish(crc);
        if (crc != header->crc32) {
            ret = LIOT_HTTPC_ERR_UNKNOWN;
            goto cleanup;
        }
        BAJI_PHOTO_HTTP_TRACE("bjp verify ok id=%s size=%ux%u crc=%08lX",
                              item->id,
                              (unsigned int)header->width,
                              (unsigned int)header->height,
                              (unsigned long)header->crc32);
    } else {
        ret = LIOT_HTTPC_ERR_UNKNOWN;
        goto cleanup;
    }

    ret = baji_photo_store_write_photo_file(item->id, photo_buf, photo_len);

cleanup:
    if (photo_buf != NULL) {
        liot_rtos_free(photo_buf);
    }
    return ret;
}

static int baji_photo_find_item_by_id(const baji_photo_manifest_item_t *items,
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

static bool baji_photo_local_file_is_valid(const baji_photo_manifest_item_t *item)
{
    baji_photo_file_header_t header;
    liot_stat_ext_s st;
    uint32_t expect_size;

    if (item == NULL) {
        return false;
    }

    if (item->format == BAJI_PHOTO_FORMAT_GIF) {
        LFILE_EXT fd;
        uint8_t *buf;
        int read_len;
        int ret;

        memset(&st, 0, sizeof(st));
        if ((item->local_path[0] == '\0') ||
            (baji_photo_http_store_stat(item->local_path, &st) != LIOT_EXTFLASH_OK) ||
            (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
            (st.size != item->file_size)) {
            return false;
        }

        buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
        if (buf == NULL) {
            return false;
        }

        ret = baji_photo_store_access_begin();
        if (ret != 0) {
            liot_rtos_free(buf);
            return false;
        }
        fd = liot_fopen_ext(item->local_path, "r");
        if (fd <= 0) {
            liot_rtos_free(buf);
            baji_photo_store_access_end();
            return false;
        }

        read_len = liot_fread_ext(buf, (unsigned int)st.size, 1, fd);
        (void)liot_fclose_ext(fd);
        baji_photo_store_access_end();
        if (read_len != (int)st.size) {
            liot_rtos_free(buf);
            return false;
        }

        ret = baji_photo_validate_gif_payload(item, buf, (unsigned int)st.size);
        liot_rtos_free(buf);
        return (ret == 0);
    }

    if ((item->format == BAJI_PHOTO_FORMAT_JPEG) || (item->format == BAJI_PHOTO_FORMAT_PNG)) {
        LFILE_EXT fd;
        uint8_t *buf;
        int read_len;
        int ret;

        memset(&st, 0, sizeof(st));
        if ((item->local_path[0] == '\0') ||
            (baji_photo_http_store_stat(item->local_path, &st) != LIOT_EXTFLASH_OK) ||
            (st.type != LIOT_EXTFLASH_TYPE_FILE) ||
            (st.size != item->file_size)) {
            return false;
        }

        buf = (uint8_t *)liot_rtos_malloc((unsigned int)st.size);
        if (buf == NULL) {
            return false;
        }

        ret = baji_photo_store_access_begin();
        if (ret != 0) {
            liot_rtos_free(buf);
            return false;
        }
        fd = liot_fopen_ext(item->local_path, "r");
        if (fd <= 0) {
            liot_rtos_free(buf);
            baji_photo_store_access_end();
            return false;
        }

        read_len = liot_fread_ext(buf, (unsigned int)st.size, 1, fd);
        (void)liot_fclose_ext(fd);
        baji_photo_store_access_end();
        if (read_len != (int)st.size) {
            liot_rtos_free(buf);
            return false;
        }

        ret = baji_photo_vpu_img_validate_payload(item, buf, (unsigned int)st.size);
        liot_rtos_free(buf);
        return (ret == 0);
    }

    if (baji_photo_store_read_photo_header(item->id, &header) != 0) {
        return false;
    }

    if (!baji_photo_true_color_dims_valid(header.width, header.height)) {
        return false;
    }

    expect_size = (uint32_t)sizeof(header) + header.data_size;
    return (header.magic == BAJI_PHOTO_MAGIC) &&
           (header.cf == LV_IMG_CF_TRUE_COLOR) &&
           (header.data_size == baji_photo_true_color_data_size(header.width, header.height)) &&
           (header.crc32 == item->crc32) &&
           (expect_size == item->file_size);
}

static bool baji_photo_local_matches_remote(const baji_photo_manifest_item_t *local,
                                            const baji_photo_manifest_item_t *remote)
{
    if ((local == NULL) || (remote == NULL)) {
        return false;
    }

    return (local->file_size == remote->file_size) &&
           (local->crc32 == remote->crc32) &&
           (local->width == remote->width) &&
           (local->height == remote->height) &&
           (local->format == remote->format) &&
           baji_photo_local_file_is_valid(local);
}

static bool baji_photo_index_item_equals(const baji_photo_manifest_item_t *lhs,
                                         const baji_photo_manifest_item_t *rhs)
{
    const char *lhs_name;
    const char *rhs_name;

    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }

    lhs_name = (lhs->name[0] != '\0') ? lhs->name : lhs->id;
    rhs_name = (rhs->name[0] != '\0') ? rhs->name : rhs->id;

    return (strcmp(lhs->id, rhs->id) == 0) &&
           (strcmp(lhs_name, rhs_name) == 0) &&
           (strcmp(lhs->remote_path, rhs->remote_path) == 0) &&
           (lhs->file_size == rhs->file_size) &&
           (lhs->crc32 == rhs->crc32) &&
           (lhs->width == rhs->width) &&
           (lhs->height == rhs->height) &&
           (lhs->format == rhs->format);
}

static bool baji_photo_index_items_equal(const baji_photo_manifest_item_t *lhs,
                                         const baji_photo_manifest_item_t *rhs,
                                         unsigned int count)
{
    unsigned int i;

    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }

    for (i = 0; i < count; ++i) {
        if (!baji_photo_index_item_equals(&lhs[i], &rhs[i])) {
            return false;
        }
    }

    return true;
}

static int baji_photo_cleanup_stale(const baji_photo_manifest_item_t *local_items,
                                    unsigned int local_count,
                                    const baji_photo_manifest_item_t *remote_items,
                                    unsigned int remote_count)
{
    unsigned int i;

    if (local_items == NULL) {
        return 0;
    }

    for (i = 0; i < local_count; ++i) {
        if (baji_photo_find_item_by_id(remote_items, remote_count, local_items[i].id) < 0) {
            int ret = baji_photo_store_remove_item(&local_items[i]);

            if (ret != 0) {
                return ret;
            }
        }
    }

    return 0;
}

static void baji_photo_sync_report_progress(baji_photo_net_service_t *service,
                                            baji_photo_sync_state_t state,
                                            unsigned int current,
                                            unsigned int total)
{
    if (service == NULL) {
        return;
    }

    service->state = state;
    if (service->progress_cb != NULL) {
        service->progress_cb(state, (uint16_t)current, (uint16_t)total, service->done_ctx);
    }
}

static void baji_photo_sync_service_reset_callbacks(baji_photo_net_service_t *service)
{
    if (service == NULL) {
        return;
    }

    service->done_cb = NULL;
    service->progress_cb = NULL;
    service->done_ctx = NULL;
}

static void baji_photo_sync_service_set_idle(baji_photo_net_service_t *service)
{
    if (service == NULL) {
        return;
    }

    service->sync_task = NULL;
    service->busy = false;
    service->state = BAJI_PHOTO_SYNC_STATE_IDLE;
}

static void baji_photo_sync_task(void *arg)
{
    baji_photo_net_service_t *service = (baji_photo_net_service_t *)arg;
    baji_photo_sync_result_t result = {0};
    char *manifest_buf = NULL;
    baji_photo_manifest_item_t *manifest_items = NULL;
    baji_photo_manifest_item_t *local_items = NULL;
    baji_photo_manifest_item_t *result_items = NULL;
    unsigned int manifest_len = 0;
    unsigned int manifest_count = 0;
    unsigned int local_count = 0;
    unsigned int result_count = 0;
    unsigned int count = 0;
    int ret;

    if (service == NULL) {
        liot_rtos_task_delete(NULL);
        return;
    }

    baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_PREPARE_NET, 0u, 0u);
    BAJI_PHOTO_HTTP_TRACE("sync task start host=%s port=%u iot_id=%s sim=%u cid=%d",
                          BAJI_PHOTO_HTTP_HOST,
                          BAJI_PHOTO_HTTP_PORT,
                          BAJI_PHOTO_HTTP_IOT_ID,
                          (unsigned int)BAJI_PHOTO_HTTP_SIM_ID,
                          BAJI_PHOTO_HTTP_PDP_CID);
    ret = baji_photo_network_prepare();
    if (ret != 0) {
        goto done;
    }

    baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_MOUNT_STORE, 0u, 0u);
    ret = baji_photo_store_mount();
    if (ret == 0) {
        baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_FETCH_INDEX, 0u, 0u);
        local_items = (baji_photo_manifest_item_t *)liot_rtos_malloc(
            sizeof(baji_photo_manifest_item_t) * BAJI_PHOTO_DL_MAX);
        if (local_items == NULL) {
            ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        } else {
            ret = baji_photo_store_load_index(local_items, BAJI_PHOTO_DL_MAX, &local_count);
        }
    }

    if (ret != 0) {
        goto done;
    }

#if BAJI_PHOTO_HTTP_RAW_RGB565_VERIFY
    {
        baji_photo_manifest_item_t raw_item;
        uint8_t *photo_buf = NULL;
        unsigned int photo_len = 0u;
        bool matched = false;
        int local_idx;

        result.total = 1u;
        baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_DOWNLOAD_ONE, 1u, 1u);
        ret = baji_photo_raw_rgb565_fetch(&raw_item, &photo_buf, &photo_len);
        if (ret != 0) {
            BAJI_PHOTO_HTTP_TRACE("raw verify fetch failed ret=%d", ret);
            goto done;
        }

        local_idx = baji_photo_find_item_by_id(local_items, local_count, raw_item.id);
        if ((local_idx >= 0) && baji_photo_local_matches_remote(&local_items[local_idx], &raw_item)) {
            matched = true;
        }

        if (!matched) {
            baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_VERIFY_ONE, 1u, 1u);
            ret = baji_photo_raw_rgb565_write(&raw_item, photo_buf, photo_len);
            if (ret != 0) {
                liot_rtos_free(photo_buf);
                BAJI_PHOTO_HTTP_TRACE("raw verify write failed ret=%d", ret);
                goto done;
            }

            baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_COMMIT_ONE, 1u, 1u);
            ret = baji_photo_raw_rgb565_commit(&raw_item, local_items, local_count);
            if (ret != 0) {
                liot_rtos_free(photo_buf);
                BAJI_PHOTO_HTTP_TRACE("raw verify commit failed ret=%d", ret);
                goto done;
            }

            result.downloaded = 1u;
            BAJI_PHOTO_HTTP_TRACE("raw verify committed id=%s size=%ux%u",
                                  raw_item.id,
                                  (unsigned int)raw_item.width,
                                  (unsigned int)raw_item.height);
        } else {
            result.skipped = 1u;
            BAJI_PHOTO_HTTP_TRACE("raw verify skip unchanged id=%s", raw_item.id);
        }

        liot_rtos_free(photo_buf);
        manifest_count = 1u;
        goto done;
    }
#endif

    baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_FETCH_INDEX, 0u, 0u);
    manifest_buf = (char *)liot_rtos_malloc(BAJI_PHOTO_HTTP_MANIFEST_MAX);
    if (manifest_buf == NULL) {
        ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        goto done;
    }

    ret = baji_photo_http_get_manifest(manifest_buf,
                                       BAJI_PHOTO_HTTP_MANIFEST_MAX,
                                       &manifest_len);
    if (ret != 0) {
        goto done;
    }
    BAJI_PHOTO_HTTP_TRACE("manifest fetch ok len=%u", manifest_len);

    manifest_items = (baji_photo_manifest_item_t *)liot_rtos_malloc(
        sizeof(baji_photo_manifest_item_t) * BAJI_PHOTO_DL_MAX);
    if (manifest_items == NULL) {
        ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        goto done;
    }
    result_items = (baji_photo_manifest_item_t *)liot_rtos_malloc(
        sizeof(baji_photo_manifest_item_t) * BAJI_PHOTO_DL_MAX);
    if (result_items == NULL) {
        ret = LIOT_HTTPC_ERR_OUT_OF_MEM;
        goto done;
    }

    ret = baji_photo_parse_manifest(manifest_buf,
                                    manifest_len,
                                    manifest_items,
                                    BAJI_PHOTO_DL_MAX,
                                    &manifest_count);
    if (ret != 0) {
        goto done;
    }
    BAJI_PHOTO_HTTP_TRACE("manifest parse ok count=%u local_count=%u",
                          manifest_count,
                          local_count);

    result.total = (uint16_t)manifest_count;
    baji_photo_sync_report_progress(service, BAJI_PHOTO_SYNC_STATE_PLAN_DIFF, 0u, manifest_count);
    for (count = 0; count < manifest_count; ++count) {
        int local_idx = baji_photo_find_item_by_id(local_items,
                                                   local_count,
                                                   manifest_items[count].id);

        BAJI_PHOTO_HTTP_TRACE("manifest item[%u/%u] id=%s format=%u remote=%s local_match=%d",
                              count + 1u,
                              manifest_count,
                              manifest_items[count].id,
                              (unsigned int)manifest_items[count].format,
                              manifest_items[count].remote_path,
                              (local_idx >= 0) ? 1 : 0);

        if ((local_idx >= 0) &&
            baji_photo_local_matches_remote(&local_items[local_idx], &manifest_items[count])) {
            result_items[result_count] = manifest_items[count];
            ++result_count;
            ++result.skipped;
            baji_photo_sync_report_progress(service,
                                            BAJI_PHOTO_SYNC_STATE_PLAN_DIFF,
                                            count + 1u,
                                            manifest_count);
            continue;
        }

        baji_photo_sync_report_progress(service,
                                        BAJI_PHOTO_SYNC_STATE_DOWNLOAD_ONE,
                                        count + 1u,
                                        manifest_count);
        ret = baji_photo_download_one(&manifest_items[count]);
        if (ret == 0) {
            result_items[result_count] = manifest_items[count];
            ++result_count;
            ++result.downloaded;
        } else {
            ++result.failed;
            if ((local_idx >= 0) && baji_photo_local_file_is_valid(&local_items[local_idx])) {
                result_items[result_count] = local_items[local_idx];
                ++result_count;
            }
            ret = 0;
        }
    }

    baji_photo_sync_report_progress(service,
                                    BAJI_PHOTO_SYNC_STATE_CLEAN_STALE,
                                    manifest_count,
                                    manifest_count);
    ret = baji_photo_cleanup_stale(local_items, local_count, manifest_items, manifest_count);
    if (ret != 0) {
        goto done;
    }
    if ((result_count == local_count) &&
        baji_photo_index_items_equal(local_items, result_items, result_count)) {
        BAJI_PHOTO_HTTP_TRACE("index save skip identical count=%u", result_count);
        ret = 0;
    } else {
        ret = baji_photo_store_save_index(result_items, result_count);
    }

done:
    if (ret == 0) {
        result.total = (uint16_t)manifest_count;
        baji_photo_sync_report_progress(service,
                                        BAJI_PHOTO_SYNC_STATE_DONE,
                                        manifest_count,
                                        manifest_count);
    } else {
        result.failed = 1u;
        baji_photo_sync_report_progress(service,
                                        BAJI_PHOTO_SYNC_STATE_FAIL,
                                        0u,
                                        (manifest_count == 0u) ? 0u : manifest_count);
    }

    if (service->done_cb != NULL) {
        service->done_cb(&result, service->done_ctx);
    }

    baji_photo_sync_service_reset_callbacks(service);
    if (manifest_buf != NULL) {
        liot_rtos_free(manifest_buf);
    }
    if (manifest_items != NULL) {
        liot_rtos_free(manifest_items);
    }
    if (local_items != NULL) {
        liot_rtos_free(local_items);
    }
    if (result_items != NULL) {
        liot_rtos_free(result_items);
    }
    BAJI_PHOTO_HTTP_TRACE("sync task done ret=%d downloaded=%u skipped=%u failed=%u busy->0",
                          ret,
                          (unsigned int)result.downloaded,
                          (unsigned int)result.skipped,
                          (unsigned int)result.failed);
    baji_photo_sync_service_set_idle(service);
    liot_rtos_task_delete(NULL);
}

bool baji_photo_net_is_busy(void)
{
    return baji_photo_net_default()->busy;
}

baji_photo_sync_start_result_t baji_photo_net_request_sync(baji_photo_sync_done_cb_t done_cb,
                                                           baji_photo_sync_progress_cb_t progress_cb,
                                                           void *ctx)
{
    LiotOSStatus_t ret;
    baji_photo_net_service_t *service = baji_photo_net_default();

    if (service->busy) {
        BAJI_PHOTO_HTTP_TRACE("sync request busy state=%d task=%p",
                              (int)service->state,
                              service->sync_task);
        return BAJI_PHOTO_SYNC_START_BUSY;
    }

    service->busy = true;
    service->state = BAJI_PHOTO_SYNC_STATE_PREPARE_NET;
    service->done_cb = done_cb;
    service->progress_cb = progress_cb;
    service->done_ctx = ctx;

    ret = liot_rtos_task_create(&service->sync_task,
                                BAJI_PHOTO_SYNC_TASK_STACK,
                                BAJI_PHOTO_SYNC_TASK_PRIO,
                                "baji_photo_sync",
                                baji_photo_sync_task,
                                service);
    if (ret != 0) {
        baji_photo_sync_service_reset_callbacks(service);
        baji_photo_sync_service_set_idle(service);
        return BAJI_PHOTO_SYNC_START_ERROR;
    }

    return BAJI_PHOTO_SYNC_START_OK;
}

baji_photo_sync_state_t baji_photo_net_get_state(void)
{
    return baji_photo_net_default()->state;
}
