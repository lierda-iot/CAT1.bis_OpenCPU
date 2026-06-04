/**
 * @File Name: liot_sms.h
 * @brief SMS (Short Message Service) API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.0
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef _LIOT_SMS_H_
#define _LIOT_SMS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*========================================================================
 *  Marco Definition
 *========================================================================*/

#define LIOT_SMS_MIN_LEN     1
#define LIOT_MAX_LONGSMS_SEGMENT  8
#define LIOT_MAX_SMS_LENGTH       (160 * 4)
#define LIOT_SMS_BUF_LEN          (LIOT_MAX_SMS_LENGTH + 32)
#define LIOT_MAX_LONGSMS_LENGTH   (160 * 4 * LIOT_MAX_LONGSMS_SEGMENT)
#define LIOT_TEL_MAX_LEN     21
#define LIOT_SMS_PDU_MAX_LEN 176
#define LIOT_SMS_SEND_MSG_GUARD_TIMER    60
/*========================================================================
 *  Enumeration Definition
 *========================================================================*/

typedef enum
{
    LIOT_SMS_SUCCESS = 0,
    LIOT_SMS_ERROR   = 1 | (LIOT_COMPONENT_SMS << 16),
    LIOT_SMS_NOT_INIT_ERR,
    LIOT_SMS_PARA_ERR,
    LIOT_SMS_NO_MEMORY_ERR,
    LIOT_SMS_SEM_CREATE_ERR,
    LIOT_SMS_SEM_TIMEOUT_ERR,
    LIOT_SMS_NO_MSG_ERR,
} liot_sms_errcode_e;

typedef enum
{
    LIOT_SMS_INIT_OK_IND = 1 | (LIOT_COMPONENT_SMS << 16), /* Not used */
    LIOT_SMS_NEW_MSG_IND,
    LIOT_SMS_LIST_IND,
    LIOT_SMS_LIST_END_IND,
    LIOT_SMS_LIST_EX_IND,
    LIOT_SMS_MEM_FULL_IND,
} liot_sms_event_id_e;

typedef enum
{
    LIOT_ME = 1, // Mobile Equipment message storage
    LIOT_SM = 2, // SIM message storage
} liot_sms_stor_e;

typedef enum
{
    LIOT_GSM  = 0,
    LIOT_UCS2 = 1,
    LIOT_IRA = 2,
} liot_sms_code_e;

typedef enum
{
    LIOT_PDU  = 0,
    LIOT_TEXT = 1,
} liot_sms_format_e;

typedef enum
{
    LIOT_SMS_UNREAD = 0,
    LIOT_SMS_READ   = 1,
    LIOT_SMS_UNSENT = 2,
    LIOT_SMS_SENT   = 3,
    LIOT_SMS_ALL,
} liot_sms_status_e;

typedef enum
{
    LIOT_SMS_DEL_INDEX = 0,      /* Delete the message specified in <index> */
    LIOT_SMS_DEL_RECV_READ,      /* Delete all read messages from <mem1> storage */
    LIOT_SMS_DEL_RECV_READ_SENT, /* Delete all read messages from <mem1> storage and sent mobile originated messages */
    LIOT_SMS_DEL_RECV_READ_MO,   /* Delete all read messages from <mem1> storage, sent and unsent mobile originated
                                    messages */
    LIOT_SMS_DEL_ALL,            /* Delete all messages from <mem1> storage */

    LIOT_SMS_DEL_MODES_NUMS
} liot_sms_delete_mode_e;

typedef enum
{
    LIOT_CS_GSM,
    LIOT_CS_IRA,
    LIOT_CS_UCS2,
    LIOT_CS_ALL,
} liot_sms_cscs_e;

/*========================================================================
 *	Struct Definition
 *========================================================================*/
typedef struct
{
    uint8 status;
    uint8 index;
    char *buf;
    uint16 buf_len;
} liot_sms_msg_s;

typedef struct
{
    uint8 index;
    uint8 mem;
} liot_sms_new_s;

typedef struct
{
    uint16 usedSlotSM;
    uint16 totalSlotSM;
    uint16 unReadRecordsSM;
    uint16 usedSlotME;
    uint16 totalSlotME;
    uint16 unReadRecordsME;
    liot_sms_stor_e newSmsStorId;
} liot_sms_stor_info_s;

typedef struct
{
    liot_sms_stor_e mem1; // messages to be read and deleted from this memory storage.
    liot_sms_stor_e mem2; // messages will be written and sent to this memory storage.
    liot_sms_stor_e mem3; // received messages will be placed in this memory storage if routing to PC is not set.
} liot_sms_mem_info_s;

typedef struct
{
    uint16 uYear; // Only the last two digits
    uint8 uMonth;
    uint8 uDay;
    uint8 uHour;
    uint8 uMinute;
    uint8 uSecond;
    int8 iZone;
} liot_sms_time_stamp_s;

typedef struct
{
    uint8 index;                       // Index in storage
    char oa[LIOT_TEL_MAX_LEN * 4 + 1]; // Originating Address
    uint8 tooa;                        // Type of originating Address
    liot_sms_time_stamp_s scts;        // Service Centre Time Stamp
    liot_sms_status_e status;
    uint8 fo;        // First octet of PDU
    uint8 dcs;       // Data Coding Scheme
    uint16 uid;      // Message identification of a concatenated message
    uint8 msg_seg;   // Sequence number of a concatenated message
    uint8 msg_total; // The total number of the segments of one concatenated message.
    uint16 dataLen;  // number of bytes
    uint8 data[LIOT_SMS_PDU_MAX_LEN * 2 + 1];
} liot_sms_recv_s;

/*========================================================================
 *  Callback Definition
 *========================================================================*/
typedef void (*liot_sms_event_handler_t)(uint8 sim_id, int event_id, void *ctx);

/**
 * @brief Send SMS message in text format
 *
 * This function sends an SMS message in text format to the specified phone number.
 *
 * @param nSim      SIM card index, value: 0-1
 * @param phone_num Destination phone number
 * @param data      SMS message data
 * @param code      SMS encoding format (refer to liot_sms_code_e)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_send_msg(uint8 nSim, char *phone_num, char *data, liot_sms_code_e code);

/**
 * @brief Send SMS message in PDU format
 *
 * This function sends an SMS message in PDU (Protocol Data Unit) format.
 *
 * @param nSim SIM card index, value: 0-1
 * @param pdu  SMS message data in PDU format
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_send_pdu(uint8_t nSim, char *pdu);

/**
 * @brief Get SMS message list
 *
 * This function retrieves a list of SMS messages in the specified format.
 *
 * @param nSim   SIM card index, value: 0-1
 * @param format SMS message format (refer to liot_sms_format_e)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_read_msg_list(uint8_t nSim, liot_sms_format_e format);

/**
 * @brief Read single SMS message in text format
 *
 * This function reads a single SMS message in text format by index.
 *
 * @param nSim     SIM card index, value: 0-1
 * @param index    SMS message index
 * @param buf      Buffer to receive SMS content
 * @param buf_len  Length of the buffer
 * @param format   SMS format (refer to liot_sms_format_e)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_read_msg(
    uint8_t nSim, uint8_t index, char *buf, uint16_t buf_len, liot_sms_format_e format);

/**
 * @brief Read single SMS message with extended information
 *
 * This function reads a single SMS message with extended information by index.
 *
 * @param nSim   SIM card index, value: 0-1
 * @param index  SMS message index
 * @param format SMS format (refer to liot_sms_format_e)
 * @param msg    Pointer to structure to store extended SMS information (refer to liot_sms_recv_s)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_read_msg_ex(uint8_t nSim, uint8_t index, liot_sms_format_e format, liot_sms_recv_s *msg);

/**
 * @brief Delete single SMS message
 *
 * This function deletes a single SMS message by index.
 *
 * @param nSim   SIM card index, value: 0-1
 * @param index  SMS message index
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_delete_msg(uint8_t nSim, uint8_t index);

/**
 * @brief Delete SMS message with specified mode
 *
 * This function deletes SMS messages using the specified deletion mode.
 *
 * @param nSim     SIM card index, value: 0-1
 * @param index    SMS message index
 * @param del_mode Deletion mode (refer to liot_sms_delete_mode_e)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_delete_msg_ex(uint8_t nSim, uint8_t index, liot_sms_delete_mode_e del_mode); 

/**
 * @brief Get SMS center address
 *
 * This function retrieves the SMS center address (SMSC) for the specified SIM.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param address Buffer to store the SMS center address
 * @param len     Length of the address buffer
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_get_center_address(uint8_t nSim, char *address, uint8_t len);

/**
 * @brief Set SMS center address
 *
 * This function sets the SMS center address (SMSC) for the specified SIM.
 *
 * @param nSim    SIM card index, value: 0-1
 * @param address SMS center address to set
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_set_center_address(uint8_t nSim, char *address);

/**
 * @brief Get SMS storage information
 *
 * This function retrieves information about SMS storage capacity and usage.
 *
 * @param nSim       SIM card index, value: 0-1
 * @param stor_info  Pointer to structure to store SMS storage information (refer to liot_sms_stor_info_s)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_get_storage_info(uint8_t nSim, liot_sms_stor_info_s *stor_info);

/**
 * @brief Set SMS storage locations
 *
 * This function sets the SMS storage locations for different operations.
 *
 * @param nSim  SIM card index, value: 0-1
 * @param mem1  Storage for reading and deleting messages (refer to liot_sms_stor_e)
 * @param mem2  Storage for writing and sending messages (refer to liot_sms_stor_e)
 * @param mem3  Storage for receiving messages (refer to liot_sms_stor_e)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_set_storage(uint8_t nSim, liot_sms_stor_e mem1, liot_sms_stor_e mem2, liot_sms_stor_e mem3);

/**
 * @brief Get SMS storage locations
 *
 * This function retrieves the current SMS storage location settings.
 *
 * @param nSim      SIM card index, value: 0-1
 * @param mem_info  Pointer to structure to store memory information (refer to liot_sms_mem_info_s)
 * @return Error code indicating success or failure
 *         - LIOT_SMS_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_sms_errcode_e liot_sms_get_storage(uint8_t nSim, liot_sms_mem_info_s *mem_info);

/**
 * @brief Register SMS callback function
 *
 * This function registers the callback function that will be called when
 * SMS events occur.
 *
 * @param cb Callback function to register (refer to liot_sms_event_handler_t)
 */
void liot_sms_callback_register(liot_sms_event_handler_t cb);

#ifdef __cplusplus
}
#endif
#endif