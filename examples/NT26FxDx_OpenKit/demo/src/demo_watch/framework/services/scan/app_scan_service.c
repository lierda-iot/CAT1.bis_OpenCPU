#include "app_scan_service.h"

#include <stddef.h>
#include <string.h>

#include "app_osal.h"

static const app_scan_port_t *s_port;
static app_scan_config_t s_config;
static bool s_initialized;
static app_scan_result_t s_frame_result;
static char s_last_result[APP_SCAN_RESULT_TEXT_MAX];

static const app_scan_port_ops_t *scan_ops(void)
{
    return (s_port != NULL) ? s_port->ops : NULL;
}

static int scan_require_ready(const app_scan_port_ops_t **ops)
{
    if (!s_initialized) {
        return APP_ERR_NOT_READY;
    }
    *ops = scan_ops();
    if (*ops == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return APP_OK;
}

static uint32_t scan_expected_frame_bytes(const app_scan_config_t *config)
{
    if (config == NULL) {
        return 0U;
    }
    return (uint32_t)config->width * (uint32_t)config->height;
}

static void scan_set_event(app_scan_event_t *event,
                           app_scan_event_id_t id,
                           uint32_t frame_id,
                           int result,
                           const char *text,
                           uint32_t text_len,
                           const char *message)
{
    if (event == NULL) {
        return;
    }

    memset(event, 0, sizeof(*event));
    event->id = id;
    event->frame_id = frame_id;
    event->result = result;
    event->text = text;
    event->text_len = text_len;
    event->message = message;
}

int app_scan_register_port(const app_scan_port_t *port)
{
    if (port == NULL || port->ops == NULL || port->ops->init == NULL ||
        port->ops->deinit == NULL ||
        port->ops->get_frame_buffer_capacity == NULL ||
        port->ops->process_frame == NULL) {
        app_log("scan port register rejected");
        return APP_ERR_INVALID_ARG;
    }

    s_port = port;
    app_log("scan port registered");
    return APP_OK;
}

int app_scan_init(const app_scan_config_t *config)
{
    const app_scan_port_ops_t *ops = scan_ops();
    app_scan_config_t effective_config;
    int ret;

    if (config == NULL || config->width == 0U || config->height == 0U) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        return APP_ERR_BUSY;
    }
    if (config->input_format != APP_SCAN_FRAME_FORMAT_GRAY8) {
        return APP_ERR_NOT_SUPPORTED;
    }
    if (ops == NULL || ops->init == NULL) {
        app_log("scan init rejected: port unavailable");
        return APP_ERR_NOT_SUPPORTED;
    }

    effective_config = *config;
    s_initialized = false;
    s_last_result[0] = '\0';
    ret = ops->init(&effective_config);
    if (ret != APP_OK) {
        app_log("scan port init failed: %d", ret);
        return ret;
    }

    s_config = effective_config;
    s_initialized = true;
    app_log("scan initialized: %ux%u %s",
            (unsigned int)s_config.width,
            (unsigned int)s_config.height,
            app_scan_frame_format_name(s_config.input_format));
    return APP_OK;
}

int app_scan_deinit(void)
{
    const app_scan_port_ops_t *ops;
    int ret = scan_require_ready(&ops);

    if (ret != APP_OK) {
        return ret;
    }

    ret = ops->deinit();
    s_initialized = false;
    s_last_result[0] = '\0';
    if (ret == APP_OK) {
        app_log("scan deinitialized");
    } else {
        app_log("scan deinit completed with decoder error: %d", ret);
    }
    return ret;
}

int app_scan_get_frame_buffer_capacity(const app_scan_config_t *config,
                                       uint32_t *capacity)
{
    const app_scan_port_ops_t *ops = scan_ops();

    if (config == NULL || capacity == NULL || config->width == 0U ||
        config->height == 0U || config->input_format != APP_SCAN_FRAME_FORMAT_GRAY8) {
        return APP_ERR_INVALID_ARG;
    }
    if (ops == NULL || ops->get_frame_buffer_capacity == NULL) {
        return APP_ERR_NOT_SUPPORTED;
    }
    return ops->get_frame_buffer_capacity(config, capacity);
}

int app_scan_process_frame(const app_scan_frame_t *frame, app_scan_event_t *event)
{
    const app_scan_port_ops_t *ops;
    app_scan_result_t *result = &s_frame_result;
    uint32_t expected_len;
    bool matched = false;
    int ret;

    if (event != NULL) {
        memset(event, 0, sizeof(*event));
        event->id = APP_SCAN_EVENT_NONE;
    }
    if (frame == NULL || frame->data == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    ret = scan_require_ready(&ops);
    if (ret != APP_OK) {
        return ret;
    }

    expected_len = scan_expected_frame_bytes(&s_config);
    if (frame->format != s_config.input_format ||
        frame->width != s_config.width ||
        frame->height != s_config.height ||
        frame->len < expected_len ||
        frame->buffer_capacity < frame->len) {
        app_log("scan frame rejected: id=%lu %ux%u len=%lu cap=%lu %s expected=%ux%u/%lu %s",
                (unsigned long)frame->frame_id,
                (unsigned int)frame->width,
                (unsigned int)frame->height,
                (unsigned long)frame->len,
                (unsigned long)frame->buffer_capacity,
                app_scan_frame_format_name(frame->format),
                (unsigned int)s_config.width,
                (unsigned int)s_config.height,
                (unsigned long)expected_len,
                app_scan_frame_format_name(s_config.input_format));
        return APP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    ret = ops->process_frame(frame, result, &matched);
    if (ret != APP_OK) {
        app_log("scan process failed: frame=%lu ret=%d",
                (unsigned long)frame->frame_id,
                ret);
        scan_set_event(event,
                       APP_SCAN_EVENT_ERROR,
                       frame->frame_id,
                       ret,
                       NULL,
                       0U,
                       "scan decoder failed");
        return ret;
    }
    if (!matched) {
        return APP_OK;
    }

    result->text[sizeof(result->text) - 1U] = '\0';
    if (result->len >= sizeof(result->text)) {
        result->len = (uint32_t)strlen(result->text);
    }
    if (result->len == 0U) {
        result->len = (uint32_t)strlen(result->text);
    }
    if (result->len == 0U || result->text[0] == '\0') {
        return APP_OK;
    }
    if (strcmp(s_last_result, result->text) == 0) {
        return APP_OK;
    }

    strncpy(s_last_result, result->text, sizeof(s_last_result) - 1U);
    s_last_result[sizeof(s_last_result) - 1U] = '\0';
    app_log("scan result: frame=%lu len=%lu text=%s",
            (unsigned long)frame->frame_id,
            (unsigned long)strlen(s_last_result),
            s_last_result);
    scan_set_event(event,
                   APP_SCAN_EVENT_RESULT,
                   frame->frame_id,
                   APP_OK,
                   s_last_result,
                   (uint32_t)strlen(s_last_result),
                   NULL);
    return APP_OK;
}

bool app_scan_is_initialized(void)
{
    return s_initialized;
}

const app_scan_config_t *app_scan_get_config(void)
{
    return s_initialized ? &s_config : NULL;
}

const app_scan_caps_t *app_scan_get_caps(void)
{
    return (s_port != NULL) ? &s_port->caps : NULL;
}

const char *app_scan_frame_format_name(app_scan_frame_format_t format)
{
    switch (format) {
    case APP_SCAN_FRAME_FORMAT_GRAY8:
        return "gray8";
    default:
        return "unknown";
    }
}

const char *app_scan_event_name(app_scan_event_id_t id)
{
    switch (id) {
    case APP_SCAN_EVENT_RESULT:
        return "result";
    case APP_SCAN_EVENT_ERROR:
        return "error";
    case APP_SCAN_EVENT_NONE:
    default:
        return "none";
    }
}
