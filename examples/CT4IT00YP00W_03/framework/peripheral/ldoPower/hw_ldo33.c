#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_log.h"
#include "app_nv.h"
#include "hw_ldo33.h"

#define PWR_TRACE(fmt, ...) liot_trace("[PWR] " fmt, ##__VA_ARGS__)

int lod33_power_init(void)
{
    PWR_TRACE("power init start");

    Liot_AonPowerCtl(true);
    PWR_TRACE("AON domain enabled");
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    PWR_TRACE("Voltage set to 3.30V for all domains");

    Liot_GpioInit((liot_gpio_e)PIN_LDO33_EN, L_IO_OUTPUT, L_IO_HIGH, NULL);
    PWR_TRACE("LDO33_EN (GPIO%d) HIGH", PIN_LDO33_EN);
    liot_rtos_task_sleep_ms(50);

    Liot_GpioInit((liot_gpio_e)PIN_GX8006_POWER, L_IO_OUTPUT, L_IO_LOW, NULL);
    liot_rtos_task_sleep_ms(10);
    Liot_GpioSetLevel((liot_gpio_e)PIN_GX8006_POWER, L_IO_HIGH);
    PWR_TRACE("GX8006 Power up", PIN_GX8006_POWER);
    
    liot_rtos_task_sleep_ms(500);
    return 0;
}

void lod33_power_deinit(void)
{
    Liot_GpioSetLevel((liot_gpio_e)PIN_GX8006_RST, L_IO_LOW);
    liot_rtos_task_sleep_ms(10);
    Liot_GpioSetLevel((liot_gpio_e)PIN_GX8006_POWER, L_IO_LOW);
    //Liot_GpioSetLevel((liot_gpio_e)PIN_GX8006_PA, L_IO_LOW);
    Liot_GpioSetLevel((liot_gpio_e)PIN_LDO33_EN, L_IO_LOW);
    PWR_TRACE("power deinit done");
}
