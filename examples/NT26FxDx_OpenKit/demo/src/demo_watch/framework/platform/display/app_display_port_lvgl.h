#ifndef APP_DISPLAY_PORT_LVGL_H
#define APP_DISPLAY_PORT_LVGL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_display_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_DISPLAY_LVGL_BACKEND_NONE = 0,
    APP_DISPLAY_LVGL_BACKEND_LIOT_LCD,
} app_display_lvgl_backend_t;

typedef enum {
    APP_DISPLAY_LVGL_LCD_INTERFACE_LSPI = 0x01,
} app_display_lvgl_lcd_interface_t;

typedef enum {
    APP_DISPLAY_LVGL_BACKLIGHT_NONE = 0,
    APP_DISPLAY_LVGL_BACKLIGHT_GPIO = 1,
    APP_DISPLAY_LVGL_BACKLIGHT_PWM = 2,
} app_display_lvgl_backlight_t;

typedef struct {
    uint8_t interface_type;
    int lspi_num;
    int lspi_cs;
    uint32_t lspi_speed_hz;
    bool lspi_3_line_spi;
    bool lspi_2_data_lane;
    bool lspi_sync;
    int backlight_type;
    int backlight_pin;
    int backlight_pwm_num;
    uint8_t brightness;
    int rst_pin;
    uint16_t rst_delay_ms;
    int ldo3v3_gpio;
    int ldo3v3_pin;
    int ldo3v3_func;
    uint16_t power_on_delay_ms;
} app_display_lvgl_lcd_config_t;

typedef struct {
    int i2c_num;
    uint8_t i2c_addr;
    int i2c_sda_pin;
    int i2c_scl_pin;
    int i2c_sda_func;
    int i2c_scl_func;
    int rst_pin;
    uint16_t rst_delay_ms;
    bool rst_active_low;
    int int_pin;
    bool fw_auto_update;
} app_display_lvgl_touch_config_t;

typedef struct {
    app_display_lvgl_backend_t backend;
    app_display_caps_t caps;
    app_display_lvgl_lcd_config_t lcd;
    app_display_lvgl_touch_config_t touch;
    uint32_t task_stack_bytes;
    uint8_t task_priority;
    uint8_t tick_ms;
    uint16_t handler_ms;
    uint8_t queue_depth;
    uint16_t draw_buf_rows;
    bool enable_touch;
} app_display_lvgl_config_t;

int app_display_lvgl_get_default_config(app_display_lvgl_config_t *config);
int app_display_lvgl_setup(const app_display_lvgl_config_t *config);
int app_display_lvgl_register(void);
const app_display_driver_t *app_display_lvgl_driver(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_PORT_LVGL_H */
