#include "app_scan_port_liot.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_osal.h"
#include "liot_decode.h"

typedef struct {
    app_scan_liot_config_t config;
    uint32_t frame_bytes;
    uint32_t required_capacity;
    uint32_t frame_count;
    uint32_t miss_count;
    bool configured;
    bool initialized;
    bool decoder_ready;
} app_scan_liot_ctx_t;

static int scan_liot_init(const app_scan_config_t *config);
static int scan_liot_deinit(void);
static int scan_liot_get_frame_buffer_capacity(const app_scan_config_t *config,
                                               uint32_t *capacity);
static int scan_liot_process_frame(const app_scan_frame_t *frame,
                                   app_scan_result_t *result,
                                   bool *matched);

static app_scan_liot_ctx_t s_scan_liot;

static const app_scan_port_ops_t s_scan_liot_ops = {
    .init = scan_liot_init,
    .deinit = scan_liot_deinit,
    .get_frame_buffer_capacity = scan_liot_get_frame_buffer_capacity,
    .process_frame = scan_liot_process_frame,
};

static app_scan_port_t s_scan_liot_port = {
    .caps = {
        .has_decode = true,
        .input_format = APP_SCAN_FRAME_FORMAT_GRAY8,
        .max_width = 640U,
        .max_height = 480U,
    },
    .ops = &s_scan_liot_ops,
};

static bool scan_liot_backend_available(app_scan_liot_backend_t backend)
{
    return backend == APP_SCAN_LIOT_BACKEND_QY;
}

static int scan_liot_calc_sizes(const app_scan_config_t *config,
                                uint32_t *frame_bytes,
                                uint32_t *required_capacity)
{
    uint32_t pixels;

    if (config == NULL || frame_bytes == NULL || required_capacity == NULL ||
        config->width == 0U || config->height == 0U ||
        config->input_format != APP_SCAN_FRAME_FORMAT_GRAY8) {
        return APP_ERR_INVALID_ARG;
    }

    pixels = (uint32_t)config->width * (uint32_t)config->height;
    if (pixels == 0U || pixels > (UINT32_MAX / 2U)) {
        return APP_ERR_NO_MEMORY;
    }

    *frame_bytes = pixels;
    *required_capacity = pixels * 2U;
    return APP_OK;
}

static bool scan_liot_should_log(uint32_t count, uint32_t interval)
{
    return count <= 3U || (count % interval) == 0U;
}

static bool scan_liot_decoder_init_ok(liot_errcode_decoder_e ret)
{
    int code = (int)ret;

    return code >= (int)LIOT_DECODER_SUCCESS &&
           code < (int)LIOT_DECODER_INIT_ERR;
}

static int scan_liot_init(const app_scan_config_t *config)
{
    uint32_t frame_bytes;
    uint32_t required_capacity;
    int ret;

    if (!s_scan_liot.configured ||
        !scan_liot_backend_available(s_scan_liot.config.backend)) {
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = scan_liot_calc_sizes(config, &frame_bytes, &required_capacity);
    if (ret != APP_OK) {
        return ret;
    }
    if (config->width > s_scan_liot_port.caps.max_width ||
        config->height > s_scan_liot_port.caps.max_height) {
        return APP_ERR_NOT_SUPPORTED;
    }

    s_scan_liot.frame_bytes = frame_bytes;
    s_scan_liot.required_capacity = required_capacity;
    s_scan_liot.frame_count = 0U;
    s_scan_liot.miss_count = 0U;
    s_scan_liot.decoder_ready = false;
    s_scan_liot.initialized = true;
    app_log("scan liot initialized: %ux%u input=%lu required_capacity=%lu (x2)",
            (unsigned int)config->width,
            (unsigned int)config->height,
            (unsigned long)frame_bytes,
            (unsigned long)required_capacity);
    return APP_OK;
}

static int scan_liot_deinit(void)
{
    liot_errcode_decoder_e decoder_ret;
    int ret = APP_OK;

    if (!s_scan_liot.initialized) {
        return APP_ERR_NOT_READY;
    }

    if (s_scan_liot.decoder_ready) {
        decoder_ret = liot_destroy_decoder();
        app_log("scan liot decoder destroy ret: %d", (int)decoder_ret);
        if (decoder_ret != LIOT_DECODER_SUCCESS) {
            app_log("scan liot decoder destroy failed: %d", (int)decoder_ret);
            ret = APP_ERR_FAIL;
        }
        s_scan_liot.decoder_ready = false;
    }
    s_scan_liot.frame_bytes = 0U;
    s_scan_liot.required_capacity = 0U;
    s_scan_liot.frame_count = 0U;
    s_scan_liot.miss_count = 0U;
    s_scan_liot.initialized = false;
    return ret;
}

static int scan_liot_get_frame_buffer_capacity(const app_scan_config_t *config,
                                               uint32_t *capacity)
{
    uint32_t frame_bytes;

    return scan_liot_calc_sizes(config, &frame_bytes, capacity);
}

static int scan_liot_process_frame(const app_scan_frame_t *frame,
                                   app_scan_result_t *result,
                                   bool *matched)
{
    liot_errcode_decoder_e decoder_ret;
    int result_len;

    if (!s_scan_liot.initialized || frame == NULL || result == NULL ||
        matched == NULL || frame->data == NULL ||
        frame->len < s_scan_liot.frame_bytes ||
        frame->buffer_capacity < s_scan_liot.required_capacity) {
        return APP_ERR_INVALID_ARG;
    }

    s_scan_liot.frame_count++;
    *matched = false;
    if (!s_scan_liot.decoder_ready) {
        app_log("scan liot decoder init begin: frame=%lu",
                (unsigned long)frame->frame_id);
        decoder_ret = liot_decoder_init(LIOT_DECODER_TYPE_QY);
        app_log("scan liot decoder init ret: frame=%lu ret=%d",
                (unsigned long)frame->frame_id,
                (int)decoder_ret);
        if (!scan_liot_decoder_init_ok(decoder_ret)) {
            app_log("scan liot decoder init failed: frame=%lu ret=%d",
                    (unsigned long)frame->frame_id,
                    (int)decoder_ret);
            return APP_ERR_FAIL;
        }
        s_scan_liot.decoder_ready = true;
        app_log("scan liot decoder ready: frame=%lu",
                (unsigned long)frame->frame_id);
    }

    decoder_ret = liot_image_decoder((unsigned char *)frame->data,
                                     (int)frame->width,
                                     (int)frame->height);
    if (decoder_ret != LIOT_DECODER_SUCCESS) {
        s_scan_liot.miss_count++;
        if (scan_liot_should_log(s_scan_liot.miss_count, 60U)) {
            app_log("scan liot miss: frame=%lu ret=%d input=%lu capacity=%lu required=%lu",
                    (unsigned long)frame->frame_id,
                    (int)decoder_ret,
                    (unsigned long)s_scan_liot.frame_bytes,
                    (unsigned long)frame->buffer_capacity,
                    (unsigned long)s_scan_liot.required_capacity);
        }
        return APP_OK;
    }

    memset(result, 0, sizeof(*result));
    result_len = liot_get_decoder_result((unsigned char *)result->text);
    if (result_len < 0) {
        app_log("scan liot get result failed: frame=%lu ret=%d",
                (unsigned long)frame->frame_id,
                result_len);
        return APP_ERR_FAIL;
    }

    result->text[sizeof(result->text) - 1U] = '\0';
    result->len = (uint32_t)result_len;
    *matched = result->text[0] != '\0';
    return APP_OK;
}

int app_scan_liot_setup(const app_scan_liot_config_t *config)
{
    if (config == NULL || !scan_liot_backend_available(config->backend)) {
        return APP_ERR_INVALID_ARG;
    }

    if (s_scan_liot.initialized) {
        return APP_ERR_BUSY;
    }

    memset(&s_scan_liot, 0, sizeof(s_scan_liot));
    s_scan_liot.config = *config;
    s_scan_liot.configured = true;
    app_log("scan liot setup: backend=%d", (int)s_scan_liot.config.backend);
    return APP_OK;
}

int app_scan_liot_register(void)
{
    int ret;

    if (!s_scan_liot.configured) {
        return APP_ERR_NOT_READY;
    }

    ret = app_scan_register_port(&s_scan_liot_port);
    if (ret != APP_OK) {
        app_log("scan liot register failed: %d", ret);
        return ret;
    }
    app_log("scan liot port registered");
    return APP_OK;
}

const app_scan_port_t *app_scan_liot_port(void)
{
    return &s_scan_liot_port;
}
