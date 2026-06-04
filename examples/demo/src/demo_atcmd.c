/**
 * @File Name: liot_atcmd_demo.c
 * @brief
 * @Author : Lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
/**
 * The main purpose of this Demo is to demonstrate how to implement, register, and initialize 
 *   custom AT commands in the system. It provides a framework that allows developers to add
 *   more AT commands by extending the liot_atcmd_def.h file and handle these commands through 
 *   a unified interface.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_at_cmd.h"
#include "liot_os.h"
#include "liot_type.h"

/**
 * @brief Handler for the `LIOTATDMO` AT command
 * 
 * Processes the `LIOTATDMO` AT command based on the operation type specified in the input context.
 * 
 * @param pAtInputCtx [Input] Pointer to the AT command input context, 
                              containing details such as the operation type and arguments.
 * 
 * @details 
 * - Supported operation types:
 *   - [LIOT_AT_CMD_SET]
 *   - [LIOT_AT_CMD_TEST]
 *   - [LIOT_AT_CMD_READ]
 *   - [LIOT_AT_CMD_EXE]
 * 
 * @return Returns the execution result as a value of [liot_at_result_enum_type]
 */
liot_at_result_enum_type liot_atcmd_exec_atdemo_cmd1(const liot_atCommand_Input *pAtInputCtx)
{
    liot_at_result_enum_type result = LIOT_AT_OK;
    CHAR resp_buf[256]              = {0};
    int arvg_num                    = pAtInputCtx->args_count;
    switch (pAtInputCtx->op)
    {
        case LIOT_AT_CMD_SET:
            snprintf((char *)resp_buf, 256, "%s,%d", "LIOT_AT_CMD_SET", arvg_num);
            break;
        case LIOT_AT_CMD_TEST:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_TEST");
            break;
        case LIOT_AT_CMD_READ:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_READ");
            break;
        case LIOT_AT_CMD_EXE:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_EXE");
            break;
    }
    result = liot_atcmd_reply(pAtInputCtx->atHandle, LIOT_AT_OK, resp_buf);
    return result;
}
/**
 * @brief Handler for the `LIOTATDMO1` AT command
 * 
 * Similar to [liot_atcmd_exec_atdemo_cmd1]
 * 
 * @param pAtInputCtx [Input] Pointer to the AT command input context, containing details such as the operation type and arguments.
 * 
 * @details 
 * - Operation types and responses are identical to those in [liot_atcmd_exec_atdemo_cmd1]
 * 
 * @return Returns the execution result as a value of [liot_at_result_enum_type]
 */
liot_at_result_enum_type liot_atcmd_exec_atdemo_cmd2(const liot_atCommand_Input *pAtInputCtx)
{
    liot_at_result_enum_type result = LIOT_AT_OK;
    CHAR resp_buf[256]              = {0};
    int arvg_num                    = pAtInputCtx->args_count;
    switch (pAtInputCtx->op)
    {
        case LIOT_AT_CMD_SET:
            snprintf((char *)resp_buf, 256, "%s,%d", "LIOT_AT_CMD_SET", arvg_num);
            break;
        case LIOT_AT_CMD_TEST:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_TEST");
            break;
        case LIOT_AT_CMD_READ:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_READ");
            break;
        case LIOT_AT_CMD_EXE:
            snprintf((char *)resp_buf, 256, "%s", "LIOT_AT_CMD_EXE");
            break;
    }
    result = liot_atcmd_reply(pAtInputCtx->atHandle, LIOT_AT_OK, resp_buf);
    return result;
}

 /**
 * @brief AT Command Table
 * 
 * This array stores all registered AT commands and their corresponding processing functions.
 * Specific AT command definitions are included via `#include "liot_atcmd_def.h"`.
 * 
 * @details 
 * - Each entry in the table is a [liot_atCommand] structure, containing:
 *   - [pName]: The name of the AT command (e.g., `"AT+EXAMPLE"`).
 *   - [atProcFunc]: A pointer to the function that handles the command logic.
 * 
 */
const liot_atCommand liot_atcmd_tab[] = {
    LIOT_ATCMD("+LIOTATDMO", liot_atcmd_exec_atdemo_cmd1)
    LIOT_ATCMD("+LIOTATDMO1", liot_atcmd_exec_atdemo_cmd2)
};

/**
 * @brief Get the size of the AT command table
 * 
 * Calculates and returns the number of entries in the [liot_atcmd_tab] array.
 * 
 * @return Returns the size of the AT command table as an integer.
 */
int LIOT_GetCustomer_ATtable_Size(void)
{
    return sizeof(liot_atcmd_tab) / sizeof(liot_atcmd_tab[0]);
}

/**
 * @brief Initialize and register custom AT commands
 * 
 * This function initializes the custom AT command module by registering the AT command table and performing necessary setup.
 * 
 * @param argv [Input] Optional argument passed to the thread (not used in this implementation).
 * 
 * @details 
 * - Steps performed:
 *   1. Retrieves the size of the AT command table using .
 *   2. Registers the AT command table using [liot_atcmd_register].
 *   3. Initializes the AT command module using [liot_open_atcmd_init].
 *   4. Logs a message indicating successful initialization.
 *   5. Deletes the task after a short delay.
 * 
 * @note 
 * - After initialization, custom AT commands can be invoked via USB AT or UART AT interfaces.
 * - UART1 must be explicitly configured as an AT port by setting `LIOT_UART1_AT_ENABLE = 1` in the [bsp_config.h] file.
 */
void liot_atcmd_init_demo_thread(void *argv)
{
    //Retrieves the size of the AT command table using
    UINT32 cmd_tab_size = LIOT_GetCustomer_ATtable_Size();

    //egisters the AT command table using
    liot_atcmd_register((liot_atCommandP)&liot_atcmd_tab[0], cmd_tab_size);
    
    //Initializes the AT command module
    liot_open_atcmd_init();
    liot_trace("atcmd_init_demo_running...");
    liot_rtos_task_sleep_ms(500);
    liot_rtos_task_delete(NULL);
}
