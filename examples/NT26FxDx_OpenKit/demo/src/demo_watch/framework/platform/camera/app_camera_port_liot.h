#ifndef APP_CAMERA_PORT_LIOT_H
#define APP_CAMERA_PORT_LIOT_H

#include <stdint.h>

#include "app_camera_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_CAMERA_LIOT_BACKEND_NONE = 0,
    APP_CAMERA_LIOT_BACKEND_GC032A,
} app_camera_liot_backend_t;

typedef enum {
    APP_CAMERA_LIOT_SENSOR_GC032A_2DDR = 0,
} app_camera_liot_sensor_t;

typedef struct {
    app_camera_liot_backend_t backend;
    app_camera_liot_sensor_t sensor;
    int power_gpio;
    uint16_t power_on_delay_ms;
    uint16_t width;
    uint16_t height;
    uint8_t bytes_per_pixel;
    app_camera_output_t output_format;
    int cspi_port;
    int cspi_speed;
    int i2c_num;
    int i2c_sda_pin;
    int i2c_scl_pin;
    uint32_t capture_timeout_ms;
    uint32_t capture_period_ms;
    uint32_t frame_buffer_capacity;
} app_camera_liot_config_t;

int app_camera_liot_get_default_config(app_camera_liot_config_t *config);
int app_camera_liot_setup(const app_camera_liot_config_t *config);
int app_camera_liot_register(void);
const app_camera_port_t *app_camera_liot_port(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAMERA_PORT_LIOT_H */
