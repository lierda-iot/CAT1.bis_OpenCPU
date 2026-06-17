#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "liot_type.h"
#include "liot_power.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "keyModule.h"

void liot_ai_power_key_callback(liot_pwrkey_statusmode_e status)
{
    event_t powerKeyEvent = {
        .eventId = EVT_KEY_POWER_OFF,
        .arg1 = 0,
        .arg2 = 0,
        .data = NULL,
        .ownerJobId = 0,
    };

    frameworkPostEvent(&powerKeyEvent);
}

void keyModuleInit(void)
{
    liot_pwrkey_callback_register(liot_ai_power_key_callback);
}

void keyModuleDeinit(void)
{
    /* 反注册电源键回调函数，恢复默认行为 */
    liot_pwrkey_callback_register(NULL);
}
