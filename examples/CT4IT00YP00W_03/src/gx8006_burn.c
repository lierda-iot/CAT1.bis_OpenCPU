#include <string.h>
#include "liot_uart2.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "app_nv.h"
#include "liot_log.h"

#define BURN_MSG_MAX_SIZE   1024
#define BURN_MSG_MAX_NUM    100

typedef struct {
    liot_uart_e port;
    uint32_t size;
    uint8_t *data;
} burn_msg_t;

static liot_queue_t g_burn_queue = NULL;

void uart_burn_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    if (g_burn_queue == NULL || size == 0)
        return;

    liot_trace("uart_burn_cb %d: datasize %d", port, size);
    
    burn_msg_t msg;
    msg.data = (uint8_t *)liot_rtos_malloc(size);
    if(msg.data == NULL)
        liot_trace("uart_burn_cb %d: datasize %d malloc failed", port, size);

    msg.port = port;
    msg.size = size;
    memcpy(msg.data, data, msg.size);
    liot_rtos_queue_release(g_burn_queue, sizeof(msg), (uint8 *)&msg, LIOT_NO_WAIT);
 }

void gx8006_into_burn(void)
{
    Liot_UartConfig_t usart_config = {0};
    usart_config.baudrate   = L_UART_BR_230400;
    usart_config.data_bit   = L_UART_DATA_8;
    usart_config.flow_ctrl  = L_UART_FC_NONE;
    usart_config.stop_bit   = L_UART_STOP_1;
    usart_config.parity_bit = L_UART_PARITY_NONE;
    usart_config.tx_way     = L_UART_TX_OPAQ;

    liot_rtos_queue_create(&g_burn_queue, sizeof(burn_msg_t), BURN_MSG_MAX_NUM);

    if(Liot_UartInit(L_USBCOM, &usart_config, uart_burn_cb, NULL) != L_UART_SUCCESS)
        liot_trace("USBCOM init failed!");

    if(Liot_UartInit(GX8006_UART_PORT, &usart_config, uart_burn_cb, NULL) != L_UART_SUCCESS)
        liot_trace("GX8006_UART_PORT init failed!");

    Liot_GpioInit(PIN_GX8006_BOOT, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(PIN_GX8006_RST,  L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit((liot_gpio_e)PIN_GX8006_POWER, L_IO_OUTPUT, L_IO_LOW, NULL);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel((liot_gpio_e)PIN_GX8006_POWER, L_IO_HIGH);

    liot_rtos_task_sleep_ms(500);

    Liot_GpioSetLevel(PIN_GX8006_BOOT, L_IO_LOW);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel(PIN_GX8006_RST, L_IO_LOW);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel(PIN_GX8006_RST, L_IO_HIGH);
    liot_rtos_task_sleep_ms(100);
    Liot_GpioSetLevel(PIN_GX8006_BOOT, L_IO_HIGH);
    liot_rtos_task_sleep_ms(1000);

    liot_trace("gx8006 into burn mode\n");

    burn_msg_t rx_msg;
    while(1)
    {
        if (liot_rtos_queue_wait(g_burn_queue, (uint8 *)&rx_msg, sizeof(rx_msg), LIOT_WAIT_FOREVER) == 0) {
            if (rx_msg.port == L_USBCOM) {
                Liot_UartSend(GX8006_UART_PORT, rx_msg.data, rx_msg.size);
            } else {
                Liot_UartSend(L_USBCOM, rx_msg.data, rx_msg.size);
            }
            liot_rtos_free(rx_msg.data);
        }
    }
}
