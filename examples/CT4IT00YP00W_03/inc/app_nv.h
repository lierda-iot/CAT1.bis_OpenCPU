#ifndef APP_NV_H
#define APP_NV_H

#include <stdint.h>
#include "hw_led.h"
#include "hw_network.h"

/* ---- hardware feature switches ---- */

#define WS2812B_RGB_LED_ENABLE          1
#define AUDIO_CODEC_GX8006              1

/* ---- audio ---- */

#define AUDIO_SAMPLERATE                16000
#define AUDIO_SAMPLERATE_CFG            SAMPLERATE_16K

/* ---- version ---- */

#define AI_APP_MAJOR    "02"
#define AI_APP_MINOR    "00"
#define AI_APP_PATCH    "01"
#define AI_HARDWARE_VERSION "L-CT4IT00-YP00W-03A Y02.01"

/* ---- defaults ---- */

#define APP_NV_MAGIC    0xA5A5A5A5

#define SYSTEM_WAIT_TIME_DEFAULT_MS     60000
#define SYSTEM_SLEEP_TIME_DEFAULT_MS    60000
#define AUDIO_VOLUME_DEFAULT            80
#define APP_LISTEN_TIMEOUT_MS           10000

/* ---- pin map ---- */
#define GX8006_UART_PORT        1
#define GX8006_VAD_TIMEOUT      25

#define PIN_LDO33_EN            L_GPIO_25
#define PIN_GX8006_POWER        L_GPIO_26

#define PIN_GX8006_RST          L_GPIO_23
#define PIN_GX8006_BOOT         L_GPIO_4
#define PIN_GX8006_PA_SD        L_GPIO_17

#define PIN_GX8006_INT_WAKEUP   L_GPIO_0

#define PIN_WS2812B_SPI_PORT    0

#define PIN_BATTERY_ADC_CH      1
#define PIN_BATTERY_CHG_GPIO    14
#define PIN_BATTERY_USB_WAKEUP  1

#define PIN_KEY_POWER           0

// creat AI bot with https://www.coze.cn/ and get the token, botid, voiceid
#define COZE_DEFAULT_TOKEN  "sat_66VJ40uptrgXQsUnPYR4RLuh1IJ1VI599zTruL5cf4xlWxZjElfKlJVn1dbmhHFM"
#define COZE_DEFAULT_BOTID  "7543145424308420627"
#define COZE_DEFAULT_VOICEID "7468512265151463451"

#define CHAT_MODE_DEFAULT       (0U)

/* ---- LBS auth ---- */
// Please contact the staff to get it
#define LBS_DEFAULT_USER_NAME   "lierda"
#define LBS_DEFAULT_USER_PWD    "123456"
#define LBS_DEFAULT_TOKEN       "B84E427D95B1DF4A3F49B18DDF714C72"

typedef enum boot_mode_e {
    APP_BOOT_MODE_NORMAL = 0,
    APP_BOOT_MODE_FACTORY = 1,
    APP_BOOT_MODE_BURN = 2,
} boot_mode_e;
/* ---- NV data ---- */

typedef struct {
    char     coze_token[96];
    char     coze_botid[32];
    char     coze_voiceid[32];
    uint32_t chat_mode;
    uint32_t audio_volume;
    uint32_t sleep_wait_time_ms;
    uint32_t sleep_time_ms;
    led_config_t led_cfg;
    network_config_t net_cfg;
    uint32_t magic;
    boot_mode_e boot_mode;
} app_nv_data_t;

app_nv_data_t *app_nv_get(void);
boot_mode_e app_nv_get_boot_mode(void);
void app_nv_set_boot_mode(boot_mode_e mode);
void app_nv_load(void);
void app_nv_save(void);

#endif /* APP_NV_H */
