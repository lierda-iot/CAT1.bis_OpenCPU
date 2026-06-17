#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "hw_powerkey.h"
#include "liot_type.h"
#include "liot_power.h"

static void powerkey_callback(liot_pwrkey_statusmode_e status)
{
    (void)status;
    event_t evt = {0};
    evt.eventId = EVT_KEY_POWER_OFF;
    frameworkPostEvent(&evt);
}

void powerkeyInit(void)
{
    liot_pwrkey_callback_register(powerkey_callback);
}
