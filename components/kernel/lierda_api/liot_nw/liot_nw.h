/**
 * @File Name: liot_nw.h
 * @brief Header file for Liot network-related functions and definitions.
 * @Version : 1.0
 * @Creat Date : 2025-06-26
 *
 * @copyright Copyright (c) 2025 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef _LIOT_NW_H_
#define _LIOT_NW_H_

#ifdef __cplusplus // this area code will compile with c program
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#define LIOT_NW_MCC_MAX_LEN        3 // maximum length of MCC
#define LIOT_NW_MNC_MAX_LEN        3 // maximum length of MNC
#define LIOT_NW_LONG_OPER_MAX_LEN  32 // maximum length of long operator name
#define LIOT_NW_SHORT_OPER_MAX_LEN 32 // maximum length of short operator name

#define LIOT_NW_CELL_MAX_NUM 7 // maxinum number of cell infomation (include serving cell and neighbour cell)
#define LIOT_NW_CELL_REQ_MAX_TIMES  8

#define LIOT_NW_SIM_1  0  //SIM1
#define LIOT_NW_SIM_2  1  //SIM2
#define LIOT_NW_MAX_SIM_NUM  2

#define LIOT_LTE_CELL_INFO_INVALID  0  //0: LTE cell information is invalid
#define LIOT_LTE_CELL_INFO_VALID  1    //1: LTE cell information is valid

#define LIOT_LTE_CELL_TYPE_SERVING  0    // Cell type, 0:serving, 1:neighbor
#define LIOT_LTE_CELL_TYPE_NEIGHBOR  1   // Cell type, 0:serving, 1:neighbor

/*===========================================================================
 * Enum
 ===========================================================================*/

typedef enum
{
    LIOT_NW_SUCCESS     = 0,
    LIOT_NW_EXECUTE_ERR = 1 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_NW_MEM_ADDR_NULL_ERR,
    LIOT_NW_INVALID_PARAM_ERR,
    LIOT_NW_CFW_CFUN_GET_ERR,
    LIOT_NW_CFUN_DISABLE_ERR = 5 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_NW_CFW_NW_STATUS_GET_ERR,
    LIOT_NW_NOT_SEARCHING_ERR,
    LIOT_NW_NOT_REGISTERED_ERR,
    LIOT_NW_CFW_GPRS_STATUS_GET_ERR,
    LIOT_GPRS_NOT_SEARCHING_ERR = 10 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_GPRS_NOT_REGISTERED_ERR,
    LIOT_NW_CFW_NW_QUAL_GET_ERR,
    LIOT_NW_CFW_OPER_ID_GET_ERR,
    LIOT_NW_CFW_OPER_NAME_GET_ERR,
    LIOT_NW_CFW_OPER_SET_ERR = 15 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_NW_SIM_ERR,
    LIOT_NW_NO_MEM_ERR,
    LIOT_NW_SEMAPHORE_CREATE_ERR,
    LIOT_NW_SEMAPHORE_TIMEOUT_ERR,
    LIOT_NW_NITZ_NOT_UPDATE_ERR = 20 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_NW_CFW_EMOD_START_ERR,
    LIOT_NW_OPERATOR_NOT_ALLOWED,
    LIOT_NW_CFW_RRCRel_SET_ERR,
} liot_nw_errcode_e;

/*network access technology type, only support 7*/
typedef enum
{
    LIOT_NW_ACCESS_TECH_GSM                = 0,
    LIOT_NW_ACCESS_TECH_GSM_COMPACT        = 1,
    LIOT_NW_ACCESS_TECH_UTRAN              = 2,
    LIOT_NW_ACCESS_TECH_GSM_wEGPRS         = 3,
    LIOT_NW_ACCESS_TECH_UTRAN_wHSDPA       = 4,
    LIOT_NW_ACCESS_TECH_UTRAN_wHSUPA       = 5,
    LIOT_NW_ACCESS_TECH_UTRAN_wHSDPA_HSUPA = 6,
    LIOT_NW_ACCESS_TECH_E_UTRAN            = 7,
} liot_nw_act_type_e;

/*network register status, equal to creg*/
typedef enum
{
    LIOT_NW_REG_STATE_NOT_REGISTERED = 0, // not registered, MT is not currently searching an operator to register to
    LIOT_NW_REG_STATE_HOME_NETWORK   = 1, // registered, home network
    LIOT_NW_REG_STATE_TRYING_ATTACH_OR_SEARCHING = 2, // not registered, but MT is currently trying to attach or
                                                      // searching an operator to register to
    LIOT_NW_REG_STATE_DENIED  = 3,                    // registration denied
    LIOT_NW_REG_STATE_UNKNOWN = 4,                    // unknown
    LIOT_NW_REG_STATE_ROAMING = 5,                    // registered, roaming
} liot_nw_reg_state_e;


typedef enum
{
    LIOT_NW_CTZU_DISABLE = 0, 
    LIOT_NW_CTZU_ENABLE = 1, 
} liot_nw_ctzu_state_e;

/*===========================================================================
 * Struct
 ===========================================================================*/

typedef struct
{
    liot_nw_reg_state_e state; // network register state
    int lac;                   // location area code
    int cid;                   // cell ID
    liot_nw_act_type_e act;    // access technology
} liot_nw_common_reg_status_info_s;

typedef struct
{
    char nitz_time[32]; // string format: YY/MM/DD HH:MM:SS '+/-'TZ daylight,   20/09/25 07:40:18 +32 00.
    long abs_time;      // Numeric format of NITZ time    0 is unavailable
} liot_nw_nitz_time_info_s;

typedef struct
{
    char long_oper_name[LIOT_NW_LONG_OPER_MAX_LEN + 1];
    char short_oper_name[LIOT_NW_SHORT_OPER_MAX_LEN + 1];
    char mcc[LIOT_NW_MCC_MAX_LEN + 1];
    char mnc[LIOT_NW_MNC_MAX_LEN + 1];
} liot_nw_operator_info_s;

typedef struct
{
    liot_nw_common_reg_status_info_s data_reg; // data register info
} liot_nw_reg_status_info_s;

typedef struct
{
    UINT64 uplink_data_count;
    UINT64 downlink_data_count;
} liot_nw_data_count_info_s;

typedef struct
{
    unsigned char nw_selection_mode;   // 0 auto select operator    1 manual select operator
    char mcc[LIOT_NW_MCC_MAX_LEN + 1]; // String format
    char mnc[LIOT_NW_MNC_MAX_LEN + 1]; // String format    eg:China Mobile -----> mcc="460"   mnc="00"
    liot_nw_act_type_e act;            // access technology
} liot_nw_seclection_info_s;

typedef struct
{
    int rssi;         // received signal strength level (return 99 indicates that not known or not detectable)
    int bitErrorRate; // channel bit error rate (return 99 indicates that not known or not detectable)
    int rsrq;         // reference signal received quality (return 255 indicates that not known or not detectable)
    int rsrp;         // reference signal received power (return 255 indicates that not known or not detectable)
    int snr;          // SNR Signal-to-Noise Ratio value in dB, value range: -20 ~ 40
} liot_nw_signal_strength_info_s;

typedef struct
{
    int flag; // Cell type, 0:serving, 1:neighbor
    int cid;  // Cell ID, (0 indicates that the cellid is not received)
    int mcc;
    int mnc;
    int tac;    // Tracing area code
    int pci;    // Physical cell ID
    int earfcn; // E-UTRA absolute radio frequency channel number of the cell. RANGE: 0 TO 65535
    int rssi;   // Receive signal strength, Value range: rsrp-140 for dbm format
    char mnc_len;
    char RX_dBm; // Received power
} liot_nw_lte_cell_info_s;

typedef struct
{
    int lte_info_valid; // 0: LTE cell information is invalid   1: LTE cell information is valid
    int lte_info_num;   // LTE cell number
    liot_nw_lte_cell_info_s lte_info[LIOT_NW_CELL_MAX_NUM]; // LTE cell information (Serving and neighbor)
} liot_nw_cell_info_s;

typedef void (*liot_nw_callback)(uint8_t nSim, unsigned int ind_type, void *ctx);
typedef void (*liot_getcell_async_callback)(liot_nw_cell_info_s *cell_info);

/**
 * @brief Get the CSQ signal strength.
 * This function retrieves the CSQ signal strength information for the specified SIM card.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param csq [out] Pointer to store the CSQ signal strength information, range (0 - 31), 99 indicates an invalid value.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_csq(uint8_t nSim, unsigned char *csq);

/**
 * @brief Get the operator information of the currently registered network.
 * This function retrieves the operator information of the currently registered network. It can only be called after successful network registration.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param oper_info [out] Pointer to store the retrieved operator information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_operator_name(uint8_t nSim, liot_nw_operator_info_s *oper_info);

/**
 * @brief Get the current network registration information.
 * This function retrieves the current network registration information for the specified SIM card.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param reg_info [out] Pointer to store the retrieved network registration information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_reg_status(uint8_t nSim, liot_nw_reg_status_info_s *reg_info);

/**
 * @brief Set the operator selection.
 * This is a synchronous API that waits up to 120 seconds, depending on the network.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param select_info [in] Pointer to the operator selection information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_set_selection(uint8_t nSim, liot_nw_seclection_info_s *select_info);

/**
 * @brief Get the selected operator information.
 * This function retrieves the selected operator information. It needs to wait for successful network registration.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param select_info [out] Pointer to store the retrieved operator selection information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_selection(uint8_t nSim, liot_nw_seclection_info_s *select_info);

/**
 * @brief Get the detailed signal strength information.
 * This function retrieves the detailed signal strength information for the specified SIM card.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param pt_info [out] Pointer to store the retrieved detailed signal strength information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_signal_strength(uint8_t nSim, liot_nw_signal_strength_info_s *pt_info);

/**
 * @brief Get the current base station time.
 * This function retrieves the current base station time. The time is only updated at the moment of successful network registration and requires local network support.
 * 
 * @param nitz_info [out] Pointer to store the time information retrieved from the base station by the modem.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_nitz_time_info(liot_nw_nitz_time_info_s *nitz_info);

/**
 * @brief Get the current serving and neighboring cell information.
 * This is a synchronous API that waits up to 20 seconds.
 * 
 * @param nSim [in] SIM card index, valid values: 0 - 1.
 * @param cell_info [out] Pointer to store the current serving and neighboring cell information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_cell_info(uint8_t nSim, liot_nw_cell_info_s *cell_info);

/**
 * @brief Register an event callback function.
 * This function registers an event callback function to handle network-related events.
 * 
 * @param nw_cb [in] Event callback function pointer.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_register_cb(liot_nw_callback nw_cb);

/**
 * @brief Get the uplink and downlink data counts.
 * This function retrieves the uplink and downlink data counts for the specified SIM card.
 * 
 * @param nSim [in] SIM card index, range: 0 - 1.
 * @param data_info [out] Pointer to store the retrieved data count information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_data_count(uint8_t nSim, liot_nw_data_count_info_s *data_info);

/**
 * @brief Reset the uplink and downlink data counts.
 * This function resets the uplink and downlink data counts for the specified SIM card.
 * 
 * @param nSim [in] SIM card index, range: 0 - 1.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_reset_data_count(uint8_t nSim);

/**
 * @brief Get the current serving and neighboring cell information asynchronously.
 * This is an asynchronous API to retrieve the current serving and neighboring cell information.
 * 
 * @param nSim [in] SIM card index, range: 0 - 1.
 * @param cell_cb [in] Callback function pointer to handle the retrieved cell information.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_get_cell_info_async(uint8_t nSim, liot_getcell_async_callback cell_cb);

/**
 * @brief Set the base station time synchronization switch.
 * This function sets the base station time synchronization switch, which takes effect immediately and is saved.
 * 
 * @param onoff [in] 0: Disable, 1: Enable.
 * @return liot_nw_errcode_e 0 indicates success, other values indicate error codes.
 */
liot_nw_errcode_e liot_nw_set_ctzu_switch(bool onoff);

/**
 * @brief Get the status of the base station time synchronization switch.
 * This function retrieves the status of the base station time synchronization switch.
 * 
 * @return bool false indicates the switch is off, true indicates the switch is on.
 */
bool  liot_nw_get_ctzu_switch(void);

#ifdef __cplusplus
}
#endif
#endif