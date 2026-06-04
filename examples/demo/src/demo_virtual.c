/**
 * @file liot_virtual_demo.c
 * @brief 
 * @author zlc (zhaolc@lierda.com)
 * @version 1.1
 * @date 2024-07-17
 * 
 * @Lierda Science & Technology Group Co., Ltd.
 * 
 * @par Modification Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2024-07-17 <td>1.0     <td>zhaoliechang     <td>new create
 * <tr><td>2025-08-21 <td>1.1     <td>zhaoliechang     <td>Optimize the demo format and change the comments to English.
 * </table>
 */
/**
 * The entire Demo runs in an independent thread (liot_virtual_demo_thread) and 
 *   executes the following operations in an infinite loop:
 *
 * 1.Register the URC callback function.
 * 2.Set AT+CEREG=1 to enable active reporting of network status changes.
 * 3.Turn off the RF function (AT+CFUN=0) and wait for 2 seconds to trigger a network disconnection.
 * 4.Turn on the RF function (AT+CFUN=1) and wait for 2 seconds to trigger a network reconnection.
 * 5.Query the device IMEI number (AT+CGSN=1).
 * 6.Query the module version information (AT+CGMR).
 * 7.Query the configuration file content (AT+ECPCFG?).
 * 8.Unregister the URC callback function.
 * 9.Print the current heap memory usage.
 * 10.Put the thread to sleep for 5 seconds before continuing to the next loop iteration.
*/
#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_virtual_at.h"

#define AT_CEREGSET                 "AT+CEREG=1\r\n"
#define AT_IMEIREAD                 "AT+CGSN=1\r\n"
#define AT_CGPADDRREAD              "AT+CGPADDR\r\n"
#define AT_CGMR_READ_TEST           "AT+CGMR\r\n"    
#define AT_ECPCFG_READ_TEST         "AT+ECPCFG?\r\n"

#define AT_ECPCFG_READ_BUFF_LEN     (1024)

#define AT_CFUN_0_SET                "AT+CFUN=0\r\n"
#define AT_CFUN_1_SET                "AT+CFUN=1\r\n"

static uint8_t g_NetConSta = 0;

/**
 * @brief Turn off RF function (AT+CFUN=0)
 * @details Sends the AT+CFUN=0 command to disable the device's RF functionality, triggering a network disconnection.
 *          Uses the `liot_VirtualAtCmd` function to send the command and receive the response.
 */
static void liot_At_Cmd_CFUN0(void)
{
    uint8_t AtRespBuff[128] = {0};
    liot_trace("SNED:%s\r\n", AT_CFUN_0_SET);
    if (liot_VirtualAtCmd(AT_CFUN_0_SET, strlen(AT_CFUN_0_SET), 3000, AtRespBuff) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespBuff);
    }
    else
    {
        liot_trace("%s fail\r\n", __FUNCTION__);
    }
}

/**
 * @brief Turn on RF function (AT+CFUN=1)
 * @details Sends the AT+CFUN=1 command to enable the device's RF functionality, triggering a network reconnection.
 *          Uses the `liot_VirtualAtCmd` function to send the command and receive the response.
 */
static void liot_At_Cmd_CFUN1(void)
{
    uint8_t AtRespBuff[128] = {0};
    liot_trace("SNED:%s\r\n", AT_CFUN_1_SET);
    if (liot_VirtualAtCmd(AT_CFUN_1_SET, strlen(AT_CFUN_1_SET), 3000, AtRespBuff) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespBuff);
    }
    else
    {
        liot_trace("%s fail\r\n", __FUNCTION__);
    }
}

/**
 * @brief Query the device IMEI number (AT+CGSN=1)
 * @details Sends the AT+CGSN=1 command to query the device's IMEI number and prints the returned result.
 */
static void liot_At_Cmd_IMEIREAD(void)
{
    uint8_t AtRespBuff[128] = {0};
    liot_trace("SNED:%s\r\n", AT_IMEIREAD);
    if (liot_VirtualAtCmd(AT_IMEIREAD, strlen(AT_IMEIREAD), 3000, AtRespBuff) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespBuff);
    }
    else
    {
        liot_trace("%s fail\r\n", __FUNCTION__);
    }
}

/**
 * @brief Query module version information (AT+CGMR)
 * @details Sends the AT+CGMR command to query the module's firmware version information and prints the returned result.
 */
static void liot_At_Cmd_CGMRREAD(void)
{
    uint8_t AtRespBuff[128] = {0};
    liot_trace("SNED:%s\r\n", AT_CGMR_READ_TEST);
    if (liot_VirtualAtCmd(AT_CGMR_READ_TEST, strlen(AT_CGMR_READ_TEST), 3000, AtRespBuff) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespBuff);
    }
    else
    {
        liot_trace("%s fail\r\n", __FUNCTION__);
    }
}

/**
 * @brief Query configuration file content (AT+ECPCFG?)
 * @details Sends the AT+ECPCFG? command to query the module's configuration file content.
 *          Dynamically allocates a buffer to handle large response data and releases the memory after completion.
 */
static void liot_At_Cmd_ECPCFG(void)
{
    uint8_t *AtRespPtr = (uint8_t *)liot_rtos_malloc(AT_ECPCFG_READ_BUFF_LEN);
    if(!AtRespPtr)
    {
        liot_trace("liot_At_Cmd_ECPCFG malloc fail\r\n");
        return;
    }

    memset(AtRespPtr, 0, AT_ECPCFG_READ_BUFF_LEN);

    liot_trace("SNED:%s\r\n", AT_ECPCFG_READ_TEST);
    if (liot_VirtualAtCmd(AT_ECPCFG_READ_TEST, strlen(AT_ECPCFG_READ_TEST), 3000, AtRespPtr) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespPtr);
    }
    else
    {
        liot_trace("%s fail\r\n", __FUNCTION__);
    }

    liot_rtos_free(AtRespPtr);
}

/**
 * @brief Initialize active reporting of network status (AT+CEREG=1)
 * @details Sends the AT+CEREG=1 command to enable active reporting of network status changes.
 */
static void liot_net_cereg_urc_init(void)
{
    uint8_t AtRespBuff[128] = {0};
    liot_trace("SNED:%s\r\n", AT_CEREGSET);
    if (liot_VirtualAtCmd(AT_CEREGSET, strlen(AT_CEREGSET), 3000, AtRespBuff) == LIOT_SUCCESS)
    {
        liot_trace("AtRespBuff:%s\r\n", AtRespBuff);
    }
    else
    {
        liot_trace("cereg_urc_init fail\r\n");
    }
}

/**
 * @brief URC callback function
 * @details Handles unsolicited result codes (URC) reported by the module, parses network status change information, 
 *          and updates the global variable `g_NetConSta`.
 * @param[in] pStr Pointer to the URC data
 * @param[in] strLen Length of the URC data
 */
static void URC_RecvData_CallBack(const uint8_t *pStr, uint32_t strLen)
{
    if ((strstr((const char *)pStr, "+CEREG: 1") != NULL) || (strstr((const char *)pStr, "+CEREG: 5") != NULL))
    {
        liot_trace("Network connection succeeded\r\n");
        g_NetConSta = 1;
    }
    else if (strstr((const char *)pStr, "+CEREG: 0") != NULL)
    {
        liot_trace("Network connection disconnected\r\n");
        g_NetConSta = 0;
    }
    else if (strstr((const char *)pStr, "+CEREG: 2") != NULL)
    {
        liot_trace("Network connection ing...\r\n");
        g_NetConSta = 0;
    }
    else if (strstr((const char *)pStr, "+CEREG: 3") != NULL)
    {
        liot_trace("Network connection denied\r\n");
        g_NetConSta = 0;
    }
}

/**
 * @brief Main thread function
 * @details Runs in an independent thread, performing a series of operations to test module functionality and monitor network status.
 * @param[in] argument Thread parameter (unused)
 */
void liot_virtual_demo_thread(void *argument)
{
    liot_trace("virtual demo begin\r\n");

    //Print out SRAM information
    liot_trace("========== rtos Get TotalHeapSize:%dKB,FreeHeapSize:%dKB,MinFreeHeapSize:%dKB,MaxFreeBlockSize:%dKB", 
                                                (liot_xPortGetTotalHeapSize()) >> 10, (liot_xPortGetFreeHeapSize()) >> 10,
                                                (liot_xPortGetMinimumEverFreeHeapSize()) >> 10, (liot_xPortGetMaximumFreeBlockSize()) >> 10);
    while (1)
    {
        // Register URC
        liot_UrcCallbackRegister(URC_RecvData_CallBack);
        liot_trace("virtual Register\r\n");

        // Set CREG=1 to actively report network status changes
        liot_net_cereg_urc_init();

        // Turn off RF to trigger network changes
        liot_At_Cmd_CFUN0();

        liot_rtos_task_sleep_s(2);

        // Turn on the device to continue triggering network changes
        liot_At_Cmd_CFUN1();

        liot_rtos_task_sleep_s(2);
        
        // Read the IMEI number
        liot_At_Cmd_IMEIREAD();

        // Read the version number
        liot_At_Cmd_CGMRREAD();

        // Read the configuration file
        liot_At_Cmd_ECPCFG();

        // Unregister the URC callback function
        liot_UrcCallbackDeRegister();        
        
        liot_trace("virtual Deregister\r\n");

        liot_rtos_task_sleep_s(5);

        //Print out SRAM information
        liot_trace("========== rtos Get TotalHeapSize:%dKB,FreeHeapSize:%dKB,MinFreeHeapSize:%dKB,MaxFreeBlockSize:%dKB", 
                                                (liot_xPortGetTotalHeapSize()) >> 10, (liot_xPortGetFreeHeapSize()) >> 10,
                                                (liot_xPortGetMinimumEverFreeHeapSize()) >> 10, (liot_xPortGetMaximumFreeBlockSize()) >> 10);
    }
}