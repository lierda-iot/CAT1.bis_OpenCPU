/**
 * @file       demo_at_download.c
 * @brief      Enter USB download mode through the USB OPAQ port
 * @version    1.0
 * @date       2026-10-10
 * @copyright  Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 */

/**
 * Send "AT+ECRST=delay,599\r\n" on the USB OPAQ virtual COM port.
 * The demo replies "OK", waits 599 ms, then resets into download mode.
 * The USB OPAQ port must not be used by another AT command handler.
 */

#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_uart2.h"
#include "liot_usb.h"

#define DOWNLOAD_COMMAND       "AT+ECRST=delay,599\r\n"
#define DOWNLOAD_REPLY         "\r\nOK\r\n"
#define DOWNLOAD_QUEUE_SIZE    4
#define DOWNLOAD_RESET_DELAY_MS 599

extern void AonPrepareForUSBDldBoot(void);
extern void ResetSwitchApbMp26M(void);
extern void ResetECSystemReset(void);

typedef enum {
    DOWNLOAD_USB_OUT = 0,
    DOWNLOAD_USB_IN,
    DOWNLOAD_REQUEST
} download_event_e;

static liot_queue_t g_download_queue = NULL;
static volatile bool g_download_port_open = false;
static volatile bool g_download_pending = false;

/**
 * @brief Send a USB or download event to the demo task without blocking.
 * @param event Event to send.
 * @return true if queued, otherwise false.
 */
static bool download_post_event(download_event_e event)
{
    uint32_t value = (uint32_t)event;

    if (g_download_queue == NULL)
        return false;

    if (liot_rtos_queue_release(g_download_queue, sizeof(value), (uint8_t *)&value,
                                LIOT_NO_WAIT) != LIOT_SUCCESS) {
        liot_trace("download demo: event queue full (%d)", event);
        return false;
    }
    return true;
}

/**
 * @brief Detect the download command in received UART data.
 * @param port UART port that received the data.
 * @param data Received UART data.
 * @param size Number of received bytes.
 * @param argc Unused callback context.
 * @note Data that does not match the command is echoed to the USB OPAQ port.
 */
static void download_uart_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    (void)argc;
    if (port != L_USBCOM || data == NULL || size == 0 ||
        !g_download_port_open || g_download_pending)
        return;

    if ((size >= (sizeof(DOWNLOAD_COMMAND) - 1U)) &&
        (strncmp(data, DOWNLOAD_COMMAND, sizeof(DOWNLOAD_COMMAND) - 1U) == 0)) {
        g_download_pending = true;
        if (!download_post_event(DOWNLOAD_REQUEST))
            g_download_pending = false;
    } else {
        Liot_UartSend(L_USBCOM, (unsigned char *)data, size);
    }
}

/**
 * @brief Forward USB connection changes to the demo task.
 * @param state Current USB hotplug state.
 * @param ctx Unused callback context.
 * @return 0 after handling the notification.
 */
static int download_usb_cb(liot_usb_hotplug_e state, void *ctx)
{
    (void)ctx;
    download_post_event(state == LIOT_USB_HOTPLUG_IN ? DOWNLOAD_USB_IN : DOWNLOAD_USB_OUT);
    return 0;
}

/**
 * @brief Open the USB OPAQ port when USB is connected.
 */
static void download_port_init(void)
{
    Liot_UartConfig_t usart_config = {0};
    if (g_download_port_open)
        return;

    usart_config.baudrate = L_UART_BR_115200;
    usart_config.data_bit = L_UART_DATA_8;
    usart_config.stop_bit = L_UART_STOP_1;
    usart_config.parity_bit = L_UART_PARITY_NONE;
    usart_config.flow_ctrl = L_UART_FC_NONE;
    usart_config.tx_way = L_UART_TX_OPAQ;

    liot_uart_err_e ret = Liot_UartInit(L_USBCOM, &usart_config, download_uart_cb, NULL);
    if (ret != L_UART_SUCCESS) {
        liot_trace("download demo: USB OPAQ init failed (%d)", ret);
        return;
    }

    g_download_pending = false;
    g_download_port_open = true;
    liot_trace("download demo: USB OPAQ ready");
}

/**
 * @brief Close the USB OPAQ port when USB is disconnected.
 */
static void download_port_deinit(void)
{
    if (!g_download_port_open)
        return;

    g_download_port_open = false;
    Liot_UartDeinit(L_USBCOM);
    g_download_pending = false;
}

/**
 * @brief Handle USB hotplug and download requests outside the callbacks.
 * @param arg Unused task argument.
 * @note The task waits on a queue until the module resets.
 */
void liot_at_download_demo_thread(void *arg)
{
    uint32_t usb_event = 0;

    (void)arg;
    if (liot_rtos_queue_create(&g_download_queue, sizeof(usb_event), DOWNLOAD_QUEUE_SIZE) != LIOT_SUCCESS) {
        liot_trace("download demo: queue creation failed");
        liot_rtos_task_delete(NULL);
        return;
    }

    if (liot_usb_bind_hotplug_cb(download_usb_cb) != LIOT_USB_SUCCESS) {
        liot_trace("download demo: USB hotplug registration failed");
        liot_rtos_queue_delete(g_download_queue);
        g_download_queue = NULL;
        liot_rtos_task_delete(NULL);
        return;
    }

    if (liot_usb_get_hotplug_state() == LIOT_USB_HOTPLUG_IN)
        download_port_init();

    while (1) {
        if (liot_rtos_queue_wait(g_download_queue, (uint8_t *)&usb_event, sizeof(usb_event),
                                 LIOT_WAIT_FOREVER) != LIOT_SUCCESS)
            continue;

        switch ((download_event_e)usb_event) {
        case DOWNLOAD_USB_IN:
            download_port_init();
            break;
        case DOWNLOAD_USB_OUT:
            download_port_deinit();
            break;
        case DOWNLOAD_REQUEST:
            if (!g_download_port_open || !g_download_pending)
                break;
            liot_trace("download demo: entering USB download mode");
            AonPrepareForUSBDldBoot();
            Liot_UartSend(L_USBCOM, (unsigned char *)DOWNLOAD_REPLY,
                          sizeof(DOWNLOAD_REPLY) - 1);
            liot_rtos_task_sleep_ms(DOWNLOAD_RESET_DELAY_MS);
            ResetSwitchApbMp26M();
            ResetECSystemReset();
            g_download_pending = false;
            break;
        default:
            break;
        }
    }
}
