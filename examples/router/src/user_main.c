/**
 * @File Name: user_main.c
 * @brief 路由器示例入口 - 启动 liotRouter
 * @Author : lierda email:ciot_iot_support@lierda.com
 * @Version : 1.1
 * @Creat Date : 2026-07-17
 *
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.
 *
 */

#include "liot_log.h"
#include "liot_os.h"
#include "liot_router_user.h"
#include "liot_gpio2.h"

void user_main(void)
{
    Liot_AonPowerCtl(true);
    liot_trace("AON domain enabled");
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    liot_trace("Voltage set to 3.30V for all domains");

    Liot_GpioInit((liot_gpio_e)PIN_LDO33_EN, L_IO_OUTPUT, L_IO_HIGH, NULL);
    liot_trace("LDO33_EN (GPIO%d) HIGH", PIN_LDO33_EN);
    liot_rtos_task_sleep_ms(50);

    liot_rtos_task_sleep_s(2);
    Liot_RouterStart();
}
