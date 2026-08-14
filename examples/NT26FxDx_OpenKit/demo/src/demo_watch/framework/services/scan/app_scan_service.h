#ifndef APP_SCAN_SERVICE_H
#define APP_SCAN_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SCAN_RESULT_TEXT_MAX (3U * 1024U)

typedef enum {
    APP_SCAN_FRAME_FORMAT_GRAY8 = 0,
} app_scan_frame_format_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    app_scan_frame_format_t input_format;
} app_scan_config_t;

typedef struct {
    bool has_decode;
    app_scan_frame_format_t input_format;
    uint16_t max_width;
    uint16_t max_height;
} app_scan_caps_t;

typedef struct {
    uint32_t frame_id;
    uint8_t *data;
    uint32_t len;
    uint32_t buffer_capacity;
    uint16_t width;
    uint16_t height;
    app_scan_frame_format_t format;
} app_scan_frame_t;

typedef struct {
    char text[APP_SCAN_RESULT_TEXT_MAX];
    uint32_t len;
} app_scan_result_t;

typedef enum {
    APP_SCAN_EVENT_NONE = 0,
    APP_SCAN_EVENT_RESULT,
    APP_SCAN_EVENT_ERROR,
} app_scan_event_id_t;

typedef struct {
    app_scan_event_id_t id;
    uint32_t frame_id;
    int result;
    const char *text;
    uint32_t text_len;
    const char *message;
} app_scan_event_t;

typedef struct {
    int (*init)(const app_scan_config_t *config);
    int (*deinit)(void);
    int (*get_frame_buffer_capacity)(const app_scan_config_t *config,
                                     uint32_t *capacity);
    int (*process_frame)(const app_scan_frame_t *frame,
                         app_scan_result_t *result,
                         bool *matched);
} app_scan_port_ops_t;

typedef struct {
    app_scan_caps_t caps;
    const app_scan_port_ops_t *ops;
} app_scan_port_t;

int app_scan_register_port(const app_scan_port_t *port);
int app_scan_init(const app_scan_config_t *config);
int app_scan_deinit(void);
int app_scan_get_frame_buffer_capacity(const app_scan_config_t *config,
                                       uint32_t *capacity);
int app_scan_process_frame(const app_scan_frame_t *frame, app_scan_event_t *event);
bool app_scan_is_initialized(void);
const app_scan_config_t *app_scan_get_config(void);
const app_scan_caps_t *app_scan_get_caps(void);
const char *app_scan_frame_format_name(app_scan_frame_format_t format);
const char *app_scan_event_name(app_scan_event_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* APP_SCAN_SERVICE_H */
