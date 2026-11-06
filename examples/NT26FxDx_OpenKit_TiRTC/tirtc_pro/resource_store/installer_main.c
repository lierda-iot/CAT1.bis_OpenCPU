/* Standalone resource installer entry point. Never link into the normal UI. */
#include "asset_installer.h"
#include "../storage/tirtc_storage.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"

void user_main(void)
{
    int result = -1;
    liot_trace("[assets14] installer starting; matching complete Pro UI resource set\r\n");
    /* This firmware has no display/storage worker, so it owns initial power
     * and the single storage initialization. Never pulse the shared rail. */
    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS ||
        Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS ||
        Liot_SetPinFunc(16, L_PIN_FUNC_0) != L_GPIO_ERR_SUCCESS ||
        Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS) goto finished;
    liot_rtos_task_sleep_ms(500U);
    result = tirtc_storage_init();
    if (!result) result = tirtc_assets_install();
finished:
    if (result) liot_trace("[assets14] INSTALL_FAILED error=%d; no automatic retry\r\n", result);
    /* Keep the result visible until the normal application is downloaded. */
    for (;;) liot_rtos_task_sleep_ms(5000U);
}
