#ifndef WATCH_SCAN_H
#define WATCH_SCAN_H

#include "app_camera_service.h"
#include "app_error.h"

#ifdef __cplusplus
extern "C" {
#endif

int watch_scan_init(void);
int watch_scan_start(void);
int watch_scan_stop(void);
int watch_scan_shutdown(void);
int watch_scan_process_camera_frame(const app_camera_frame_t *camera_frame);
void watch_scan_flush_result(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_SCAN_H */
