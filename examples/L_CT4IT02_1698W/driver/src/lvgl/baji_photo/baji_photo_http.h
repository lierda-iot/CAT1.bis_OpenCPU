#ifndef BAJI_PHOTO_HTTP_H
#define BAJI_PHOTO_HTTP_H

#include <stdbool.h>

#include "baji_photo_types.h"

typedef enum {
    BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_UNSUPPORTED = -32001,
    BAJI_PHOTO_HTTP_IMAGE_ERR_RANGE_RESUME = -32002,
    BAJI_PHOTO_HTTP_IMAGE_ERR_SIZE_MISMATCH = -32003,
    BAJI_PHOTO_HTTP_IMAGE_ERR_MD5_MISMATCH = -32004,
    BAJI_PHOTO_HTTP_IMAGE_ERR_PARSE_FAIL = -32005,
    BAJI_PHOTO_HTTP_IMAGE_ERR_BODY_TRUNCATED = -32006,
} baji_photo_http_image_error_t;

/* Keep legacy PDP-active classification distinct from extflash/sdk error codes. */
#define BAJI_PHOTO_HTTP_LEGACY_PDP_ACTIVE_FAIL (-32010)

typedef struct {
    int http_status;
    uint32_t downloaded_size;
    bool range_resumed;
} baji_photo_http_image_result_t;

typedef void (*baji_photo_http_image_progress_cb_t)(const baji_photo_image_task_t *task,
                                                    uint32_t downloaded_size,
                                                    uint32_t total_size,
                                                    bool range_resumed,
                                                    void *ctx);

bool baji_photo_network_is_ready(void);
int baji_photo_network_prepare(void);
int baji_photo_http_download_image_task(const baji_photo_image_task_t *task,
                                        baji_photo_task_state_t *state,
                                        baji_photo_http_image_progress_cb_t progress_cb,
                                        void *progress_ctx,
                                        baji_photo_manifest_item_t *out_item,
                                        baji_photo_http_image_result_t *out_result);
bool baji_photo_net_is_busy(void);
baji_photo_sync_start_result_t baji_photo_net_request_sync(baji_photo_sync_done_cb_t done_cb,
                                                           baji_photo_sync_progress_cb_t progress_cb,
                                                           void *ctx);
baji_photo_sync_state_t baji_photo_net_get_state(void);

#endif
