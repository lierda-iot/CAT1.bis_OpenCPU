/**
 * @file battery.h
 * @brief Battery management driver interface
 * @details Provides battery voltage sampling, level estimation (with hysteresis),
 *          and charging status detection. Internally uses periodic ADC sampling with
 *          averaging, and GPIO/USB wakeup for charge state detection.
 *          Singleton pattern, only one battery device instance globally.
 *
 * @note Usage example:
 * @code
 *   battery_config_t cfg = {
 *       .adc_channel        = 0,    // LIOT_ADC_VBAT_CHANNEL
 *       .chg_state_gpio     = 45,   // CHG_STATE pin
 *       .usb_wakeup_id      = 1,    // LIOT_WAKEUP_1
 *       .voltage_max        = 4200, // Full charge voltage mV
 *       .voltage_min        = 3000, // Cutoff voltage mV
 *       .sample_count       = 10,
 *       .sample_interval_ms = 30000,
 *   };
 *   battery_init(&cfg);
 *   int voltage = battery_get_voltage();
 *   uint8_t percent = battery_get_percent();
 *   battery_level_e level = battery_get_level();
 *   battery_deinit();
 * @endcode
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 */

#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Battery level enum (percentage-based with hysteresis)
 */
typedef enum {
    BATTERY_LEVEL_FULL = 0, ///< Full (>= 80%)
    BATTERY_LEVEL_HIGH,     ///< High (>= 60%)
    BATTERY_LEVEL_MID,      ///< Medium (>= 40%)
    BATTERY_LEVEL_LOW,      ///< Low (>= 20%)
    BATTERY_LEVEL_EMPTY,    ///< Empty (< 20%)
} battery_level_e;

/**
 * @brief Charging status enum
 */
typedef enum {
    BATTERY_CHG_NONE = 0,   ///< No charger connected
    BATTERY_CHG_CHARGING,   ///< Charging
    BATTERY_CHG_DONE,       ///< Charge complete
} battery_chg_status_e;

/**
 * @brief Event type enum
 */
typedef enum {
    BATTERY_EVENT_LEVEL_CHANGE = 0, ///< Battery level changed
    BATTERY_EVENT_CHG_INSERT,       ///< Charger inserted
    BATTERY_EVENT_CHG_REMOVE,       ///< Charger removed
    BATTERY_EVENT_CHG_DONE,         ///< Charge complete
} battery_event_e;

/**
 * @brief Status change callback function type
 * @param[in] event      Event type that triggered the callback
 * @param[in] voltage_mv Current battery voltage (mV)
 * @param[in] percent    Current battery percentage (0~100)
 */
typedef void (*battery_event_cb_t)(battery_event_e event, int voltage_mv, uint8_t percent);

/**
 * @brief Battery driver initialization config
 */
typedef struct {
    uint8_t  adc_channel;       ///< ADC channel number
    uint8_t  chg_state_gpio;    ///< Charge state detection GPIO
    uint8_t  usb_wakeup_id;     ///< USB wakeup source ID
    uint8_t  sample_count;      ///< Sample averaging count (recommend 10, max 20)
    uint16_t voltage_max;       ///< Full charge voltage (mV), e.g. 4200
    uint16_t voltage_min;       ///< Cutoff voltage (mV), e.g. 3000
    uint32_t sample_interval_ms; ///< Sample interval (ms)
    battery_event_cb_t event_cb; ///< Status change callback (may be NULL)
} battery_config_t;

/* ======================== Init & Deinit ======================== */

/**
 * @brief Initialize battery management driver
 * @param[in] config Config pointer, must not be NULL
 * @return 0 on success, -1 on failure
 */
int battery_init(const battery_config_t *config);

/**
 * @brief Release battery management driver resources
 */
void battery_deinit(void);

/* ======================== Status query ======================== */

/**
 * @brief Get current battery voltage
 * @return Voltage in mV, -1 if not initialized
 */
int battery_get_voltage(void);

/**
 * @brief Get current battery percentage
 * @return 0~100, 0 if not initialized
 */
uint8_t battery_get_percent(void);

/**
 * @brief Get current battery level
 * @return Battery level enum value
 */
battery_level_e battery_get_level(void);

/**
 * @brief Get current charging status
 * @return Charging status enum value
 */
battery_chg_status_e battery_get_chg_status(void);


#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_H__ */
