/*================================================================
  Copyright (c) 2021, Magic Wireless Solutions Co., Ltd. All rights reserved.
  Magic Wireless Solutions Proprietary and Confidential.
=================================================================*/

#include "liot_uart2.h"
#include "liot_os.h"
#include "lierda_app_main.h"

#define LIOT_UART_PORT_USB_TEST_DEMO
//#define LIOT_UART_PORT_0_TEST_DEMO
#define LIOT_UART_PORT_1_TEST_DEMO
//#define LIOT_UART_PORT_2_TEST_DEMO
//#define LIOT_UART_PORT_3_TEST_DEMO

void liot_uart2_notify_cb(liot_uart_e port, char *data, uint32_t size, void *argc)
{
    liot_trace("UART port %d receive size:%d, data=%s", port, size, data);
}

void liot_uart2_demo_thread(void *arvg)
{
    int ret                         = 0;
    Liot_UartConfig_t usart_config = {0};
    usart_config.baudrate   = L_UART_BR_115200;
    usart_config.data_bit   = L_UART_DATA_8;
    usart_config.flow_ctrl  = L_UART_FC_NONE;
    usart_config.stop_bit   = L_UART_STOP_1;
    usart_config.parity_bit = L_UART_PARITY_NONE;

    liot_rtos_task_sleep_ms(10000);
    liot_trace("==========Uart2 Demo Init: Baudrate-%d ==========\r\n", usart_config.baudrate);

#ifdef LIOT_UART_PORT_USB_TEST_DEMO
    ret = Liot_UartInit(L_USBCOM, &usart_config, liot_uart2_notify_cb, NULL);
    if(ret != L_UART_SUCCESS)
        liot_trace("Liot_UartInit failed, ret=%d", ret);
#endif
#ifdef LIOT_UART_PORT_0_TEST_DEMO
    ret = Liot_UartInit(L_UART0, &usart_config, liot_uart2_notify_cb, NULL);
    if(ret != L_UART_SUCCESS)
        liot_trace("Liot_UartInit failed, ret=%d", ret);
#endif
#ifdef LIOT_UART_PORT_1_TEST_DEMO
    ret = Liot_UartInit(L_UART1, &usart_config, liot_uart2_notify_cb, NULL);
    if(ret != L_UART_SUCCESS)
        liot_trace("Liot_UartInit failed, ret=%d", ret);
#endif
#ifdef LIOT_UART_PORT_2_TEST_DEMO
    ret = Liot_UartInit(L_UART2, &usart_config, liot_uart2_notify_cb, NULL);
    if(ret != L_UART_SUCCESS)
        liot_trace("Liot_UartInit failed, ret=%d", ret);
#endif
#ifdef LIOT_UART_PORT_3_TEST_DEMO
    ret = Liot_UartInit(L_UART3, &usart_config, liot_uart2_notify_cb, NULL);
    if(ret != L_UART_SUCCESS)
        liot_trace("Liot_UartInit failed, ret=%d", ret);
#endif
    while (1)
    {
        liot_trace("demo_main EPAT Log print\r\n");
#ifdef LIOT_UART_PORT_USB_TEST_DEMO
        Liot_UartSend(L_USBCOM, (unsigned char *)"helloworld\r\n", 10);
        printf("%s\r\n", "USBPORT PRINTF TEST");
#endif
#ifdef LIOT_UART_PORT_0_TEST_DEMO
        Liot_UartSend(L_UART0, (unsigned char *)"helloworld\n\n", 10);
        printf("%s\r\n", "UART0 PRINTF TEST");
#endif
#ifdef LIOT_UART_PORT_1_TEST_DEMO
        Liot_UartSend(L_UART1, (unsigned char *)"helloworld\n\n", 10);
        printf("%s\r\n", "UART1 PRINTF TEST");
#endif
#ifdef LIOT_UART_PORT_2_TEST_DEMO
        Liot_UartSend(L_UART2, (unsigned char *)"helloworld\n\n", 10);
        printf("%s\r\n", "UART2 PRINTF TEST");
#endif
#ifdef LIOT_UART_PORT_3_TEST_DEMO
        Liot_UartSend(L_UART3, (unsigned char *)"helloworld\n\n", 10);
        printf("%s\r\n", "UART3 PRINTF TEST");
#endif
        liot_rtos_task_sleep_ms(1000);
    }

#ifdef LIOT_UART_PORT_USB_TEST_DEMO
    Liot_UartDeinit(L_USBCOM);
#endif
#ifdef LIOT_UART_PORT_0_TEST_DEMO
    Liot_UartDeinit(L_UART0);
#endif
#ifdef LIOT_UART_PORT_1_TEST_DEMO
    Liot_UartDeinit(L_UART1);
#endif
#ifdef LIOT_UART_PORT_2_TEST_DEMO
    Liot_UartDeinit(L_UART2);
#endif
#ifdef LIOT_UART_PORT_3_TEST_DEMO
    Liot_UartDeinit(L_UART3);
#endif
    liot_rtos_task_delete(NULL); // kill itsel

}
