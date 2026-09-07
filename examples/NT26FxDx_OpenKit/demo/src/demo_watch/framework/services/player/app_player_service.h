#ifndef APP_PLAYER_SERVICE_H
#define APP_PLAYER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PLAYER_FILE_NAME_MAX 64U
#define APP_PLAYER_FILE_PATH_MAX 128U

typedef enum {
    APP_PLAYER_FILE_MP3 = 0,
    APP_PLAYER_FILE_WAV,
    APP_PLAYER_FILE_TTS,
} app_player_file_type_t;

typedef struct {
    char name[APP_PLAYER_FILE_NAME_MAX];
    char path[APP_PLAYER_FILE_PATH_MAX];
    uint32_t size_bytes;
    app_player_file_type_t type;
} app_player_file_t;

typedef struct {
    const app_player_file_t *file;
    uint32_t bytes_done;
    uint32_t total_bytes;
    uint8_t percent;
    bool playing;
    bool done;
} app_player_progress_t;

typedef void (*app_player_progress_cb_t)(const app_player_progress_t *progress,
                                         void *user);

typedef struct {
    int (*init)(void);
    int (*deinit)(void);
    int (*list_files)(const char *root,
                      app_player_file_t *files,
                      uint32_t capacity,
                      uint32_t *count);
    int (*play_file)(const app_player_file_t *file,
                     volatile bool *stop_requested,
                     app_player_progress_cb_t progress_cb,
                     void *progress_user);
    int (*stop)(void);
} app_player_port_ops_t;

typedef struct {
    const app_player_port_ops_t *ops;
} app_player_port_t;

int app_player_register_port(const app_player_port_t *port);
int app_player_init(void);
int app_player_deinit(void);
int app_player_list_files(const char *root,
                          app_player_file_t *files,
                          uint32_t capacity,
                          uint32_t *count);
int app_player_play_file(const app_player_file_t *file,
                         volatile bool *stop_requested,
                         app_player_progress_cb_t progress_cb,
                         void *progress_user);
int app_player_stop(void);
bool app_player_is_initialized(void);
const char *app_player_file_type_name(app_player_file_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_SERVICE_H */
