/**
 * @File Name: liot_at_cmd.h
 * @brief
 * @Author : Lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef LIOT_AT_CMD_H
#define LIOT_AT_CMD_H

#include "liot_type.h"

#define LIOT_AT_CMD_MAX_ARG      128
#define LIOT_AT_CMD_NAME_MAX_LEN 32

#define NA 1 /*  Name field found      */
#define EQ 2 /*  <=> found             */
#define QU 4 /*  <?> found             */
#define AR 8 /*  Argument field found  */
#undef LIOT_ATCMD
#define LIOT_ATCMD(cmd_name, proc_func) {cmd_name, proc_func},
#define CHAR_CTRL_Z                     0x1a
#define CHAR_ESC                        0x1b
#define CHAR_BACKSPACE                  8

typedef enum
{
    LIOT_AT_CMD_SET  = NA | EQ | AR,
    LIOT_AT_CMD_TEST = NA | EQ | QU,
    LIOT_AT_CMD_READ = NA | QU,
    LIOT_AT_CMD_EXE  = NA
} liot_atCmdType_t;

typedef enum
{
    LIOT_AT_BASIC_N_C_IDX = 0,
    LIOT_AT_BASIC_N_D_IDX,
    LIOT_AT_BASIC_N_E_IDX,
    LIOT_AT_BASIC_N_F_IDX,
    LIOT_AT_BASIC_N_S_IDX,
    LIOT_AT_BASIC_N_V_IDX,
    LIOT_AT_BASIC_N_W_IDX,
    LIOT_AT_BASIC_E_IDX,
    LIOT_AT_BASIC_I_IDX,
    LIOT_AT_BASIC_L_IDX,
    LIOT_AT_BASIC_M_IDX,
    LIOT_AT_BASIC_Q_IDX,
    LIOT_AT_BASIC_V_IDX,
    LIOT_AT_BASIC_X_IDX,
    LIOT_AT_BASIC_Z_IDX,
    LIOT_AT_BASIC_T_IDX,
    LIOT_AT_BASIC_P_IDX,
} liot_atcmd_basic_type_e;

typedef enum
{
    LIOT_AT_NO_RSP     = -1,    // no response
    LIOT_AT_OK         = 0,     // OK
    LIOT_AT_CONNECT    = 1,     // CONNECT
    LIOT_AT_RING       = 2,     // RING
    LIOT_AT_NO_CARRIER = 3,     // NO CARRIER
    LIOT_AT_ERROR      = 4,     // ERROR

    LIOT_AT_CME_ERROR = 100,     // +CME ERROR:<err>
} liot_at_result_enum_type;

typedef void *liot_atHandle;

/**
 * @brief Define the input context structure for an AT command
 * 
 * This structure describes the input parameters and context information for a single AT command. 
 * It is a core data structure for AT command processing, containing key information such as the command name, 
 * operation type, handle, number of arguments, and argument values.
 * @note 
 *      The current implementation only supports a single argument and does not support multiple arguments.
 */
typedef struct liot_atCommand_Input_t
{
    CHAR name[LIOT_AT_CMD_NAME_MAX_LEN + 1];  // AT command name
    UINT8 op;                                 // Operation type 
    liot_atHandle atHandle;                   // AT command handle
    UINT8 args_count;                         // Number of arguments
    CHAR arg[LIOT_AT_CMD_MAX_ARG];            // Argument values
} liot_atCommand_Input;

/**
 * @brief Define the basic structure of an AT command
 * 
 * This structure describes the name and processing function of a single AT command. 
 * It is a core component of the AT command table, used to register and manage all supported AT commands.
 * 
 */
typedef struct liot_atCommand_t
{
    const CHAR *pName; // AT command name
    liot_at_result_enum_type (*atProcFunc)(const liot_atCommand_Input *pAtInputCtx);// Command processing function
} liot_atCommand, *liot_atCommandP; // 16 bytes

typedef enum
{
    LIOT_AT_MIN_MSG = 0,

    LIOT_AT_URC_MSG,
    LIOT_AT_SIO_MSG,

    LIOT_AT_SIM_MSG,

    LIOT_AT_FILE_MSG,

    LIOT_AT_SOCKET_MSG,
    LIOT_AT_NTP_MSG,
    LIOT_AT_PING_MSG,
    LIOT_AT_HTTP_MSG,
    LIOT_AT_CTCC_SREG_MSG,
    LIOT_AT_FTP_MSG,
    LIOT_AT_MQTT_MSG,
    LIOT_AT_LBS_MSG,
    LIOT_AT_LWM2M_MSG,
    LIOT_AT_CMCC_DM_MSG,
    LIOT_AT_CTWING_MSG,
    LIOT_AT_PDP_MSG,
    LIOT_AT_DNS_MSG,
    LIOT_AT_MAX_MSG
} liot_atMsgId_e;

typedef struct
{
    UINT16 msg_id;
    void *msg_param;
} liot_atMsg_t;

typedef struct
{
    const liot_atMsgId_e msg_id;
    void (*msgProcFunc)(void *msg_param);
} liot_AsyncMsgEntry, *liot_AsyncMsgEntryP;

/**
 * @brief Send the response result of an AT command
 * 
 * This function generates the corresponding AT command response based on the input result code 
 * and sends it to the requester by calling a lower-level interface.
 * It converts the `result_code` into the corresponding AT response code and can append a custom response string.
 * 
 * @param atHandle [Input] AT command handle, used to identify a specific AT command request.
 * @param result_code [Input] Result code indicating the execution result of the AT command. For example:
 *        - LIOT_AT_NO_RSP: No response
 *        - LIOT_AT_OK: Success
 *        - LIOT_AT_ERROR: Error
 *        - LIOT_AT_CME_ERROR: Device error
 * @param resp_str [Input] Optional response string, used to return additional information or data.
 * 
 * @return Returns the status of the operation:
 *         - Returns 0 or a positive number on success.
 *         - Returns a negative number on failure.
 */
extern int liot_atcmd_reply(liot_atHandle atHandle, liot_at_result_enum_type result_code, const CHAR *resp_str);

/**
 * @brief Initialize the AT command module
 * 
 * This function is used to initialize the relevant resources of the AT command module.
 * 
 * @details 
 * - The main purpose of this function is to provide initialization support for the operation of the AT command module.
 * 
 * @return No return value
 */
extern void liot_open_atcmd_init(void);

/**
 * @brief Register the AT command table
 * 
 * This function is used to register a set of predefined AT commands and their related configuration information. 
 * It stores the input AT command table into a global variable.
 * 
 * @param tab_ptr [Input] Pointer to the AT command table, containing all the AT commands that need to be registered.
 * @param tab_size [Input] Size of the AT command table, indicating the number of commands in the table.
 * 
 * @return No return value
 */
extern void liot_atcmd_register(const liot_atCommandP tab_ptr, const UINT32 tab_size);

#endif // LIOT_AT_CMD_H
