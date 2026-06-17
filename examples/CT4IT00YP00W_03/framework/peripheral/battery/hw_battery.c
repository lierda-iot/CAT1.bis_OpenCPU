#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "hw_battery.h"
#include "battery.h"
#include "app_nv.h"

static void battery_event_handler(battery_event_e event, int voltage_mv, uint8_t percent)
{
    (void)voltage_mv;
    event_t evt = {0};

    switch (event) {
    case BATTERY_EVENT_LEVEL_CHANGE:
        if (percent <= 5) {
            evt.eventId = EVT_LOW_POWER;
            frameworkPostEvent(&evt);
        }
        break;
    case BATTERY_EVENT_CHG_INSERT:
        evt.eventId = EVT_CHARGE_INSERT;
        frameworkPostEvent(&evt);
        break;
    case BATTERY_EVENT_CHG_REMOVE:
        evt.eventId = EVT_CHARGE_REMOVE;
        frameworkPostEvent(&evt);
        break;
    default:
        break;
    }
}

void batteryModuleInit(void)
{
    battery_config_t cfg = {
        .adc_channel        = PIN_BATTERY_ADC_CH,
        .chg_state_gpio     = PIN_BATTERY_CHG_GPIO,
        .usb_wakeup_id      = PIN_BATTERY_USB_WAKEUP,
        .voltage_max        = 1200,
        .voltage_min        = 200,
        .sample_count       = 10,
        .sample_interval_ms = 60000,
        .event_cb           = battery_event_handler,
    };
    battery_init(&cfg);
}

void batteryModuleSleep(void)
{
}

void batteryModuleShutdown(void)
{
}
