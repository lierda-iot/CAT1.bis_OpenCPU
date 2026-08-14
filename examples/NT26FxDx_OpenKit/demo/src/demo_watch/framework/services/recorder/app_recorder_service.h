#ifndef APP_RECORDER_SERVICE_H
#define APP_RECORDER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_RECORDER_FILE_NAME_MAX 64U
#define APP_RECORDER_FILE_PATH_MAX 128U

typedef struct {
    char name[APP_RECORDER_FILE_NAME_MAX];
    char path[APP_RECORDER_FILE_PATH_MAX];
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint32_t chunk_bytes;
} app_recorder_file_t;

typedef struct {
    const app_recorder_file_t *file;
    uint32_t bytes_done;
    uint32_t duration_ms;
    uint8_t level;
    bool recording;
    bool done;
    app_recorder_stop_reason_t stop_reason;
} app_recorder_progress_t;

typedef void (*app_recorder_progress_cb_t)(const app_recorder_progress_t *progress,
                                           void *user);

typedef struct {
    int (*init)(void);
    int (*deinit)(void);
    int (*file_exists)(const char *path, bool *exists);
    int (*record_file)(const app_recorder_file_t *file,
                       volatile bool *stop_requested,
                       app_recorder_progress_cb_t progress_cb,
                       void *progress_user);
    int (*stop)(void);
} app_recorder_port_ops_t;

typedef struct {
    const app_recorder_port_ops_t *ops;
} app_recorder_port_t;

int app_recorder_register_port(const app_recorder_port_t *port);
int app_recorder_init(void);
int app_recorder_deinit(void);
int app_recorder_file_exists(const char *path, bool *exists);
int app_recorder_record_file(const app_recorder_file_t *file,
                             volatile bool *stop_requested,
                             app_recorder_progress_cb_t progress_cb,
                             void *progress_user);
int app_recorder_stop(void);
bool app_recorder_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RECORDER_SERVICE_H */
