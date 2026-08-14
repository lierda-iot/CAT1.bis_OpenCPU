#ifndef APP_AUDIO_PORT_LIOT_H
#define APP_AUDIO_PORT_LIOT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_audio_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_AUDIO_LIOT_BACKEND_NONE = 0,
    APP_AUDIO_LIOT_BACKEND_GX8006,
    APP_AUDIO_LIOT_BACKEND_I2S_CODEC,
} app_audio_liot_backend_t;

typedef struct {
    app_audio_liot_backend_t backend;
    app_audio_format_t record_format;
    app_audio_format_t playback_format;
    uint32_t frame_samples;
    uint32_t max_encoded_bytes;
    bool enable_record;
    bool enable_playback;
    bool enable_opus_encode;
    bool enable_opus_decode;
    bool enable_vad;
    int power_gpio;
    int power_pin;
    int power_pin_func;
    uint16_t power_on_delay_ms;
    int reset_gpio;
    int reset_pin;
    int reset_pin_func;
    int boot_gpio;
    int boot_pin;
    int boot_pin_func;
    int pa_gpio;
    int pa_pin;
    int pa_pin_func;
    int uart_port;
    int uart_tx_pin;
    int uart_tx_func;
    int uart_rx_pin;
    int uart_rx_func;
    uint32_t uart_baudrate;
    uint8_t default_volume;
    uint8_t vad_timeout_s;
} app_audio_liot_config_t;

int app_audio_liot_get_default_config(app_audio_liot_config_t *config);
int app_audio_liot_setup(const app_audio_liot_config_t *config);
int app_audio_liot_register(void);
const app_audio_port_t *app_audio_liot_port(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AUDIO_PORT_LIOT_H */
