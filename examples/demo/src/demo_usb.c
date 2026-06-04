#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_usb.h"

int liot_usb_notify_cb(liot_usb_hotplug_e state, void *ctx)
{
    liot_trace("USB change hotplug state(%d)", state);
    return 0;
}

void liot_usb_demo_thread(void *arvg)
{
    bool usbisenabled = FALSE;

    liot_usb_bind_hotplug_cb(liot_usb_notify_cb);

    liot_rtos_task_sleep_ms(10000);

    liot_usb_drv_disable();
    liot_trace("usb driver deinit");

    usbisenabled = liot_usb_drv_is_enabled();
    liot_trace("usb driver enable status:%d", usbisenabled);
    liot_trace("USB get hotplug state(%d)", liot_usb_get_hotplug_state());
    
    liot_rtos_task_sleep_ms(10000);

    liot_usb_drv_enable();
    liot_trace("usb driver init");
    liot_rtos_task_sleep_ms(1000);

    liot_usb_bind_hotplug_cb(liot_usb_notify_cb);
    usbisenabled = liot_usb_drv_is_enabled();
    liot_trace("usb driver enable status:%d", usbisenabled);
    liot_trace("USB get hotplug state(%d)", liot_usb_get_hotplug_state());

    liot_rtos_task_delete(NULL); // kill itsel
}
