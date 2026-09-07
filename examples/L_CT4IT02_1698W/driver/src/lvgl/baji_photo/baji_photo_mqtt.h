#ifndef BAJI_PHOTO_MQTT_H
#define BAJI_PHOTO_MQTT_H

#include <stdbool.h>

#include "liot_os.h"

#include "baji_photo_types.h"

int baji_photo_mqtt_start(void);
int baji_photo_mqtt_stop(void);
int baji_photo_mqtt_request_bind_token(void);
int baji_photo_mqtt_request_unbind(void);
int baji_photo_mqtt_set_event_cb(baji_photo_mqtt_event_cb_t cb, void *ctx);
int baji_photo_mqtt_notify_display_result(const baji_photo_mqtt_display_result_t *result);
bool baji_photo_mqtt_is_running(void);
bool baji_photo_mqtt_is_connected(void);
bool baji_photo_mqtt_has_active_task(void);
liot_task_t baji_photo_mqtt_get_task_ref(void);
baji_photo_mqtt_state_t baji_photo_mqtt_get_state(void);
baji_photo_bind_status_t baji_photo_mqtt_get_bind_status(void);
int baji_photo_mqtt_get_fatal_error(void);

#endif
